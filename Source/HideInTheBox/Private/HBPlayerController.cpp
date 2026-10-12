#include "HBPlayerController.h"
#include "HBDebugSubsystem.h"
#include "HBPlayerCharacter.h"
#include "HBEnemyCharacter.h"
#include "HBGameMode.h"
#include "HBIndicator.h"
#include "HBInputSave.h"
#include "HBLoadout.h"
#include "HBHUD.h"
#include "HBCommonGameplayHUDWidget.h"
#include "HideInTheBox.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "EnhancedActionKeyMapping.h"
#include "InputModifiers.h"
#include "InputAction.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

namespace HBInput
{
    enum class EUtility : int32
    {
        EU_Advance, EU_Build, EU_Cycle, EU_Repair, EU_Cancel, EU_Pause, EU_Restart, EU_Interact, EU_Diagnostics, EU_Count
    };
    const FKey KeyboardUtilities[] = { EKeys::Enter, EKeys::B, EKeys::Tab, EKeys::R, EKeys::SpaceBar, EKeys::P, EKeys::F5, EKeys::E, EKeys::F2 };
    const FKey GamepadUtilities[] = { EKeys::Gamepad_DPad_Up, EKeys::Gamepad_DPad_Down, EKeys::Gamepad_DPad_Right, EKeys::Gamepad_DPad_Left,
        EKeys::Gamepad_RightThumbstick, EKeys::Gamepad_Special_Right, EKeys::Gamepad_Special_Left, EKeys::Gamepad_LeftThumbstick, EKeys::Gamepad_LeftTrigger };
    const TCHAR* SaveSlot = TEXT("HideInTheBox_Input_v1");
}

AHBPlayerController::AHBPlayerController()
{
    this->bShowMouseCursor = true;
    this->DefaultMouseCursor = EMouseCursor::Crosshairs;
    this->KeyboardKeys = { EKeys::LeftMouseButton, EKeys::RightMouseButton, EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four };
    this->GamepadKeys = { EKeys::Gamepad_FaceButton_Bottom, EKeys::Gamepad_FaceButton_Right, EKeys::Gamepad_FaceButton_Left,
        EKeys::Gamepad_FaceButton_Top, EKeys::Gamepad_LeftShoulder, EKeys::Gamepad_RightShoulder };
}

void AHBPlayerController::CleanupMappingsAndBindings()
{
    UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(this->GetLocalPlayer());
    if (Subsystem)
    {
        if (this->Mapping)
        {
            Subsystem->RemoveMappingContext(this->Mapping);
        }
        if (this->SlotMapping)
        {
            Subsystem->RemoveMappingContext(this->SlotMapping);
        }
    }
    if (this->BoundInput.IsValid())
    {
        for (uint32 iHandle : this->BindingHandles)
        {
            this->BoundInput->RemoveBindingByHandle(iHandle);
        }
    }
    this->BindingHandles.Reset();
    this->UtilityActions.Reset();
    this->SlotActions.Reset();
}

void AHBPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();
    this->CleanupMappingsAndBindings();
    UEnhancedInputComponent* Enhanced = Cast<UEnhancedInputComponent>(this->InputComponent);
    if (!Enhanced)
    {
        UE_LOG(LogHideInTheBox, Error, TEXT("EnhancedInputComponent missing; inspect DefaultInput.ini"));
        return;
    }
    this->BoundInput = Enhanced;
    this->LoadBindings();
    this->Mapping = NewObject<UInputMappingContext>(this);
    this->SlotMapping = NewObject<UInputMappingContext>(this);
    for (int32 iFamily = 0; iFamily < 2; ++iFamily)
    {
        const bool bPad = iFamily == 1;
        for (int32 iIndex = 0; iIndex < static_cast<int32>(HBInput::EUtility::EU_Count); ++iIndex)
        {
            UInputAction* Action = NewObject<UInputAction>(this);
            Action->ValueType = EInputActionValueType::Boolean;
            Action->bTriggerWhenPaused = iIndex == static_cast<int32>(HBInput::EUtility::EU_Pause) || iIndex == static_cast<int32>(HBInput::EUtility::EU_Diagnostics);
            this->UtilityActions.Add(Action);
            this->Mapping->MapKey(Action, bPad ? HBInput::GamepadUtilities[iIndex] : HBInput::KeyboardUtilities[iIndex]);
            this->BindingHandles.Add(Enhanced->BindAction(Action, ETriggerEvent::Started, this, &AHBPlayerController::HandleUtility, iIndex, bPad).GetHandle());
        }
        for (int32 iSlot = 0; iSlot < HBRuleCore::iSlotCount; ++iSlot)
        {
            UInputAction* Action = NewObject<UInputAction>(this);
            Action->ValueType = EInputActionValueType::Boolean;
            this->SlotActions.Add(Action);
            this->BindingHandles.Add(Enhanced->BindAction(Action, ETriggerEvent::Started, this, &AHBPlayerController::HandleSlot, iSlot, bPad).GetHandle());
        }
    }
    this->MoveAction = NewObject<UInputAction>(this);
    this->MoveAction->ValueType = EInputActionValueType::Axis2D;
    this->Mapping->MapKey(this->MoveAction, EKeys::Gamepad_Left2D);
    this->AimAction = NewObject<UInputAction>(this);
    this->AimAction->ValueType = EInputActionValueType::Axis2D;
    this->Mapping->MapKey(this->AimAction, EKeys::Gamepad_Right2D);
    this->KeyboardMoveAction = NewObject<UInputAction>(this);
    this->KeyboardMoveAction->ValueType = EInputActionValueType::Axis2D;
    this->KeyboardMoveAction->AccumulationBehavior = EInputActionAccumulationBehavior::Cumulative;
    const FKey MovementKeys[] = { EKeys::D, EKeys::A, EKeys::W, EKeys::S };
    for (int32 iIndex = 0; iIndex < 4; ++iIndex)
    {
        FEnhancedActionKeyMapping& KeyMapping = this->Mapping->MapKey(this->KeyboardMoveAction, MovementKeys[iIndex]);
        if (iIndex == 1 || iIndex == 3)
        {
            KeyMapping.Modifiers.Add(NewObject<UInputModifierNegate>(this->Mapping));
        }
        if (iIndex >= 2)
        {
            UInputModifierSwizzleAxis* Swizzle = NewObject<UInputModifierSwizzleAxis>(this->Mapping);
            Swizzle->Order = EInputAxisSwizzle::YXZ;
            KeyMapping.Modifiers.Add(Swizzle);
        }
    }
    const ETriggerEvent Events[] = { ETriggerEvent::Triggered, ETriggerEvent::Completed, ETriggerEvent::Canceled };
    for (ETriggerEvent Event : Events)
    {
        this->BindingHandles.Add(Enhanced->BindAction(this->MoveAction, Event, this, &AHBPlayerController::HandleMove).GetHandle());
        this->BindingHandles.Add(Enhanced->BindAction(this->KeyboardMoveAction, Event, this, &AHBPlayerController::HandleKeyboardMove).GetHandle());
        this->BindingHandles.Add(Enhanced->BindAction(this->AimAction, Event, this, &AHBPlayerController::HandleAim).GetHandle());
    }
    UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(this->GetLocalPlayer());
    if (Subsystem)
    {
        FModifyContextOptions Options;
        Options.bIgnoreAllPressedKeysUntilRelease = true;
        Subsystem->AddMappingContext(this->Mapping, 0, Options);
    }
    this->RebuildSlotMapping();
}

