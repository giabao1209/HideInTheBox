#include "HBPracticeWidget.h"
#include "HBArenaAbilities.h"
#include "HBStructure.h"

#include "AbilitySystemComponent.h"
#include "CommonInputModeTypes.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "HBAttributes.h"
#include "HBGameMode.h"
#include "HBGameplayAbility.h"
#include "HBPlayerCharacter.h"
#include "HBPlayerController.h"
#include "Widgets/CommonActivatableWidgetContainer.h"

namespace
{
    void SetInventoryText(UUserWidget& Menu, FName Name, const FString& Value)
    {
        if (UTextBlock* Label = Cast<UTextBlock>(Menu.GetWidgetFromName(Name)))
        {
            Label->SetText(FText::FromString(Value));
        }
    }
}

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

bool UHBArenaHUD::SetInventoryVisible(bool bVisible)
{
    if (!MenuStack) { return false; }
    UCommonActivatableWidget* Active = MenuStack->GetActiveWidget();
    if (!bVisible)
    {
        if (Active && Active->IsA<UHBInventoryMenu>()) { MenuStack->RemoveWidget(*Active); }
        MenuStack->SetVisibility(ESlateVisibility::Collapsed);
        return true;
    }
    if (Active) { return false; }
    const TSubclassOf<UCommonActivatableWidget> MenuClass = LoadClass<UHBInventoryMenu>(nullptr,
        TEXT("/Game/DemoArena/UI/WBP_DemoArenaInventory.WBP_DemoArenaInventory_C"));
    if (!MenuClass) { return false; }
    MenuStack->SetVisibility(ESlateVisibility::Visible);
    if (MenuStack->AddWidget<UHBInventoryMenu>(MenuClass)) { return true; }
    MenuStack->SetVisibility(ESlateVisibility::Collapsed);
    return false;
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

TOptional<FUIInputConfig> UHBInventoryMenu::GetDesiredInputConfig() const
{
    return FUIInputConfig(ECommonInputMode::All, EMouseCaptureMode::NoCapture, EMouseLockMode::DoNotLock, false);
}

UWidget* UHBInventoryMenu::NativeGetDesiredFocusTarget() const
{
    return GetWidgetFromName(TEXT("Slot0"));
}

void UHBInventoryMenu::BindButton(const TCHAR* Name, void (UHBInventoryMenu::*Handler)(int32), int32 Index)
{
    if (UCommonButtonBase* Button = Cast<UCommonButtonBase>(GetWidgetFromName(Name)))
    {
        Button->OnClicked().AddUObject(this, Handler, Index);
    }
}

void UHBInventoryMenu::NativeConstruct()
{
    Super::NativeConstruct();
    for (int32 Index = 0; Index < HBRuleCore::iSlotCount; ++Index)
    {
        BindButton(*FString::Printf(TEXT("Slot%d"), Index), &UHBInventoryMenu::SelectSlot, Index);
        BindButton(*FString::Printf(TEXT("Skill%d"), Index), &UHBInventoryMenu::SelectAbility, Index);
    }
    for (int32 Index = 0; Index < UHBLoadoutComponent::ItemCount; ++Index)
    {
        BindButton(*FString::Printf(TEXT("Gear%d"), Index), &UHBInventoryMenu::Equip, Index);
    }
    if (UCommonButtonBase* Button = Cast<UCommonButtonBase>(GetWidgetFromName(TEXT("AssignButton"))))
    {
        Button->OnClicked().AddUObject(this, &UHBInventoryMenu::Assign);
    }
    if (UCommonButtonBase* Button = Cast<UCommonButtonBase>(GetWidgetFromName(TEXT("UpgradeButton"))))
    {
        Button->OnClicked().AddUObject(this, &UHBInventoryMenu::Upgrade);
    }
    if (UCommonButtonBase* Button = Cast<UCommonButtonBase>(GetWidgetFromName(TEXT("CloseButton"))))
    {
        Button->OnClicked().AddUObject(this, &UHBInventoryMenu::Close);
    }
}

void UHBInventoryMenu::NativeDestruct()
{
    for (int32 Index = 0; Index < HBRuleCore::iSlotCount; ++Index)
    {
        if (UCommonButtonBase* SlotButton = Cast<UCommonButtonBase>(GetWidgetFromName(*FString::Printf(TEXT("Slot%d"), Index))))
        {
            SlotButton->OnClicked().RemoveAll(this);
        }
        if (UCommonButtonBase* Skill = Cast<UCommonButtonBase>(GetWidgetFromName(*FString::Printf(TEXT("Skill%d"), Index))))
        {
            Skill->OnClicked().RemoveAll(this);
        }
    }
    for (int32 Index = 0; Index < UHBLoadoutComponent::ItemCount; ++Index)
    {
        if (UCommonButtonBase* Gear = Cast<UCommonButtonBase>(GetWidgetFromName(*FString::Printf(TEXT("Gear%d"), Index))))
        {
            Gear->OnClicked().RemoveAll(this);
        }
    }
    for (const TCHAR* Name : { TEXT("AssignButton"), TEXT("UpgradeButton"), TEXT("CloseButton") })
    {
        if (UCommonButtonBase* Button = Cast<UCommonButtonBase>(GetWidgetFromName(Name))) { Button->OnClicked().RemoveAll(this); }
    }
    Super::NativeDestruct();
}

void UHBInventoryMenu::NativeOnActivated()
{
    Super::NativeOnActivated();
    SelectedSlot = 0;
    Feedback.Reset();
    if (const UHBLoadoutComponent* Loadout = GetLoadout())
    {
        SelectedAbility = static_cast<int32>(Loadout->GetSlotAbility(SelectedSlot));
    }
    Refresh();
}

UHBLoadoutComponent* UHBInventoryMenu::GetLoadout() const
{
    const AHBPlayerCharacter* Player = Cast<AHBPlayerCharacter>(GetOwningPlayerPawn());
    return Player ? Player->GetLoadout() : nullptr;
}

AHBGameMode* UHBInventoryMenu::GetArena() const
{
    return GetWorld() ? GetWorld()->GetAuthGameMode<AHBGameMode>() : nullptr;
}

void UHBInventoryMenu::SelectSlot(int32 Index)
{
    SelectedSlot = Index;
    if (const UHBLoadoutComponent* Loadout = GetLoadout())
    {
        SelectedAbility = static_cast<int32>(Loadout->GetSlotAbility(Index));
    }
    Feedback.Reset();
    Refresh();
}

void UHBInventoryMenu::SelectAbility(int32 Index)
{
    SelectedAbility = Index;
    Feedback.Reset();
    Refresh();
}

void UHBInventoryMenu::Equip(int32 Index)
{
    UHBLoadoutComponent* Loadout = GetLoadout();
    if (!Loadout) { return; }
    if (Loadout->EquipItem(Index)) { Feedback = UHBLoadoutComponent::GetItemName(Index) + TEXT(" equipped."); }
    else if (!Loadout->OwnsItem(Index))
    {
        Feedback = Index == 1 ? TEXT("Unlock this axe in the west cache.") : TEXT("Unlock this shield in the east cache.");
    }
    else { Feedback = Loadout->IsItemEquipped(Index) ? TEXT("Already equipped.") : TEXT("Unable to equip this item."); }
    Refresh();
}

void UHBInventoryMenu::Assign()
{
    const AHBGameMode* Mode = GetArena();
    UHBLoadoutComponent* Loadout = GetLoadout();
    const EHBAbility Ability = static_cast<EHBAbility>(SelectedAbility);
    const AHBPlayerController* Controller = GetOwningPlayer<AHBPlayerController>();
    const bool bAssigned = Mode && Mode->CanEditLoadout() && Loadout && Loadout->AssignAbilityToSlot(SelectedSlot, Ability);
    if (!bAssigned) { Feedback = TEXT("Unable to assign skill."); }
    else if (Ability != EHBAbility::EHBA_Basic && SelectedSlot == 0 && Controller &&
        Controller->GetSlotKey(SelectedSlot, false) == EKeys::LeftMouseButton)
    {
        Feedback = TEXT("Assigned. Shift + LMB casts on empty ground.");
    }
    else { Feedback = TEXT("Skill assigned."); }
    Refresh();
}

void UHBInventoryMenu::Upgrade()
{
    UHBLoadoutComponent* Loadout = GetLoadout();
    const EHBAbility Ability = static_cast<EHBAbility>(SelectedAbility);
    if (!Loadout) { return; }
    if (Loadout->UpgradeSkill(Ability)) { Feedback = TEXT("Skill upgraded."); }
    else if (Loadout->GetSkillLevel(Ability) >= UHBLoadoutComponent::MaxSkillLevel) { Feedback = TEXT("Maximum level reached."); }
    else { Feedback = TEXT("Not enough Stones or skill unavailable."); }
    Refresh();
}

void UHBInventoryMenu::Close()
{
    if (AHBPlayerController* Controller = GetOwningPlayer<AHBPlayerController>()) { Controller->SetInventoryOpen(false); }
}

void UHBInventoryMenu::Refresh()
{
    const UHBLoadoutComponent* Loadout = GetLoadout();
    const AHBGameMode* Mode = GetArena();
    if (!Loadout || !Mode) { return; }
    const bool bCanEdit = Mode->CanEditLoadout();
    for (const TCHAR* Name : { TEXT("AssignButton"), TEXT("UpgradeButton") })
    {
        if (UWidget* Button = GetWidgetFromName(Name)) { Button->SetIsEnabled(bCanEdit); }
    }
    SetInventoryText(*this, TEXT("StonesLabel"), FString::Printf(TEXT("%d STONES"), Mode->GetStones()));
    for (int32 Index = 0; Index < HBRuleCore::iSlotCount; ++Index)
    {
        const EHBAbility Ability = static_cast<EHBAbility>(Index);
        const FHBAbilitySpec* Spec = Loadout->FindSpec(Ability);
        if (!Spec) { continue; }
        const AHBPlayerController* Controller = GetOwningPlayer<AHBPlayerController>();
        const FString Key = Controller ? Controller->GetSlotKey(Index, Controller->IsUsingGamepad()).GetDisplayName(false).ToString() : FString::FromInt(Index + 1);
        SetInventoryText(*this, *FString::Printf(TEXT("SlotLabel%d"), Index), FString::Printf(TEXT("%s%s   %s"),
            Index == SelectedSlot ? TEXT("> ") : TEXT(""), *Key, *Loadout->GetSlotSpec(Index).Name.ToString()));
        SetInventoryText(*this, *FString::Printf(TEXT("SkillLabel%d"), Index), FString::Printf(TEXT("%s%s   Lv %d"),
            Index == SelectedAbility ? TEXT("> ") : TEXT(""), *Spec->Name.ToString(), Loadout->GetSkillLevel(Ability)));
    }
    for (int32 Index = 0; Index < UHBLoadoutComponent::ItemCount; ++Index)
    {
        if (UWidget* Button = GetWidgetFromName(*FString::Printf(TEXT("Gear%d"), Index))) { Button->SetIsEnabled(bCanEdit); }
        const TCHAR* State = Loadout->IsItemEquipped(Index) ? TEXT("EQUIPPED") : (Loadout->OwnsItem(Index) ? TEXT("OWNED") : TEXT("LOCKED"));
        SetInventoryText(*this, *FString::Printf(TEXT("GearLabel%d"), Index), FString::Printf(TEXT("%s   [%s]"),
            *UHBLoadoutComponent::GetItemName(Index), State));
    }
    const int32 Level = Loadout->GetSkillLevel(static_cast<EHBAbility>(SelectedAbility));
    const FString Cost = Level >= UHBLoadoutComponent::MaxSkillLevel ? FString(TEXT("Maximum skill level"))
        : FString::Printf(TEXT("Upgrade cost: %d Stones"), UHBLoadoutComponent::SkillUpgradeCost(Level));
    const FString Message = bCanEdit ? (Feedback.IsEmpty() ? Cost : Feedback) : TEXT("Change skills and equipment in Town or Prep.");
    SetInventoryText(*this, TEXT("InventoryFeedback"), Message);
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
            FString Objectives;
            if (Mode->GetPhase() == EHBPhase::EHBP_Dungeon && Mode->UsesDungeonRoute())
            {
                const int32 Remaining = Mode->GetDungeonEnemiesRemaining();
                const int32 Claimed = static_cast<int32>(Mode->IsGreedClaimed(0)) + Mode->IsGreedClaimed(1) + Mode->IsGreedClaimed(2);
                Objectives = Remaining > 0
                    ? FString::Printf(TEXT("MAIN TRAIL %d LEFT  /  CACHES %d/3"), Remaining, Claimed)
                    : FString::Printf(TEXT("EXIT %d m  /  CACHES %d/3"),
                        FMath::CeilToInt(FVector::Dist2D(Player->GetActorLocation(), Mode->GetDungeonExitPosition()) / 100.0), Claimed);
                if (Remaining == 0 && Mode->IsAtDungeonExit(*Player))
                {
                    Objectives = FString::Printf(TEXT("%s TO EXTRACT  /  CACHES %d/3"), bGamepad ? TEXT("D-PAD UP") : TEXT("ENTER"), Claimed);
                }
            }
            else
            {
                Objectives = FString::Printf(TEXT("GATE %.0f   /   ANIMA %.0f"), Mode->GetGateHealth(), Mode->GetCoreHealth());
            }
            ArenaObjectives->SetText(FText::FromString(Objectives));
        }
        if (ArenaInstruction)
        {
            FString Instruction = Mode->GetStatus();
            if (Player->IsBuilding())
            {
                const TCHAR* Navigate = bGamepad ? TEXT("RS / D-pad Right") : TEXT("Mouse / Tab");
                const FKey Confirm = Controller->GetSlotKey(0, bGamepad);
                const FString ConfirmKey = Confirm == EKeys::LeftMouseButton ? TEXT("LMB") : Confirm.GetDisplayName(false).ToString();
                const FKey Type = Controller->GetSlotKey(1, bGamepad);
                const FString TypeKey = Type == EKeys::RightMouseButton ? TEXT("RMB") : Type.GetDisplayName(false).ToString();
                const FString RotateKey = Controller->GetSlotKey(2, bGamepad).GetDisplayName(false).ToString();
                const bool bFence = Mode->GetBuildType() == EHBStructure::EHBS_Fence;
                const int32 Cell = Mode->GetBuildSocketIndex() + 1;
                if (const AHBStructure* Structure = Mode->GetSelectedStructure())
                {
                    const FString Upgrade = Structure->IsTurret() && !Structure->IsUpgraded() && !bFence
                        ? FString::Printf(TEXT(" / %s upgrades (%d)"), *ConfirmKey, AHBGameMode::UpgradeCost) : FString();
                    const TCHAR* Label = Structure->IsFence() ? TEXT("FENCE") : (Structure->IsUpgraded() ? TEXT("TURRET MK II") : TEXT("TURRET"));
                    Instruction = FString::Printf(TEXT("Cell %d / %s %.0f/%.0f HP / %s repairs (%d)%s"),
                        Cell, Label, Structure->GetHealth(), Structure->GetMaxHealth(),
                        bGamepad ? TEXT("D-pad Left") : TEXT("R"), AHBGameMode::RepairCost, *Upgrade);
                }
                else
                {
                    const FString Rotation = bFence ? FString::Printf(TEXT(" / %s rotates"), *RotateKey) : FString();
                    Instruction = FString::Printf(TEXT("Cell %d / %s places %s (%d)%s"),
                        Cell, *ConfirmKey, bFence ? TEXT("FENCE") : TEXT("TURRET"), bFence ? AHBGameMode::FenceCost : AHBGameMode::BuildCost, *Rotation);
                }
                Instruction += FString::Printf(TEXT(" / %s switches type / %s selects"), *TypeKey, Navigate);
            }
            ArenaInstruction->SetText(FText::FromString(Instruction));
        }
        if (UTextBlock* Controls = Cast<UTextBlock>(GetWidgetFromName(TEXT("Instructions"))))
        {
            Controls->SetText(FText::FromString(bGamepad
                ? TEXT("D-PAD UP  PHASE  /  L3  CACHE  /  RT  INVENTORY  /  DOWN  BUILD  /  LEFT  REPAIR  /  START  PAUSE  /  LT  DIAGNOSTICS")
                : TEXT("ENTER PHASE / E CACHE / I INVENTORY / B BUILD / R REPAIR / P PAUSE / F2 DEBUG / F6 RESET / F7 HIT / F8 +MP")));
        }
    }

    static const FGameplayTag GuardTag = FGameplayTag::RequestGameplayTag(TEXT("State.Guarding"));
    const float GuardRemaining = Player->GetGASEffectTimeRemaining(GuardTag);
    for (int32 SlotIndex = 0; SlotIndex < HBRuleCore::iSlotCount; ++SlotIndex)
    {
        UTextBlock* SlotLabel = Cast<UTextBlock>(GetWidgetFromName(FName(*FString::Printf(TEXT("Ability%d"), SlotIndex))));
        if (!SlotLabel) { continue; }
        const FHBAbilitySpec Spec = Player->GetLoadout()->GetSlotSpec(SlotIndex);
        UImage* SlotIcon = Cast<UImage>(GetWidgetFromName(FName(*FString::Printf(TEXT("AbilityIcon%d"), SlotIndex))));
        const float Remaining = Player->GetLoadout()->GetSlotCooldown(SlotIndex);
        const EHBAbility Ability = Player->GetLoadout()->GetSlotAbility(SlotIndex);
        const float ManaCost = Spec.GetManaCost(Ability);
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
        FText State = Remaining > 0.0f ? FText::FromString(FString::Printf(TEXT("%.1f s"), Remaining)) :
            (bCanAfford ? NSLOCTEXT("GASDemo", "Ready", "Ready") : NSLOCTEXT("GASDemo", "NoMana", "Low MP"));
        if (Ability == EHBAbility::EHBA_Pulse && Player->GetAbilitySystemComponent()->HasMatchingGameplayTag(HBGetPulseCastingTag()))
        {
            State = NSLOCTEXT("GASDemo", "PulseCharging", "Charging");
        }
        else if (Ability == EHBAbility::EHBA_Guard && GuardRemaining > 0.0f)
        {
            const FText Seconds = FText::FromString(FString::Printf(TEXT("%.1f"), FMath::Max(0.1f, GuardRemaining)));
            State = FText::Format(NSLOCTEXT("GASDemo", "GuardActive", "Active {0}s"), Seconds);
        }
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
    UnbindHealthChanges();
    Combatant = InCombatant;
    if (IsValid(InCombatant))
    {
        UAbilitySystemComponent* ASC = InCombatant->GetAbilitySystemComponent();
        ASC->GetGameplayAttributeValueChangeDelegate(UHBAttributes::GetHealthAttribute())
            .AddUObject(this, &UHBGASTargetWidget::OnHealthChanged);
        ASC->GetGameplayAttributeValueChangeDelegate(UHBAttributes::GetMaxHealthAttribute())
            .AddUObject(this, &UHBGASTargetWidget::OnHealthChanged);
        ASC->RegisterGameplayTagEvent(HBGetExposedTag()).AddUObject(this, &UHBGASTargetWidget::OnExposedChanged);
        ASC->RegisterGameplayTagEvent(HBArenaTags::EnemyCasting).AddUObject(this, &UHBGASTargetWidget::OnExposedChanged);
        ASC->RegisterGameplayTagEvent(HBArenaTags::Staggered).AddUObject(this, &UHBGASTargetWidget::OnExposedChanged);
    }
    RefreshHealth();
}

