#include "HBGameMode.h"
#include "HBArenaAbilities.h"
#include "AbilitySystemComponent.h"
#include "HBDebugSubsystem.h"
#include "HBEffects.h"
#include "HBGameplayAbility.h"
#include "HBPlayerCharacter.h"
#include "HBPlayerController.h"
#include "HBEnemyCharacter.h"
#include "HBStructure.h"
#include "HBHUD.h"
#include "HideInTheBox.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "TimerManager.h"

namespace
{
    const FName DungeonExitTag(TEXT("HB_DungeonExit"));
    const FName CacheGuardTags[] = { TEXT("HB_DungeonGuard_West"), TEXT("HB_DungeonGuard_East"), TEXT("HB_DungeonGuard_Elite") };
    constexpr float DungeonInteractRadius = 180.0f;
    constexpr float DungeonExitRadius = 240.0f;

    bool IsOptionalGuard(const AActor& Actor)
    {
        return Actor.ActorHasTag(CacheGuardTags[0]) || Actor.ActorHasTag(CacheGuardTags[1]);
    }
}

AHBGameMode::AHBGameMode()
{
    this->DefaultPawnClass = AHBPlayerCharacter::StaticClass();
    this->PlayerControllerClass = AHBPlayerController::StaticClass();
    this->HUDClass = AHBHUD::StaticClass();
    this->EnemyClass = AHBEnemyCharacter::StaticClass();
    this->Enemies.Reserve(32);
}

void AHBGameMode::StartPlay()
{
    Super::StartPlay();
    if (this->bGASDemo)
    {
        this->ResetGASDemo();
        return;
    }
    if (!this->DiscoverArenaActors())
    {
        this->EndRun();
        return;
    }
    AHBPlayerCharacter* PlayerCharacter = this->GetPlayer();
    if (PlayerCharacter)
    {
        PlayerCharacter->GetAbilitySystemComponent()->GiveAbility(FGameplayAbilitySpec(UHBGA_Construction::StaticClass(), 1));
        PlayerCharacter->ResetForPhase(this->GetTaggedLocation(this->TownSpawnTag, PlayerCharacter->GetActorLocation()));
    }
    this->Status = UsesDungeonRoute()
        ? TEXT("Dungeon camp: B / D-pad Down builds. Enter / D-pad Up starts the raid.")
        : TEXT("Enter the dungeon with Enter / D-pad Up. Earn stones to build defenses.");
}

void AHBGameMode::ResetGASDemo()
{
    AHBPlayerCharacter* Player = this->GetPlayer();
    if (!Player) { return; }
    if (!this->bGASDemo)
    {
        if (Player->IsAlive() && this->Phase != EHBPhase::EHBP_GameOver)
        {
            Player->ResetCombatState();
            UHBDebugSubsystem::Record(this, TEXT("Combat"), TEXT("Arena GAS reset: HP/MP full, cooldowns cleared"));
        }
        return;
    }
    this->ClearEnemies();
    this->Transition(EHBPhase::EHBP_Dungeon);
    AHBPlayerController* Controller = Cast<AHBPlayerController>(Player->GetController());
    if (Controller) { Controller->ResetTransientInput(); }
    Player->ResetForPhase(this->GetTaggedLocation(this->TownSpawnTag, Player->GetActorLocation()));
    TArray<TObjectPtr<AActor>> Targets;
    this->FindTaggedActors(TEXT("HB_GASDemoTarget"), Targets);
    for (AActor* Target : Targets)
    {
        this->SpawnEnemyAt(Target->GetActorLocation(), Target->GetActorRotation());
    }
    for (const TWeakObjectPtr<AHBEnemyCharacter>& Enemy : this->Enemies)
    {
        if (Enemy.IsValid()) { Enemy->SetActorTickEnabled(false); }
    }
    this->Status = TEXT("RMB: Pulse on cursor | 3: Mend | F6: reset targets + resources | F7: lose 60 HP | F8: refill MP");
    UHBDebugSubsystem::Record(this, TEXT("Combat"), TEXT("GAS practice reset: targets restored, HP/MP full, cooldowns cleared"));
}

void AHBGameMode::DamageGASDemoPlayer()
{
    AHBPlayerCharacter* Player = this->GetPlayer();
    if (this->Phase == EHBPhase::EHBP_GameOver || !Player || !Player->IsAlive()) { return; }
    HBApplyDamageEffect(this, Player, FMath::Min(60.0f, Player->GetHealth() - 1.0f));
}

void AHBGameMode::RefillGASDemoMana()
{
    AHBPlayerCharacter* Player = this->GetPlayer();
    if (this->Phase == EHBPhase::EHBP_GameOver || !Player || !Player->IsAlive()) { return; }
    Player->ApplyGASManaDelta(Player->GetGASMaxMana() - Player->GetGASMana());
}

