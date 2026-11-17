#pragma once
#include "CoreMinimal.h"
#include "TimerManager.h"
#include "GameFramework/GameModeBase.h"
#include "HBTypes.h"
#include "HBGameMode.generated.h"
class AHBPlayerCharacter;
class AHBEnemyCharacter;
class AHBStructure;
class AActor;
class UUserWidget;
struct FGameplayTag;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FHBPhaseChanged, EHBPhase, PreviousPhase, EHBPhase, NewPhase);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FHBWaveStarted, int32, WaveIndex, int32, EnemyCount);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FHBWaveResolved, int32, WaveIndex, bool, bTimedOut);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FHBEnemyDefeated, AHBEnemyCharacter*, Enemy, EHBPhase, PhaseAtDefeat);

UCLASS()
class HIDEINTHEBOX_API AHBGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    AHBGameMode();
    virtual void StartPlay() override;
    bool IsGASDemo() const { return this->bGASDemo; }
    TSubclassOf<UUserWidget> GetPracticeHUDClass() const { return PracticeHUDClass; }
    void ResetGASDemo();
    void DamageGASDemoPlayer();
    void RefillGASDemoMana();

    /** C++ owns the phase/economy/wave rules. Blueprint GameMode supplies class/data defaults and presentation. */
    void AdvancePhase();

    UFUNCTION(BlueprintCallable, Category="Demo|Build") void ToggleBuildMode();
    UFUNCTION(BlueprintCallable, Category="Demo|Build") void CycleBuildSocket();
    UFUNCTION(BlueprintCallable, Category="Demo|Build") void ConfirmBuild();
    UFUNCTION(BlueprintCallable, Category="Demo|Build") void RepairGate();
    UFUNCTION(BlueprintCallable, Category="Demo|Dungeon") void ClaimNearbyGreed();
    UFUNCTION(BlueprintCallable, Category="Demo|Flow") void EndRun();
    UFUNCTION(BlueprintPure, Category="Demo|Dungeon")
    FVector GetGreedPosition(int32 iIndex) const;
    void RegisterEnemy(AHBEnemyCharacter* Enemy);
    void UnregisterEnemy(AHBEnemyCharacter* Enemy);
    void NotifyEnemyKilled(AHBEnemyCharacter* Enemy);
    void HandlePlayerDeath();

    AHBPlayerCharacter* GetPlayer() const;
    AHBStructure* GetDefenseTarget() const;
    AHBStructure* FindNearbyTurret(const FVector& Origin, float Range) const;
    AHBStructure* FindBlockingFence(const AActor* Mover, const FVector& Destination) const;
    AHBStructure* GetSelectedTurret() const;
    AHBStructure* GetSelectedStructure() const;
    bool ExecuteConstruction(bool bRepair);
    bool CanAffordSelectedBuild() const;
    const TArray<FVector>& GetBuildCells() const { return BuildCells; }
    bool SelectBuildCell(const FVector& Location);
    void MoveBuildSelection(const FVector& Direction);
    void SwitchBuildType();
    void RotateBuildFence();
    EHBStructure GetBuildType() const { return BuildType; }
    float GetBuildYaw() const { return BuildYaw; }
    static constexpr float BuildCellRadius = 104.0f;
    static constexpr int32 BuildCost = 30;
    static constexpr int32 FenceCost = 15;
    static constexpr int32 UpgradeCost = 25;
    static constexpr int32 RepairCost = 20;
    AHBEnemyCharacter* FindEnemy(const FVector& Origin, const FVector& Aim, float fRange) const;
    const TArray<TWeakObjectPtr<AHBEnemyCharacter>>& GetEnemies() const { return this->Enemies; }
    UFUNCTION(BlueprintPure, Category="Demo|State") EHBPhase GetPhase() const { return this->Phase; }
    UFUNCTION(BlueprintPure, Category="Demo|State") int32 GetStones() const { return this->iStones; }
    UFUNCTION(BlueprintPure, Category="Demo|State") int32 GetThreat() const { return this->iThreat; }
    UFUNCTION(BlueprintPure, Category="Demo|State") int32 GetWave() const { return this->iWave; }
    UFUNCTION(BlueprintPure, Category="Demo|State") int32 GetCycle() const { return this->iCycle; }
    UFUNCTION(BlueprintPure, Category="Demo|State") float GetGateHealth() const;
    UFUNCTION(BlueprintPure, Category="Demo|State") float GetCoreHealth() const;
    UFUNCTION(BlueprintPure, Category="Demo|State") float GetWaveSeconds() const { return this->fWaveSeconds; }
    UFUNCTION(BlueprintPure, Category="Demo|Build") FVector GetBuildSocket() const;
    UFUNCTION(BlueprintPure, Category="Demo|Build") int32 GetBuildSocketIndex() const { return this->iBuildSocket; }
    UFUNCTION(BlueprintPure, Category="Demo|Build") bool IsBuildSocketOccupied() const;
    UFUNCTION(BlueprintPure, Category="Demo|State") FString GetPhaseLabel() const;
    UFUNCTION(BlueprintPure, Category="Demo|State") FString GetStatus() const { return this->Status; }
    UFUNCTION(BlueprintPure, Category="Demo|Dungeon") bool IsGreedClaimed(int32 iIndex) const { return this->Greed.IsClaimed(iIndex); }

    UPROPERTY(BlueprintAssignable, Category="Demo|Events") FHBPhaseChanged OnPhaseChanged;
    UPROPERTY(BlueprintAssignable, Category="Demo|Events") FHBWaveStarted OnWaveStarted;
    UPROPERTY(BlueprintAssignable, Category="Demo|Events") FHBWaveResolved OnWaveResolved;
    UPROPERTY(BlueprintAssignable, Category="Demo|Events") FHBEnemyDefeated OnEnemyDefeated;

