#include "HBPracticeWidget.h"

#include "AbilitySystemComponent.h"
#include "CommonInputModeTypes.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "HBGameMode.h"
#include "HBPlayerCharacter.h"
#include "HBPlayerController.h"
#include "Widgets/CommonActivatableWidgetContainer.h"

TOptional<FUIInputConfig> UHBArenaHUD::GetDesiredInputConfig() const
{
    return FUIInputConfig(ECommonInputMode::All, EMouseCaptureMode::CaptureDuringMouseDown, EMouseLockMode::DoNotLock, false);
}

void UHBArenaHUD::SetPauseMenuVisible(bool bVisible)
{
    if (!MenuStack || !PauseMenuClass)
    {
        return;
    }
    if (bVisible)
    {
        if (!MenuStack->GetActiveWidget())
        {
            MenuStack->SetVisibility(ESlateVisibility::Visible);
            MenuStack->AddWidget<UHBPauseMenu>(PauseMenuClass);
        }
    }
    else if (UCommonActivatableWidget* ActiveMenu = MenuStack->GetActiveWidget())
    {
        MenuStack->RemoveWidget(*ActiveMenu);
        MenuStack->SetVisibility(ESlateVisibility::Collapsed);
    }
}

UWidget* UHBPauseMenu::NativeGetDesiredFocusTarget() const
{
    return ResumeButton;
}

void UHBPauseMenu::ResumeGame()
{
    if (AHBPlayerController* Controller = GetOwningPlayer<AHBPlayerController>())
    {
        Controller->SetArenaPaused(false);
    }
}

void UHBPauseMenu::RestartGame()
{
    if (AHBPlayerController* Controller = GetOwningPlayer<AHBPlayerController>())
    {
        Controller->RestartArena();
    }
}

void UHBPracticeButton::SynchronizeProperties()
{
    Super::SynchronizeProperties();
    if (ButtonLabel)
    {
        ButtonLabel->SetText(Label);
    }
}

void UHBPracticeWidget::NativeConstruct()
{
    Super::NativeConstruct();
    Refresh();
}

void UHBPracticeWidget::ResetPractice()
{
    if (AHBPlayerController* Controller = GetOwningPlayer<AHBPlayerController>())
    {
        Controller->HBGASReset();
    }
}

void UHBPracticeWidget::DamagePracticePlayer()
{
    if (AHBPlayerController* Controller = GetOwningPlayer<AHBPlayerController>())
    {
        Controller->HBGASDamage();
    }
}

void UHBPracticeWidget::RefillPracticeMana()
{
    if (AHBPlayerController* Controller = GetOwningPlayer<AHBPlayerController>())
    {
        Controller->HBGASRefill();
    }
}

void UHBPracticeWidget::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
    Super::NativeTick(Geometry, DeltaTime);
    RefreshElapsed += DeltaTime;
    if (RefreshElapsed >= 0.1f)
    {
        RefreshElapsed = 0.0f;
        Refresh();
    }
}