void AHBPlayerController::BeginPlay()
{
    Super::BeginPlay();
#if !UE_BUILD_SHIPPING
    this->GetWorldTimerManager().SetTimer(this->DebugCheckTimer, this, &AHBPlayerController::HBDebugCheck, 0.2f, false);
#endif
    this->ConnectionHandle = IPlatformInputDeviceMapper::Get().GetOnInputDeviceConnectionChange().AddUObject(this, &AHBPlayerController::OnDeviceConnectionChanged);
    FInputModeGameAndUI Mode;
    Mode.SetHideCursorDuringCapture(false);
    this->SetInputMode(Mode);
    this->GameplayHUD = CreateWidget<UHBCommonGameplayHUDWidget>(this, UHBCommonGameplayHUDWidget::StaticClass());
    if (this->GameplayHUD)
    {
        this->GameplayHUD->AddToViewport(10);
        this->GameplayHUD->RefreshFromGame();
        this->GetWorldTimerManager().SetTimer(this->GameplayHUDRefreshTimer, this, &AHBPlayerController::RefreshGameplayHUD, 0.1f, true);
    }
    FActorSpawnParameters Parameters;
    Parameters.Owner = this;
    this->BuildPreview = this->GetWorld()->SpawnActor<AHBIndicator>(AHBIndicator::StaticClass(), FTransform::Identity, Parameters);
    if (this->BuildPreview && this->RingMaterial)
    {
        this->BuildPreview->SetRingMaterial(this->RingMaterial);
    }
}

void AHBPlayerController::SetInputFamily(bool bFromGamepad)
{
    if (this->bGamepad != bFromGamepad)
    {
        // Switching devices must not cancel an active swing or its hit.
        AHBPlayerCharacter* PlayerCharacter = Cast<AHBPlayerCharacter>(this->GetPawn());
        if (PlayerCharacter && !PlayerCharacter->IsAttacking())
        {
            PlayerCharacter->CancelCommands();
        }
        this->bGamepad = bFromGamepad;
    }
}

void AHBPlayerController::RefreshGameplayHUD()
{
    if (this->GameplayHUD)
    {
        this->GameplayHUD->RefreshFromGame();
    }
}

void AHBPlayerController::HandleMove(const FInputActionValue& Value)
{
    this->MoveInput = HBRules::ApplyRadialDeadZone(Value.Get<FVector2D>(), 0.2f);
    if (!this->MoveInput.IsNearlyZero())
    {
        this->SetInputFamily(true);
    }
}

void AHBPlayerController::HandleKeyboardMove(const FInputActionValue& Value)
{
    this->KeyboardInput = Value.Get<FVector2D>().GetClampedToMaxSize(1.0);
    if (!this->KeyboardInput.IsNearlyZero())
    {
        this->SetInputFamily(false);
    }
}

void AHBPlayerController::HandleAim(const FInputActionValue& Value)
{
    this->AimInput = HBRules::ApplyRadialDeadZone(Value.Get<FVector2D>(), 0.2f);
    if (!this->AimInput.IsNearlyZero())
    {
        this->SetInputFamily(true);
    }
}

void AHBPlayerController::HandleSlot(const FInputActionValue& Value, int32 iSlot, bool bFromGamepad)
{
    this->ActivateSkillSlot(iSlot, bFromGamepad, false);
}

void AHBPlayerController::UseSkillFromUI(int32 SlotIndex)
{
    this->ActivateSkillSlot(SlotIndex, false, true);
}

