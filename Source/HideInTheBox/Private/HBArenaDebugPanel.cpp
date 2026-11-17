#include "HBArenaDebugPanel.h"

#include "AbilitySystemComponent.h"
#include "CommonButtonBase.h"
#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "HBEnemyCharacter.h"
#include "HBGameMode.h"
#include "HBHUD.h"
#include "HBDebugSubsystem.h"
#include "HBPlayerCharacter.h"
#include "HBPlayerController.h"
#include "HBStructure.h"
#include "Misc/DefaultValueHelper.h"

#if !UE_BUILD_SHIPPING
namespace
{
    const TArray<FName> TestButtons = { TEXT("ApplyValues"), TEXT("GrantStones"), TEXT("RefillVitals"), TEXT("ClosePanel") };

    FString ReadTestValue(const UUserWidget& Widget, FName Name)
    {
        const UEditableTextBox* Input = Cast<UEditableTextBox>(Widget.GetWidgetFromName(Name));
        return Input ? Input->GetText().ToString().TrimStartAndEnd() : FString();
    }

    AHBGameMode* GetTestMode(UHBArenaDebugPanel& Panel)
    {
        AHBPlayerController* Controller = Panel.GetOwningPlayer<AHBPlayerController>();
        const AHBHUD* HUD = Controller ? Cast<AHBHUD>(Controller->GetHUD()) : nullptr;
        UWorld* World = Panel.GetWorld();
        AHBGameMode* Mode = World ? World->GetAuthGameMode<AHBGameMode>() : nullptr;
        return HUD && HUD->IsArenaTestPanelOpen() && Mode && Mode->GetPhase() != EHBPhase::EHBP_GameOver ? Mode : nullptr;
    }

    FString DescribeAbilitySystem(const UAbilitySystemComponent& ASC)
    {
        FGameplayTagContainer Tags;
        ASC.GetOwnedGameplayTags(Tags);
        return FString::Printf(TEXT("ASC  |  %d abilities  /  %d effects\n%s"),
            ASC.GetActivatableAbilities().Num(), ASC.GetActiveEffects(FGameplayEffectQuery()).Num(),
            Tags.IsEmpty() ? TEXT("No active tags") : *Tags.ToStringSimple());
    }
}
#endif

void UHBArenaDebugPanel::NativeOnInitialized()
{
    Super::NativeOnInitialized();
#if !UE_BUILD_SHIPPING
    using FTestHandler = void (UHBArenaDebugPanel::*)();
    const FTestHandler Handlers[] = { &UHBArenaDebugPanel::ApplyTestValues, &UHBArenaDebugPanel::GrantTestStones,
        &UHBArenaDebugPanel::RefillTestVitals, &UHBArenaDebugPanel::CloseTestPanel };
    for (int32 Index = 0; Index < TestButtons.Num(); ++Index)
    {
        if (UCommonButtonBase* Button = Cast<UCommonButtonBase>(GetWidgetFromName(TestButtons[Index])))
        {
            Button->OnClicked().AddUObject(this, Handlers[Index]);
        }
    }
#endif
}

void UHBArenaDebugPanel::NativeConstruct()
{
    Super::NativeConstruct();
#if !UE_BUILD_SHIPPING
    SetTestFeedback(TEXT("Enter values, then apply."));
#endif
    Refresh();
}

bool UHBArenaDebugPanel::HasTestControls() const
{
    return GetWidgetFromName(TEXT("ApplyValues")) != nullptr;
}

void UHBArenaDebugPanel::SetTestFeedback(const FString& Message)
{
    if (UTextBlock* Label = Cast<UTextBlock>(GetWidgetFromName(TEXT("TestFeedback")))) { Label->SetText(FText::FromString(Message)); }
}

