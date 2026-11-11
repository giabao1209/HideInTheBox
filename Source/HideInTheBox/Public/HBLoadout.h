#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/DataAsset.h"
#include "ActiveGameplayEffectHandle.h"
#include "HBRuleCore.h"
#include "HBLoadout.generated.h"
class AHBEnemyCharacter;
class UHBGameplayAbility;
class UTexture2D;

UENUM(BlueprintType)
enum class EHBAbility : uint8
{
    EHBA_Basic, EHBA_Pulse, EHBA_Dodge, EHBA_Guard, EHBA_Mend, EHBA_Bolt
};

UENUM(BlueprintType)
enum class EHBActionResult : uint8
{
    EHBAR_Accepted, EHBAR_InvalidSlot, EHBAR_Blocked, EHBAR_Cooldown, EHBAR_NoMana, EHBAR_InvalidTarget
};

USTRUCT(BlueprintType)
struct FHBAbilitySpec
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ability") FText Name;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ability") TObjectPtr<UTexture2D> Icon;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ability") TSubclassOf<UHBGameplayAbility> AbilityClass;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ability", meta=(ClampMin="0")) float fManaCost = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ability", meta=(ClampMin="0")) float fCooldown = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ability", meta=(ClampMin="0")) float fPower = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ability", meta=(ClampMin="1")) float fRange = 165.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ability", meta=(ClampMin="1")) float fRadius = 220.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ability", meta=(ClampMin="0.05")) float fDuration = 0.25f;

    float GetManaCost(EHBAbility Ability) const
    {
        return Ability == EHBAbility::EHBA_Basic || Ability == EHBAbility::EHBA_Dodge ? 0.0f : fManaCost;
    }
};

/** Six definitions indexed by EHBAbility; six freely assignable slots, no class restriction. */
UCLASS(BlueprintType)
class HIDEINTHEBOX_API UHBLoadoutData : public UDataAsset
{
    GENERATED_BODY()
public:
    UHBLoadoutData();
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Loadout") TArray<FHBAbilitySpec> Abilities;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Loadout") TArray<EHBAbility> InitialSlots;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Resources", meta=(ClampMin="1")) float fMaxMana = 100.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Resources", meta=(ClampMin="0")) float fManaRegeneration = 8.0f;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FHBActionResolved, int32, SlotIndex, EHBActionResult, Result);

UCLASS(ClassGroup=(Gameplay), meta=(BlueprintSpawnableComponent))
class HIDEINTHEBOX_API UHBLoadoutComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UHBLoadoutComponent();
    EHBActionResult TryActivateSlot(int32 iSlot, AHBEnemyCharacter* Target, const FVector& AimPoint, bool bInPlace);
    UFUNCTION(BlueprintCallable, Category="Loadout") bool AssignAbilityToSlot(int32 iSlot, EHBAbility Ability);
    UFUNCTION(BlueprintPure, Category="Loadout") EHBAbility GetSlotAbility(int32 iSlot) const;
    UFUNCTION(BlueprintPure, Category="Loadout") FHBAbilitySpec GetSlotSpec(int32 iSlot) const;
    UFUNCTION(BlueprintPure, Category="Loadout") float GetSlotCooldown(int32 iSlot) const;
    UFUNCTION(BlueprintPure, Category="Resources") float GetMana() const;
    float GetManaFraction() const;
    void AppendDebugChecks(TArray<FString>& Checks) const;
    void RefreshManaRegeneration();
    void ResetResources();
    const FHBAbilitySpec* FindSpec(EHBAbility Ability) const;
    TSubclassOf<UHBGameplayAbility> GetAbilityClass(EHBAbility Ability) const;
    UPROPERTY(BlueprintAssignable, Category="Loadout") FHBActionResolved OnActionResolved;
protected:
    virtual void BeginPlay() override;
    UPROPERTY(EditDefaultsOnly, Category="Loadout") TObjectPtr<UHBLoadoutData> Definition;
private:
    EHBActionResult Resolve(int32 iSlot, EHBActionResult Result);
    UPROPERTY(Transient) TArray<EHBAbility> Slots;
    bool bUsedDefaultDefinition = false;
    FActiveGameplayEffectHandle ManaRegenerationHandle;
};