void AHBPlayerController::ActivateSkillSlot(int32 iSlot, bool bFromGamepad, bool bFromUI)
{
    if (iSlot < 0 || iSlot >= HBRuleCore::iSlotCount)
    {
        return;
    }
    UHBDebugSubsystem::Record(this, TEXT("Input"), FString::Printf(TEXT("Slot %d from %s paused=%d neutralWait=%d"), iSlot, bFromGamepad ? TEXT("Gamepad") : TEXT("Mouse/KB"), this->IsPaused(), this->bAwaitNeutral));
    if (this->IsPaused() || this->bAwaitNeutral || (!bFromGamepad && !bFromUI && this->GameplayHUD && this->GameplayHUD->IsPointerOverSkillBar() && this->GetSlotKey(iSlot, false).IsMouseButton()))
    {
        return;
    }
    AHBPlayerCharacter* PlayerCharacter = Cast<AHBPlayerCharacter>(this->GetPawn());
    AHBGameMode* Mode = this->GetWorld()->GetAuthGameMode<AHBGameMode>();
    if (!PlayerCharacter || !Mode || !PlayerCharacter->IsAlive() || Mode->GetPhase() == EHBPhase::EHBP_GameOver)
    {
        return;
    }
    this->SetInputFamily(bFromGamepad);
    if (PlayerCharacter->IsBuilding())
    {
        if (iSlot == 0 && !bFromUI)
        {
            Mode->ConfirmBuild();
        }
        return;
    }
    UHBLoadoutComponent* Loadout = PlayerCharacter->GetLoadout();
    const EHBAbility Ability = Loadout->GetSlotAbility(iSlot);
    AHBEnemyCharacter* Target = nullptr;
    FVector AimPoint = PlayerCharacter->GetActorLocation();
    bool bInPlace = false;
    // HUD cards cast toward the last world aim, not the floor behind the button.
    if (bFromGamepad || bFromUI || (this->GameplayHUD && this->GameplayHUD->IsPointerOverSkillBar()))
    {
        const FVector Aim = PlayerCharacter->GetAimDirection();
        Target = Mode->FindEnemy(PlayerCharacter->GetActorLocation(), Aim, Loadout->GetSlotSpec(iSlot).fRange + 500.0f);
        AimPoint += Aim * FMath::Min(300.0f, Loadout->GetSlotSpec(iSlot).fRange);
    }
    else
    {
        FHitResult Hit;
        if (this->GetHitResultUnderCursor(ECC_Visibility, false, Hit))
        {
            Target = Cast<AHBEnemyCharacter>(Hit.GetActor());
        }
        const bool bFloor = this->GetHitResultUnderCursor(ECC_GameTraceChannel1, false, Hit);
        if (bFloor)
        {
            AimPoint = Hit.ImpactPoint;
        }
        bInPlace = this->IsInputKeyDown(EKeys::LeftShift) || this->IsInputKeyDown(EKeys::RightShift);
        if (iSlot == 0 && Ability == EHBAbility::EHBA_Basic && !Target)
        {
            if (bFloor && !bInPlace)
            {
                PlayerCharacter->RequestMove(AimPoint);
            }
            return;
        }
        if (!bFloor && Ability == EHBAbility::EHBA_Pulse)
        {
            this->LastActionResult = TEXT("No valid floor aim point");
            return;
        }
    }
    const EHBActionResult Result = Loadout->TryActivateSlot(iSlot, Target, AimPoint, bInPlace);
    FString ResultLabel;
    switch (Result)
    {
        case EHBActionResult::EHBAR_Accepted: ResultLabel = TEXT("Used"); break;
        case EHBActionResult::EHBAR_Cooldown: ResultLabel = TEXT("On cooldown"); break;
        case EHBActionResult::EHBAR_NoMana: ResultLabel = TEXT("Not enough mana"); break;
        case EHBActionResult::EHBAR_InvalidTarget: ResultLabel = Ability == EHBAbility::EHBA_Mend ? TEXT("Health already full") : TEXT("Needs a valid target in range"); break;
        default: ResultLabel = TEXT("Cannot use now"); break;
    }
    this->LastActionResult = FString::Printf(TEXT("%s: %s"), *Loadout->GetSlotSpec(iSlot).Name.ToString(), *ResultLabel);
    this->RefreshGameplayHUD();
}

