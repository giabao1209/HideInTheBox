#include "HBGameplayAbility.h"
#include "HBPlayerCharacter.h"
#include "HBAttributes.h"
#include "HBEffects.h"
#include "HBEnemyCharacter.h"
#include "HBGameMode.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "NativeGameplayTags.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitAttributeChange.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimMontage.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"
#include "UObject/ConstructorHelpers.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(PulseAbilityTag, "Ability.Pulse");
UE_DEFINE_GAMEPLAY_TAG_STATIC(PulseCastingTag, "State.Casting.Pulse");

FGameplayTag HBGetPulseAbilityTag() { return PulseAbilityTag; }
FGameplayTag HBGetPulseCastingTag() { return PulseCastingTag; }
FGameplayTag HBGetPulseImpactTag() { return FGameplayTag::RequestGameplayTag(TEXT("Event.Pulse.Impact")); }
FGameplayTag HBGetExposedTag() { return FGameplayTag::RequestGameplayTag(TEXT("State.Exposed")); }

FGameplayTag HBGetCooldownTag(EHBAbility Ability)
{
    static const FName Names[] = {
        TEXT("Cooldown.Basic"), TEXT("Cooldown.Pulse"), TEXT("Cooldown.Dodge"),
        TEXT("Cooldown.Guard"), TEXT("Cooldown.Mend"), TEXT("Cooldown.Bolt")
    };
    const int32 Index = FMath::Clamp(static_cast<int32>(Ability), 0, HBRuleCore::iAbilityCount - 1);
    return FGameplayTag::RequestGameplayTag(Names[Index]);
}

UHBGameplayAbility::UHBGameplayAbility()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    // Server-authoritative demo; input-to-server RPC is intentionally not implemented yet.
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
    ActivationBlockedTags.AddTag(PulseCastingTag);
}

bool UHBGameplayAbility::CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags,
    const FGameplayTagContainer* TargetTags, FGameplayTagContainer* RelevantTags) const
{
    const AHBPlayerCharacter* Player = ActorInfo ? Cast<AHBPlayerCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
    return Player && Player->CanExecutePendingGAS(AbilityKind) && CanExecuteAction(*Player) &&
        Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, RelevantTags);
}

bool UHBGameplayAbility::CheckCost(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* RelevantTags) const
{
    const AHBPlayerCharacter* Player = ActorInfo ? Cast<AHBPlayerCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
    return Player && Player->GetGASMana() + KINDA_SMALL_NUMBER >= Player->GetPendingGASCost(AbilityKind);
}

bool UHBGameplayAbility::CheckCooldown(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* RelevantTags) const
{
    return ActorInfo && ActorInfo->AbilitySystemComponent.IsValid() &&
        !ActorInfo->AbilitySystemComponent->HasMatchingGameplayTag(HBGetCooldownTag(AbilityKind));
}

const FGameplayTagContainer* UHBGameplayAbility::GetCooldownTags() const
{
    static const FGameplayTagContainer Tags[] = {
        FGameplayTagContainer(HBGetCooldownTag(EHBAbility::EHBA_Basic)),
        FGameplayTagContainer(HBGetCooldownTag(EHBAbility::EHBA_Pulse)),
        FGameplayTagContainer(HBGetCooldownTag(EHBAbility::EHBA_Dodge)),
        FGameplayTagContainer(HBGetCooldownTag(EHBAbility::EHBA_Guard)),
        FGameplayTagContainer(HBGetCooldownTag(EHBAbility::EHBA_Mend)),
        FGameplayTagContainer(HBGetCooldownTag(EHBAbility::EHBA_Bolt))
    };
    return &Tags[FMath::Clamp(static_cast<int32>(AbilityKind), 0, HBRuleCore::iAbilityCount - 1)];
}

void UHBGameplayAbility::ApplyCost(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
    AHBPlayerCharacter* Player = ActorInfo ? Cast<AHBPlayerCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
    if (Player && Player->GetPendingGASCost(AbilityKind) > 0.0f)
    {
        Player->ApplyGASManaDelta(-Player->GetPendingGASCost(AbilityKind));
    }
}

