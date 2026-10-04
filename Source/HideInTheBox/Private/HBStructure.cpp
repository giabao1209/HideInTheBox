#include "HBStructure.h"
#include "HBGameMode.h"
#include "HBEnemyCharacter.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/DamageType.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "TimerManager.h"

AHBStructure::AHBStructure()
{
    this->PrimaryActorTick.bCanEverTick = false;
    this->Mesh = this->CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StructureMesh"));
    this->SetRootComponent(this->Mesh);
    // Arena structures are objectives, not the GDD weighted navigation obstacles.
    this->Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> MeshAsset(TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (MeshAsset.Succeeded())
    {
        this->Mesh->SetStaticMesh(MeshAsset.Object);
    }
}

void AHBStructure::BeginPlay()
{
    Super::BeginPlay();
    this->Mode = this->GetWorld()->GetAuthGameMode<AHBGameMode>();
}

void AHBStructure::Configure(EHBStructure NewKind)
{
    this->Kind = NewKind;
    const bool bTurret = this->Kind == EHBStructure::EHBS_Turret;
    this->fMaxHealth = this->Kind == EHBStructure::EHBS_Anima ? 250.0f : 350.0f;
    this->fHealth = this->fMaxHealth;
    this->Mesh->SetWorldScale3D(bTurret ? FVector(0.8f, 0.8f, 1.8f) : FVector(1.5f, 2.5f, 1.5f));
    UMaterialInstanceDynamic* Material = this->Mesh->CreateAndSetMaterialInstanceDynamic(0);
    if (Material)
    {
        Material->SetVectorParameterValue(TEXT("Color"), bTurret ? FLinearColor::Yellow : FLinearColor(0.2f, 0.8f, 0.9f));
    }
    if (bTurret)
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
