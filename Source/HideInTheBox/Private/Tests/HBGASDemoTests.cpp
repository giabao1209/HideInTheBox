#include "HBGameMode.h"
#include "HBPlayerCharacter.h"
#include "HBEnemyCharacter.h"
#include "HBGameplayAbility.h"
#include "HBPlayerController.h"
#include "HBPracticeWidget.h"
#include "HBDebugSubsystem.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/WidgetComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
    void OnPulseDamageApplied(UAbilitySystemComponent* Source, const FGameplayEffectSpec& Spec,
        FActiveGameplayEffectHandle, UAbilitySystemComponent* ExpectedSource, bool* bApplied)
    {
        *bApplied |= Source == ExpectedSource && Spec.Def->GetClass()->GetName() == TEXT("GE_GASDemo_Damage_C");
    }
}

DEFINE_LATENT_AUTOMATION_COMMAND_FIVE_PARAMETER(FHBVerifyMontageHit, FAutomationTestBase*, Test,
    TWeakObjectPtr<AHBPlayerCharacter>, Player, TWeakObjectPtr<AHBEnemyCharacter>, Target, float, ExpectedHealth, double, Deadline);

bool FHBVerifyMontageHit::Update()
{
    const bool bGuardActive = Player.IsValid() && Player->GetAbilitySystemComponent()->HasMatchingGameplayTag(
        FGameplayTag::RequestGameplayTag(TEXT("State.Guarding")));
    if (Player.IsValid() && (Player->IsAttacking() || bGuardActive) && FPlatformTime::Seconds() < Deadline)
    {
        return false;
    }
    Test->TestTrue(TEXT("Two buffered swings applied one hit each"), Target.IsValid() &&
        FMath::IsNearlyEqual(Target->GetHealth(), ExpectedHealth));
    Test->TestTrue(TEXT("Montage task completed the swing"), Player.IsValid() &&
        !Player->IsAttacking() && Player->GetLastAttackOutcome() == TEXT("Completed"));
    Test->TestFalse(TEXT("Guard effect expires without reset"), bGuardActive);
    return true;
}

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
    const UAnimInstance* Anim = Player->GetMesh()->GetAnimInstance();
    if (!this->TestTrue(TEXT("Player mesh and locomotion animation are assigned"), Player->GetMesh()->GetSkeletalMeshAsset() && Anim))
    {
        return false;
    }
    this->TestEqual(TEXT("Pulse is granted from the Data Asset"), Loadout->GetAbilityClass(EHBAbility::EHBA_Pulse)->GetName(), FString(TEXT("GA_GASDemo_Pulse_C")));
    this->TestEqual(TEXT("Six granted abilities"), ASC->GetActivatableAbilities().Num(), 6);
    this->TestEqual(TEXT("Three practice targets"), Mode->GetEnemies().Num(), 3);
    if (Mode->GetEnemies().Num() != 3) { return false; }
    AHBEnemyCharacter* Target = Mode->GetEnemies()[0].Get();
    if (!this->TestNotNull(TEXT("Target spawned"), Target)) { return false; }
    this->TestFalse(TEXT("Target AI disabled"), Target->IsActorTickEnabled());
    this->TestTrue(TEXT("Target mesh and idle animation are assigned"),
        Target->GetMesh()->GetSkeletalMeshAsset() && Target->GetMesh()->GetAnimInstance());
    UWidgetComponent* TargetHealthComponent = Target->FindComponentByClass<UWidgetComponent>();
    this->TestTrue(TEXT("Target uses its Widget Blueprint"), TargetHealthComponent && Cast<UHBGASTargetWidget>(TargetHealthComponent->GetUserWidgetObject()));
    const FVector Aim = Target->GetActorLocation();
    const float TargetHealth = Target->GetHealth();
    bool bPulseAppliedDamageEffect = false;
    UAbilitySystemComponent* TargetASC = Target->GetAbilitySystemComponent();
    const FDelegateHandle DamageProbe = TargetASC->OnGameplayEffectAppliedDelegateToSelf.AddStatic(
        &OnPulseDamageApplied, ASC, &bPulseAppliedDamageEffect);
    this->TestEqual(TEXT("Pulse accepted"), Loadout->TryActivateSlot(1, nullptr, Aim, false), EHBActionResult::EHBAR_Accepted);
    TargetASC->OnGameplayEffectAppliedDelegateToSelf.Remove(DamageProbe);
    this->TestTrue(TEXT("Pulse applies the Blueprint Damage GE from the player's ASC"), bPulseAppliedDamageEffect);
    const UAnimMontage* PulseMontage = Anim->GetCurrentActiveMontage();
    this->TestTrue(TEXT("Pulse uses the charged attack animation"), PulseMontage && PulseMontage->GetFName() == TEXT("AM_GAS_PulseCast"));
    TArray<FString> PulseEvents;
    Player->GetWorld()->GetSubsystem<UHBDebugSubsystem>()->GetRecent(PulseEvents, 12);
    this->TestTrue(TEXT("Pulse Gameplay Cue executes the shockwave"), FString::Join(PulseEvents, TEXT("\n")).Contains(TEXT("GameplayCue.GASDemo.Pulse executed")));
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
    GameplayPanel->Refresh();
    const UImage* ReassignedIcon = Cast<UImage>(GameplayPanel->GetWidgetFromName(TEXT("AbilityIcon2")));
    UTexture2D* PulseIcon = Loadout->GetSlotSpec(1).Icon;
    this->TestTrue(TEXT("Reassigned slot displays the Pulse icon"), PulseIcon && ReassignedIcon &&
        ReassignedIcon->GetBrush().GetResourceObject() == PulseIcon);
    Loadout->AssignAbilityToSlot(2, EHBAbility::EHBA_Dodge);

    Mode->ResetGASDemo();
    this->TestEqual(TEXT("Reset restores mana"), Player->GetGASMana(), 100.0f);
    this->TestNull(TEXT("Reset stops the charged attack animation"), Anim->GetCurrentActiveMontage());
    this->TestFalse(TEXT("Reset clears cooldown tag"), ASC->HasMatchingGameplayTag(HBGetCooldownTag(EHBAbility::EHBA_Pulse)));
    this->TestEqual(TEXT("Guard accepted"), Loadout->TryActivateSlot(3, nullptr, Player->GetActorLocation(), false), EHBActionResult::EHBAR_Accepted);
    const FGameplayTag GuardTag = FGameplayTag::RequestGameplayTag(TEXT("State.Guarding"));
    const TArray<FActiveGameplayEffectHandle> GuardEffects = ASC->GetActiveEffects(
        FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(FGameplayTagContainer(GuardTag)));
    const FActiveGameplayEffect* Guard = GuardEffects.Num() == 1 ? ASC->GetActiveGameplayEffect(GuardEffects[0]) : nullptr;
    this->TestTrue(TEXT("Guard grants a timed Blueprint buff"), Guard && Guard->Spec.Def->GetClass()->GetName() == TEXT("GE_GASDemo_Guard_C") &&
        FMath::IsNearlyEqual(Guard->Spec.GetDuration(), Loadout->GetSlotSpec(3).fDuration));
    const float GuardReduction = FMath::Clamp(Loadout->GetSlotSpec(3).fPower, 0.0f, 0.95f);
    Mode->DamageGASDemoPlayer();
    this->TestEqual(TEXT("Guard reduces practice combat damage"), Player->GetHealth(), Player->GetMaxHealth() - 60.0f * (1.0f - GuardReduction));
    Mode->ResetGASDemo();
    this->TestFalse(TEXT("Reset removes the Guard buff"), ASC->HasMatchingGameplayTag(GuardTag));
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
    Target = Mode->GetEnemies().IsEmpty() ? nullptr : Mode->GetEnemies()[0].Get();
    if (!this->TestNotNull(TEXT("Montage test target spawned"), Target)) { return false; }
    Player->SetActorLocation(Target->GetActorLocation() - FVector(800.0f, 0.0f, 0.0f));
    this->TestEqual(TEXT("Out-of-range in-place Basic is rejected"), Loadout->TryActivateSlot(0, Target, Target->GetActorLocation(), true), EHBActionResult::EHBAR_InvalidTarget);
    Player->SetActorLocation(Target->GetActorLocation() - FVector(120.0f, 0.0f, 0.0f));
    this->TestEqual(TEXT("Guard reapplied for expiry check"), Loadout->TryActivateSlot(3, nullptr, Player->GetActorLocation(), false), EHBActionResult::EHBAR_Accepted);
    const float ExpectedHealth = Target->GetHealth() - 2.0f * Loadout->GetSlotSpec(0).fPower;
    this->TestEqual(TEXT("Basic starts with the Montage assigned"), Loadout->TryActivateSlot(0, Target, Target->GetActorLocation(), false), EHBActionResult::EHBAR_Accepted);
    this->TestEqual(TEXT("Basic uses animation playback"), Player->GetAttackPlayback(), FString(TEXT("Montage")));
    const FGameplayAbilitySpec* Basic = ASC->FindAbilitySpecFromClass(Loadout->GetAbilityClass(EHBAbility::EHBA_Basic));
    this->TestTrue(TEXT("Basic remains active while montage and hit tasks run"), Basic && Basic->IsActive());
    this->TestEqual(TEXT("Second Basic can queue during montage"), Loadout->TryActivateSlot(0, Target, Target->GetActorLocation(), false), EHBActionResult::EHBAR_Accepted);
    ADD_LATENT_AUTOMATION_COMMAND(FHBVerifyMontageHit(this, Player, Target, ExpectedHealth, FPlatformTime::Seconds() + 4.0));
    return true;
}