bool AHBGameMode::DiscoverArenaActors()
{
    AActor* GateActor = this->FindFirstTaggedActor(this->GateTag);
    AActor* AnimaActor = this->FindFirstTaggedActor(this->AnimaTag);
    this->Gate = Cast<AHBStructure>(GateActor);
    this->Anima = Cast<AHBStructure>(AnimaActor);
    this->FindTaggedActors(this->BuildSocketTag, this->BuildSockets);
    this->FindTaggedActors(this->GreedCacheTag, this->GreedCaches);
    this->FindTaggedActors(this->DungeonSpawnTag, this->DungeonSpawnPoints);
    this->FindTaggedActors(this->WaveSpawnTag, this->WaveSpawnPoints);
    DungeonExit = this->FindFirstTaggedActor(DungeonExitTag);

    const bool bValidGate = IsValid(this->Gate);
    const bool bValidAnima = IsValid(this->Anima);
    const bool bValidTownSpawn = IsValid(this->FindFirstTaggedActor(this->TownSpawnTag));
    const bool bValidDungeonPlayerSpawn = IsValid(this->FindFirstTaggedActor(this->DungeonPlayerSpawnTag));
    const bool bValidDefensePlayerSpawn = IsValid(this->FindFirstTaggedActor(this->DefensePlayerSpawnTag));
    const bool bValidSockets = this->BuildSockets.Num() >= 1;
    const bool bValidCaches = this->GreedCaches.Num() >= 3;
    const bool bValidDungeonSpawns = !this->DungeonSpawnPoints.IsEmpty();
    const bool bValidWaveSpawns = !this->WaveSpawnPoints.IsEmpty();
    const bool bValidClasses = this->EnemyClass && this->TurretClass;
    if (!bValidGate) { UE_LOG(LogHideInTheBox, Error, TEXT("DemoArena missing BP_Gate actor tagged HB_Gate.")); }
    if (!bValidAnima) { UE_LOG(LogHideInTheBox, Error, TEXT("DemoArena missing BP_Anima actor tagged HB_Anima.")); }
    if (!bValidTownSpawn) { UE_LOG(LogHideInTheBox, Error, TEXT("DemoArena missing player anchor tagged HB_TownSpawn.")); }
    if (!bValidDungeonPlayerSpawn) { UE_LOG(LogHideInTheBox, Error, TEXT("DemoArena missing player anchor tagged HB_DungeonPlayerSpawn.")); }
    if (!bValidDefensePlayerSpawn) { UE_LOG(LogHideInTheBox, Error, TEXT("DemoArena missing player anchor tagged HB_DefensePlayerSpawn.")); }
    if (!bValidSockets) { UE_LOG(LogHideInTheBox, Error, TEXT("DemoArena needs at least one actor tagged HB_BuildSocket.")); }
    if (!bValidCaches) { UE_LOG(LogHideInTheBox, Error, TEXT("DemoArena needs three actors tagged HB_GreedCache.")); }
    if (!bValidDungeonSpawns) { UE_LOG(LogHideInTheBox, Error, TEXT("DemoArena needs an actor tagged HB_DungeonSpawn.")); }
    if (!bValidWaveSpawns) { UE_LOG(LogHideInTheBox, Error, TEXT("DemoArena needs an actor tagged HB_WaveSpawn.")); }
    if (!bValidClasses) { UE_LOG(LogHideInTheBox, Error, TEXT("Assign EnemyClass and TurretClass on BP_DemoGameMode.")); }
    if (!(bValidGate && bValidAnima && bValidTownSpawn && bValidDungeonPlayerSpawn && bValidDefensePlayerSpawn && bValidSockets && bValidCaches && bValidDungeonSpawns && bValidWaveSpawns && bValidClasses))
    {
        return false;
    }

    this->Gate->Configure(EHBStructure::EHBS_Gate);
    this->Anima->Configure(EHBStructure::EHBS_Anima);
    this->Gate->SetActorHiddenInGame(false);
    this->Anima->SetActorHiddenInGame(false);
    if (!CreateBuildBoard())
    {
        UE_LOG(LogHideInTheBox, Error, TEXT("Build board has no walkable cells."));
        return false;
    }
    this->ResetGreedCaches();
    for (AActor* Cache : this->GreedCaches)
    {
        if (IsValid(Cache))
        {
            Cache->SetActorHiddenInGame(true);
        }
    }
    return true;
}

bool AHBGameMode::CreateBuildBoard()
{
    const FVector Origin = BuildSockets[0]->GetActorLocation();
    FBox Bounds(ForceInit);
    for (const AActor* Anchor : BuildSockets)
    {
        if (IsValid(Anchor)) { Bounds += Anchor->GetActorLocation(); }
    }
    Bounds = Bounds.ExpandBy(245.0);
    const FVector TownSpawn = GetTaggedLocation(TownSpawnTag, Origin);
    const FVector DefenseSpawn = GetTaggedLocation(DefensePlayerSpawnTag, Origin);
    constexpr double RowSpacing = 1.7320508075688772 * BuildCellRadius;
    const int32 Columns = FMath::CeilToInt((Bounds.Max.X - Bounds.Min.X) / (1.5 * BuildCellRadius));
    const int32 Rows = FMath::CeilToInt((Bounds.Max.Y - Bounds.Min.Y) / RowSpacing) + Columns;
    for (int32 Q = -Columns; Q <= Columns; ++Q)
    {
        for (int32 R = -Rows; R <= Rows; ++R)
        {
            const FVector Point = Origin + FVector(1.5 * BuildCellRadius * Q, RowSpacing * (R + 0.5 * Q), 0.0);
            if (!Bounds.IsInsideOrOn(Point)) { continue; }
            const FVector GateDelta = Point - Gate->GetActorLocation();
            if ((FMath::Abs(GateDelta.X) < 135.0 && FMath::Abs(GateDelta.Y) < 215.0) ||
                FVector::DistSquaredXY(Point, Anima->GetActorLocation()) < FMath::Square(180.0) ||
                FVector::DistSquaredXY(Point, TownSpawn) < FMath::Square(120.0) ||
                FVector::DistSquaredXY(Point, DefenseSpawn) < FMath::Square(120.0))
            {
                continue;
            }
            FHitResult Floor;
            if (GetWorld()->LineTraceSingleByChannel(Floor, Point + FVector(0, 0, 125), Point - FVector(0, 0, 125),
                    ECC_GameTraceChannel1) && Floor.ImpactNormal.Z > 0.9)
            {
                BuildCells.Add(Floor.ImpactPoint);
            }
        }
    }
    SelectBuildCell(Origin);
    Structures.SetNum(BuildCells.Num());
    for (AActor* Anchor : BuildSockets)
    {
        if (IsValid(Anchor))
        {
            Anchor->SetActorHiddenInGame(true);
            Anchor->SetActorEnableCollision(false);
        }
    }
    return !BuildCells.IsEmpty();
}

