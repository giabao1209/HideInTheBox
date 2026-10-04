#include "HBPlayerCharacter.h"
#include "HBEnemyCharacter.h"
#include "HBGameMode.h"
#include "HBIndicator.h"
#include "HideInTheBox.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/DamageType.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "TimerManager.h"

AHBPlayerCharacter::AHBPlayerCharacter()
{
    this->PrimaryActorTick.bCanEverTick = true;
    this->Tint = FLinearColor(0.1f, 0.7f, 1.0f);
    this->fMaxHealth = 220.0f;
    this->CameraBoom = this->CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
    this->CameraBoom->SetupAttachment(this->GetRootComponent());
    this->CameraBoom->SetUsingAbsoluteRotation(true);
    this->CameraBoom->SetRelativeRotation(FRotator(-60.0f, 0.0f, 0.0f));
    this->CameraBoom->TargetArmLength = 1800.0f;
    this->CameraBoom->bDoCollisionTest = false;
    this->Camera = this->CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
    this->Camera->SetupAttachment(this->CameraBoom);
    this->Feedback = this->CreateDefaultSubobject<UHBFeedbackComponent>(TEXT("Feedback"));
}

void AHBPlayerCharacter::RequestMove(const FVector& Position)
{
    if (!this->IsAlive() || this->bBuildMode)
    {
        return;
    }
    if (this->bAttacking)
    {
        this->BufferedCommand = EHBCommand::EHBC_Move;
        this->BufferedDestination = Position;
        this->BufferedTarget.Reset();
        return;
    }
    this->Command = EHBCommand::EHBC_Move;
    this->Destination = Position;
    this->AttackTarget.Reset();
    this->fBlockedSeconds = 0.0f;
}

void AHBPlayerCharacter::RequestAttack(AHBEnemyCharacter* Target, bool bInPlace)
{
    if (!this->IsAlive() || this->bBuildMode || !IsValid(Target) || !Target->IsAlive())
    {
        return;
    }
    if (this->bAttacking)
    {
        this->BufferedCommand = EHBCommand::EHBC_Attack;
        this->BufferedTarget = Target;
        this->bBufferedInPlace = bInPlace;
        return;
    }
    this->Command = EHBCommand::EHBC_Attack;
    this->AttackTarget = Target;
    this->bInPlaceAttack = bInPlace;
    this->fBlockedSeconds = 0.0f;
    // An in-range press can attack while the stick is held; the swing owns movement.
    if (FVector::DistSquaredXY(this->GetActorLocation(), Target->GetActorLocation()) <= FMath::Square(this->fAttackRange))
    {
        this->StartAttack();
    }
}

void AHBPlayerCharacter::SetDirectInput(const FVector2D& Input)
{
    this->DirectInput = Input.GetClampedToMaxSize(1.0);
    if (!this->DirectInput.IsNearlyZero() && !this->bAttacking)
    {
        this->Command = EHBCommand::EHBC_None;
        this->AttackTarget.Reset();
    }
}

void AHBPlayerCharacter::SetAimInput(const FVector2D& Input)
{
    this->AimInput = Input;
}

void AHBPlayerCharacter::SetHover(AHBEnemyCharacter* Target)
{
    this->HoverTarget = Target;
}

AHBEnemyCharacter* AHBPlayerCharacter::GetAttackTarget() const
{
    return this->Command == EHBCommand::EHBC_Attack ? this->AttackTarget.Get() : nullptr;
}

void AHBPlayerCharacter::FaceDirection(const FVector& Direction)
{
    if (!Direction.IsNearlyZero())
    {
        this->SetActorRotation(FRotator(0.0f, Direction.Rotation().Yaw, 0.0f));
    }
}

void AHBPlayerCharacter::MoveToward(const FVector& Position, float fDeltaSeconds)
{
    FVector Direction = Position - this->GetActorLocation();
    Direction.Z = 0.0f;
    this->FaceDirection(Direction);
    this->AddMovementInput(Direction.GetSafeNormal());
    if (this->GetVelocity().SizeSquared2D() < 25.0f)
    {
        this->fBlockedSeconds += fDeltaSeconds;
        if (this->fBlockedSeconds > 1.5f)
        {
            this->CancelCommands();
        }
    }
    else
    {
        this->fBlockedSeconds = 0.0f;
    }
}

