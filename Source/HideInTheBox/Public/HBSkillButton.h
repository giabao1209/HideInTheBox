#pragma once

#include "CoreMinimal.h"
#include "CommonButtonBase.h"
#include "HBSkillButton.generated.h"

UCLASS()
class HIDEINTHEBOX_API UHBSkillButtonStyle : public UCommonButtonStyle
{
    GENERATED_BODY()
public:
    UHBSkillButtonStyle();
};

/** Native CommonUI button wrapper; content is supplied by the gameplay HUD. */
UCLASS()
class HIDEINTHEBOX_API UHBSkillButton : public UCommonButtonBase
{
    GENERATED_BODY()

public:
    void SetSkillContent(UWidget* Content);

protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeOnHovered() override;
    virtual void NativeOnUnhovered() override;

private:
    UPROPERTY(Transient) TObjectPtr<class USizeBox> ContentBox;
};
