#pragma once
#include "CoreMinimal.h"
#include "TimerManager.h"
#include "HBCombatant.h"
#include "HBTypes.h"
#include "HBLoadout.h"
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
    void AppendDebugChecks(TArray<FString>& Checks) const;
    /** Synchronous single-player GAS activation; existing input/slot system is preserved. */
    bool TryActivateGASAction(EHBAbility Ability, const FHBAbilitySpec& Spec, AHBEnemyCharacter* Target, const FVector& AimPoint, bool bInPlace);
    bool CanExecutePendingGAS(EHBAbility Ability) const;
    bool ExecutePendingGAS(EHBAbility Ability);
    bool ExecutePendingGuard(TSubclassOf<UGameplayEffect> GuardEffectClass);
    bool ExecutePendingPulse(TSubclassOf<UGameplayEffect> DamageEffectClass);
    float GetPendingGASCost(EHBAbility Ability) const;
    float GetPendingGASCooldown(EHBAbility Ability) const;
    void SetLastGASActivationResult(bool bAccepted) { bLastGASActivationAccepted = bAccepted; }
    float GetGASMana() const;
    float GetGASMaxMana() const;
    void ApplyGASManaDelta(float Delta);
    void ApplyGASCooldown(EHBAbility Ability, float Duration);
    float GetGASCooldown(EHBAbility Ability) const;
    void ResetGASMana(float MaxMana);
    void ClearGASCooldowns();
    void RequestMove(const FVector& Position);
    void RequestAttack(AHBEnemyCharacter* Target, bool bInPlace);
    void SetDirectInput(const FVector2D& Input);
    void SetAimInput(const FVector2D& Input);
    void SetHover(AHBEnemyCharacter* Target);
    void CancelCommands();
    bool ExecuteAbility(EHBAbility Ability, const FHBAbilitySpec& Spec, AHBEnemyCharacter* Target, const FVector& AimPoint, bool bInPlace);
    virtual float TakeDamage(float fDamage, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;
    FVector CameraInputToWorld(const FVector2D& Input) const;
    void SetAimWorldDirection(const FVector& Direction);
    bool IsDodging() const { return this->bDodging; }
    UFUNCTION(BlueprintPure, Category="Loadout") UHBLoadoutComponent* GetLoadout() const { return this->Loadout; }
    FString GetAttackPlayback() const { return this->AttackPlayback; }
    FString GetLastAttackOutcome() const { return this->LastAttackOutcome; }
    float GetAimError() const;
    FVector GetAimDirection() const { return this->bHasAim ? this->AimDirection : this->GetActorForwardVector(); }
    UFUNCTION(BlueprintCallable, Category="Camera") void SetCameraYaw(float fYaw);
    void SetBuildMode(bool bEnabled);
    void ResetForPhase(const FVector& Position);
    /** Called by the attack notify; guarded against duplicates and late delivery. */
    void DeliverAttackHit();
    void DeliverAnimationHit(UAnimSequenceBase* Animation);
    UFUNCTION(BlueprintPure, Category="Combat") bool IsAttacking() const { return this->bAttacking; }
    UFUNCTION(BlueprintPure, Category="Combat") float GetMana() const { return this->Loadout->GetMana(); }
    bool UsesAttackMontage() const { return this->AttackMontage != nullptr; }
    UAnimMontage* GetAttackMontage() const { return AttackMontage; }
    void OnAttackMontageFinished(bool bInterrupted);
    void PresentPulseCue(FVector Location, float Radius, FLinearColor Color);
    EHBCommand GetCommand() const { return this->Command; }
    bool HasBufferedCommand() const { return this->BufferedCommand != EHBCommand::EHBC_None; }
    bool IsBuilding() const { return this->bBuildMode; }
    int32 GetHitCount() const { return this->iHitCount; }
    AHBEnemyCharacter* GetAttackTarget() const;
protected:
    UPROPERTY(EditDefaultsOnly, Category="GAS") TSubclassOf<UGameplayEffect> ManaEffectClass;
    UPROPERTY(EditDefaultsOnly, Category="GAS") TSubclassOf<UGameplayEffect> CooldownEffectClass;
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void OnDeath() override;
    UPROPERTY(VisibleAnywhere, Category="Camera") TObjectPtr<USpringArmComponent> CameraBoom;
    UPROPERTY(VisibleAnywhere, Category="Camera") TObjectPtr<UCameraComponent> Camera;
    UPROPERTY(VisibleAnywhere, Category="Feedback") TObjectPtr<UHBFeedbackComponent> Feedback;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Loadout") TObjectPtr<UHBLoadoutComponent> Loadout;
    UPROPERTY(EditDefaultsOnly, Category="Movement", meta=(ClampMin="1")) float fTurnSpeed = 720.0f;
    UPROPERTY(EditDefaultsOnly, Category="Combat") TObjectPtr<UAnimMontage> AttackMontage;
    UPROPERTY(EditDefaultsOnly, Category="Combat", meta=(ClampMin="1")) float fAttackRange = 165.0f;
    UPROPERTY(EditDefaultsOnly, Category="Combat", meta=(ClampMin="1")) float fAttackDamage = 30.0f;
    UPROPERTY(EditDefaultsOnly, Category="Combat", meta=(ClampMin="0.05")) float fFallbackWindup = 0.18f;
    UPROPERTY(EditDefaultsOnly, Category="Combat", meta=(ClampMin="0.3")) float fFallbackDuration = 0.55f;
    UPROPERTY(EditDefaultsOnly, Category="Movement", meta=(ClampMin="1")) float fArrivalDistance = 30.0f;
private:
    bool bGASContextReady = false;
    bool bLastGASActivationAccepted = false;
    EHBAbility PendingGASAbility = EHBAbility::EHBA_Basic;
    FHBAbilitySpec PendingGASSpec;
    TWeakObjectPtr<AHBEnemyCharacter> PendingGASTarget;
    FVector PendingGASAim = FVector::ZeroVector;
    bool bPendingGASInPlace = false;
    void StartAttack();
    void FinishAttack();
    void OnAttackTimeout();
    void EndDodge();
    bool HasHitLine(const FVector& Point, const AActor* Target) const;
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
    bool bDodging = false;
    bool bHasAim = false;
    FVector AimDirection = FVector::ForwardVector;
    FVector DodgeDirection = FVector::ForwardVector;
    float fDodgeEndTime = 0.0f;
    float fInvulnerableUntil = 0.0f;
    float fSavedWalkSpeed = 450.0f;
    FString AttackPlayback = TEXT("Idle");
    FString LastAttackOutcome = TEXT("None");
    float fBlockedSeconds = 0.0f;
    int32 iHitCount = 0;
    FTimerHandle AttackHitTimer;
    FTimerHandle AttackEndTimer;
};