void AHBPlayerCharacter::UpdateCommand(float fDeltaSeconds)
{
    if (this->Command == EHBCommand::EHBC_Move)
    {
        if (FVector::DistSquaredXY(this->GetActorLocation(), this->Destination) <= FMath::Square(this->fArrivalDistance))
        {
            this->Command = EHBCommand::EHBC_None;
            this->GetCharacterMovement()->StopMovementImmediately();
            return;
        }
        this->MoveToward(this->Destination, fDeltaSeconds);
    }
    else if (this->Command == EHBCommand::EHBC_Attack)
    {
        AHBEnemyCharacter* Target = this->AttackTarget.Get();
        if (!IsValid(Target) || !Target->IsAlive())
        {
            this->Command = EHBCommand::EHBC_None;
            this->AttackTarget.Reset();
            return;
        }
        if (FVector::DistSquaredXY(this->GetActorLocation(), Target->GetActorLocation()) <= FMath::Square(this->fAttackRange))
        {
            this->StartAttack();
        }
        else if (this->bInPlaceAttack)
        {
            this->Command = EHBCommand::EHBC_None;
            this->AttackTarget.Reset();
        }
        else
        {
            this->MoveToward(Target->GetActorLocation(), fDeltaSeconds);
        }
    }
}

void AHBPlayerCharacter::Tick(float fDeltaSeconds)
{
    Super::Tick(fDeltaSeconds);
    if (!this->IsAlive() || !this->IsLocallyControlled() || this->bBuildMode)
    {
        this->Feedback->HideAll();
        return;
    }
    this->fMana = FMath::Min(100.0f, this->fMana + 8.0f * fDeltaSeconds);
    if (!this->bAttacking)
    {
        if (!this->DirectInput.IsNearlyZero())
        {
            // Fixed camera yaw: screen up is +X and screen right is +Y.
            const FVector Direction(this->DirectInput.Y, this->DirectInput.X, 0.0f);
            this->Command = EHBCommand::EHBC_None;
            this->AttackTarget.Reset();
            this->AddMovementInput(Direction.GetSafeNormal(), this->DirectInput.Size());
            const FVector Facing = this->AimInput.IsNearlyZero() ? Direction : FVector(this->AimInput.Y, this->AimInput.X, 0.0f);
            this->FaceDirection(Facing);
        }
        else
        {
            this->UpdateCommand(fDeltaSeconds);
            if (this->Command == EHBCommand::EHBC_None && !this->AimInput.IsNearlyZero())
            {
                this->FaceDirection(FVector(this->AimInput.Y, this->AimInput.X, 0.0f));
            }
        }
    }
    this->UpdateFeedback();
}

void AHBPlayerCharacter::UpdateFeedback()
{
    AHBEnemyCharacter* Hover = this->HoverTarget.Get();
    AHBEnemyCharacter* Target = this->GetAttackTarget();
    this->Feedback->Present(this->Command == EHBCommand::EHBC_Move, this->Destination,
        IsValid(Hover) && Hover->IsAlive() ? Hover : nullptr, IsValid(Target) && Target->IsAlive() ? Target : nullptr);
}

void AHBPlayerCharacter::StartAttack()
{
    this->bAttacking = true;
    this->bHitDelivered = false;
    this->GetCharacterMovement()->StopMovementImmediately();
    if (this->AttackTarget.IsValid())
    {
        this->FaceDirection(this->AttackTarget->GetActorLocation() - this->GetActorLocation());
    }
    UAnimInstance* Anim = this->GetMesh()->GetAnimInstance();
    if (this->AttackMontage && Anim)
    {
        const float fDuration = Anim->Montage_Play(this->AttackMontage);
        if (fDuration > 0.0f)
        {
            FOnMontageEnded EndDelegate;
            EndDelegate.BindUObject(this, &AHBPlayerCharacter::OnMontageEnded);
            Anim->Montage_SetEndDelegate(EndDelegate, this->AttackMontage);
            // No timed damage in montage mode: a missing notify must be visible.
            this->GetWorldTimerManager().SetTimer(this->AttackEndTimer, this, &AHBPlayerCharacter::FinishAttack, fDuration + 0.5f, false);
            return;
        }
    }
    this->GetWorldTimerManager().SetTimer(this->AttackHitTimer, this, &AHBPlayerCharacter::DeliverAttackHit, this->fFallbackWindup, false);
    this->GetWorldTimerManager().SetTimer(this->AttackEndTimer, this, &AHBPlayerCharacter::FinishAttack, this->fFallbackDuration, false);
}

void AHBPlayerCharacter::DeliverAnimationHit(UAnimSequenceBase* Animation)
{
    UAnimInstance* Anim = this->GetMesh()->GetAnimInstance();
    if (Animation == this->AttackMontage && Anim && Anim->Montage_IsActive(this->AttackMontage))
    {
        this->DeliverAttackHit();
    }
}

void AHBPlayerCharacter::DeliverAttackHit()
{
    if (!this->bAttacking || this->bHitDelivered || !this->IsAlive())
    {
        return;
    }
    this->bHitDelivered = true;
    AHBEnemyCharacter* Target = this->AttackTarget.Get();
    if (IsValid(Target) && Target->IsAlive() && FVector::DistSquaredXY(this->GetActorLocation(), Target->GetActorLocation()) <= FMath::Square(this->fAttackRange + 25.0f))
    {
        UGameplayStatics::ApplyDamage(Target, this->fAttackDamage, this->GetController(), this, UDamageType::StaticClass());
        ++this->iHitCount;
    }
}

