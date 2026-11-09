#include "HBGameplayAbility.h"
#include "HBPlayerCharacter.h"
#include "HBEffects.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"

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
}

bool UHBGameplayAbility::CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags,
    const FGameplayTagContainer* TargetTags, FGameplayTagContainer* RelevantTags) const
{
    const AHBPlayerCharacter* Player = ActorInfo ? Cast<AHBPlayerCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
    return Player && Player->CanExecutePendingGAS(AbilityKind) &&
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
    if (!Player || !Player->CanExecutePendingGAS(AbilityKind))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // Execute first so rejected targets do not cost mana or trigger cooldown.
    // This demo is single-player/server-authoritative; production network play should
    // use proper target-data replication and reserve/commit before irreversible effects.
    const bool bExecuted = ExecuteAction(Player);
    const bool bCommitted = bExecuted && CommitAbility(Handle, ActorInfo, ActivationInfo);
    Player->SetLastGASActivationResult(bCommitted);
    if (bCommitted)
    {
        OnAbilityCommitted(Player);
    }
    EndAbility(Handle, ActorInfo, ActivationInfo, true, !bCommitted);
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

bool UHBGA_Pulse::ExecuteAction(AHBPlayerCharacter* Player)
{
    return Player->ExecutePendingPulse(DamageEffectClass);
}

void UHBGA_Pulse::PlayPulseMontage(UAnimMontage* Montage)
{
    if (AHBPlayerCharacter* Player = Cast<AHBPlayerCharacter>(GetAvatarActorFromActorInfo()))
    {
        Player->PlayAnimMontage(Montage);
    }
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
}
UHBGA_Dodge::UHBGA_Dodge() { AbilityKind = EHBAbility::EHBA_Dodge; }
UHBGA_Guard::UHBGA_Guard()
{
    AbilityKind = EHBAbility::EHBA_Guard;
    GuardEffectClass = UHBGE_Guard::StaticClass();
}
UHBGA_Mend::UHBGA_Mend() { AbilityKind = EHBAbility::EHBA_Mend; }
UHBGA_Bolt::UHBGA_Bolt() { AbilityKind = EHBAbility::EHBA_Bolt; }
