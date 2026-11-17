#pragma once

#include "Abilities/GameplayAbility.h"
#include "GameplayCueNotify_Static.h"
#include "GameplayEffect.h"
#include "NativeGameplayTags.h"
#include "HBArenaAbilities.generated.h"

class UAnimMontage;
class UMaterialInterface;
class USoundBase;

namespace HBArenaTags
{
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(EnemySlam);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(EnemyMelee);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(EnemyMeleeHit);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(EnemyAttacking);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(EnemyCasting);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Staggered);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(TurretFire);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(TurretUpgraded);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Build);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Repair);
}

UCLASS()
class HIDEINTHEBOX_API UHBGE_TurretUpgrade : public UGameplayEffect
{
    GENERATED_BODY()
public:
    UHBGE_TurretUpgrade();
};

UCLASS(Abstract)
class HIDEINTHEBOX_API UHBGA_ArenaAttack : public UGameplayAbility
{
    GENERATED_BODY()
public:
    UHBGA_ArenaAttack();
    virtual const FGameplayTagContainer* GetCooldownTags() const override;
    virtual void ApplyCooldown(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
        FGameplayAbilityActivationInfo ActivationInfo) const override;
protected:
    void TriggerOnEvent(FGameplayTag EventTag);
    FGameplayTagContainer ArenaCooldownTags;
    UPROPERTY(EditDefaultsOnly, Category="Combat", meta=(ClampMin="0.05")) float CooldownSeconds = 1.0f;
};

UCLASS()
class HIDEINTHEBOX_API UHBGA_EnemyMelee : public UHBGA_ArenaAttack
{
    GENERATED_BODY()
public:
    UHBGA_EnemyMelee();
    virtual void ActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
        FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* Event) override;
private:
    UFUNCTION() void Hit(FGameplayEventData Event);
    UFUNCTION() void Complete();
    UFUNCTION() void Interrupt();
    TWeakObjectPtr<AActor> TargetActor;
    float AttackRange = 0.0f;
};

UCLASS()
class HIDEINTHEBOX_API UHBGA_EnemySlam : public UHBGA_ArenaAttack
{
    GENERATED_BODY()
public:
    UHBGA_EnemySlam();
    virtual void ActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
        FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* Event) override;
    virtual void EndAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
        FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
    float GetCastRange() const { return CastRange; }
protected:
    UPROPERTY(EditDefaultsOnly, Category="Combat") TObjectPtr<UAnimMontage> SlamMontage;
    UPROPERTY(EditDefaultsOnly, Category="Combat", meta=(ClampMin="0.1")) float WindupSeconds = 1.2f;
    UPROPERTY(EditDefaultsOnly, Category="Combat", meta=(ClampMin="1")) float CastRange = 420.0f;
    UPROPERTY(EditDefaultsOnly, Category="Combat", meta=(ClampMin="1")) float ImpactRadius = 155.0f;
    UPROPERTY(EditDefaultsOnly, Category="Combat", meta=(ClampMin="1")) float Damage = 36.0f;
private:
    UFUNCTION() void Impact();
    UFUNCTION() void Interrupt();
    FVector ImpactPoint = FVector::ZeroVector;
    TWeakObjectPtr<AActor> TargetStructure;
    bool bWarningActive = false;
};

UCLASS()
class HIDEINTHEBOX_API UHBGA_TurretFire : public UHBGA_ArenaAttack
{
    GENERATED_BODY()
public:
    UHBGA_TurretFire();
    virtual void ActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
        FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* Event) override;
protected:
    UPROPERTY(EditDefaultsOnly, Category="Combat", meta=(ClampMin="1")) float ShotDamage = 22.0f;
    UPROPERTY(EditDefaultsOnly, Category="Combat", meta=(ClampMin="0")) float UpgradeBonus = 14.0f;
};

UCLASS()
class HIDEINTHEBOX_API UHBGA_Construction : public UGameplayAbility
{
    GENERATED_BODY()
public:
    UHBGA_Construction();
    virtual void ActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
        FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* Event) override;
};

UCLASS(Blueprintable)
class HIDEINTHEBOX_API UHBGameplayCue_Arena : public UGameplayCueNotify_Static
{
    GENERATED_BODY()
public:
    UHBGameplayCue_Arena();
protected:
    virtual bool OnActive_Implementation(AActor* Target, const FGameplayCueParameters& Parameters) const override;
    virtual bool WhileActive_Implementation(AActor* Target, const FGameplayCueParameters& Parameters) const override;
    virtual bool OnRemove_Implementation(AActor* Target, const FGameplayCueParameters& Parameters) const override;
    virtual bool OnExecute_Implementation(AActor* Target, const FGameplayCueParameters& Parameters) const override;
    UPROPERTY(EditDefaultsOnly, Category="Presentation") TObjectPtr<UMaterialInterface> CueMaterial;
    UPROPERTY(EditDefaultsOnly, Category="Presentation") TObjectPtr<USoundBase> CueSound;
    UPROPERTY(EditDefaultsOnly, Category="Presentation") TObjectPtr<UMaterialInterface> SparkSprite;
    UPROPERTY(EditDefaultsOnly, Category="Presentation") TObjectPtr<UMaterialInterface> DustSprite;
    UPROPERTY(EditDefaultsOnly, Category="Presentation") FLinearColor CueColor = FLinearColor(1.0f, 0.12f, 0.035f);
    UPROPERTY(EditDefaultsOnly, Category="Presentation") bool bTracer = false;
    UPROPERTY(EditDefaultsOnly, Category="Presentation") float Volume = 0.8f;
    UPROPERTY(EditDefaultsOnly, Category="Presentation") float Pitch = 1.0f;
};

void HBInterruptEnemySlam(class AHBEnemyCharacter* Enemy);