AActor* AHBGameMode::FindFirstTaggedActor(FName Tag) const
{
    TArray<TObjectPtr<AActor>> Actors;
    this->FindTaggedActors(Tag, Actors);
    return Actors.IsEmpty() ? nullptr : Actors[0].Get();
}

void AHBGameMode::FindTaggedActors(
    FName Tag,
    TArray<TObjectPtr<AActor>>& OutActors) const
{
    OutActors.Reset();

    if (Tag.IsNone() || !IsValid(this->GetWorld()))
    {
        return;
    }

    TArray<AActor*> FoundActors;
    UGameplayStatics::GetAllActorsWithTag(this, Tag, FoundActors);

    OutActors.Reserve(FoundActors.Num());

    for (AActor* FoundActor : FoundActors)
    {
        if (IsValid(FoundActor))
        {
            OutActors.Add(FoundActor);
        }
    }

    for (int32 LeftIndex = 0; LeftIndex < OutActors.Num(); ++LeftIndex)
    {
        for (int32 RightIndex = LeftIndex + 1; RightIndex < OutActors.Num(); ++RightIndex)
        {
            if (OutActors[RightIndex]->GetName() < OutActors[LeftIndex]->GetName())
            {
                OutActors.Swap(LeftIndex, RightIndex);
            }
        }
    }
}

FVector AHBGameMode::GetTaggedLocation(FName Tag, const FVector& Fallback) const
{
    AActor* Anchor = this->FindFirstTaggedActor(Tag);
    return IsValid(Anchor) ? Anchor->GetActorLocation() : Fallback;
}

AHBPlayerCharacter* AHBGameMode::GetPlayer() const
{
    return Cast<AHBPlayerCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
}

void AHBGameMode::Transition(EHBPhase NewPhase)
{
    const EHBPhase PreviousPhase = this->Phase;
    if (PreviousPhase == NewPhase)
    {
        return;
    }
    UHBDebugSubsystem::Record(this, TEXT("System"), FString::Printf(TEXT("Phase %d -> %d"), static_cast<int32>(PreviousPhase), static_cast<int32>(NewPhase)));
    this->Phase = NewPhase;
    this->OnPhaseChanged.Broadcast(PreviousPhase, NewPhase);
    AHBPlayerController* Controller = Cast<AHBPlayerController>(UGameplayStatics::GetPlayerController(this, 0));
    if (Controller)
    {
        Controller->ResetTransientInput();
    }
    AHBPlayerCharacter* PlayerCharacter = this->GetPlayer();
    if (PlayerCharacter)
    {
        PlayerCharacter->SetBuildMode(false);
    }
    UE_LOG(LogHideInTheBox, Log, TEXT("Phase=%s cycle=%d stones=%d threat=%d"), *this->GetPhaseLabel(), this->iCycle, this->iStones, this->iThreat);
}

void AHBGameMode::AdvancePhase()
{
    if (this->bGASDemo) { return; }
    AHBPlayerCharacter* PlayerCharacter = this->GetPlayer();
    if (!PlayerCharacter || !PlayerCharacter->IsAlive())
    {
        return;
    }
    if (this->Phase == EHBPhase::EHBP_Town)
    {
        this->iThreat = 0;
        this->Transition(EHBPhase::EHBP_Dungeon);
        PlayerCharacter->ResetForPhase(this->GetTaggedLocation(this->DungeonPlayerSpawnTag, PlayerCharacter->GetActorLocation()));
        this->SpawnDungeon();
        this->ResetGreedCaches();
        this->Status = UsesDungeonRoute()
            ? TEXT("Follow the trail and defeat the Slam elite. Side caches are optional and add Threat.")
            : TEXT("Clear the dungeon. LMB / A attacks, RMB / B casts Pulse. E / L3 opens a nearby cache.");
    }
    else if (this->Phase == EHBPhase::EHBP_Dungeon)
    {
        if (GetDungeonEnemiesRemaining() > 0)
        {
            Status = UsesDungeonRoute() ? TEXT("Clear the main trail and defeat the elite to open extraction.")
                : TEXT("Clear the dungeon before returning to prep.");
            return;
        }
        if (UsesDungeonRoute() && !IsAtDungeonExit(*PlayerCharacter))
        {
            Status = GetDungeonExitHint(*PlayerCharacter);
            return;
        }
        if (UsesDungeonRoute() && (PlayerCharacter->IsAttacking() || PlayerCharacter->IsDodging() ||
            PlayerCharacter->GetAbilitySystemComponent()->HasMatchingGameplayTag(HBGetPulseCastingTag())))
        {
            Status = TEXT("Finish your action before extracting.");
            return;
        }
        this->ClearEnemies();
        for (AActor* Cache : this->GreedCaches)
        {
            if (IsValid(Cache))
            {
                Cache->SetActorHiddenInGame(true);
            }
        }
        this->Transition(EHBPhase::EHBP_Return);
        PlayerCharacter->ResetForPhase(this->GetTaggedLocation(this->TownSpawnTag, PlayerCharacter->GetActorLocation()));
        this->Status = UsesDungeonRoute()
            ? TEXT("Camp prep: build with B / D-pad Down. Enter / D-pad Up starts Defense here.")
            : TEXT("Prep: B / D-pad Down opens the build board. Enter / D-pad Up starts defense.");
    }
    else if (this->Phase == EHBPhase::EHBP_Return)
    {
        PlayerCharacter->SetBuildMode(false);
        this->Transition(EHBPhase::EHBP_Defense);
        this->iWave = 0;
        this->SpawnWave();
        this->GetWorldTimerManager().SetTimer(this->WaveTimer, this, &AHBGameMode::MonitorWave, 0.25f, true);
    }
}

