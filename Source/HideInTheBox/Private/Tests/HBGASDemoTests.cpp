#include "HBGameMode.h"
#include "HBPlayerCharacter.h"
#include "HBEnemyCharacter.h"
#include "HBEffects.h"
#include "HBGameplayAbility.h"
#include "HBPlayerController.h"
#include "HBPracticeWidget.h"
#include "HBStructure.h"
#include "HBDebugSubsystem.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "CommonActivatableWidget.h"
#include "CommonButtonBase.h"
#include "CommonGameViewportClient.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/WidgetComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/DamageEvents.h"
#include "Engine/Engine.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "InputKeyEventArgs.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Widgets/CommonActivatableWidgetContainer.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
    void SendPulseImpact(AHBPlayerCharacter& Player)
    {
        FGameplayEventData Event;
        Event.EventTag = HBGetPulseImpactTag();
        Event.Instigator = &Player;
        UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(&Player, Event.EventTag, Event);
    }

    void SendInput(AHBPlayerController& Controller, const FKey& Key, EInputEvent Event)
    {
        const FInputDeviceId Device = IPlatformInputDeviceMapper::Get().GetDefaultInputDevice();
        Controller.InputKey(FInputKeyEventArgs(nullptr, Device, Key, Event, FPlatformTime::Cycles64()));
    }

    bool ClickPracticeButton(UUserWidget& Panel, FName Name)
    {
        UCommonButtonBase* Button = Cast<UCommonButtonBase>(Panel.GetWidgetFromName(Name));
        UFunction* Click = Button ? Button->FindFunction(TEXT("HandleButtonClicked")) : nullptr;
        if (!Click)
        {
            return false;
        }
        Button->ProcessEvent(Click, nullptr);
        return true;
    }

    void OnPulseDamageApplied(UAbilitySystemComponent* Source, const FGameplayEffectSpec& Spec,
        FActiveGameplayEffectHandle, UAbilitySystemComponent* ExpectedSource, bool* bApplied)
    {
        *bApplied |= Source == ExpectedSource && Spec.Def->GetClass()->GetName() == TEXT("GE_GASDemo_Damage_C");
    }
}

DEFINE_LATENT_AUTOMATION_COMMAND_TWO_PARAMETER(FHBStartMappedInput, TWeakObjectPtr<AHBPlayerController>, Controller, FKey, GuardKey);

bool FHBStartMappedInput::Update()
{
    if (Controller.IsValid())
    {
        SendInput(*Controller, EKeys::W, IE_Pressed);
        SendInput(*Controller, EKeys::F7, IE_Pressed);
        SendInput(*Controller, GuardKey, IE_Pressed);
    }
    return true;
}

DEFINE_LATENT_AUTOMATION_COMMAND_FIVE_PARAMETER(FHBVerifyMappedInput, FAutomationTestBase*, Test,
    TWeakObjectPtr<AHBPlayerController>, Controller, FVector, Start, FKey, GuardKey, FKey, OriginalKey);

