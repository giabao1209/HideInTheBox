#pragma once
#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "HBHUD.generated.h"

UCLASS()
class HIDEINTHEBOX_API AHBHUD : public AHUD
{
    GENERATED_BODY()
public:
    virtual void DrawHUD() override;
};