void UHBGameplayAbility::ApplyCooldown(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
    AHBPlayerCharacter* Player = ActorInfo ? Cast<AHBPlayerCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
    if (Player)
    {
        Player->ApplyGASCooldown(AbilityKind, Player->GetPendingGASCooldown(AbilityKind));
    }
}

void UHBGameplayAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    AHBPlayerCharacter* Player = ActorInfo ? Cast<AHBPlayerCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
    if (!Player || !Player->CanExecutePendingGAS(AbilityKind) || !CanExecuteAction(*Player) ||
        !CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    const bool bExecuted = ExecuteAction(Player);
    Player->SetLastGASActivationResult(bExecuted);
    if (bExecuted)
    {
        OnAbilityCommitted(Player);
    }
    EndAbility(Handle, ActorInfo, ActivationInfo, true, !bExecuted);
}

bool UHBGameplayAbility::ExecuteAction(AHBPlayerCharacter* Player)
{
    return Player->ExecutePendingGAS(AbilityKind);
}

bool UHBGA_Basic::CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags,
    const FGameplayTagContainer* TargetTags, FGameplayTagContainer* RelevantTags) const
{
    const AHBPlayerCharacter* Player = ActorInfo ? Cast<AHBPlayerCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
    if (Player && Player->UsesAttackMontage())
    {
        return Player->IsAttacking() &&
            UGameplayAbility::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, RelevantTags);
    }
    return Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, RelevantTags);
}

void UHBGA_Basic::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    AHBPlayerCharacter* Player = ActorInfo ? Cast<AHBPlayerCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
    if (!Player || !Player->UsesAttackMontage())
    {
        Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
        return;
    }
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        Player->OnAttackMontageFinished(true);
        return;
    }
    auto* HitTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this,
        FGameplayTag::RequestGameplayTag(TEXT("Event.Attack.Hit")), nullptr, true);
    HitTask->EventReceived.AddDynamic(this, &UHBGA_Basic::OnAttackHit);
    HitTask->ReadyForActivation();

    if (GetClass()->IsFunctionImplementedInScript(GET_FUNCTION_NAME_CHECKED(UHBGA_Basic, PlayAttackMontage)))
    {
        PlayAttackMontage(Player->GetAttackMontage());
        return;
    }
    auto* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
        this, NAME_None, Player->GetAttackMontage(), 1.0f, NAME_None, true, 1.0f, 0.0f, true);
    MontageTask->OnCompleted.AddDynamic(this, &UHBGA_Basic::OnAttackCompleted);
    MontageTask->OnInterrupted.AddDynamic(this, &UHBGA_Basic::OnAttackInterrupted);
    MontageTask->OnCancelled.AddDynamic(this, &UHBGA_Basic::OnAttackInterrupted);
    MontageTask->ReadyForActivation();
}

void UHBGA_Basic::OnAttackHit(FGameplayEventData Payload)
{
    if (AHBPlayerCharacter* Player = Cast<AHBPlayerCharacter>(GetAvatarActorFromActorInfo()))
    {
        Player->DeliverAttackHit();
    }
}

void UHBGA_Basic::OnAttackCompleted()
{
    FinishAttackMontage(false);
}

void UHBGA_Basic::OnAttackInterrupted()
{
    FinishAttackMontage(true);
}

void UHBGA_Basic::FinishAttackMontage(bool bInterrupted)
{
    if (!IsActive())
    {
        return;
    }
    AHBPlayerCharacter* Player = Cast<AHBPlayerCharacter>(GetAvatarActorFromActorInfo());
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, bInterrupted);
    if (Player)
    {
        Player->OnAttackMontageFinished(bInterrupted);
    }
}

bool UHBGA_Pulse::CanExecuteAction(const AHBPlayerCharacter& Player) const
{
    return DamageEffectClass && FVector::DistSquaredXY(Player.GetActorLocation(), Player.GetPendingGASAim()) <=
        FMath::Square(Player.GetPendingGASSpec().fRange);
}

