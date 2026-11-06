#include "HBPlayerCharacter.h"
#include "HBAttributes.h"
#include "HBEffects.h"
#include "HBGameplayAbility.h"
#include "AbilitySystemComponent.h"
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
#include "GameFramework/DamageType.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "TimerManager.h"

AHBPlayerCharacter::AHBPlayerCharacter()
{
    this->PrimaryActorTick.bCanEverTick = true;
    this->Tint = FLinearColor(0.1f, 0.7f, 1.0f);
    this->fMaxHealth = 220.0f;
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

    TSubclassOf<UGameplayAbility> AbilityClass;
    switch (Ability)
    {
    case EHBAbility::EHBA_Basic: AbilityClass = UHBGA_Basic::StaticClass(); break;
    case EHBAbility::EHBA_Pulse: AbilityClass = UHBGA_Pulse::StaticClass(); break;
    case EHBAbility::EHBA_Dodge: AbilityClass = UHBGA_Dodge::StaticClass(); break;
    case EHBAbility::EHBA_Guard: AbilityClass = UHBGA_Guard::StaticClass(); break;
    case EHBAbility::EHBA_Mend: AbilityClass = UHBGA_Mend::StaticClass(); break;
    case EHBAbility::EHBA_Bolt: AbilityClass = UHBGA_Bolt::StaticClass(); break;
    }
    const bool bActivated = AbilityClass && AbilitySystem->TryActivateAbilityByClass(AbilityClass);
    const bool bAccepted = bActivated && bLastGASActivationAccepted;
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
    if (Ability == EHBAbility::EHBA_Pulse)
    {
        return FVector::DistSquaredXY(GetActorLocation(), PendingGASAim) <= FMath::Square(PendingGASSpec.fRange);
    }
    if (Ability == EHBAbility::EHBA_Basic || Ability == EHBAbility::EHBA_Bolt)
    {
        if (!IsValid(Target) || !Target->IsAlive())
        {
            return false;
        }
        const bool bInRange = FVector::DistSquaredXY(GetActorLocation(), Target->GetActorLocation()) <= FMath::Square(PendingGASSpec.fRange);
        if (Ability == EHBAbility::EHBA_Bolt)
        {
            return bInRange && HasHitLine(Target->GetActorLocation(), Target);
        }
        return !bPendingGASInPlace || bInRange;
    }
    return true;
}

bool AHBPlayerCharacter::ExecutePendingGAS(EHBAbility Ability)
{
    return CanExecutePendingGAS(Ability) &&
        ExecuteAbility(Ability, PendingGASSpec, PendingGASTarget.Get(), PendingGASAim, bPendingGASInPlace);
}

float AHBPlayerCharacter::GetPendingGASCost(EHBAbility Ability) const
{
    return bGASContextReady && PendingGASAbility == Ability && Ability != EHBAbility::EHBA_Basic &&
        Ability != EHBAbility::EHBA_Dodge ? FMath::Max(0.0f, PendingGASSpec.fManaCost) : 0.0f;
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
    FGameplayEffectSpecHandle Effect = AbilitySystem->MakeOutgoingSpec(UHBGE_ManaDelta::StaticClass(), 1.0f, AbilitySystem->MakeEffectContext());
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
    FGameplayEffectSpecHandle Effect = AbilitySystem->MakeOutgoingSpec(UHBGE_Cooldown::StaticClass(), 1.0f, AbilitySystem->MakeEffectContext());
    if (Effect.IsValid())
    {
        Effect.Data->SetDuration(Duration, true);
        Effect.Data->DynamicGrantedTags.AddTag(HBGetCooldownTag(Ability));
        AbilitySystem->ApplyGameplayEffectSpecToSelf(*Effect.Data.Get());
    }
}

