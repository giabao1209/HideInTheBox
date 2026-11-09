#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "HBCombatant.generated.h"
class UStaticMeshComponent;
class UAbilitySystemComponent;
class UHBAttributes;
class UGameplayEffect;
struct FOnAttributeChangeData;

UCLASS()
class HIDEINTHEBOX_API AHBCombatant : public ACharacter, public IAbilitySystemInterface
{
    GENERATED_BODY()
public:
    AHBCombatant();
    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
    virtual float TakeDamage(float fDamage, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;
    float ApplyHealthDelta(float Delta);
    UFUNCTION(BlueprintPure, Category="GAS") UHBAttributes* GetGASAttributes() const { return Attributes; }
    UFUNCTION(BlueprintPure, Category="Combat") float GetHealth() const;
    UFUNCTION(BlueprintPure, Category="Combat") bool IsAlive() const { return GetHealth() > 0.0f; }
    void RestoreHealth();
    float Heal(float fAmount);
    float GetMaxHealth() const;
protected:
    virtual void BeginPlay() override;
    virtual void OnDeath();
    void OnHealthAttributeChanged(const FOnAttributeChangeData& Data);
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GAS") TObjectPtr<UAbilitySystemComponent> AbilitySystem;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GAS") TObjectPtr<UHBAttributes> Attributes;
    UPROPERTY(EditDefaultsOnly, Category="GAS") TSubclassOf<UGameplayEffect> HealthEffectClass;
    UPROPERTY(EditDefaultsOnly, Category="Combat", meta=(ClampMin="1")) float fMaxHealth = 100.0f;
    UPROPERTY(VisibleAnywhere, Category="Appearance") TObjectPtr<UStaticMeshComponent> Placeholder;
    UPROPERTY(VisibleAnywhere, Category="Appearance") TObjectPtr<UStaticMeshComponent> FacingMarker;
    UPROPERTY(EditDefaultsOnly, Category="Appearance") TObjectPtr<class UMaterialInterface> TintMaterial;
    UPROPERTY(EditDefaultsOnly, Category="Appearance") FLinearColor Tint = FLinearColor::White;
};