void UHBGA_Pulse::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    AHBPlayerCharacter* Player = ActorInfo ? Cast<AHBPlayerCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
    if (!Player || !Player->CanExecutePendingGAS(AbilityKind) || !CanExecuteAction(*Player) ||
        !CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    CastAim = Player->GetPendingGASAim();
    CastPower = Player->GetPendingGASSpec().fPower;
    CastRadius = Player->GetPendingGASSpec().fRadius;
    Player->GetCharacterMovement()->StopMovementImmediately();
    Player->SetLastGASActivationResult(true);

    auto* Impact = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, HBGetPulseImpactTag(), nullptr, true, true);
    Impact->EventReceived.AddDynamic(this, &UHBGA_Pulse::OnImpact);
    Impact->ReadyForActivation();
    auto* Health = UAbilityTask_WaitAttributeChange::WaitForAttributeChangeWithComparison(this,
        UHBAttributes::GetHealthAttribute(), FGameplayTag(), FGameplayTag(),
        EWaitAttributeChangeComparison::LessThan, Player->GetHealth(), false);
    Health->OnChange.AddDynamic(this, &UHBGA_Pulse::OnHealthLost);
    Health->ReadyForActivation();

    if (CastMontage && Player->GetMesh()->GetAnimInstance())
    {
        PlayPulseMontage(CastMontage);
    }
    else if (!CastMontage && GetClass()->IsFunctionImplementedInScript(GET_FUNCTION_NAME_CHECKED(UHBGA_Pulse, OnAbilityCommitted)))
    {
        OnAbilityCommitted(Player);
    }
    else
    {
        auto* Windup = UAbilityTask_WaitDelay::WaitDelay(this, 0.35f);
        Windup->OnFinish.AddDynamic(this, &UHBGA_Pulse::ReleasePulse);
        Windup->ReadyForActivation();
    }
}

void UHBGA_Pulse::PlayPulseMontage(UAnimMontage* Montage)
{
    if (!IsActive())
    {
        return;
    }
    auto* Task = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
        this, NAME_None, Montage, 1.0f, NAME_None, false, 1.0f, 0.0f, true);
    Task->OnCompleted.AddDynamic(this, &UHBGA_Pulse::OnCastAborted);
    Task->OnInterrupted.AddDynamic(this, &UHBGA_Pulse::OnCastAborted);
    Task->OnCancelled.AddDynamic(this, &UHBGA_Pulse::OnCastAborted);
    Task->ReadyForActivation();
}

void UHBGA_Pulse::OnImpact(FGameplayEventData Payload)
{
    ReleasePulse();
}

void UHBGA_Pulse::OnHealthLost()
{
    const UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
    if (IsActive() && ASC && !ASC->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(TEXT("State.Guarding"))))
    {
        CancelAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true);
    }
}

void UHBGA_Pulse::OnCastAborted()
{
    if (IsActive())
    {
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
    }
}

