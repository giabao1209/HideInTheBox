#include "HBArenaAbilities.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimMontage.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HBEffects.h"
#include "HBDebugSubsystem.h"
#include "HBEnemyCharacter.h"
#include "HBGameMode.h"
#include "HBIndicator.h"
#include "HBPlayerCharacter.h"
#include "HBStructure.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"

namespace HBArenaTags
{
    UE_DEFINE_GAMEPLAY_TAG(EnemySlam, "Ability.Enemy.Slam");
    UE_DEFINE_GAMEPLAY_TAG(EnemyMelee, "Event.Enemy.Melee");
    UE_DEFINE_GAMEPLAY_TAG(EnemyMeleeHit, "Event.Enemy.MeleeHit");
    UE_DEFINE_GAMEPLAY_TAG(EnemyAttacking, "State.Attacking.Enemy");
    UE_DEFINE_GAMEPLAY_TAG(EnemyCasting, "State.Casting.EnemySlam");
    UE_DEFINE_GAMEPLAY_TAG(Staggered, "State.Staggered");
    UE_DEFINE_GAMEPLAY_TAG(TurretFire, "Event.Structure.Fire");
    UE_DEFINE_GAMEPLAY_TAG(TurretUpgraded, "State.Structure.Upgraded");
    UE_DEFINE_GAMEPLAY_TAG(Build, "Event.Structure.Build");
    UE_DEFINE_GAMEPLAY_TAG(Repair, "Event.Structure.Repair");
}

UE_DEFINE_GAMEPLAY_TAG_STATIC(SlamCooldown, "Cooldown.Enemy.Slam");
UE_DEFINE_GAMEPLAY_TAG_STATIC(MeleeCooldown, "Cooldown.Enemy.Melee");
UE_DEFINE_GAMEPLAY_TAG_STATIC(ShotCooldown, "Cooldown.Structure.Fire");
UE_DEFINE_GAMEPLAY_TAG_STATIC(SlamWarningCue, "GameplayCue.GASDemo.Enemy.SlamWarning");
UE_DEFINE_GAMEPLAY_TAG_STATIC(SlamImpactCue, "GameplayCue.GASDemo.Enemy.SlamImpact");
UE_DEFINE_GAMEPLAY_TAG_STATIC(SlamInterruptCue, "GameplayCue.GASDemo.Enemy.Interrupted");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TurretShotCue, "GameplayCue.GASDemo.Structure.Shot");
UE_DEFINE_GAMEPLAY_TAG_STATIC(PulseBurstCue, "GameplayCue.GASDemo.Pulse.Burst");
UE_DEFINE_GAMEPLAY_TAG_STATIC(DodgeCue, "GameplayCue.GASDemo.Dodge");
UE_DEFINE_GAMEPLAY_TAG_STATIC(GuardCue, "GameplayCue.GASDemo.Guard");
UE_DEFINE_GAMEPLAY_TAG_STATIC(MendCue, "GameplayCue.GASDemo.Mend");
UE_DEFINE_GAMEPLAY_TAG_STATIC(BasicSwingCue, "GameplayCue.GASDemo.BasicSwing");
UE_DEFINE_GAMEPLAY_TAG_STATIC(BasicHitCue, "GameplayCue.GASDemo.BasicHit");
UE_DEFINE_GAMEPLAY_TAG_STATIC(BoltImpactCue, "GameplayCue.GASDemo.Bolt.Impact");

namespace
{
    const FName GuardVisualTag(TEXT("HB_GuardVisual"));

    void ConfigureCombatMesh(UStaticMeshComponent* Mesh, UMaterialInterface* Material, UStaticMesh* Shape,
        const FLinearColor& Color, float Duration)
    {
        Mesh->SetMobility(EComponentMobility::Movable);
        Mesh->SetCollisionProfileName(TEXT("NoCollision"));
        Mesh->SetCanEverAffectNavigation(false);
        Mesh->SetCastShadow(false);
        Mesh->SetStaticMesh(Shape);
        Mesh->SetMaterial(0, Material);
        if (UMaterialInstanceDynamic* Instance = Mesh->CreateAndSetMaterialInstanceDynamic(0))
        {
            Instance->SetVectorParameterValue(TEXT("Color"), Color);
            if (Duration > 0.0f)
            {
                Instance->SetScalarParameterValue(TEXT("SpawnTime"), Mesh->GetWorld()->GetTimeSeconds());
                Instance->SetScalarParameterValue(TEXT("Duration"), Duration);
            }
        }
    }

