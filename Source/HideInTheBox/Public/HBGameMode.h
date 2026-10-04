#pragma once
#include "CoreMinimal.h"
#include "TimerManager.h"
#include "GameFramework/GameModeBase.h"
#include "HBTypes.h"
#include "HBGameMode.generated.h"
class AHBPlayerCharacter;
class AHBEnemyCharacter;
class AHBStructure;
class UStaticMesh;

UCLASS()
class HIDEINTHEBOX_API AHBGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    AHBGameMode();
    virtual void StartPlay() override;
    void AdvancePhase();
    void RegisterEnemy(AHBEnemyCharacter* Enemy);
    void UnregisterEnemy(AHBEnemyCharacter* Enemy);
    void NotifyEnemyKilled(AHBEnemyCharacter* Enemy);
    void HandlePlayerDeath();
    void EndRun();
    void ToggleBuildMode();
    void CycleBuildSocket();
    void ConfirmBuild();
    void RepairGate();
    AHBPlayerCharacter* GetPlayer() const;
    AHBStructure* GetDefenseTarget() const;
    AHBEnemyCharacter* FindEnemy(const FVector& Origin, const FVector& Aim, float fRange) const;
    const TArray<TWeakObjectPtr<AHBEnemyCharacter>>& GetEnemies() const { return this->Enemies; }
    EHBPhase GetPhase() const { return this->Phase; }
    int32 GetStones() const { return this->iStones; }
    int32 GetThreat() const { return this->iThreat; }
    int32 GetWave() const { return this->iWave; }
    int32 GetCycle() const { return this->iCycle; }
    float GetGateHealth() const;
    float GetCoreHealth() const;
    float GetWaveSeconds() const { return this->fWaveSeconds; }
    FVector GetBuildSocket() const;
    int32 GetBuildSocketIndex() const { return this->iBuildSocket; }
    bool IsBuildSocketOccupied() const;
    FString GetPhaseLabel() const;
    FString GetStatus() const { return this->Status; }
protected:
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    UPROPERTY(EditDefaultsOnly, Category="Demo") TSubclassOf<AHBEnemyCharacter> EnemyClass;
    UPROPERTY(EditDefaultsOnly, Category="Demo", meta=(ClampMin="1")) int32 iDungeonEnemyCount = 6;
    UPROPERTY(EditDefaultsOnly, Category="Demo", meta=(ClampMin="1")) int32 iWavesPerDefense = 5;
    UPROPERTY(EditDefaultsOnly, Category="Demo", meta=(ClampMin="5")) float fWaveDuration = 40.0f;
private:
    void CreateArena();
    void SpawnDungeon();
    void SpawnWave();
    void MonitorWave();
    void CompleteDefense();
    void ClearEnemies();
    void RespawnPlayer();
    void Transition(EHBPhase NewPhase);
    void SpawnEnemyAt(const FVector& Position);
    UPROPERTY() TObjectPtr<UStaticMesh> CubeMesh;
    UPROPERTY(Transient) TObjectPtr<AHBStructure> Gate;
    UPROPERTY(Transient) TObjectPtr<AHBStructure> Anima;
    UPROPERTY(Transient) TArray<TObjectPtr<AHBStructure>> Turrets;
    TArray<TWeakObjectPtr<AHBEnemyCharacter>> Enemies;
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
};