void UHBArenaDebugPanel::ApplyTestValues()
{
#if !UE_BUILD_SHIPPING
    AHBGameMode* Mode = GetTestMode(*this);
    AHBPlayerCharacter* Player = Mode ? Mode->GetPlayer() : nullptr;
    if (!Player || !Player->IsAlive()) { return; }

    const FString StonesText = ReadTestValue(*this, TEXT("StonesInput"));
    const FString HealthText = ReadTestValue(*this, TEXT("HealthInput"));
    const FString ManaText = ReadTestValue(*this, TEXT("ManaInput"));
    TCHAR* StonesEnd = nullptr;
    const int64 Stones = StonesText.IsEmpty() ? 0 : FCString::Strtoi64(*StonesText, &StonesEnd, 10);
    float Health = 0.0f;
    float Mana = 0.0f;
    if ((StonesEnd && (*StonesEnd != TEXT('\0') || Stones < 0 || Stones > AHBGameMode::MaxDebugStones)) ||
        (!HealthText.IsEmpty() && (!FDefaultValueHelper::ParseFloat(HealthText, Health) || !FMath::IsFinite(Health))) ||
        (!ManaText.IsEmpty() && (!FDefaultValueHelper::ParseFloat(ManaText, Mana) || !FMath::IsFinite(Mana))))
    {
        SetTestFeedback(FString::Printf(TEXT("Enter Stones from 0 to %d and numeric HP/MP."), AHBGameMode::MaxDebugStones));
        return;
    }

    if (!StonesText.IsEmpty()) { Mode->SetDebugStones(static_cast<int32>(Stones)); }
    if (!HealthText.IsEmpty()) { Player->ApplyHealthDelta(FMath::Clamp(Health, 1.0f, Player->GetMaxHealth()) - Player->GetHealth()); }
    if (!ManaText.IsEmpty()) { Player->ApplyGASManaDelta(FMath::Clamp(Mana, 0.0f, Player->GetGASMaxMana()) - Player->GetGASMana()); }
    for (const TCHAR* Name : { TEXT("StonesInput"), TEXT("HealthInput"), TEXT("ManaInput") })
    {
        if (UEditableTextBox* Input = Cast<UEditableTextBox>(GetWidgetFromName(Name))) { Input->SetText(FText::GetEmpty()); }
    }
    SetTestFeedback(TEXT("Test values applied."));
    Refresh();
#endif
}

void UHBArenaDebugPanel::GrantTestStones()
{
#if !UE_BUILD_SHIPPING
    if (AHBGameMode* Mode = GetTestMode(*this))
    {
        if (Mode->GetStones() < AHBGameMode::MaxDebugStones) { Mode->SetDebugStones(Mode->GetStones() + 100); }
        SetTestFeedback(FString::Printf(TEXT("%d Stones available."), Mode->GetStones()));
        Refresh();
    }
#endif
}

void UHBArenaDebugPanel::RefillTestVitals()
{
#if !UE_BUILD_SHIPPING
    AHBGameMode* Mode = GetTestMode(*this);
    AHBPlayerCharacter* Player = Mode ? Mode->GetPlayer() : nullptr;
    if (!Player || !Player->IsAlive()) { return; }
    Player->Heal(Player->GetMaxHealth() - Player->GetHealth());
    Mode->RefillGASDemoMana();
    SetTestFeedback(TEXT("HP and MP refilled."));
    Refresh();
#endif
}

void UHBArenaDebugPanel::CloseTestPanel()
{
#if !UE_BUILD_SHIPPING
    if (AHBPlayerController* Controller = GetOwningPlayer<AHBPlayerController>())
    {
        if (AHBHUD* HUD = Cast<AHBHUD>(Controller->GetHUD())) { HUD->CloseArenaTestPanel(); }
    }
#endif
}

void UHBArenaDebugPanel::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
    Super::NativeTick(Geometry, DeltaTime);
    RefreshElapsed += DeltaTime;
    if (RefreshElapsed >= 0.15f)
    {
        RefreshElapsed = 0.0f;
        Refresh();
    }
}