AHBEnemyCharacter* AHBGameMode::SpawnEnemyAt(const FVector& Position, const FRotator& Rotation)
{
    if (!this->EnemyClass || !IsValid(this->GetWorld()))
    {
        return nullptr;
    }
    FActorSpawnParameters Parameters;
    Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
    AHBEnemyCharacter* SpawnedEnemy = this->GetWorld()->SpawnActor<AHBEnemyCharacter>(this->EnemyClass, Position, Rotation, Parameters);
    if (!IsValid(SpawnedEnemy))
    {
        UE_LOG(LogHideInTheBox, Warning, TEXT("Enemy spawn failed at %s."), *Position.ToString());
        return nullptr;
    }
    return SpawnedEnemy;
}

void AHBGameMode::SpawnDungeon()
{
    const int32 PointCount = this->DungeonSpawnPoints.Num();
    if (PointCount < 1) { return; }
    if (UsesDungeonRoute())
    {
        for (const AActor* Point : DungeonSpawnPoints)
        {
            if (!IsValid(Point)) { continue; }
            AHBEnemyCharacter* Spawned = SpawnEnemyAt(Point->GetActorLocation(), Point->GetActorRotation());
            if (!IsValid(Spawned)) { continue; }
            for (const FName GuardTag : CacheGuardTags)
            {
                if (Point->ActorHasTag(GuardTag)) { Spawned->Tags.Add(GuardTag); }
            }
            if (Point->ActorHasTag(CacheGuardTags[2])) { Spawned->EnableSlam(); }
        }
        return;
    }
    AHBEnemyCharacter* LastSpawned = nullptr;
    for (int32 Index = 0; Index < this->iDungeonEnemyCount; ++Index)
    {
        AActor* Point = this->DungeonSpawnPoints[Index % PointCount];
        if (!IsValid(Point)) { continue; }
        const FVector Offset(0.0f, 120.0f * static_cast<float>(Index / PointCount), 0.0f);
        if (AHBEnemyCharacter* Spawned = SpawnEnemyAt(Point->GetActorLocation() + Offset, Point->GetActorRotation()))
        {
            LastSpawned = Spawned;
        }
    }
    if (LastSpawned) { LastSpawned->EnableSlam(); }
}

void AHBGameMode::RegisterEnemy(AHBEnemyCharacter* Enemy)
{
    if (IsValid(Enemy)) { this->Enemies.AddUnique(Enemy); }
}

void AHBGameMode::UnregisterEnemy(AHBEnemyCharacter* Enemy)
{
    this->Enemies.Remove(Enemy);
}

void AHBGameMode::NotifyEnemyKilled(AHBEnemyCharacter* Enemy)
{
    if (this->bGASDemo) { return; }
    const EHBPhase PhaseAtDefeat = this->Phase;
    if (PhaseAtDefeat == EHBPhase::EHBP_Dungeon)
    {
        if (!UsesDungeonRoute() || !IsOptionalGuard(*Enemy))
        {
            this->iStones += 15;
            ++this->iThreat;
            if (GetDungeonEnemiesRemaining() == 0)
            {
                const AHBPlayerCharacter* Player = GetPlayer();
                Status = Player ? GetDungeonExitHint(*Player) : TEXT("Dungeon clear.");
                if (UsesDungeonRoute() && Player)
                {
                    FGameplayCueParameters Cue;
                    Cue.Location = GetDungeonExitPosition();
                    Cue.RawMagnitude = DungeonExitRadius;
                    Player->GetAbilitySystemComponent()->ExecuteGameplayCue(
                        FGameplayTag::RequestGameplayTag(TEXT("GameplayCue.GASDemo.Dungeon.Extraction")), Cue);
                }
            }
        }
        else
        {
            const int32 Cache = Enemy->ActorHasTag(CacheGuardTags[0]) ? 0 : 1;
            if (!IsGreedLocked(Cache))
            {
                Status = Cache == 0 ? TEXT("West cache unlocked. E / L3 opens it and raises Threat.")
                    : TEXT("East cache unlocked. E / L3 opens it and raises Threat.");
            }
        }
    }
    this->OnEnemyDefeated.Broadcast(Enemy, PhaseAtDefeat);
}

