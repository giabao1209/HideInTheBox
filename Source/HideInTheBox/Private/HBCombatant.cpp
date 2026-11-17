#include "HBCombatant.h"
#include "HBAttributes.h"
#include "HBEffects.h"
#include "HBPlayerCharacter.h"
#include "HBEnemyCharacter.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "GameplayEffect.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"

AHBCombatant::AHBCombatant()
{
    bReplicates = true;
    HealthEffectClass = UHBGE_HealthDelta::StaticClass();
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
    if (GetWorld()->GetMapName().EndsWith(TEXT("DemoArena")) && GetNetMode() != NM_DedicatedServer)
    {
        ConfigureArenaArt();
    }
}

void AHBCombatant::ConfigureArenaArt()
{
    const bool bKnight = IsA<AHBPlayerCharacter>();
    if (!bKnight && !IsA<AHBEnemyCharacter>()) { return; }
    const FString Name = bKnight ? TEXT("SK_VaultKnight") : TEXT("SK_VaultSkeleton");
    const FString Folder = TEXT("/Game/DemoArena/Art/Characters/") + Name;
    const FString MeshFolder = Folder + TEXT("/SkeletalMeshes/");
    USkeletalMesh* Asset = LoadObject<USkeletalMesh>(nullptr, *(MeshFolder + Name));
    if (!Asset || !GetMesh()->GetAnimInstance()) { return; }

    const FString ClipPrefix = MeshFolder + Name;
    ArenaIdle = LoadObject<UAnimSequence>(nullptr, *(ClipPrefix + TEXT("Idle")));
    ArenaRun = LoadObject<UAnimSequence>(nullptr, *(ClipPrefix + TEXT("Running_A")));
    ArenaMelee = LoadObject<UAnimSequence>(nullptr, *(ClipPrefix + TEXT("1H_Melee_Attack_Slice_Horizontal")));
    ArenaCast = LoadObject<UAnimSequence>(nullptr, *(ClipPrefix + (bKnight ? TEXT("Spellcast_Long") : TEXT("1H_Melee_Attack_Jump_Chop"))));
    ArenaDodge = bKnight ? LoadObject<UAnimSequence>(nullptr, *(ClipPrefix + TEXT("Dodge_Forward"))) : nullptr;
    if (!ArenaIdle || !ArenaRun || !ArenaMelee || !ArenaCast) { return; }

    ArenaMesh = NewObject<USkeletalMeshComponent>(this, TEXT("ArenaCharacterArt"));
    ArenaMesh->SetupAttachment(GetRootComponent());
    ArenaMesh->SetSkeletalMeshAsset(Asset);
    ArenaMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    ArenaMesh->SetCanEverAffectNavigation(false);
    ArenaMesh->SetRelativeLocation(FVector(0.0, 0.0, -GetCapsuleComponent()->GetScaledCapsuleHalfHeight()));
    ArenaMesh->SetRelativeRotation(FRotator(0.0, -90.0, 0.0));
    ArenaMesh->SetRelativeScale3D(FVector(bKnight ? 0.78 : 0.8));
    ArenaMesh->RegisterComponent();
    ArenaMesh->PlayAnimation(ArenaIdle, true);
    CurrentArenaClip = ArenaIdle;

    if (bKnight)
    {
        AttachArenaAccessory(TEXT("/Game/DemoArena/Art/Meshes/SM_VaultKnightHelmet/StaticMeshes/SM_VaultKnightHelmet"),
            TEXT("head"), FTransform(FRotator::ZeroRotator, FVector(0, 0, -5.5), FVector(0.99, 0.98, 0.96)));
        AttachArenaAccessory(TEXT("/Game/DemoArena/Art/Meshes/SM_VaultKnightSword/StaticMeshes/SM_VaultKnightSword"),
            TEXT("handslot_r"), FTransform(FRotator(0, 180, 0), FVector(0, 0, 3.33), FVector(1.05)));
        AttachArenaAccessory(TEXT("/Game/DemoArena/Art/Meshes/SM_VaultKnightShield/StaticMeshes/SM_VaultKnightShield"),
            TEXT("handslot_l"), FTransform(FRotator::ZeroRotator, FVector(0, 15.59, 1.70), FVector(0.83)));
        AttachArenaAccessory(TEXT("/Game/DemoArena/Art/Meshes/SM_VaultKnightCape/StaticMeshes/SM_VaultKnightCape"),
            TEXT("chest"), FTransform(FVector(0, 0, 24.33)));
    }
    else
    {
        AttachArenaAccessory(TEXT("/Game/DemoArena/Art/Meshes/SM_VaultSkeletonHelmet/StaticMeshes/SM_VaultSkeletonHelmet"),
            TEXT("head"), FTransform(FRotator::ZeroRotator, FVector(0, 0, -8.4), FVector(0.94, 0.97, 0.91)));
    }
    ApplyArenaFinish(LoadObject<UMaterialInterface>(nullptr, bKnight
        ? TEXT("/Game/DemoArena/Art/Characters/Finishes/M_VaultKnightFinish")
        : TEXT("/Game/DemoArena/Art/Characters/Finishes/M_VaultSkeletonFinish")));

    // The gameplay mesh still drives GAS montage timing and hit notifies.
    GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
    GetMesh()->SetVisibility(false);
}

