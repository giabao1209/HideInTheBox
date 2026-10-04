#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "HBDebugSubsystem.generated.h"

/** Bounded, world-local diagnostic history. No actor references or per-frame recording. */
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
    TArray<FHBDebugEntry> Entries;
    TArray<FString> Checks;
    uint64 NextSequence = 1;
    bool bFrozen = false;
    FName Filter = NAME_None;
};
