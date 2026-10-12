#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "HBCommonGameplayHUDWidget.generated.h"

class UCommonTextBlock;
class UProgressBar;
class UCommonBorder;
class UBorder;
class UCanvasPanel;
class UWidget;
class UHBSkillButton;

/** CommonUI gameplay skill bar backed by the player's real loadout. */
UCLASS()
class HIDEINTHEBOX_API UHBCommonGameplayHUDWidget : public UCommonUserWidget
{
    GENERATED_BODY()

public:
    void RefreshFromGame();
    bool IsPointerOverSkillBar() const;

protected:
    virtual void NativeOnInitialized() override;

private:
    UCommonTextBlock* MakeText(const FString& Text, int32 FontSize, const FLinearColor& Color, bool bBold = false);
    UCommonBorder* MakePanel(const FLinearColor& Accent);
    void AddCanvasPanel(UCanvasPanel* Canvas, UWidget* Widget, const FAnchors& Anchors, const FMargin& Offsets, const FVector2D& Alignment = FVector2D::ZeroVector);

    UPROPERTY(Transient) TObjectPtr<UCommonTextBlock> PhaseText;
    UPROPERTY(Transient) TObjectPtr<UCommonTextBlock> WaveText;
    UPROPERTY(Transient) TObjectPtr<UCommonTextBlock> EconomyText;
    UPROPERTY(Transient) TObjectPtr<UCommonTextBlock> ObjectiveText;
    UPROPERTY(Transient) TObjectPtr<UCommonTextBlock> StatusText;
    UPROPERTY(Transient) TObjectPtr<UCommonTextBlock> ResultText;
    UPROPERTY(Transient) TObjectPtr<UCommonTextBlock> ControlsText;
    UPROPERTY(Transient) TObjectPtr<UCommonTextBlock> HealthText;
    UPROPERTY(Transient) TObjectPtr<UCommonTextBlock> ManaText;
    UPROPERTY(Transient) TObjectPtr<UCommonTextBlock> RunStateText;
    UPROPERTY(Transient) TObjectPtr<UBorder> RunStatePanel;
    UPROPERTY(Transient) TObjectPtr<UProgressBar> HealthBar;
    UPROPERTY(Transient) TObjectPtr<UProgressBar> ManaBar;
    UPROPERTY(Transient) TArray<TObjectPtr<UCommonBorder>> SlotPanels;
    UPROPERTY(Transient) TArray<TObjectPtr<UHBSkillButton>> SlotButtons;
    UPROPERTY(Transient) TArray<TObjectPtr<UCommonTextBlock>> SlotKeyLabels;
    UPROPERTY(Transient) TArray<TObjectPtr<UCommonTextBlock>> SlotNameLabels;
    UPROPERTY(Transient) TArray<TObjectPtr<UCommonTextBlock>> SlotStateLabels;
    UPROPERTY(Transient) TArray<TObjectPtr<UProgressBar>> SlotCooldownBars;
};