void UHBGASTargetWidget::NativeConstruct()
{
    Super::NativeConstruct();
    SetCombatant(Combatant.Get());
}

void UHBGASTargetWidget::NativeDestruct()
{
    UnbindHealthChanges();
    Super::NativeDestruct();
}

void UHBGASTargetWidget::UnbindHealthChanges()
{
    if (const AHBCombatant* Target = Combatant.Get())
    {
        UAbilitySystemComponent* ASC = Target->GetAbilitySystemComponent();
        ASC->GetGameplayAttributeValueChangeDelegate(UHBAttributes::GetHealthAttribute()).RemoveAll(this);
        ASC->GetGameplayAttributeValueChangeDelegate(UHBAttributes::GetMaxHealthAttribute()).RemoveAll(this);
        ASC->RegisterGameplayTagEvent(HBGetExposedTag()).RemoveAll(this);
        ASC->RegisterGameplayTagEvent(HBArenaTags::EnemyCasting).RemoveAll(this);
        ASC->RegisterGameplayTagEvent(HBArenaTags::Staggered).RemoveAll(this);
    }
}

void UHBGASTargetWidget::OnHealthChanged(const FOnAttributeChangeData& Data)
{
    RefreshHealth();
}

void UHBGASTargetWidget::OnExposedChanged(const FGameplayTag Tag, int32 NewCount)
{
    RefreshHealth();
}

void UHBGASTargetWidget::RefreshHealth()
{
    const AHBCombatant* Target = Combatant.Get();
    if (Target && TargetHealthBar && TargetHealthValue)
    {
        TargetHealthBar->SetPercent(Target->GetHealth() / FMath::Max(1.0f, Target->GetMaxHealth()));
        const UAbilitySystemComponent* ASC = Target->GetAbilitySystemComponent();
        FString Label = FString::FromInt(FMath::RoundToInt(Target->GetHealth()));
        if (ASC->HasMatchingGameplayTag(HBArenaTags::EnemyCasting)) { Label += TEXT("  SLAM"); }
        if (ASC->HasMatchingGameplayTag(HBArenaTags::Staggered)) { Label += TEXT("  INTERRUPTED"); }
        if (ASC->HasMatchingGameplayTag(HBGetExposedTag())) { Label += TEXT("  EXPOSED"); }
        TargetHealthValue->SetText(FText::FromString(Label));
    }
}
