#pragma once
#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "HBAnimation.generated.h"

UCLASS()
class HIDEINTHEBOX_API UHBAnimInstance : public UAnimInstance
{
    GENERATED_BODY()
public:
    virtual void NativeUpdateAnimation(float fDeltaSeconds) override;
protected:
    UPROPERTY(BlueprintReadOnly, Category="Locomotion") float fSpeed = 0.0f;
    UPROPERTY(BlueprintReadOnly, Category="Locomotion") float fForwardSpeed = 0.0f;
    UPROPERTY(BlueprintReadOnly, Category="Locomotion") float fRightSpeed = 0.0f;
    UPROPERTY(BlueprintReadOnly, Category="Locomotion") bool bFalling = false;
    UPROPERTY(BlueprintReadOnly, Category="Combat") bool bAttacking = false;
};

UCLASS(meta=(DisplayName="HB Attack Hit"))
class HIDEINTHEBOX_API UHBAnimNotifyAttackHit : public UAnimNotify
{
    GENERATED_BODY()
public:
    virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
};
