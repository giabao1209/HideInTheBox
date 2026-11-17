#pragma once
#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "InputActionValue.h"
#include "InputCoreTypes.h"
#include "TimerManager.h"
#include "GenericPlatform/GenericPlatformInputDeviceMapper.h"
#include "HBPlayerController.generated.h"
class UInputAction;
class UInputMappingContext;
class UEnhancedInputComponent;
class AHBIndicator;
class AHBGameMode;
class UUserWidget;

UCLASS(Config=Input)
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
    void SetArenaPaused(bool bPaused);
    void SetInventoryOpen(bool bOpen);
    void RestartArena();
    bool IsUsingGamepad() const { return this->bGamepad; }
    FString GetLastActionResult() const { return this->LastActionResult; }
    UUserWidget* GetPracticeHUD() const { return PracticeHUD; }
    UFUNCTION(BlueprintCallable, Category="Input") bool RebindSlotKey(int32 iSlot, FKey NewKey, bool bForGamepad);
    UFUNCTION(BlueprintCallable, Category="Input") bool SaveBindings();
    UFUNCTION(BlueprintPure, Category="Input") FKey GetSlotKey(int32 iSlot, bool bForGamepad) const;
    UFUNCTION(Exec) void HBDebug();
    UFUNCTION(Exec) void HBDebugClear();
    UFUNCTION(Exec) void HBPerfClear();
    UFUNCTION(Exec) void HBDebugFreeze();
    UFUNCTION(Exec) void HBDebugFilter(FName Category);
    UFUNCTION(Exec) void HBDebugDump();
    UFUNCTION(Exec) void HBDebugCheck();
    UFUNCTION(Exec) void HBDebugReset();
    UFUNCTION(Exec) void HBRebind(int32 SlotIndex, FName KeyName, bool bForGamepad);
    UFUNCTION(Exec) void HBAssign(int32 SlotIndex, int32 AbilityIndex);
    UFUNCTION(Exec) void HBCameraYaw(float Yaw);
    UFUNCTION(Exec) void HBGASReset();
    UFUNCTION(Exec) void HBGASDamage();
    UFUNCTION(Exec) void HBGASRefill();
protected:
    virtual void SetupInputComponent() override;
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    UPROPERTY(EditDefaultsOnly, Category="Feedback") TObjectPtr<class UMaterialInterface> RingMaterial;
private:
    void HandleUtility(const FInputActionValue& Value, int32 iAction);
    void HandleSlot(const FInputActionValue& Value, int32 iSlot);
    void HandleMove(const FInputActionValue& Value);
    void HandleKeyboardMove(const FInputActionValue& Value);
    void HandleAim(const FInputActionValue& Value);
    bool RefreshMouseBuildCell(AHBGameMode& Mode, bool bConfirm);
    void SetInputFamily(bool bFromGamepad);
    void RebuildSlotMapping();
    void LoadBindings();
    bool IsActionFromGamepad(const UInputMappingContext& Context, const UInputAction* Action) const;
    bool ValidateKeys(const TArray<FKey>& Keys, bool bForGamepad) const;
    bool IsReserved(const FKey& Key) const;
    bool IsNeutral() const;
    void CleanupMappingsAndBindings();
    void OnDeviceConnectionChanged(EInputDeviceConnectionState State, FPlatformUserId UserId, FInputDeviceId DeviceId);
    UPROPERTY(Config) TSoftObjectPtr<UInputMappingContext> GameplayContext;
    UPROPERTY(Config) TSoftObjectPtr<UInputMappingContext> SlotContext;
    UPROPERTY(Config) TArray<TSoftObjectPtr<UInputAction>> UtilityActionAssets;
    UPROPERTY(Config) TArray<TSoftObjectPtr<UInputAction>> SlotActionAssets;
    UPROPERTY(Config) TSoftObjectPtr<UInputAction> GamepadMoveAsset;
    UPROPERTY(Config) TSoftObjectPtr<UInputAction> KeyboardMoveAsset;
    UPROPERTY(Config) TSoftObjectPtr<UInputAction> GamepadAimAsset;
    UPROPERTY(Config) TSoftObjectPtr<UInputAction> AttackInPlaceAsset;
    UPROPERTY(Transient) TObjectPtr<UInputMappingContext> Mapping;
    UPROPERTY(Transient) TObjectPtr<UInputMappingContext> SlotMapping;
    UPROPERTY(Transient) TArray<TObjectPtr<UInputAction>> UtilityActions;
    UPROPERTY(Transient) TArray<TObjectPtr<UInputAction>> SlotActions;
    UPROPERTY(Transient) TObjectPtr<UInputAction> AttackInPlaceAction;
    UPROPERTY(Transient) TObjectPtr<AHBIndicator> BuildPreview;
    UPROPERTY(Transient) TObjectPtr<AHBIndicator> BuildGrid;
    UPROPERTY(Transient) TObjectPtr<AHBIndicator> BuildCursor;
    UPROPERTY(Transient) TObjectPtr<UUserWidget> PracticeHUD;
    TWeakObjectPtr<UEnhancedInputComponent> BoundInput;
    TArray<uint32> BindingHandles;
    TArray<FKey> KeyboardKeys;
    TArray<FKey> GamepadKeys;
    FTimerHandle DebugCheckTimer;
    FDelegateHandle ConnectionHandle;
    FVector2D MoveInput = FVector2D::ZeroVector;
    FVector2D KeyboardInput = FVector2D::ZeroVector;
    FVector2D AimInput = FVector2D::ZeroVector;
    bool bGamepad = false;
    bool bAwaitNeutral = false;
    bool bInventoryOpen = false;
    bool bBoardReady = false;
    bool bFenceCursor = false;
    bool bMouseCellValid = true;
    bool bManualBuildSelection = false;
    bool bGridStickHeld = false;
    FVector2D LastBuildMouse = FVector2D(-1.0, -1.0);
    FString LastActionResult = TEXT("Ready");
};
