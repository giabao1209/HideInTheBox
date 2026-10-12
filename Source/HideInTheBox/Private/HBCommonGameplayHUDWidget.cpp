#include "HBCommonGameplayHUDWidget.h"
#include "HBGameMode.h"
#include "HBLoadout.h"
#include "HBPlayerCharacter.h"
#include "HBPlayerController.h"
#include "HBSkillButton.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/ScaleBox.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "CommonBorder.h"
#include "CommonTextBlock.h"

namespace
{
    const FLinearColor HBColorPanel(0.015f, 0.025f, 0.045f, 0.94f);
    const FLinearColor HBColorText(0.92f, 0.96f, 1.0f, 1.0f);
    const FLinearColor HBColorMuted(0.55f, 0.67f, 0.78f, 1.0f);
    const FLinearColor HBColorIce(0.25f, 0.78f, 1.0f, 1.0f);
    const FLinearColor HBColorGold(1.0f, 0.66f, 0.22f, 1.0f);
    const FLinearColor HBColorRed(0.95f, 0.2f, 0.25f, 1.0f);
}

void UHBCommonGameplayHUDWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    if (!this->WidgetTree)
    {
        return;
    }

    UCanvasPanel* Root = this->WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("GameplayRoot"));
    this->WidgetTree->RootWidget = Root;
    this->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

    UCommonBorder* LeftPanel = this->MakePanel(HBColorIce);
    UVerticalBox* LeftContent = this->WidgetTree->ConstructWidget<UVerticalBox>();
    LeftPanel->SetContent(LeftContent);
    LeftContent->AddChildToVerticalBox(this->MakeText(TEXT("HIDE IN THE BOX"), 18, HBColorIce, true));
    this->PhaseText = this->MakeText(TEXT("TOWN  /  CYCLE 01"), 15, HBColorText, true);
    this->WaveText = this->MakeText(TEXT("WAVE 0 / 5  |  0 HOSTILES"), 13, HBColorMuted);
    LeftContent->AddChildToVerticalBox(this->PhaseText);
    LeftContent->AddChildToVerticalBox(this->WaveText);
    this->AddCanvasPanel(Root, LeftPanel, FAnchors(0.0f, 0.0f), FMargin(20.0f, 20.0f, 340.0f, 132.0f));

    UCommonBorder* RightPanel = this->MakePanel(HBColorGold);
    UVerticalBox* RightContent = this->WidgetTree->ConstructWidget<UVerticalBox>();
    RightPanel->SetContent(RightContent);
    RightContent->AddChildToVerticalBox(this->MakeText(TEXT("RUN RESOURCES"), 16, HBColorGold, true));
    this->EconomyText = this->MakeText(TEXT("0 MAGIC STONES   |   THREAT 0"), 13, HBColorText);
    this->EconomyText->SetAutoWrapText(true);
    this->ObjectiveText = this->MakeText(TEXT("GATE 100   |   ANIMA 100"), 13, HBColorMuted);
    RightContent->AddChildToVerticalBox(this->EconomyText);
    RightContent->AddChildToVerticalBox(this->ObjectiveText);
    this->AddCanvasPanel(Root, RightPanel, FAnchors(1.0f, 0.0f), FMargin(-20.0f, 20.0f, 340.0f, 132.0f), FVector2D(1.0f, 0.0f));

    UCommonBorder* SkillPanel = this->MakePanel(HBColorIce);
    SkillPanel->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    UVerticalBox* SkillContent = this->WidgetTree->ConstructWidget<UVerticalBox>();
    SkillPanel->SetContent(SkillContent);

    UHorizontalBox* ResourceRow = this->WidgetTree->ConstructWidget<UHorizontalBox>();
    UVerticalBoxSlot* ResourceRowSlot = SkillContent->AddChildToVerticalBox(ResourceRow);
    ResourceRowSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 7.0f));

    UVerticalBox* HealthColumn = this->WidgetTree->ConstructWidget<UVerticalBox>();
    UVerticalBox* ManaColumn = this->WidgetTree->ConstructWidget<UVerticalBox>();
    UHorizontalBoxSlot* HealthColumnSlot = ResourceRow->AddChildToHorizontalBox(HealthColumn);
    UHorizontalBoxSlot* ManaColumnSlot = ResourceRow->AddChildToHorizontalBox(ManaColumn);
    HealthColumnSlot->SetPadding(FMargin(0.0f, 0.0f, 12.0f, 0.0f));
    ManaColumnSlot->SetPadding(FMargin(12.0f, 0.0f, 0.0f, 0.0f));
    HealthColumnSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    ManaColumnSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    this->HealthText = this->MakeText(TEXT("HP 100 / 100"), 12, HBColorText, true);
    this->ManaText = this->MakeText(TEXT("MP 100 / 100"), 12, HBColorIce, true);
    HealthColumn->AddChildToVerticalBox(this->HealthText);
    ManaColumn->AddChildToVerticalBox(this->ManaText);
    this->HealthBar = this->WidgetTree->ConstructWidget<UProgressBar>();
    this->ManaBar = this->WidgetTree->ConstructWidget<UProgressBar>();
    this->HealthBar->SetFillColorAndOpacity(HBColorRed);
    this->ManaBar->SetFillColorAndOpacity(HBColorIce);
    HealthColumn->AddChildToVerticalBox(this->HealthBar);
    ManaColumn->AddChildToVerticalBox(this->ManaBar);

    UHorizontalBox* SlotRow = this->WidgetTree->ConstructWidget<UHorizontalBox>();
    SkillContent->AddChildToVerticalBox(SlotRow);
    const FLinearColor SlotAccents[] = { HBColorGold, FLinearColor(1.0f, 0.38f, 0.16f), HBColorIce, FLinearColor(0.72f, 0.42f, 1.0f), FLinearColor(0.24f, 0.92f, 0.52f), FLinearColor(0.38f, 0.64f, 1.0f) };
    this->SlotPanels.SetNum(6);
    this->SlotButtons.SetNum(6);
    this->SlotKeyLabels.SetNum(6);
    this->SlotNameLabels.SetNum(6);
    this->SlotStateLabels.SetNum(6);
    this->SlotCooldownBars.SetNum(6);
    for (int32 iSlot = 0; iSlot < 6; ++iSlot)
    {
        UCommonBorder* SlotPanel = this->MakePanel(SlotAccents[iSlot]);
        SlotPanel->SetPadding(FMargin(9.0f, 7.0f));
        UVerticalBox* SlotContent = this->WidgetTree->ConstructWidget<UVerticalBox>();
        SlotPanel->SetContent(SlotContent);
        this->SlotKeyLabels[iSlot] = this->MakeText(TEXT("UNBOUND"), 10, HBColorMuted, true);
        this->SlotKeyLabels[iSlot]->SetAutoWrapText(true);
        this->SlotNameLabels[iSlot] = this->MakeText(TEXT("Skill"), 14, HBColorText, true);
        this->SlotStateLabels[iSlot] = this->MakeText(TEXT("READY"), 10, HBColorIce);
        this->SlotCooldownBars[iSlot] = this->WidgetTree->ConstructWidget<UProgressBar>();
        this->SlotCooldownBars[iSlot]->SetFillColorAndOpacity(SlotAccents[iSlot]);
        SlotContent->AddChildToVerticalBox(this->SlotKeyLabels[iSlot]);
        SlotContent->AddChildToVerticalBox(this->SlotNameLabels[iSlot]);
        SlotContent->AddChildToVerticalBox(this->SlotStateLabels[iSlot]);
        SlotContent->AddChildToVerticalBox(this->SlotCooldownBars[iSlot]);
        USizeBox* SlotSize = this->WidgetTree->ConstructWidget<USizeBox>();
        SlotSize->SetWidthOverride(142.0f);
        SlotSize->SetHeightOverride(78.0f);
        UHBSkillButton* SkillButton = CreateWidget<UHBSkillButton>(this->GetOwningPlayer(), UHBSkillButton::StaticClass(), FName(*FString::Printf(TEXT("SkillButton_%d"), iSlot)));
        SkillButton->SetSkillContent(SlotPanel);
        if (AHBPlayerController* Controller = this->GetOwningPlayer<AHBPlayerController>())
        {
            SkillButton->OnClicked().AddUObject(Controller, &AHBPlayerController::UseSkillFromUI, iSlot);
        }
        SlotSize->SetContent(SkillButton);
        UHorizontalBoxSlot* CardSlot = SlotRow->AddChildToHorizontalBox(SlotSize);
        CardSlot->SetPadding(FMargin(3.0f, 0.0f));
        this->SlotPanels[iSlot] = SlotPanel;
        this->SlotButtons[iSlot] = SkillButton;
    }

    UHorizontalBox* Footer = this->WidgetTree->ConstructWidget<UHorizontalBox>();
    SkillContent->AddChildToVerticalBox(Footer);
    this->ResultText = this->MakeText(TEXT("Ready"), 11, HBColorGold);
    this->ControlsText = this->MakeText(TEXT("WASD move  |  Mouse/1-4 use skills"), 10, HBColorMuted);
    this->ControlsText->SetAutoWrapText(true);
    UHorizontalBoxSlot* ResultSlot = Footer->AddChildToHorizontalBox(this->ResultText);
    UHorizontalBoxSlot* ControlsSlot = Footer->AddChildToHorizontalBox(this->ControlsText);
    ResultSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    FSlateChildSize ControlsSize(ESlateSizeRule::Fill);
    ControlsSize.Value = 2.0f;
    ControlsSlot->SetSize(ControlsSize);
    ResultSlot->SetPadding(FMargin(2.0f, 7.0f, 8.0f, 0.0f));
    ControlsSlot->SetPadding(FMargin(8.0f, 7.0f, 2.0f, 0.0f));

    this->StatusText = this->MakeText(TEXT(""), 12, HBColorGold, true);
    SkillContent->AddChildToVerticalBox(this->StatusText);
    this->RunStateText = this->MakeText(TEXT(""), 18, HBColorGold, true);
    this->RunStatePanel = this->WidgetTree->ConstructWidget<UBorder>();
    this->RunStatePanel->SetBrushColor(FLinearColor(0.01f, 0.015f, 0.035f, 0.88f));
    this->RunStatePanel->SetPadding(FMargin(24.0f, 14.0f));
    this->RunStatePanel->SetContent(this->RunStateText);
    this->AddCanvasPanel(Root, this->RunStatePanel, FAnchors(0.5f, 0.5f), FMargin(0.0f, 0.0f, 500.0f, 76.0f), FVector2D(0.5f, 0.5f));
    USizeBox* SkillSize = this->WidgetTree->ConstructWidget<USizeBox>();
    SkillSize->SetWidthOverride(960.0f);
    SkillSize->SetHeightOverride(208.0f);
    SkillSize->SetContent(SkillPanel);
    UScaleBox* SkillScale = this->WidgetTree->ConstructWidget<UScaleBox>();
    SkillScale->SetStretch(EStretch::ScaleToFit);
    SkillScale->SetStretchDirection(EStretchDirection::DownOnly);
    SkillScale->SetContent(SkillSize);
    this->AddCanvasPanel(Root, SkillScale, FAnchors(0.0f, 1.0f, 1.0f, 1.0f), FMargin(20.0f, -226.0f, 20.0f, 208.0f));
    this->RefreshFromGame();
}

