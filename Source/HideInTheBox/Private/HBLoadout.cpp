#include "HBLoadout.h"
#include "HBEffects.h"
#include "HBGameplayAbility.h"
#include "HBDebugSubsystem.h"
#include "HBPlayerCharacter.h"
#include "HBGameMode.h"
#include "AbilitySystemComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"

namespace
{
    struct FEquipment
    {
        const TCHAR* Name;
        const TCHAR* Perk;
        const TCHAR* Mesh;
        const TCHAR* Socket;
        FTransform Fit;
    };

    const FEquipment Equipment[] = {
        { TEXT("Steel Sword"), TEXT("Balanced damage and reach"), TEXT("/Game/DemoArena/Art/Meshes/SM_VaultKnightSword/StaticMeshes/SM_VaultKnightSword"),
          TEXT("handslot_r"), FTransform(FRotator(0,180,0), FVector(0,0,3.33), FVector(1.05)) },
        { TEXT("Raider Axe"), TEXT("Basic +10 damage, -15 cm reach"), TEXT("/Game/DemoArena/Art/Equipment/axe_1handed/StaticMeshes/axe_1handed"),
          TEXT("handslot_r"), FTransform(FRotator(0,180,0), FVector(0,0,3.33), FVector(1.14)) },
        { TEXT("Knight Shield"), TEXT("Standard Guard duration"), TEXT("/Game/DemoArena/Art/Meshes/SM_VaultKnightShield/StaticMeshes/SM_VaultKnightShield"),
          TEXT("handslot_l"), FTransform(FRotator::ZeroRotator, FVector(0,15.59,1.70), FVector(.83)) },
        { TEXT("Spiked Bulwark"), TEXT("Guard lasts +0.75 s"), TEXT("/Game/DemoArena/Art/Equipment/shield_spikes/StaticMeshes/shield_spikes"),
          TEXT("handslot_l"), FTransform(FRotator::ZeroRotator, FVector(0,15.59,1.70), FVector(.96)) },
    };
    static_assert(UE_ARRAY_COUNT(Equipment) == UHBLoadoutComponent::ItemCount);
}

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

int32 UHBLoadoutComponent::GetSkillLevel(EHBAbility Ability) const
{
    const AHBPlayerCharacter* Player = Cast<AHBPlayerCharacter>(GetOwner());
    if (Player && FindSpec(Ability))
    {
        const FGameplayAbilitySpec* Granted = Player->GetAbilitySystemComponent()->FindAbilitySpecFromClass(GetAbilityClass(Ability));
        if (Granted) { return Granted->Level; }
    }
    return 1;
}

bool UHBLoadoutComponent::UpgradeSkill(EHBAbility Ability)
{
    AHBPlayerCharacter* Player = Cast<AHBPlayerCharacter>(GetOwner());
    AHBGameMode* Mode = GetWorld() ? GetWorld()->GetAuthGameMode<AHBGameMode>() : nullptr;
    if (!Player || !Player->HasAuthority() || !Mode || !Mode->CanEditLoadout() || !FindSpec(Ability)) { return false; }
    UAbilitySystemComponent* ASC = Player->GetAbilitySystemComponent();
    FGameplayAbilitySpec* Granted = ASC->FindAbilitySpecFromClass(GetAbilityClass(Ability));
    if (!Granted || Granted->IsActive() || Granted->Level >= MaxSkillLevel || !Mode->SpendSkillStones(SkillUpgradeCost(Granted->Level)))
    {
        return false;
    }
    ++Granted->Level;
    ASC->MarkAbilitySpecDirty(*Granted);
    return true;
}

bool UHBLoadoutComponent::OwnsItem(int32 Item) const
{
    return Item >= 0 && Item < ItemCount && (OwnedEquipment & (1 << Item)) != 0;
}

bool UHBLoadoutComponent::IsItemEquipped(int32 Item) const
{
    return Item == EquippedWeapon || Item == EquippedShield;
}

bool UHBLoadoutComponent::UnlockEquipment(int32 Item)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || Item < 0 || Item >= ItemCount || OwnsItem(Item)) { return false; }
    OwnedEquipment |= 1 << Item;
    return true;
}

FString UHBLoadoutComponent::GetItemName(int32 Item)
{
    return Item >= 0 && Item < ItemCount ? FString(Equipment[Item].Name) : FString();
}

FString UHBLoadoutComponent::GetItemPerk(int32 Item)
{
    return Item >= 0 && Item < ItemCount ? FString(Equipment[Item].Perk) : FString();
}

bool UHBLoadoutComponent::EquipItem(int32 Item)
{
    AHBPlayerCharacter* Player = Cast<AHBPlayerCharacter>(GetOwner());
    const AHBGameMode* Mode = GetWorld() ? GetWorld()->GetAuthGameMode<AHBGameMode>() : nullptr;
    if (!Player || !Player->HasAuthority() || !Mode || !Mode->CanEditLoadout() || !OwnsItem(Item) || IsItemEquipped(Item)) { return false; }
    const FEquipment& Entry = Equipment[Item];
    UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, Entry.Mesh);
    if (!Player->SetArenaEquipmentMesh(Entry.Socket, Mesh, Entry.Fit)) { return false; }
    if (Item < 2) { EquippedWeapon = Item; }
    else { EquippedShield = Item; }
    return true;
}