void UHBArenaDebugPanel::Refresh()
{
#if !UE_BUILD_SHIPPING
    const AHBPlayerController* Controller = GetOwningPlayer<AHBPlayerController>();
    const AHBPlayerCharacter* Player = Controller ? Cast<AHBPlayerCharacter>(Controller->GetPawn()) : nullptr;
    UWorld* World = GetWorld();
    const AHBGameMode* Mode = World ? World->GetAuthGameMode<AHBGameMode>() : nullptr;
    if (!Player || !Mode || !DebugWorld || !DebugPlayer || !DebugTarget || !DebugBuild || !DebugEvents)
    {
        return;
    }

    const UHBDebugSubsystem* Debug = World->GetSubsystem<UHBDebugSubsystem>();
    const HBPerformance::FFrameStats& Stats = Debug->GetFrameStats();
    DebugWorld->SetText(FText::FromString(FString::Printf(TEXT("%s  /  CYCLE %d  /  WAVE %d   ·   %.0f FPS"),
        *Mode->GetPhaseLabel(), Mode->GetCycle(), Mode->GetWave(), Stats.CurrentFPS())));
    DebugPlayer->SetText(FText::FromString(FString::Printf(TEXT("PLAYER   %.0f / %.0f HP   ·   %.0f / %.0f MP\n%s"),
        Player->GetHealth(), Player->GetMaxHealth(), Player->GetGASMana(), Player->GetGASMaxMana(),
        *DescribeAbilitySystem(*Player->GetAbilitySystemComponent()))));

    if (UTextBlock* Wallet = Cast<UTextBlock>(GetWidgetFromName(TEXT("TestWallet"))))
    {
        Wallet->SetText(FText::FromString(FString::Printf(TEXT("%d available"), Mode->GetStones())));
    }

    FHitResult CursorHit;
    const bool bGamepad = Controller->IsUsingGamepad();
    const AHBEnemyCharacter* Target = bGamepad
        ? Mode->FindEnemy(Player->GetActorLocation(), Player->GetAimDirection(), 1000.0f)
        : (Controller->GetHitResultUnderCursor(ECC_Visibility, false, CursorHit) ? Cast<AHBEnemyCharacter>(CursorHit.GetActor()) : nullptr);
    DebugTarget->SetText(FText::FromString(Target
        ? FString::Printf(TEXT("AIM TARGET   %.0f / %.0f HP\n%s"), Target->GetHealth(), Target->GetMaxHealth(),
            *DescribeAbilitySystem(*Target->GetAbilitySystemComponent()))
        : (bGamepad ? TEXT("AIM TARGET   ·   No enemy in aim cone") : TEXT("AIM TARGET   ·   No enemy under cursor"))));

    const AHBStructure* Structure = Mode->GetSelectedStructure();
    const TCHAR* Type = Mode->GetBuildType() == EHBStructure::EHBS_Fence ? TEXT("FENCE") : TEXT("TURRET");
    FString Build = FString::Printf(TEXT("BUILD   CELL %d / %d   ·   %s   ·   %d STONES"),
        Mode->GetBuildSocketIndex() + 1, Mode->GetBuildCells().Num(), Type, Mode->GetStones());
    if (Structure)
    {
        Build += FString::Printf(TEXT("\n%s   %.0f / %.0f HP\n%s"), Structure->IsFence() ? TEXT("FENCE") :
            (Structure->IsUpgraded() ? TEXT("TURRET MK II") : TEXT("TURRET")),
            Structure->GetHealth(), Structure->GetMaxHealth(), *DescribeAbilitySystem(*Structure->GetAbilitySystemComponent()));
    }
    else
    {
        Build += TEXT("\nEmpty cell");
    }
    DebugBuild->SetText(FText::FromString(Build));

    TArray<FString> Events;
    Debug->GetRecent(Events, 2);
    DebugEvents->SetText(FText::FromString(Events.IsEmpty() ? TEXT("No matching events") : FString::Join(Events, TEXT("\n"))));
#endif
}