    AStaticMeshActor* SpawnCombatVisual(AActor* Target, UMaterialInterface* Material, UStaticMesh* Shape,
        const FVector& Position, const FRotator& Rotation, const FVector& Scale, const FLinearColor& Color, float Duration)
    {
        if (!Material || !Shape) { return nullptr; }
        FActorSpawnParameters Spawn;
        Spawn.Owner = Target;
        Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        AStaticMeshActor* Visual = Target->GetWorld()->SpawnActor<AStaticMeshActor>(Position, Rotation, Spawn);
        if (!Visual) { return nullptr; }
        Visual->Tags.Add(TEXT("HB_CombatCueVisual"));
        ConfigureCombatMesh(Visual->GetStaticMeshComponent(), Material, Shape, Color, Duration);
        Visual->SetActorScale3D(Scale);
        Visual->SetLifeSpan(Duration);
        return Visual;
    }

    void PlayCombatCard(AActor* Target, UMaterialInterface* Material, const FVector& Position, const FRotator& Rotation,
        const FVector2D& Size, const FLinearColor& Color, float Duration)
    {
        UStaticMesh* Plane = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
        SpawnCombatVisual(Target, Material, Plane, Position, Rotation, FVector(Size.X / 100.0, Size.Y / 100.0, 1.0), Color, Duration);
    }

    void PlayCombatSprite(AActor* Target, UMaterialInterface* Material, const FVector& Position,
        const FLinearColor& Color, float Size, float Duration, bool bFollow = false)
    {
        if (Material)
        {
            if (AHBIndicator* Visual = Target->GetWorld()->SpawnActor<AHBIndicator>())
            {
                Visual->Tags.Add(TEXT("HB_CombatCueVisual"));
                Visual->PlaySprite(Position, Material, Color, Size, Duration);
                if (bFollow) { Visual->AttachToActor(Target, FAttachmentTransformRules::KeepWorldTransform); }
            }
        }
    }
}

UHBGE_TurretUpgrade::UHBGE_TurretUpgrade()
{
    DurationPolicy = EGameplayEffectDurationType::Infinite;
}

UHBGA_ArenaAttack::UHBGA_ArenaAttack()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
}

void UHBGA_ArenaAttack::TriggerOnEvent(FGameplayTag EventTag)
{
    FAbilityTriggerData& Trigger = AbilityTriggers.AddDefaulted_GetRef();
    Trigger.TriggerTag = EventTag;
    Trigger.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
}

const FGameplayTagContainer* UHBGA_ArenaAttack::GetCooldownTags() const
{
    return &ArenaCooldownTags;
}

void UHBGA_ArenaAttack::ApplyCooldown(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
    FGameplayAbilityActivationInfo ActivationInfo) const
{
    UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
    const FGameplayEffectSpecHandle Effect = ASC->MakeOutgoingSpec(UHBGE_Cooldown::StaticClass(), 1.0f, ASC->MakeEffectContext());
    if (Effect.IsValid())
    {
        Effect.Data->SetDuration(CooldownSeconds, true);
        Effect.Data->DynamicGrantedTags.AppendTags(ArenaCooldownTags);
        ASC->ApplyGameplayEffectSpecToSelf(*Effect.Data);
    }
}

UHBGA_EnemyMelee::UHBGA_EnemyMelee()
{
    TriggerOnEvent(HBArenaTags::EnemyMelee);
    ArenaCooldownTags.AddTag(MeleeCooldown);
    ActivationOwnedTags.AddTag(HBArenaTags::EnemyAttacking);
    ActivationBlockedTags.AddTag(HBArenaTags::EnemyCasting);
    ActivationBlockedTags.AddTag(HBArenaTags::Staggered);
}

