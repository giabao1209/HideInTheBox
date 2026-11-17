#include "HBPlayerCharacter.h"
#include "HBAttributes.h"
#include "HBEffects.h"
#include "HBGameplayAbility.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "GameplayEffect.h"
#include "HBDebugSubsystem.h"
#include "Engine/SkeletalMesh.h"
#include "HBEnemyCharacter.h"
#include "HBGameMode.h"
#include "HBIndicator.h"
#include "HideInTheBox.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Engine/World.h"
#include "TimerManager.h"

AHBPlayerCharacter::AHBPlayerCharacter()
{
    this->PrimaryActorTick.bCanEverTick = true;
    this->Tint = FLinearColor(0.1f, 0.7f, 1.0f);
    this->fMaxHealth = 220.0f;
    ManaEffectClass = UHBGE_ManaDelta::StaticClass();
    CooldownEffectClass = UHBGE_Cooldown::StaticClass();
    this->CameraBoom = this->CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
    this->CameraBoom->SetupAttachment(this->GetRootComponent());
    this->CameraBoom->SetUsingAbsoluteRotation(true);
    this->CameraBoom->SetRelativeRotation(FRotator(-60.0f, 0.0f, 0.0f));
    this->CameraBoom->TargetArmLength = 1800.0f;
    this->CameraBoom->bDoCollisionTest = false;
    this->Camera = this->CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
    this->Camera->SetupAttachment(this->CameraBoom);
    this->Feedback = this->CreateDefaultSubobject<UHBFeedbackComponent>(TEXT("Feedback"));
    this->Loadout = this->CreateDefaultSubobject<UHBLoadoutComponent>(TEXT("Loadout"));
}

bool AHBPlayerCharacter::TryActivateGASAction(EHBAbility Ability, const FHBAbilitySpec& Spec,
    AHBEnemyCharacter* Target, const FVector& AimPoint, bool bInPlace)
{
    // The prototype targets standalone/PIE authority. No client target-data RPC yet.
    if (!HasAuthority() || !AbilitySystem || bGASContextReady)
    {
        return false;
    }
    bGASContextReady = true;
    bLastGASActivationAccepted = false;
    PendingGASAbility = Ability;
    PendingGASSpec = Spec;
    PendingGASTarget = Target;
    PendingGASAim = AimPoint;
    bPendingGASInPlace = bInPlace;

    const TSubclassOf<UHBGameplayAbility> AbilityClass = Loadout->GetAbilityClass(Ability);
    // With a montage, Basic activates when the queued command actually starts its swing.
    const bool bAccepted = Ability == EHBAbility::EHBA_Basic && UsesAttackMontage()
        ? ExecutePendingGAS(Ability)
        : AbilityClass && AbilitySystem->TryActivateAbilityByClass(AbilityClass) && bLastGASActivationAccepted;
    bGASContextReady = false;
    PendingGASTarget.Reset();
    UHBDebugSubsystem::Record(this, TEXT("Combat"),
        FString::Printf(TEXT("GAS Activate %s accepted=%d mana=%.1f cooldown=%.2f"),
            *StaticEnum<EHBAbility>()->GetNameStringByValue(static_cast<int64>(Ability)),
            bAccepted, GetGASMana(), GetGASCooldown(Ability)));
    return bAccepted;
}

bool AHBPlayerCharacter::CanExecutePendingGAS(EHBAbility Ability) const
{
    if (!bGASContextReady || PendingGASAbility != Ability || !IsAlive() || bBuildMode || bDodging ||
        (bAttacking && Ability != EHBAbility::EHBA_Basic) || PendingGASAim.ContainsNaN())
    {
        return false;
    }
    AHBGameMode* Mode = GetWorld()->GetAuthGameMode<AHBGameMode>();
    if (!Mode || Mode->GetPhase() == EHBPhase::EHBP_GameOver)
    {
        return false;
    }
    AHBEnemyCharacter* Target = PendingGASTarget.Get();
    if (Ability == EHBAbility::EHBA_Mend)
    {
        return GetHealth() + KINDA_SMALL_NUMBER < GetMaxHealth();
    }
    if (Ability == EHBAbility::EHBA_Basic)
    {
        return !AbilitySystem->HasMatchingGameplayTag(HBGetPulseCastingTag()) && IsValid(Target) && Target->IsAlive() &&
            (!bPendingGASInPlace || FVector::DistSquaredXY(GetActorLocation(), Target->GetActorLocation()) <=
                FMath::Square(PendingGASSpec.fRange));
    }
    return true;
}

bool AHBPlayerCharacter::ExecutePendingGAS(EHBAbility Ability)
{
    return CanExecutePendingGAS(Ability) &&
        ExecuteAbility(Ability, PendingGASSpec, PendingGASTarget.Get(), PendingGASAim, bPendingGASInPlace);
}

