#include "HBCommonGameplayHUDWidget.h"
#include "HBLoadout.h"
#include "HBPlayerCharacter.h"
#include "HBPlayerController.h"
#include "HBSkillButton.h"
#include "Blueprint/WidgetTree.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "UObject/UObjectIterator.h"
#include "UnrealClient.h"

#if WITH_DEV_AUTOMATION_TESTS
// Run in the native demo's standalone game world; exercises the actual CommonButton callback.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBSkillUIButtonTest, "HideInTheBox.Skills.CommonUIButton", EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FHBSkillUIButtonTest::RunTest(const FString& Parameters)
{
    AHBPlayerController* Controller = nullptr;
    for (const FWorldContext& Context : GEngine->GetWorldContexts())
    {
        if (Context.WorldType == EWorldType::Game && Context.World())
        {
            Controller = Cast<AHBPlayerController>(Context.World()->GetFirstPlayerController());
            if (Controller)
            {
                break;
            }
        }
    }
    AHBPlayerCharacter* Character = Controller ? Cast<AHBPlayerCharacter>(Controller->GetPawn()) : nullptr;
    if (!this->TestNotNull(TEXT("Standalone HB player exists"), Character))
    {
        return false;
    }
    UHBCommonGameplayHUDWidget* SkillHUD = nullptr;
    for (TObjectIterator<UHBCommonGameplayHUDWidget> It; It; ++It)
    {
        if (It->GetOwningPlayer() == Controller && It->IsInViewport())
        {
            SkillHUD = *It;
            break;
        }
    }
    if (!this->TestNotNull(TEXT("Controller created a CommonUI HUD"), SkillHUD))
    {
        return false;
    }
    UHBLoadoutComponent* Loadout = Character->GetLoadout();
    UHBSkillButton* GuardButton = Cast<UHBSkillButton>(SkillHUD->WidgetTree->FindWidget(TEXT("SkillButton_3")));
    UHBSkillButton* AliasButton = Cast<UHBSkillButton>(SkillHUD->WidgetTree->FindWidget(TEXT("SkillButton_5")));
    if (!this->TestNotNull(TEXT("Guard has a CommonButton"), GuardButton) || !this->TestNotNull(TEXT("Slot 6 has a CommonButton"), AliasButton))
    {
        return false;
    }
    const EHBAbility PreviousGuard = Loadout->GetSlotAbility(3);
    const EHBAbility PreviousAlias = Loadout->GetSlotAbility(5);
    this->TestTrue(TEXT("Assign Guard to both test slots"), Loadout->AssignAbilityToSlot(3, EHBAbility::EHBA_Guard) && Loadout->AssignAbilityToSlot(5, EHBAbility::EHBA_Guard));
    const float ManaBefore = Loadout->GetMana();
    const float Cost = Loadout->GetSlotSpec(3).fManaCost;
    GuardButton->OnClicked().Broadcast();
    this->TestTrue(TEXT("CommonUI callback spends the real mana cost"), FMath::IsNearlyEqual(Loadout->GetMana(), ManaBefore - Cost));
    this->TestTrue(TEXT("CommonUI callback starts a real cooldown"), Loadout->GetSlotCooldown(3) > 0.0f);
    this->TestTrue(TEXT("Another slot shares the same ability cooldown"), FMath::IsNearlyEqual(Loadout->GetSlotCooldown(3), Loadout->GetSlotCooldown(5)));
    const float ManaAfter = Loadout->GetMana();
    const float CooldownAfter = Loadout->GetSlotCooldown(3);
    AliasButton->OnClicked().Broadcast();
    this->TestTrue(TEXT("Second button cannot spend mana during shared cooldown"), FMath::IsNearlyEqual(Loadout->GetMana(), ManaAfter));
    this->TestTrue(TEXT("Rejected click does not restart cooldown"), FMath::IsNearlyEqual(Loadout->GetSlotCooldown(3), CooldownAfter));
    this->TestTrue(TEXT("Player receives readable cooldown feedback"), Controller->GetLastActionResult().Contains(TEXT("On cooldown")));
    Loadout->AssignAbilityToSlot(3, PreviousGuard);
    Loadout->AssignAbilityToSlot(5, PreviousAlias);
    SkillHUD->RefreshFromGame();
    if (FParse::Param(FCommandLine::Get(), TEXT("HBSkillScreenshot")))
    {
        FScreenshotRequest::RequestScreenshot(TEXT("SkillUICooldown"), true, false);
    }
    return true;
}
#endif
