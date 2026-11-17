#pragma once

#include "CommonUserWidget.h"
#include "HBArenaDebugPanel.generated.h"

class UTextBlock;

UCLASS(Abstract, Blueprintable)
class HIDEINTHEBOX_API UHBArenaDebugPanel : public UCommonUserWidget
{
    GENERATED_BODY()

public:
    void Refresh();
    bool HasTestControls() const;

protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;

    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> DebugWorld;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> DebugPlayer;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> DebugTarget;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> DebugBuild;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> DebugEvents;

private:
    void ApplyTestValues();
    void GrantTestStones();
    void RefillTestVitals();
    void CloseTestPanel();
    void SetTestFeedback(const FString& Message);

    float RefreshElapsed = 0.0f;
};