UCommonTextBlock* UHBCommonGameplayHUDWidget::MakeText(const FString& Text, int32 FontSize, const FLinearColor& Color, bool bBold)
{
    UCommonTextBlock* Label = this->WidgetTree->ConstructWidget<UCommonTextBlock>();
    Label->SetText(FText::FromString(Text));
    Label->SetColorAndOpacity(FSlateColor(Color));
    FSlateFontInfo Font = Label->GetFont();
    Font.Size = FontSize;
    Font.TypefaceFontName = bBold ? FName(TEXT("Bold")) : FName(TEXT("Regular"));
    Label->SetFont(Font);
    return Label;
}

UCommonBorder* UHBCommonGameplayHUDWidget::MakePanel(const FLinearColor& Accent)
{
    UCommonBorder* Panel = this->WidgetTree->ConstructWidget<UCommonBorder>();
    const FLinearColor Background(HBColorPanel.R + Accent.R * 0.035f, HBColorPanel.G + Accent.G * 0.035f, HBColorPanel.B + Accent.B * 0.035f, HBColorPanel.A);
    FSlateBrush PanelBrush;
    PanelBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
    PanelBrush.OutlineSettings.CornerRadii = FVector4(8.0f, 8.0f, 8.0f, 8.0f);
    PanelBrush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
    PanelBrush.OutlineSettings.Width = 1.0f;
    PanelBrush.OutlineSettings.Color = FSlateColor(Accent.CopyWithNewOpacity(0.5f));
    Panel->SetBrush(PanelBrush);
    Panel->SetBrushColor(Background);
    Panel->SetPadding(FMargin(13.0f, 10.0f));
    Panel->SetVisibility(ESlateVisibility::HitTestInvisible);
    return Panel;
}