void AHBPlayerCharacter::FinishAttack()
{
    if (!this->bAttacking)
    {
        return;
    }
    this->GetWorldTimerManager().ClearTimer(this->AttackHitTimer);
    this->GetWorldTimerManager().ClearTimer(this->AttackEndTimer);
    this->bAttacking = false;
    UAnimInstance* Anim = this->GetMesh()->GetAnimInstance();
    if (Anim && this->AttackMontage && Anim->Montage_IsActive(this->AttackMontage))
    {
        FOnMontageEnded EmptyDelegate;
        Anim->Montage_SetEndDelegate(EmptyDelegate, this->AttackMontage);
        Anim->Montage_Stop(0.0f, this->AttackMontage);
    }
    this->Command = EHBCommand::EHBC_None;
    this->AttackTarget.Reset();
    const EHBCommand NextCommand = this->BufferedCommand;
    this->BufferedCommand = EHBCommand::EHBC_None;
    if (NextCommand == EHBCommand::EHBC_Move)
    {
        this->RequestMove(this->BufferedDestination);
    }
    else if (NextCommand == EHBCommand::EHBC_Attack)
    {
        this->RequestAttack(this->BufferedTarget.Get(), this->bBufferedInPlace);
    }
    this->BufferedTarget.Reset();
}

void AHBPlayerCharacter::OnMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
    if (Montage != this->AttackMontage)
    {
        return;
    }
    if (bInterrupted)
    {
        this->BufferedCommand = EHBCommand::EHBC_None;
    }
    this->FinishAttack();
}

void AHBPlayerCharacter::CancelCommands()
{
    this->bAttacking = false;
    this->Command = EHBCommand::EHBC_None;
    this->BufferedCommand = EHBCommand::EHBC_None;
    this->AttackTarget.Reset();
    this->BufferedTarget.Reset();
    this->HoverTarget.Reset();
    this->DirectInput = FVector2D::ZeroVector;
    this->AimInput = FVector2D::ZeroVector;
    this->GetWorldTimerManager().ClearTimer(this->AttackHitTimer);
    this->GetWorldTimerManager().ClearTimer(this->AttackEndTimer);
    UAnimInstance* Anim = this->GetMesh()->GetAnimInstance();
    if (Anim && this->AttackMontage)
    {
        FOnMontageEnded EmptyDelegate;
        Anim->Montage_SetEndDelegate(EmptyDelegate, this->AttackMontage);
        Anim->Montage_Stop(0.0f, this->AttackMontage);
    }
    this->GetCharacterMovement()->StopMovementImmediately();
    this->Feedback->HideAll();
}

void AHBPlayerCharacter::CastPulse()
{
    if (!this->IsAlive() || this->bBuildMode || this->bAttacking || this->fMana < 20.0f || this->GetWorld()->GetTimeSeconds() < this->fPulseReadyTime)
    {
        return;
    }
    AHBGameMode* Mode = this->GetWorld()->GetAuthGameMode<AHBGameMode>();
    if (!Mode)
    {
        return;
    }
    this->fMana -= 20.0f;
    this->fPulseReadyTime = this->GetWorld()->GetTimeSeconds() + 1.5f;
    // Damage can unregister enemies. Iterate a snapshot, outside Tick.
    const TArray<TWeakObjectPtr<AHBEnemyCharacter>> Targets = Mode->GetEnemies();
    for (const TWeakObjectPtr<AHBEnemyCharacter>& Entry : Targets)
    {
        AHBEnemyCharacter* Target = Entry.Get();
        if (IsValid(Target) && Target->IsAlive() && FVector::DistSquaredXY(this->GetActorLocation(), Target->GetActorLocation()) <= FMath::Square(350.0f))
        {
            UGameplayStatics::ApplyDamage(Target, 45.0f, this->GetController(), this, UDamageType::StaticClass());
        }
    }
}

void AHBPlayerCharacter::SetBuildMode(bool bEnabled)
{
    this->CancelCommands();
    this->bBuildMode = bEnabled;
}

void AHBPlayerCharacter::ResetForPhase(const FVector& Position)
{
    this->SetBuildMode(false);
    this->RestoreHealth();
    this->fMana = 100.0f;
    this->SetActorLocation(Position, false, nullptr, ETeleportType::TeleportPhysics);
}

void AHBPlayerCharacter::OnDeath()
{
    this->CancelCommands();
    AHBGameMode* Mode = this->GetWorld()->GetAuthGameMode<AHBGameMode>();
    if (Mode)
    {
        Mode->HandlePlayerDeath();
    }
}

void AHBPlayerCharacter::UnPossessed()
{
    this->CancelCommands();
    Super::UnPossessed();
}

void AHBPlayerCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    this->CancelCommands();
    Super::EndPlay(EndPlayReason);
}