void UHBGA_EnemyMelee::ActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
    FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* Event)
{
    AHBEnemyCharacter* Enemy = Cast<AHBEnemyCharacter>(ActorInfo->AvatarActor.Get());
    AActor* Target = Event ? const_cast<AActor*>(Event->Target.Get()) : nullptr;
    if (!Enemy || !Enemy->IsAlive() || !IsValid(Target) || !FMath::IsFinite(Event->EventMagnitude) ||
        Event->EventMagnitude <= 0.0f ||
        FVector::DistSquaredXY(Enemy->GetActorLocation(), Target->GetActorLocation()) > FMath::Square(Event->EventMagnitude) ||
        !CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }
    TargetActor = Target;
    AttackRange = Event->EventMagnitude;
    UAnimMontage* Montage = Enemy->GetMeleeMontage();
    const USkeletalMesh* SkeletalAsset = Enemy->GetMesh()->GetSkeletalMeshAsset();
    if (!Montage || !ActorInfo->GetAnimInstance() || !SkeletalAsset || Montage->GetSkeleton() != SkeletalAsset->GetSkeleton())
    {
        HBApplyDamageEffect(Enemy, Target, 8.0f);
        Complete();
        return;
    }
    auto* WaitHit = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, HBArenaTags::EnemyMeleeHit, nullptr, true, true);
    WaitHit->EventReceived.AddDynamic(this, &UHBGA_EnemyMelee::Hit);
    WaitHit->ReadyForActivation();
    auto* Task = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
        this, NAME_None, Montage, 1.0f, NAME_None, true, 1.0f, 0.0f, true);
    Task->OnCompleted.AddDynamic(this, &UHBGA_EnemyMelee::Complete);
    Task->OnInterrupted.AddDynamic(this, &UHBGA_EnemyMelee::Interrupt);
    Task->OnCancelled.AddDynamic(this, &UHBGA_EnemyMelee::Interrupt);
    Task->ReadyForActivation();
}

void UHBGA_EnemyMelee::Hit(FGameplayEventData Event)
{
    AHBEnemyCharacter* Enemy = Cast<AHBEnemyCharacter>(GetAvatarActorFromActorInfo());
    AActor* Target = TargetActor.Get();
    if (Enemy && Enemy->IsAlive() && IsValid(Target) &&
        FVector::DistSquaredXY(Enemy->GetActorLocation(), Target->GetActorLocation()) <= FMath::Square(AttackRange))
    {
        HBApplyDamageEffect(Enemy, Target, 8.0f);
        UHBDebugSubsystem::Record(Enemy, TEXT("Combat"), TEXT("Enemy melee hit via montage notify"));
    }
}

void UHBGA_EnemyMelee::Complete()
{
    if (IsActive()) { EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false); }
}

void UHBGA_EnemyMelee::Interrupt()
{
    if (IsActive()) { EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true); }
}

UHBGA_EnemySlam::UHBGA_EnemySlam()
{
    TriggerOnEvent(HBArenaTags::EnemySlam);
    SetAssetTags(FGameplayTagContainer(HBArenaTags::EnemySlam));
    ActivationOwnedTags.AddTag(HBArenaTags::EnemyCasting);
    ActivationBlockedTags.AddTag(HBArenaTags::Staggered);
    ActivationBlockedTags.AddTag(HBArenaTags::EnemyAttacking);
    ArenaCooldownTags.AddTag(SlamCooldown);
    CooldownSeconds = 5.0f;
}

void UHBGA_EnemySlam::ActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
    FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* Event)
{
    AHBEnemyCharacter* Enemy = Cast<AHBEnemyCharacter>(ActorInfo->AvatarActor.Get());
    AActor* Target = Event ? const_cast<AActor*>(Event->Target.Get()) : nullptr;
    if (!IsValid(Enemy) || !Enemy->IsAlive() || !IsValid(Target) ||
        FVector::DistSquaredXY(Target->GetActorLocation(), Enemy->GetActorLocation()) > FMath::Square(CastRange) ||
        !CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    TargetStructure = Cast<AHBStructure>(Target);
    ImpactPoint = Target->GetActorLocation();
    if (const ACharacter* Character = Cast<ACharacter>(Target))
    {
        ImpactPoint.Z -= Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    }
    Enemy->GetCharacterMovement()->StopMovementImmediately();
    FGameplayCueParameters Cue;
    Cue.Location = ImpactPoint;
    Cue.RawMagnitude = ImpactRadius;
    Enemy->GetAbilitySystemComponent()->AddGameplayCue(SlamWarningCue, Cue);
    bWarningActive = true;
    UHBDebugSubsystem::Record(Enemy, TEXT("Combat"), TEXT("Enemy Slam windup"));

    if (SlamMontage)
    {
        auto* Montage = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
            this, NAME_None, SlamMontage, SlamMontage->GetPlayLength() /
                (WindupSeconds * FMath::Max(0.01f, SlamMontage->RateScale)),
            NAME_None, true, 1.0f, 0.0f, true);
        Montage->OnInterrupted.AddDynamic(this, &UHBGA_EnemySlam::Interrupt);
        Montage->OnCancelled.AddDynamic(this, &UHBGA_EnemySlam::Interrupt);
        Montage->ReadyForActivation();
        if (!IsActive()) { return; }
    }
    auto* Windup = UAbilityTask_WaitDelay::WaitDelay(this, WindupSeconds);
    Windup->OnFinish.AddDynamic(this, &UHBGA_EnemySlam::Impact);
    Windup->ReadyForActivation();
}

