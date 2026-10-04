#include "HBPlayerController.h"
#include "HBPlayerCharacter.h"
#include "HBEnemyCharacter.h"
#include "HBGameMode.h"
#include "HBIndicator.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

namespace HBInput
{
    enum EAction : int32
    {
        EA_Click, EA_Attack, EA_Pulse, EA_Advance, EA_Build, EA_Cycle, EA_Repair, EA_Cancel, EA_Pause, EA_Restart, EA_Count
    };
}

AHBPlayerController::AHBPlayerController()
{
    this->bShowMouseCursor = true;
    this->DefaultMouseCursor = EMouseCursor::Crosshairs;
}

void AHBPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();
    UEnhancedInputComponent* Enhanced = CastChecked<UEnhancedInputComponent>(this->InputComponent);
    this->Mapping = NewObject<UInputMappingContext>(this);
    const FKey Keys[HBInput::EA_Count] = { EKeys::LeftMouseButton, EKeys::Gamepad_FaceButton_Bottom, EKeys::RightMouseButton, EKeys::Enter, EKeys::B, EKeys::Tab, EKeys::R, EKeys::SpaceBar, EKeys::P, EKeys::F5 };
    for (int32 iIndex = 0; iIndex < HBInput::EA_Count; ++iIndex)
    {
        UInputAction* Action = NewObject<UInputAction>(this);
        Action->ValueType = EInputActionValueType::Boolean;
        Action->bTriggerWhenPaused = iIndex == HBInput::EA_Pause;
        this->Actions.Add(Action);
        this->Mapping->MapKey(Action, Keys[iIndex]);
        Enhanced->BindAction(Action, ETriggerEvent::Started, this, &AHBPlayerController::HandleAction, iIndex);
    }
    this->Mapping->MapKey(this->Actions[HBInput::EA_Pulse], EKeys::Gamepad_FaceButton_Left);
    this->Mapping->MapKey(this->Actions[HBInput::EA_Advance], EKeys::Gamepad_FaceButton_Top);
    this->Mapping->MapKey(this->Actions[HBInput::EA_Build], EKeys::Gamepad_LeftShoulder);
    this->Mapping->MapKey(this->Actions[HBInput::EA_Cycle], EKeys::Gamepad_DPad_Right);
    this->Mapping->MapKey(this->Actions[HBInput::EA_Repair], EKeys::Gamepad_RightShoulder);
    this->Mapping->MapKey(this->Actions[HBInput::EA_Cancel], EKeys::Gamepad_FaceButton_Right);
    this->Mapping->MapKey(this->Actions[HBInput::EA_Pause], EKeys::Gamepad_Special_Right);
    this->Mapping->MapKey(this->Actions[HBInput::EA_Restart], EKeys::Gamepad_Special_Left);
    this->MoveAction = NewObject<UInputAction>(this);
    this->MoveAction->ValueType = EInputActionValueType::Axis2D;
    this->Mapping->MapKey(this->MoveAction, EKeys::Gamepad_Left2D);
    Enhanced->BindAction(this->MoveAction, ETriggerEvent::Triggered, this, &AHBPlayerController::HandleMove);
    Enhanced->BindAction(this->MoveAction, ETriggerEvent::Completed, this, &AHBPlayerController::HandleMove);
    Enhanced->BindAction(this->MoveAction, ETriggerEvent::Canceled, this, &AHBPlayerController::HandleMove);
    this->AimAction = NewObject<UInputAction>(this);
    this->AimAction->ValueType = EInputActionValueType::Axis2D;
    this->Mapping->MapKey(this->AimAction, EKeys::Gamepad_Right2D);
    Enhanced->BindAction(this->AimAction, ETriggerEvent::Triggered, this, &AHBPlayerController::HandleAim);
    Enhanced->BindAction(this->AimAction, ETriggerEvent::Completed, this, &AHBPlayerController::HandleAim);
    Enhanced->BindAction(this->AimAction, ETriggerEvent::Canceled, this, &AHBPlayerController::HandleAim);
    UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(this->GetLocalPlayer());
    if (Subsystem)
    {
        FModifyContextOptions Options;
        Options.bIgnoreAllPressedKeysUntilRelease = true;
        Subsystem->AddMappingContext(this->Mapping, 0, Options);
    }
}

