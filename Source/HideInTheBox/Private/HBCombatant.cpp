#include "HBCombatant.h"
#include "HBAttributes.h"
#include "HBEffects.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "HBDebugSubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"

AHBCombatant::AHBCombatant()
{
    bReplicates = true;
    AbilitySystem = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystem"));
    AbilitySystem->SetIsReplicated(true);
    AbilitySystem->SetReplicationMode(EGameplayEffectReplicationMode::Full);
    Attributes = CreateDefaultSubobject<UHBAttributes>(TEXT("Attributes"));
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
    AbilitySystem->InitAbilityActorInfo(this, this);
    AbilitySystem->GetGameplayAttributeValueChangeDelegate(UHBAttributes::GetHealthAttribute()).AddUObject(this, &AHBCombatant::OnHealthAttributeChanged);
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

UAbilitySystemComponent* AHBCombatant::GetAbilitySystemComponent() const
{
    return AbilitySystem;
}

float AHBCombatant::GetHealth() const
{
    return Attributes ? Attributes->GetHealth() : 0.0f;
}

float AHBCombatant::GetMaxHealth() const
{
    return Attributes ? Attributes->GetMaxHealth() : fMaxHealth;
}

float AHBCombatant::ApplyHealthDelta(float Delta)
{
    if (!HasAuthority() || !AbilitySystem || !FMath::IsFinite(Delta) || FMath::IsNearlyZero(Delta))
    {
        return 0.0f;
    }
    const float OldHealth = GetHealth();
    FGameplayEffectSpecHandle Spec = AbilitySystem->MakeOutgoingSpec(UHBGE_HealthDelta::StaticClass(), 1.0f, AbilitySystem->MakeEffectContext());
    if (!Spec.IsValid())
    {
        return 0.0f;
    }
    Spec.Data->SetSetByCallerMagnitude(FName(TEXT("Magnitude")), Delta);
    AbilitySystem->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
    return GetHealth() - OldHealth;
}

float AHBCombatant::TakeDamage(float Damage, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
    if (!IsAlive() || !FMath::IsFinite(Damage) || Damage <= 0.0f || !HasAuthority())
    {
        return 0.0f;
    }
    const float Applied = FMath::Min(Damage, GetHealth());
    UHBDebugSubsystem::Record(this, TEXT("Combat"),
        FString::Printf(TEXT("GAS %s -> %s damage=%.1f HP=%.1f"),
            *GetNameSafe(DamageCauser), *GetName(), Applied, FMath::Max(0.0f, GetHealth() - Applied)));
    ApplyHealthDelta(-Applied);
    return Applied;
}

void AHBCombatant::RestoreHealth()
{
    if (HasAuthority() && AbilitySystem)
    {
        AbilitySystem->SetNumericAttributeBase(UHBAttributes::GetMaxHealthAttribute(), FMath::Max(1.0f, fMaxHealth));
        AbilitySystem->SetNumericAttributeBase(UHBAttributes::GetHealthAttribute(), FMath::Max(1.0f, fMaxHealth));
    }
}

float AHBCombatant::Heal(float Amount)
{
    if (!IsAlive() || !FMath::IsFinite(Amount) || Amount <= 0.0f)
    {
        return 0.0f;
    }
    return FMath::Max(0.0f, ApplyHealthDelta(Amount));
}

void AHBCombatant::OnHealthAttributeChanged(const FOnAttributeChangeData& Data)
{
    if (HasAuthority() && Data.OldValue > 0.0f && Data.NewValue <= 0.0f)
    {
        OnDeath();
    }
}

void AHBCombatant::OnDeath()
{
    Destroy();
}
