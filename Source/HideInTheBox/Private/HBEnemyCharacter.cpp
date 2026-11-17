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
#include "HBArenaAbilities.h"
#include "HBIndicator.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Misc/PackageName.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/World.h"

AHBEnemyCharacter::AHBEnemyCharacter()
{
    this->PrimaryActorTick.bCanEverTick = true;
    this->PrimaryActorTick.TickInterval = 0.0f;
    this->GetCharacterMovement()->MaxWalkSpeed = 180.0f;
    this->fMaxHealth = 90.0f;
    this->Tint = FLinearColor(0.8f, 0.15f, 0.12f);
    SlamAbilityClass = UHBGA_EnemySlam::StaticClass();
    if (FPackageName::DoesPackageExist(TEXT("/Game/GASDemo/GAS/Abilities/GA_GASDemo_EnemySlam")))
    {
        static ConstructorHelpers::FClassFinder<UHBGA_EnemySlam> Slam(TEXT("/Game/GASDemo/GAS/Abilities/GA_GASDemo_EnemySlam"));
        if (Slam.Succeeded()) { SlamAbilityClass = Slam.Class; }
    }
}

void AHBEnemyCharacter::BeginPlay()
{
    Super::BeginPlay();
    this->Mode = this->GetWorld()->GetAuthGameMode<AHBGameMode>();
    if (this->Mode.IsValid())
    {
        this->Mode->RegisterEnemy(this);
        if (HasAuthority() && !this->Mode->IsGASDemo())
        {
            AbilitySystem->GiveAbility(FGameplayAbilitySpec(UHBGA_EnemyMelee::StaticClass(), 1));
        }
    }
    if (PracticeHealthWidgetClass && GetNetMode() != NM_DedicatedServer)
    {
        UWidgetComponent* HealthDisplay = NewObject<UWidgetComponent>(this, TEXT("PracticeHealthDisplay"));
        HealthDisplay->SetupAttachment(GetRootComponent());
        HealthDisplay->SetRelativeLocation(FVector(0.0f, 0.0f, 112.0f));
        HealthDisplay->SetWidgetSpace(EWidgetSpace::Screen);
        HealthDisplay->SetDrawSize(FVector2D(148.0, 24.0));
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

void AHBEnemyCharacter::EnableSlam()
{
    if (!HasAuthority() || bSlamEnabled || !SlamAbilityClass)
    {
        return;
    }
    bSlamEnabled = true;
    AbilitySystem->GiveAbility(FGameplayAbilitySpec(SlamAbilityClass, 1));
    fMaxHealth = FMath::Max(fMaxHealth, 150.0f);
    RestoreHealth();
    ApplyArenaEliteFinish();
}

void AHBEnemyCharacter::ShowSlamWarning(FVector Position, float Radius, FLinearColor Color, UMaterialInterface* Material)
{
    if (!IsValid(SlamWarning))
    {
        SlamWarning = GetWorld()->SpawnActor<AHBIndicator>();
        if (!SlamWarning) { return; }
        SlamWarning->SetRingMaterial(Material);
    }
    SlamWarning->ShowAt(Position, Radius, 5.0f, Color);
}

void AHBEnemyCharacter::HideSlamWarning()
{
    if (IsValid(SlamWarning))
    {
        SlamWarning->Destroy();
        SlamWarning = nullptr;
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
    if (AbilitySystem->HasMatchingGameplayTag(HBArenaTags::EnemyAttacking) ||
        AbilitySystem->HasMatchingGameplayTag(HBArenaTags::EnemyCasting) ||
        AbilitySystem->HasMatchingGameplayTag(HBArenaTags::Staggered))
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
        Target = this->Mode->FindNearbyTurret(GetActorLocation(), 185.0f);
        if (!Target) { Target = this->Mode->GetDefenseTarget(); }
        fRange = 160.0f;
    }
    if (!IsValid(Target))
    {
        return;
    }
    if (AHBStructure* Fence = Mode->FindBlockingFence(this, Target->GetActorLocation()))
    {
        Target = Fence;
        fRange = 160.0f;
    }
    FVector Direction = Target->GetActorLocation() - this->GetActorLocation();
    Direction.Z = 0.0f;
    this->SetActorRotation(Direction.Rotation());
    const UHBGA_EnemySlam* Slam = bSlamEnabled ? SlamAbilityClass->GetDefaultObject<UHBGA_EnemySlam>() : nullptr;
    if (Slam && Direction.SizeSquared() <= FMath::Square(Slam->GetCastRange()))
    {
        FGameplayEventData Event;
        Event.EventTag = HBArenaTags::EnemySlam;
        Event.Instigator = this;
        Event.Target = Target;
        if (AbilitySystem->HandleGameplayEvent(Event.EventTag, &Event) > 0)
        {
            return;
        }
    }
    if (Direction.SizeSquared() > FMath::Square(fRange))
    {
        this->AddMovementInput(Direction.GetSafeNormal(), 1.0f, true);
        return;
    }
    this->GetCharacterMovement()->StopMovementImmediately();
    FGameplayEventData Event;
    Event.EventTag = HBArenaTags::EnemyMelee;
    Event.Instigator = this;
    Event.Target = Target;
    Event.EventMagnitude = fRange;
    AbilitySystem->HandleGameplayEvent(Event.EventTag, &Event);
}

void AHBEnemyCharacter::DeliverAnimationHit(UAnimSequenceBase* Animation)
{
    const UAnimInstance* Anim = GetMesh()->GetAnimInstance();
    if (!AttackMontage || Animation != AttackMontage || !Anim || !Anim->Montage_IsActive(AttackMontage) ||
        !HasAuthority() || !IsAlive() || !Mode.IsValid())
    {
        return;
    }

    FGameplayEventData Event;
    Event.EventTag = HBArenaTags::EnemyMeleeHit;
    Event.Instigator = this;
    UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(this, Event.EventTag, Event);
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
    AbilitySystem->CancelAllAbilities();
    HideSlamWarning();
    if (this->Mode.IsValid())
    {
        this->Mode->UnregisterEnemy(this);
    }
    Super::EndPlay(EndPlayReason);
}