void AHBCombatant::ApplyArenaFinish(UMaterialInterface* Finish)
{
    if (!ArenaMesh || !Finish) { return; }
    for (int32 Index = 0; Index < ArenaMesh->GetNumMaterials(); ++Index)
    {
        ArenaMesh->SetMaterial(Index, Finish);
    }
    for (USceneComponent* Child : ArenaMesh->GetAttachChildren())
    {
        if (UStaticMeshComponent* Accessory = Cast<UStaticMeshComponent>(Child))
        {
            if (Accessory->GetStaticMesh() && Accessory->GetStaticMesh()->GetFName() == TEXT("SM_VaultKnightSword"))
            {
                if (UMaterialInterface* Steel = LoadObject<UMaterialInterface>(nullptr,
                    TEXT("/Game/DemoArena/Art/Characters/Finishes/M_VaultSwordSteel")))
                {
                    Accessory->SetMaterial(0, Steel);
                }
                continue;
            }
            Accessory->SetMaterial(0, Finish);
        }
    }
}

void AHBCombatant::ApplyArenaEliteFinish()
{
    if (!ArenaMesh) { return; }
    ApplyArenaFinish(LoadObject<UMaterialInterface>(nullptr,
        TEXT("/Game/DemoArena/Art/Characters/Finishes/MI_VaultEliteFinish")));
    ArenaMesh->SetRelativeScale3D(FVector(0.91));
}

void AHBCombatant::AttachArenaAccessory(const TCHAR* AssetPath, FName Bone, const FTransform& RelativeTransform)
{
    UStaticMesh* MeshAsset = LoadObject<UStaticMesh>(nullptr, AssetPath);
    if (!MeshAsset || ArenaMesh->GetBoneIndex(Bone) == INDEX_NONE) { return; }
    UStaticMeshComponent* Model = NewObject<UStaticMeshComponent>(this);
    Model->SetupAttachment(ArenaMesh, Bone);
    Model->SetStaticMesh(MeshAsset);
    Model->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Model->SetCanEverAffectNavigation(false);
    Model->SetRelativeTransform(RelativeTransform);
    Model->RegisterComponent();
}

bool AHBCombatant::SetArenaEquipmentMesh(FName Socket, UStaticMesh* EquipmentMesh, const FTransform& Fit)
{
    if (!ArenaMesh || !EquipmentMesh) { return false; }
    for (USceneComponent* Child : ArenaMesh->GetAttachChildren())
    {
        UStaticMeshComponent* Model = Cast<UStaticMeshComponent>(Child);
        if (Model && Model->GetAttachSocketName() == Socket)
        {
            Model->SetStaticMesh(EquipmentMesh);
            Model->SetRelativeTransform(Fit);
            Model->SetMaterial(0, EquipmentMesh->GetMaterial(0));
            if (EquipmentMesh->GetFName() == TEXT("SM_VaultKnightSword"))
            {
                Model->SetMaterial(0, LoadObject<UMaterialInterface>(nullptr,
                    TEXT("/Game/DemoArena/Art/Characters/Finishes/M_VaultSwordSteel")));
            }
            else if (EquipmentMesh->GetFName() == TEXT("SM_VaultKnightShield"))
            {
                Model->SetMaterial(0, ArenaMesh->GetMaterial(0));
            }
            return true;
        }
    }
    return false;
}

void AHBCombatant::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!ArenaMesh) { return; }

    const UAnimInstance* Source = GetMesh()->GetAnimInstance();
    const UAnimMontage* Montage = Source ? Source->GetCurrentActiveMontage() : nullptr;
    const AHBPlayerCharacter* Player = Cast<AHBPlayerCharacter>(this);
    const bool bDodge = Player && Player->IsDodging() && ArenaDodge;
    const bool bTimedSwing = Player && Player->IsAttacking() && !Montage;
    UAnimSequence* Desired = GetVelocity().SizeSquared2D() > FMath::Square(20.0f) ? ArenaRun.Get() : ArenaIdle.Get();
    if (Montage)
    {
        const FName MontageName = Montage->GetFName();
        const bool bCast = MontageName == TEXT("AM_GAS_PulseCombat") || MontageName == TEXT("AM_GAS_EnemySlam");
        Desired = bCast ? ArenaCast.Get() : ArenaMelee.Get();
    }
    else if (bDodge)
    {
        Desired = ArenaDodge;
    }
    else if (bTimedSwing)
    {
        Desired = ArenaMelee;
    }
    if (CurrentArenaClip != Desired)
    {
        ArenaMesh->PlayAnimation(Desired, !Montage && !bDodge && !bTimedSwing);
        CurrentArenaClip = Desired;
    }
    if (Montage)
    {
        const float Progress = Source->Montage_GetPosition(Montage) / FMath::Max(0.01f, Montage->GetPlayLength());
        ArenaMesh->SetPlayRate(0.0f);
        ArenaMesh->SetPosition(FMath::Clamp(Progress, 0.0f, 1.0f) * Desired->GetPlayLength(), false);
    }
    else
    {
        ArenaMesh->SetPlayRate(Desired == ArenaRun ? FMath::Clamp(GetVelocity().Size2D() / 290.0, 0.65, 1.5) : 1.0f);
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
    FGameplayEffectSpecHandle Spec = AbilitySystem->MakeOutgoingSpec(HealthEffectClass, 1.0f, AbilitySystem->MakeEffectContext());
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
    return HBApplyDamageEffect(DamageCauser, this, Damage);
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
