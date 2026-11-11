#include "HBEnemyCharacter.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "HBGameMode.h"
#include "HBDebugSubsystem.h"
#include "HBPracticeWidget.h"
#include "Components/WidgetComponent.h"
#include "HBPlayerCharacter.h"
#include "HBStructure.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HBEffects.h"
#include "Engine/World.h"

AHBEnemyCharacter::AHBEnemyCharacter()
{
    this->PrimaryActorTick.bCanEverTick = true;
    this->PrimaryActorTick.TickInterval = 0.0f;
    this->GetCharacterMovement()->MaxWalkSpeed = 180.0f;
    this->fMaxHealth = 90.0f;
    this->Tint = FLinearColor(0.8f, 0.15f, 0.12f);
}

void AHBEnemyCharacter::BeginPlay()
{
    Super::BeginPlay();
    this->Mode = this->GetWorld()->GetAuthGameMode<AHBGameMode>();
    if (this->Mode.IsValid())
    {
        this->Mode->RegisterEnemy(this);
    }
    if (PracticeHealthWidgetClass && GetNetMode() != NM_DedicatedServer)
    {
        UWidgetComponent* HealthDisplay = NewObject<UWidgetComponent>(this, TEXT("PracticeHealthDisplay"));
        HealthDisplay->SetupAttachment(GetRootComponent());
        HealthDisplay->SetRelativeLocation(FVector(0.0f, 0.0f, 112.0f));
        HealthDisplay->SetWidgetSpace(EWidgetSpace::Screen);
        HealthDisplay->SetDrawSize(FVector2D(116.0, 24.0));
        HealthDisplay->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        HealthDisplay->SetWidgetClass(PracticeHealthWidgetClass);
        HealthDisplay->RegisterComponent();
        HealthDisplay->InitWidget();
        if (UHBGASTargetWidget* Widget = Cast<UHBGASTargetWidget>(HealthDisplay->GetUserWidgetObject()))
        {
            Widget->SetCombatant(this);
        }
    }
}

void AHBEnemyCharacter::Tick(float fDeltaSeconds)
{
    Super::Tick(fDeltaSeconds);
    if (!this->Mode.IsValid() || !this->IsAlive())
    {
        return;
    }
    const EHBPhase Phase = this->Mode->GetPhase();
    if (Phase != EHBPhase::EHBP_Dungeon && Phase != EHBPhase::EHBP_Defense)
    {
        return;
    }
    const UAnimInstance* Anim = AttackMontage ? GetMesh()->GetAnimInstance() : nullptr;
    if (Anim && Anim->Montage_IsActive(AttackMontage))
    {
        return;
    }
    AHBPlayerCharacter* Player = this->Mode->GetPlayer();
    AActor* Target = nullptr;
    float fRange = 125.0f;
    if (IsValid(Player) && Player->IsAlive() && (Phase == EHBPhase::EHBP_Dungeon || FVector::DistSquaredXY(this->GetActorLocation(), Player->GetActorLocation()) < FMath::Square(230.0f)))
    {
        Target = Player;
    }
    else if (Phase == EHBPhase::EHBP_Defense)
    {
        Target = this->Mode->GetDefenseTarget();
        fRange = 160.0f;
    }
    if (!IsValid(Target))
    {
        return;
    }
    if (Phase == EHBPhase::EHBP_Dungeon && FVector::DistSquaredXY(this->GetActorLocation(), Target->GetActorLocation()) > FMath::Square(700.0f))
    {
        return;
    }
    FVector Direction = Target->GetActorLocation() - this->GetActorLocation();
    Direction.Z = 0.0f;
    this->SetActorRotation(Direction.Rotation());
    if (Direction.SizeSquared() > FMath::Square(fRange))
    {
        this->AddMovementInput(Direction.GetSafeNormal(), 1.0f, true);
        return;
    }
    this->GetCharacterMovement()->StopMovementImmediately();
    if (this->GetWorld()->GetTimeSeconds() >= this->fNextAttackTime)
    {
        this->fNextAttackTime = this->GetWorld()->GetTimeSeconds() + 1.0f;
        if (AttackMontage)
        {
            PendingAttackTarget = Target;
            PendingAttackRange = fRange;
            if (PlayAnimMontage(AttackMontage) > 0.0f)
            {
                return;
            }
            PendingAttackTarget.Reset();
        }
        ApplyMeleeDamage(Target);
    }
}

void AHBEnemyCharacter::DeliverAnimationHit(UAnimSequenceBase* Animation)
{
    const UAnimInstance* Anim = GetMesh()->GetAnimInstance();
    if (!AttackMontage || Animation != AttackMontage || !Anim || !Anim->Montage_IsActive(AttackMontage) ||
        !HasAuthority() || !IsAlive() || !Mode.IsValid())
    {
        return;
    }

    AActor* Target = PendingAttackTarget.Get();
    PendingAttackTarget.Reset();
    if (IsValid(Target) && FVector::DistSquaredXY(GetActorLocation(), Target->GetActorLocation()) <= FMath::Square(PendingAttackRange))
    {
        ApplyMeleeDamage(Target);
        UHBDebugSubsystem::Record(this, TEXT("Combat"), TEXT("Enemy melee hit via montage notify"));
    }
}

void AHBEnemyCharacter::ApplyMeleeDamage(AActor* Target)
{
    HBApplyDamageEffect(this, Target, 8.0f);
}

void AHBEnemyCharacter::OnDeath()
{
    if (this->Mode.IsValid())
    {
        this->Mode->NotifyEnemyKilled(this);
    }
    Super::OnDeath();
}

void AHBEnemyCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (this->Mode.IsValid())
    {
        this->Mode->UnregisterEnemy(this);
    }
    Super::EndPlay(EndPlayReason);
}