float AHBPlayerCharacter::GetGASCooldown(EHBAbility Ability) const
{
    if (!AbilitySystem || Ability == EHBAbility::EHBA_Basic)
    {
        return 0.0f;
    }
    const FGameplayEffectQuery Query = FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(
        FGameplayTagContainer(HBGetCooldownTag(Ability)));
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
    if (!this->IsAlive() || !this->IsLocallyControlled() || this->bBuildMode)
    {
        this->Feedback->HideAll();
        return;
    }
    this->Loadout->Regenerate(fDeltaSeconds);
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
    if (!this->bAttacking)
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
    UAnimInstance* Anim = this->GetMesh()->GetAnimInstance();
    if (this->AttackMontage)
    {
        const float fDuration = Anim ? Anim->Montage_Play(this->AttackMontage) : 0.0f;
        if (fDuration <= 0.0f)
        {
            UHBDebugSubsystem::Record(this, TEXT("Animation"), TEXT("Playback FAILED: check mesh, AnimBP, skeleton and slot"));
            this->AttackPlayback = TEXT("Playback failed");
            this->LastAttackOutcome = TEXT("Playback failed: check mesh / AnimBP / skeleton");
            UE_LOG(LogHideInTheBox, Warning, TEXT("Attack montage could not play on %s"), *this->GetName());
            this->bAttacking = false;
            this->Command = EHBCommand::EHBC_None;
            this->AttackTarget.Reset();
            return;
        }
        UHBDebugSubsystem::Record(this, TEXT("Animation"), TEXT("Montage started"));
        this->AttackPlayback = TEXT("Montage");
        FOnMontageEnded EndDelegate;
        EndDelegate.BindUObject(this, &AHBPlayerCharacter::OnMontageEnded);
        Anim->Montage_SetEndDelegate(EndDelegate, this->AttackMontage);
        this->GetWorldTimerManager().SetTimer(this->AttackEndTimer, this, &AHBPlayerCharacter::OnAttackTimeout, fDuration + 0.5f, false);
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
        this->DeliverAttackHit();
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
    AHBEnemyCharacter* Target = this->AttackTarget.Get();
    if (IsValid(Target) && Target->IsAlive() && FVector::DistSquaredXY(this->GetActorLocation(), Target->GetActorLocation()) <= FMath::Square(this->fAttackRange + 25.0f) && this->HasHitLine(Target->GetActorLocation(), Target))
    {
        UGameplayStatics::ApplyDamage(Target, this->fAttackDamage, this->GetController(), this, UDamageType::StaticClass());
        ++this->iHitCount;
        this->Feedback->PlayEffect(this->GetActorLocation() - FVector(0.0f, 0.0f, this->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()), 90.0f, FLinearColor(1.0f, 0.5f, 0.15f));
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
    UAnimInstance* Anim = this->GetMesh()->GetAnimInstance();
    if (Anim && this->AttackMontage && Anim->Montage_IsActive(this->AttackMontage))
    {
        FOnMontageEnded EmptyDelegate;
        Anim->Montage_SetEndDelegate(EmptyDelegate, this->AttackMontage);
        Anim->Montage_Stop(0.0f, this->AttackMontage);
    }
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

void AHBPlayerCharacter::OnMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
    if (Montage != this->AttackMontage || !this->bAttacking)
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
    UAnimInstance* Anim = this->GetMesh()->GetAnimInstance();
    if (Anim && this->AttackMontage)
    {
        FOnMontageEnded EmptyDelegate;
        Anim->Montage_SetEndDelegate(EmptyDelegate, this->AttackMontage);
        Anim->Montage_Stop(0.0f, this->AttackMontage);
    }
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
        const FVector Direction = !this->DirectInput.IsNearlyZero() ? this->CameraInputToWorld(this->DirectInput) : (this->bHasAim ? this->AimDirection : this->GetActorForwardVector());
        this->CancelCommands();
        this->DodgeDirection = Direction.GetSafeNormal2D();
        this->bDodging = true;
        this->Feedback->PlayEffect(FootPosition, 100.0f, FLinearColor(0.2f, 0.8f, 1.0f));
        this->fDodgeEndTime = this->GetWorld()->GetTimeSeconds() + Spec.fDuration;
        this->fInvulnerableUntil = this->GetWorld()->GetTimeSeconds() + FMath::Min(0.15f, Spec.fDuration);
        this->fSavedWalkSpeed = this->GetCharacterMovement()->MaxWalkSpeed;
        this->GetCharacterMovement()->MaxWalkSpeed = FMath::Min(1600.0f, Spec.fRange / Spec.fDuration);
        this->GetCharacterMovement()->Velocity = this->DodgeDirection * this->GetCharacterMovement()->MaxWalkSpeed;
        return true;
    }
    if (Ability == EHBAbility::EHBA_Guard)
    {
        this->Feedback->PlayEffect(FootPosition, 110.0f, FLinearColor(0.65f, 0.35f, 1.0f));
        this->fGuardUntil = this->GetWorld()->GetTimeSeconds() + Spec.fDuration;
        this->fGuardReduction = FMath::Clamp(Spec.fPower, 0.0f, 0.95f);
        return true;
    }
    if (Ability == EHBAbility::EHBA_Mend)
    {
        const bool bHealed = this->Heal(Spec.fPower) > 0.0f;
        if (bHealed)
        {
            this->Feedback->PlayEffect(FootPosition, 100.0f, FLinearColor(0.15f, 1.0f, 0.45f));
        }
        return bHealed;
    }
    if (Ability == EHBAbility::EHBA_Bolt)
    {
        if (!IsValid(Target) || !Target->IsAlive() || FVector::DistSquaredXY(this->GetActorLocation(), Target->GetActorLocation()) > FMath::Square(Spec.fRange) || !this->HasHitLine(Target->GetActorLocation(), Target))
        {
            return false;
        }
        this->Feedback->PlayEffect(Target->GetActorLocation() - FVector(0.0f, 0.0f, Target->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()), 80.0f, FLinearColor(0.4f, 0.65f, 1.0f));
        UGameplayStatics::ApplyDamage(Target, FMath::Max(0.0f, Spec.fPower), this->GetController(), this, UDamageType::StaticClass());
        return true;
    }
    if (Ability == EHBAbility::EHBA_Pulse)
    {
        if (FVector::DistSquaredXY(this->GetActorLocation(), AimPoint) > FMath::Square(Spec.fRange))
        {
            return false;
        }
        FVector EffectPosition = AimPoint;
        EffectPosition.Z = FootPosition.Z;
        this->Feedback->PlayEffect(EffectPosition, Spec.fRadius, FLinearColor(1.0f, 0.5f, 0.12f));
        const TArray<TWeakObjectPtr<AHBEnemyCharacter>> Targets = Mode->GetEnemies();
        for (const TWeakObjectPtr<AHBEnemyCharacter>& Entry : Targets)
        {
            AHBEnemyCharacter* Enemy = Entry.Get();
            if (IsValid(Enemy) && Enemy->IsAlive() && FVector::DistSquaredXY(AimPoint, Enemy->GetActorLocation()) <= FMath::Square(Spec.fRadius) && this->HasHitLine(Enemy->GetActorLocation(), Enemy))
            {
                UGameplayStatics::ApplyDamage(Enemy, FMath::Max(0.0f, Spec.fPower), this->GetController(), this, UDamageType::StaticClass());
            }
        }
        return true;
    }
    return false;
}

void AHBPlayerCharacter::SetBuildMode(bool bEnabled)
{
    this->CancelCommands();
    this->bBuildMode = bEnabled;
}

void AHBPlayerCharacter::ResetForPhase(const FVector& Position)
{
    this->SetBuildMode(false);
    this->RestoreHealth();
    this->Loadout->ResetResources();
    this->fGuardUntil = 0.0f;
    this->SetActorLocation(Position, false, nullptr, ETeleportType::TeleportPhysics);
}

void AHBPlayerCharacter::OnDeath()
{
    this->CancelCommands();
    AHBGameMode* Mode = this->GetWorld()->GetAuthGameMode<AHBGameMode>();
    if (Mode)
    {
        Mode->HandlePlayerDeath();
    }
}

void AHBPlayerCharacter::UnPossessed()
{
    this->CancelCommands();
    Super::UnPossessed();
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
        this->fInvulnerableUntil = 0.0f;
        this->GetCharacterMovement()->MaxWalkSpeed = this->fSavedWalkSpeed;
        this->GetCharacterMovement()->StopMovementImmediately();
    }
}

