#pragma once
#include "CoreMinimal.h"
#include "HBCombatant.h"
#include "HBEnemyCharacter.generated.h"
class AHBGameMode;
class UHBGASTargetWidget;

UCLASS()
class HIDEINTHEBOX_API AHBEnemyCharacter : public AHBCombatant
{
    GENERATED_BODY()
public:
    AHBEnemyCharacter();
    virtual void Tick(float fDeltaSeconds) override;
protected:
    UPROPERTY(EditDefaultsOnly, Category="GAS Demo") TSubclassOf<UHBGASTargetWidget> PracticeHealthWidgetClass;
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void OnDeath() override;
private:
    TWeakObjectPtr<AHBGameMode> Mode;
    float fNextAttackTime = 0.0f;
};
