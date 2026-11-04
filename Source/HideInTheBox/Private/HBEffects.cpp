#include "HBEffects.h"
#include "HBAttributes.h"

namespace
{
    FGameplayModifierInfo MakeDeltaModifier(FGameplayAttribute Attribute)
    {
        FGameplayModifierInfo Modifier;
        Modifier.Attribute = Attribute;
        Modifier.ModifierOp = EGameplayModOp::Additive;
        FSetByCallerFloat CallerMagnitude;
        CallerMagnitude.DataName = TEXT("Magnitude");
        Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(CallerMagnitude);
        return Modifier;
    }
}

UHBGE_HealthDelta::UHBGE_HealthDelta()
{
    DurationPolicy = EGameplayEffectDurationType::Instant;
    Modifiers.Add(MakeDeltaModifier(UHBAttributes::GetHealthAttribute()));
}

UHBGE_ManaDelta::UHBGE_ManaDelta()
{
    DurationPolicy = EGameplayEffectDurationType::Instant;
    Modifiers.Add(MakeDeltaModifier(UHBAttributes::GetManaAttribute()));
}

UHBGE_Cooldown::UHBGE_Cooldown()
{
    DurationPolicy = EGameplayEffectDurationType::HasDuration;
    DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(1.0f));
}