bool FHBVerifyMappedInput::Update()
{
    AHBPlayerCharacter* Player = Controller.IsValid() ? Cast<AHBPlayerCharacter>(Controller->GetPawn()) : nullptr;
    if (!Test->TestNotNull(TEXT("Player remains possessed"), Player)) { return true; }
    SendInput(*Controller, EKeys::W, IE_Released);
    SendInput(*Controller, EKeys::F7, IE_Released);
    SendInput(*Controller, GuardKey, IE_Released);
    Test->TestTrue(TEXT("The IMC keyboard movement action moves the pawn"),
        FVector::DistSquaredXY(Start, Player->GetActorLocation()) > 1.0f);
    Test->TestTrue(TEXT("The IMC practice action damages the player"), Player->GetHealth() < Player->GetMaxHealth());
    Test->TestTrue(TEXT("The remapped Guard key starts its GAS cooldown"), Player->GetGASCooldown(EHBAbility::EHBA_Guard) > 0.0f);
    Test->TestTrue(TEXT("Original key restored without writing a save"), Controller->RebindSlotKey(3, OriginalKey, false));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBInputMappingTest, "HideInTheBox.Input.MappingContexts",
    EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FHBInputMappingTest::RunTest(const FString& Parameters)
{
    AHBGameMode* Mode = nullptr;
    for (const FWorldContext& Context : GEngine->GetWorldContexts())
    {
        if (Context.World() && Context.World()->IsGameWorld())
        {
            Mode = Context.World()->GetAuthGameMode<AHBGameMode>();
            if (Mode) { break; }
        }
    }
    if (!TestNotNull(TEXT("Run in DemoArena or GASWidgetArena with -game"), Mode)) { return false; }
    Mode->ResetGASDemo();
    AHBPlayerCharacter* Player = Mode->GetPlayer();
    AHBPlayerController* Controller = Player ? Cast<AHBPlayerController>(Player->GetController()) : nullptr;
    if (!TestNotNull(TEXT("Arena controller"), Controller)) { return false; }
    const FKey OriginalKey = Controller->GetSlotKey(3, false);
    TestFalse(TEXT("A key used by the gameplay IMC cannot be assigned to a slot"), Controller->RebindSlotKey(3, EKeys::W, false));
    FKey GuardKey = EKeys::F9;
    if (!Controller->RebindSlotKey(3, GuardKey, false))
    {
        GuardKey = EKeys::F10;
        if (!TestTrue(TEXT("Guard slot accepts an unused key"), Controller->RebindSlotKey(3, GuardKey, false))) { return false; }
    }
    ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.1f));
    ADD_LATENT_AUTOMATION_COMMAND(FHBStartMappedInput(Controller, GuardKey));
    ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.3f));
    ADD_LATENT_AUTOMATION_COMMAND(FHBVerifyMappedInput(this, Controller, Player->GetActorLocation(), GuardKey, OriginalKey));
    return true;
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
    Test->TestTrue(TEXT("Periodic GAS effect restores mana after Guard"), Player.IsValid() &&
        Player->GetGASMana() > Player->GetGASMaxMana() - Player->GetLoadout()->GetSlotSpec(3).fManaCost);
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
    AHBPlayerController* Controller = Cast<AHBPlayerController>(Player->GetController());
    UUserWidget* HUD = Controller ? Controller->GetPracticeHUD() : nullptr;
    if (!this->TestNotNull(TEXT("Widget Blueprint HUD mounted"), HUD)) { return false; }
    const UCommonActivatableWidget* CommonHUD = Cast<UCommonActivatableWidget>(HUD);
    this->TestTrue(TEXT("The practice HUD is an active CommonUI screen"), CommonHUD && CommonHUD->IsActivated());
    this->TestTrue(TEXT("The game viewport routes CommonUI input"), Cast<UCommonGameViewportClient>(Player->GetWorld()->GetGameViewport()) != nullptr);
    UCommonActivatableWidgetStack* MenuStack = Cast<UCommonActivatableWidgetStack>(HUD->GetWidgetFromName(TEXT("MenuStack")));
    if (!this->TestNotNull(TEXT("HUD has a CommonUI menu stack"), MenuStack)) { return false; }
    Controller->SetArenaPaused(true);
    UCommonActivatableWidget* PauseMenu = MenuStack->GetActiveWidget();
    this->TestTrue(TEXT("Pause pushes an active menu"), Controller->IsPaused() && PauseMenu && PauseMenu->IsActivated());
    if (PauseMenu)
    {
        this->TestTrue(TEXT("The CommonUI Resume button can be clicked"), ClickPracticeButton(*PauseMenu, TEXT("ResumeButton")));
    }
    this->TestFalse(TEXT("Resume unpauses the world"), Controller->IsPaused());
    this->TestNull(TEXT("Resume removes the pause menu from the stack"), MenuStack->GetActiveWidget());
    Controller->SetArenaPaused(false);
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
    const FGameplayTag RegenTag = FGameplayTag::RequestGameplayTag(TEXT("State.ManaRegenerating"));
    this->TestTrue(TEXT("Mana regeneration is an active Gameplay Effect"), ASC->HasMatchingGameplayTag(RegenTag));
    Player->SetBuildMode(true);
    this->TestFalse(TEXT("Build mode suspends mana regeneration"), ASC->HasMatchingGameplayTag(RegenTag));
    Player->SetBuildMode(false);
    this->TestTrue(TEXT("Leaving build mode restores mana regeneration"), ASC->HasMatchingGameplayTag(RegenTag));
    this->TestEqual(TEXT("Three practice targets"), Mode->GetEnemies().Num(), 3);
    if (Mode->GetEnemies().Num() != 3) { return false; }
    AHBEnemyCharacter* Target = Mode->GetEnemies()[0].Get();
    if (!this->TestNotNull(TEXT("Target spawned"), Target)) { return false; }
    this->TestFalse(TEXT("Target AI disabled"), Target->IsActorTickEnabled());
    this->TestTrue(TEXT("Target mesh and idle animation are assigned"),
        Target->GetMesh()->GetSkeletalMeshAsset() && Target->GetMesh()->GetAnimInstance());
    UWidgetComponent* TargetHealthComponent = Target->FindComponentByClass<UWidgetComponent>();
    UHBGASTargetWidget* TargetWidget = TargetHealthComponent ? Cast<UHBGASTargetWidget>(TargetHealthComponent->GetUserWidgetObject()) : nullptr;
    this->TestNotNull(TEXT("Target uses its CommonUI Widget Blueprint"), TargetWidget);
    const FVector Aim = Target->GetActorLocation();
    const float TargetHealth = Target->GetHealth();
    bool bPulseAppliedDamageEffect = false;
    UAbilitySystemComponent* TargetASC = Target->GetAbilitySystemComponent();
    const FDelegateHandle DamageProbe = TargetASC->OnGameplayEffectAppliedDelegateToSelf.AddStatic(
        &OnPulseDamageApplied, ASC, &bPulseAppliedDamageEffect);
    this->TestEqual(TEXT("Pulse accepted"), Loadout->TryActivateSlot(1, nullptr, Aim, false), EHBActionResult::EHBAR_Accepted);
    const UAnimMontage* PulseMontage = Anim->GetCurrentActiveMontage();
    this->TestTrue(TEXT("Pulse uses the charged attack animation"), PulseMontage && PulseMontage->GetFName() == TEXT("AM_GAS_PulseCombat"));
    this->TestTrue(TEXT("Pulse owns a casting state before impact"), ASC->HasMatchingGameplayTag(HBGetPulseCastingTag()));
    this->TestEqual(TEXT("Pulse cannot damage before its impact event"), Target->GetHealth(), TargetHealth);
    this->TestTrue(TEXT("Casting blocks Bolt"), Loadout->TryActivateSlot(5, Target, Aim, false) != EHBActionResult::EHBAR_Accepted);
    SendPulseImpact(*Player);
    SendPulseImpact(*Player);
    TargetASC->OnGameplayEffectAppliedDelegateToSelf.Remove(DamageProbe);
    this->TestTrue(TEXT("Pulse applies the Blueprint Damage GE from the player's ASC"), bPulseAppliedDamageEffect);
    this->TestFalse(TEXT("Impact releases the Pulse casting state"), ASC->HasMatchingGameplayTag(HBGetPulseCastingTag()));
    this->TestTrue(TEXT("A surviving target becomes Exposed"), TargetASC->HasMatchingGameplayTag(HBGetExposedTag()));
    TArray<FString> PulseEvents;
    Player->GetWorld()->GetSubsystem<UHBDebugSubsystem>()->GetRecent(PulseEvents, 12);
    this->TestTrue(TEXT("Pulse Gameplay Cue executes the shockwave"), FString::Join(PulseEvents, TEXT("\n")).Contains(TEXT("GameplayCue.GASDemo.Pulse executed")));
    this->TestEqual(TEXT("Pulse spends 20 mana"), Player->GetGASMana(), 80.0f);
    this->TestEqual(TEXT("Pulse GE deals 45 damage"), Target->GetHealth(), TargetHealth - 45.0f);
    const UProgressBar* TargetBar = TargetWidget ? Cast<UProgressBar>(TargetWidget->GetWidgetFromName(TEXT("TargetHealthBar"))) : nullptr;
    this->TestTrue(TEXT("Health bar updates immediately from the attribute delegate"), TargetBar &&
        FMath::IsNearlyEqual(TargetBar->GetPercent(), Target->GetHealth() / Target->GetMaxHealth()));
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

    this->TestEqual(TEXT("Bolt rejects a cast without an enemy"), Loadout->TryActivateSlot(5, nullptr, Aim, false), EHBActionResult::EHBAR_InvalidTarget);
    this->TestEqual(TEXT("A failed Bolt costs no mana"), Player->GetGASMana(), 80.0f);
    Target->Heal(30.0f);
    const float ExposedHealth = Target->GetHealth();
    this->TestEqual(TEXT("Bolt hits the selected enemy"), Loadout->TryActivateSlot(5, Target, Aim, false), EHBActionResult::EHBAR_Accepted);
    this->TestEqual(TEXT("Bolt deals 45 damage to an Exposed target"), Target->GetHealth(), ExposedHealth - 45.0f);
    this->TestFalse(TEXT("Bolt consumes the Exposed effect"), TargetASC->HasMatchingGameplayTag(HBGetExposedTag()));
    this->TestEqual(TEXT("Bolt spends 10 mana"), Player->GetGASMana(), 70.0f);
    this->TestTrue(TEXT("Bolt starts its cooldown"), ASC->HasMatchingGameplayTag(HBGetCooldownTag(EHBAbility::EHBA_Bolt)));
    TArray<FString> BoltEvents;
    Player->GetWorld()->GetSubsystem<UHBDebugSubsystem>()->GetRecent(BoltEvents, 12);
    this->TestTrue(TEXT("Bolt dispatches its lightning Gameplay Cue"), FString::Join(BoltEvents, TEXT("\n")).Contains(TEXT("GameplayCue.GASDemo.Bolt executed")));

    Mode->ResetGASDemo();
    this->TestEqual(TEXT("Reset restores mana"), Player->GetGASMana(), 100.0f);
    this->TestNull(TEXT("Reset stops the charged attack animation"), Anim->GetCurrentActiveMontage());
    this->TestFalse(TEXT("Reset clears cooldown tag"), ASC->HasMatchingGameplayTag(HBGetCooldownTag(EHBAbility::EHBA_Pulse)));
    AHBEnemyCharacter* CancelTarget = Mode->GetEnemies()[0].Get();
    if (!this->TestNotNull(TEXT("Target for interrupted Pulse"), CancelTarget)) { return false; }
    const float CancelHealth = CancelTarget->GetHealth();
    this->TestEqual(TEXT("Pulse starts a second windup"), Loadout->TryActivateSlot(1, nullptr, CancelTarget->GetActorLocation(), false), EHBActionResult::EHBAR_Accepted);
    this->TestEqual(TEXT("Dodge can interrupt a charging Pulse"), Loadout->TryActivateSlot(2, nullptr, Player->GetActorLocation(), false), EHBActionResult::EHBAR_Accepted);
    this->TestFalse(TEXT("Dodge removes the charging state"), ASC->HasMatchingGameplayTag(HBGetPulseCastingTag()));
    SendPulseImpact(*Player);
    this->TestEqual(TEXT("An interrupted Pulse ignores a late impact event"), CancelTarget->GetHealth(), CancelHealth);
    this->TestFalse(TEXT("An interrupted Pulse leaves no mark"), CancelTarget->GetAbilitySystemComponent()->HasMatchingGameplayTag(HBGetExposedTag()));
    Mode->ResetGASDemo();
    CancelTarget = Mode->GetEnemies()[0].Get();
    if (!this->TestNotNull(TEXT("Target for hit interruption"), CancelTarget)) { return false; }
    const FVector CancelAim = CancelTarget->GetActorLocation();
    this->TestEqual(TEXT("Pulse starts without Guard"), Loadout->TryActivateSlot(1, nullptr, CancelAim, false), EHBActionResult::EHBAR_Accepted);
    Player->TakeDamage(60.0f, FDamageEvent(), nullptr, nullptr);
    this->TestFalse(TEXT("Unguarded damage interrupts Pulse"), ASC->HasMatchingGameplayTag(HBGetPulseCastingTag()));
    SendPulseImpact(*Player);
    this->TestEqual(TEXT("Interrupted Pulse deals no damage"), CancelTarget->GetHealth(), CancelHealth);
    Mode->ResetGASDemo();
    this->TestEqual(TEXT("Guard accepted"), Loadout->TryActivateSlot(3, nullptr, Player->GetActorLocation(), false), EHBActionResult::EHBAR_Accepted);
    AHBEnemyCharacter* GuardedCastTarget = Mode->GetEnemies()[0].Get();
    if (!this->TestNotNull(TEXT("Target for protected cast"), GuardedCastTarget)) { return false; }
    this->TestEqual(TEXT("Pulse starts with Guard"), Loadout->TryActivateSlot(1, nullptr, GuardedCastTarget->GetActorLocation(), false), EHBActionResult::EHBAR_Accepted);
    Player->TakeDamage(60.0f, FDamageEvent(), nullptr, nullptr);
    this->TestTrue(TEXT("Guard protects Pulse windup from damage interruption"), ASC->HasMatchingGameplayTag(HBGetPulseCastingTag()));
    SendPulseImpact(*Player);
    this->TestEqual(TEXT("Protected Pulse deals its damage"), GuardedCastTarget->GetHealth(), CancelHealth - 45.0f);
    Mode->ResetGASDemo();
    this->TestEqual(TEXT("Guard accepted after reset"), Loadout->TryActivateSlot(3, nullptr, Player->GetActorLocation(), false), EHBActionResult::EHBAR_Accepted);
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
    if (!this->TestTrue(TEXT("The CommonUI damage button can be clicked"), ClickPracticeButton(*GameplayPanel, TEXT("DamageButton")))) { return false; }
    this->TestEqual(TEXT("Damage helper removes 60 HP"), Player->GetHealth(), Player->GetMaxHealth() - 60.0f);
    GameplayPanel->Refresh();
    UProgressBar* HealthBar = Cast<UProgressBar>(GameplayPanel->GetWidgetFromName(TEXT("HealthBar")));
    this->TestTrue(TEXT("Widget health bar reflects damage"), HealthBar && FMath::IsNearlyEqual(HealthBar->GetPercent(), Player->GetHealth() / Player->GetMaxHealth()));
    this->TestEqual(TEXT("Mend accepted"), Loadout->TryActivateSlot(4, nullptr, Player->GetActorLocation(), false), EHBActionResult::EHBAR_Accepted);
    this->TestEqual(TEXT("Mend heals 45 HP"), Player->GetHealth(), Player->GetMaxHealth() - 15.0f);
    this->TestEqual(TEXT("Mend spends 25 MP"), Player->GetGASMana(), 75.0f);
    this->TestTrue(TEXT("The CommonUI refill button can be clicked"), ClickPracticeButton(*GameplayPanel, TEXT("RefillButton")));
    this->TestEqual(TEXT("Refill restores mana"), Player->GetGASMana(), 100.0f);
    this->TestTrue(TEXT("The CommonUI reset button can be clicked"), ClickPracticeButton(*GameplayPanel, TEXT("ResetButton")));
    this->TestEqual(TEXT("The reset button restores full HP"), Player->GetHealth(), Player->GetMaxHealth());
    this->TestFalse(TEXT("The reset button clears Mend cooldown"), ASC->HasMatchingGameplayTag(HBGetCooldownTag(EHBAbility::EHBA_Mend)));
    Player->ApplyGASManaDelta(-100.0f);
    this->TestEqual(TEXT("No-mana Pulse rejected"), Loadout->TryActivateSlot(1, nullptr, Aim, false), EHBActionResult::EHBAR_NoMana);
    GameplayPanel->Refresh();
    const UImage* PulseIconAtZeroMana = Cast<UImage>(GameplayPanel->GetWidgetFromName(TEXT("AbilityIcon1")));
    const UImage* BasicIconAtZeroMana = Cast<UImage>(GameplayPanel->GetWidgetFromName(TEXT("AbilityIcon0")));
    this->TestTrue(TEXT("Low mana dims Pulse and leaves free Basic available"),
        PulseIconAtZeroMana && BasicIconAtZeroMana && PulseIconAtZeroMana->GetRenderOpacity() < BasicIconAtZeroMana->GetRenderOpacity());
    Mode->ResetGASDemo();
    this->TestEqual(TEXT("Dodge accepted"), Loadout->TryActivateSlot(2, nullptr, Player->GetActorLocation(), false), EHBActionResult::EHBAR_Accepted);
    const FGameplayTag DodgeTag = FGameplayTag::RequestGameplayTag(TEXT("State.DamageImmune.Dodge"));
    this->TestTrue(TEXT("Dodge grants an immunity tag"), ASC->HasMatchingGameplayTag(DodgeTag));
    const float HealthBeforeDodgeHit = Player->GetHealth();
    Controller->HBGASDamage();
    this->TestEqual(TEXT("Dodge blocks the test hit through the damage execution"), Player->GetHealth(), HealthBeforeDodgeHit);
    this->TestEqual(TEXT("Dodge blocks direct damage effects too"), HBApplyDamageEffect(Mode->GetEnemies()[0].Get(), Player, 60.0f), 0.0f);
    Mode->ResetGASDemo();
    this->TestFalse(TEXT("Reset cancels dodge"), Player->IsDodging());
    this->TestFalse(TEXT("Reset removes Dodge immunity"), ASC->HasMatchingGameplayTag(DodgeTag));
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