void AHBGameMode::SpawnWave()
{
    if (this->Phase != EHBPhase::EHBP_Defense)
    {
        return;
    }
    ++this->iWave;
    this->bWaveActive = true;
    this->fWaveSeconds = this->fWaveDuration;
    const int32 EnemyCount = HBRules::GetWaveEnemyCount(this->iWave, this->iThreat);
    const int32 PointCount = this->WaveSpawnPoints.Num();
    AHBEnemyCharacter* LastSpawned = nullptr;
    for (int32 Index = 0; Index < EnemyCount && PointCount > 0; ++Index)
    {
        AActor* Point = this->WaveSpawnPoints[Index % PointCount];
        if (!IsValid(Point)) { continue; }
        const int32 Ring = Index / PointCount;
        const FVector Offset(150.0f * static_cast<float>(Ring), 110.0f * static_cast<float>((Index % 3) - 1), 0.0f);
        if (AHBEnemyCharacter* Spawned = SpawnEnemyAt(Point->GetActorLocation() + Offset, Point->GetActorRotation()))
        {
            LastSpawned = Spawned;
        }
    }
    if (LastSpawned) { LastSpawned->EnableSlam(); }
    this->OnWaveStarted.Broadcast(this->iWave, EnemyCount);
    this->Status = TEXT("Defend Gate then Anima. Building and repair stay locked between all five waves.");
}

void AHBGameMode::MonitorWave()
{
    if (this->Phase != EHBPhase::EHBP_Defense || !this->bWaveActive)
    {
        return;
    }
    this->fWaveSeconds = FMath::Max(0.0f, this->fWaveSeconds - 0.25f);
    if (this->Enemies.IsEmpty() || this->fWaveSeconds <= 0.0f)
    {
        const bool bTimedOut = !this->Enemies.IsEmpty() && this->fWaveSeconds <= 0.0f;
        const int32 ResolvedWave = this->iWave;
        this->bWaveActive = false;
        this->ClearEnemies();
        this->OnWaveResolved.Broadcast(ResolvedWave, bTimedOut);
        if (this->iWave >= this->iWavesPerDefense)
        {
            this->CompleteDefense();
        }
        else
        {
            this->Status = TEXT("Three-second wave break. Reposition; construction remains locked.");
            this->GetWorldTimerManager().SetTimer(this->BreakTimer, this, &AHBGameMode::SpawnWave, 3.0f, false);
        }
    }
}

void AHBGameMode::CompleteDefense()
{
    this->GetWorldTimerManager().ClearTimer(this->WaveTimer);
    this->GetWorldTimerManager().ClearTimer(this->RespawnTimer);
    this->iStones += 30 + this->iThreat * 2;
    ++this->iCycle;
    this->Transition(EHBPhase::EHBP_Town);
    AHBPlayerCharacter* PlayerCharacter = this->GetPlayer();
    if (PlayerCharacter)
    {
        PlayerCharacter->ResetForPhase(this->GetTaggedLocation(this->TownSpawnTag, PlayerCharacter->GetActorLocation()));
    }
    this->Status = TEXT("Defense survived! Reward credited. Gate damage persists: repair before the next cycle.");
}

void AHBGameMode::ClearEnemies()
{
    const TArray<TWeakObjectPtr<AHBEnemyCharacter>> Snapshot = this->Enemies;
    for (const TWeakObjectPtr<AHBEnemyCharacter>& Entry : Snapshot)
    {
        if (Entry.IsValid()) { Entry->Destroy(); }
    }
    this->Enemies.Reset();
}

AHBStructure* AHBGameMode::GetDefenseTarget() const
{
    return this->Gate && this->Gate->IsAlive() ? this->Gate.Get() : this->Anima.Get();
}

AHBStructure* AHBGameMode::FindNearbyTurret(const FVector& Origin, float Range) const
{
    AHBStructure* Nearest = nullptr;
    float NearestDistance = FMath::Square(Range);
    for (AHBStructure* Turret : Structures)
    {
        if (!IsValid(Turret) || !Turret->IsTurret() || !Turret->IsAlive()) { continue; }
        const float Distance = FVector::DistSquaredXY(Origin, Turret->GetActorLocation());
        if (Distance < NearestDistance)
        {
            Nearest = Turret;
            NearestDistance = Distance;
        }
    }
    return Nearest;
}

AHBStructure* AHBGameMode::FindBlockingFence(const AActor* Mover, const FVector& Destination) const
{
    const FVector Location = Mover->GetActorLocation();
    const FVector Start(Location.X, Location.Y, Location.Z - 35.0);
    const FVector Direction = (Destination - Location).GetClampedToMaxSize2D(280.0);
    const FVector End = Start + FVector(Direction.X, Direction.Y, 0.0);
    FCollisionQueryParams Query(SCENE_QUERY_STAT(BuildFence), false, Mover);
    TArray<FHitResult> Hits;
    GetWorld()->SweepMultiByObjectType(Hits, Start, End, FQuat::Identity,
        FCollisionObjectQueryParams(ECC_WorldDynamic), FCollisionShape::MakeSphere(34.0f), Query);
    for (const FHitResult& Hit : Hits)
    {
        AHBStructure* Fence = Cast<AHBStructure>(Hit.GetActor());
        if (IsValid(Fence) && Fence->IsFence() && Fence->IsAlive())
        {
            return Fence;
        }
    }
    return nullptr;
}

