#include "HBGameplayAbility.h"
#include "HBPlayerCharacter.h"
#include "HBEffects.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"

FGameplayTag HBGetCooldownTag(EHBAbility Ability)
{
    static const FName Names[] = {
        TEXT("Cooldown.Basic"), TEXT("Cooldown.Pulse"), TEXT("Cooldown.Dodge"),
        TEXT("Cooldown.Guard"), TEXT("Cooldown.Mend"), TEXT("Cooldown.Bolt")
    };
    const int32 Index = FMath::Clamp(static_cast<int32>(Ability), 0, UE_ARRAY_COUNT(Names) - 1);
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
    return &Tags[FMath::Clamp(static_cast<int32>(AbilityKind), 0, UE_ARRAY_COUNT(Tags) - 1)];
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
    const bool bExecuted = Player->ExecutePendingGAS(AbilityKind);
    const bool bCommitted = bExecuted && CommitAbility(Handle, ActorInfo, ActivationInfo);
    Player->SetLastGASActivationResult(bCommitted);
    EndAbility(Handle, ActorInfo, ActivationInfo, true, !bCommitted);
}

UHBGA_Basic::UHBGA_Basic() { AbilityKind = EHBAbility::EHBA_Basic; }
UHBGA_Pulse::UHBGA_Pulse() { AbilityKind = EHBAbility::EHBA_Pulse; }
UHBGA_Dodge::UHBGA_Dodge() { AbilityKind = EHBAbility::EHBA_Dodge; }
UHBGA_Guard::UHBGA_Guard() { AbilityKind = EHBAbility::EHBA_Guard; }
UHBGA_Mend::UHBGA_Mend() { AbilityKind = EHBAbility::EHBA_Mend; }
UHBGA_Bolt::UHBGA_Bolt() { AbilityKind = EHBAbility::EHBA_Bolt; }
