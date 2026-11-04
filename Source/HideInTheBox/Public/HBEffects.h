#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "HBEffects.generated.h"

/** Instant SetByCaller "Magnitude" additive health change. Damage is negative; Mend is positive. */
UCLASS()
class HIDEINTHEBOX_API UHBGE_HealthDelta : public UGameplayEffect
{
    GENERATED_BODY()
public:
    UHBGE_HealthDelta();
};

/** Instant SetByCaller "Magnitude" additive mana change (cost or regeneration). */
UCLASS()
class HIDEINTHEBOX_API UHBGE_ManaDelta : public UGameplayEffect
{
    GENERATED_BODY()
public:
    UHBGE_ManaDelta();
};

/** Timed effect. GAS ability adds the relevant Cooldown.* tag to each outgoing spec. */
UCLASS()
class HIDEINTHEBOX_API UHBGE_Cooldown : public UGameplayEffect
{
    GENERATED_BODY()
public:
    UHBGE_Cooldown();
};