AHBEnemyCharacter* AHBGameMode::FindEnemy(const FVector& Origin, const FVector& Aim, float Range) const
{
    AHBEnemyCharacter* Best = nullptr;
    float BestDistance = FMath::Square(Range);
    for (const TWeakObjectPtr<AHBEnemyCharacter>& Entry : this->Enemies)
    {
        AHBEnemyCharacter* Enemy = Entry.Get();
        if (!IsValid(Enemy) || !Enemy->IsAlive()) { continue; }
        const FVector Delta = Enemy->GetActorLocation() - Origin;
        const float Distance = Delta.SizeSquared2D();
        if (Distance < BestDistance && (Aim.IsNearlyZero() || FVector::DotProduct(Delta.GetSafeNormal2D(), Aim.GetSafeNormal2D()) >= 0.4f))
        {
            Best = Enemy;
            BestDistance = Distance;
        }
    }
    return Best;
}

void AHBGameMode::HandlePlayerDeath()
{
    if (this->Phase == EHBPhase::EHBP_Defense)
    {
        this->GetWorldTimerManager().SetTimer(this->RespawnTimer, this, &AHBGameMode::RespawnPlayer, 3.0f, false);
    }
    else
    {
        this->EndRun();
    }
}

void AHBGameMode::RespawnPlayer()
{
    if (this->Phase != EHBPhase::EHBP_Defense) { return; }
    AHBPlayerCharacter* PlayerCharacter = this->GetPlayer();
    if (PlayerCharacter)
    {
        PlayerCharacter->ResetForPhase(this->GetTaggedLocation(this->DefensePlayerSpawnTag, PlayerCharacter->GetActorLocation()));
    }
}

void AHBGameMode::EndRun()
{
    for (AActor* Cache : this->GreedCaches)
    {
        if (IsValid(Cache)) { Cache->SetActorHiddenInGame(true); }
    }
    this->Transition(EHBPhase::EHBP_GameOver);
    this->GetWorldTimerManager().ClearTimer(this->WaveTimer);
    this->GetWorldTimerManager().ClearTimer(this->BreakTimer);
    this->GetWorldTimerManager().ClearTimer(this->RespawnTimer);
    this->ClearEnemies();
    this->Status = TEXT("Run over. F5 / gamepad Special Left restarts the level.");
}

void AHBGameMode::ToggleBuildMode()
{
    if (this->Phase != EHBPhase::EHBP_Town && this->Phase != EHBPhase::EHBP_Return) { return; }
    AHBPlayerCharacter* PlayerCharacter = this->GetPlayer();
    if (PlayerCharacter) { PlayerCharacter->SetBuildMode(!PlayerCharacter->IsBuilding()); }
}

FVector AHBGameMode::GetBuildSocket() const
{
    return BuildCells.IsValidIndex(iBuildSocket) ? BuildCells[iBuildSocket] : FVector::ZeroVector;
}

bool AHBGameMode::IsBuildSocketOccupied() const
{
    return GetSelectedStructure() != nullptr;
}

AHBStructure* AHBGameMode::GetSelectedTurret() const
{
    AHBStructure* Structure = GetSelectedStructure();
    return Structure && Structure->IsTurret() ? Structure : nullptr;
}

AHBStructure* AHBGameMode::GetSelectedStructure() const
{
    AHBStructure* Structure = Structures.IsValidIndex(iBuildSocket) ? Structures[iBuildSocket].Get() : nullptr;
    return IsValid(Structure) && Structure->IsAlive() ? Structure : nullptr;
}

bool AHBGameMode::CanAffordSelectedBuild() const
{
    if (!BuildCells.IsValidIndex(iBuildSocket)) { return false; }
    if (const AHBStructure* Structure = GetSelectedStructure())
    {
        return BuildType == EHBStructure::EHBS_Turret && Structure->IsTurret() && !Structure->IsUpgraded() && iStones >= UpgradeCost;
    }
    const AHBPlayerCharacter* Player = GetPlayer();
    if (BuildType == EHBStructure::EHBS_Fence && Player &&
        FVector::DistSquaredXY(GetBuildSocket(), Player->GetActorLocation()) < FMath::Square(120.0))
    {
        return false;
    }
    return iStones >= (BuildType == EHBStructure::EHBS_Fence ? FenceCost : BuildCost);
}

void AHBGameMode::CycleBuildSocket()
{
    if (BuildCells.IsEmpty()) { return; }
    iBuildSocket = (iBuildSocket + 1) % BuildCells.Num();
}

bool AHBGameMode::SelectBuildCell(const FVector& Location)
{
    int32 Selected = INDEX_NONE;
    double Closest = FMath::Square(static_cast<double>(BuildCellRadius));
    for (int32 Index = 0; Index < BuildCells.Num(); ++Index)
    {
        const double Distance = FVector::DistSquaredXY(Location, BuildCells[Index]);
        if (Distance < Closest)
        {
            Closest = Distance;
            Selected = Index;
        }
    }
    if (Selected == INDEX_NONE) { return false; }
    const FVector Delta = (Location - BuildCells[Selected]).GetAbs();
    if (Delta.Y > 0.8660254037844386 * BuildCellRadius + 0.01 || Delta.X + 0.5773502691896258 * Delta.Y > BuildCellRadius + 0.01)
    {
        return false;
    }
    iBuildSocket = Selected;
    return true;
}

