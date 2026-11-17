#pragma once
#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "HBHUD.generated.h"

class UHBArenaDebugPanel;

UCLASS()
class HIDEINTHEBOX_API AHBHUD : public AHUD
{
    GENERATED_BODY()
public:
    virtual void DrawHUD() override;
    void ToggleDiagnostics();
    bool IsArenaTestPanelOpen() const;
    void CloseArenaTestPanel();
protected:
    UPROPERTY(EditDefaultsOnly, Category="Presentation") bool bShowDiagnostics = false;
private:
    UPROPERTY(Transient) TObjectPtr<UHBArenaDebugPanel> ArenaDebugPanel;
    int32 iDebugPage = 0;
    void Label(const FString& Text, float fX, float fY, const FLinearColor& Color, float fScale = 1.0f);
    void Panel(float fX, float fY, float fWidth, float fHeight, const FLinearColor& Accent);
};
