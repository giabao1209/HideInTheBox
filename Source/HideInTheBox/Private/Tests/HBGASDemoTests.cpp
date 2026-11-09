#include "HBGameMode.h"
#include "HBPlayerCharacter.h"
#include "HBEnemyCharacter.h"
#include "HBGameplayAbility.h"
#include "HBPlayerController.h"
#include "HBPracticeWidget.h"
#include "AbilitySystemComponent.h"
#include "Components/Button.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/WidgetComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBGASPracticeTest, "HideInTheBox.GAS.Practice",
    EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FHBGASPracticeTest::RunTest(const FString& Parameters)
{
    AHBGameMode* Mode = nullptr;
    for (const FWorldContext& Context : GEngine->GetWorldContexts())
    {
        if (Context.World() && Context.World()->IsGameWorld())
        {
            Mode = Context.World()->GetAuthGameMode<AHBGameMode>();
            if (Mode && Mode->IsGASDemo()) { break; }
        }
    }
    if (!this->TestTrue(TEXT("Run in the Widget Blueprint practice arena with -game"), Mode && Mode->IsGASDemo())) { return false; }
    Mode->ResetGASDemo();
    AHBPlayerCharacter* Player = Mode->GetPlayer();
    if (!this->TestNotNull(TEXT("Player spawned"), Player)) { return false; }
    FHitResult FloorHit;
    FCollisionQueryParams FloorQuery;
    FloorQuery.AddIgnoredActor(Player);
    this->TestTrue(TEXT("Arena floor supports mouse targeting"), Player->GetWorld()->LineTraceSingleByChannel(
        FloorHit, Player->GetActorLocation(), Player->GetActorLocation() - FVector(0.0f, 0.0f, 500.0f), ECC_GameTraceChannel1, FloorQuery));
    UHBLoadoutComponent* Loadout = Player->GetLoadout();
    UAbilitySystemComponent* ASC = Player->GetAbilitySystemComponent();
    const AHBPlayerController* Controller = Cast<AHBPlayerController>(Player->GetController());
    UUserWidget* HUD = Controller ? Controller->GetPracticeHUD() : nullptr;
    if (!this->TestNotNull(TEXT("Widget Blueprint HUD mounted"), HUD)) { return false; }
    this->TestEqual(TEXT("HUD class comes from asset"), HUD->GetClass()->GetName(), FString(TEXT("WBP_GASDemoHUD_C")));
    UHBPracticeWidget* GameplayPanel = Cast<UHBPracticeWidget>(HUD->GetWidgetFromName(TEXT("GameplayPanel")));
    UHBPracticeWidget* InspectorPanel = Cast<UHBPracticeWidget>(HUD->GetWidgetFromName(TEXT("InspectorPanel")));
    if (!this->TestNotNull(TEXT("Gameplay widget mounted"), GameplayPanel) ||
        !this->TestNotNull(TEXT("Inspector widget mounted"), InspectorPanel)) { return false; }
    this->TestEqual(TEXT("Player uses the demo Blueprint"), Player->GetClass()->GetName(), FString(TEXT("BP_GASDemoPlayer_C")));
    this->TestEqual(TEXT("Pulse is granted from the Data Asset"), Loadout->GetAbilityClass(EHBAbility::EHBA_Pulse)->GetName(), FString(TEXT("GA_GASDemo_Pulse_C")));
    this->TestEqual(TEXT("Six granted abilities"), ASC->GetActivatableAbilities().Num(), 6);
    this->TestEqual(TEXT("Three practice targets"), Mode->GetEnemies().Num(), 3);
    if (Mode->GetEnemies().Num() != 3) { return false; }
    AHBEnemyCharacter* Target = Mode->GetEnemies()[0].Get();
    if (!this->TestNotNull(TEXT("Target spawned"), Target)) { return false; }
    this->TestFalse(TEXT("Target AI disabled"), Target->IsActorTickEnabled());
    UWidgetComponent* TargetHealthComponent = Target->FindComponentByClass<UWidgetComponent>();
    this->TestTrue(TEXT("Target uses its Widget Blueprint"), TargetHealthComponent && Cast<UHBGASTargetWidget>(TargetHealthComponent->GetUserWidgetObject()));
    const FVector Aim = Target->GetActorLocation();
    const float TargetHealth = Target->GetHealth();
    this->TestEqual(TEXT("Pulse accepted"), Loadout->TryActivateSlot(1, nullptr, Aim, false), EHBActionResult::EHBAR_Accepted);
    this->TestEqual(TEXT("Pulse spends 20 mana"), Player->GetGASMana(), 80.0f);
    this->TestEqual(TEXT("Pulse GE deals 45 damage"), Target->GetHealth(), TargetHealth - 45.0f);
    this->TestTrue(TEXT("Pulse tag granted"), ASC->HasMatchingGameplayTag(HBGetCooldownTag(EHBAbility::EHBA_Pulse)));
    const TArray<FActiveGameplayEffectHandle> Cooldowns = ASC->GetActiveEffects(FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(
        FGameplayTagContainer(HBGetCooldownTag(EHBAbility::EHBA_Pulse))));
    const FActiveGameplayEffect* Cooldown = Cooldowns.IsEmpty() ? nullptr : ASC->GetActiveGameplayEffect(Cooldowns[0]);
    this->TestTrue(TEXT("Blueprint cooldown effect is active"), Cooldown && Cooldown->Spec.Def->GetClass()->GetName() == TEXT("GE_GASDemo_Cooldown_C"));
    InspectorPanel->Refresh();
    UTextBlock* ActiveTags = Cast<UTextBlock>(InspectorPanel->GetWidgetFromName(TEXT("ActiveTags")));
    this->TestTrue(TEXT("Inspector displays the live tag"), ActiveTags && ActiveTags->GetText().ToString().Contains(TEXT("Cooldown.Pulse")));
    this->TestEqual(TEXT("Immediate recast rejected"), Loadout->TryActivateSlot(1, nullptr, Aim, false), EHBActionResult::EHBAR_Cooldown);
    this->TestEqual(TEXT("Rejected cast spends no mana"), Player->GetGASMana(), 80.0f);
    Loadout->AssignAbilityToSlot(2, EHBAbility::EHBA_Pulse);
    this->TestEqual(TEXT("Reassigned slot shares cooldown"), Loadout->TryActivateSlot(2, nullptr, Aim, false), EHBActionResult::EHBAR_Cooldown);
    Loadout->AssignAbilityToSlot(2, EHBAbility::EHBA_Dodge);

    Mode->ResetGASDemo();
    this->TestEqual(TEXT("Reset restores mana"), Player->GetGASMana(), 100.0f);
    this->TestFalse(TEXT("Reset clears cooldown tag"), ASC->HasMatchingGameplayTag(HBGetCooldownTag(EHBAbility::EHBA_Pulse)));
    this->TestEqual(TEXT("Full-health Mend rejected"), Loadout->TryActivateSlot(4, nullptr, Player->GetActorLocation(), false), EHBActionResult::EHBAR_InvalidTarget);
    this->TestEqual(TEXT("Invalid Mend costs no mana"), Player->GetGASMana(), 100.0f);
    UButton* DamageButton = Cast<UButton>(GameplayPanel->GetWidgetFromName(TEXT("DamageButton")));
    if (!this->TestNotNull(TEXT("Widget damage button exists"), DamageButton)) { return false; }
    DamageButton->OnClicked.Broadcast();
    this->TestEqual(TEXT("Damage helper removes 60 HP"), Player->GetHealth(), Player->GetMaxHealth() - 60.0f);
    GameplayPanel->Refresh();
    UProgressBar* HealthBar = Cast<UProgressBar>(GameplayPanel->GetWidgetFromName(TEXT("HealthBar")));
    this->TestTrue(TEXT("Widget health bar reflects damage"), HealthBar && FMath::IsNearlyEqual(HealthBar->GetPercent(), Player->GetHealth() / Player->GetMaxHealth()));
    this->TestEqual(TEXT("Mend accepted"), Loadout->TryActivateSlot(4, nullptr, Player->GetActorLocation(), false), EHBActionResult::EHBAR_Accepted);
    this->TestEqual(TEXT("Mend heals 45 HP"), Player->GetHealth(), Player->GetMaxHealth() - 15.0f);
    this->TestEqual(TEXT("Mend spends 25 MP"), Player->GetGASMana(), 75.0f);
    Mode->RefillGASDemoMana();
    this->TestEqual(TEXT("Refill restores mana"), Player->GetGASMana(), 100.0f);
    Mode->ResetGASDemo();
    Player->ApplyGASManaDelta(-100.0f);
    this->TestEqual(TEXT("No-mana Pulse rejected"), Loadout->TryActivateSlot(1, nullptr, Aim, false), EHBActionResult::EHBAR_NoMana);
    Mode->ResetGASDemo();
    this->TestEqual(TEXT("Dodge accepted"), Loadout->TryActivateSlot(2, nullptr, Player->GetActorLocation(), false), EHBActionResult::EHBAR_Accepted);
    Mode->ResetGASDemo();
    this->TestFalse(TEXT("Reset cancels dodge"), Player->IsDodging());
    Target = Mode->GetEnemies()[0].Get();
    Player->SetActorLocation(Target->GetActorLocation() - FVector(120.0f, 0.0f, 0.0f));
    this->TestEqual(TEXT("Basic attack accepted"), Loadout->TryActivateSlot(0, Target, Target->GetActorLocation(), false), EHBActionResult::EHBAR_Accepted);
    this->TestTrue(TEXT("Swing started before reset"), Player->IsAttacking());
    Mode->ResetGASDemo();
    this->TestFalse(TEXT("Reset cancels attack"), Player->IsAttacking());
    this->TestEqual(TEXT("Reset clears commands"), Player->GetCommand(), EHBCommand::EHBC_None);
    const TArray<TWeakObjectPtr<AHBEnemyCharacter>> Targets = Mode->GetEnemies();
    for (const TWeakObjectPtr<AHBEnemyCharacter>& Entry : Targets)
    {
        if (Entry.IsValid()) { Entry->ApplyHealthDelta(-Entry->GetMaxHealth()); }
    }
    this->TestTrue(TEXT("All defeated targets unregister"), Mode->GetEnemies().IsEmpty());
    Mode->AdvancePhase();
    this->TestEqual(TEXT("Enter keeps practice phase"), Mode->GetPhase(), EHBPhase::EHBP_Dungeon);
    Mode->ResetGASDemo();
    this->TestEqual(TEXT("Reset respawns defeated targets"), Mode->GetEnemies().Num(), 3);
    AHBGameMode* NormalMode = Player->GetWorld()->SpawnActor<AHBGameMode>();
    if (this->TestNotNull(TEXT("Normal mode fixture"), NormalMode))
    {
        const float Health = Player->GetHealth();
        Player->ApplyGASManaDelta(-20.0f);
        NormalMode->ResetGASDemo();
        NormalMode->DamageGASDemoPlayer();
        NormalMode->RefillGASDemoMana();
        this->TestEqual(TEXT("Practice helpers leave normal mode health alone"), Player->GetHealth(), Health);
        this->TestEqual(TEXT("Practice helpers leave normal mode mana alone"), Player->GetGASMana(), 80.0f);
        NormalMode->Destroy();
    }
    Mode->ResetGASDemo();
    return true;
}
#endif