void AHBPlayerController::HandleUtility(const FInputActionValue& Value, int32 iAction, bool bFromGamepad)
{
    const HBInput::EUtility Action = static_cast<HBInput::EUtility>(iAction);
    if (Action == HBInput::EUtility::EU_Diagnostics)
    {
        this->HBDebug();
        return;
    }
    const TCHAR* UtilityNames[] = { TEXT("Advance"), TEXT("Build"), TEXT("Socket"), TEXT("Repair"), TEXT("Cancel"), TEXT("Pause"), TEXT("Restart"), TEXT("Interact"), TEXT("Diagnostics") };
    UHBDebugSubsystem::Record(this, TEXT("Input"), FString::Printf(TEXT("Utility %s from %s"), iAction >= 0 && iAction < 9 ? UtilityNames[iAction] : TEXT("Unknown"), bFromGamepad ? TEXT("Gamepad") : TEXT("Mouse/KB")));
    if (Action == HBInput::EUtility::EU_Pause)
    {
        const bool bPaused = this->IsPaused();
        this->ResetTransientInput();
        this->SetPause(!bPaused);
        this->RefreshGameplayHUD();
        return;
    }
    if (this->bAwaitNeutral || this->IsPaused())
    {
        return;
    }
    this->SetInputFamily(bFromGamepad);
    AHBPlayerCharacter* PlayerCharacter = Cast<AHBPlayerCharacter>(this->GetPawn());
    AHBGameMode* Mode = this->GetWorld()->GetAuthGameMode<AHBGameMode>();
    if (!PlayerCharacter || !Mode)
    {
        return;
    }
    if (Action == HBInput::EUtility::EU_Restart)
    {
        UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this, true)));
        return;
    }
    if (!PlayerCharacter->IsAlive() || Mode->GetPhase() == EHBPhase::EHBP_GameOver)
    {
        return;
    }
    switch (Action)
    {
        case HBInput::EUtility::EU_Advance: Mode->AdvancePhase(); break;
        case HBInput::EUtility::EU_Build: Mode->ToggleBuildMode(); break;
        case HBInput::EUtility::EU_Cycle: Mode->CycleBuildSocket(); break;
        case HBInput::EUtility::EU_Repair: Mode->RepairGate(); break;
        case HBInput::EUtility::EU_Interact: Mode->ClaimNearbyGreed(); break;
        case HBInput::EUtility::EU_Cancel: PlayerCharacter->SetBuildMode(false); this->ResetTransientInput(); break;
        default: break;
    }
}

bool AHBPlayerController::IsNeutral() const
{
    for (const FKey& Key : this->KeyboardKeys)
    {
        if (this->IsInputKeyDown(Key))
        {
            return false;
        }
    }
    for (const FKey& Key : this->GamepadKeys)
    {
        if (this->IsInputKeyDown(Key))
        {
            return false;
        }
    }
    for (const FKey& Key : HBInput::KeyboardUtilities)
    {
        if (this->IsInputKeyDown(Key))
        {
            return false;
        }
    }
    for (const FKey& Key : HBInput::GamepadUtilities)
    {
        if (this->IsInputKeyDown(Key))
        {
            return false;
        }
    }
    return this->MoveInput.IsNearlyZero() && this->AimInput.IsNearlyZero() && this->KeyboardInput.IsNearlyZero();
}

void AHBPlayerController::PlayerTick(float fDeltaSeconds)
{
    Super::PlayerTick(fDeltaSeconds);
    AHBPlayerCharacter* PlayerCharacter = Cast<AHBPlayerCharacter>(this->GetPawn());
    AHBGameMode* Mode = this->GetWorld()->GetAuthGameMode<AHBGameMode>();
    if (!PlayerCharacter || !Mode)
    {
        if (this->BuildPreview)
        {
            this->BuildPreview->SetActorHiddenInGame(true);
        }
        return;
    }
    if (this->bAwaitNeutral)
    {
        this->bAwaitNeutral = !this->IsNeutral();
        return;
    }
    if (this->IsPaused() || Mode->GetPhase() == EHBPhase::EHBP_GameOver || !PlayerCharacter->IsAlive())
    {
        return;
    }
    PlayerCharacter->SetDirectInput(this->bGamepad ? this->MoveInput : this->KeyboardInput);
    AHBEnemyCharacter* Hover = nullptr;
    if (this->bGamepad)
    {
        PlayerCharacter->SetAimInput(this->AimInput);
    }
    else if (!this->GameplayHUD || !this->GameplayHUD->IsPointerOverSkillBar())
    {
        FHitResult Hit;
        if (this->GetHitResultUnderCursor(ECC_Visibility, false, Hit))
        {
            Hover = Cast<AHBEnemyCharacter>(Hit.GetActor());
        }
        if (this->GetHitResultUnderCursor(ECC_GameTraceChannel1, false, Hit))
        {
            PlayerCharacter->SetAimWorldDirection(Hit.ImpactPoint - PlayerCharacter->GetActorLocation());
        }
    }
    PlayerCharacter->SetHover(Hover);
    if (this->BuildPreview)
    {
        if (PlayerCharacter->IsBuilding())
        {
            FVector Position = Mode->GetBuildSocket();
            Position.Z = 0.0f;
            this->BuildPreview->ShowAt(Position, 70.0f, 2.0f, Mode->IsBuildSocketOccupied() ? FLinearColor::Red : FLinearColor::Green);
        }
        else
        {
            this->BuildPreview->SetActorHiddenInGame(true);
        }
    }
}

