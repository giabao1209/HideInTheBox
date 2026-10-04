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
    void ToggleDiagnostics();
protected:
    UPROPERTY(EditDefaultsOnly, Category="Presentation") bool bShowDiagnostics = false;
private:
    int32 iDebugPage = 0;
    void Label(const FString& Text, float fX, float fY, const FLinearColor& Color, float fScale = 1.0f);
    void Panel(float fX, float fY, float fWidth, float fHeight, const FLinearColor& Accent);
};
