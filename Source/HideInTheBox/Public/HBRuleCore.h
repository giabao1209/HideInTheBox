#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>

// Engine-independent rules, used by gameplay and the native regression executable.
namespace HBRuleCore
{
    constexpr int32_t iAbilityCount = 6;
    constexpr int32_t iSlotCount = 6;

    struct FStick
    {
        double fX = 0.0;
        double fY = 0.0;
    };

    inline FStick DeadZone(double fX, double fY, double fThreshold)
    {
        if (!std::isfinite(fX) || !std::isfinite(fY) || !std::isfinite(fThreshold))
        {
            return {};
        }
        const double fLength = std::hypot(fX, fY);
        const double fDeadZone = std::clamp(fThreshold, 0.0, 0.99);
        if (fLength <= fDeadZone || fLength == 0.0)
        {
            return {};
        }
        const double fScale = std::clamp((fLength - fDeadZone) / (1.0 - fDeadZone), 0.0, 1.0) / fLength;
        return {fX * fScale, fY * fScale};
    }

    inline int32_t WaveCount(int32_t iWave, int32_t iThreat)
    {
        const int64_t iTotal = 2LL + std::max(iWave, 0) + std::max(iThreat, 0) / 3;
        return static_cast<int32_t>(std::clamp<int64_t>(iTotal, 1, 16));
    }

    class FAbilityBudget
    {
    public:
        bool CanSpend(int32_t iAbility, double fMana, double fCost, double fNow) const
        {
            return iAbility >= 0 && iAbility < iAbilityCount && std::isfinite(fMana) && std::isfinite(fCost)
                && std::isfinite(fNow) && fCost >= 0.0 && fMana >= fCost && fNow >= this->ReadyAt[iAbility];
        }
        bool Commit(int32_t iAbility, double& OutMana, double fCost, double fCooldown, double fNow)
        {
            if (!this->CanSpend(iAbility, OutMana, fCost, fNow) || !std::isfinite(fCooldown) || fCooldown < 0.0)
            {
                return false;
            }
            OutMana -= fCost;
            this->ReadyAt[iAbility] = fNow + fCooldown;
            return true;
        }
        double Remaining(int32_t iAbility, double fNow) const
        {
            return iAbility >= 0 && iAbility < iAbilityCount ? std::max(0.0, this->ReadyAt[iAbility] - fNow) : 0.0;
        }
        void Reset()
        {
            this->ReadyAt.fill(0.0);
        }
    private:
        std::array<double, iAbilityCount> ReadyAt{};
    };

    class FGreedLedger
    {
    public:
        bool Claim(int32_t iChest, int32_t& OutStones, int32_t& OutThreat)
        {
            if (iChest < 0 || iChest >= 3 || this->Claimed[iChest] || OutStones < 0 || OutThreat < 0 || OutStones > std::numeric_limits<int32_t>::max() - 30 || OutThreat > std::numeric_limits<int32_t>::max() - 3)
            {
                return false;
            }
            this->Claimed[iChest] = true;
            OutStones += 30;
            OutThreat += 3;
            return true;
        }
        bool IsClaimed(int32_t iChest) const
        {
            return iChest < 0 || iChest >= 3 || this->Claimed[iChest];
        }
        void Reset()
        {
            this->Claimed.fill(false);
        }
    private:
        std::array<bool, 3> Claimed{};
    };
}