void AHBGameMode::MoveBuildSelection(const FVector& Direction)
{
    if (!BuildCells.IsValidIndex(iBuildSocket)) { return; }
    const FVector Origin = BuildCells[iBuildSocket];
    const FVector Input = Direction.GetSafeNormal2D();
    int32 Best = iBuildSocket;
    double BestAlignment = 0.72;
    for (int32 Index = 0; Index < BuildCells.Num(); ++Index)
    {
        const FVector Offset = BuildCells[Index] - Origin;
        if (Index == iBuildSocket || Offset.SizeSquared2D() > FMath::Square(1.8 * BuildCellRadius)) { continue; }
        const double Alignment = FVector::DotProduct(Offset.GetSafeNormal2D(), Input);
        if (Alignment > BestAlignment)
        {
            Best = Index;
            BestAlignment = Alignment;
        }
    }
    iBuildSocket = Best;
}

void AHBGameMode::SwitchBuildType()
{
    BuildType = BuildType == EHBStructure::EHBS_Turret ? EHBStructure::EHBS_Fence : EHBStructure::EHBS_Turret;
}

void AHBGameMode::RotateBuildFence()
{
    BuildYaw = FMath::Fmod(BuildYaw + 60.0f, 180.0f);
}

void AHBGameMode::ConfirmBuild()
{
    RequestConstruction(HBArenaTags::Build);
}

void AHBGameMode::RepairGate()
{
    RequestConstruction(HBArenaTags::Repair);
}

void AHBGameMode::RequestConstruction(FGameplayTag EventTag)
{
    if (AHBPlayerCharacter* Player = GetPlayer())
    {
        FGameplayEventData Event;
        Event.EventTag = EventTag;
        Event.Instigator = Player;
        Player->GetAbilitySystemComponent()->HandleGameplayEvent(EventTag, &Event);
    }
}

bool AHBGameMode::ExecuteConstruction(bool bRepair)
{
    AHBPlayerCharacter* Player = GetPlayer();
    if (!HasAuthority() || !IsValid(Player) || !Player->IsAlive() ||
        (Phase != EHBPhase::EHBP_Town && Phase != EHBPhase::EHBP_Return) || (!bRepair && !Player->IsBuilding()))
    {
        return false;
    }

    AHBStructure* Structure = GetSelectedStructure();
    if (bRepair)
    {
        AHBStructure* Target = Player->IsBuilding() && Structure ? Structure : Gate.Get();
        if (!IsValid(Target) || !Target->NeedsRepair())
        {
            Status = TEXT("Structure is already at full health.");
            return false;
        }
        if (iStones < RepairCost)
        {
            Status = FString::Printf(TEXT("Repair needs %d stones."), RepairCost);
            return false;
        }
        if (!Target->ApplyRepair(Player)) { return false; }
        iStones -= RepairCost;
        Target->PlayServiceCue();
        Status = Target == Gate ? TEXT("Gate repaired.") : TEXT("Structure repaired.");
        return true;
    }
    if (!Structures.IsValidIndex(iBuildSocket))
    {
        return false;
    }
    if (!CanAffordSelectedBuild())
    {
        Status = Structure ? TEXT("Cell occupied or upgrade unavailable.") : TEXT("Placement blocked or not enough Stones.");
        return false;
    }
    if (Structure)
    {
        if (!Structure->ApplyUpgrade(Player)) { return false; }
        iStones -= UpgradeCost;
        Status = TEXT("Turret upgraded. Damage and range increased.");
    }
    else
    {
        FActorSpawnParameters Parameters;
        Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        const bool bFence = BuildType == EHBStructure::EHBS_Fence;
        Structure = GetWorld()->SpawnActor<AHBStructure>(TurretClass, GetBuildSocket(),
            FRotator(0.0, bFence ? BuildYaw : 0.0, 0.0), Parameters);
        if (!IsValid(Structure)) { return false; }
        Structure->Configure(BuildType);
        Structures[iBuildSocket] = Structure;
        iStones -= bFence ? FenceCost : BuildCost;
        Status = bFence ? TEXT("Fence built.") : TEXT("Turret built.");
    }
    Structure->PlayServiceCue();
    return true;
}

float AHBGameMode::GetGateHealth() const { return IsValid(this->Gate) ? this->Gate->GetHealth() : 0.0f; }
float AHBGameMode::GetCoreHealth() const { return IsValid(this->Anima) ? this->Anima->GetHealth() : 0.0f; }

FString AHBGameMode::GetPhaseLabel() const
{
    switch (this->Phase)
    {
        case EHBPhase::EHBP_Town: return TEXT("TOWN");
        case EHBPhase::EHBP_Dungeon: return TEXT("DUNGEON");
        case EHBPhase::EHBP_Return: return TEXT("RETURN / PREP");
        case EHBPhase::EHBP_Defense: return TEXT("DEFENSE");
        default: return TEXT("GAME OVER");
    }
}

void AHBGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    this->GetWorldTimerManager().ClearAllTimersForObject(this);
    Super::EndPlay(EndPlayReason);
}