protected:
    UPROPERTY(EditDefaultsOnly, Category="GAS Demo") bool bGASDemo = false;
    UPROPERTY(EditDefaultsOnly, Category="GAS Demo") TSubclassOf<UUserWidget> PracticeHUDClass;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    UPROPERTY(EditDefaultsOnly, Category="Demo|Classes") TSubclassOf<AHBEnemyCharacter> EnemyClass;
    UPROPERTY(EditDefaultsOnly, Category="Demo|Classes") TSubclassOf<AHBStructure> TurretClass;
    UPROPERTY(EditDefaultsOnly, Category="Demo|Map Tags") FName TownSpawnTag = TEXT("HB_TownSpawn");
    UPROPERTY(EditDefaultsOnly, Category="Demo|Map Tags") FName DungeonPlayerSpawnTag = TEXT("HB_DungeonPlayerSpawn");
    UPROPERTY(EditDefaultsOnly, Category="Demo|Map Tags") FName DefensePlayerSpawnTag = TEXT("HB_DefensePlayerSpawn");
    UPROPERTY(EditDefaultsOnly, Category="Demo|Map Tags") FName GateTag = TEXT("HB_Gate");
    UPROPERTY(EditDefaultsOnly, Category="Demo|Map Tags") FName AnimaTag = TEXT("HB_Anima");
    UPROPERTY(EditDefaultsOnly, Category="Demo|Map Tags") FName BuildSocketTag = TEXT("HB_BuildSocket");
    UPROPERTY(EditDefaultsOnly, Category="Demo|Map Tags") FName GreedCacheTag = TEXT("HB_GreedCache");
    UPROPERTY(EditDefaultsOnly, Category="Demo|Map Tags") FName DungeonSpawnTag = TEXT("HB_DungeonSpawn");
    UPROPERTY(EditDefaultsOnly, Category="Demo|Map Tags") FName WaveSpawnTag = TEXT("HB_WaveSpawn");
    UPROPERTY(EditDefaultsOnly, Category="Demo", meta=(ClampMin="1")) int32 iDungeonEnemyCount = 6;
    UPROPERTY(EditDefaultsOnly, Category="Demo", meta=(ClampMin="1")) int32 iWavesPerDefense = 5;
    UPROPERTY(EditDefaultsOnly, Category="Demo", meta=(ClampMin="5")) float fWaveDuration = 40.0f;

private:
    bool DiscoverArenaActors();
    bool CreateBuildBoard();
    AActor* FindFirstTaggedActor(FName Tag) const;
    void FindTaggedActors(FName Tag, TArray<TObjectPtr<AActor>>& OutActors) const;
    FVector GetTaggedLocation(FName Tag, const FVector& Fallback) const;
    void SpawnDungeon();
    void ResetGreedCaches();
    void SpawnWave();
    void MonitorWave();
    void CompleteDefense();
    void ClearEnemies();
    void RespawnPlayer();
    void Transition(EHBPhase NewPhase);
    AHBEnemyCharacter* SpawnEnemyAt(const FVector& Position, const FRotator& Rotation);
    void RequestConstruction(FGameplayTag EventTag);
    UPROPERTY(Transient) TObjectPtr<AHBStructure> Gate;
    UPROPERTY(Transient) TObjectPtr<AHBStructure> Anima;
    UPROPERTY(Transient) TArray<TObjectPtr<AActor>> BuildSockets;
    UPROPERTY(Transient) TArray<TObjectPtr<AActor>> GreedCaches;
    UPROPERTY(Transient) TArray<TObjectPtr<AActor>> DungeonSpawnPoints;
    UPROPERTY(Transient) TArray<TObjectPtr<AActor>> WaveSpawnPoints;
    UPROPERTY(Transient) TArray<TObjectPtr<AHBStructure>> Structures;
    TArray<FVector> BuildCells;
    EHBStructure BuildType = EHBStructure::EHBS_Turret;
    float BuildYaw = 0.0f;
    TArray<TWeakObjectPtr<AHBEnemyCharacter>> Enemies;
    HBRuleCore::FGreedLedger Greed;
    EHBPhase Phase = EHBPhase::EHBP_Town;
    int32 iStones = 0;
    int32 iThreat = 0;
    int32 iWave = 0;
    int32 iCycle = 1;
    int32 iBuildSocket = 0;
    float fWaveSeconds = 0.0f;
    bool bWaveActive = false;
    FString Status;
    FTimerHandle WaveTimer;
    FTimerHandle BreakTimer;
    FTimerHandle RespawnTimer;

public:
    bool UsesDungeonRoute() const;
    int32 GetDungeonEnemiesRemaining() const;
    bool IsGreedLocked(int32 Index) const;
    FVector GetDungeonExitPosition() const;
    bool IsAtDungeonExit(const AHBPlayerCharacter& Player) const;
    bool CanEditLoadout() const;
    bool SpendSkillStones(int32 Cost);
    static constexpr int32 MaxDebugStones = 999999;
    void SetDebugStones(int32 Stones);

private:
    FString GetDungeonExitHint(const AHBPlayerCharacter& Player) const;
    TWeakObjectPtr<AActor> DungeonExit;
};