FHBAbilitySpec UHBLoadoutComponent::GetEffectiveSpec(EHBAbility Ability) const
{
    const FHBAbilitySpec* Base = FindSpec(Ability);
    if (!Base) { return FHBAbilitySpec(); }
    FHBAbilitySpec Spec = *Base;
    const int32 BonusLevel = FMath::Max(0, GetSkillLevel(Ability) - 1);
    switch (Ability)
    {
        case EHBAbility::EHBA_Basic:
            Spec.fPower += 8.0f * BonusLevel + (EquippedWeapon == 1 ? 10.0f : 0.0f);
            if (EquippedWeapon == 1) { Spec.fRange = FMath::Max(1.0f, Spec.fRange - 15.0f); }
            break;
        case EHBAbility::EHBA_Pulse:
            Spec.fPower += 12.0f * BonusLevel;
            Spec.fRadius += 28.0f * BonusLevel;
            break;
        case EHBAbility::EHBA_Dodge:
            Spec.fRange += 55.0f * BonusLevel;
            Spec.fDuration += 0.04f * BonusLevel;
            break;
        case EHBAbility::EHBA_Guard:
            Spec.fPower = FMath::Min(0.95f, Spec.fPower + 0.08f * BonusLevel);
            Spec.fDuration += EquippedShield == 3 ? 0.75f : 0.0f;
            break;
        case EHBAbility::EHBA_Mend: Spec.fPower += 12.0f * BonusLevel; break;
        case EHBAbility::EHBA_Bolt: Spec.fPower += 10.0f * BonusLevel; break;
        default: break;
    }
    return Spec;
}

EHBAbility UHBLoadoutComponent::GetSlotAbility(int32 iSlot) const
{
    return this->Slots.IsValidIndex(iSlot) ? this->Slots[iSlot] : EHBAbility::EHBA_Basic;
}

FHBAbilitySpec UHBLoadoutComponent::GetSlotSpec(int32 iSlot) const
{
    return Slots.IsValidIndex(iSlot) ? GetEffectiveSpec(Slots[iSlot]) : FHBAbilitySpec();
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
    if (!Player || !FindSpec(Ability) || !Player->IsAlive() || Player->IsBuilding() || Player->IsDodging() || (Player->IsAttacking() && Ability != EHBAbility::EHBA_Basic))
    {
        return this->Resolve(iSlot, EHBActionResult::EHBAR_Blocked);
    }
    const FHBAbilitySpec Effective = GetEffectiveSpec(Ability);
    const FHBAbilitySpec* Spec = &Effective;
    // Basic attack always has no mana cost or extra cooldown, as required by the GDD.
    const float fCost = Spec->GetManaCost(Ability);
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

void UHBLoadoutComponent::RefreshManaRegeneration()
{
    const AHBPlayerCharacter* Player = Cast<AHBPlayerCharacter>(GetOwner());
    if (!Player || !Player->HasAuthority())
    {
        return;
    }
    UAbilitySystemComponent* ASC = Player->GetAbilitySystemComponent();
    const float Rate = Definition ? Definition->fManaRegeneration : 0.0f;
    const bool bShouldRegenerate = ASC->GetAvatarActor() == Player && Player->IsLocallyControlled() &&
        Player->IsAlive() && !Player->IsBuilding() && FMath::IsFinite(Rate) && Rate > 0.0f;
    if (!bShouldRegenerate)
    {
        if (ManaRegenerationHandle.IsValid())
        {
            ASC->RemoveActiveGameplayEffect(ManaRegenerationHandle);
            ManaRegenerationHandle.Invalidate();
        }
        return;
    }
    if (ASC->GetActiveGameplayEffect(ManaRegenerationHandle))
    {
        return;
    }
    const FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(UHBGE_ManaRegeneration::StaticClass(),
        1.0f, ASC->MakeEffectContext());
    if (Spec.IsValid())
    {
        Spec.Data->SetSetByCallerMagnitude(FName(TEXT("Magnitude")), Rate * UHBGE_ManaRegeneration::Interval);
        Spec.Data->DynamicGrantedTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("State.ManaRegenerating")));
        ManaRegenerationHandle = ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data);
    }
}

void UHBLoadoutComponent::ResetResources()
{
    AHBPlayerCharacter* Player = Cast<AHBPlayerCharacter>(GetOwner());
    if (Player && Player->HasAuthority())
    {
        if (ManaRegenerationHandle.IsValid())
        {
            Player->GetAbilitySystemComponent()->RemoveActiveGameplayEffect(ManaRegenerationHandle);
            ManaRegenerationHandle.Invalidate();
        }
        Player->ResetGASMana(Definition ? Definition->fMaxMana : 100.0f);
        Player->ClearGASCooldowns();
        RefreshManaRegeneration();
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
