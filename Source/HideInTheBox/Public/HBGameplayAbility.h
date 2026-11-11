#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "HBLoadout.h"
#include "HBGameplayAbility.generated.h"

class AHBPlayerCharacter;
class UAnimMontage;

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
    virtual bool CanExecuteAction(const AHBPlayerCharacter& Player) const { return true; }
    virtual bool ExecuteAction(AHBPlayerCharacter* Player);
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
    virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags,
        const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const override;
    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
        const FGameplayEventData* TriggerEventData) override;
protected:
    UFUNCTION(BlueprintImplementableEvent, Category="GAS|Basic")
    void PlayAttackMontage(UAnimMontage* Montage);
    UFUNCTION(BlueprintCallable, Category="GAS|Basic")
    void FinishAttackMontage(bool bInterrupted);
private:
    UFUNCTION() void OnAttackHit(FGameplayEventData Payload);
    UFUNCTION() void OnAttackCompleted();
    UFUNCTION() void OnAttackInterrupted();
};

UCLASS()
class HIDEINTHEBOX_API UHBGA_Pulse : public UHBGameplayAbility
{
    GENERATED_BODY()
public:
    UHBGA_Pulse();
    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
        const FGameplayEventData* TriggerEventData) override;
protected:
    virtual bool CanExecuteAction(const AHBPlayerCharacter& Player) const override;
    UPROPERTY(EditDefaultsOnly, Category="GAS|Pulse")
    TSubclassOf<UGameplayEffect> DamageEffectClass;
    UPROPERTY(EditDefaultsOnly, Category="GAS|Pulse")
    TObjectPtr<UAnimMontage> CastMontage;
    UFUNCTION(BlueprintCallable, Category="GAS|Pulse")
    void PlayPulseMontage(UAnimMontage* Montage);
private:
    UFUNCTION() void OnImpact(FGameplayEventData Payload);
    UFUNCTION() void OnHealthLost();
    UFUNCTION() void OnCastAborted();
    UFUNCTION() void ReleasePulse();
    FVector CastAim = FVector::ZeroVector;
    float CastPower = 0.0f;
    float CastRadius = 0.0f;
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
protected:
    virtual bool ExecuteAction(AHBPlayerCharacter* Player) override;
    UPROPERTY(EditDefaultsOnly, Category="GAS|Guard")
    TSubclassOf<UGameplayEffect> GuardEffectClass;
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
protected:
    virtual bool CanExecuteAction(const AHBPlayerCharacter& Player) const override;
    virtual bool ExecuteAction(AHBPlayerCharacter* Player) override;
    UPROPERTY(EditDefaultsOnly, Category="GAS|Bolt", meta=(ClampMin="0"))
    float ExposedBonusDamage = 20.0f;
};
/** Tags registered in Config/DefaultGameplayTags.ini. Shared across reassigned slots. */
HIDEINTHEBOX_API FGameplayTag HBGetCooldownTag(EHBAbility Ability);
FGameplayTag HBGetPulseAbilityTag();
FGameplayTag HBGetPulseCastingTag();
FGameplayTag HBGetPulseImpactTag();
FGameplayTag HBGetExposedTag();
TSubclassOf<UHBGameplayAbility> HBGetNativeAbilityClass(EHBAbility Ability);
