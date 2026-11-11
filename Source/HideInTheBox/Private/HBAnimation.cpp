#include "HBAnimation.h"
#include "HBEnemyCharacter.h"
#include "HBGameplayAbility.h"
#include "HBPlayerCharacter.h"
#include "AbilitySystemBlueprintLibrary.h"
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
    this->bDodging = false;
    if (Player)
    {
        const FVector Velocity = Player->GetVelocity();
        this->fSpeed = Velocity.Size2D();
        this->fForwardSpeed = FVector::DotProduct(Velocity, Player->GetActorForwardVector());
        this->fRightSpeed = FVector::DotProduct(Velocity, Player->GetActorRightVector());
        this->bFalling = Player->GetCharacterMovement()->IsFalling();
        this->bAttacking = Player->IsAttacking();
        this->bDodging = Player->IsDodging();
    }
}

void UHBAnimNotifyAttackHit::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
    Super::Notify(MeshComp, Animation, EventReference);
    AActor* Owner = MeshComp ? MeshComp->GetOwner() : nullptr;
    if (AHBPlayerCharacter* Player = Cast<AHBPlayerCharacter>(Owner))
    {
        Player->DeliverAnimationHit(Animation);
    }
    else if (AHBEnemyCharacter* Enemy = Cast<AHBEnemyCharacter>(Owner))
    {
        Enemy->DeliverAnimationHit(Animation);
    }
}

void UHBAnimNotifyPulseImpact::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
    Super::Notify(MeshComp, Animation, EventReference);
    AActor* Owner = MeshComp ? MeshComp->GetOwner() : nullptr;
    if (Owner && Owner->HasAuthority())
    {
        FGameplayEventData Event;
        Event.EventTag = HBGetPulseImpactTag();
        Event.Instigator = Owner;
        UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(Owner, Event.EventTag, Event);
    }
}
