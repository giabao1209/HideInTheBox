#include "HBGameMode.h"
#include "HBDebugSubsystem.h"
#include "HBPlayerCharacter.h"
#include "HBPlayerController.h"
#include "HBEnemyCharacter.h"
#include "HBStructure.h"
#include "HBHUD.h"
#include "HideInTheBox.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/DirectionalLight.h"
#include "Engine/PointLight.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/LightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "TimerManager.h"

AHBGameMode::AHBGameMode()
{
    this->DefaultPawnClass = AHBPlayerCharacter::StaticClass();
    this->PlayerControllerClass = AHBPlayerController::StaticClass();
    this->HUDClass = AHBHUD::StaticClass();
    this->EnemyClass = AHBEnemyCharacter::StaticClass();
    this->Enemies.Reserve(32);
    this->Turrets.SetNum(4);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> MeshAsset(TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (MeshAsset.Succeeded())
    {
        this->CubeMesh = MeshAsset.Object;
    }
}

void AHBGameMode::StartPlay()
{
    this->CreateArena();
    Super::StartPlay();
    if (!IsValid(this->Gate) || !IsValid(this->Anima))
    {
        UE_LOG(LogHideInTheBox, Error, TEXT("Arena objective creation failed."));
        this->EndRun();
        return;
    }
    AHBPlayerCharacter* Player = this->GetPlayer();
    if (Player)
    {
        Player->ResetForPhase(FVector(-400.0f, 0.0f, 100.0f));
    }
    this->Status = TEXT("Enter the dungeon with Enter / D-pad Up. Earn stones to build defenses.");
}

void AHBGameMode::CreateArena()
{
    FActorSpawnParameters Parameters;
    Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AStaticMeshActor* Floor = this->GetWorld()->SpawnActor<AStaticMeshActor>(FVector(0.0f, 0.0f, -50.0f), FRotator::ZeroRotator, Parameters);
    if (Floor)
    {
        Floor->SetMobility(EComponentMobility::Movable);
        Floor->GetStaticMeshComponent()->SetStaticMesh(this->CubeMesh);
        if (this->FloorMaterial)
        {
            Floor->GetStaticMeshComponent()->SetMaterial(0, this->FloorMaterial);
        }
        Floor->SetActorScale3D(FVector(40.0f, 32.0f, 1.0f));
        Floor->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
        Floor->GetStaticMeshComponent()->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Block);
    }
    // Low boundary walls keep direct movement inside the test arena.
    for (int32 iIndex = 0; iIndex < 4; ++iIndex)
    {
        const bool bAlongX = iIndex < 2;
        const float fSign = iIndex % 2 == 0 ? -1.0f : 1.0f;
        const FVector Position = bAlongX ? FVector(fSign * 2000.0f, 0.0f, 80.0f) : FVector(0.0f, fSign * 1600.0f, 80.0f);
        AStaticMeshActor* Wall = this->GetWorld()->SpawnActor<AStaticMeshActor>(Position, FRotator::ZeroRotator, Parameters);
        if (Wall)
        {
            Wall->SetMobility(EComponentMobility::Movable);
            Wall->GetStaticMeshComponent()->SetStaticMesh(this->CubeMesh);
            if (this->WallMaterial)
            {
                Wall->GetStaticMeshComponent()->SetMaterial(0, this->WallMaterial);
            }
            Wall->SetActorScale3D(bAlongX ? FVector(0.5f, 32.0f, 2.0f) : FVector(40.0f, 0.5f, 2.0f));
            Wall->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
            Wall->GetStaticMeshComponent()->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Ignore);
        }
    }
    ADirectionalLight* Light = this->GetWorld()->SpawnActor<ADirectionalLight>(FVector(0.0f, 0.0f, 1000.0f), FRotator(-65.0f, -30.0f, 0.0f), Parameters);
    if (Light)
    {
        Light->GetLightComponent()->SetIntensity(5.0f);
    }
    for (int32 iIndex = 0; iIndex < 4; ++iIndex)
    {
        const FVector Position(iIndex < 2 ? -1300.0f : 1300.0f, iIndex % 2 == 0 ? -1100.0f : 1100.0f, 350.0f);
        APointLight* Accent = this->GetWorld()->SpawnActor<APointLight>(Position, FRotator::ZeroRotator, Parameters);
        if (Accent)
        {
            UPointLightComponent* Component = Cast<UPointLightComponent>(Accent->GetLightComponent());
            if (Component)
            {
                Component->SetMobility(EComponentMobility::Movable);
                Component->SetIntensity(3500.0f);
                Component->SetAttenuationRadius(1300.0f);
                Component->SetCastShadows(false);
                Component->SetLightColor(iIndex < 2 ? FLinearColor(0.1f, 0.55f, 1.0f) : FLinearColor(1.0f, 0.35f, 0.08f));
            }
        }
    }
    this->Gate = this->GetWorld()->SpawnActor<AHBStructure>(FVector(450.0f, 0.0f, 75.0f), FRotator::ZeroRotator, Parameters);
    this->Anima = this->GetWorld()->SpawnActor<AHBStructure>(FVector(-1000.0f, 0.0f, 75.0f), FRotator::ZeroRotator, Parameters);
    if (this->Gate)
    {
        this->Gate->SetTintMaterial(this->TintMaterial);
        this->Gate->Configure(EHBStructure::EHBS_Gate);
    }
    if (this->Anima)
    {
        this->Anima->SetTintMaterial(this->TintMaterial);
        this->Anima->Configure(EHBStructure::EHBS_Anima);
    }
}

