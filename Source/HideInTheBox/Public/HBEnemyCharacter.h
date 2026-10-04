#pragma once
#include "CoreMinimal.h"
#include "HBCombatant.h"
#include "HBEnemyCharacter.generated.h"
class AHBGameMode;

UCLASS()
class HIDEINTHEBOX_API AHBEnemyCharacter : public AHBCombatant
{
    GENERATED_BODY()
public:
    AHBEnemyCharacter();
    virtual void Tick(float fDeltaSeconds) override;
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void OnDeath() override;
private:
    TWeakObjectPtr<AHBGameMode> Mode;
    float fNextAttackTime = 0.0f;
};