void AHBPlayerController::ResetTransientInput()
{
    UHBDebugSubsystem::Record(this, TEXT("Input"), TEXT("Transient input reset; waiting for neutral"));
    this->MoveInput = FVector2D::ZeroVector;
    this->KeyboardInput = FVector2D::ZeroVector;
    this->AimInput = FVector2D::ZeroVector;
    this->bAwaitNeutral = true;
    AHBPlayerCharacter* PlayerCharacter = Cast<AHBPlayerCharacter>(this->GetPawn());
    if (PlayerCharacter)
    {
        PlayerCharacter->CancelCommands();
    }
    if (this->BuildPreview)
    {
        this->BuildPreview->SetActorHiddenInGame(true);
    }
}

void AHBPlayerController::FlushPressedKeys()
{
    Super::FlushPressedKeys();
    this->ResetTransientInput();
}

void AHBPlayerController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);
    this->ResetTransientInput();
}

void AHBPlayerController::OnUnPossess()
{
    this->ResetTransientInput();
    Super::OnUnPossess();
}

void AHBPlayerController::OnDeviceConnectionChanged(EInputDeviceConnectionState State, FPlatformUserId UserId, FInputDeviceId DeviceId)
{
    if (State == EInputDeviceConnectionState::Disconnected && this->bGamepad)
    {
        this->FlushPressedKeys();
        this->SetPause(true);
        this->RefreshGameplayHUD();
    }
}

bool AHBPlayerController::IsReserved(const FKey& Key) const
{
    const FKey Reserved[] = { EKeys::W, EKeys::A, EKeys::S, EKeys::D, EKeys::LeftShift, EKeys::RightShift, EKeys::Escape, EKeys::Tilde };
    for (const FKey& Entry : Reserved)
    {
        if (Entry == Key)
        {
            return true;
        }
    }
    for (const FKey& Entry : HBInput::KeyboardUtilities)
    {
        if (Entry == Key)
        {
            return true;
        }
    }
    for (const FKey& Entry : HBInput::GamepadUtilities)
    {
        if (Entry == Key)
        {
            return true;
        }
    }
    return false;
}

bool AHBPlayerController::ValidateKeys(const TArray<FKey>& Keys, bool bForGamepad) const
{
    if (Keys.Num() != HBRuleCore::iSlotCount)
    {
        return false;
    }
    TSet<FKey> Used;
    for (const FKey& Key : Keys)
    {
        if (!Key.IsValid() || Key.IsAnalog() || Key.IsGamepadKey() != bForGamepad || this->IsReserved(Key) || Used.Contains(Key))
        {
            return false;
        }
        Used.Add(Key);
    }
    return true;
}