bool AHBPlayerCharacter::ExecutePendingGuard(TSubclassOf<UGameplayEffect> GuardEffectClass)
{
    const FGameplayEffectSpecHandle Effect = AbilitySystem->MakeOutgoingSpec(GuardEffectClass, 1.0f, AbilitySystem->MakeEffectContext());
    if (!Effect.IsValid())
    {
        return false;
    }
    const FGameplayTag GuardTag = FGameplayTag::RequestGameplayTag(TEXT("State.Guarding"));
    const FGameplayEffectQuery GuardQuery = FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(FGameplayTagContainer(GuardTag));
    const TArray<FActiveGameplayEffectHandle> PreviousGuards = AbilitySystem->GetActiveEffects(GuardQuery);
    Effect.Data->SetSetByCallerMagnitude(FName(TEXT("Magnitude")), FMath::Clamp(PendingGASSpec.fPower, 0.0f, 0.95f));
    Effect.Data->SetDuration(PendingGASSpec.fDuration, true);
    Effect.Data->DynamicGrantedTags.AddTag(GuardTag);
    const FActiveGameplayEffectHandle NewGuard = AbilitySystem->ApplyGameplayEffectSpecToSelf(*Effect.Data);
    if (!NewGuard.IsValid())
    {
        return false;
    }
    for (const FActiveGameplayEffectHandle PreviousGuard : PreviousGuards)
    {
        if (PreviousGuard != NewGuard)
        {
            AbilitySystem->RemoveActiveGameplayEffect(PreviousGuard);
        }
    }
    return true;
}

void AHBPlayerCharacter::OnGuardingChanged(const FGameplayTag Tag, int32 NewCount)
{
    const FGameplayTag Cue = FGameplayTag::RequestGameplayTag(TEXT("GameplayCue.GASDemo.Guard"));
    if (NewCount > 0) { AbilitySystem->AddGameplayCue(Cue); }
    else { AbilitySystem->RemoveGameplayCue(Cue); }
}

AHBEnemyCharacter* AHBPlayerCharacter::GetPendingGASTarget() const
{
    return PendingGASTarget.Get();
}

void AHBPlayerCharacter::PresentPulseCue(FVector Location, float Radius, FLinearColor Color)
{
    UHBDebugSubsystem::Record(this, TEXT("Combat"), TEXT("GameplayCue.GASDemo.Pulse executed"));
    Feedback->PlayEffect(Location, Radius, Color);
}

float AHBPlayerCharacter::GetPendingGASCost(EHBAbility Ability) const
{
    return bGASContextReady && PendingGASAbility == Ability ? FMath::Max(0.0f, PendingGASSpec.GetManaCost(Ability)) : 0.0f;
}

float AHBPlayerCharacter::GetPendingGASCooldown(EHBAbility Ability) const
{
    return bGASContextReady && PendingGASAbility == Ability && Ability != EHBAbility::EHBA_Basic
        ? FMath::Max(0.0f, PendingGASSpec.fCooldown) : 0.0f;
}

float AHBPlayerCharacter::GetGASMana() const
{
    return Attributes ? Attributes->GetMana() : 0.0f;
}

float AHBPlayerCharacter::GetGASMaxMana() const
{
    return Attributes ? Attributes->GetMaxMana() : 100.0f;
}

void AHBPlayerCharacter::ApplyGASManaDelta(float Delta)
{
    if (!HasAuthority() || !AbilitySystem || !FMath::IsFinite(Delta) || FMath::IsNearlyZero(Delta))
    {
        return;
    }
    FGameplayEffectSpecHandle Effect = AbilitySystem->MakeOutgoingSpec(ManaEffectClass, 1.0f, AbilitySystem->MakeEffectContext());
    if (Effect.IsValid())
    {
        Effect.Data->SetSetByCallerMagnitude(FName(TEXT("Magnitude")), Delta);
        AbilitySystem->ApplyGameplayEffectSpecToSelf(*Effect.Data.Get());
    }
}

void AHBPlayerCharacter::ApplyGASCooldown(EHBAbility Ability, float Duration)
{
    if (!HasAuthority() || !AbilitySystem || !FMath::IsFinite(Duration) || Duration <= 0.0f)
    {
        return;
    }
    FGameplayEffectSpecHandle Effect = AbilitySystem->MakeOutgoingSpec(CooldownEffectClass, 1.0f, AbilitySystem->MakeEffectContext());
    if (Effect.IsValid())
    {
        Effect.Data->SetDuration(Duration, true);
        Effect.Data->DynamicGrantedTags.AddTag(HBGetCooldownTag(Ability));
        AbilitySystem->ApplyGameplayEffectSpecToSelf(*Effect.Data.Get());
    }
}

float AHBPlayerCharacter::GetGASCooldown(EHBAbility Ability) const
{
    return Ability == EHBAbility::EHBA_Basic ? 0.0f : GetGASEffectTimeRemaining(HBGetCooldownTag(Ability));
}

