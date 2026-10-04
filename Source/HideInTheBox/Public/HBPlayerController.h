#pragma once
#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "InputActionValue.h"
#include "InputCoreTypes.h"
#include "GenericPlatform/GenericPlatformInputDeviceMapper.h"
#include "HBPlayerController.generated.h"
class UInputAction;
class UInputMappingContext;
class UEnhancedInputComponent;
class AHBIndicator;

UCLASS()
class HIDEINTHEBOX_API AHBPlayerController : public APlayerController
{
    GENERATED_BODY()
public:
    AHBPlayerController();
    virtual void PlayerTick(float fDeltaSeconds) override;
    virtual void FlushPressedKeys() override;
    virtual void OnPossess(APawn* InPawn) override;
    virtual void OnUnPossess() override;
    void ResetTransientInput();
    bool IsUsingGamepad() const { return this->bGamepad; }
    FString GetLastActionResult() const { return this->LastActionResult; }
    UFUNCTION(BlueprintCallable, Category="Input") bool RebindSlotKey(int32 iSlot, FKey NewKey, bool bForGamepad);
    UFUNCTION(BlueprintCallable, Category="Input") bool SaveBindings();
    UFUNCTION(BlueprintPure, Category="Input") FKey GetSlotKey(int32 iSlot, bool bForGamepad) const;
    UFUNCTION(Exec) void HBRebind(int32 SlotIndex, FName KeyName, bool bForGamepad);
    UFUNCTION(Exec) void HBAssign(int32 SlotIndex, int32 AbilityIndex);
    UFUNCTION(Exec) void HBCameraYaw(float Yaw);
protected:
    virtual void SetupInputComponent() override;
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    UPROPERTY(EditDefaultsOnly, Category="Feedback") TObjectPtr<class UMaterialInterface> RingMaterial;
    UPROPERTY(EditDefaultsOnly, Category="Input") TArray<FKey> KeyboardKeys;
    UPROPERTY(EditDefaultsOnly, Category="Input") TArray<FKey> GamepadKeys;
private:
    void HandleUtility(const FInputActionValue& Value, int32 iAction, bool bFromGamepad);
    void HandleSlot(const FInputActionValue& Value, int32 iSlot, bool bFromGamepad);
    void HandleMove(const FInputActionValue& Value);
    void HandleKeyboardMove(const FInputActionValue& Value);
    void HandleAim(const FInputActionValue& Value);
    void SetInputFamily(bool bFromGamepad);
    void RebuildSlotMapping();
    void LoadBindings();
    bool ValidateKeys(const TArray<FKey>& Keys, bool bForGamepad) const;
    bool IsReserved(const FKey& Key) const;
    bool IsNeutral() const;
    void CleanupMappingsAndBindings();
    void OnDeviceConnectionChanged(EInputDeviceConnectionState State, FPlatformUserId UserId, FInputDeviceId DeviceId);
    UPROPERTY(Transient) TObjectPtr<UInputMappingContext> Mapping;
    UPROPERTY(Transient) TObjectPtr<UInputMappingContext> SlotMapping;
    UPROPERTY(Transient) TArray<TObjectPtr<UInputAction>> UtilityActions;
    UPROPERTY(Transient) TArray<TObjectPtr<UInputAction>> SlotActions;
    UPROPERTY(Transient) TObjectPtr<UInputAction> MoveAction;
    UPROPERTY(Transient) TObjectPtr<UInputAction> KeyboardMoveAction;
    UPROPERTY(Transient) TObjectPtr<UInputAction> AimAction;
    UPROPERTY(Transient) TObjectPtr<AHBIndicator> BuildPreview;
    TWeakObjectPtr<UEnhancedInputComponent> BoundInput;
    TArray<uint32> BindingHandles;
    FDelegateHandle ConnectionHandle;
    FVector2D MoveInput = FVector2D::ZeroVector;
    FVector2D KeyboardInput = FVector2D::ZeroVector;
    FVector2D AimInput = FVector2D::ZeroVector;
    bool bGamepad = false;
    bool bAwaitNeutral = false;
    FString LastActionResult = TEXT("Ready");
};