float AHBPlayerCharacter::TakeDamage(float fDamage, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
    const float fNow = this->GetWorld()->GetTimeSeconds();
    if (this->bDodging && fNow < this->fInvulnerableUntil)
    {
        UHBDebugSubsystem::Record(this, TEXT("Combat"), TEXT("Damage avoided: dodge immunity window"));
        return 0.0f;
    }
    const float fMultiplier = fNow < this->fGuardUntil ? 1.0f - this->fGuardReduction : 1.0f;
    // Taking a non-lethal hit never cancels a montage or its pending command.
    return Super::TakeDamage(fDamage * fMultiplier, DamageEvent, EventInstigator, DamageCauser);
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
    if (HasAuthority() && AbilitySystem)
    {
        AbilitySystem->GiveAbility(FGameplayAbilitySpec(UHBGA_Basic::StaticClass(), 1));
        AbilitySystem->GiveAbility(FGameplayAbilitySpec(UHBGA_Pulse::StaticClass(), 1));
        AbilitySystem->GiveAbility(FGameplayAbilitySpec(UHBGA_Dodge::StaticClass(), 1));
        AbilitySystem->GiveAbility(FGameplayAbilitySpec(UHBGA_Guard::StaticClass(), 1));
        AbilitySystem->GiveAbility(FGameplayAbilitySpec(UHBGA_Mend::StaticClass(), 1));
        AbilitySystem->GiveAbility(FGameplayAbilitySpec(UHBGA_Bolt::StaticClass(), 1));
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