float AHBPlayerCharacter::GetGASEffectTimeRemaining(FGameplayTag Tag) const
{
    if (!AbilitySystem)
    {
        return 0.0f;
    }
    const FGameplayEffectQuery Query = FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(
        FGameplayTagContainer(Tag));
    const TArray<float> Remaining = AbilitySystem->GetActiveEffectsTimeRemaining(Query);
    float Longest = 0.0f;
    for (const float Seconds : Remaining)
    {
        Longest = FMath::Max(Longest, Seconds);
    }
    return Longest;
}

void AHBPlayerCharacter::ResetGASMana(float MaxMana)
{
    if (HasAuthority() && AbilitySystem && FMath::IsFinite(MaxMana))
    {
        AbilitySystem->SetNumericAttributeBase(UHBAttributes::GetMaxManaAttribute(), FMath::Max(1.0f, MaxMana));
        AbilitySystem->SetNumericAttributeBase(UHBAttributes::GetManaAttribute(), FMath::Max(1.0f, MaxMana));
    }
}

void AHBPlayerCharacter::ClearGASCooldowns()
{
    if (!HasAuthority() || !AbilitySystem)
    {
        return;
    }
    FGameplayTagContainer CooldownTags;
    for (int32 Index = 0; Index < HBRuleCore::iAbilityCount; ++Index)
    {
        CooldownTags.AddTag(HBGetCooldownTag(static_cast<EHBAbility>(Index)));
    }
    AbilitySystem->RemoveActiveEffects(FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(CooldownTags));
}

void AHBPlayerCharacter::RequestMove(const FVector& Position)
{
    if (!this->IsAlive() || this->bBuildMode || this->bDodging)
    {
        UHBDebugSubsystem::Record(this, TEXT("Command"), TEXT("Move rejected: dead, building or dodging"));
        return;
    }
    if (this->bAttacking)
    {
        UHBDebugSubsystem::Record(this, TEXT("Command"), TEXT("Move buffered; replaces previous buffer"));
        this->BufferedCommand = EHBCommand::EHBC_Move;
        this->BufferedDestination = Position;
        this->BufferedTarget.Reset();
        return;
    }
    UHBDebugSubsystem::Record(this, TEXT("Command"), FString::Printf(TEXT("Move accepted %s"), *Position.ToCompactString()));
    this->Command = EHBCommand::EHBC_Move;
    this->Destination = Position;
    this->AttackTarget.Reset();
    this->fBlockedSeconds = 0.0f;
}

void AHBPlayerCharacter::RequestAttack(AHBEnemyCharacter* Target, bool bInPlace)
{
    if (!this->IsAlive() || this->bBuildMode || this->bDodging || !IsValid(Target) || !Target->IsAlive())
    {
        UHBDebugSubsystem::Record(this, TEXT("Command"), TEXT("Attack rejected: player state or invalid target"));
        return;
    }
    if (this->bAttacking)
    {
        UHBDebugSubsystem::Record(this, TEXT("Command"), TEXT("Attack buffered; replaces previous buffer"));
        this->BufferedCommand = EHBCommand::EHBC_Attack;
        this->BufferedTarget = Target;
        this->bBufferedInPlace = bInPlace;
        return;
    }
    UHBDebugSubsystem::Record(this, TEXT("Command"), FString::Printf(TEXT("Attack accepted target=%s"), *GetNameSafe(Target)));
    this->Command = EHBCommand::EHBC_Attack;
    this->AttackTarget = Target;
    this->bInPlaceAttack = bInPlace;
    this->fBlockedSeconds = 0.0f;
    // An in-range press can attack while the stick is held; the swing owns movement.
    if (FVector::DistSquaredXY(this->GetActorLocation(), Target->GetActorLocation()) <= FMath::Square(this->fAttackRange))
    {
        this->StartAttack();
    }
}

void AHBPlayerCharacter::SetDirectInput(const FVector2D& Input)
{
    this->DirectInput = Input.GetClampedToMaxSize(1.0);
    if (!this->DirectInput.IsNearlyZero() && !this->bAttacking)
    {
        this->Command = EHBCommand::EHBC_None;
        this->AttackTarget.Reset();
    }
}

void AHBPlayerCharacter::SetAimInput(const FVector2D& Input)
{
    this->AimInput = Input;
    if (!Input.IsNearlyZero())
    {
        this->SetAimWorldDirection(this->CameraInputToWorld(Input));
    }
}

void AHBPlayerCharacter::SetHover(AHBEnemyCharacter* Target)
{
    this->HoverTarget = Target;
}

AHBEnemyCharacter* AHBPlayerCharacter::GetAttackTarget() const
{
    return this->Command == EHBCommand::EHBC_Attack ? this->AttackTarget.Get() : nullptr;
}

