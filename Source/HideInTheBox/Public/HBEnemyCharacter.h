#pragma once
#include "CoreMinimal.h"
#include "HBCombatant.h"
#include "HBEnemyCharacter.generated.h"
class AHBGameMode;
class UAnimMontage;
class UAnimSequenceBase;
class UHBGASTargetWidget;
class UHBGA_EnemySlam;
class AHBIndicator;

UCLASS()
class HIDEINTHEBOX_API AHBEnemyCharacter : public AHBCombatant
{
    GENERATED_BODY()
public:
    AHBEnemyCharacter();
    virtual void Tick(float fDeltaSeconds) override;
    void DeliverAnimationHit(UAnimSequenceBase* Animation);
    UAnimMontage* GetMeleeMontage() const { return AttackMontage; }
    void EnableSlam();
    void ShowSlamWarning(FVector Position, float Radius, FLinearColor Color, class UMaterialInterface* Material);
    void HideSlamWarning();
protected:
    UPROPERTY(EditDefaultsOnly, Category="GAS Demo") TSubclassOf<UHBGASTargetWidget> PracticeHealthWidgetClass;
    UPROPERTY(EditDefaultsOnly, Category="Combat") TObjectPtr<UAnimMontage> AttackMontage;
    UPROPERTY(EditDefaultsOnly, Category="Combat") TSubclassOf<UHBGA_EnemySlam> SlamAbilityClass;
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void OnDeath() override;
private:
    TWeakObjectPtr<AHBGameMode> Mode;
    UPROPERTY(Transient) TObjectPtr<AHBIndicator> SlamWarning;
    bool bSlamEnabled = false;
};