void UHBCommonGameplayHUDWidget::AddCanvasPanel(UCanvasPanel* Canvas, UWidget* Widget, const FAnchors& Anchors, const FMargin& Offsets, const FVector2D& Alignment)
{
    if (!Canvas || !Widget)
    {
        return;
    }
    UCanvasPanelSlot* PanelSlot = Canvas->AddChildToCanvas(Widget);
    if (PanelSlot)
    {
        PanelSlot->SetAnchors(Anchors);
        PanelSlot->SetOffsets(Offsets);
        PanelSlot->SetAlignment(Alignment);
    }
}

bool UHBCommonGameplayHUDWidget::IsPointerOverSkillBar() const
{
    for (const TObjectPtr<UHBSkillButton>& Button : this->SlotButtons)
    {
        if (Button && Button->IsHovered())
        {
            return true;
        }
    }
    return false;
}

void UHBCommonGameplayHUDWidget::RefreshFromGame()
{
    AHBPlayerController* Controller = this->GetOwningPlayer<AHBPlayerController>();
    UWorld* World = this->GetWorld();
    AHBGameMode* Mode = World ? World->GetAuthGameMode<AHBGameMode>() : nullptr;
    AHBPlayerCharacter* Player = Mode ? Mode->GetPlayer() : nullptr;
    if (!Controller || !Mode || !Player || !Player->GetLoadout() || !this->PhaseText)
    {
        return;
    }

    this->PhaseText->SetText(FText::FromString(FString::Printf(TEXT("%s  /  CYCLE %02d"), *Mode->GetPhaseLabel(), Mode->GetCycle())));
    this->WaveText->SetText(FText::FromString(FString::Printf(TEXT("WAVE %d / 5  |  %.0fs  |  %d HOSTILES"), Mode->GetWave(), Mode->GetWaveSeconds(), Mode->GetEnemies().Num())));
    this->EconomyText->SetText(FText::FromString(FString::Printf(TEXT("%d MAGIC STONES  |  THREAT %d  |  NEXT %d"), Mode->GetStones(), Mode->GetThreat(), HBRules::GetWaveEnemyCount(FMath::Max(1, Mode->GetWave()), Mode->GetThreat()))));
    this->ObjectiveText->SetText(FText::FromString(FString::Printf(TEXT("GATE %.0f  |  ANIMA %.0f"), Mode->GetGateHealth(), Mode->GetCoreHealth())));
    this->HealthText->SetText(FText::FromString(FString::Printf(TEXT("HP  %.0f / %.0f"), Player->GetHealth(), Player->GetMaxHealth())));
    this->ManaText->SetText(FText::FromString(FString::Printf(TEXT("MP  %.0f / %.0f"), Player->GetMana(), Player->GetLoadout()->GetMaxMana())));
    this->HealthBar->SetPercent(FMath::Clamp(Player->GetHealth() / FMath::Max(1.0f, Player->GetMaxHealth()), 0.0f, 1.0f));
    this->ManaBar->SetPercent(Player->GetLoadout()->GetManaFraction());
    this->ResultText->SetText(FText::FromString(Controller->GetLastActionResult()));
    this->ControlsText->SetText(FText::FromString(Controller->IsUsingGamepad()
        ? TEXT("L-Stick move  |  R-Stick aim  |  A/B/X/Y/LB/RB skills  |  D-pad actions")
        : TEXT("WASD move  |  Click a skill card or LMB/RMB + 1-4  |  Shift: in-place")));

    FString Status = Mode->GetStatus();
    if (Player->IsBuilding())
    {
        Status = FString::Printf(TEXT("BUILD MODE  /  SOCKET %d  /  %s  /  30 STONES"), Mode->GetBuildSocketIndex() + 1, Mode->IsBuildSocketOccupied() ? TEXT("OCCUPIED") : TEXT("EMPTY"));
    }
    this->StatusText->SetText(FText::FromString(Status));

    const bool bGameOver = Mode->GetPhase() == EHBPhase::EHBP_GameOver;
    this->RunStateText->SetText(FText::FromString(Controller->IsPaused() ? TEXT("PAUSED  |  P / START TO RESUME") : (bGameOver ? TEXT("RUN ENDED  |  F5 / VIEW TO RESTART") : TEXT(""))));
    this->RunStatePanel->SetVisibility(Controller->IsPaused() || bGameOver ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);

    for (int32 iSlot = 0; iSlot < 6; ++iSlot)
    {
        if (!this->SlotKeyLabels.IsValidIndex(iSlot) || !this->SlotKeyLabels[iSlot])
        {
            continue;
        }
        const FHBAbilitySpec Spec = Player->GetLoadout()->GetSlotSpec(iSlot);
        const float Cooldown = Player->GetLoadout()->GetSlotCooldown(iSlot);
        const FKey Key = Controller->GetSlotKey(iSlot, Controller->IsUsingGamepad());
        this->SlotKeyLabels[iSlot]->SetText(FText::FromString(Key.IsValid() ? Key.GetDisplayName().ToString() : TEXT("UNBOUND")));
        this->SlotNameLabels[iSlot]->SetText(Spec.Name);
        const EHBAbility Ability = Player->GetLoadout()->GetSlotAbility(iSlot);
        const float ManaCost = Ability == EHBAbility::EHBA_Basic || Ability == EHBAbility::EHBA_Dodge ? 0.0f : Spec.fManaCost;
        const bool bNoMana = Player->GetMana() < ManaCost;
        const bool bBlocked = Controller->IsPaused() || bGameOver || !Player->IsAlive() || Player->IsBuilding() || Player->IsDodging() || (Player->IsAttacking() && Ability != EHBAbility::EHBA_Basic);
        const FString State = Cooldown > 0.0f ? FString::Printf(TEXT("%.1fs COOLDOWN"), Cooldown) : (bBlocked ? TEXT("UNAVAILABLE") : (bNoMana ? TEXT("NO MANA") : FString::Printf(TEXT("READY  |  %.0f MP"), ManaCost)));
        this->SlotStateLabels[iSlot]->SetText(FText::FromString(State));
        this->SlotStateLabels[iSlot]->SetColorAndOpacity(FSlateColor(Cooldown > 0.0f ? HBColorMuted : (bNoMana ? HBColorRed : HBColorIce)));
        const float CooldownFraction = Spec.fCooldown > 0.0f ? FMath::Clamp(1.0f - Cooldown / Spec.fCooldown, 0.0f, 1.0f) : 1.0f;
        this->SlotCooldownBars[iSlot]->SetPercent(CooldownFraction);
        this->SlotPanels[iSlot]->SetRenderOpacity(Cooldown > 0.0f || bNoMana || bBlocked ? 0.55f : 1.0f);
        // Gamepad uses the existing six mapped skill actions, avoiding a synthetic A click as well.
        this->SlotButtons[iSlot]->SetIsInteractionEnabled(!Controller->IsUsingGamepad() && !bBlocked);
        this->SlotButtons[iSlot]->SetToolTipText(FText::FromString(FString::Printf(TEXT("%s | %.0f MP | %.1fs cooldown\nClick: cast toward your last aim. Basic/Bolt need an enemy; Mend needs missing HP."), *Spec.Name.ToString(), ManaCost, Ability == EHBAbility::EHBA_Basic ? 0.0f : Spec.fCooldown)));
    }
}