void AHBPlayerCharacter::FaceDirection(const FVector& Direction)
{
    if (!Direction.IsNearlyZero())
    {
        const FRotator Desired(0.0f, Direction.Rotation().Yaw, 0.0f);
        this->SetActorRotation(FMath::RInterpConstantTo(this->GetActorRotation(), Desired, this->GetWorld()->GetDeltaSeconds(), this->fTurnSpeed));
    }
}

void AHBPlayerCharacter::MoveToward(const FVector& Position, float fDeltaSeconds)
{
    FVector Direction = Position - this->GetActorLocation();
    Direction.Z = 0.0f;
    this->FaceDirection(Direction);
    this->AddMovementInput(Direction.GetSafeNormal());
    if (this->GetVelocity().SizeSquared2D() < 25.0f)
    {
        this->fBlockedSeconds += fDeltaSeconds;
        if (this->fBlockedSeconds > 1.5f)
        {
            this->CancelCommands();
        }
    }
    else
    {
        this->fBlockedSeconds = 0.0f;
    }
}

void AHBPlayerCharacter::UpdateCommand(float fDeltaSeconds)
{
    if (this->Command == EHBCommand::EHBC_Move)
    {
        if (FVector::DistSquaredXY(this->GetActorLocation(), this->Destination) <= FMath::Square(this->fArrivalDistance))
        {
            this->Command = EHBCommand::EHBC_None;
            this->GetCharacterMovement()->StopMovementImmediately();
            return;
        }
        this->MoveToward(this->Destination, fDeltaSeconds);
    }
    else if (this->Command == EHBCommand::EHBC_Attack)
    {
        AHBEnemyCharacter* Target = this->AttackTarget.Get();
        if (!IsValid(Target) || !Target->IsAlive())
        {
            this->Command = EHBCommand::EHBC_None;
            this->AttackTarget.Reset();
            return;
        }
        if (FVector::DistSquaredXY(this->GetActorLocation(), Target->GetActorLocation()) <= FMath::Square(this->fAttackRange))
        {
            this->StartAttack();
        }
        else if (this->bInPlaceAttack)
        {
            this->Command = EHBCommand::EHBC_None;
            this->AttackTarget.Reset();
        }
        else
        {
            this->MoveToward(Target->GetActorLocation(), fDeltaSeconds);
        }
    }
}

void AHBPlayerCharacter::Tick(float fDeltaSeconds)
{
    Super::Tick(fDeltaSeconds);
    if (HasArenaArt() && IsLocallyControlled())
    {
        const AHBGameMode* Mode = GetWorld()->GetAuthGameMode<AHBGameMode>();
        const bool bDungeon = Mode && Mode->GetPhase() == EHBPhase::EHBP_Dungeon;
        const float Distance = bBuildMode ? 1800.0f : (bDungeon ? 1430.0f : 1640.0f);
        FRotator Angle = CameraBoom->GetComponentRotation();
        Angle.Pitch = FMath::FInterpTo(Angle.Pitch, bBuildMode ? -60.0 : (bDungeon ? -52.0 : -57.0),
            fDeltaSeconds, 5.0);
        CameraBoom->SetWorldRotation(Angle);
        CameraBoom->TargetArmLength = FMath::FInterpTo(CameraBoom->TargetArmLength, Distance, fDeltaSeconds, 5.0f);
    }
    if (!this->IsAlive() || !this->IsLocallyControlled() || this->bBuildMode)
    {
        this->Feedback->HideAll();
        return;
    }
    if (this->bDodging)
    {
        if (this->GetWorld()->GetTimeSeconds() >= this->fDodgeEndTime)
        {
            this->EndDodge();
        }
        else
        {
            this->AddMovementInput(this->DodgeDirection, 1.0f);
        }
        this->UpdateFeedback();
        return;
    }
    if (!this->bAttacking && !AbilitySystem->HasMatchingGameplayTag(HBGetPulseCastingTag()))
    {
        if (!this->DirectInput.IsNearlyZero())
        {
            const FVector Direction = this->CameraInputToWorld(this->DirectInput);
            this->Command = EHBCommand::EHBC_None;
            this->AttackTarget.Reset();
            this->AddMovementInput(Direction.GetSafeNormal(), this->DirectInput.Size());
            this->FaceDirection(this->bHasAim ? this->AimDirection : Direction);
        }
        else
        {
            this->UpdateCommand(fDeltaSeconds);
            if (this->Command == EHBCommand::EHBC_None && this->bHasAim)
            {
                this->FaceDirection(this->AimDirection);
            }
        }
    }
    this->UpdateFeedback();
}

void AHBPlayerCharacter::UpdateFeedback()
{
    AHBEnemyCharacter* Hover = this->HoverTarget.Get();
    AHBEnemyCharacter* Target = this->GetAttackTarget();
    this->Feedback->Present(this->Command == EHBCommand::EHBC_Move, this->Destination,
        IsValid(Hover) && Hover->IsAlive() ? Hover : nullptr, IsValid(Target) && Target->IsAlive() ? Target : nullptr);
}

