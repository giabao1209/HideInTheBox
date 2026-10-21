#include "HBRuleCore.h"
#include "HBFrameStats.h"
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

static void TestFrameStats()
{
    HBPerformance::FFrameStats Stats;
    Expect(Stats.Num() == 0 && Stats.AverageFPS() == 0.0, "empty performance window is safe");
    Stats.Observe(10, 10.0, false);
    Expect(Stats.Num() == 0, "first observation establishes clock only");
    Stats.Observe(11, 10.01, false);
    Stats.Observe(12, 10.04, false);
    Expect(Stats.Num() == 2, "two consecutive frame intervals captured");
    Expect(std::abs(Stats.CurrentMilliseconds() - 30.0) < 1e-8, "latest frame time measured in milliseconds");
    Expect(std::abs(Stats.CurrentFPS() - 100.0 / 3.0) < 1e-8, "current FPS is reciprocal interval");
    Expect(std::abs(Stats.AverageFPS() - 50.0) < 1e-8, "average is count over elapsed, not mean instantaneous FPS");
    Expect(std::abs(Stats.AverageMilliseconds() - 20.0) < 1e-8, "average frame duration correct");
    Stats.Observe(12, 10.045, false);
    Expect(Stats.Num() == 2, "duplicate draw in same frame ignored");
    Stats.Observe(13, 10.05, false);
    Expect(std::abs(Stats.CurrentMilliseconds() - 10.0) < 1e-8, "duplicate draw cannot move baseline");
    Stats.Observe(14, 10.06, true);
    Stats.Observe(100, 1000.0, false);
    Expect(Stats.Num() == 3, "pause and first resume frame excluded");
    Stats.Observe(101, 1000.02, false);
    Expect(Stats.Num() == 4 && Stats.WorstMilliseconds() < 31.0, "pause does not create a fake long frame");
    Stats.Observe(110, 1002.0, false);
    Expect(Stats.Num() == 4, "missing HUD frames rebase without false hitch");
    Stats.Observe(111, 1002.5, false);
    Expect(std::abs(Stats.WorstMilliseconds() - 500.0) < 1e-8, "real consecutive-frame hitch retained without clamp");
    Stats.Observe(112, std::numeric_limits<double>::quiet_NaN(), false);
    Stats.Observe(113, 1003.0, false);
    Expect(Stats.Num() == 5, "invalid clock does not contaminate totals");
    Stats.Observe(114, 1002.0, false);
    Expect(Stats.Num() == 5, "backwards clock interval rejected");
    Stats.Reset();
    Expect(Stats.Num() == 0 && Stats.WorstMilliseconds() == 0.0 && Stats.CurrentFPS() == 0.0, "reset clears all statistics");
    for (std::uint64_t Frame = 0; Frame <= 300; ++Frame)
    {
        Stats.Observe(Frame, Frame * 0.01, false);
    }
    Expect(Stats.Num() == 240, "sample ring remains bounded after wrap");
    Expect(std::abs(Stats.AverageFPS() - 100.0) < 1e-8, "running total stays correct after eviction");
    Stats.Observe(301, 3.2, false);
    Expect(std::abs(Stats.SecondsAt(239) - 0.2) < 1e-8 && std::abs(Stats.SecondsAt(0) - 0.01) < 1e-8, "graph is ordered oldest to newest after ring wrap");
    Expect(std::abs(Stats.AverageFPS() - 240.0 / 2.59) < 1e-8, "evicted sample removed from FPS denominator");
    Expect(Stats.SecondsAt(240) == 0.0, "out of range graph access safe");
}

int main()
{
    TestAnalog();
    TestResources();
    TestGreed();
    TestFrameStats();
    std::cout << "PASS: " << iChecks << " native gameplay/performance checks. Unreal runtime not covered.\n";
    return 0;
}
