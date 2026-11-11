#include "HBEffects.h"
#include "HBAttributes.h"
#include "HBDebugSubsystem.h"
#include "HBIndicator.h"
#include "HBPlayerCharacter.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"

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
    static const FGameplayTag DamageImmuneTag = FGameplayTag::RequestGameplayTag(TEXT("State.DamageImmune"));
    if (!Target || Target->HasMatchingGameplayTag(DamageImmuneTag) ||
        Target->GetNumericAttribute(UHBAttributes::GetHealthAttribute()) <= 0.0f)
    {
        return;
    }
    const float Damage = Parameters.GetOwningSpec().GetSetByCallerMagnitude(FName(TEXT("Magnitude")), false);
    if (!FMath::IsFinite(Damage) || Damage <= 0.0f)
    {
        return;
    }
    const float Reduction = Target->GetNumericAttribute(UHBAttributes::GetGuardReductionAttribute());
    const FGameplayTag ExposedTag = FGameplayTag::RequestGameplayTag(TEXT("State.Exposed"));
    const float Bonus = Target->HasMatchingGameplayTag(ExposedTag)
        ? FMath::Max(0.0f, Parameters.GetOwningSpec().GetSetByCallerMagnitude(FName(TEXT("ExposedBonus")), false)) : 0.0f;
    Output.AddOutputModifier(FGameplayModifierEvaluatedData(UHBAttributes::GetHealthAttribute(),
        EGameplayModOp::Additive, -(Damage + Bonus) * (1.0f - FMath::Clamp(Reduction, 0.0f, 0.95f))));
}

UHBGE_Damage::UHBGE_Damage()
{
    DurationPolicy = EGameplayEffectDurationType::Instant;
    FGameplayEffectExecutionDefinition Execution;
    Execution.CalculationClass = UHBExec_Damage::StaticClass();
    Executions.Add(Execution);
}

UHBGE_DodgeImmunity::UHBGE_DodgeImmunity()
{
    DurationPolicy = EGameplayEffectDurationType::HasDuration;
    DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(0.15f));
}

UHBGE_Exposed::UHBGE_Exposed()
{
    DurationPolicy = EGameplayEffectDurationType::HasDuration;
    DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(3.0f));
    StackingType = EGameplayEffectStackingType::AggregateByTarget;
    StackLimitCount = 1;
    StackDurationRefreshPolicy = EGameplayEffectStackingDurationPolicy::RefreshOnSuccessfulApplication;
}

UHBGE_ManaRegeneration::UHBGE_ManaRegeneration()
{
    DurationPolicy = EGameplayEffectDurationType::Infinite;
    Period = FScalableFloat(Interval);
    bExecutePeriodicEffectOnApplication = false;
    Modifiers.Add(MakeDeltaModifier(UHBAttributes::GetManaAttribute()));
}

float HBApplyDamageEffect(AActor* Source, AActor* Target, float Damage, TSubclassOf<UGameplayEffect> EffectClass, float ExposedBonus)
{
    if (!IsValid(Target) || !Target->HasAuthority() || !EffectClass || !FMath::IsFinite(Damage) || Damage <= 0.0f)
    {
        return 0.0f;
    }
    UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target);
    if (!TargetASC || !TargetASC->HasAttributeSetForAttribute(UHBAttributes::GetHealthAttribute()))
    {
        return 0.0f;
    }
    UAbilitySystemComponent* SourceASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Source);
    UAbilitySystemComponent* SpecOwner = SourceASC ? SourceASC : TargetASC;
    FGameplayEffectContextHandle Context = SpecOwner->MakeEffectContext();
    if (!SourceASC)
    {
        Context.AddInstigator(Source, Source);
    }
    const FGameplayEffectSpecHandle Spec = SpecOwner->MakeOutgoingSpec(EffectClass, 1.0f, Context);
    if (!Spec.IsValid())
    {
        return 0.0f;
    }
    Spec.Data->SetSetByCallerMagnitude(FName(TEXT("Magnitude")), Damage);
    if (FMath::IsFinite(ExposedBonus) && ExposedBonus > 0.0f)
    {
        Spec.Data->SetSetByCallerMagnitude(FName(TEXT("ExposedBonus")), ExposedBonus);
    }
    const FGameplayAttribute Health = UHBAttributes::GetHealthAttribute();
    const float Previous = TargetASC->GetNumericAttribute(Health);
    SpecOwner->ApplyGameplayEffectSpecToTarget(*Spec.Data, TargetASC);
    const float Current = TargetASC->GetNumericAttribute(Health);
    const float Applied = FMath::Max(0.0f, Previous - Current);
    UHBDebugSubsystem::Record(Target, TEXT("Combat"), FString::Printf(TEXT("GAS %s -> %s damage=%.1f HP=%.1f"),
        *GetNameSafe(Source), *GetNameSafe(Target), Applied, Current));
    return Applied;
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

bool UHBGameplayCue_Bolt::OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const
{
    UHBFeedbackComponent* Feedback = MyTarget ? MyTarget->FindComponentByClass<UHBFeedbackComponent>() : nullptr;
    if (!Feedback || !BoltMaterial)
    {
        return false;
    }
    const FVector Origin = MyTarget->GetActorLocation() + FVector(0.0, 0.0, 28.0);
    Feedback->PlayBolt(Origin, Parameters.Location, BoltMaterial, BoltDuration);
    UHBDebugSubsystem::Record(MyTarget, TEXT("Combat"), TEXT("GameplayCue.GASDemo.Bolt executed"));
    return true;
}
