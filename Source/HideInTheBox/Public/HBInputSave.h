#pragma once
#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "InputCoreTypes.h"
#include "HBInputSave.generated.h"

UCLASS()
class HIDEINTHEBOX_API UHBInputSave : public USaveGame
{
    GENERATED_BODY()
public:
    UPROPERTY(SaveGame) int32 iVersion = 1;
    UPROPERTY(SaveGame) TArray<FKey> KeyboardKeys;
    UPROPERTY(SaveGame) TArray<FKey> GamepadKeys;
};
