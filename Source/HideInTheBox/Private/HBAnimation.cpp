#include "HBAnimation.h"
#include "HBPlayerCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

void UHBAnimInstance::NativeUpdateAnimation(float fDeltaSeconds)
{
    Super::NativeUpdateAnimation(fDeltaSeconds);
    const AHBPlayerCharacter* Player = Cast<AHBPlayerCharacter>(this->TryGetPawnOwner());
    this->fSpeed = 0.0f;
    this->fForwardSpeed = 0.0f;
    this->fRightSpeed = 0.0f;
    this->bFalling = false;
    this->bAttacking = false;
    if (Player)
    {
        const FVector Velocity = Player->GetVelocity();
        this->fSpeed = Velocity.Size2D();
        this->fForwardSpeed = FVector::DotProduct(Velocity, Player->GetActorForwardVector());
        this->fRightSpeed = FVector::DotProduct(Velocity, Player->GetActorRightVector());
        this->bFalling = Player->GetCharacterMovement()->IsFalling();
        this->bAttacking = Player->IsAttacking();
    }
}

void UHBAnimNotifyAttackHit::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
    Super::Notify(MeshComp, Animation, EventReference);
    AHBPlayerCharacter* Player = MeshComp ? Cast<AHBPlayerCharacter>(MeshComp->GetOwner()) : nullptr;
    if (Player)
    {
        Player->DeliverAnimationHit(Animation);
    }
}
