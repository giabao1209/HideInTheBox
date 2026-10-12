#include "HBSkillButton.h"
#include "Blueprint/WidgetTree.h"
#include "Components/SizeBox.h"

UHBSkillButtonStyle::UHBSkillButtonStyle()
{
    // Let the card's CommonBorder draw its background instead of the default white Slate button.
    this->NormalBase.DrawAs = ESlateBrushDrawType::NoDrawType;
    this->NormalHovered.DrawAs = ESlateBrushDrawType::NoDrawType;
    this->NormalPressed.DrawAs = ESlateBrushDrawType::NoDrawType;
    this->SelectedBase.DrawAs = ESlateBrushDrawType::NoDrawType;
    this->SelectedHovered.DrawAs = ESlateBrushDrawType::NoDrawType;
    this->SelectedPressed.DrawAs = ESlateBrushDrawType::NoDrawType;
    this->Disabled.DrawAs = ESlateBrushDrawType::NoDrawType;
    this->ButtonPadding = FMargin(0.0f);
}

void UHBSkillButton::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    // Gameplay retains keyboard/gamepad skill shortcuts instead of moving focus into the HUD.
    this->SetIsFocusable(false);
    this->SetStyle(UHBSkillButtonStyle::StaticClass());
    this->ContentBox = this->WidgetTree->ConstructWidget<USizeBox>();
    this->WidgetTree->RootWidget = this->ContentBox;
}

void UHBSkillButton::NativeOnHovered()
{
    Super::NativeOnHovered();
    this->SetRenderScale(FVector2D(1.03f, 1.03f));
}

void UHBSkillButton::NativeOnUnhovered()
{
    Super::NativeOnUnhovered();
    this->SetRenderScale(FVector2D(1.0f, 1.0f));
}

void UHBSkillButton::SetSkillContent(UWidget* Content)
{
    if (this->ContentBox)
    {
        this->ContentBox->SetContent(Content);
    }
}
