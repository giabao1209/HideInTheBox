#pragma once
#include "CoreMinimal.h"
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
        const double fLength = Input.Size();
        if (fLength <= fThreshold)
        {
            return FVector2D::ZeroVector;
        }
        return Input / fLength * FMath::Clamp((fLength - fThreshold) / (1.0 - fThreshold), 0.0, 1.0);
    }

    inline int32 GetWaveEnemyCount(int32 iWave, int32 iThreat)
    {
        return FMath::Clamp(2 + iWave + iThreat / 3, 1, 16);
    }
}