namespace
{
    void DefeatArenaEnemies(AHBGameMode& Mode)
    {
        const TArray<TWeakObjectPtr<AHBEnemyCharacter>> Enemies = Mode.GetEnemies();
        for (const TWeakObjectPtr<AHBEnemyCharacter>& Enemy : Enemies)
        {
            if (Enemy.IsValid())
            {
                Enemy->ApplyHealthDelta(-Enemy->GetHealth());
            }
        }
    }
}

DEFINE_LATENT_AUTOMATION_COMMAND_FOUR_PARAMETER(FHBVerifyArenaDungeon, FAutomationTestBase*, Test,
    TWeakObjectPtr<AHBGameMode>, Mode, TWeakObjectPtr<AHBEnemyCharacter>, Enemy, double, Deadline);

bool FHBVerifyArenaDungeon::Update()
{
    AHBPlayerCharacter* Player = Mode.IsValid() ? Mode->GetPlayer() : nullptr;
    if (Player && Player->GetHealth() >= Player->GetMaxHealth() && FPlatformTime::Seconds() < Deadline)
    {
        return false;
    }
    if (!Test->TestTrue(TEXT("Dungeon AI reaches and damages the player"), Enemy.IsValid() &&
        Player && Player->IsAlive() && Player->GetHealth() < Player->GetMaxHealth()))
    {
        return true;
    }
    DefeatArenaEnemies(*Mode);
    Test->TestEqual(TEXT("Six dungeon kills earn 90 stones"), Mode->GetStones(), 90);
    Test->TestEqual(TEXT("Six dungeon kills earn six threat"), Mode->GetThreat(), 6);
    Test->TestTrue(TEXT("The last kill displays the return instruction"), Mode->GetStatus().Contains(TEXT("Dungeon clear")));
    const FVector Cache = Mode->GetGreedPosition(0);
    Player->SetActorLocation(FVector(Cache.X, Cache.Y, Player->GetActorLocation().Z));
    Mode->ClaimNearbyGreed();
    Test->TestTrue(TEXT("The Dungeon cache can be collected"), Mode->IsGreedClaimed(0));
    Test->TestEqual(TEXT("The cache grants 30 stones"), Mode->GetStones(), 120);
    Test->TestEqual(TEXT("The cache adds three threat"), Mode->GetThreat(), 9);
    Test->TestTrue(TEXT("Claiming a cache keeps the return instruction"), Mode->GetStatus().Contains(TEXT("returns to prep")));
    Mode->AdvancePhase();
    Test->TestEqual(TEXT("Cleared dungeon advances to prep"), Mode->GetPhase(), EHBPhase::EHBP_Return);
    Mode->ToggleBuildMode();
    Mode->ConfirmBuild();
    Test->TestTrue(TEXT("The selected socket has a turret"), Mode->IsBuildSocketOccupied());
    Test->TestEqual(TEXT("A turret costs 30 stones"), Mode->GetStones(), 90);
    Mode->AdvancePhase();
    Test->TestEqual(TEXT("Prep advances to Defense"), Mode->GetPhase(), EHBPhase::EHBP_Defense);
    Test->TestEqual(TEXT("The first wave spawns six enemies at threat nine"), Mode->GetEnemies().Num(), 6);
    return true;
}

