#pragma once
#include "CoreMinimal.h"
#include "HBCombatant.h"
#include "HBEnemyCharacter.generated.h"
class AHBGameMode;
class UAnimMontage;
class UAnimSequenceBase;
class UHBGASTargetWidget;

UCLASS()
class HIDEINTHEBOX_API AHBEnemyCharacter : public AHBCombatant
{
    GENERATED_BODY()
public:
    AHBEnemyCharacter();
    virtual void Tick(float fDeltaSeconds) override;
    void DeliverAnimationHit(UAnimSequenceBase* Animation);
protected:
    UPROPERTY(EditDefaultsOnly, Category="GAS Demo") TSubclassOf<UHBGASTargetWidget> PracticeHealthWidgetClass;
    UPROPERTY(EditDefaultsOnly, Category="Combat") TObjectPtr<UAnimMontage> AttackMontage;
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void OnDeath() override;
private:
    void ApplyMeleeDamage(AActor* Target);
    TWeakObjectPtr<AHBGameMode> Mode;
    TWeakObjectPtr<AActor> PendingAttackTarget;
    float PendingAttackRange = 0.0f;
    float fNextAttackTime = 0.0f;
};
