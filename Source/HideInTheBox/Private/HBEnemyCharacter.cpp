#include "HBEnemyCharacter.h"
#include "HBGameMode.h"
#include "HBPlayerCharacter.h"
#include "HBStructure.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/DamageType.h"
#include "Kismet/GameplayStatics.h"
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
        UGameplayStatics::ApplyDamage(Target, 8.0f, this->GetController(), this, UDamageType::StaticClass());
    }
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
