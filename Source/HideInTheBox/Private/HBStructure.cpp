#include "HBStructure.h"
#include "HBAttributes.h"
#include "HBEffects.h"
#include "HBGameMode.h"
#include "HBEnemyCharacter.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"

AHBStructure::AHBStructure()
{
    this->PrimaryActorTick.bCanEverTick = false;
    this->AbilitySystem = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystem"));
    this->Attributes = CreateDefaultSubobject<UHBAttributes>(TEXT("Attributes"));
    this->Mesh = this->CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StructureMesh"));
    this->SetRootComponent(this->Mesh);
    // Arena structures are objectives, not the GDD weighted navigation obstacles.
    this->Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AHBStructure::BeginPlay()
{
    Super::BeginPlay();
    this->Mode = this->GetWorld()->GetAuthGameMode<AHBGameMode>();
    this->AbilitySystem->InitAbilityActorInfo(this, this);
    this->AbilitySystem->GetGameplayAttributeValueChangeDelegate(UHBAttributes::GetHealthAttribute())
        .AddUObject(this, &AHBStructure::OnHealthChanged);
    this->Configure(this->Kind);
}

void AHBStructure::Configure(EHBStructure NewKind)
{
    if (!HasAuthority())
    {
        return;
    }
    this->GetWorldTimerManager().ClearTimer(this->FireTimer);
    this->Kind = NewKind;
    const bool bTurret = this->Kind == EHBStructure::EHBS_Turret;
    const FGameplayTag ImmuneTag = FGameplayTag::RequestGameplayTag(TEXT("State.DamageImmune"));
    this->AbilitySystem->SetLooseGameplayTagCount(ImmuneTag, bTurret ? 1 : 0);
    const float MaxHealth = this->Kind == EHBStructure::EHBS_Anima ? 250.0f : 350.0f;
    this->AbilitySystem->SetNumericAttributeBase(UHBAttributes::GetMaxHealthAttribute(), MaxHealth);
    this->AbilitySystem->SetNumericAttributeBase(UHBAttributes::GetHealthAttribute(), MaxHealth);
    if (bTurret)
    {
        this->GetWorldTimerManager().SetTimer(this->FireTimer, this, &AHBStructure::Fire, 0.7f, true);
    }
}

float AHBStructure::TakeDamage(float fDamage, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
    return HBApplyDamageEffect(DamageCauser, this, fDamage);
}

UAbilitySystemComponent* AHBStructure::GetAbilitySystemComponent() const
{
    return this->AbilitySystem;
}

float AHBStructure::GetHealth() const
{
    return this->Attributes->GetHealth();
}

bool AHBStructure::NeedsRepair() const
{
    return GetHealth() < this->Attributes->GetMaxHealth();
}

void AHBStructure::OnHealthChanged(const FOnAttributeChangeData& Data)
{
    if (HasAuthority() && Data.OldValue > 0.0f && Data.NewValue <= 0.0f)
    {
        this->SetActorHiddenInGame(true);
        if (this->Kind == EHBStructure::EHBS_Anima && this->Mode.IsValid())
        {
            this->Mode->EndRun();
        }
    }
}

void AHBStructure::Repair()
{
    if (HasAuthority())
    {
        this->AbilitySystem->SetNumericAttributeBase(UHBAttributes::GetHealthAttribute(), this->Attributes->GetMaxHealth());
        this->SetActorHiddenInGame(false);
    }
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
        HBApplyDamageEffect(this, Enemy, 22.0f);
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
