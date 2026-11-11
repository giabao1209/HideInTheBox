#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "GameplayEffectExecutionCalculation.h"
#include "GameplayCueNotify_Static.h"
#include "HBEffects.generated.h"

/** Instant SetByCaller "Magnitude" additive health adjustment, used by Mend. */
UCLASS()
class HIDEINTHEBOX_API UHBGE_HealthDelta : public UGameplayEffect
{
    GENERATED_BODY()
public:
    UHBGE_HealthDelta();
};

/** Instant SetByCaller "Magnitude" additive mana change for costs and refills. */
UCLASS()
class HIDEINTHEBOX_API UHBGE_ManaDelta : public UGameplayEffect
{
    GENERATED_BODY()
public:
    UHBGE_ManaDelta();
};

/** Timed effect. GAS ability adds the relevant Cooldown.* tag to each outgoing spec. */
UCLASS()
class HIDEINTHEBOX_API UHBGE_Cooldown : public UGameplayEffect
{
    GENERATED_BODY()
public:
    UHBGE_Cooldown();
};

UCLASS()
class HIDEINTHEBOX_API UHBGE_Guard : public UGameplayEffect
{
    GENERATED_BODY()
public:
    UHBGE_Guard();
};

UCLASS()
class HIDEINTHEBOX_API UHBExec_Damage : public UGameplayEffectExecutionCalculation
{
    GENERATED_BODY()
public:
    virtual void Execute_Implementation(const FGameplayEffectCustomExecutionParameters& Parameters,
        FGameplayEffectCustomExecutionOutput& Output) const override;
};

UCLASS()
class HIDEINTHEBOX_API UHBGE_Damage : public UGameplayEffect
{
    GENERATED_BODY()
public:
    UHBGE_Damage();
};

UCLASS()
class HIDEINTHEBOX_API UHBGE_DodgeImmunity : public UGameplayEffect
{
    GENERATED_BODY()
public:
    UHBGE_DodgeImmunity();
};

UCLASS()
class HIDEINTHEBOX_API UHBGE_ManaRegeneration : public UGameplayEffect
{
    GENERATED_BODY()
public:
    UHBGE_ManaRegeneration();
    static constexpr float Interval = 0.25f;
};

HIDEINTHEBOX_API float HBApplyDamageEffect(AActor* Source, AActor* Target, float Damage,
    TSubclassOf<UGameplayEffect> EffectClass = UHBGE_Damage::StaticClass());

UCLASS(Blueprintable)
class HIDEINTHEBOX_API UHBGameplayCue_Pulse : public UGameplayCueNotify_Static
{
    GENERATED_BODY()
protected:
    virtual bool OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const override;
    UPROPERTY(EditDefaultsOnly, Category="Presentation")
    FLinearColor ShockwaveColor = FLinearColor(1.0f, 0.58f, 0.18f);
};

UCLASS(Blueprintable)
class HIDEINTHEBOX_API UHBGameplayCue_Bolt : public UGameplayCueNotify_Static
{
    GENERATED_BODY()
protected:
    virtual bool OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const override;
    UPROPERTY(EditDefaultsOnly, Category="Presentation")
    TObjectPtr<class UMaterialInterface> BoltMaterial;
    UPROPERTY(EditDefaultsOnly, Category="Presentation", meta=(ClampMin="0.05"))
    float BoltDuration = 0.32f;
};
