#pragma once
#include "CoreMinimal.h"
#include "TimerManager.h"
#include "GameFramework/Actor.h"
#include "AbilitySystemInterface.h"
#include "HBTypes.h"
#include "HBStructure.generated.h"
class UStaticMeshComponent;
class AHBGameMode;
class AHBEnemyCharacter;
class UHBAttributes;
struct FOnAttributeChangeData;

UCLASS()
class HIDEINTHEBOX_API AHBStructure : public AActor, public IAbilitySystemInterface
{
    GENERATED_BODY()
public:
    AHBStructure();
    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
    UFUNCTION(BlueprintCallable, Category="Structure") void Configure(EHBStructure NewKind);
    void SetTintMaterial(class UMaterialInterface* NewMaterial);
    virtual float TakeDamage(float fDamage, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;
    UFUNCTION(BlueprintPure, Category="Structure") bool IsAlive() const { return GetHealth() > 0.0f; }
    UFUNCTION(BlueprintPure, Category="Structure") float GetHealth() const;
    UFUNCTION(BlueprintPure, Category="Structure") bool NeedsRepair() const;
    UFUNCTION(BlueprintCallable, Category="Structure") void Repair();
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
private:
    void Fire();
    void OnHealthChanged(const FOnAttributeChangeData& Data);
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Appearance", meta=(AllowPrivateAccess="true")) TObjectPtr<UStaticMeshComponent> Mesh;
    UPROPERTY(VisibleAnywhere, Category="GAS") TObjectPtr<UAbilitySystemComponent> AbilitySystem;
    UPROPERTY() TObjectPtr<UHBAttributes> Attributes;
    EHBStructure Kind = EHBStructure::EHBS_Gate;
    TWeakObjectPtr<AHBGameMode> Mode;
    TWeakObjectPtr<AHBEnemyCharacter> Target;
    FTimerHandle FireTimer;
};