void AHBPlayerController::BeginPlay()
{
    Super::BeginPlay();
    this->ConnectionHandle = IPlatformInputDeviceMapper::Get().GetOnInputDeviceConnectionChange().AddUObject(this, &AHBPlayerController::OnDeviceConnectionChanged);
    FInputModeGameAndUI Mode;
    Mode.SetHideCursorDuringCapture(false);
    this->SetInputMode(Mode);
    FActorSpawnParameters Parameters;
    Parameters.Owner = this;
    this->BuildPreview = this->GetWorld()->SpawnActor<AHBIndicator>(AHBIndicator::StaticClass(), FTransform::Identity, Parameters);
}

void AHBPlayerController::HandleMove(const FInputActionValue& Value)
{
    this->MoveInput = HBRules::ApplyRadialDeadZone(Value.Get<FVector2D>(), 0.2f);
    if (!this->MoveInput.IsNearlyZero())
    {
        this->bGamepad = true;
    }
}

void AHBPlayerController::HandleAim(const FInputActionValue& Value)
{
    this->AimInput = HBRules::ApplyRadialDeadZone(Value.Get<FVector2D>(), 0.2f);
    if (!this->AimInput.IsNearlyZero())
    {
        this->bGamepad = true;
    }
}

void AHBPlayerController::ContextClick()
{
    AHBPlayerCharacter* Player = Cast<AHBPlayerCharacter>(this->GetPawn());
    AHBGameMode* Mode = this->GetWorld()->GetAuthGameMode<AHBGameMode>();
    if (!Player || !Mode)
    {
        return;
    }
    this->bGamepad = false;
    this->MoveInput = FVector2D::ZeroVector;
    this->AimInput = FVector2D::ZeroVector;
    if (Player->IsBuilding())
    {
        Mode->ConfirmBuild();
        return;
    }
    FHitResult Hit;
    if (this->GetHitResultUnderCursor(ECC_Visibility, false, Hit))
    {
        AHBEnemyCharacter* Enemy = Cast<AHBEnemyCharacter>(Hit.GetActor());
        if (Enemy && Enemy->IsAlive())
        {
            Player->RequestAttack(Enemy, this->IsInputKeyDown(EKeys::LeftShift) || this->IsInputKeyDown(EKeys::RightShift));
            return;
        }
    }
    if (!this->IsInputKeyDown(EKeys::LeftShift) && !this->IsInputKeyDown(EKeys::RightShift) && this->GetHitResultUnderCursor(ECC_GameTraceChannel1, false, Hit))
    {
        Player->RequestMove(Hit.ImpactPoint);
    }
}

void AHBPlayerController::PadAttack()
{
    this->bGamepad = true;
    AHBPlayerCharacter* Player = Cast<AHBPlayerCharacter>(this->GetPawn());
    AHBGameMode* Mode = this->GetWorld()->GetAuthGameMode<AHBGameMode>();
    if (!Player || !Mode)
    {
        return;
    }
    if (Player->IsBuilding())
    {
        Mode->ConfirmBuild();
        return;
    }
    const FVector Aim = this->AimInput.IsNearlyZero() ? FVector::ZeroVector : FVector(this->AimInput.Y, this->AimInput.X, 0.0f);
    Player->RequestAttack(Mode->FindEnemy(Player->GetActorLocation(), Aim, 750.0f), false);
}

void AHBPlayerController::HandleAction(const FInputActionValue& Value, int32 iAction)
{
    if (iAction == HBInput::EA_Pause)
    {
        const bool bPaused = this->IsPaused();
        this->ResetTransientInput();
        this->SetPause(!bPaused);
        return;
    }
    if (this->bAwaitNeutral || this->IsPaused())
    {
        return;
    }
    AHBPlayerCharacter* Player = Cast<AHBPlayerCharacter>(this->GetPawn());
    AHBGameMode* Mode = this->GetWorld()->GetAuthGameMode<AHBGameMode>();
    if (!Player || !Mode)
    {
        return;
    }
    if (iAction == HBInput::EA_Restart)
    {
        UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this, true)));
        return;
    }
    if (Mode->GetPhase() == EHBPhase::EHBP_GameOver || !Player->IsAlive())
    {
        return;
    }
    switch (iAction)
    {
        case HBInput::EA_Click: this->ContextClick(); break;
        case HBInput::EA_Attack: this->PadAttack(); break;
        case HBInput::EA_Pulse: Player->CastPulse(); break;
        case HBInput::EA_Advance: Mode->AdvancePhase(); break;
        case HBInput::EA_Build: Mode->ToggleBuildMode(); break;
        case HBInput::EA_Cycle: Mode->CycleBuildSocket(); break;
        case HBInput::EA_Repair: Mode->RepairGate(); break;
        case HBInput::EA_Cancel: Player->SetBuildMode(false); this->ResetTransientInput(); break;
        default: break;
    }
}

