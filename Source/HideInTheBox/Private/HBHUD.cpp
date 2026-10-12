#include "HBHUD.h"
#include "HBDebugSubsystem.h"
#include "CoreGlobals.h"
#include "HAL/PlatformTime.h"
#include "HBGameMode.h"
#include "HBPlayerCharacter.h"
#include "HBPlayerController.h"
#include "HBEnemyCharacter.h"
#include "HBLoadout.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

void AHBHUD::Label(const FString& Text, float fX, float fY, const FLinearColor& Color, float fScale)
{
    this->DrawText(Text, Color, fX, fY, GEngine->GetSmallFont(), fScale);
}

void AHBHUD::Panel(float fX, float fY, float fWidth, float fHeight, const FLinearColor& Accent)
{
    this->DrawRect(FLinearColor(0.005f, 0.009f, 0.018f, 0.93f), fX, fY, fWidth, fHeight);
    this->DrawRect(Accent, fX, fY, 3.0f, fHeight);
}

void AHBHUD::DrawHUD()
{
    Super::DrawHUD();
#if !UE_BUILD_SHIPPING
    if (this->GetWorld() && this->GetOwningPlayerController())
    {
        this->GetWorld()->GetSubsystem<UHBDebugSubsystem>()->SampleFrame(GFrameCounter, FPlatformTime::Seconds(), this->GetOwningPlayerController()->IsPaused());
    }
#endif
    AHBGameMode* Mode = this->GetWorld()->GetAuthGameMode<AHBGameMode>();
    AHBPlayerCharacter* Player = Mode ? Mode->GetPlayer() : nullptr;
    AHBPlayerController* Controller = Cast<AHBPlayerController>(this->GetOwningPlayerController());
    if (!Mode || !Player || !Controller || !this->Canvas || !GEngine)
    {
        return;
    }
    const FLinearColor Ice(0.3f, 0.88f, 1.0f);
    const FLinearColor Gold(1.0f, 0.72f, 0.28f);
    const FLinearColor Muted(0.6f, 0.68f, 0.78f);
    const float fWidth = this->Canvas->ClipX;
    const float fScale = FMath::Clamp(fWidth / 1600.0f, 0.7f, 1.2f);
    const float fInfoY = 152.0f * fScale;
    if (Mode->GetPhase() == EHBPhase::EHBP_Dungeon)
    {
        for (int32 iChest = 0; iChest < 3; ++iChest)
        {
            if (Mode->IsGreedClaimed(iChest))
            {
                continue;
            }
            const FVector Screen = this->Project(Mode->GetGreedPosition(iChest) + FVector(0.0f, 0.0f, 60.0f));
            if (Screen.Z > 0.0f) { this->Label(TEXT("CACHE  +30 / +3 THREAT"), Screen.X - 70.0f, Screen.Y, Gold, 0.8f); }
        }
    }
    for (const TWeakObjectPtr<AHBEnemyCharacter>& Entry : Mode->GetEnemies())
    {
        const AHBEnemyCharacter* Enemy = Entry.Get();
        if (!IsValid(Enemy))
        {
            continue;
        }
        const FVector Screen = this->Project(Enemy->GetActorLocation() + FVector(0.0f, 0.0f, 110.0f));
        if (Screen.Z > 0.0f)
        {
            this->DrawRect(FLinearColor(0.03f, 0.01f, 0.01f, 0.9f), Screen.X - 25.0f, Screen.Y, 50.0f, 4.0f);
            this->DrawRect(FLinearColor(0.9f, 0.2f, 0.18f), Screen.X - 25.0f, Screen.Y, 50.0f * FMath::Clamp(Enemy->GetHealth() / Enemy->GetMaxHealth(), 0.0f, 1.0f), 4.0f);
        }
    }
#if !UE_BUILD_SHIPPING
    if (this->bShowDiagnostics)
    {
        const float fY = fInfoY + 62.0f;
        const float fDebugWidth = FMath::Min(fWidth - 48.0f, 950.0f);
        const float fDebugHeight = this->iDebugPage == 0 ? 328.0f : 238.0f;
        this->Panel(24.0f, fY, fDebugWidth, fDebugHeight, Muted);
        UHBDebugSubsystem* Debug = this->GetWorld()->GetSubsystem<UHBDebugSubsystem>();
        const TCHAR* PageNames[] = { TEXT("STATE"), TEXT("EVENTS"), TEXT("CONFIG CHECKS") };
        this->Label(FString::Printf(TEXT("DEBUG %s | F2 / LT: next page then hide | %s | filter=%s"), PageNames[this->iDebugPage], Debug->IsFrozen() ? TEXT("EVENTS FROZEN") : TEXT("EVENTS RECORDING"), Debug->GetFilter().IsNone() ? TEXT("All") : *Debug->GetFilter().ToString()), 36.0f, fY + 8.0f, Ice, 0.8f);
        TArray<FString> Lines;
        if (this->iDebugPage == 0)
        {
            Lines.Add(FString::Printf(TEXT("Map %s | GM %s"), *this->GetWorld()->GetMapName(), *Mode->GetClass()->GetName()));
            Lines.Add(FString::Printf(TEXT("Controller %s | Pawn %s"), *Controller->GetClass()->GetName(), *Player->GetClass()->GetName()));
            const UAnimInstance* Anim = Player->GetMesh()->GetAnimInstance();
            Lines.Add(FString::Printf(TEXT("Anim %s | Playback %s"), Anim ? *Anim->GetClass()->GetName() : TEXT("NONE"), *Player->GetAttackPlayback()));
            Lines.Add(FString::Printf(TEXT("Outcome %s | Hits %d"), *Player->GetLastAttackOutcome(), Player->GetHitCount()));
            Lines.Add(FString::Printf(TEXT("Command %d | Buffer %d | Attack %d | Dodge %d"), static_cast<int32>(Player->GetCommand()), Player->HasBufferedCommand(), Player->IsAttacking(), Player->IsDodging()));
            Lines.Add(FString::Printf(TEXT("Speed %.0f | Aim error %.1f | Target %s"), Player->GetVelocity().Size2D(), Player->GetAimError(), *GetNameSafe(Player->GetAttackTarget())));
            const HBPerformance::FFrameStats& Stats = Debug->GetFrameStats();
            if (Stats.Num() == 0)
            {
                Lines.Add(TEXT("FPS: warming up (needs two consecutive unpaused HUD frames)."));
            }
            else
            {
                Lines.Add(FString::Printf(TEXT("FPS %.1f | Average %.1f | Frame %.2f ms | Average %.2f ms %s"), Stats.CurrentFPS(), Stats.AverageFPS(), Stats.CurrentMilliseconds(), Stats.AverageMilliseconds(), Debug->IsPerformancePaused() ? TEXT("[PAUSED: last values]") : TEXT("")));
                Lines.Add(FString::Printf(TEXT("Worst %.2f ms | %d intervals / %.2fs sampled | HBPerfClear: reset"), Stats.WorstMilliseconds(), static_cast<int32>(Stats.Num()), Stats.WindowSeconds()));
            }
            const float fGraphX = 36.0f;
            const float fGraphY = fY + 202.0f;
            const float fGraphWidth = fDebugWidth - 125.0f;
            const float fGraphHeight = 70.0f;
            const double MaxMs = FMath::Max(50.0, Stats.WorstMilliseconds());
            this->DrawRect(FLinearColor(0.01f, 0.02f, 0.04f), fGraphX, fGraphY, fGraphWidth, fGraphHeight);
            const double Budgets[] = { 1000.0 / 60.0, 1000.0 / 30.0 };
            for (double Budget : Budgets)
            {
                const float fLineY = fGraphY + fGraphHeight * (1.0 - Budget / MaxMs);
                this->DrawLine(fGraphX, fLineY, fGraphX + fGraphWidth, fLineY, Muted, 1.0f);
                this->Label(FString::Printf(TEXT("%.2f ms"), Budget), fGraphX + fGraphWidth + 6.0f, fLineY - 7.0f, Muted, 0.7f);
            }
            for (std::size_t Index = 1; Index < Stats.Num(); ++Index)
            {
                const float fX0 = fGraphX + fGraphWidth * (Index - 1) / (HBPerformance::FFrameStats::Capacity - 1);
                const float fX1 = fGraphX + fGraphWidth * Index / (HBPerformance::FFrameStats::Capacity - 1);
                const double Ms0 = Stats.SecondsAt(Index - 1) * 1000.0;
                const double Ms1 = Stats.SecondsAt(Index) * 1000.0;
                this->DrawLine(fX0, fGraphY + fGraphHeight * (1.0 - Ms0 / MaxMs), fX1, fGraphY + fGraphHeight * (1.0 - Ms1 / MaxMs), Ms1 > 1000.0 / 30.0 ? Gold : Ice, 1.5f);
            }
            this->Label(FString::Printf(TEXT("HUD frame intervals | oldest -> newest | Y: 0..%.1f ms | up to 240 samples"), MaxMs), fGraphX, fGraphY + fGraphHeight + 8.0f, Muted, 0.75f);
        }
        else if (this->iDebugPage == 1)
        {
            Debug->GetRecent(Lines, 9);
            if (Lines.IsEmpty())
            {
                Lines.Add(TEXT("No matching events. Check filter/freeze, then perform an action."));
            }
        }
        else
        {
            Lines = Debug->GetChecks();
        }
        const int32 MaxCharacters = FMath::Max(30, FMath::FloorToInt((fDebugWidth - 24.0f) / 6.8f));
        for (int32 Index = 0; Index < FMath::Min(Lines.Num(), 9); ++Index)
        {
            const FString Display = Lines[Index].Len() > MaxCharacters ? Lines[Index].Left(MaxCharacters - 3) + TEXT("...") : Lines[Index];
            const FLinearColor Color = Lines[Index].StartsWith(TEXT("ERROR")) ? FLinearColor(1.0f, 0.3f, 0.25f) : Muted;
            this->Label(Display, 36.0f, fY + 33.0f + Index * 20.0f, Color, 0.85f);
        }
        this->Label(TEXT("Console: HBDebugCheck / Clear / Freeze / Filter / Dump / Reset (full command names)"), 36.0f, fY + fDebugHeight - 20.0f, Ice, 0.75f);
    }
#endif
}

void AHBHUD::ToggleDiagnostics()
{
#if !UE_BUILD_SHIPPING
    if (!this->bShowDiagnostics)
    {
        this->bShowDiagnostics = true;
        this->iDebugPage = 0;
        return;
    }
    ++this->iDebugPage;
    if (this->iDebugPage >= 3)
    {
        this->bShowDiagnostics = false;
        this->iDebugPage = 0;
    }
#endif
}
