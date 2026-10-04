#include "HBRuleCore.h"
#include <cstdlib>
#include <iostream>
#include <limits>

static int32_t iChecks = 0;
static void Expect(bool bCondition, const char* Message)
{
    ++iChecks;
    if (!bCondition)
    {
        std::cerr << "FAIL: " << Message << '\n';
        std::exit(1);
    }
}

static void TestAnalog()
{
    const HBRuleCore::FStick Neutral = HBRuleCore::DeadZone(0.1, -0.1, 0.2);
    Expect(Neutral.fX == 0.0 && Neutral.fY == 0.0, "stick noise is neutral");
    const HBRuleCore::FStick Half = HBRuleCore::DeadZone(0.6, 0.0, 0.2);
    Expect(std::abs(Half.fX - 0.5) < 1e-9, "analog magnitude survives dead zone");
    const HBRuleCore::FStick NaN = HBRuleCore::DeadZone(std::numeric_limits<double>::quiet_NaN(), 0.0, 0.2);
    Expect(NaN.fX == 0.0 && NaN.fY == 0.0, "non-finite input rejected");
    for (int32_t iX = -20; iX <= 20; ++iX)
    {
        for (int32_t iY = -20; iY <= 20; ++iY)
        {
            const HBRuleCore::FStick Result = HBRuleCore::DeadZone(iX / 10.0, iY / 10.0, 0.2);
            Expect(std::hypot(Result.fX, Result.fY) <= 1.000001, "no diagonal speed bonus");
            Expect(Result.fX * iX >= 0.0 && Result.fY * iY >= 0.0, "input direction preserved");
        }
    }
}

static void TestResources()
{
    HBRuleCore::FAbilityBudget Budget;
    double fMana = 25.0;
    Expect(Budget.Commit(1, fMana, 20.0, 1.5, 10.0), "valid cast accepted");
    Expect(fMana == 5.0, "mana debited once");
    fMana = 100.0;
    Expect(!Budget.Commit(1, fMana, 20.0, 1.5, 10.1), "same ability in another slot cannot bypass cooldown");
    Expect(fMana == 100.0, "failed cast does not spend");
    Expect(Budget.Commit(2, fMana, 0.0, 2.0, 10.1), "dodge costs no mana");
    Expect(fMana == 100.0, "zero-cost dodge preserves mana");
    Expect(Budget.Commit(1, fMana, 20.0, 1.5, 11.5), "cooldown boundary accepted");
    Expect(!Budget.Commit(7, fMana, 1.0, 1.0, 20.0), "invalid ability blocked");
    Expect(!Budget.Commit(-1, fMana, 1.0, 1.0, 20.0), "negative index blocked");
    Expect(!Budget.Commit(1, fMana, -3.0, 1.0, 20.0), "negative mana cost blocked");
    Expect(!Budget.Commit(1, fMana, 1.0, -1.0, 20.0), "negative cooldown blocked");
    Expect(!Budget.Commit(1, fMana, std::numeric_limits<double>::infinity(), 1.0, 20.0), "non-finite cost blocked");
    const double fBefore = fMana;
    Expect(!Budget.Commit(4, fMana, 1000.0, 1.0, 20.0) && fBefore == fMana, "not enough mana leaves budget unchanged");
    Budget.Reset();
    Expect(Budget.Remaining(1, 0.0) == 0.0, "reset clears cooldown across phase time origins");
}

static void TestGreed()
{
    HBRuleCore::FGreedLedger Ledger;
    int32_t iStones = 90;
    int32_t iThreat = 6;
    Expect(HBRuleCore::WaveCount(1, iThreat) == 5, "baseline first wave");
    Expect(Ledger.Claim(0, iStones, iThreat), "optional cache claimed");
    Expect(iStones == 120 && iThreat == 9, "cache awards stones and threat together");
    Expect(!Ledger.Claim(0, iStones, iThreat), "same cache never pays twice");
    Expect(iStones == 120 && iThreat == 9, "duplicate claim is atomic no-op");
    Expect(!Ledger.Claim(-1, iStones, iThreat) && !Ledger.Claim(3, iStones, iThreat), "invalid chest id rejected");
    Expect(HBRuleCore::WaveCount(1, iThreat) == 6, "one greed choice adds one enemy per wave");
    Expect(Ledger.Claim(1, iStones, iThreat) && Ledger.Claim(2, iStones, iThreat), "other caches remain independent");
    Expect(iStones == 180 && iThreat == 15 && HBRuleCore::WaveCount(5, iThreat) == 12, "full-greed economy and final wave");
    HBRuleCore::FGreedLedger OverflowLedger;
    int32_t iMaxStones = std::numeric_limits<int32_t>::max();
    int32_t iSafeThreat = 6;
    Expect(!OverflowLedger.Claim(0, iMaxStones, iSafeThreat) && !OverflowLedger.IsClaimed(0), "overflow rejects the whole transaction");
    Ledger.Reset();
    Expect(!Ledger.IsClaimed(0) && Ledger.Claim(0, iStones, iThreat), "new dungeon resets chest ledger");
    Expect(HBRuleCore::WaveCount(std::numeric_limits<int32_t>::max(), std::numeric_limits<int32_t>::max()) == 16, "wave count saturates without integer overflow");
    Expect(HBRuleCore::WaveCount(-10, -10) >= 1, "invalid negative tuning remains bounded");
}

int main()
{
    TestAnalog();
    TestResources();
    TestGreed();
    std::cout << "PASS: " << iChecks << " native rule checks. Unreal runtime not covered.\n";
    return 0;
}