DEFINE_LATENT_AUTOMATION_COMMAND_FIVE_PARAMETER(FHBVerifyArenaDefense, FAutomationTestBase*, Test,
    TWeakObjectPtr<AHBGameMode>, Mode, int32, LastClearedWave, bool, bTurretHit, double, Deadline);

bool FHBVerifyArenaDefense::Update()
{
    if (!Test->TestTrue(TEXT("Defense finishes within the test window"), Mode.IsValid() && FPlatformTime::Seconds() < Deadline))
    {
        return true;
    }
    if (Mode->GetPhase() != EHBPhase::EHBP_Defense)
    {
        Test->TestEqual(TEXT("All five waves ran"), LastClearedWave, 5);
        Test->TestEqual(TEXT("Defense victory returns to Town"), Mode->GetPhase(), EHBPhase::EHBP_Town);
        Test->TestEqual(TEXT("Defense victory advances the cycle"), Mode->GetCycle(), 2);
        return true;
    }
    if (Mode->GetWave() > LastClearedWave)
    {
        if (LastClearedWave == 0)
        {
            for (const TWeakObjectPtr<AHBEnemyCharacter>& Enemy : Mode->GetEnemies())
            {
                bTurretHit |= Enemy.IsValid() && Enemy->GetHealth() < Enemy->GetMaxHealth();
            }
            if (!bTurretHit || Mode->GetGateHealth() >= 350.0f)
            {
                return false;
            }
            Test->TestEqual(TEXT("The intact Gate shields Anima from wave AI"), Mode->GetCoreHealth(), 250.0f);
            Test->AddInfo(TEXT("First Defense wave damaged the Gate and the built turret damaged a hostile."));
        }
        else
        {
            Test->TestEqual(TEXT("Wave spawns the expected hostiles"), Mode->GetEnemies().Num(), Mode->GetWave() + 5);
        }
        LastClearedWave = Mode->GetWave();
        DefeatArenaEnemies(*Mode);
    }
    return false;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBDemoArenaTest, "HideInTheBox.DemoArena.PlayLoop",
    EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FHBDemoArenaTest::RunTest(const FString& Parameters)
{
    AHBGameMode* Mode = nullptr;
    for (const FWorldContext& Context : GEngine->GetWorldContexts())
    {
        if (Context.World() && Context.World()->IsGameWorld() && Context.World()->GetMapName().EndsWith(TEXT("DemoArena")))
        {
            Mode = Context.World()->GetAuthGameMode<AHBGameMode>();
            break;
        }
    }
    if (!this->TestTrue(TEXT("DemoArena starts in Town"), Mode && !Mode->IsGASDemo() && Mode->GetPhase() == EHBPhase::EHBP_Town))
    {
        return false;
    }
    AHBPlayerCharacter* Player = Mode->GetPlayer();
    const AHBPlayerController* Controller = Player ? Cast<AHBPlayerController>(Player->GetController()) : nullptr;
    UUserWidget* HUD = Controller ? Controller->GetPracticeHUD() : nullptr;
    if (!this->TestNotNull(TEXT("The full-game Widget Blueprint is mounted"), HUD)) { return false; }
    this->TestEqual(TEXT("Arena uses its own Widget Blueprint"), HUD->GetClass()->GetName(), FString(TEXT("WBP_DemoArenaHUD_C")));
    this->TestEqual(TEXT("Arena shares the GAS mannequin"), Player->GetClass()->GetName(), FString(TEXT("BP_GASDemoPlayer_C")));
    this->TestEqual(TEXT("Arena grants six abilities"), Player->GetAbilitySystemComponent()->GetActivatableAbilities().Num(), 6);
    this->TestEqual(TEXT("Gate starts at full HP"), Mode->GetGateHealth(), 350.0f);
    this->TestEqual(TEXT("Anima starts at full HP"), Mode->GetCoreHealth(), 250.0f);
    Mode->AdvancePhase();
    this->TestEqual(TEXT("Advance enters Dungeon"), Mode->GetPhase(), EHBPhase::EHBP_Dungeon);
    if (!this->TestEqual(TEXT("Dungeon spawns six enemies"), Mode->GetEnemies().Num(), 6)) { return false; }
    UHBPracticeWidget* GameplayPanel = Cast<UHBPracticeWidget>(HUD->GetWidgetFromName(TEXT("GameplayPanel")));
    if (!this->TestNotNull(TEXT("Arena gameplay panel mounted"), GameplayPanel)) { return false; }
    GameplayPanel->Refresh();
    const UTextBlock* Phase = Cast<UTextBlock>(GameplayPanel->GetWidgetFromName(TEXT("ArenaPhase")));
    this->TestTrue(TEXT("The HUD displays the current phase"), Phase && Phase->GetText().ToString().Contains(TEXT("DUNGEON")));
    AHBEnemyCharacter* Enemy = Mode->GetEnemies()[0].Get();
    if (!this->TestNotNull(TEXT("Dungeon enemy"), Enemy)) { return false; }
    this->TestTrue(TEXT("Dungeon enemy AI is enabled"), Enemy->IsActorTickEnabled());
    FHitResult FloorHit;
    FCollisionQueryParams FloorQuery;
    FloorQuery.AddIgnoredActor(Player);
    this->TestTrue(TEXT("Dungeon floor supports mouse targeting"), Player->GetWorld()->LineTraceSingleByChannel(
        FloorHit, Player->GetActorLocation(), Player->GetActorLocation() - FVector(0, 0, 500), ECC_GameTraceChannel1, FloorQuery));
    const float Health = Enemy->GetHealth();
    this->TestEqual(TEXT("Pulse casts in Dungeon"), Player->GetLoadout()->TryActivateSlot(1, nullptr, Enemy->GetActorLocation(), false), EHBActionResult::EHBAR_Accepted);
    this->TestEqual(TEXT("Pulse damages the live enemy through GAS"), Enemy->GetHealth(), Health - 45.0f);
    ADD_LATENT_AUTOMATION_COMMAND(FHBVerifyArenaDungeon(this, Mode, Enemy, FPlatformTime::Seconds() + 8.0));
    ADD_LATENT_AUTOMATION_COMMAND(FHBVerifyArenaDefense(this, Mode, 0, false, FPlatformTime::Seconds() + 35.0));
    return true;
}
#endif
