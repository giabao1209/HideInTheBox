#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HBPracticeWidget.generated.h"

class AHBCombatant;
class UProgressBar;
class UTextBlock;

/** Supplies live gameplay values to the designer-owned widget tree. */
UCLASS(Abstract, Blueprintable)
class HIDEINTHEBOX_API UHBPracticeWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category="GAS Demo") void Refresh();
    UFUNCTION(BlueprintCallable, Category="GAS Demo") void ResetPractice();
    UFUNCTION(BlueprintCallable, Category="GAS Demo") void DamagePracticePlayer();
    UFUNCTION(BlueprintCallable, Category="GAS Demo") void RefillPracticeMana();

protected:
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;

    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UProgressBar> HealthBar;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UProgressBar> ManaBar;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> HealthValue;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> ManaValue;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> FeedbackValue;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> TargetsValue;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> ArenaPhase;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> ArenaStats;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> ArenaObjectives;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> ArenaInstruction;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> AbilityCount;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> ActiveEffects;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> ActiveTags;

private:
    float RefreshElapsed = 0.0f;
};

UCLASS(Abstract, Blueprintable)
class HIDEINTHEBOX_API UHBGASTargetWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    void SetCombatant(AHBCombatant* InCombatant);

protected:
    virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;

    UPROPERTY(meta=(BindWidget)) TObjectPtr<UProgressBar> TargetHealthBar;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> TargetHealthValue;

private:
    TWeakObjectPtr<AHBCombatant> Combatant;
    void RefreshHealth();
};
