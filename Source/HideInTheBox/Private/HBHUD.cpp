#include "HBHUD.h"
#include "HBDebugSubsystem.h"
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
    const float fHeight = this->Canvas->ClipY;
    const float fScale = FMath::Clamp(fWidth / 1600.0f, 0.7f, 1.2f);
    this->Panel(24.0f, 24.0f, 300.0f * fScale, 105.0f * fScale, Ice);
    this->Label(TEXT("HIDE IN THE BOX"), 40.0f, 36.0f, Ice, 1.1f * fScale);
    this->Label(FString::Printf(TEXT("%s   /   CYCLE %02d"), *Mode->GetPhaseLabel(), Mode->GetCycle()), 40.0f, 64.0f * fScale, FLinearColor::White, fScale);
    this->Label(FString::Printf(TEXT("WAVE %d / 5    %.0fs    %d HOSTILES"), Mode->GetWave(), Mode->GetWaveSeconds(), Mode->GetEnemies().Num()), 40.0f, 93.0f * fScale, Muted, fScale);
    this->Panel(fWidth - 280.0f * fScale - 24.0f, 24.0f, 280.0f * fScale, 105.0f * fScale, Gold);
    const float fRightX = fWidth - 280.0f * fScale - 8.0f;
    this->Label(FString::Printf(TEXT("%d   MAGIC STONES"), Mode->GetStones()), fRightX, 38.0f, Gold, fScale);
    this->Label(FString::Printf(TEXT("THREAT %d   /   NEXT WAVE %d"), Mode->GetThreat(), HBRules::GetWaveEnemyCount(FMath::Max(1, Mode->GetWave()), Mode->GetThreat())), fRightX, 68.0f * fScale, Muted, fScale);
    this->Label(FString::Printf(TEXT("GATE %.0f    ANIMA %.0f"), Mode->GetGateHealth(), Mode->GetCoreHealth()), fRightX, 94.0f * fScale, Ice, fScale);

    const float fSlotWidth = 110.0f * fScale;
    const float fBarWidth = 6.0f * fSlotWidth + 5.0f * 8.0f;
    const float fLeft = (fWidth - fBarWidth) * 0.5f;
    const float fTop = fHeight - 138.0f * fScale;
    this->DrawRect(FLinearColor(0.1f, 0.025f, 0.035f), fLeft, fTop - 28.0f, fBarWidth * 0.5f - 8.0f, 5.0f);
    this->DrawRect(FLinearColor(0.9f, 0.2f, 0.3f), fLeft, fTop - 28.0f, (fBarWidth * 0.5f - 8.0f) * FMath::Clamp(Player->GetHealth() / Player->GetMaxHealth(), 0.0f, 1.0f), 5.0f);
    this->DrawRect(FLinearColor(0.03f, 0.05f, 0.14f), fLeft + fBarWidth * 0.5f, fTop - 28.0f, fBarWidth * 0.5f, 5.0f);
    this->DrawRect(Ice, fLeft + fBarWidth * 0.5f, fTop - 28.0f, fBarWidth * 0.5f * Player->GetLoadout()->GetManaFraction(), 5.0f);
    this->Label(FString::Printf(TEXT("HP %.0f / %.0f"), Player->GetHealth(), Player->GetMaxHealth()), fLeft, fTop - 53.0f, FLinearColor::White, fScale);
    this->Label(FString::Printf(TEXT("MP %.0f"), Player->GetMana()), fLeft + fBarWidth * 0.5f, fTop - 53.0f, Ice, fScale);
    const FLinearColor Colors[] = { Gold, FLinearColor(1.0f, 0.4f, 0.15f), Ice, FLinearColor(0.75f, 0.4f, 1.0f), FLinearColor(0.25f, 1.0f, 0.55f), FLinearColor(0.5f, 0.7f, 1.0f) };
    for (int32 iSlot = 0; iSlot < 6; ++iSlot)
    {
        const float fX = fLeft + iSlot * (fSlotWidth + 8.0f);
        const FHBAbilitySpec Spec = Player->GetLoadout()->GetSlotSpec(iSlot);
        const float fCooldown = Player->GetLoadout()->GetSlotCooldown(iSlot);
        this->Panel(fX, fTop, fSlotWidth, 78.0f * fScale, Colors[static_cast<int32>(Player->GetLoadout()->GetSlotAbility(iSlot))]);
        const FString KeyName = Controller->GetSlotKey(iSlot, Controller->IsUsingGamepad()).GetDisplayName().ToString();
        this->Label(KeyName.Left(14), fX + 10.0f, fTop + 8.0f, Muted, 0.8f * fScale);
        this->Label(Spec.Name.ToString(), fX + 10.0f, fTop + 31.0f * fScale, FLinearColor::White, fScale);
        this->Label(fCooldown > 0.0f ? FString::Printf(TEXT("%.1fs"), fCooldown) : TEXT("READY"), fX + 10.0f, fTop + 55.0f * fScale, fCooldown > 0.0f ? Muted : Ice, 0.8f * fScale);
    }
    this->Label(Controller->GetLastActionResult(), fLeft, fTop - 82.0f, Muted, 0.85f * fScale);
    const float fInfoY = 148.0f * fScale;
    this->Label(Mode->GetStatus(), 28.0f, fInfoY, Gold, 0.9f * fScale);
    const FString Controls = Controller->IsUsingGamepad() ? TEXT("D-pad Up: next phase   Down: build   Right: socket   Left: repair   L3: cache   R3: cancel   Start: pause") : TEXT("Enter: next phase   B: build   Tab: socket   R: repair   E: cache   Space: cancel   P: pause   F2: diagnostics");
    this->Label(Controls, 28.0f, fHeight - 25.0f, Muted, 0.85f * fScale);
    if (Player->IsBuilding())
    {
        this->Label(FString::Printf(TEXT("BUILD MODE  /  SOCKET %d  /  %s  /  30 STONES"), Mode->GetBuildSocketIndex() + 1, Mode->IsBuildSocketOccupied() ? TEXT("OCCUPIED") : TEXT("EMPTY")), 28.0f, fInfoY + 30.0f, Ice, fScale);
    }
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
        this->Panel(24.0f, fY, fDebugWidth, 238.0f, Muted);
        UHBDebugSubsystem* Debug = this->GetWorld()->GetSubsystem<UHBDebugSubsystem>();
        const TCHAR* PageNames[] = { TEXT("STATE"), TEXT("EVENTS"), TEXT("CONFIG CHECKS") };
        this->Label(FString::Printf(TEXT("DEBUG %s | F2 / LT: next page then hide | %s | filter=%s"), PageNames[this->iDebugPage], Debug->IsFrozen() ? TEXT("FROZEN") : TEXT("RECORDING"), Debug->GetFilter().IsNone() ? TEXT("All") : *Debug->GetFilter().ToString()), 36.0f, fY + 8.0f, Ice, 0.8f);
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
            Lines.Add(TEXT("Command enum: 0=None, 1=Move, 2=Attack. Buffer: 1=pending."));
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
        this->Label(TEXT("Console: HBDebugCheck / Clear / Freeze / Filter / Dump / Reset (full command names)"), 36.0f, fY + 218.0f, Ice, 0.75f);
    }
#endif
    if (Controller->IsPaused() || Mode->GetPhase() == EHBPhase::EHBP_GameOver)
    {
        this->DrawRect(FLinearColor(0.0f, 0.0f, 0.015f, 0.7f), 0.0f, fHeight * 0.4f, fWidth, 110.0f);
        this->Label(Controller->IsPaused() ? TEXT("PAUSED   /   P or Start to resume") : TEXT("RUN ENDED   /   F5 or Back to restart"), fWidth * 0.5f - 180.0f, fHeight * 0.4f + 35.0f, Gold, 1.4f);
    }
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