bool AHBPlayerController::IsNeutral() const
{
    const FKey Keys[] = { EKeys::W, EKeys::A, EKeys::S, EKeys::D, EKeys::LeftMouseButton, EKeys::RightMouseButton,
        EKeys::Gamepad_FaceButton_Bottom, EKeys::Gamepad_FaceButton_Right, EKeys::Gamepad_FaceButton_Left,
        EKeys::Gamepad_FaceButton_Top, EKeys::Gamepad_LeftShoulder, EKeys::Gamepad_RightShoulder,
        EKeys::Enter, EKeys::B, EKeys::R, EKeys::Tab, EKeys::SpaceBar };
    for (const FKey& Key : Keys)
    {
        if (this->IsInputKeyDown(Key))
        {
            return false;
        }
    }
    return this->MoveInput.IsNearlyZero() && this->AimInput.IsNearlyZero();
}

void AHBPlayerController::PlayerTick(float fDeltaSeconds)
{
    Super::PlayerTick(fDeltaSeconds);
    AHBPlayerCharacter* Player = Cast<AHBPlayerCharacter>(this->GetPawn());
    AHBGameMode* Mode = this->GetWorld()->GetAuthGameMode<AHBGameMode>();
    if (!Player || !Mode)
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
    if (this->IsPaused() || Mode->GetPhase() == EHBPhase::EHBP_GameOver || !Player->IsAlive())
    {
        return;
    }
    const FVector2D Keyboard((this->IsInputKeyDown(EKeys::D) ? 1.0f : 0.0f) - (this->IsInputKeyDown(EKeys::A) ? 1.0f : 0.0f),
        (this->IsInputKeyDown(EKeys::W) ? 1.0f : 0.0f) - (this->IsInputKeyDown(EKeys::S) ? 1.0f : 0.0f));
    if (!Keyboard.IsNearlyZero())
    {
        this->bGamepad = false;
    }
    Player->SetDirectInput(Keyboard.IsNearlyZero() ? this->MoveInput : Keyboard.GetClampedToMaxSize(1.0));
    Player->SetAimInput(this->bGamepad ? this->AimInput : FVector2D::ZeroVector);
    FHitResult Hit;
    AHBEnemyCharacter* Hover = nullptr;
    if (!this->bGamepad && this->GetHitResultUnderCursor(ECC_Visibility, false, Hit))
    {
        Hover = Cast<AHBEnemyCharacter>(Hit.GetActor());
    }
    Player->SetHover(Hover);
    if (this->BuildPreview)
    {
        if (Player->IsBuilding())
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
    this->MoveInput = FVector2D::ZeroVector;
    this->AimInput = FVector2D::ZeroVector;
    this->bAwaitNeutral = true;
    AHBPlayerCharacter* Player = Cast<AHBPlayerCharacter>(this->GetPawn());
    if (Player)
    {
        Player->CancelCommands();
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

void AHBPlayerController::OnDeviceConnectionChanged(EInputDeviceConnectionState State, FPlatformUserId UserId, FInputDeviceId DeviceId)
{
    if (State == EInputDeviceConnectionState::Disconnected && this->bGamepad)
    {
        this->FlushPressedKeys();
        this->SetPause(true);
    }
}

void AHBPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    IPlatformInputDeviceMapper::Get().GetOnInputDeviceConnectionChange().Remove(this->ConnectionHandle);
    UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(this->GetLocalPlayer());
    if (Subsystem && this->Mapping)
    {
        Subsystem->RemoveMappingContext(this->Mapping);
    }
    if (IsValid(this->BuildPreview))
    {
        this->BuildPreview->Destroy();
    }
    Super::EndPlay(EndPlayReason);
}