void AHBPlayerCharacter::StartAttack()
{
    this->bAttacking = true;
    this->bHitDelivered = false;
    this->GetCharacterMovement()->StopMovementImmediately();
    if (this->AttackTarget.IsValid())
    {
        const FVector Direction = this->AttackTarget->GetActorLocation() - this->GetActorLocation();
        this->SetActorRotation(FRotator(0.0f, Direction.Rotation().Yaw, 0.0f));
    }
    if (this->AttackMontage)
    {
        this->AttackPlayback = TEXT("Montage");
        if (!AbilitySystem->TryActivateAbilityByClass(Loadout->GetAbilityClass(EHBAbility::EHBA_Basic)))
        {
            this->OnAttackMontageFinished(true);
        }
        return;
    }
    UHBDebugSubsystem::Record(this, TEXT("Animation"), TEXT("Timed fallback started; no montage assigned"));
    this->AttackPlayback = TEXT("Timed prototype");
    this->GetWorldTimerManager().SetTimer(this->AttackHitTimer, this, &AHBPlayerCharacter::DeliverAttackHit, FMath::Max(0.01f, this->fFallbackWindup), false);
    this->GetWorldTimerManager().SetTimer(this->AttackEndTimer, this, &AHBPlayerCharacter::OnAttackTimeout, FMath::Max(this->fFallbackWindup + 0.01f, this->fFallbackDuration), false);
}

void AHBPlayerCharacter::DeliverAnimationHit(UAnimSequenceBase* Animation)
{
    UHBDebugSubsystem::Record(this, TEXT("Animation"), FString::Printf(TEXT("Notify received asset=%s"), *GetNameSafe(Animation)));
    UAnimInstance* Anim = this->GetMesh()->GetAnimInstance();
    if (Animation == this->AttackMontage && Anim && Anim->Montage_IsActive(this->AttackMontage))
    {
        const FGameplayTag HitTag = FGameplayTag::RequestGameplayTag(TEXT("Event.Attack.Hit"));
        FGameplayEventData Event;
        Event.EventTag = HitTag;
        Event.Instigator = this;
        Event.Target = AttackTarget.Get();
        UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(this, HitTag, Event);
    }
    else
    {
        UHBDebugSubsystem::Record(this, TEXT("Animation"), TEXT("Notify ignored: wrong asset or inactive montage"));
    }
}

void AHBPlayerCharacter::DeliverAttackHit()
{
    if (!this->bAttacking || this->bHitDelivered || !this->IsAlive())
    {
        UHBDebugSubsystem::Record(this, TEXT("Animation"), TEXT("Hit ignored: duplicate, late or dead"));
        return;
    }
    UHBDebugSubsystem::Record(this, TEXT("Animation"), TEXT("Hit event accepted (notify or fallback)"));
    this->bHitDelivered = true;
    UE_LOG(LogHideInTheBox, Log, TEXT("Attack hit event: source=%s player=%s"), *this->AttackPlayback, *this->GetName());
    FGameplayCueParameters SwingCue;
    SwingCue.Location = GetActorLocation() + GetActorForwardVector() * 76.0f + FVector(0, 0, 33);
    SwingCue.Normal = GetActorForwardVector();
    AbilitySystem->ExecuteGameplayCue(FGameplayTag::RequestGameplayTag(TEXT("GameplayCue.GASDemo.BasicSwing")), SwingCue);
    AHBEnemyCharacter* Target = this->AttackTarget.Get();
    if (IsValid(Target) && Target->IsAlive() && FVector::DistSquaredXY(this->GetActorLocation(), Target->GetActorLocation()) <= FMath::Square(this->fAttackRange + 25.0f) && this->HasHitLine(Target->GetActorLocation(), Target))
    {
        FGameplayCueParameters HitCue;
        HitCue.Location = Target->GetActorLocation() + FVector(0, 0, 12);
        const float Damage = HBApplyDamageEffect(this, Target, this->fAttackDamage);
        ++this->iHitCount;
        if (Damage > 0.0f)
        {
            AbilitySystem->ExecuteGameplayCue(FGameplayTag::RequestGameplayTag(TEXT("GameplayCue.GASDemo.BasicHit")), HitCue);
        }
    }
    else
    {
        UHBDebugSubsystem::Record(this, TEXT("Combat"), TEXT("Melee missed: target gone, outside range or blocked line"));
    }
}

