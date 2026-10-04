#pragma once
#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "InputActionValue.h"
#include "GenericPlatform/GenericPlatformInputDeviceMapper.h"
#include "HBPlayerController.generated.h"
class UInputAction;
class UInputMappingContext;
class AHBIndicator;

UCLASS()
class HIDEINTHEBOX_API AHBPlayerController : public APlayerController
{
    GENERATED_BODY()
public:
    AHBPlayerController();
    virtual void PlayerTick(float fDeltaSeconds) override;
    virtual void FlushPressedKeys() override;
    void ResetTransientInput();
    bool IsUsingGamepad() const { return this->bGamepad; }
protected:
    virtual void SetupInputComponent() override;
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
private:
    void HandleAction(const FInputActionValue& Value, int32 iAction);
    void HandleMove(const FInputActionValue& Value);
    void HandleAim(const FInputActionValue& Value);
    void ContextClick();
    void PadAttack();
    void OnDeviceConnectionChanged(EInputDeviceConnectionState State, FPlatformUserId UserId, FInputDeviceId DeviceId);
    FDelegateHandle ConnectionHandle;
    bool IsNeutral() const;
    UPROPERTY(Transient) TObjectPtr<UInputMappingContext> Mapping;
    UPROPERTY(Transient) TArray<TObjectPtr<UInputAction>> Actions;
    UPROPERTY(Transient) TObjectPtr<UInputAction> MoveAction;
    UPROPERTY(Transient) TObjectPtr<UInputAction> AimAction;
    UPROPERTY(Transient) TObjectPtr<AHBIndicator> BuildPreview;
    FVector2D MoveInput = FVector2D::ZeroVector;
    FVector2D AimInput = FVector2D::ZeroVector;
    bool bGamepad = false;
    bool bAwaitNeutral = false;
};