void UHBGA_Pulse::ReleasePulse()
{
    if (!IsActive())
    {
        return;
    }
    AHBPlayerCharacter* Player = Cast<AHBPlayerCharacter>(GetAvatarActorFromActorInfo());
    const AHBGameMode* Mode = Player ? Player->GetWorld()->GetAuthGameMode<AHBGameMode>() : nullptr;
    if (!Mode || !Player->IsAlive())
    {
        OnCastAborted();
        return;
    }

    UAbilitySystemComponent* ASC = Player->GetAbilitySystemComponent();
    const FGameplayEffectSpecHandle Exposed = ASC->MakeOutgoingSpec(UHBGE_Exposed::StaticClass(), 1.0f, ASC->MakeEffectContext());
    if (!Exposed.IsValid())
    {
        OnCastAborted();
        return;
    }
    Exposed.Data->DynamicGrantedTags.AddTag(HBGetExposedTag());
    const TArray<TWeakObjectPtr<AHBEnemyCharacter>> Targets = Mode->GetEnemies();
    for (const TWeakObjectPtr<AHBEnemyCharacter>& Entry : Targets)
    {
        AHBEnemyCharacter* Enemy = Entry.Get();
        if (IsValid(Enemy) && Enemy->IsAlive() &&
            FVector::DistSquaredXY(CastAim, Enemy->GetActorLocation()) <= FMath::Square(CastRadius) &&
            Player->HasHitLine(Enemy->GetActorLocation(), Enemy))
        {
            const float Damage = HBApplyDamageEffect(Player, Enemy, CastPower, DamageEffectClass);
            if (Damage > 0.0f && IsValid(Enemy) && Enemy->IsAlive())
            {
                ASC->ApplyGameplayEffectSpecToTarget(*Exposed.Data, Enemy->GetAbilitySystemComponent());
            }
        }
    }
    FGameplayCueParameters Cue;
    Cue.Location = FVector(CastAim.X, CastAim.Y,
        Player->GetActorLocation().Z - Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
    Cue.RawMagnitude = CastRadius;
    ASC->ExecuteGameplayCue(FGameplayTag::RequestGameplayTag(TEXT("GameplayCue.GASDemo.Pulse")), Cue);
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

bool UHBGA_Bolt::CanExecuteAction(const AHBPlayerCharacter& Player) const
{
    const AHBEnemyCharacter* Target = Player.GetPendingGASTarget();
    return IsValid(Target) && Target->IsAlive() &&
        FVector::DistSquaredXY(Player.GetActorLocation(), Target->GetActorLocation()) <= FMath::Square(Player.GetPendingGASSpec().fRange) &&
        Player.HasHitLine(Target->GetActorLocation(), Target);
}

bool UHBGA_Bolt::ExecuteAction(AHBPlayerCharacter* Player)
{
    AHBEnemyCharacter* Target = Player->GetPendingGASTarget();
    if (!IsValid(Target) || !Target->IsAlive())
    {
        return false;
    }
    FGameplayCueParameters Cue;
    Cue.Location = Target->GetActorLocation();
    const float Damage = HBApplyDamageEffect(Player, Target, Player->GetPendingGASSpec().fPower,
        UHBGE_Damage::StaticClass(), ExposedBonusDamage);
    if (Damage > 0.0f && IsValid(Target))
    {
        Target->GetAbilitySystemComponent()->RemoveActiveEffectsWithGrantedTags(FGameplayTagContainer(HBGetExposedTag()));
    }
    Player->GetAbilitySystemComponent()->ExecuteGameplayCue(FGameplayTag::RequestGameplayTag(TEXT("GameplayCue.GASDemo.Bolt")), Cue);
    return true;
}

bool UHBGA_Guard::ExecuteAction(AHBPlayerCharacter* Player)
{
    return Player->ExecutePendingGuard(GuardEffectClass);
}

TSubclassOf<UHBGameplayAbility> HBGetNativeAbilityClass(EHBAbility Ability)
{
    switch (Ability)
    {
    case EHBAbility::EHBA_Basic: return UHBGA_Basic::StaticClass();
    case EHBAbility::EHBA_Pulse: return UHBGA_Pulse::StaticClass();
    case EHBAbility::EHBA_Dodge: return UHBGA_Dodge::StaticClass();
    case EHBAbility::EHBA_Guard: return UHBGA_Guard::StaticClass();
    case EHBAbility::EHBA_Mend: return UHBGA_Mend::StaticClass();
    case EHBAbility::EHBA_Bolt: return UHBGA_Bolt::StaticClass();
    }
    return nullptr;
}

UHBGA_Basic::UHBGA_Basic() { AbilityKind = EHBAbility::EHBA_Basic; }
UHBGA_Pulse::UHBGA_Pulse()
{
    AbilityKind = EHBAbility::EHBA_Pulse;
    DamageEffectClass = UHBGE_Damage::StaticClass();
    static ConstructorHelpers::FObjectFinder<UAnimMontage> DefaultCast(
        TEXT("/Game/GASDemo/Presentation/AM_GAS_PulseCombat.AM_GAS_PulseCombat"));
    CastMontage = DefaultCast.Object;
    SetAssetTags(FGameplayTagContainer(PulseAbilityTag));
    ActivationOwnedTags.AddTag(PulseCastingTag);
}
UHBGA_Dodge::UHBGA_Dodge()
{
    AbilityKind = EHBAbility::EHBA_Dodge;
    ActivationBlockedTags.RemoveTag(PulseCastingTag);
    CancelAbilitiesWithTag.AddTag(PulseAbilityTag);
}
UHBGA_Guard::UHBGA_Guard()
{
    AbilityKind = EHBAbility::EHBA_Guard;
    GuardEffectClass = UHBGE_Guard::StaticClass();
}
UHBGA_Mend::UHBGA_Mend() { AbilityKind = EHBAbility::EHBA_Mend; }
UHBGA_Bolt::UHBGA_Bolt() { AbilityKind = EHBAbility::EHBA_Bolt; }
