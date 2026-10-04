#include "HBDebugSubsystem.h"
#include "Engine/World.h"
#include "HideInTheBox.h"

void UHBDebugSubsystem::Record(const UObject* Context, FName Category, const FString& Message)
{
#if !UE_BUILD_SHIPPING
    UWorld* World = Context ? Context->GetWorld() : nullptr;
    UHBDebugSubsystem* Debug = World ? World->GetSubsystem<UHBDebugSubsystem>() : nullptr;
    if (Debug)
    {
        Debug->Add(Category, Message);
    }
#endif
}

void UHBDebugSubsystem::Add(FName Category, const FString& Message)
{
#if !UE_BUILD_SHIPPING
    if (this->bFrozen)
    {
        return;
    }
    if (this->Entries.Num() >= 128)
    {
        this->Entries.RemoveAt(0);
    }
    FHBDebugEntry Entry;
    Entry.Sequence = this->NextSequence++;
    Entry.Time = this->GetWorld()->GetTimeSeconds();
    Entry.Category = Category;
    Entry.Message = Message.Left(240).Replace(TEXT("\n"), TEXT(" ")).Replace(TEXT("\r"), TEXT(" "));
    this->Entries.Add(Entry);
#endif
}

void UHBDebugSubsystem::Clear()
{
    this->Entries.Reset();
    // Keep sequence monotonic so a clear cannot look like an old event.
}

void UHBDebugSubsystem::SetChecks(const TArray<FString>& InChecks)
{
    this->Checks.Reset();
    for (int32 Index = 0; Index < FMath::Min(InChecks.Num(), 32); ++Index)
    {
        this->Checks.Add(InChecks[Index].Left(240));
    }
}

void UHBDebugSubsystem::GetRecent(TArray<FString>& Lines, int32 Count) const
{
    Lines.Reset();
    Count = FMath::Clamp(Count, 0, 128);
    for (int32 Index = this->Entries.Num() - 1; Index >= 0 && Lines.Num() < Count; --Index)
    {
        const FHBDebugEntry& Entry = this->Entries[Index];
        if (this->Filter.IsNone() || Entry.Category == this->Filter)
        {
            Lines.Insert(FString::Printf(TEXT("#%llu %7.2f [%s] %s"), static_cast<unsigned long long>(Entry.Sequence), Entry.Time, *Entry.Category.ToString(), *Entry.Message), 0);
        }
    }
}

void UHBDebugSubsystem::Dump() const
{
#if !UE_BUILD_SHIPPING
    UE_LOG(LogHideInTheBox, Display, TEXT("HB DEBUG SNAPSHOT: map=%s frozen=%d filter=%s"), *this->GetWorld()->GetMapName(), this->bFrozen, *this->Filter.ToString());
    for (const FString& Check : this->Checks)
    {
        UE_LOG(LogHideInTheBox, Display, TEXT("[Check] %s"), *Check);
    }
    TArray<FString> Lines;
    this->GetRecent(Lines, 128);
    for (const FString& Line : Lines)
    {
        UE_LOG(LogHideInTheBox, Display, TEXT("%s"), *Line);
    }
#endif
}
