#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "HBAttributes.generated.h"

#define HB_ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
    GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
    GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
    GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
    GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

/** GAS owns combatant HP and player MP. Replicated so HUD clients can observe values. */
UCLASS()
class HIDEINTHEBOX_API UHBAttributes : public UAttributeSet
{
    GENERATED_BODY()
public:
    UHBAttributes();

    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_Health, Category="GAS|Attributes")
    FGameplayAttributeData Health;
    HB_ATTRIBUTE_ACCESSORS(UHBAttributes, Health)

    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_MaxHealth, Category="GAS|Attributes")
    FGameplayAttributeData MaxHealth;
    HB_ATTRIBUTE_ACCESSORS(UHBAttributes, MaxHealth)

    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_Mana, Category="GAS|Attributes")
    FGameplayAttributeData Mana;
    HB_ATTRIBUTE_ACCESSORS(UHBAttributes, Mana)

    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_MaxMana, Category="GAS|Attributes")
    FGameplayAttributeData MaxMana;
    HB_ATTRIBUTE_ACCESSORS(UHBAttributes, MaxMana)

    virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
    virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UFUNCTION() void OnRep_Health(const FGameplayAttributeData& OldValue);
    UFUNCTION() void OnRep_MaxHealth(const FGameplayAttributeData& OldValue);
    UFUNCTION() void OnRep_Mana(const FGameplayAttributeData& OldValue);
    UFUNCTION() void OnRep_MaxMana(const FGameplayAttributeData& OldValue);
};