DEFINE_LATENT_AUTOMATION_COMMAND_FIVE_PARAMETER(FHBVerifyArenaDungeon, FAutomationTestBase*, Test,
    TWeakObjectPtr<AHBGameMode>, Mode, TWeakObjectPtr<AHBEnemyCharacter>, Enemy, bool, bSawWindup, double, Deadline);

bool FHBVerifyArenaDungeon::Update()
{
    AHBPlayerCharacter* Player = Mode.IsValid() ? Mode->GetPlayer() : nullptr;
    if (Player && Player->GetHealth() >= Player->GetMaxHealth() && FPlatformTime::Seconds() < Deadline)
    {
        for (const TWeakObjectPtr<AHBEnemyCharacter>& Attacker : Mode->GetEnemies())
        {
            const UAnimInstance* Anim = Attacker.IsValid() ? Attacker->GetMesh()->GetAnimInstance() : nullptr;
            const UAnimMontage* ActiveMontage = Anim ? Anim->GetCurrentActiveMontage() : nullptr;
            bSawWindup |= ActiveMontage && ActiveMontage->GetFName() == TEXT("AM_GAS_BasicStrike");
        }
        return false;
    }
    if (!Test->TestTrue(TEXT("Dungeon AI reaches and damages the player"), Enemy.IsValid() &&
        Player && Player->IsAlive() && Player->GetHealth() < Player->GetMaxHealth()))
    {
        return true;
    }
    Test->TestTrue(TEXT("Enemy montage winds up before melee damage"), bSawWindup);
    TArray<FString> CombatEvents;
    Mode->GetWorld()->GetSubsystem<UHBDebugSubsystem>()->GetRecent(CombatEvents, 24);
    Test->TestTrue(TEXT("Enemy melee damage comes from the montage notify"),
        FString::Join(CombatEvents, TEXT("\n")).Contains(TEXT("Enemy melee hit via montage notify")));
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
    AHBPlayerController* Controller = Player ? Cast<AHBPlayerController>(Player->GetController()) : nullptr;
    UUserWidget* HUD = Controller ? Controller->GetPracticeHUD() : nullptr;
    if (!this->TestNotNull(TEXT("The full-game Widget Blueprint is mounted"), HUD)) { return false; }
    const UCommonActivatableWidget* CommonHUD = Cast<UCommonActivatableWidget>(HUD);
    this->TestTrue(TEXT("The full-game HUD is an active CommonUI screen"), CommonHUD && CommonHUD->IsActivated());
    this->TestEqual(TEXT("Arena uses its own Widget Blueprint"), HUD->GetClass()->GetName(), FString(TEXT("WBP_DemoArenaHUD_C")));
    this->TestEqual(TEXT("Arena shares the GAS mannequin"), Player->GetClass()->GetName(), FString(TEXT("BP_GASDemoPlayer_C")));
    this->TestEqual(TEXT("Arena grants six abilities"), Player->GetAbilitySystemComponent()->GetActivatableAbilities().Num(), 6);
    this->TestEqual(TEXT("Gate starts at full HP"), Mode->GetGateHealth(), 350.0f);
    this->TestEqual(TEXT("Anima starts at full HP"), Mode->GetCoreHealth(), 250.0f);
    AHBStructure* Gate = Mode->GetDefenseTarget();
    if (!this->TestNotNull(TEXT("Gate uses the shared Ability System"), Gate ? Gate->GetAbilitySystemComponent() : nullptr)) { return false; }
    this->TestEqual(TEXT("Gate receives the same damage effect as a combatant"), HBApplyDamageEffect(Player, Gate, 8.0f), 8.0f);
    this->TestEqual(TEXT("Gate loses health through its AttributeSet"), Mode->GetGateHealth(), 342.0f);
    Gate->Repair();
    this->TestEqual(TEXT("Gate repair restores its GAS health"), Mode->GetGateHealth(), 350.0f);
    Mode->AdvancePhase();
    this->TestEqual(TEXT("Advance enters Dungeon"), Mode->GetPhase(), EHBPhase::EHBP_Dungeon);
    if (!this->TestEqual(TEXT("Dungeon spawns six enemies"), Mode->GetEnemies().Num(), 6)) { return false; }
    UHBPracticeWidget* GameplayPanel = Cast<UHBPracticeWidget>(HUD->GetWidgetFromName(TEXT("GameplayPanel")));
    if (!this->TestNotNull(TEXT("Arena gameplay panel mounted"), GameplayPanel)) { return false; }
    GameplayPanel->Refresh();
    const UTextBlock* Phase = Cast<UTextBlock>(GameplayPanel->GetWidgetFromName(TEXT("ArenaPhase")));
    this->TestTrue(TEXT("The HUD displays the current phase"), Phase && Phase->GetText().ToString().Contains(TEXT("DUNGEON")));
    const UTextBlock* Instructions = Cast<UTextBlock>(GameplayPanel->GetWidgetFromName(TEXT("Instructions")));
    this->TestTrue(TEXT("The HUD shows the GAS test shortcuts"), Instructions && Instructions->GetText().ToString().Contains(TEXT("F7 HIT")));
    AHBEnemyCharacter* Enemy = Mode->GetEnemies()[0].Get();
    if (!this->TestNotNull(TEXT("Dungeon enemy"), Enemy)) { return false; }
    Controller->HBGASDamage();
    this->TestEqual(TEXT("DemoArena test hit deals 60 HP"), Player->GetHealth(), Player->GetMaxHealth() - 60.0f);
    const FVector ResetLocation = Player->GetActorLocation();
    Controller->HBGASReset();
    this->TestEqual(TEXT("DemoArena reset restores HP"), Player->GetHealth(), Player->GetMaxHealth());
    this->TestEqual(TEXT("DemoArena reset preserves the player's location"), Player->GetActorLocation(), ResetLocation);
    this->TestEqual(TEXT("DemoArena reset preserves the Dungeon phase"), Mode->GetPhase(), EHBPhase::EHBP_Dungeon);
    if (!this->TestTrue(TEXT("DemoArena reset leaves the active enemies in place"),
        Mode->GetEnemies().Num() == 6 && Mode->GetEnemies()[0].Get() == Enemy)) { return false; }
    this->TestEqual(TEXT("Guard casts in DemoArena"), Player->GetLoadout()->TryActivateSlot(3, nullptr, ResetLocation, false), EHBActionResult::EHBAR_Accepted);
    GameplayPanel->Refresh();
    const UTextBlock* GuardSlot = Cast<UTextBlock>(GameplayPanel->GetWidgetFromName(TEXT("Ability3")));
    this->TestTrue(TEXT("The Guard slot shows the active buff before its cooldown"), GuardSlot && GuardSlot->GetText().ToString().Contains(TEXT("Active")));
    Controller->HBGASDamage();
    this->TestEqual(TEXT("Guard reduces the DemoArena test hit to 30 HP"), Player->GetHealth(), Player->GetMaxHealth() - 30.0f);
    Controller->HBGASRefill();
    this->TestEqual(TEXT("DemoArena refill restores MP"), Player->GetGASMana(), Player->GetGASMaxMana());
    this->TestTrue(TEXT("Refill preserves the Guard cooldown"), Player->GetGASCooldown(EHBAbility::EHBA_Guard) > 0.0f);
    Controller->HBGASReset();
    this->TestEqual(TEXT("DemoArena reset clears the Guard cooldown"), Player->GetGASCooldown(EHBAbility::EHBA_Guard), 0.0f);
    this->TestFalse(TEXT("DemoArena reset removes Guard"), Player->GetAbilitySystemComponent()->HasMatchingGameplayTag(
        FGameplayTag::RequestGameplayTag(TEXT("State.Guarding"))));
    GameplayPanel->Refresh();
    this->TestTrue(TEXT("The Guard slot shows Ready after reset"), GuardSlot && GuardSlot->GetText().ToString().Contains(TEXT("Ready")));
    this->TestEqual(TEXT("Dungeon spawns the combat enemy Blueprint"), Enemy->GetClass()->GetName(), FString(TEXT("BP_DemoArenaEnemy_C")));
    this->TestTrue(TEXT("Dungeon enemy AI is enabled"), Enemy->IsActorTickEnabled());
    const FVector MeshFacing = Enemy->GetMesh()->GetComponentQuat().RotateVector(FVector::RightVector);
    this->TestTrue(TEXT("Manny faces the same direction as the enemy actor"), FVector::DotProduct(MeshFacing, Enemy->GetActorForwardVector()) > 0.99f);
    FHitResult FloorHit;
    FCollisionQueryParams FloorQuery;
    FloorQuery.AddIgnoredActor(Player);
    this->TestTrue(TEXT("Dungeon floor supports mouse targeting"), Player->GetWorld()->LineTraceSingleByChannel(
        FloorHit, Player->GetActorLocation(), Player->GetActorLocation() - FVector(0, 0, 500), ECC_GameTraceChannel1, FloorQuery));
    const float Health = Enemy->GetHealth();
    this->TestEqual(TEXT("Pulse casts in Dungeon"), Player->GetLoadout()->TryActivateSlot(1, nullptr, Enemy->GetActorLocation(), false), EHBActionResult::EHBAR_Accepted);
    SendPulseImpact(*Player);
    this->TestEqual(TEXT("Pulse damages the live enemy through GAS"), Enemy->GetHealth(), Health - 45.0f);
    ADD_LATENT_AUTOMATION_COMMAND(FHBVerifyArenaDungeon(this, Mode, Enemy, false, FPlatformTime::Seconds() + 8.0));
    ADD_LATENT_AUTOMATION_COMMAND(FHBVerifyArenaDefense(this, Mode, 0, false, FPlatformTime::Seconds() + 35.0));
    return true;
}
#endif
