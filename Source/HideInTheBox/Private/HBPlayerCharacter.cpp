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
#include "Components/CapsuleComponent.h"
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
    this->Loadout = this->CreateDefaultSubobject<UHBLoadoutComponent>(TEXT("Loadout"));
}

void AHBPlayerCharacter::RequestMove(const FVector& Position)
{
    if (!this->IsAlive() || this->bBuildMode || this->bDodging)
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
    if (!this->IsAlive() || this->bBuildMode || this->bDodging || !IsValid(Target) || !Target->IsAlive())
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
    if (!Input.IsNearlyZero())
    {
        this->SetAimWorldDirection(this->CameraInputToWorld(Input));
    }
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
        const FRotator Desired(0.0f, Direction.Rotation().Yaw, 0.0f);
        this->SetActorRotation(FMath::RInterpConstantTo(this->GetActorRotation(), Desired, this->GetWorld()->GetDeltaSeconds(), this->fTurnSpeed));
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
    this->Loadout->Regenerate(fDeltaSeconds);
    if (this->bDodging)
    {
        if (this->GetWorld()->GetTimeSeconds() >= this->fDodgeEndTime)
        {
            this->EndDodge();
        }
        else
        {
            this->AddMovementInput(this->DodgeDirection, 1.0f);
        }
        this->UpdateFeedback();
        return;
    }
    if (!this->bAttacking)
    {
        if (!this->DirectInput.IsNearlyZero())
        {
            const FVector Direction = this->CameraInputToWorld(this->DirectInput);
            this->Command = EHBCommand::EHBC_None;
            this->AttackTarget.Reset();
            this->AddMovementInput(Direction.GetSafeNormal(), this->DirectInput.Size());
            this->FaceDirection(this->bHasAim ? this->AimDirection : Direction);
        }
        else
        {
            this->UpdateCommand(fDeltaSeconds);
            if (this->Command == EHBCommand::EHBC_None && this->bHasAim)
            {
                this->FaceDirection(this->AimDirection);
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
        const FVector Direction = this->AttackTarget->GetActorLocation() - this->GetActorLocation();
        this->SetActorRotation(FRotator(0.0f, Direction.Rotation().Yaw, 0.0f));
    }
    UAnimInstance* Anim = this->GetMesh()->GetAnimInstance();
    if (this->AttackMontage)
    {
        const float fDuration = Anim ? Anim->Montage_Play(this->AttackMontage) : 0.0f;
        if (fDuration <= 0.0f)
        {
            this->AttackPlayback = TEXT("Playback failed");
            this->LastAttackOutcome = TEXT("Playback failed: check mesh / AnimBP / skeleton");
            UE_LOG(LogHideInTheBox, Warning, TEXT("Attack montage could not play on %s"), *this->GetName());
            this->bAttacking = false;
            this->Command = EHBCommand::EHBC_None;
            this->AttackTarget.Reset();
            return;
        }
        this->AttackPlayback = TEXT("Montage");
        FOnMontageEnded EndDelegate;
        EndDelegate.BindUObject(this, &AHBPlayerCharacter::OnMontageEnded);
        Anim->Montage_SetEndDelegate(EndDelegate, this->AttackMontage);
        this->GetWorldTimerManager().SetTimer(this->AttackEndTimer, this, &AHBPlayerCharacter::OnAttackTimeout, fDuration + 0.5f, false);
        return;
    }
    this->AttackPlayback = TEXT("Timed prototype");
    this->GetWorldTimerManager().SetTimer(this->AttackHitTimer, this, &AHBPlayerCharacter::DeliverAttackHit, FMath::Max(0.01f, this->fFallbackWindup), false);
    this->GetWorldTimerManager().SetTimer(this->AttackEndTimer, this, &AHBPlayerCharacter::OnAttackTimeout, FMath::Max(this->fFallbackWindup + 0.01f, this->fFallbackDuration), false);
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
    UE_LOG(LogHideInTheBox, Log, TEXT("Attack hit event: source=%s player=%s"), *this->AttackPlayback, *this->GetName());
    AHBEnemyCharacter* Target = this->AttackTarget.Get();
    if (IsValid(Target) && Target->IsAlive() && FVector::DistSquaredXY(this->GetActorLocation(), Target->GetActorLocation()) <= FMath::Square(this->fAttackRange + 25.0f) && this->HasHitLine(Target->GetActorLocation(), Target))
    {
        UGameplayStatics::ApplyDamage(Target, this->fAttackDamage, this->GetController(), this, UDamageType::StaticClass());
        ++this->iHitCount;
        this->Feedback->PlayEffect(this->GetActorLocation() - FVector(0.0f, 0.0f, this->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()), 90.0f, FLinearColor(1.0f, 0.5f, 0.15f));
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
    if (Montage != this->AttackMontage || !this->bAttacking)
    {
        return;
    }
    this->LastAttackOutcome = bInterrupted ? TEXT("Interrupted") : (this->bHitDelivered ? TEXT("Completed") : TEXT("Completed without notify"));
    UE_LOG(LogHideInTheBox, Log, TEXT("Attack ended: %s"), *this->LastAttackOutcome);
    if (bInterrupted)
    {
        this->BufferedCommand = EHBCommand::EHBC_None;
    }
    this->FinishAttack();
}

void AHBPlayerCharacter::CancelCommands()
{
    if (this->bAttacking)
    {
        this->LastAttackOutcome = TEXT("Canceled by command / lifecycle");
        UE_LOG(LogHideInTheBox, Log, TEXT("Attack canceled on %s"), *this->GetName());
    }
    this->EndDodge();
    this->bHasAim = false;
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

bool AHBPlayerCharacter::ExecuteAbility(EHBAbility Ability, const FHBAbilitySpec& Spec, AHBEnemyCharacter* Target, const FVector& AimPoint, bool bInPlace)
{
    AHBGameMode* Mode = this->GetWorld()->GetAuthGameMode<AHBGameMode>();
    if (!Mode || Mode->GetPhase() == EHBPhase::EHBP_GameOver || !this->IsAlive() || AimPoint.ContainsNaN())
    {
        return false;
    }
    const FVector FootPosition = this->GetActorLocation() - FVector(0.0f, 0.0f, this->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
    if (Ability == EHBAbility::EHBA_Basic)
    {
        if (!IsValid(Target) || !Target->IsAlive() || (bInPlace && FVector::DistSquaredXY(this->GetActorLocation(), Target->GetActorLocation()) > FMath::Square(Spec.fRange)))
        {
            return false;
        }
        this->fAttackDamage = FMath::Max(0.0f, Spec.fPower);
        this->fAttackRange = Spec.fRange;
        this->RequestAttack(Target, bInPlace);
        return this->bAttacking || this->Command == EHBCommand::EHBC_Attack || this->BufferedCommand == EHBCommand::EHBC_Attack;
    }
    if (Ability == EHBAbility::EHBA_Dodge)
    {
        const FVector Direction = !this->DirectInput.IsNearlyZero() ? this->CameraInputToWorld(this->DirectInput) : (this->bHasAim ? this->AimDirection : this->GetActorForwardVector());
        this->CancelCommands();
        this->DodgeDirection = Direction.GetSafeNormal2D();
        this->bDodging = true;
        this->Feedback->PlayEffect(FootPosition, 100.0f, FLinearColor(0.2f, 0.8f, 1.0f));
        this->fDodgeEndTime = this->GetWorld()->GetTimeSeconds() + Spec.fDuration;
        this->fInvulnerableUntil = this->GetWorld()->GetTimeSeconds() + FMath::Min(0.15f, Spec.fDuration);
        this->fSavedWalkSpeed = this->GetCharacterMovement()->MaxWalkSpeed;
        this->GetCharacterMovement()->MaxWalkSpeed = FMath::Min(1600.0f, Spec.fRange / Spec.fDuration);
        this->GetCharacterMovement()->Velocity = this->DodgeDirection * this->GetCharacterMovement()->MaxWalkSpeed;
        return true;
    }
    if (Ability == EHBAbility::EHBA_Guard)
    {
        this->Feedback->PlayEffect(FootPosition, 110.0f, FLinearColor(0.65f, 0.35f, 1.0f));
        this->fGuardUntil = this->GetWorld()->GetTimeSeconds() + Spec.fDuration;
        this->fGuardReduction = FMath::Clamp(Spec.fPower, 0.0f, 0.95f);
        return true;
    }
    if (Ability == EHBAbility::EHBA_Mend)
    {
        const bool bHealed = this->Heal(Spec.fPower) > 0.0f;
        if (bHealed)
        {
            this->Feedback->PlayEffect(FootPosition, 100.0f, FLinearColor(0.15f, 1.0f, 0.45f));
        }
        return bHealed;
    }
    if (Ability == EHBAbility::EHBA_Bolt)
    {
        if (!IsValid(Target) || !Target->IsAlive() || FVector::DistSquaredXY(this->GetActorLocation(), Target->GetActorLocation()) > FMath::Square(Spec.fRange) || !this->HasHitLine(Target->GetActorLocation(), Target))
        {
            return false;
        }
        this->Feedback->PlayEffect(Target->GetActorLocation() - FVector(0.0f, 0.0f, Target->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()), 80.0f, FLinearColor(0.4f, 0.65f, 1.0f));
        UGameplayStatics::ApplyDamage(Target, FMath::Max(0.0f, Spec.fPower), this->GetController(), this, UDamageType::StaticClass());
        return true;
    }
    if (Ability == EHBAbility::EHBA_Pulse)
    {
        if (FVector::DistSquaredXY(this->GetActorLocation(), AimPoint) > FMath::Square(Spec.fRange))
        {
            return false;
        }
        FVector EffectPosition = AimPoint;
        EffectPosition.Z = FootPosition.Z;
        this->Feedback->PlayEffect(EffectPosition, Spec.fRadius, FLinearColor(1.0f, 0.5f, 0.12f));
        const TArray<TWeakObjectPtr<AHBEnemyCharacter>> Targets = Mode->GetEnemies();
        for (const TWeakObjectPtr<AHBEnemyCharacter>& Entry : Targets)
        {
            AHBEnemyCharacter* Enemy = Entry.Get();
            if (IsValid(Enemy) && Enemy->IsAlive() && FVector::DistSquaredXY(AimPoint, Enemy->GetActorLocation()) <= FMath::Square(Spec.fRadius) && this->HasHitLine(Enemy->GetActorLocation(), Enemy))
            {
                UGameplayStatics::ApplyDamage(Enemy, FMath::Max(0.0f, Spec.fPower), this->GetController(), this, UDamageType::StaticClass());
            }
        }
        return true;
    }
    return false;
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
    this->Loadout->ResetResources();
    this->fGuardUntil = 0.0f;
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

FVector AHBPlayerCharacter::CameraInputToWorld(const FVector2D& Input) const
{
    const FRotator CameraYaw(0.0f, this->CameraBoom->GetComponentRotation().Yaw, 0.0f);
    return CameraYaw.RotateVector(FVector(Input.Y, Input.X, 0.0f));
}

void AHBPlayerCharacter::SetCameraYaw(float fYaw)
{
    if (FMath::IsFinite(fYaw))
    {
        this->CameraBoom->SetWorldRotation(FRotator(-60.0f, fYaw, 0.0f));
    }
}

void AHBPlayerCharacter::SetAimWorldDirection(const FVector& Direction)
{
    if (!Direction.ContainsNaN() && !Direction.IsNearlyZero())
    {
        this->AimDirection = Direction.GetSafeNormal2D();
        this->bHasAim = true;
    }
}

float AHBPlayerCharacter::GetAimError() const
{
    return this->bHasAim ? FMath::Abs(FMath::FindDeltaAngleDegrees(this->GetActorRotation().Yaw, this->AimDirection.Rotation().Yaw)) : 0.0f;
}

void AHBPlayerCharacter::EndDodge()
{
    if (this->bDodging)
    {
        this->bDodging = false;
        this->fInvulnerableUntil = 0.0f;
        this->GetCharacterMovement()->MaxWalkSpeed = this->fSavedWalkSpeed;
        this->GetCharacterMovement()->StopMovementImmediately();
    }
}

float AHBPlayerCharacter::TakeDamage(float fDamage, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
    const float fNow = this->GetWorld()->GetTimeSeconds();
    if (this->bDodging && fNow < this->fInvulnerableUntil)
    {
        return 0.0f;
    }
    const float fMultiplier = fNow < this->fGuardUntil ? 1.0f - this->fGuardReduction : 1.0f;
    // Taking a non-lethal hit never cancels a montage or its pending command.
    return Super::TakeDamage(fDamage * fMultiplier, DamageEvent, EventInstigator, DamageCauser);
}

bool AHBPlayerCharacter::HasHitLine(const FVector& Point, const AActor* Target) const
{
    FCollisionQueryParams Parameters(SCENE_QUERY_STAT(HBAttackLine), false, this);
    FHitResult Hit;
    const bool bBlocked = this->GetWorld()->LineTraceSingleByChannel(Hit, this->GetActorLocation(), Point, ECC_Visibility, Parameters);
    return !bBlocked || Hit.GetActor() == Target;
}

void AHBPlayerCharacter::OnAttackTimeout()
{
    this->LastAttackOutcome = this->AttackMontage ? TEXT("Watchdog timeout") : TEXT("Timed attack completed");
    UE_LOG(LogHideInTheBox, Log, TEXT("Attack ended: %s"), *this->LastAttackOutcome);
    this->FinishAttack();
}

void AHBPlayerCharacter::BeginPlay()
{
    Super::BeginPlay();
    const UAnimInstance* Anim = this->GetMesh()->GetAnimInstance();
    UE_LOG(LogHideInTheBox, Log, TEXT("Runtime identity: pawn=%s class=%s anim=%s montage=%s"), *this->GetName(), *this->GetClass()->GetName(), Anim ? *Anim->GetClass()->GetName() : TEXT("NONE"), *GetNameSafe(this->AttackMontage));
    if (this->AttackMontage && !Anim)
    {
        UE_LOG(LogHideInTheBox, Warning, TEXT("Attack montage is assigned without an AnimInstance. Wire mesh and Anim Class before testing damage."));
    }
}