void UHBPracticeWidget::Refresh()
{
    const AHBPlayerController* Controller = GetOwningPlayer<AHBPlayerController>();
    const AHBPlayerCharacter* Player = Controller ? Cast<AHBPlayerCharacter>(Controller->GetPawn()) : nullptr;
    if (!Player)
    {
        return;
    }

    const AHBGameMode* Mode = GetWorld()->GetAuthGameMode<AHBGameMode>();
    if (HealthBar) { HealthBar->SetPercent(Player->GetHealth() / FMath::Max(1.0f, Player->GetMaxHealth())); }
    if (ManaBar) { ManaBar->SetPercent(Player->GetLoadout()->GetManaFraction()); }
    if (HealthValue) { HealthValue->SetText(FText::FromString(FString::Printf(TEXT("%.0f / %.0f"), Player->GetHealth(), Player->GetMaxHealth()))); }
    if (ManaValue) { ManaValue->SetText(FText::FromString(FString::Printf(TEXT("%.0f / %.0f"), Player->GetGASMana(), Player->GetGASMaxMana()))); }
    if (FeedbackValue)
    {
        const FString Feedback = Mode && ArenaPhase && Player->IsBuilding() ? Mode->GetStatus() : Controller->GetLastActionResult();
        FeedbackValue->SetText(Controller->IsPaused() ? NSLOCTEXT("GASDemo", "Paused", "Paused") : FText::FromString(Feedback));
    }
    if (TargetsValue) { TargetsValue->SetText(FText::AsNumber(Mode ? Mode->GetEnemies().Num() : 0)); }
    if (Mode && ArenaPhase)
    {
        const bool bGamepad = Controller->IsUsingGamepad();
        const FString Phase = Mode->GetPhase() == EHBPhase::EHBP_Defense
            ? FString::Printf(TEXT("DEFENSE   /   WAVE %d   /   %.0fs"), Mode->GetWave(), Mode->GetWaveSeconds())
            : FString::Printf(TEXT("%s   /   CYCLE %02d"), *Mode->GetPhaseLabel().ToUpper(), Mode->GetCycle());
        ArenaPhase->SetText(FText::FromString(Phase));
        if (ArenaStats)
        {
            ArenaStats->SetText(FText::FromString(FString::Printf(TEXT("%d STONES   /   THREAT %d"), Mode->GetStones(), Mode->GetThreat())));
        }
        if (ArenaObjectives)
        {
            ArenaObjectives->SetText(FText::FromString(FString::Printf(TEXT("GATE %.0f   /   ANIMA %.0f"), Mode->GetGateHealth(), Mode->GetCoreHealth())));
        }
        if (ArenaInstruction)
        {
            FString Instruction = Mode->GetStatus();
            if (Player->IsBuilding())
            {
                const TCHAR* CycleKey = bGamepad ? TEXT("D-pad Right") : TEXT("Tab");
                const TCHAR* CancelKey = bGamepad ? TEXT("R3") : TEXT("Space");
                const FKey Confirm = Controller->GetSlotKey(0, bGamepad);
                const FString ConfirmKey = Confirm == EKeys::LeftMouseButton ? TEXT("LMB") : Confirm.GetDisplayName(false).ToString();
                Instruction = Mode->IsBuildSocketOccupied()
                    ? FString::Printf(TEXT("Socket %d occupied. %s cycles, %s cancels."), Mode->GetBuildSocketIndex() + 1, CycleKey, CancelKey)
                    : FString::Printf(TEXT("Socket %d. %s cycles, %s builds (30 stones), %s cancels."),
                        Mode->GetBuildSocketIndex() + 1, CycleKey, *ConfirmKey, CancelKey);
            }
            ArenaInstruction->SetText(FText::FromString(Instruction));
        }
        if (UTextBlock* Controls = Cast<UTextBlock>(GetWidgetFromName(TEXT("Instructions"))))
        {
            Controls->SetText(FText::FromString(bGamepad
                ? TEXT("D-PAD UP  PHASE    /    L3  CACHE    /    DOWN  BUILD    /    LEFT  REPAIR    /    START  PAUSE    /    LT  DIAGNOSTICS")
                : TEXT("ENTER  PHASE    /    E  CACHE    /    B  BUILD    /    R  REPAIR    /    P  PAUSE    /    F2  DIAGNOSTICS")));
        }
    }

    for (int32 SlotIndex = 0; SlotIndex < HBRuleCore::iSlotCount; ++SlotIndex)
    {
        UTextBlock* SlotLabel = Cast<UTextBlock>(GetWidgetFromName(FName(*FString::Printf(TEXT("Ability%d"), SlotIndex))));
        if (!SlotLabel) { continue; }
        const FHBAbilitySpec Spec = Player->GetLoadout()->GetSlotSpec(SlotIndex);
        UImage* SlotIcon = Cast<UImage>(GetWidgetFromName(FName(*FString::Printf(TEXT("AbilityIcon%d"), SlotIndex))));
        const float Remaining = Player->GetLoadout()->GetSlotCooldown(SlotIndex);
        const float ManaCost = Spec.GetManaCost(Player->GetLoadout()->GetSlotAbility(SlotIndex));
        const bool bCanAfford = Player->GetGASMana() + KINDA_SMALL_NUMBER >= ManaCost;
        if (SlotIcon)
        {
            if (SlotIcon->GetBrush().GetResourceObject() != Spec.Icon)
            {
                SlotIcon->SetBrushFromTexture(Spec.Icon);
            }
            SlotIcon->SetRenderOpacity(Remaining > 0.0f || !bCanAfford ? 0.48f : 1.0f);
        }
        const FKey Key = Controller->GetSlotKey(SlotIndex, Controller->IsUsingGamepad());
        const FText KeyLabel = Key == EKeys::LeftMouseButton ? NSLOCTEXT("GASDemo", "LMB", "LMB") :
            Key == EKeys::RightMouseButton ? NSLOCTEXT("GASDemo", "RMB", "RMB") : Key.GetDisplayName();
        const FText State = Remaining > 0.0f ? FText::FromString(FString::Printf(TEXT("%.1f s"), Remaining)) :
            (bCanAfford ? NSLOCTEXT("GASDemo", "Ready", "Ready") : NSLOCTEXT("GASDemo", "NoMana", "Low MP"));
        SlotLabel->SetText(FText::Format(NSLOCTEXT("GASDemo", "Slot", "{0}  {1}\n{2}  /  {3} MP"), KeyLabel, Spec.Name, State, FText::AsNumber(ManaCost)));
    }

    const UAbilitySystemComponent* ASC = Player->GetAbilitySystemComponent();
    if (AbilityCount) { AbilityCount->SetText(FText::AsNumber(ASC->GetActivatableAbilities().Num())); }
    if (ActiveEffects) { ActiveEffects->SetText(FText::AsNumber(ASC->GetActiveEffects(FGameplayEffectQuery()).Num())); }
    if (ActiveTags)
    {
        FGameplayTagContainer OwnedTags;
        ASC->GetOwnedGameplayTags(OwnedTags);
        ActiveTags->SetText(OwnedTags.IsEmpty() ? NSLOCTEXT("GASDemo", "NoTags", "No active tags") : FText::FromString(OwnedTags.ToStringSimple()));
    }
}

void UHBGASTargetWidget::SetCombatant(AHBCombatant* InCombatant)
{
    Combatant = InCombatant;
    RefreshHealth();
}

void UHBGASTargetWidget::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
    Super::NativeTick(Geometry, DeltaTime);
    RefreshHealth();
}

void UHBGASTargetWidget::RefreshHealth()
{
    if (const AHBCombatant* Target = Combatant.Get())
    {
        TargetHealthBar->SetPercent(Target->GetHealth() / FMath::Max(1.0f, Target->GetMaxHealth()));
        TargetHealthValue->SetText(FText::AsNumber(FMath::RoundToInt(Target->GetHealth())));
    }
}