void AHBPlayerController::LoadBindings()
{
    UHBInputSave* Saved = Cast<UHBInputSave>(UGameplayStatics::LoadGameFromSlot(HBInput::SaveSlot, 0));
    if (Saved && Saved->iVersion == 1 && this->ValidateKeys(Saved->KeyboardKeys, false) && this->ValidateKeys(Saved->GamepadKeys, true))
    {
        this->KeyboardKeys = Saved->KeyboardKeys;
        this->GamepadKeys = Saved->GamepadKeys;
    }
    if (!this->ValidateKeys(this->KeyboardKeys, false))
    {
        this->KeyboardKeys = { EKeys::LeftMouseButton, EKeys::RightMouseButton, EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four };
    }
    if (!this->ValidateKeys(this->GamepadKeys, true))
    {
        this->GamepadKeys = { EKeys::Gamepad_FaceButton_Bottom, EKeys::Gamepad_FaceButton_Right, EKeys::Gamepad_FaceButton_Left,
            EKeys::Gamepad_FaceButton_Top, EKeys::Gamepad_LeftShoulder, EKeys::Gamepad_RightShoulder };
    }
}

void AHBPlayerController::RebuildSlotMapping()
{
    UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(this->GetLocalPlayer());
    if (!Subsystem || !this->SlotMapping || this->SlotActions.Num() != 12)
    {
        return;
    }
    Subsystem->RemoveMappingContext(this->SlotMapping);
    this->SlotMapping->UnmapAll();
    for (int32 iSlot = 0; iSlot < HBRuleCore::iSlotCount; ++iSlot)
    {
        this->SlotMapping->MapKey(this->SlotActions[iSlot], this->KeyboardKeys[iSlot]);
        this->SlotMapping->MapKey(this->SlotActions[iSlot + HBRuleCore::iSlotCount], this->GamepadKeys[iSlot]);
    }
    FModifyContextOptions Options;
    Options.bIgnoreAllPressedKeysUntilRelease = true;
    Subsystem->AddMappingContext(this->SlotMapping, 1, Options);
    this->ResetTransientInput();
}

bool AHBPlayerController::RebindSlotKey(int32 iSlot, FKey NewKey, bool bForGamepad)
{
    TArray<FKey>& Keys = bForGamepad ? this->GamepadKeys : this->KeyboardKeys;
    if (!Keys.IsValidIndex(iSlot))
    {
        return false;
    }
    TArray<FKey> Candidate = Keys;
    Candidate[iSlot] = NewKey;
    if (!this->ValidateKeys(Candidate, bForGamepad))
    {
        return false;
    }
    Keys = MoveTemp(Candidate);
    this->RebuildSlotMapping();
    return true;
}

bool AHBPlayerController::SaveBindings()
{
    UHBInputSave* Saved = Cast<UHBInputSave>(UGameplayStatics::CreateSaveGameObject(UHBInputSave::StaticClass()));
    if (!Saved)
    {
        return false;
    }
    Saved->KeyboardKeys = this->KeyboardKeys;
    Saved->GamepadKeys = this->GamepadKeys;
    return UGameplayStatics::SaveGameToSlot(Saved, HBInput::SaveSlot, 0);
}

FKey AHBPlayerController::GetSlotKey(int32 iSlot, bool bForGamepad) const
{
    const TArray<FKey>& Keys = bForGamepad ? this->GamepadKeys : this->KeyboardKeys;
    return Keys.IsValidIndex(iSlot) ? Keys[iSlot] : FKey();
}

void AHBPlayerController::HBRebind(int32 SlotIndex, FName KeyName, bool bForGamepad)
{
    const bool bChanged = this->RebindSlotKey(SlotIndex, FKey(KeyName), bForGamepad);
    const bool bSaved = bChanged && this->SaveBindings();
    this->LastActionResult = FString::Printf(TEXT("Rebind accepted=%d saved=%d"), bChanged, bSaved);
    UE_LOG(LogHideInTheBox, Log, TEXT("%s"), *this->LastActionResult);
}

void AHBPlayerController::HBAssign(int32 SlotIndex, int32 AbilityIndex)
{
    AHBPlayerCharacter* PlayerCharacter = Cast<AHBPlayerCharacter>(this->GetPawn());
    if (PlayerCharacter && AbilityIndex >= 0 && AbilityIndex < HBRuleCore::iAbilityCount)
    {
        const bool bAccepted = PlayerCharacter->GetLoadout()->AssignAbilityToSlot(SlotIndex, static_cast<EHBAbility>(AbilityIndex));
        this->LastActionResult = bAccepted ? TEXT("Slot reassigned") : TEXT("Assignment rejected");
    }
}

