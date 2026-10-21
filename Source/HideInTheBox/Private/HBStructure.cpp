#include "HBStructure.h"
#include "HBDebugSubsystem.h"
#include "HBGameMode.h"
#include "HBEnemyCharacter.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/DamageType.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "TimerManager.h"

AHBStructure::AHBStructure()
{
    this->PrimaryActorTick.bCanEverTick = false;
    this->Mesh = this->CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StructureMesh"));
    this->SetRootComponent(this->Mesh);
    // Arena structures are objectives, not the GDD weighted navigation obstacles.
    this->Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AHBStructure::BeginPlay()
{
    Super::BeginPlay();
    this->Mode = this->GetWorld()->GetAuthGameMode<AHBGameMode>();
}

void AHBStructure::Configure(EHBStructure NewKind)
{
    this->GetWorldTimerManager().ClearTimer(this->FireTimer);
    this->Kind = NewKind;
    this->fMaxHealth = this->Kind == EHBStructure::EHBS_Anima ? 250.0f : 350.0f;
    this->fHealth = this->fMaxHealth;
    if (this->Kind == EHBStructure::EHBS_Turret)
    {
        this->GetWorldTimerManager().SetTimer(this->FireTimer, this, &AHBStructure::Fire, 0.7f, true);
    }
}

float AHBStructure::TakeDamage(float fDamage, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
    if (this->Kind == EHBStructure::EHBS_Turret || !this->IsAlive() || fDamage <= 0.0f)
    {
        return 0.0f;
    }
    const float fApplied = FMath::Min(fDamage, this->fHealth);
    this->fHealth -= fApplied;
    UHBDebugSubsystem::Record(this, TEXT("Combat"), FString::Printf(TEXT("Structure %s damage=%.1f HP=%.1f"), *this->GetName(), fApplied, this->fHealth));
    if (!this->IsAlive())
    {
        this->SetActorHiddenInGame(true);
        if (this->Kind == EHBStructure::EHBS_Anima && this->Mode.IsValid())
        {
            this->Mode->EndRun();
        }
    }
    return fApplied;
}

void AHBStructure::Repair()
{
    this->fHealth = this->fMaxHealth;
    this->SetActorHiddenInGame(false);
}

void AHBStructure::Fire()
{
    if (!this->Mode.IsValid() || this->Mode->GetPhase() != EHBPhase::EHBP_Defense)
    {
        this->Target.Reset();
        return;
    }
    AHBEnemyCharacter* Enemy = this->Target.Get();
    constexpr float fRange = 750.0f;
    if (!IsValid(Enemy) || !Enemy->IsAlive() || FVector::DistSquaredXY(this->GetActorLocation(), Enemy->GetActorLocation()) > FMath::Square(fRange))
    {
        Enemy = this->Mode->FindEnemy(this->GetActorLocation(), FVector::ZeroVector, fRange);
        this->Target = Enemy;
    }
    if (IsValid(Enemy))
    {
        UGameplayStatics::ApplyDamage(Enemy, 22.0f, nullptr, this, UDamageType::StaticClass());
    }
}

void AHBStructure::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    this->GetWorldTimerManager().ClearTimer(this->FireTimer);
    Super::EndPlay(EndPlayReason);
}

void AHBStructure::SetTintMaterial(UMaterialInterface* NewMaterial)
{
    if (NewMaterial)
    {
        this->Mesh->SetMaterial(0, NewMaterial);
    }
}
