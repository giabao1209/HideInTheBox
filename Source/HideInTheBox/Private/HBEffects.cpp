#include "HBEffects.h"
#include "HBAttributes.h"
#include "HBPlayerCharacter.h"

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

UHBGE_Guard::UHBGE_Guard()
{
    DurationPolicy = EGameplayEffectDurationType::HasDuration;
    DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(1.0f));
    Modifiers.Add(MakeDeltaModifier(UHBAttributes::GetGuardReductionAttribute()));
}

void UHBExec_Damage::Execute_Implementation(const FGameplayEffectCustomExecutionParameters& Parameters,
    FGameplayEffectCustomExecutionOutput& Output) const
{
    const UAbilitySystemComponent* Target = Parameters.GetTargetAbilitySystemComponent();
    if (!Target)
    {
        return;
    }
    const float Damage = Parameters.GetOwningSpec().GetSetByCallerMagnitude(FName(TEXT("Magnitude")), false);
    const float Reduction = Target->GetNumericAttribute(UHBAttributes::GetGuardReductionAttribute());
    Output.AddOutputModifier(FGameplayModifierEvaluatedData(UHBAttributes::GetHealthAttribute(),
        EGameplayModOp::Additive, -FMath::Max(0.0f, Damage) * (1.0f - FMath::Clamp(Reduction, 0.0f, 0.95f))));
}

UHBGE_Damage::UHBGE_Damage()
{
    DurationPolicy = EGameplayEffectDurationType::Instant;
    FGameplayEffectExecutionDefinition Execution;
    Execution.CalculationClass = UHBExec_Damage::StaticClass();
    Executions.Add(Execution);
}

bool UHBGameplayCue_Pulse::OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const
{
    if (AHBPlayerCharacter* Player = Cast<AHBPlayerCharacter>(MyTarget))
    {
        Player->PresentPulseCue(Parameters.Location, Parameters.RawMagnitude, ShockwaveColor);
        return true;
    }
    return false;
}
