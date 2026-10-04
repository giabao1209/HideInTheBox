#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "HBFrameStats.h"
#include "HBDebugSubsystem.generated.h"

/** Bounded, world-local diagnostic history. No actor references; frame samples are separate from event history. */
struct FHBDebugEntry
{
    uint64 Sequence = 0;
    float Time = 0.0f;
    FName Category;
    FString Message;
};

UCLASS()
class HIDEINTHEBOX_API UHBDebugSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()
public:
    void SampleFrame(uint64 Frame, double Now, bool bPaused);
    void ClearPerformance() { this->FrameStats.Reset(); }
    const HBPerformance::FFrameStats& GetFrameStats() const { return this->FrameStats; }
    bool IsPerformancePaused() const { return this->bPerformancePaused; }
    static void Record(const UObject* Context, FName Category, const FString& Message);
    void Add(FName Category, const FString& Message);
    void Clear();
    void SetChecks(const TArray<FString>& InChecks);
    void Dump() const;
    void ToggleFreeze() { this->bFrozen = !this->bFrozen; }
    bool IsFrozen() const { return this->bFrozen; }
    void SetFilter(FName Category) { this->Filter = Category; }
    FName GetFilter() const { return this->Filter; }
    void GetRecent(TArray<FString>& Lines, int32 Count) const;
    const TArray<FString>& GetChecks() const { return this->Checks; }
private:
    HBPerformance::FFrameStats FrameStats;
    bool bPerformancePaused = false;
    TArray<FHBDebugEntry> Entries;
    TArray<FString> Checks;
    uint64 NextSequence = 1;
    bool bFrozen = false;
    FName Filter = NAME_None;
};