AHBPlayerCharacter* AHBGameMode::GetPlayer() const
{
    return Cast<AHBPlayerCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
}

void AHBGameMode::Transition(EHBPhase NewPhase)
{
    UHBDebugSubsystem::Record(this, TEXT("System"), FString::Printf(TEXT("Phase %d -> %d"), static_cast<int32>(this->Phase), static_cast<int32>(NewPhase)));
    this->Phase = NewPhase;
    AHBPlayerController* Controller = Cast<AHBPlayerController>(UGameplayStatics::GetPlayerController(this, 0));
    if (Controller)
    {
        Controller->ResetTransientInput();
    }
    AHBPlayerCharacter* Player = this->GetPlayer();
    if (Player)
    {
        Player->SetBuildMode(false);
    }
    UE_LOG(LogHideInTheBox, Log, TEXT("Phase=%s cycle=%d stones=%d threat=%d"), *this->GetPhaseLabel(), this->iCycle, this->iStones, this->iThreat);
}

void AHBGameMode::AdvancePhase()
{
    AHBPlayerCharacter* Player = this->GetPlayer();
    if (!Player || !Player->IsAlive())
    {
        return;
    }
    if (this->Phase == EHBPhase::EHBP_Town)
    {
        this->iThreat = 0;
        this->Transition(EHBPhase::EHBP_Dungeon);
        Player->ResetForPhase(FVector(-700.0f, 0.0f, 100.0f));
        this->Gate->SetActorHiddenInGame(true);
        this->Anima->SetActorHiddenInGame(true);
        for (AHBStructure* Turret : this->Turrets)
        {
            if (Turret)
            {
                Turret->SetActorHiddenInGame(true);
            }
        }
        this->SpawnDungeon();
        this->SpawnGreedChests();
        this->Status = TEXT("Clear all six enemies. LMB attacks; RMB / B-pad uses Pulse (20 MP).");
    }
    else if (this->Phase == EHBPhase::EHBP_Dungeon && this->Enemies.IsEmpty())
    {
        this->ClearGreedChests();
        this->Transition(EHBPhase::EHBP_Return);
        Player->ResetForPhase(FVector(-400.0f, 0.0f, 100.0f));
        this->Gate->SetActorHiddenInGame(!this->Gate->IsAlive());
        this->Anima->SetActorHiddenInGame(false);
        for (AHBStructure* Turret : this->Turrets)
        {
            if (Turret)
            {
                Turret->SetActorHiddenInGame(false);
            }
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

void AHBGameMode::SpawnEnemyAt(const FVector& Position)
{
    FActorSpawnParameters Parameters;
    Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
    this->GetWorld()->SpawnActor<AHBEnemyCharacter>(this->EnemyClass, Position, FRotator::ZeroRotator, Parameters);
}

void AHBGameMode::SpawnDungeon()
{
    for (int32 iIndex = 0; iIndex < this->iDungeonEnemyCount; ++iIndex)
    {
        this->SpawnEnemyAt(FVector(200.0f + (iIndex / 3) * 400.0f, (iIndex % 3 - 1) * 350.0f, 100.0f));
    }
}

void AHBGameMode::RegisterEnemy(AHBEnemyCharacter* Enemy)
{
    this->Enemies.AddUnique(Enemy);
}

void AHBGameMode::UnregisterEnemy(AHBEnemyCharacter* Enemy)
{
    this->Enemies.Remove(Enemy);
}

void AHBGameMode::NotifyEnemyKilled(AHBEnemyCharacter* Enemy)
{
    if (this->Phase == EHBPhase::EHBP_Dungeon)
    {
        this->iStones += 15;
        ++this->iThreat;
    }
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
    const int32 iCount = HBRules::GetWaveEnemyCount(this->iWave, this->iThreat);
    for (int32 iIndex = 0; iIndex < iCount; ++iIndex)
    {
        this->SpawnEnemyAt(FVector(1250.0f + (iIndex / 5) * 150.0f, (iIndex % 5 - 2) * 180.0f, 100.0f));
    }
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
        this->bWaveActive = false;
        this->ClearEnemies();
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
    AHBPlayerCharacter* Player = this->GetPlayer();
    if (Player)
    {
        Player->ResetForPhase(FVector(-400.0f, 0.0f, 100.0f));
    }
    this->Status = TEXT("Defense survived! Reward credited. Gate damage persists: repair before the next cycle.");
}

void AHBGameMode::ClearEnemies()
{
    const TArray<TWeakObjectPtr<AHBEnemyCharacter>> Snapshot = this->Enemies;
    for (const TWeakObjectPtr<AHBEnemyCharacter>& Entry : Snapshot)
    {
        if (Entry.IsValid())
        {
            Entry->Destroy();
        }
    }
    this->Enemies.Reset();
}

AHBStructure* AHBGameMode::GetDefenseTarget() const
{
    return this->Gate && this->Gate->IsAlive() ? this->Gate.Get() : this->Anima.Get();
}

AHBEnemyCharacter* AHBGameMode::FindEnemy(const FVector& Origin, const FVector& Aim, float fRange) const
{
    AHBEnemyCharacter* Best = nullptr;
    float fBestDistance = FMath::Square(fRange);
    for (const TWeakObjectPtr<AHBEnemyCharacter>& Entry : this->Enemies)
    {
        AHBEnemyCharacter* Enemy = Entry.Get();
        if (!IsValid(Enemy) || !Enemy->IsAlive())
        {
            continue;
        }
        const FVector Delta = Enemy->GetActorLocation() - Origin;
        const float fDistance = Delta.SizeSquared2D();
        if (fDistance < fBestDistance && (Aim.IsNearlyZero() || FVector::DotProduct(Delta.GetSafeNormal2D(), Aim.GetSafeNormal2D()) >= 0.4f))
        {
            Best = Enemy;
            fBestDistance = fDistance;
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
    if (this->Phase != EHBPhase::EHBP_Defense)
    {
        return;
    }
    AHBPlayerCharacter* Player = this->GetPlayer();
    if (Player)
    {
        Player->ResetForPhase(FVector(-700.0f, 0.0f, 100.0f));
    }
}

void AHBGameMode::EndRun()
{
    this->ClearGreedChests();
    this->Transition(EHBPhase::EHBP_GameOver);
    this->GetWorldTimerManager().ClearTimer(this->WaveTimer);
    this->GetWorldTimerManager().ClearTimer(this->BreakTimer);
    this->GetWorldTimerManager().ClearTimer(this->RespawnTimer);
    this->ClearEnemies();
    this->Status = TEXT("Run over. F5 / gamepad Special Left restarts the level.");
}

void AHBGameMode::ToggleBuildMode()
{
    if (this->Phase != EHBPhase::EHBP_Town && this->Phase != EHBPhase::EHBP_Return)
    {
        return;
    }
    AHBPlayerCharacter* Player = this->GetPlayer();
    if (Player)
    {
        Player->SetBuildMode(!Player->IsBuilding());
    }
}

FVector AHBGameMode::GetBuildSocket() const
{
    return FVector(this->iBuildSocket < 2 ? 100.0f : -500.0f, this->iBuildSocket % 2 == 0 ? -350.0f : 350.0f, 90.0f);
}

bool AHBGameMode::IsBuildSocketOccupied() const
{
    return IsValid(this->Turrets[this->iBuildSocket]);
}

void AHBGameMode::CycleBuildSocket()
{
    this->iBuildSocket = (this->iBuildSocket + 1) % this->Turrets.Num();
}

void AHBGameMode::ConfirmBuild()
{
    AHBPlayerCharacter* Player = this->GetPlayer();
    if (!Player || !Player->IsBuilding() || (this->Phase != EHBPhase::EHBP_Town && this->Phase != EHBPhase::EHBP_Return))
    {
        return;
    }
    if (this->iStones < 30 || this->IsBuildSocketOccupied())
    {
        this->Status = TEXT("Need 30 stones and an empty socket. Tab / D-pad Right selects the next socket.");
        return;
    }
    AHBStructure* Turret = this->GetWorld()->SpawnActor<AHBStructure>(this->GetBuildSocket(), FRotator::ZeroRotator);
    if (Turret)
    {
        Turret->SetTintMaterial(this->TintMaterial);
        Turret->Configure(EHBStructure::EHBS_Turret);
        this->Turrets[this->iBuildSocket] = Turret;
        this->iStones -= 30;
        this->Status = TEXT("Turret built. B / D-pad Down leaves build mode; Enter / D-pad Up starts defense.");
    }
}

void AHBGameMode::RepairGate()
{
    if ((this->Phase != EHBPhase::EHBP_Town && this->Phase != EHBPhase::EHBP_Return) || !this->Gate || !this->Gate->NeedsRepair())
    {
        return;
    }
    if (this->iStones < 20)
    {
        this->Status = TEXT("Gate repair needs 20 stones.");
        return;
    }
    this->iStones -= 20;
    this->Gate->Repair();
    this->Status = TEXT("Gate repaired.");
}

float AHBGameMode::GetGateHealth() const { return this->Gate ? this->Gate->GetHealth() : 0.0f; }
float AHBGameMode::GetCoreHealth() const { return this->Anima ? this->Anima->GetHealth() : 0.0f; }

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

FVector AHBGameMode::GetGreedPosition(int32 iIndex) const
{
    const FVector Positions[] = { FVector(900.0f, -1000.0f, 45.0f), FVector(1500.0f, 0.0f, 45.0f), FVector(900.0f, 1000.0f, 45.0f) };
    return iIndex >= 0 && iIndex < 3 ? Positions[iIndex] : FVector::ZeroVector;
}

void AHBGameMode::SpawnGreedChests()
{
    this->ClearGreedChests();
    this->Greed.Reset();
    for (int32 iIndex = 0; iIndex < 3; ++iIndex)
    {
        AStaticMeshActor* Chest = this->GetWorld()->SpawnActor<AStaticMeshActor>(this->GetGreedPosition(iIndex), FRotator::ZeroRotator);
        this->GreedChests.Add(Chest);
        if (Chest)
        {
            Chest->SetMobility(EComponentMobility::Movable);
            Chest->GetStaticMeshComponent()->SetStaticMesh(this->CubeMesh);
            Chest->SetActorScale3D(FVector(0.9f, 0.65f, 0.6f));
            Chest->GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            if (this->TintMaterial)
            {
                Chest->GetStaticMeshComponent()->SetMaterial(0, this->TintMaterial);
            }
            UMaterialInstanceDynamic* Material = Chest->GetStaticMeshComponent()->CreateAndSetMaterialInstanceDynamic(0);
            if (Material) { Material->SetVectorParameterValue(TEXT("Color"), FLinearColor(1.0f, 0.6f, 0.08f)); }
        }
    }
}

void AHBGameMode::ClearGreedChests()
{
    for (AStaticMeshActor* Chest : this->GreedChests)
    {
        if (IsValid(Chest))
        {
            Chest->Destroy();
        }
    }
    this->GreedChests.Reset();
}

void AHBGameMode::ClaimNearbyGreed()
{
    AHBPlayerCharacter* Player = this->GetPlayer();
    if (this->Phase != EHBPhase::EHBP_Dungeon || !Player || !Player->IsAlive() || Player->IsAttacking() || Player->IsDodging())
    {
        return;
    }
    for (int32 iIndex = 0; iIndex < 3; ++iIndex)
    {
        if (this->GreedChests.IsValidIndex(iIndex) && IsValid(this->GreedChests[iIndex]) && FVector::DistSquaredXY(Player->GetActorLocation(), this->GetGreedPosition(iIndex)) <= FMath::Square(180.0f))
        {
            if (this->Greed.Claim(iIndex, this->iStones, this->iThreat))
            {
                this->GreedChests[iIndex]->Destroy();
                this->GreedChests[iIndex] = nullptr;
                this->Status = TEXT("Optional cache claimed: +30 stones, +3 Threat. Next defense gains one enemy per wave.");
                return;
            }
        }
    }
    this->Status = TEXT("Move within 180 cm of an optional gold cache; E / Left-stick click opens it.");
}
