#include "HBGameMode.h"
#include "HBDebugSubsystem.h"
#include "HBPlayerCharacter.h"
#include "HBPlayerController.h"
#include "HBEnemyCharacter.h"
#include "HBStructure.h"
#include "HBHUD.h"
#include "HideInTheBox.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "TimerManager.h"

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
        PlayerCharacter->ResetForPhase(this->GetTaggedLocation(this->TownSpawnTag, PlayerCharacter->GetActorLocation()));
    }
    this->Status = TEXT("Enter the dungeon with Enter / D-pad Up. Earn stones to build defenses.");
}

void AHBGameMode::ResetGASDemo()
{
    if (!this->bGASDemo) { return; }
    AHBPlayerCharacter* Player = this->GetPlayer();
    if (!Player) { return; }
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
    if (!this->bGASDemo || !Player || !Player->IsAlive()) { return; }
    Player->ApplyHealthDelta(-FMath::Min(60.0f, Player->GetHealth() - 1.0f));
    UHBDebugSubsystem::Record(this, TEXT("Combat"), TEXT("Practice: applied HealthDelta GE; use Mend to heal"));
}

void AHBGameMode::RefillGASDemoMana()
{
    AHBPlayerCharacter* Player = this->GetPlayer();
    if (!this->bGASDemo || !Player || !Player->IsAlive()) { return; }
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
    this->Turrets.SetNum(this->BuildSockets.Num());
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
        this->Gate->SetActorHiddenInGame(true);
        this->Anima->SetActorHiddenInGame(true);
        for (AHBStructure* Turret : this->Turrets)
        {
            if (IsValid(Turret)) { Turret->SetActorHiddenInGame(true); }
        }
        this->SpawnDungeon();
        this->ResetGreedCaches();
        this->Status = TEXT("Clear all dungeon enemies. LMB attacks; RMB / B-pad uses Pulse (20 MP).");
    }
    else if (this->Phase == EHBPhase::EHBP_Dungeon && this->Enemies.IsEmpty())
    {
        for (AActor* Cache : this->GreedCaches)
        {
            if (IsValid(Cache))
            {
                Cache->SetActorHiddenInGame(true);
            }
        }
        this->Transition(EHBPhase::EHBP_Return);
        PlayerCharacter->ResetForPhase(this->GetTaggedLocation(this->TownSpawnTag, PlayerCharacter->GetActorLocation()));
        this->Gate->SetActorHiddenInGame(!this->Gate->IsAlive());
        this->Anima->SetActorHiddenInGame(false);
        for (AHBStructure* Turret : this->Turrets)
        {
            if (IsValid(Turret)) { Turret->SetActorHiddenInGame(false); }
        }
        this->Status = TEXT("Return prep: B / D-pad Down builds (30 stones), R / D-pad Left repairs gate (20). Enter / D-pad Up starts defense.");
    }
    else if (this->Phase == EHBPhase::EHBP_Return)
    {
        this->Transition(EHBPhase::EHBP_Defense);
        this->iWave = 0;
        this->SpawnWave();
        this->GetWorldTimerManager().SetTimer(this->WaveTimer, this, &AHBGameMode::MonitorWave, 0.25f, true);
    }
}

void AHBGameMode::SpawnEnemyAt(const FVector& Position, const FRotator& Rotation)
{
    if (!this->EnemyClass || !IsValid(this->GetWorld()))
    {
        return;
    }
    FActorSpawnParameters Parameters;
    Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
    AHBEnemyCharacter* SpawnedEnemy = this->GetWorld()->SpawnActor<AHBEnemyCharacter>(this->EnemyClass, Position, Rotation, Parameters);
    if (!IsValid(SpawnedEnemy))
    {
        UE_LOG(LogHideInTheBox, Warning, TEXT("Enemy spawn failed at %s."), *Position.ToString());
    }
}