void UHBGA_EnemySlam::Impact()
{
    AHBEnemyCharacter* Enemy = Cast<AHBEnemyCharacter>(GetAvatarActorFromActorInfo());
    if (!IsValid(Enemy) || !Enemy->IsAlive())
    {
        Interrupt();
        return;
    }

    const AHBGameMode* Mode = Enemy->GetWorld()->GetAuthGameMode<AHBGameMode>();
    FGameplayCueParameters Cue;
    Cue.Location = ImpactPoint;
    Cue.RawMagnitude = ImpactRadius;
    Enemy->GetAbilitySystemComponent()->ExecuteGameplayCue(SlamImpactCue, Cue);
    UHBDebugSubsystem::Record(Enemy, TEXT("Combat"), TEXT("Enemy Slam impact"));
    for (AActor* Target : { static_cast<AActor*>(Mode ? Mode->GetPlayer() : nullptr), TargetStructure.Get() })
    {
        if (IsValid(Target) && FVector::DistSquaredXY(Target->GetActorLocation(), ImpactPoint) <= FMath::Square(ImpactRadius))
        {
            HBApplyDamageEffect(Enemy, Target, Damage);
        }
    }
    if (IsActive())
    {
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
    }
}

void UHBGA_EnemySlam::Interrupt()
{
    if (IsActive())
    {
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
    }
}

void UHBGA_EnemySlam::EndAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
    FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    if (bWarningActive)
    {
        bWarningActive = false;
        if (UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get())
        {
            ASC->RemoveGameplayCue(SlamWarningCue);
        }
    }
    TargetStructure.Reset();
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void HBInterruptEnemySlam(AHBEnemyCharacter* Enemy)
{
    UAbilitySystemComponent* ASC = Enemy->GetAbilitySystemComponent();
    if (!ASC->HasMatchingGameplayTag(HBArenaTags::EnemyCasting))
    {
        return;
    }
    const FGameplayEffectSpecHandle Stagger = ASC->MakeOutgoingSpec(UHBGE_Cooldown::StaticClass(), 1.0f, ASC->MakeEffectContext());
    if (Stagger.IsValid())
    {
        Stagger.Data->SetDuration(0.85f, true);
        Stagger.Data->DynamicGrantedTags.AddTag(HBArenaTags::Staggered);
        ASC->ApplyGameplayEffectSpecToSelf(*Stagger.Data);
    }
    const FGameplayTagContainer SlamTags(HBArenaTags::EnemySlam);
    ASC->CancelAbilities(&SlamTags);
    FGameplayCueParameters Cue;
    Cue.Location = Enemy->GetActorLocation() - FVector(0.0f, 0.0f, Enemy->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
    Cue.RawMagnitude = 105.0f;
    ASC->ExecuteGameplayCue(SlamInterruptCue, Cue);
    UHBDebugSubsystem::Record(Enemy, TEXT("Combat"), TEXT("Enemy Slam interrupted"));
}

UHBGA_TurretFire::UHBGA_TurretFire()
{
    TriggerOnEvent(HBArenaTags::TurretFire);
    ArenaCooldownTags.AddTag(ShotCooldown);
    CooldownSeconds = 0.7f;
}

void UHBGA_TurretFire::ActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
    FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* Event)
{
    AHBStructure* Tower = Cast<AHBStructure>(ActorInfo->AvatarActor.Get());
    AHBEnemyCharacter* Enemy = Event ? Cast<AHBEnemyCharacter>(const_cast<AActor*>(Event->Target.Get())) : nullptr;
    if (!IsValid(Tower) || !Tower->CanFire() || !IsValid(Enemy) || !Enemy->IsAlive() ||
        FVector::DistSquaredXY(Enemy->GetActorLocation(), Tower->GetActorLocation()) > FMath::Square(Tower->GetFireRange()) ||
        !CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }
    FGameplayCueParameters Cue;
    Cue.Location = Enemy->GetActorLocation();
    HBApplyDamageEffect(Tower, Enemy, ShotDamage + (Tower->IsUpgraded() ? UpgradeBonus : 0.0f));
    Tower->GetAbilitySystemComponent()->ExecuteGameplayCue(TurretShotCue, Cue);
    EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

UHBGA_Construction::UHBGA_Construction()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
    for (const FGameplayTag Tag : { HBArenaTags::Build.GetTag(), HBArenaTags::Repair.GetTag() })
    {
        FAbilityTriggerData& Trigger = AbilityTriggers.AddDefaulted_GetRef();
        Trigger.TriggerTag = Tag;
        Trigger.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
    }
}

