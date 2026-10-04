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
    void Configure(EHBStructure NewKind);
    virtual float TakeDamage(float fDamage, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;
    bool IsAlive() const { return this->fHealth > 0.0f; }
    float GetHealth() const { return this->fHealth; }
    bool NeedsRepair() const { return this->fHealth < this->fMaxHealth; }
    void Repair();
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
private:
    void Fire();
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Mesh;
    EHBStructure Kind = EHBStructure::EHBS_Gate;
    float fMaxHealth = 350.0f;
    float fHealth = 350.0f;
    TWeakObjectPtr<AHBGameMode> Mode;
    TWeakObjectPtr<AHBEnemyCharacter> Target;
    FTimerHandle FireTimer;
};