void AHBPlayerController::HBCameraYaw(float Yaw)
{
    AHBPlayerCharacter* PlayerCharacter = Cast<AHBPlayerCharacter>(this->GetPawn());
    if (PlayerCharacter)
    {
        PlayerCharacter->SetCameraYaw(Yaw);
    }
}

void AHBPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    this->GetWorldTimerManager().ClearTimer(this->DebugCheckTimer);
    this->GetWorldTimerManager().ClearTimer(this->GameplayHUDRefreshTimer);
    if (this->GameplayHUD)
    {
        this->GameplayHUD->RemoveFromParent();
        this->GameplayHUD = nullptr;
    }
    IPlatformInputDeviceMapper::Get().GetOnInputDeviceConnectionChange().Remove(this->ConnectionHandle);
    this->CleanupMappingsAndBindings();
    if (IsValid(this->BuildPreview))
    {
        this->BuildPreview->Destroy();
    }
    Super::EndPlay(EndPlayReason);
}

void AHBPlayerController::HBDebug()
{
#if !UE_BUILD_SHIPPING
    AHBHUD* DemoHUD = Cast<AHBHUD>(this->GetHUD());
    if (DemoHUD)
    {
        DemoHUD->ToggleDiagnostics();
    }
#endif
}

void AHBPlayerController::HBDebugClear()
{
    this->GetWorld()->GetSubsystem<UHBDebugSubsystem>()->Clear();
}

void AHBPlayerController::HBDebugFreeze()
{
    this->GetWorld()->GetSubsystem<UHBDebugSubsystem>()->ToggleFreeze();
}

void AHBPlayerController::HBDebugFilter(FName Category)
{
    if (Category == TEXT("All"))
    {
        Category = NAME_None;
    }
    if (!Category.IsNone() && Category != TEXT("Input") && Category != TEXT("Command") && Category != TEXT("Animation") && Category != TEXT("Combat") && Category != TEXT("System"))
    {
        UE_LOG(LogHideInTheBox, Warning, TEXT("Filter must be All, Input, Command, Animation, Combat or System."));
        return;
    }
    this->GetWorld()->GetSubsystem<UHBDebugSubsystem>()->SetFilter(Category);
}

void AHBPlayerController::HBDebugDump()
{
    this->GetWorld()->GetSubsystem<UHBDebugSubsystem>()->Dump();
}

void AHBPlayerController::HBDebugCheck()
{
#if !UE_BUILD_SHIPPING
    TArray<FString> Checks;
    Checks.Add(Cast<UEnhancedInputComponent>(this->InputComponent) && this->Mapping && this->SlotMapping ? TEXT("OK Enhanced Input component and owned mapping objects exist.") : TEXT("ERROR Enhanced Input missing: inspect DefaultInput.ini and controller."));
    AHBPlayerCharacter* PlayerCharacter = Cast<AHBPlayerCharacter>(this->GetPawn());
    if (PlayerCharacter)
    {
        PlayerCharacter->AppendDebugChecks(Checks);
    }
    else
    {
        Checks.Add(TEXT("ERROR No HB player possessed: inspect GameMode / Default Pawn Class."));
    }
    UHBDebugSubsystem* Debug = this->GetWorld()->GetSubsystem<UHBDebugSubsystem>();
    Debug->SetChecks(Checks);
    Debug->Add(TEXT("System"), TEXT("Configuration checks refreshed; open checks page or HBDebugDump."));
#endif
}

void AHBPlayerController::HBDebugReset()
{
#if !UE_BUILD_SHIPPING
    // Full map reload uses normal EndPlay cleanup; never jumps around phase invariants.
    UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this, true)));
#endif
}

void AHBPlayerController::HBPerfClear()
{
#if !UE_BUILD_SHIPPING
    this->GetWorld()->GetSubsystem<UHBDebugSubsystem>()->ClearPerformance();
#endif
}