void UHBGA_Construction::ActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
    FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* Event)
{
    AHBPlayerCharacter* Player = Cast<AHBPlayerCharacter>(ActorInfo->AvatarActor.Get());
    AHBGameMode* Mode = Player ? Player->GetWorld()->GetAuthGameMode<AHBGameMode>() : nullptr;
    const bool bSucceeded = Event && Mode && Player->IsAlive() && Mode->ExecuteConstruction(Event->EventTag == HBArenaTags::Repair);
    if (bSucceeded)
    {
        CommitAbility(Handle, ActorInfo, ActivationInfo);
    }
    UHBDebugSubsystem::Record(Player, TEXT("Build"), FString::Printf(TEXT("GAS %s %s"), Event ? *Event->EventTag.ToString() : TEXT("No event"),
        bSucceeded ? TEXT("committed") : TEXT("rejected")));
    EndAbility(Handle, ActorInfo, ActivationInfo, true, !bSucceeded);
}

UHBGameplayCue_Arena::UHBGameplayCue_Arena()
{
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Sparks(TEXT("/Game/DemoArena/Art/FX/M_ArenaImpactSprite.M_ArenaImpactSprite"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Dust(TEXT("/Game/DemoArena/Art/FX/M_ArenaDustSprite.M_ArenaDustSprite"));
    SparkSprite = Sparks.Object;
    DustSprite = Dust.Object;
}

bool UHBGameplayCue_Arena::OnActive_Implementation(AActor* Target, const FGameplayCueParameters& Parameters) const
{
    if (GameplayCueTag == GuardCue && IsValid(Target) && Target->GetNetMode() != NM_DedicatedServer)
    {
        if (Target->FindComponentByTag<UStaticMeshComponent>(GuardVisualTag)) { return true; }
        UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
        if (!Sphere || !CueMaterial || !Target->GetRootComponent()) { return false; }
        UStaticMeshComponent* Shield = NewObject<UStaticMeshComponent>(Target);
        Shield->ComponentTags.Add(GuardVisualTag);
        Shield->SetupAttachment(Target->GetRootComponent());
        Shield->SetRelativeLocation(FVector(0, 0, -17));
        Shield->SetRelativeScale3D(FVector(1.68, 1.68, 1.76));
        ConfigureCombatMesh(Shield, CueMaterial, Sphere, CueColor, 0.0f);
        Shield->RegisterComponent();
        UHBDebugSubsystem::Record(Target, TEXT("Combat"), TEXT("GameplayCue.GASDemo.Guard active"));
        return true;
    }
    if (AHBEnemyCharacter* Enemy = Cast<AHBEnemyCharacter>(Target))
    {
        if (Enemy->GetNetMode() == NM_DedicatedServer) { return false; }
        Enemy->ShowSlamWarning(Parameters.Location, Parameters.RawMagnitude, CueColor, CueMaterial);
        UGameplayStatics::PlaySoundAtLocation(Enemy, CueSound, Enemy->GetActorLocation(), Volume, Pitch);
        return true;
    }
    return false;
}

bool UHBGameplayCue_Arena::WhileActive_Implementation(AActor* Target, const FGameplayCueParameters& Parameters) const
{
    return GameplayCueTag == GuardCue && OnActive_Implementation(Target, Parameters);
}

bool UHBGameplayCue_Arena::OnRemove_Implementation(AActor* Target, const FGameplayCueParameters& Parameters) const
{
    if (GameplayCueTag == GuardCue && IsValid(Target))
    {
        if (UStaticMeshComponent* Shield = Target->FindComponentByTag<UStaticMeshComponent>(GuardVisualTag))
        {
            Shield->DestroyComponent();
        }
        return true;
    }
    if (AHBEnemyCharacter* Enemy = Cast<AHBEnemyCharacter>(Target))
    {
        Enemy->HideSlamWarning();
        return true;
    }
    return false;
}

bool UHBGameplayCue_Arena::OnExecute_Implementation(AActor* Target, const FGameplayCueParameters& Parameters) const
{
    if (!IsValid(Target) || Target->GetNetMode() == NM_DedicatedServer)
    {
        return false;
    }
    const FVector Location = Parameters.Location;
    const FRotator Direction(0.0, FVector(Parameters.Normal).Rotation().Yaw, 0.0);
    if (GameplayCueTag == PulseBurstCue)
    {
        const float Diameter = FMath::Max(120.0f, Parameters.RawMagnitude * 2.0f);
        PlayCombatCard(Target, CueMaterial, Location + FVector(0, 0, 6), FRotator::ZeroRotator, FVector2D(Diameter * 1.17), CueColor, 0.70f);
        PlayCombatCard(Target, DustSprite, Location + FVector(0, 0, 12), FRotator(0, 25, 0), FVector2D(Diameter * 0.77), CueColor * 0.85f, 0.55f);
        PlayCombatSprite(Target, SparkSprite, Location + FVector(0, 0, 55), CueColor * 1.25f, Diameter * 0.30f, 0.43f);
    }
    else if (GameplayCueTag == DodgeCue)
    {
        const FVector Wake = Location - FVector(Parameters.Normal) * 18.0f + FVector(0, 0, 10);
        const FVector Side = FRotationMatrix(Direction).GetScaledAxis(EAxis::Y) * 29.0f;
        PlayCombatCard(Target, CueMaterial, Wake, Direction, FVector2D(250, 85), CueColor, 0.44f);
        PlayCombatCard(Target, CueMaterial, Wake + Side, Direction, FVector2D(180, 48), CueColor * 0.52f, 0.37f);
        PlayCombatCard(Target, CueMaterial, Wake - Side, Direction, FVector2D(155, 48), CueColor * 0.44f, 0.33f);
        PlayCombatSprite(Target, DustSprite, Location + FVector(0, 0, 18), CueColor * 0.4f, 58.0f, 0.49f);
    }
    else if (GameplayCueTag == MendCue)
    {
        PlayCombatCard(Target, CueMaterial, Location + FVector(0, 0, 6), FRotator::ZeroRotator, FVector2D(285), CueColor, 0.90f);
        const FVector Chest = Target->GetActorLocation() + FVector(0, 0, 44);
        PlayCombatSprite(Target, SparkSprite, Chest + FVector(0, 18, 82), CueColor, 49.0f, 0.72f, true);
        PlayCombatSprite(Target, DustSprite, Chest + FVector(-15, -70, 23), CueColor * 0.75f, 53.0f, 0.56f, true);
        PlayCombatSprite(Target, DustSprite, Chest + FVector(25, 68, 37), CueColor * 0.62f, 41.0f, 0.49f, true);
    }
    else if (GameplayCueTag == BasicSwingCue)
    {
        PlayCombatCard(Target, CueMaterial, Location, Direction + FRotator(0, -35, 0), FVector2D(318, 285), CueColor, 0.34f);
    }
    else if (GameplayCueTag == BasicHitCue || GameplayCueTag == BoltImpactCue)
    {
        const bool bBoltImpact = GameplayCueTag == BoltImpactCue;
        PlayCombatSprite(Target, SparkSprite, Location, CueColor, bBoltImpact ? 93.0f : 56.0f, bBoltImpact ? 0.38f : 0.27f);
    }
    else
    {
        if (AHBIndicator* Visual = Target->GetWorld()->SpawnActor<AHBIndicator>())
        {
            if (bTracer)
            {
                Visual->PlayBolt(Target->GetActorLocation() + FVector(0, 0, 103), Location, CueMaterial, 0.22f);
            }
            else
            {
                Visual->SetRingMaterial(CueMaterial);
                Visual->PlayBurst(Location, Parameters.RawMagnitude, CueColor, 0.34f);
            }
        }
        PlayCombatSprite(Target, SparkSprite, Location + FVector(0, 0, bTracer ? 12 : 46),
            CueColor, bTracer ? 58.0f : 102.0f, 0.28f);
        if (!bTracer)
        {
            PlayCombatSprite(Target, DustSprite, Location + FVector(0, 0, 22),
                CueColor * 0.23f, FMath::Max(65.0f, Parameters.RawMagnitude), 0.55f);
        }
    }
    const FVector SoundLocation = bTracer ? Target->GetActorLocation() : Location;
    if (CueSound) { UGameplayStatics::PlaySoundAtLocation(Target, CueSound, SoundLocation, Volume, Pitch); }
    UHBDebugSubsystem::Record(Target, TEXT("Combat"), FString::Printf(TEXT("%s executed"), *Parameters.MatchedTagName.ToString()));
    return true;
}
