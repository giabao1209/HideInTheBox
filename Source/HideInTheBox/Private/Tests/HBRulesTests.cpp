#include "HBTypes.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBDeadZoneTest, "HideInTheBox.Rules.DeadZone", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBDeadZoneTest::RunTest(const FString& Parameters)
{
    this->TestTrue(TEXT("Small stick noise is neutral"), HBRules::ApplyRadialDeadZone(FVector2D(0.1, 0.05), 0.2f).IsNearlyZero());
    const FVector2D Partial = HBRules::ApplyRadialDeadZone(FVector2D(0.6, 0.0), 0.2f);
    this->TestTrue(TEXT("Partial deflection preserves analog magnitude"), FMath::IsNearlyEqual(Partial.Size(), 0.5, 0.001));
    this->TestTrue(TEXT("Full diagonal input is clamped"), FMath::IsNearlyEqual(HBRules::ApplyRadialDeadZone(FVector2D(1.0, 1.0), 0.2f).Size(), 1.0, 0.001));
    this->TestTrue(TEXT("Negative direction preserved"), HBRules::ApplyRadialDeadZone(FVector2D(-1.0, 0.0), 0.2f).X < 0.0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBThreatTest, "HideInTheBox.Rules.ThreatScaling", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBThreatTest::RunTest(const FString& Parameters)
{
    this->TestEqual(TEXT("First wave after six dungeon kills"), HBRules::GetWaveEnemyCount(1, 6), 5);
    this->TestEqual(TEXT("Fifth wave after six dungeon kills"), HBRules::GetWaveEnemyCount(5, 6), 9);
    this->TestTrue(TEXT("More greed cannot reduce enemy count"), HBRules::GetWaveEnemyCount(2, 12) >= HBRules::GetWaveEnemyCount(2, 0));
    this->TestEqual(TEXT("Prototype spawn safety cap"), HBRules::GetWaveEnemyCount(100, 1000), 16);
    return true;
}
#endif
