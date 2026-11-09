#include "HBLoadout.h"
#include "HBGameplayAbility.h"
#include "HBDebugSubsystem.h"
#include "HBPlayerCharacter.h"
#include "Engine/World.h"

UHBLoadoutData::UHBLoadoutData()
{
    const TCHAR* Names[] = { TEXT("Basic"), TEXT("Pulse"), TEXT("Dodge"), TEXT("Guard"), TEXT("Mend"), TEXT("Bolt") };
    const float Costs[] = { 0.0f, 20.0f, 0.0f, 15.0f, 25.0f, 10.0f };
    const float Cooldowns[] = { 0.0f, 1.5f, 2.0f, 5.0f, 6.0f, 0.8f };
    const float Powers[] = { 30.0f, 45.0f, 0.0f, 0.5f, 45.0f, 25.0f };
    const float Ranges[] = { 165.0f, 650.0f, 280.0f, 1.0f, 1.0f, 750.0f };
    const float Durations[] = { 0.55f, 0.25f, 0.25f, 2.0f, 0.25f, 0.25f };
    for (int32 iIndex = 0; iIndex < HBRuleCore::iAbilityCount; ++iIndex)
    {
        FHBAbilitySpec Spec;
        Spec.Name = FText::FromString(Names[iIndex]);
        Spec.AbilityClass = HBGetNativeAbilityClass(static_cast<EHBAbility>(iIndex));
        Spec.fManaCost = Costs[iIndex];
        Spec.fCooldown = Cooldowns[iIndex];
        Spec.fPower = Powers[iIndex];
        Spec.fRange = Ranges[iIndex];
        Spec.fDuration = Durations[iIndex];
        this->Abilities.Add(Spec);
        this->InitialSlots.Add(static_cast<EHBAbility>(iIndex));
    }
}

UHBLoadoutComponent::UHBLoadoutComponent()
{
    this->PrimaryComponentTick.bCanEverTick = false;
}

void UHBLoadoutComponent::BeginPlay()
{
    Super::BeginPlay();
    if (!this->Definition || this->Definition->Abilities.Num() != HBRuleCore::iAbilityCount || this->Definition->InitialSlots.Num() != HBRuleCore::iSlotCount)
    {
        this->bUsedDefaultDefinition = true;
        this->Definition = NewObject<UHBLoadoutData>(this);
    }
    this->Slots = this->Definition->InitialSlots;
    for (int32 iIndex = 0; iIndex < this->Slots.Num(); ++iIndex)
    {
        if (static_cast<int32>(this->Slots[iIndex]) >= HBRuleCore::iAbilityCount)
        {
            this->Slots[iIndex] = EHBAbility::EHBA_Basic;
        }
    }
    this->ResetResources();
}

const FHBAbilitySpec* UHBLoadoutComponent::FindSpec(EHBAbility Ability) const
{
    const int32 iIndex = static_cast<int32>(Ability);
    return this->Definition && this->Definition->Abilities.IsValidIndex(iIndex) ? &this->Definition->Abilities[iIndex] : nullptr;
}

TSubclassOf<UHBGameplayAbility> UHBLoadoutComponent::GetAbilityClass(EHBAbility Ability) const
{
    const FHBAbilitySpec* Spec = FindSpec(Ability);
    if (Spec && Spec->AbilityClass && !Spec->AbilityClass->HasAnyClassFlags(CLASS_Abstract) &&
        Spec->AbilityClass.GetDefaultObject()->GetAbilityKind() == Ability)
    {
        return Spec->AbilityClass;
    }
    return HBGetNativeAbilityClass(Ability);
}

bool UHBLoadoutComponent::AssignAbilityToSlot(int32 iSlot, EHBAbility Ability)
{
    const AHBPlayerCharacter* Player = Cast<AHBPlayerCharacter>(this->GetOwner());
    if (!this->Slots.IsValidIndex(iSlot) || !this->FindSpec(Ability) || !Player || Player->IsAttacking() || Player->IsDodging())
    {
        return false;
    }
    this->Slots[iSlot] = Ability;
    return true;
}

EHBAbility UHBLoadoutComponent::GetSlotAbility(int32 iSlot) const
{
    return this->Slots.IsValidIndex(iSlot) ? this->Slots[iSlot] : EHBAbility::EHBA_Basic;
}

FHBAbilitySpec UHBLoadoutComponent::GetSlotSpec(int32 iSlot) const
{
    const FHBAbilitySpec* Spec = this->Slots.IsValidIndex(iSlot) ? this->FindSpec(this->Slots[iSlot]) : nullptr;
    return Spec ? *Spec : FHBAbilitySpec();
}

float UHBLoadoutComponent::GetSlotCooldown(int32 iSlot) const
{
    const AHBPlayerCharacter* Player = Cast<AHBPlayerCharacter>(GetOwner());
    return Player && Slots.IsValidIndex(iSlot) ? Player->GetGASCooldown(Slots[iSlot]) : 0.0f;
}

EHBActionResult UHBLoadoutComponent::Resolve(int32 iSlot, EHBActionResult Result)
{
    UHBDebugSubsystem::Record(this, TEXT("Combat"), FString::Printf(TEXT("Slot %d result=%s MP=%.1f"), iSlot, *StaticEnum<EHBActionResult>()->GetNameStringByValue(static_cast<int64>(Result)), this->GetMana()));
    this->OnActionResolved.Broadcast(iSlot, Result);
    return Result;
}