void AHBPlayerCharacter::FinishAttack()
{
    if (!this->bAttacking)
    {
        return;
    }
    this->GetWorldTimerManager().ClearTimer(this->AttackHitTimer);
    this->GetWorldTimerManager().ClearTimer(this->AttackEndTimer);
    this->bAttacking = false;
    this->Command = EHBCommand::EHBC_None;
    this->AttackTarget.Reset();
    UHBDebugSubsystem::Record(this, TEXT("Command"), TEXT("Swing released; consume buffered command if present"));
    const EHBCommand NextCommand = this->BufferedCommand;
    this->BufferedCommand = EHBCommand::EHBC_None;
    if (NextCommand == EHBCommand::EHBC_Move)
    {
        this->RequestMove(this->BufferedDestination);
    }
    else if (NextCommand == EHBCommand::EHBC_Attack)
    {
        this->RequestAttack(this->BufferedTarget.Get(), this->bBufferedInPlace);
    }
    this->BufferedTarget.Reset();
}

void AHBPlayerCharacter::OnAttackMontageFinished(bool bInterrupted)
{
    if (!this->bAttacking)
    {
        return;
    }
    this->LastAttackOutcome = bInterrupted ? TEXT("Interrupted") : (this->bHitDelivered ? TEXT("Completed") : TEXT("Completed without notify"));
    UHBDebugSubsystem::Record(this, TEXT("Animation"), this->LastAttackOutcome);
    UE_LOG(LogHideInTheBox, Log, TEXT("Attack ended: %s"), *this->LastAttackOutcome);
    if (bInterrupted)
    {
        this->BufferedCommand = EHBCommand::EHBC_None;
    }
    this->FinishAttack();
}

void AHBPlayerCharacter::CancelCommands()
{
    if (this->bAttacking || this->bDodging || this->Command != EHBCommand::EHBC_None || this->HasBufferedCommand())
    {
        UHBDebugSubsystem::Record(this, TEXT("Command"), TEXT("Cancel: command, buffer and attack timers cleared"));
    }
    if (this->bAttacking)
    {
        this->LastAttackOutcome = TEXT("Canceled by command / lifecycle");
        UE_LOG(LogHideInTheBox, Log, TEXT("Attack canceled on %s"), *this->GetName());
    }
    this->EndDodge();
    this->bHasAim = false;
    this->bAttacking = false;
    this->Command = EHBCommand::EHBC_None;
    this->BufferedCommand = EHBCommand::EHBC_None;
    this->AttackTarget.Reset();
    this->BufferedTarget.Reset();
    this->HoverTarget.Reset();
    this->DirectInput = FVector2D::ZeroVector;
    this->AimInput = FVector2D::ZeroVector;
    this->GetWorldTimerManager().ClearTimer(this->AttackHitTimer);
    this->GetWorldTimerManager().ClearTimer(this->AttackEndTimer);
    if (this->AttackMontage)
    {
        if (const FGameplayAbilitySpec* Spec = AbilitySystem->FindAbilitySpecFromClass(Loadout->GetAbilityClass(EHBAbility::EHBA_Basic)))
        {
            AbilitySystem->CancelAbilityHandle(Spec->Handle);
        }
    }
    if (AbilitySystem->HasMatchingGameplayTag(HBGetPulseCastingTag()))
    {
        const FGameplayTagContainer PulseTags(HBGetPulseAbilityTag());
        AbilitySystem->CancelAbilities(&PulseTags);
    }
    this->StopAnimMontage();
    this->GetCharacterMovement()->StopMovementImmediately();
    this->Feedback->HideAll();
}

