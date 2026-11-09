#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "HBLoadout.h"
#include "HBGameplayAbility.generated.h"

class AHBPlayerCharacter;

/** One native ability implementation shared by six registered GAS specs. */
UCLASS(Abstract, Blueprintable)
class HIDEINTHEBOX_API UHBGameplayAbility : public UGameplayAbility
{
    GENERATED_BODY()
public:
    UHBGameplayAbility();
    EHBAbility GetAbilityKind() const { return AbilityKind; }

    virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayTagContainer* SourceTags = nullptr,
        const FGameplayTagContainer* TargetTags = nullptr,
        FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
    virtual bool CheckCost(const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
    virtual bool CheckCooldown(const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
    virtual const FGameplayTagContainer* GetCooldownTags() const override;
    virtual void ApplyCost(const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const override;
    virtual void ApplyCooldown(const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const override;
    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
        const FGameplayEventData* TriggerEventData) override;
protected:
    UFUNCTION(BlueprintImplementableEvent, Category="Ability", meta=(DisplayName="On Ability Committed"))
    void OnAbilityCommitted(AHBPlayerCharacter* Player);
    EHBAbility AbilityKind = EHBAbility::EHBA_Basic;
};

UCLASS()
class HIDEINTHEBOX_API UHBGA_Basic : public UHBGameplayAbility
{
    GENERATED_BODY()
public:
    UHBGA_Basic();
};

UCLASS()
class HIDEINTHEBOX_API UHBGA_Pulse : public UHBGameplayAbility
{
    GENERATED_BODY()
public:
    UHBGA_Pulse();
};

UCLASS()
class HIDEINTHEBOX_API UHBGA_Dodge : public UHBGameplayAbility
{
    GENERATED_BODY()
public:
    UHBGA_Dodge();
};

UCLASS()
class HIDEINTHEBOX_API UHBGA_Guard : public UHBGameplayAbility
{
    GENERATED_BODY()
public:
    UHBGA_Guard();
};

UCLASS()
class HIDEINTHEBOX_API UHBGA_Mend : public UHBGameplayAbility
{
    GENERATED_BODY()
public:
    UHBGA_Mend();
};

UCLASS()
class HIDEINTHEBOX_API UHBGA_Bolt : public UHBGameplayAbility
{
    GENERATED_BODY()
public:
    UHBGA_Bolt();
};
/** Tags registered in Config/DefaultGameplayTags.ini. Shared across reassigned slots. */
HIDEINTHEBOX_API FGameplayTag HBGetCooldownTag(EHBAbility Ability);
TSubclassOf<UHBGameplayAbility> HBGetNativeAbilityClass(EHBAbility Ability);
