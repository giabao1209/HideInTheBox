#include "HBPlayerController.h"
#include "CommonActivatableWidget.h"
#include "HBPracticeWidget.h"
#include "HBDebugSubsystem.h"
#include "HBPlayerCharacter.h"
#include "HBEnemyCharacter.h"
#include "HBGameplayAbility.h"
#include "AbilitySystemComponent.h"
#include "HBGameMode.h"
#include "HBIndicator.h"
#include "HBInputSave.h"
#include "HBLoadout.h"
#include "HBHUD.h"
#include "Blueprint/UserWidget.h"
#include "HideInTheBox.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "EnhancedActionKeyMapping.h"
#include "EnhancedPlayerInput.h"
#include "InputAction.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

namespace HBInput
{
    enum class EUtility : int32
    {
        EU_Advance, EU_Build, EU_Cycle, EU_Repair, EU_Cancel, EU_Pause, EU_Restart,
        EU_Interact, EU_Diagnostics, EU_PracticeReset, EU_PracticeDamage, EU_PracticeRefill, EU_Count
    };
    const TCHAR* SaveSlot = TEXT("HideInTheBox_Input_v1");
}

AHBPlayerController::AHBPlayerController()
{
    this->bShowMouseCursor = true;
    this->DefaultMouseCursor = EMouseCursor::Crosshairs;
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
    this->Mapping = this->GameplayContext.LoadSynchronous();
    UInputMappingContext* DefaultSlots = this->SlotContext.LoadSynchronous();
    UInputAction* MoveAction = this->GamepadMoveAsset.LoadSynchronous();
    UInputAction* KeyboardMoveAction = this->KeyboardMoveAsset.LoadSynchronous();
    UInputAction* AimAction = this->GamepadAimAsset.LoadSynchronous();
    this->AttackInPlaceAction = this->AttackInPlaceAsset.LoadSynchronous();
    if (!this->Mapping || !DefaultSlots || !MoveAction || !KeyboardMoveAction || !AimAction || !this->AttackInPlaceAction ||
        this->UtilityActionAssets.Num() != static_cast<int32>(HBInput::EUtility::EU_Count) ||
        this->SlotActionAssets.Num() != HBRuleCore::iSlotCount)
    {
        UE_LOG(LogHideInTheBox, Error, TEXT("Arena Input Actions or Mapping Contexts are missing. Inspect DefaultInput.ini."));
        return;
    }
    for (const TSoftObjectPtr<UInputAction>& Asset : this->UtilityActionAssets)
    {
        UInputAction* Action = Asset.LoadSynchronous();
        if (!Action || !this->Mapping->HasMappingForInputAction(Action))
        {
            UE_LOG(LogHideInTheBox, Error, TEXT("Utility Input Action %s is missing from IMC_ArenaGameplay."), *Asset.ToString());
            return;
        }
        this->UtilityActions.Add(Action);
    }
    this->KeyboardKeys.SetNum(HBRuleCore::iSlotCount);
    this->GamepadKeys.SetNum(HBRuleCore::iSlotCount);
    for (int32 iSlot = 0; iSlot < this->SlotActionAssets.Num(); ++iSlot)
    {
        UInputAction* Action = this->SlotActionAssets[iSlot].LoadSynchronous();
        if (!Action || this->SlotActions.Contains(Action))
        {
            UE_LOG(LogHideInTheBox, Error, TEXT("Input slot %d needs a unique Input Action in DefaultInput.ini."), iSlot);
            return;
        }
        this->SlotActions.Add(Action);
        for (const FEnhancedActionKeyMapping& Entry : DefaultSlots->GetMappings())
        {
            if (Entry.Action != Action)
            {
                continue;
            }
            FKey& DefaultKey = Entry.Key.IsGamepadKey() ? this->GamepadKeys[iSlot] : this->KeyboardKeys[iSlot];
            if (DefaultKey.IsValid())
            {
                UE_LOG(LogHideInTheBox, Error, TEXT("Slot %d needs one keyboard/mouse key and one gamepad key in IMC_ArenaSlots."), iSlot);
                return;
            }
            DefaultKey = Entry.Key;
        }
    }
    if (!this->ValidateKeys(this->KeyboardKeys, false) || !this->ValidateKeys(this->GamepadKeys, true))
    {
        UE_LOG(LogHideInTheBox, Error, TEXT("Invalid or conflicting slot keys in IMC_ArenaSlots. Check both input families."));
        return;
    }
    this->LoadBindings();

    // Keep per-player remaps out of the authored IMC while preserving its mapping modifiers and triggers.
    this->SlotMapping = DuplicateObject<UInputMappingContext>(DefaultSlots, this);
    this->BoundInput = Enhanced;
    for (int32 iAction = 0; iAction < this->UtilityActions.Num(); ++iAction)
    {
        this->BindingHandles.Add(Enhanced->BindAction(this->UtilityActions[iAction], ETriggerEvent::Started,
            this, &AHBPlayerController::HandleUtility, iAction).GetHandle());
    }
    for (int32 iSlot = 0; iSlot < this->SlotActions.Num(); ++iSlot)
    {
        this->BindingHandles.Add(Enhanced->BindAction(this->SlotActions[iSlot], ETriggerEvent::Started,
            this, &AHBPlayerController::HandleSlot, iSlot).GetHandle());
    }
    const ETriggerEvent Events[] = { ETriggerEvent::Triggered, ETriggerEvent::Completed, ETriggerEvent::Canceled };
    for (ETriggerEvent Event : Events)
    {
        this->BindingHandles.Add(Enhanced->BindAction(MoveAction, Event, this, &AHBPlayerController::HandleMove).GetHandle());
        this->BindingHandles.Add(Enhanced->BindAction(KeyboardMoveAction, Event, this, &AHBPlayerController::HandleKeyboardMove).GetHandle());
        this->BindingHandles.Add(Enhanced->BindAction(AimAction, Event, this, &AHBPlayerController::HandleAim).GetHandle());
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
    const AHBGameMode* GameMode = GetWorld()->GetAuthGameMode<AHBGameMode>();
    if (IsLocalController() && GameMode && GameMode->GetPracticeHUDClass())
    {
        PracticeHUD = CreateWidget<UUserWidget>(this, GameMode->GetPracticeHUDClass());
        if (PracticeHUD)
        {
            PracticeHUD->AddToPlayerScreen();
            if (UCommonActivatableWidget* CommonHUD = Cast<UCommonActivatableWidget>(PracticeHUD))
            {
                CommonHUD->ActivateWidget();
            }
        }
    }
#if !UE_BUILD_SHIPPING
    this->GetWorldTimerManager().SetTimer(this->DebugCheckTimer, this, &AHBPlayerController::HBDebugCheck, 0.2f, false);
#endif
    this->ConnectionHandle = IPlatformInputDeviceMapper::Get().GetOnInputDeviceConnectionChange().AddUObject(this, &AHBPlayerController::OnDeviceConnectionChanged);
    FInputModeGameAndUI Mode;
    Mode.SetHideCursorDuringCapture(false);
    this->SetInputMode(Mode);
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

bool AHBPlayerController::IsActionFromGamepad(const UInputMappingContext& Context, const UInputAction* Action) const
{
    bool bFromGamepad = this->bGamepad;
    float PressedTime = TNumericLimits<float>::Max();
    for (const FEnhancedActionKeyMapping& Entry : Context.GetMappings())
    {
        if (Entry.Action == Action && this->IsInputKeyDown(Entry.Key))
        {
            const float Time = this->GetInputKeyTimeDown(Entry.Key);
            if (Time < PressedTime)
            {
                PressedTime = Time;
                bFromGamepad = Entry.Key.IsGamepadKey();
            }
        }
    }
    return bFromGamepad;
}

void AHBPlayerController::HandleSlot(const FInputActionValue& Value, int32 iSlot)
{
    const bool bFromGamepad = this->IsActionFromGamepad(*this->SlotMapping, this->SlotActions[iSlot]);
    UHBDebugSubsystem::Record(this, TEXT("Input"), FString::Printf(TEXT("Slot %d from %s paused=%d neutralWait=%d"), iSlot, bFromGamepad ? TEXT("Gamepad") : TEXT("Mouse/KB"), this->IsPaused(), this->bAwaitNeutral));
    if (this->IsPaused() || this->bAwaitNeutral)
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
        if (iSlot == 0)
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
    if (bFromGamepad)
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
        if (const UEnhancedPlayerInput* Enhanced = Cast<UEnhancedPlayerInput>(this->PlayerInput))
        {
            bInPlace = Enhanced->GetActionValue(this->AttackInPlaceAction).Get<bool>();
        }
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
    const bool bBolt = Ability == EHBAbility::EHBA_Bolt;
    const float PreviousHealth = bBolt && IsValid(Target) ? Target->GetHealth() : 0.0f;
    const bool bExposed = bBolt && IsValid(Target) && Target->GetAbilitySystemComponent()->HasMatchingGameplayTag(HBGetExposedTag());
    const EHBActionResult Result = Loadout->TryActivateSlot(iSlot, Target, AimPoint, bInPlace);
    if (bBolt)
    {
        switch (Result)
        {
            case EHBActionResult::EHBAR_Accepted:
            {
                const float CurrentHealth = IsValid(Target) ? Target->GetHealth() : 0.0f;
                this->LastActionResult = FString::Printf(TEXT("Bolt hit for %.0f damage%s"), FMath::Max(0.0f, PreviousHealth - CurrentHealth),
                    bExposed ? TEXT(" (Exposed)") : TEXT(""));
                return;
            }
            case EHBActionResult::EHBAR_InvalidTarget:
                this->LastActionResult = TEXT("Bolt: Aim at a visible enemy within range");
                return;
            case EHBActionResult::EHBAR_NoMana:
                this->LastActionResult = TEXT("Bolt: Not enough mana");
                return;
            case EHBActionResult::EHBAR_Cooldown:
                this->LastActionResult = TEXT("Bolt: On cooldown");
                return;
            default:
                break;
        }
    }
    this->LastActionResult = FString::Printf(TEXT("Slot %d: %s"), iSlot + 1, *UEnum::GetValueAsString(Result));
}

void AHBPlayerController::HandleUtility(const FInputActionValue& Value, int32 iAction)
{
    const HBInput::EUtility Action = static_cast<HBInput::EUtility>(iAction);
    if (Action == HBInput::EUtility::EU_Diagnostics)
    {
        this->HBDebug();
        return;
    }
    const UInputAction* InputAction = this->UtilityActions[iAction];
    const bool bFromGamepad = this->IsActionFromGamepad(*this->Mapping, InputAction);
    UHBDebugSubsystem::Record(this, TEXT("Input"), FString::Printf(TEXT("Utility %s from %s"),
        *GetNameSafe(InputAction), bFromGamepad ? TEXT("Gamepad") : TEXT("Mouse/KB")));
    if (Action == HBInput::EUtility::EU_Pause)
    {
        this->SetArenaPaused(!this->IsPaused());
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
        this->RestartArena();
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
        case HBInput::EUtility::EU_PracticeReset: this->HBGASReset(); break;
        case HBInput::EUtility::EU_PracticeDamage: this->HBGASDamage(); break;
        case HBInput::EUtility::EU_PracticeRefill: this->HBGASRefill(); break;
        case HBInput::EUtility::EU_Cancel: PlayerCharacter->SetBuildMode(false); this->ResetTransientInput(); break;
        default: break;
    }
}

bool AHBPlayerController::IsNeutral() const
{
    for (const UInputMappingContext* Context : { this->Mapping.Get(), this->SlotMapping.Get() })
    {
        if (!Context)
        {
            continue;
        }
        for (const FEnhancedActionKeyMapping& Entry : Context->GetMappings())
        {
            if (!Entry.Key.IsAnalog() && this->IsInputKeyDown(Entry.Key))
            {
                return false;
            }
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
    else
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
        this->SetArenaPaused(true);
    }
}

void AHBPlayerController::SetArenaPaused(bool bPaused)
{
    ResetTransientInput();
    SetPause(bPaused);
    if (UHBArenaHUD* ArenaHUD = Cast<UHBArenaHUD>(PracticeHUD))
    {
        ArenaHUD->SetPauseMenuVisible(IsPaused());
    }
}

void AHBPlayerController::RestartArena()
{
    SetArenaPaused(false);
    UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this, true)));
}

