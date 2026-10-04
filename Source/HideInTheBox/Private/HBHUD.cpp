#include "HBHUD.h"
#include "HBGameMode.h"
#include "HBPlayerCharacter.h"
#include "HBPlayerController.h"
#include "HBEnemyCharacter.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

void AHBHUD::DrawHUD()
{
    Super::DrawHUD();
    AHBGameMode* Mode = this->GetWorld()->GetAuthGameMode<AHBGameMode>();
    AHBPlayerCharacter* Player = Mode ? Mode->GetPlayer() : nullptr;
    if (!Mode || !Player || !this->Canvas)
    {
        return;
    }
    const float fScale = FMath::Clamp(this->Canvas->ClipX / 1600.0f, 0.65f, 1.2f);
    this->DrawRect(FLinearColor(0.02f, 0.03f, 0.05f, 0.88f), 12.0f, 12.0f, FMath::Min(this->Canvas->ClipX - 24.0f, 1250.0f * fScale), 224.0f * fScale);
    const FString Title = FString::Printf(TEXT("VAULTBREAKER / %s / Cycle %d / %s"), *Mode->GetPhaseLabel(), Mode->GetCycle(), Player->UsesAttackMontage() ? TEXT("MONTAGE ASSIGNED") : TEXT("TIMED ATTACK"));
    this->DrawText(Title, FLinearColor(0.3f, 0.9f, 1.0f), 24.0f, 22.0f, GEngine->GetMediumFont(), fScale);
    const FString Stats = FString::Printf(TEXT("HP %.0f | MP %.0f | Stones %d | Threat %d | Gate %.0f | Anima %.0f | Wave %d / 5 | %.0fs | Enemies %d"),
        Player->GetHealth(), Player->GetMana(), Mode->GetStones(), Mode->GetThreat(), Mode->GetGateHealth(), Mode->GetCoreHealth(), Mode->GetWave(), Mode->GetWaveSeconds(), Mode->GetEnemies().Num());
    this->DrawText(Stats, FLinearColor::White, 24.0f, 54.0f * fScale, GEngine->GetSmallFont(), fScale);
    this->DrawText(Mode->GetStatus(), FLinearColor::Yellow, 24.0f, 82.0f * fScale, GEngine->GetSmallFont(), fScale);
    this->DrawText(TEXT("Mouse: LMB move/attack | Shift+LMB in place | RMB Pulse | WASD direct move"), FLinearColor::White, 24.0f, 110.0f * fScale, GEngine->GetSmallFont(), fScale);
    this->DrawText(TEXT("Pad: LS move | RS aim | A attack/confirm | X Pulse | Y next phase | LB build | RB repair"), FLinearColor::White, 24.0f, 134.0f * fScale, GEngine->GetSmallFont(), fScale);
    this->DrawText(TEXT("Enter next | B build | Tab/D-pad Right socket | R repair | Space/B-pad cancel | P/Start pause | F5/Back restart"), FLinearColor::White, 24.0f, 158.0f * fScale, GEngine->GetSmallFont(), fScale);
    const AHBPlayerController* Controller = Cast<AHBPlayerController>(this->GetOwningPlayerController());
    const FString Debug = FString::Printf(TEXT("%s | Command %d | Attack %s | Buffered %s | Hit count %d | Speed %.0f | Socket %d %s"),
        Controller && Controller->IsUsingGamepad() ? TEXT("GAMEPAD") : TEXT("MOUSE/KEYBOARD"), static_cast<int32>(Player->GetCommand()), Player->IsAttacking() ? TEXT("yes") : TEXT("no"),
        Player->HasBufferedCommand() ? TEXT("yes") : TEXT("no"), Player->GetHitCount(), Player->GetVelocity().Size2D(), Mode->GetBuildSocketIndex() + 1, Mode->IsBuildSocketOccupied() ? TEXT("occupied") : TEXT("empty"));
    this->DrawText(Debug, FLinearColor(0.65f, 0.75f, 0.8f), 24.0f, 188.0f * fScale, GEngine->GetSmallFont(), fScale);
    if (Controller && Controller->IsPaused())
    {
        this->DrawText(TEXT("PAUSED - release controls, then P / Start to resume"), FLinearColor::Yellow, 40.0f, 280.0f, GEngine->GetMediumFont());
    }
    if (!Player->IsAlive())
    {
        this->DrawText(TEXT("PLAYER DOWN"), FLinearColor::Red, 40.0f, 320.0f, GEngine->GetMediumFont());
    }
    for (const TWeakObjectPtr<AHBEnemyCharacter>& Entry : Mode->GetEnemies())
    {
        AHBEnemyCharacter* Enemy = Entry.Get();
        if (!IsValid(Enemy))
        {
            continue;
        }
        const FVector Screen = this->Project(Enemy->GetActorLocation() + FVector(0.0f, 0.0f, 110.0f));
        if (Screen.Z > 0.0f)
        {
            this->DrawText(FString::Printf(TEXT("%.0f"), Enemy->GetHealth()), FLinearColor::Red, Screen.X, Screen.Y, GEngine->GetSmallFont());
        }
    }
}