bool AHBPlayerCharacter::ExecuteAbility(EHBAbility Ability, const FHBAbilitySpec& Spec, AHBEnemyCharacter* Target, const FVector& AimPoint, bool bInPlace)
{
    AHBGameMode* Mode = this->GetWorld()->GetAuthGameMode<AHBGameMode>();
    if (!Mode || Mode->GetPhase() == EHBPhase::EHBP_GameOver || !this->IsAlive() || AimPoint.ContainsNaN())
    {
        return false;
    }
    const FVector FootPosition = this->GetActorLocation() - FVector(0.0f, 0.0f, this->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
    if (Ability == EHBAbility::EHBA_Basic)
    {
        if (!IsValid(Target) || !Target->IsAlive() || (bInPlace && FVector::DistSquaredXY(this->GetActorLocation(), Target->GetActorLocation()) > FMath::Square(Spec.fRange)))
        {
            return false;
        }
        this->fAttackDamage = FMath::Max(0.0f, Spec.fPower);
        this->fAttackRange = Spec.fRange;
        this->RequestAttack(Target, bInPlace);
        return this->bAttacking || this->Command == EHBCommand::EHBC_Attack || this->BufferedCommand == EHBCommand::EHBC_Attack;
    }
    if (Ability == EHBAbility::EHBA_Dodge)
    {
        const FGameplayEffectSpecHandle Immunity = AbilitySystem->MakeOutgoingSpec(UHBGE_DodgeImmunity::StaticClass(),
            1.0f, AbilitySystem->MakeEffectContext());
        if (!Immunity.IsValid())
        {
            return false;
        }
        Immunity.Data->SetDuration(FMath::Min(0.15f, Spec.fDuration), true);
        Immunity.Data->DynamicGrantedTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("State.DamageImmune.Dodge")));
        const FVector Direction = !this->DirectInput.IsNearlyZero() ? this->CameraInputToWorld(this->DirectInput) : (this->bHasAim ? this->AimDirection : this->GetActorForwardVector());
        this->CancelCommands();
        if (!AbilitySystem->ApplyGameplayEffectSpecToSelf(*Immunity.Data).IsValid())
        {
            return false;
        }
        this->DodgeDirection = Direction.GetSafeNormal2D();
        this->bDodging = true;
        FGameplayCueParameters Cue;
        Cue.Location = FootPosition;
        Cue.Normal = DodgeDirection;
        AbilitySystem->ExecuteGameplayCue(FGameplayTag::RequestGameplayTag(TEXT("GameplayCue.GASDemo.Dodge")), Cue);
        this->fDodgeEndTime = this->GetWorld()->GetTimeSeconds() + Spec.fDuration;
        this->fSavedWalkSpeed = this->GetCharacterMovement()->MaxWalkSpeed;
        this->GetCharacterMovement()->MaxWalkSpeed = FMath::Min(1600.0f, Spec.fRange / Spec.fDuration);
        this->GetCharacterMovement()->Velocity = this->DodgeDirection * this->GetCharacterMovement()->MaxWalkSpeed;
        return true;
    }
    if (Ability == EHBAbility::EHBA_Mend)
    {
        const bool bHealed = this->Heal(Spec.fPower) > 0.0f;
        if (bHealed)
        {
            FGameplayCueParameters Cue;
            Cue.Location = FootPosition;
            AbilitySystem->ExecuteGameplayCue(FGameplayTag::RequestGameplayTag(TEXT("GameplayCue.GASDemo.Mend")), Cue);
        }
        return bHealed;
    }
    return false;
}

void AHBPlayerCharacter::SetBuildMode(bool bEnabled)
{
    this->CancelCommands();
    this->bBuildMode = bEnabled;
    this->Loadout->RefreshManaRegeneration();
}

void AHBPlayerCharacter::ResetCombatState()
{
    this->CancelCommands();
    this->RestoreHealth();
    this->Loadout->ResetResources();
    const FGameplayTag GuardTag = FGameplayTag::RequestGameplayTag(TEXT("State.Guarding"));
    AbilitySystem->RemoveActiveEffects(FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(FGameplayTagContainer(GuardTag)));
}

void AHBPlayerCharacter::ResetForPhase(const FVector& Position)
{
    this->bBuildMode = false;
    this->ResetCombatState();
    this->SetActorLocation(Position, false, nullptr, ETeleportType::TeleportPhysics);
}

void AHBPlayerCharacter::OnDeath()
{
    this->CancelCommands();
    this->Loadout->RefreshManaRegeneration();
    AHBGameMode* Mode = this->GetWorld()->GetAuthGameMode<AHBGameMode>();
    if (Mode)
    {
        Mode->HandlePlayerDeath();
    }
}

void AHBPlayerCharacter::PossessedBy(AController* NewController)
{
    Super::PossessedBy(NewController);
    this->Loadout->RefreshManaRegeneration();
}

void AHBPlayerCharacter::UnPossessed()
{
    this->CancelCommands();
    Super::UnPossessed();
    this->Loadout->RefreshManaRegeneration();
}

void AHBPlayerCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    this->CancelCommands();
    Super::EndPlay(EndPlayReason);
}

FVector AHBPlayerCharacter::CameraInputToWorld(const FVector2D& Input) const
{
    const FRotator CameraYaw(0.0f, this->CameraBoom->GetComponentRotation().Yaw, 0.0f);
    return CameraYaw.RotateVector(FVector(Input.Y, Input.X, 0.0f));
}

void AHBPlayerCharacter::SetCameraYaw(float fYaw)
{
    if (FMath::IsFinite(fYaw))
    {
        this->CameraBoom->SetWorldRotation(FRotator(-60.0f, fYaw, 0.0f));
    }
}

void AHBPlayerCharacter::SetAimWorldDirection(const FVector& Direction)
{
    if (!Direction.ContainsNaN() && !Direction.IsNearlyZero())
    {
        this->AimDirection = Direction.GetSafeNormal2D();
        this->bHasAim = true;
    }
}

float AHBPlayerCharacter::GetAimError() const
{
    return this->bHasAim ? FMath::Abs(FMath::FindDeltaAngleDegrees(this->GetActorRotation().Yaw, this->AimDirection.Rotation().Yaw)) : 0.0f;
}

