#pragma once
#include "CoreMinimal.h"
#include "HBRuleCore.h"
#include "HBTypes.generated.h"

UENUM(BlueprintType)
enum class EHBPhase : uint8
{
    EHBP_Town, EHBP_Dungeon, EHBP_Return, EHBP_Defense, EHBP_GameOver
};

UENUM(BlueprintType)
enum class EHBCommand : uint8
{
    EHBC_None, EHBC_Move, EHBC_Attack
};

UENUM(BlueprintType)
enum class EHBStructure : uint8
{
    EHBS_Gate, EHBS_Anima, EHBS_Turret
};

namespace HBRules
{
inline FVector2D ApplyRadialDeadZone(const FVector2D& Input, float fThreshold)
    {
        const HBRuleCore::FStick Result = HBRuleCore::DeadZone(Input.X, Input.Y, fThreshold);
        return FVector2D(Result.fX, Result.fY);
    }

inline int32 GetWaveEnemyCount(int32 iWave, int32 iThreat)
    {
        return HBRuleCore::WaveCount(iWave, iThreat);
    }
}