void AHBGameMode::SpawnDungeon()
{
    const int32 PointCount = this->DungeonSpawnPoints.Num();
    if (PointCount < 1) { return; }
    for (int32 Index = 0; Index < this->iDungeonEnemyCount; ++Index)
    {
        AActor* Point = this->DungeonSpawnPoints[Index % PointCount];
        if (!IsValid(Point)) { continue; }
        const FVector Offset(0.0f, 120.0f * static_cast<float>(Index / PointCount), 0.0f);
        this->SpawnEnemyAt(Point->GetActorLocation() + Offset, Point->GetActorRotation());
    }
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
        this->iStones += 15;
        ++this->iThreat;
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
    for (int32 Index = 0; Index < EnemyCount && PointCount > 0; ++Index)
    {
        AActor* Point = this->WaveSpawnPoints[Index % PointCount];
        if (!IsValid(Point)) { continue; }
        const int32 Ring = Index / PointCount;
        const FVector Offset(150.0f * static_cast<float>(Ring), 110.0f * static_cast<float>((Index % 3) - 1), 0.0f);
        this->SpawnEnemyAt(Point->GetActorLocation() + Offset, Point->GetActorRotation());
    }
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
    return this->BuildSockets.IsValidIndex(this->iBuildSocket) && IsValid(this->BuildSockets[this->iBuildSocket])
        ? this->BuildSockets[this->iBuildSocket]->GetActorLocation() : FVector::ZeroVector;
}

bool AHBGameMode::IsBuildSocketOccupied() const
{
    return this->Turrets.IsValidIndex(this->iBuildSocket) && IsValid(this->Turrets[this->iBuildSocket]);
}

void AHBGameMode::CycleBuildSocket()
{
    if (this->BuildSockets.IsEmpty()) { return; }
    this->iBuildSocket = (this->iBuildSocket + 1) % this->BuildSockets.Num();
}

void AHBGameMode::ConfirmBuild()
{
    AHBPlayerCharacter* PlayerCharacter = this->GetPlayer();
    if (!PlayerCharacter || !PlayerCharacter->IsBuilding() || (this->Phase != EHBPhase::EHBP_Town && this->Phase != EHBPhase::EHBP_Return)) { return; }
    if (this->iStones < 30 || this->IsBuildSocketOccupied())
    {
        this->Status = TEXT("Need 30 stones and an empty socket. Tab / D-pad Right selects the next socket.");
        return;
    }
    FActorSpawnParameters Parameters;
    Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
    AHBStructure* Turret = this->GetWorld()->SpawnActor<AHBStructure>(this->TurretClass, this->GetBuildSocket(), FRotator::ZeroRotator, Parameters);
    if (IsValid(Turret))
    {
        Turret->Configure(EHBStructure::EHBS_Turret);
        this->Turrets[this->iBuildSocket] = Turret;
        this->iStones -= 30;
        this->Status = TEXT("Turret built. B / D-pad Down leaves build mode; Enter / D-pad Up starts defense.");
    }
}

void AHBGameMode::RepairGate()
{
    if ((this->Phase != EHBPhase::EHBP_Town && this->Phase != EHBPhase::EHBP_Return) || !IsValid(this->Gate) || !this->Gate->NeedsRepair()) { return; }
    if (this->iStones < 20)
    {
        this->Status = TEXT("Gate repair needs 20 stones.");
        return;
    }
    this->iStones -= 20;
    this->Gate->Repair();
    this->Status = TEXT("Gate repaired.");
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
void AHBGameMode::ClaimNearbyGreed()
{
    AHBPlayerCharacter* PlayerCharacter = this->GetPlayer();
    if (this->Phase != EHBPhase::EHBP_Dungeon || !PlayerCharacter || !PlayerCharacter->IsAlive() || PlayerCharacter->IsAttacking() || PlayerCharacter->IsDodging()) { return; }
    for (int32 Index = 0; Index < 3; ++Index)
    {
        AActor* Cache = this->GreedCaches.IsValidIndex(Index) ? this->GreedCaches[Index] : nullptr;
        if (IsValid(Cache) && !Cache->IsHidden() && FVector::DistSquaredXY(PlayerCharacter->GetActorLocation(), Cache->GetActorLocation()) <= FMath::Square(180.0f))
        {
            if (this->Greed.Claim(Index, this->iStones, this->iThreat))
            {
                Cache->SetActorHiddenInGame(true);
                this->Status = TEXT("Optional cache claimed: +30 stones, +3 Threat. Next defense gains one enemy per wave.");
                return;
            }
        }
    }
    this->Status = TEXT("Move within 180 cm of an optional gold cache; E / Left-stick click opens it.");
}
