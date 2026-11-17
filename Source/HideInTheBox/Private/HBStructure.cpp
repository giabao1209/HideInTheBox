#include "HBStructure.h"
#include "HBAttributes.h"
#include "AbilitySystemGlobals.h"
#include "HBEffects.h"
#include "HBGameMode.h"
#include "HBEnemyCharacter.h"
#include "HBArenaAbilities.h"
#include "Misc/PackageName.h"
#include "UObject/ConstructorHelpers.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "TimerManager.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(StructureServiceCue, "GameplayCue.GASDemo.Structure.Service");

AHBStructure::AHBStructure()
{
    this->PrimaryActorTick.bCanEverTick = false;
    this->AbilitySystem = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystem"));
    this->Attributes = CreateDefaultSubobject<UHBAttributes>(TEXT("Attributes"));
    this->Mesh = this->CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StructureMesh"));
    this->SetRootComponent(this->Mesh);
    // Arena structures are objectives, not the GDD weighted navigation obstacles.
    this->Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    FenceBlocker = CreateDefaultSubobject<UBoxComponent>(TEXT("FenceBlocker"));
    FenceBlocker->SetupAttachment(Mesh);
    FenceBlocker->SetRelativeLocation(FVector(0.0, 0.0, 40.0));
    FenceBlocker->InitBoxExtent(FVector(72.0, 24.0, 40.0));
    FenceBlocker->SetCollisionObjectType(ECC_WorldDynamic);
    FenceBlocker->SetCollisionResponseToAllChannels(ECR_Ignore);
    FenceBlocker->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
    FenceBlocker->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    BuildModel = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BuildModel"));
    TurretHead = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TurretHead"));
    TurretBarrel = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TurretBarrel"));
    BuildModel->SetupAttachment(Mesh);
    TurretHead->SetupAttachment(Mesh);
    TurretBarrel->SetupAttachment(TurretHead);
    for (UStaticMeshComponent* Visual : { BuildModel.Get(), TurretHead.Get(), TurretBarrel.Get() })
    {
        Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Visual->SetCanEverAffectNavigation(false);
        Visual->SetVisibility(false);
    }
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Base(TEXT("/Game/DemoArena/Art/Meshes/SM_Bastion/StaticMeshes/SM_Bastion.SM_Bastion"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Fence(TEXT("/Game/DemoArena/Art/Meshes/SM_ArenaFence/StaticMeshes/SM_ArenaFence.SM_ArenaFence"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Head(TEXT("/Game/DemoArena/Art/Meshes/SM_ArenaTurret/StaticMeshes/weapon-turret.weapon-turret"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Barrel(TEXT("/Game/DemoArena/Art/Meshes/SM_ArenaTurret/StaticMeshes/barrel.barrel"));
    TurretModel = Base.Object;
    FenceModel = Fence.Object;
    TurretHead->SetStaticMesh(Head.Object);
    TurretBarrel->SetStaticMesh(Barrel.Object);
    TurretHead->SetRelativeLocation(FVector(0.0, 0.0, 76.0));
    TurretBarrel->SetRelativeLocation(FVector(0.0, 0.0, 23.0));
    TurretAbilityClass = UHBGA_TurretFire::StaticClass();
    if (FPackageName::DoesPackageExist(TEXT("/Game/GASDemo/GAS/Abilities/GA_GASDemo_TurretFire")))
    {
        static ConstructorHelpers::FClassFinder<UHBGA_TurretFire> Shot(TEXT("/Game/GASDemo/GAS/Abilities/GA_GASDemo_TurretFire"));
        if (Shot.Succeeded()) { TurretAbilityClass = Shot.Class; }
    }
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
    if (FireAbility.IsValid())
    {
        AbilitySystem->ClearAbility(FireAbility);
        FireAbility = FGameplayAbilitySpecHandle();
    }
    Target.Reset();
    if (Kind != NewKind)
    {
        AbilitySystem->RemoveActiveEffectsWithGrantedTags(FGameplayTagContainer(HBArenaTags::TurretUpgraded));
    }
    this->Kind = NewKind;
    const bool bTurret = this->Kind == EHBStructure::EHBS_Turret;
    const bool bFence = IsFence();
    UStaticMesh* Model = bTurret ? TurretModel.Get() : (bFence ? FenceModel.Get() : nullptr);
    BuildModel->SetStaticMesh(Model);
    BuildModel->SetRelativeScale3D(bFence ? FVector(1.44, 0.48, 1.6) : FVector(1.3));
    BuildModel->SetVisibility(Model != nullptr);
    if (bTurret || bFence)
    {
        Mesh->SetVisibility(Model == nullptr);
    }
    TurretHead->SetVisibility(bTurret);
    TurretBarrel->SetVisibility(bTurret);
    FenceBlocker->SetCollisionEnabled(bFence ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
    const float MaxHealth = bFence ? 220.0f : (bTurret ? 145.0f : (this->Kind == EHBStructure::EHBS_Anima ? 250.0f : 350.0f));
    this->AbilitySystem->SetNumericAttributeBase(UHBAttributes::GetMaxHealthAttribute(), MaxHealth);
    this->AbilitySystem->SetNumericAttributeBase(UHBAttributes::GetHealthAttribute(), MaxHealth);
    if (bTurret && TurretAbilityClass)
    {
        FireAbility = AbilitySystem->GiveAbility(FGameplayAbilitySpec(TurretAbilityClass, 1));
        this->GetWorldTimerManager().SetTimer(this->FireTimer, this, &AHBStructure::Fire, 0.15f, true);
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
    return GetHealth() < GetMaxHealth();
}

float AHBStructure::GetMaxHealth() const
{
    return Attributes->GetMaxHealth();
}

bool AHBStructure::IsUpgraded() const
{
    return AbilitySystem->HasMatchingGameplayTag(HBArenaTags::TurretUpgraded);
}

float AHBStructure::GetFireRange() const
{
    return IsUpgraded() ? 900.0f : 750.0f;
}

bool AHBStructure::CanFire() const
{
    return Kind == EHBStructure::EHBS_Turret && IsAlive() && Mode.IsValid() && Mode->GetPhase() == EHBPhase::EHBP_Defense;
}

void AHBStructure::OnHealthChanged(const FOnAttributeChangeData& Data)
{
    if (HasAuthority() && Data.OldValue <= 0.0f && Data.NewValue > 0.0f)
    {
        SetActorHiddenInGame(false);
    }
    if (HasAuthority() && Data.OldValue > 0.0f && Data.NewValue <= 0.0f)
    {
        this->SetActorHiddenInGame(true);
        if (IsTurret() || IsFence())
        {
            Destroy();
            return;
        }
        if (this->Kind == EHBStructure::EHBS_Anima && this->Mode.IsValid())
        {
            this->Mode->EndRun();
        }
    }
}

void AHBStructure::Repair()
{
    ApplyRepair(this);
}

bool AHBStructure::ApplyRepair(AActor* Source)
{
    if (!HasAuthority() || !NeedsRepair())
    {
        return false;
    }
    UAbilitySystemComponent* SourceASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Source);
    if (!SourceASC) { return false; }
    const FGameplayEffectSpecHandle Effect = SourceASC->MakeOutgoingSpec(UHBGE_HealthDelta::StaticClass(), 1.0f, SourceASC->MakeEffectContext());
    if (!Effect.IsValid()) { return false; }
    const float PreviousHealth = GetHealth();
    Effect.Data->SetSetByCallerMagnitude(FName(TEXT("Magnitude")), GetMaxHealth() - PreviousHealth);
    SourceASC->ApplyGameplayEffectSpecToTarget(*Effect.Data, AbilitySystem);
    return GetHealth() > PreviousHealth;
}

bool AHBStructure::ApplyUpgrade(AActor* Source)
{
    if (!HasAuthority() || Kind != EHBStructure::EHBS_Turret || !IsAlive() || IsUpgraded())
    {
        return false;
    }
    UAbilitySystemComponent* SourceASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Source);
    if (!SourceASC) { return false; }
    const FGameplayEffectSpecHandle Upgrade = SourceASC->MakeOutgoingSpec(UHBGE_TurretUpgrade::StaticClass(), 1.0f, SourceASC->MakeEffectContext());
    if (!Upgrade.IsValid()) { return false; }
    Upgrade.Data->DynamicGrantedTags.AddTag(HBArenaTags::TurretUpgraded);
    return SourceASC->ApplyGameplayEffectSpecToTarget(*Upgrade.Data, AbilitySystem).IsValid();
}

void AHBStructure::PlayServiceCue()
{
    FGameplayCueParameters Cue;
    Cue.Location = GetActorLocation();
    Cue.RawMagnitude = 100.0f;
    AbilitySystem->ExecuteGameplayCue(StructureServiceCue, Cue);
}

void AHBStructure::Fire()
{
    if (!CanFire())
    {
        this->Target.Reset();
        return;
    }
    AHBEnemyCharacter* Enemy = this->Target.Get();
    const float fRange = GetFireRange();
    if (!IsValid(Enemy) || !Enemy->IsAlive() || FVector::DistSquaredXY(this->GetActorLocation(), Enemy->GetActorLocation()) > FMath::Square(fRange))
    {
        Enemy = this->Mode->FindEnemy(this->GetActorLocation(), FVector::ZeroVector, fRange);
        this->Target = Enemy;
    }
    if (IsValid(Enemy))
    {
        TurretHead->SetWorldRotation(FRotator(0.0, (Enemy->GetActorLocation() - GetActorLocation()).Rotation().Yaw, 0.0));
        FGameplayEventData Event;
        Event.EventTag = HBArenaTags::TurretFire;
        Event.Instigator = this;
        Event.Target = Enemy;
        AbilitySystem->HandleGameplayEvent(Event.EventTag, &Event);
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