EHBActionResult UHBLoadoutComponent::TryActivateSlot(int32 iSlot, AHBEnemyCharacter* Target, const FVector& AimPoint, bool bInPlace)
{
    if (!this->Slots.IsValidIndex(iSlot))
    {
        return this->Resolve(iSlot, EHBActionResult::EHBAR_InvalidSlot);
    }
    AHBPlayerCharacter* Player = Cast<AHBPlayerCharacter>(this->GetOwner());
    const EHBAbility Ability = this->Slots[iSlot];
    const FHBAbilitySpec* Spec = this->FindSpec(Ability);
    if (!Player || !Spec || !Player->IsAlive() || Player->IsBuilding() || Player->IsDodging() || (Player->IsAttacking() && Ability != EHBAbility::EHBA_Basic))
    {
        return this->Resolve(iSlot, EHBActionResult::EHBAR_Blocked);
    }
    // Basic attack always has no mana cost or extra cooldown, as required by the GDD.
    const float fCost = Ability == EHBAbility::EHBA_Basic || Ability == EHBAbility::EHBA_Dodge ? 0.0f : Spec->fManaCost;
    const float fCooldown = Ability == EHBAbility::EHBA_Basic ? 0.0f : Spec->fCooldown;
    if (this->GetSlotCooldown(iSlot) > 0.0f)
    {
        return this->Resolve(iSlot, EHBActionResult::EHBAR_Cooldown);
    }
    if (!FMath::IsFinite(fCost) || !FMath::IsFinite(fCooldown) || fCost < 0.0f || fCooldown < 0.0f || !FMath::IsFinite(Spec->fPower) || !FMath::IsFinite(Spec->fRange) || Spec->fRange <= 0.0f || !FMath::IsFinite(Spec->fRadius) || Spec->fRadius <= 0.0f || !FMath::IsFinite(Spec->fDuration) || Spec->fDuration <= 0.0f)
    {
        return this->Resolve(iSlot, EHBActionResult::EHBAR_Blocked);
    }
    if (GetMana() + KINDA_SMALL_NUMBER < fCost)
    {
        return Resolve(iSlot, EHBActionResult::EHBAR_NoMana);
    }
    // GAS now owns ability activation, attributes, mana cost and shared cooldown.
    if (!Player->TryActivateGASAction(Ability, *Spec, Target, AimPoint, bInPlace))
    {
        return Resolve(iSlot, EHBActionResult::EHBAR_InvalidTarget);
    }
    return Resolve(iSlot, EHBActionResult::EHBAR_Accepted);
}

float UHBLoadoutComponent::GetMana() const
{
    const AHBPlayerCharacter* Player = Cast<AHBPlayerCharacter>(GetOwner());
    return Player ? Player->GetGASMana() : 0.0f;
}

float UHBLoadoutComponent::GetManaFraction() const
{
    const AHBPlayerCharacter* Player = Cast<AHBPlayerCharacter>(GetOwner());
    return Player ? FMath::Clamp(Player->GetGASMana() / FMath::Max(1.0f, Player->GetGASMaxMana()), 0.0f, 1.0f) : 0.0f;
}

void UHBLoadoutComponent::Regenerate(float DeltaSeconds)
{
    AHBPlayerCharacter* Player = Cast<AHBPlayerCharacter>(GetOwner());
    if (!Player || !Definition || !Player->HasAuthority() || !FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0.0f)
    {
        return;
    }
    // Coalesce regeneration into four GAS effects per second instead of one every frame.
    RegenAccumulator += DeltaSeconds;
    if (RegenAccumulator >= 0.25f)
    {
        const float Amount = FMath::Max(0.0f, Definition->fManaRegeneration) * RegenAccumulator;
        RegenAccumulator = 0.0f;
        if (Player->GetGASMana() < Player->GetGASMaxMana())
        {
            Player->ApplyGASManaDelta(Amount);
        }
    }
}

void UHBLoadoutComponent::ResetResources()
{
    RegenAccumulator = 0.0f;
    AHBPlayerCharacter* Player = Cast<AHBPlayerCharacter>(GetOwner());
    if (Player)
    {
        Player->ResetGASMana(Definition ? Definition->fMaxMana : 100.0f);
        Player->ClearGASCooldowns();
    }
}

void UHBLoadoutComponent::AppendDebugChecks(TArray<FString>& Checks) const
{
    Checks.Add(this->bUsedDefaultDefinition ? TEXT("INFO Loadout: native fallback used (missing/invalid Definition arrays).") : TEXT("OK Loadout: configured Data Asset loaded."));
    if (!this->Definition)
    {
        Checks.Add(TEXT("ERROR Loadout: no runtime definition."));
        return;
    }
    bool bValid = FMath::IsFinite(this->Definition->fMaxMana) && this->Definition->fMaxMana > 0.0f && FMath::IsFinite(this->Definition->fManaRegeneration) && this->Definition->fManaRegeneration >= 0.0f;
    for (const FHBAbilitySpec& Spec : this->Definition->Abilities)
    {
        bValid = bValid && FMath::IsFinite(Spec.fManaCost) && Spec.fManaCost >= 0.0f && FMath::IsFinite(Spec.fCooldown) && Spec.fCooldown >= 0.0f && FMath::IsFinite(Spec.fPower) && Spec.fPower >= 0.0f && FMath::IsFinite(Spec.fRange) && Spec.fRange > 0.0f && FMath::IsFinite(Spec.fRadius) && Spec.fRadius > 0.0f && FMath::IsFinite(Spec.fDuration) && Spec.fDuration > 0.0f;
    }
    Checks.Add(bValid ? TEXT("OK Loadout: finite, valid ability/resource values.") : TEXT("ERROR Loadout: invalid number/range; inspect DA_DemoLoadout."));
}