void AHBGameMode::ResetGreedCaches()
{
    this->Greed.Reset();
    for (int32 Index = 0; Index < this->GreedCaches.Num(); ++Index)
    {
        AActor* Cache = this->GreedCaches[Index];
        if (IsValid(Cache))
        {
            const bool bAvailable = Index < 3;
            Cache->SetActorHiddenInGame(!bAvailable);
        }
    }
}
FVector AHBGameMode::GetGreedPosition(int32 iIndex) const
{
    AActor* Cache = this->GreedCaches.IsValidIndex(iIndex)
        ? this->GreedCaches[iIndex].Get()
        : nullptr;

    return IsValid(Cache) ? Cache->GetActorLocation() : FVector::ZeroVector;
}

bool AHBGameMode::UsesDungeonRoute() const
{
    return DungeonExit.IsValid();
}

bool AHBGameMode::CanEditLoadout() const
{
    const AHBPlayerCharacter* Player = GetPlayer();
    return UsesDungeonRoute() && Player && Player->IsAlive() && !Player->IsBuilding() &&
        (Phase == EHBPhase::EHBP_Town || Phase == EHBPhase::EHBP_Return);
}

bool AHBGameMode::SpendSkillStones(int32 Cost)
{
    if (!CanEditLoadout() || Cost <= 0 || iStones < Cost) { return false; }
    iStones -= Cost;
    return true;
}

void AHBGameMode::SetDebugStones(int32 Stones)
{
#if !UE_BUILD_SHIPPING
    if (UsesDungeonRoute() && Phase != EHBPhase::EHBP_GameOver)
    {
        iStones = FMath::Clamp(Stones, 0, MaxDebugStones);
    }
#endif
}

int32 AHBGameMode::GetDungeonEnemiesRemaining() const
{
    int32 Remaining = 0;
    for (const TWeakObjectPtr<AHBEnemyCharacter>& Enemy : Enemies)
    {
        if (Enemy.IsValid() && Enemy->IsAlive() && (!UsesDungeonRoute() || !IsOptionalGuard(*Enemy))) { ++Remaining; }
    }
    return Remaining;
}

bool AHBGameMode::IsGreedLocked(int32 Index) const
{
    if (!UsesDungeonRoute() || Index < 0 || Index >= UE_ARRAY_COUNT(CacheGuardTags)) { return false; }
    for (const TWeakObjectPtr<AHBEnemyCharacter>& Enemy : Enemies)
    {
        if (Enemy.IsValid() && Enemy->IsAlive() && Enemy->ActorHasTag(CacheGuardTags[Index])) { return true; }
    }
    return false;
}

FVector AHBGameMode::GetDungeonExitPosition() const
{
    return UsesDungeonRoute() ? DungeonExit->GetActorLocation() : FVector::ZeroVector;
}

bool AHBGameMode::IsAtDungeonExit(const AHBPlayerCharacter& Player) const
{
    return UsesDungeonRoute() && FVector::DistSquaredXY(Player.GetActorLocation(), GetDungeonExitPosition()) <= FMath::Square(DungeonExitRadius);
}

FString AHBGameMode::GetDungeonExitHint(const AHBPlayerCharacter& Player) const
{
    if (!UsesDungeonRoute()) { return TEXT("Dungeon clear. Enter / D-pad Up returns to prep."); }
    if (IsAtDungeonExit(Player)) { return TEXT("Extraction ready. Enter / D-pad Up returns to the Dungeon camp."); }
    return TEXT("Dungeon clear. Follow the cyan beacon to extraction.");
}

void AHBGameMode::ClaimNearbyGreed()
{
    AHBPlayerCharacter* PlayerCharacter = this->GetPlayer();
    if (this->Phase != EHBPhase::EHBP_Dungeon || !PlayerCharacter || !PlayerCharacter->IsAlive() || PlayerCharacter->IsAttacking() || PlayerCharacter->IsDodging()) { return; }
    const FString ReturnHint = GetDungeonEnemiesRemaining() == 0 ? TEXT(" ") + GetDungeonExitHint(*PlayerCharacter) : FString();
    for (int32 Index = 0; Index < 3; ++Index)
    {
        AActor* Cache = this->GreedCaches.IsValidIndex(Index) ? this->GreedCaches[Index] : nullptr;
        if (IsValid(Cache) && !Cache->IsHidden() && FVector::DistSquaredXY(PlayerCharacter->GetActorLocation(), Cache->GetActorLocation()) <= FMath::Square(DungeonInteractRadius))
        {
            if (IsGreedLocked(Index))
            {
                Status = TEXT("Cache locked. Defeat the guards around this cache first.");
                return;
            }
            if (this->Greed.Claim(Index, this->iStones, this->iThreat))
            {
                Cache->SetActorHiddenInGame(true);
                this->Status = FString(TEXT("Cache claimed: +30 stones, +3 Threat.")) + ReturnHint;
                if (UsesDungeonRoute() && Index < 2)
                {
                    const int32 Item = Index == 0 ? 1 : 3;
                    if (PlayerCharacter->GetLoadout()->UnlockEquipment(Item))
                    {
                        Status = FString::Printf(TEXT("%s added to inventory. +30 Stones, +3 Threat."), *UHBLoadoutComponent::GetItemName(Item)) + ReturnHint;
                    }
                }
                return;
            }
        }
    }
    this->Status = FString(TEXT("Move closer to a gold cache and press E / L3 to open it.")) + ReturnHint;
}