bool AHBPlayerController::IsReserved(const FKey& Key) const
{
    if (Key == EKeys::Escape || Key == EKeys::Tilde)
    {
        return true;
    }
    if (this->Mapping)
    {
        for (const FEnhancedActionKeyMapping& Entry : this->Mapping->GetMappings())
        {
            if (Entry.Key == Key)
            {
                return true;
            }
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
}

void AHBPlayerController::RebuildSlotMapping()
{
    UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(this->GetLocalPlayer());
    if (!Subsystem || !this->SlotMapping)
    {
        return;
    }
    Subsystem->RemoveMappingContext(this->SlotMapping);
    for (int32 iIndex = 0; iIndex < this->SlotMapping->GetMappings().Num(); ++iIndex)
    {
        FEnhancedActionKeyMapping& Entry = this->SlotMapping->GetMapping(iIndex);
        const int32 iSlot = this->SlotActions.IndexOfByKey(Entry.Action);
        if (this->SlotActions.IsValidIndex(iSlot))
        {
            Entry.Key = this->GetSlotKey(iSlot, Entry.Key.IsGamepadKey());
        }
    }
    FModifyContextOptions Options;
    Options.bIgnoreAllPressedKeysUntilRelease = true;
    Subsystem->AddMappingContext(this->SlotMapping, 1, Options);
    this->ResetTransientInput();
}

bool AHBPlayerController::RebindSlotKey(int32 iSlot, FKey NewKey, bool bForGamepad)
{
    TArray<FKey>& Keys = bForGamepad ? this->GamepadKeys : this->KeyboardKeys;
    if (!this->SlotMapping || !Keys.IsValidIndex(iSlot))
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
    if (PracticeHUD)
    {
        if (UCommonActivatableWidget* CommonHUD = Cast<UCommonActivatableWidget>(PracticeHUD))
        {
            CommonHUD->DeactivateWidget();
        }
        PracticeHUD->RemoveFromParent();
    }
    this->GetWorldTimerManager().ClearTimer(this->DebugCheckTimer);
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
    Checks.Add(this->BoundInput.IsValid() && this->Mapping && this->SlotMapping ? TEXT("OK Enhanced Input contexts loaded from IMC assets.") : TEXT("ERROR Enhanced Input missing: inspect DefaultInput.ini and /Game/Input."));
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

void AHBPlayerController::HBGASReset()
{
    AHBGameMode* Mode = this->GetWorld()->GetAuthGameMode<AHBGameMode>();
    if (Mode && !this->IsPaused()) { Mode->ResetGASDemo(); }
}

void AHBPlayerController::HBGASDamage()
{
    AHBGameMode* Mode = this->GetWorld()->GetAuthGameMode<AHBGameMode>();
    if (Mode && !this->IsPaused()) { Mode->DamageGASDemoPlayer(); }
}

void AHBPlayerController::HBGASRefill()
{
    AHBGameMode* Mode = this->GetWorld()->GetAuthGameMode<AHBGameMode>();
    if (Mode && !this->IsPaused()) { Mode->RefillGASDemoMana(); }
}
