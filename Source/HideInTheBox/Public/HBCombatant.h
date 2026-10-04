#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "HBCombatant.generated.h"
class UStaticMeshComponent;

UCLASS()
class HIDEINTHEBOX_API AHBCombatant : public ACharacter
{
    GENERATED_BODY()
public:
    AHBCombatant();
    virtual float TakeDamage(float fDamage, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;
    UFUNCTION(BlueprintPure, Category="Combat") float GetHealth() const { return this->fHealth; }
    UFUNCTION(BlueprintPure, Category="Combat") bool IsAlive() const { return this->fHealth > 0.0f; }
    void RestoreHealth();
protected:
    virtual void BeginPlay() override;
    virtual void OnDeath();
    UPROPERTY(EditDefaultsOnly, Category="Combat", meta=(ClampMin="1")) float fMaxHealth = 100.0f;
    UPROPERTY(VisibleAnywhere, Category="Appearance") TObjectPtr<UStaticMeshComponent> Placeholder;
    UPROPERTY(EditDefaultsOnly, Category="Appearance") FLinearColor Tint = FLinearColor::White;
private:
    UPROPERTY(VisibleInstanceOnly, Category="Combat") float fHealth = 100.0f;
};