void AHBPlayerCharacter::EndDodge()
{
    if (this->bDodging)
    {
        this->bDodging = false;
        const FGameplayTag DodgeTag = FGameplayTag::RequestGameplayTag(TEXT("State.DamageImmune.Dodge"));
        AbilitySystem->RemoveActiveEffects(FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(FGameplayTagContainer(DodgeTag)));
        this->GetCharacterMovement()->MaxWalkSpeed = this->fSavedWalkSpeed;
        this->GetCharacterMovement()->StopMovementImmediately();
    }
}

bool AHBPlayerCharacter::HasHitLine(const FVector& Point, const AActor* Target) const
{
    FCollisionQueryParams Parameters(SCENE_QUERY_STAT(HBAttackLine), false, this);
    FHitResult Hit;
    const bool bBlocked = this->GetWorld()->LineTraceSingleByChannel(Hit, this->GetActorLocation(), Point, ECC_Visibility, Parameters);
    return !bBlocked || Hit.GetActor() == Target;
}

void AHBPlayerCharacter::OnAttackTimeout()
{
    this->LastAttackOutcome = this->AttackMontage ? TEXT("Watchdog timeout") : TEXT("Timed attack completed");
    UHBDebugSubsystem::Record(this, TEXT("Animation"), this->LastAttackOutcome);
    UE_LOG(LogHideInTheBox, Log, TEXT("Attack ended: %s"), *this->LastAttackOutcome);
    this->FinishAttack();
}

void AHBPlayerCharacter::BeginPlay()
{
    Super::BeginPlay();
    this->Loadout->RefreshManaRegeneration();
    if (HasAuthority() && AbilitySystem)
    {
        AbilitySystem->RegisterGameplayTagEvent(FGameplayTag::RequestGameplayTag(TEXT("State.Guarding")),
            EGameplayTagEventType::NewOrRemoved).AddUObject(this, &AHBPlayerCharacter::OnGuardingChanged);
        for (int32 Index = 0; Index < HBRuleCore::iAbilityCount; ++Index)
        {
            const TSubclassOf<UHBGameplayAbility> AbilityClass = Loadout->GetAbilityClass(static_cast<EHBAbility>(Index));
            AbilitySystem->GiveAbility(FGameplayAbilitySpec(AbilityClass, 1));
        }
        UHBDebugSubsystem::Record(this, TEXT("Combat"), TEXT("GAS: six gameplay abilities granted on server"));
    }
    const UAnimInstance* Anim = this->GetMesh()->GetAnimInstance();
    UE_LOG(LogHideInTheBox, Log, TEXT("Runtime identity: pawn=%s class=%s anim=%s montage=%s"), *this->GetName(), *this->GetClass()->GetName(), Anim ? *Anim->GetClass()->GetName() : TEXT("NONE"), *GetNameSafe(this->AttackMontage));
    if (this->AttackMontage && !Anim)
    {
        UE_LOG(LogHideInTheBox, Warning, TEXT("Attack montage is assigned without an AnimInstance. Wire mesh and Anim Class before testing damage."));
    }
}

void AHBPlayerCharacter::AppendDebugChecks(TArray<FString>& Checks) const
{
    const USkeletalMesh* MeshAsset = this->GetMesh()->GetSkeletalMeshAsset();
    const UAnimInstance* Anim = this->GetMesh()->GetAnimInstance();
    Checks.Add(MeshAsset ? TEXT("OK Skeletal mesh assigned.") : TEXT("INFO No skeletal mesh: native placeholder mode."));
    if (this->AttackMontage)
    {
        Checks.Add(Anim ? TEXT("OK AnimInstance exists; playback still needs runtime test.") : TEXT("ERROR Montage assigned but no AnimInstance: set Mesh + Anim Class."));
        if (MeshAsset && MeshAsset->GetSkeleton() != this->AttackMontage->GetSkeleton())
        {
            Checks.Add(TEXT("WARN Mesh/montage skeletons differ; verify compatibility in Editor."));
        }
        Checks.Add(TEXT("CHECK DefaultSlot output + HB Attack Hit on montage; verify by attacking."));
    }
    else
    {
        Checks.Add(TEXT("INFO No montage: timed prototype attack enabled."));
    }
    if (!MeshAsset && !this->TintMaterial)
    {
        Checks.Add(TEXT("WARN Placeholder Tint Material missing; assign M_DemoTint for colors."));
    }
    Checks.Add(this->Feedback->HasRingMaterial() ? TEXT("OK Feedback Ring Material assigned; verify Color/Opacity parameters in Editor.") : TEXT("WARN Feedback Ring Material missing; assign M_DemoRing for tint/fade."));
    Checks.Add(AbilitySystem && Attributes ? TEXT("OK GAS: ASC + AttributeSet attached.") : TEXT("ERROR GAS: missing ASC/AttributeSet."));
    Checks.Add(FString::Printf(TEXT("GAS HP %.0f/%.0f MP %.0f/%.0f"), GetHealth(), GetMaxHealth(), GetGASMana(), GetGASMaxMana()));
    this->Loadout->AppendDebugChecks(Checks);
}
