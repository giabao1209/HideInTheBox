#include "HBCombatant.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"

AHBCombatant::AHBCombatant()
{
    this->GetCapsuleComponent()->InitCapsuleSize(34.0f, 88.0f);
    this->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    this->GetCharacterMovement()->bOrientRotationToMovement = false;
    this->GetCharacterMovement()->bRunPhysicsWithNoController = true;
    this->GetCharacterMovement()->MaxWalkSpeed = 450.0f;
    this->GetCharacterMovement()->MinAnalogWalkSpeed = 0.0f;
    this->GetCharacterMovement()->BrakingDecelerationWalking = 2400.0f;
    this->bUseControllerRotationYaw = false;
    this->Placeholder = this->CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Placeholder"));
    this->Placeholder->SetupAttachment(this->GetRootComponent());
    this->Placeholder->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    this->Placeholder->SetRelativeScale3D(FVector(0.5f, 0.5f, 1.3f));
    this->FacingMarker = this->CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FacingMarker"));
    this->FacingMarker->SetupAttachment(this->GetRootComponent());
    this->FacingMarker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    this->FacingMarker->SetRelativeLocation(FVector(38.0f, 0.0f, 25.0f));
    this->FacingMarker->SetRelativeScale3D(FVector(0.3f, 0.12f, 0.12f));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> MeshAsset(TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (MeshAsset.Succeeded())
    {
        this->Placeholder->SetStaticMesh(MeshAsset.Object);
        this->FacingMarker->SetStaticMesh(MeshAsset.Object);
    }
}

void AHBCombatant::BeginPlay()
{
    Super::BeginPlay();
    this->RestoreHealth();
    if (this->GetMesh()->GetSkeletalMeshAsset())
    {
        this->Placeholder->SetVisibility(false);
        this->FacingMarker->SetVisibility(false);
    }
    if (this->TintMaterial)
    {
        this->Placeholder->SetMaterial(0, this->TintMaterial);
    }
    UMaterialInstanceDynamic* Material = this->Placeholder->CreateAndSetMaterialInstanceDynamic(0);
    if (Material)
    {
        Material->SetVectorParameterValue(TEXT("Color"), this->Tint);
    }
}

float AHBCombatant::TakeDamage(float fDamage, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
    if (!this->IsAlive() || !FMath::IsFinite(fDamage) || fDamage <= 0.0f)
    {
        return 0.0f;
    }
    const float fApplied = FMath::Min(fDamage, this->fHealth);
    this->fHealth -= fApplied;
    if (!this->IsAlive())
    {
        this->OnDeath();
    }
    return fApplied;
}

void AHBCombatant::RestoreHealth()
{
    this->fHealth = this->fMaxHealth;
}

void AHBCombatant::OnDeath()
{
    this->Destroy();
}

float AHBCombatant::Heal(float fAmount)
{
    if (!this->IsAlive() || !FMath::IsFinite(fAmount) || fAmount <= 0.0f)
    {
        return 0.0f;
    }
    const float fRestored = FMath::Min(fAmount, this->fMaxHealth - this->fHealth);
    this->fHealth += fRestored;
    return fRestored;
}
