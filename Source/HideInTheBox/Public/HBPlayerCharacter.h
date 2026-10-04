#pragma once
#include "CoreMinimal.h"
#include "TimerManager.h"
#include "HBCombatant.h"
#include "HBTypes.h"
#include "HBPlayerCharacter.generated.h"
class UCameraComponent;
class USpringArmComponent;
class UHBFeedbackComponent;
class UAnimMontage;
class UAnimSequenceBase;
class AHBEnemyCharacter;

UCLASS()
class HIDEINTHEBOX_API AHBPlayerCharacter : public AHBCombatant
{
    GENERATED_BODY()
public:
    AHBPlayerCharacter();
    virtual void Tick(float fDeltaSeconds) override;
    virtual void UnPossessed() override;
    void RequestMove(const FVector& Position);
    void RequestAttack(AHBEnemyCharacter* Target, bool bInPlace);
    void SetDirectInput(const FVector2D& Input);
    void SetAimInput(const FVector2D& Input);
    void SetHover(AHBEnemyCharacter* Target);
    void CancelCommands();
    void CastPulse();
    void SetBuildMode(bool bEnabled);
    void ResetForPhase(const FVector& Position);
    /** Called by the attack notify; guarded against duplicates and late delivery. */
    void DeliverAttackHit();
    void DeliverAnimationHit(UAnimSequenceBase* Animation);
    UFUNCTION(BlueprintPure, Category="Combat") bool IsAttacking() const { return this->bAttacking; }
    UFUNCTION(BlueprintPure, Category="Combat") float GetMana() const { return this->fMana; }
    bool UsesAttackMontage() const { return this->AttackMontage != nullptr; }
    EHBCommand GetCommand() const { return this->Command; }
    bool HasBufferedCommand() const { return this->BufferedCommand != EHBCommand::EHBC_None; }
    bool IsBuilding() const { return this->bBuildMode; }
    int32 GetHitCount() const { return this->iHitCount; }
    AHBEnemyCharacter* GetAttackTarget() const;
protected:
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void OnDeath() override;
    UPROPERTY(VisibleAnywhere, Category="Camera") TObjectPtr<USpringArmComponent> CameraBoom;
    UPROPERTY(VisibleAnywhere, Category="Camera") TObjectPtr<UCameraComponent> Camera;
    UPROPERTY(VisibleAnywhere, Category="Feedback") TObjectPtr<UHBFeedbackComponent> Feedback;
    UPROPERTY(EditDefaultsOnly, Category="Combat") TObjectPtr<UAnimMontage> AttackMontage;
    UPROPERTY(EditDefaultsOnly, Category="Combat", meta=(ClampMin="1")) float fAttackRange = 165.0f;
    UPROPERTY(EditDefaultsOnly, Category="Combat", meta=(ClampMin="1")) float fAttackDamage = 30.0f;
    UPROPERTY(EditDefaultsOnly, Category="Combat", meta=(ClampMin="0.05")) float fFallbackWindup = 0.18f;
    UPROPERTY(EditDefaultsOnly, Category="Combat", meta=(ClampMin="0.3")) float fFallbackDuration = 0.55f;
    UPROPERTY(EditDefaultsOnly, Category="Movement", meta=(ClampMin="1")) float fArrivalDistance = 30.0f;
private:
    void StartAttack();
    void FinishAttack();
    void OnMontageEnded(UAnimMontage* Montage, bool bInterrupted);
    void UpdateCommand(float fDeltaSeconds);
    void UpdateFeedback();
    void FaceDirection(const FVector& Direction);
    void MoveToward(const FVector& Position, float fDeltaSeconds);
    EHBCommand Command = EHBCommand::EHBC_None;
    EHBCommand BufferedCommand = EHBCommand::EHBC_None;
    FVector Destination = FVector::ZeroVector;
    FVector BufferedDestination = FVector::ZeroVector;
    FVector2D DirectInput = FVector2D::ZeroVector;
    FVector2D AimInput = FVector2D::ZeroVector;
    TWeakObjectPtr<AHBEnemyCharacter> AttackTarget;
    TWeakObjectPtr<AHBEnemyCharacter> BufferedTarget;
    TWeakObjectPtr<AHBEnemyCharacter> HoverTarget;
    bool bAttacking = false;
    bool bHitDelivered = false;
    bool bBuildMode = false;
    bool bInPlaceAttack = false;
    bool bBufferedInPlace = false;
    float fMana = 100.0f;
    float fBlockedSeconds = 0.0f;
    float fPulseReadyTime = 0.0f;
    int32 iHitCount = 0;
    FTimerHandle AttackHitTimer;
    FTimerHandle AttackEndTimer;
};
