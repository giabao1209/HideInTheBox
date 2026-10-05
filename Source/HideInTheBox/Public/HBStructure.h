#pragma once
#include "CoreMinimal.h"
#include "TimerManager.h"
#include "GameFramework/Actor.h"
#include "HBTypes.h"
#include "HBStructure.generated.h"
class UStaticMeshComponent;
class AHBGameMode;
class AHBEnemyCharacter;

UCLASS()
class HIDEINTHEBOX_API AHBStructure : public AActor
{
    GENERATED_BODY()
public:
    AHBStructure();
    UFUNCTION(BlueprintCallable, Category="Structure") void Configure(EHBStructure NewKind);
    void SetTintMaterial(class UMaterialInterface* NewMaterial);
    virtual float TakeDamage(float fDamage, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;
    UFUNCTION(BlueprintPure, Category="Structure") bool IsAlive() const { return this->fHealth > 0.0f; }
    UFUNCTION(BlueprintPure, Category="Structure") float GetHealth() const { return this->fHealth; }
    UFUNCTION(BlueprintPure, Category="Structure") bool NeedsRepair() const { return this->fHealth < this->fMaxHealth; }
    UFUNCTION(BlueprintCallable, Category="Structure") void Repair();
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
private:
    void Fire();
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Appearance", meta=(AllowPrivateAccess="true")) TObjectPtr<UStaticMeshComponent> Mesh;
    EHBStructure Kind = EHBStructure::EHBS_Gate;
    float fMaxHealth = 350.0f;
    float fHealth = 350.0f;
    TWeakObjectPtr<AHBGameMode> Mode;
    TWeakObjectPtr<AHBEnemyCharacter> Target;
    FTimerHandle FireTimer;
};
