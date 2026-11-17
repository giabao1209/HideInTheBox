#pragma once
#include "CoreMinimal.h"
#include "TimerManager.h"
#include "GameFramework/Actor.h"
#include "AbilitySystemInterface.h"
#include "GameplayAbilitySpecHandle.h"
#include "HBTypes.h"
#include "HBStructure.generated.h"
class UStaticMeshComponent;
class UBoxComponent;
class AHBGameMode;
class AHBEnemyCharacter;
class UHBAttributes;
class UHBGA_TurretFire;
struct FOnAttributeChangeData;

UCLASS()
class HIDEINTHEBOX_API AHBStructure : public AActor, public IAbilitySystemInterface
{
    GENERATED_BODY()
public:
    AHBStructure();
    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
    UFUNCTION(BlueprintCallable, Category="Structure") void Configure(EHBStructure NewKind);
    void SetTintMaterial(class UMaterialInterface* NewMaterial);
    virtual float TakeDamage(float fDamage, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;
    UFUNCTION(BlueprintPure, Category="Structure") bool IsAlive() const { return GetHealth() > 0.0f; }
    UFUNCTION(BlueprintPure, Category="Structure") float GetHealth() const;
    UFUNCTION(BlueprintPure, Category="Structure") bool NeedsRepair() const;
    UFUNCTION(BlueprintCallable, Category="Structure") void Repair();
    bool ApplyRepair(AActor* Source);
    bool ApplyUpgrade(AActor* Source);
    bool IsUpgraded() const;
    bool IsFence() const { return Kind == EHBStructure::EHBS_Fence; }
    bool IsTurret() const { return Kind == EHBStructure::EHBS_Turret; }
    bool CanFire() const;
    float GetFireRange() const;
    float GetMaxHealth() const;
    void PlayServiceCue();
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
private:
    void Fire();
    void OnHealthChanged(const FOnAttributeChangeData& Data);
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Appearance", meta=(AllowPrivateAccess="true")) TObjectPtr<UStaticMeshComponent> Mesh;
    UPROPERTY(VisibleAnywhere, Category="Collision") TObjectPtr<UBoxComponent> FenceBlocker;
    UPROPERTY(VisibleAnywhere, Category="Appearance") TObjectPtr<UStaticMeshComponent> BuildModel;
    UPROPERTY(VisibleAnywhere, Category="Appearance") TObjectPtr<UStaticMeshComponent> TurretHead;
    UPROPERTY(VisibleAnywhere, Category="Appearance") TObjectPtr<UStaticMeshComponent> TurretBarrel;
    UPROPERTY(EditDefaultsOnly, Category="Appearance") TObjectPtr<UStaticMesh> TurretModel;
    UPROPERTY(EditDefaultsOnly, Category="Appearance") TObjectPtr<UStaticMesh> FenceModel;
    UPROPERTY(VisibleAnywhere, Category="GAS") TObjectPtr<UAbilitySystemComponent> AbilitySystem;
    UPROPERTY() TObjectPtr<UHBAttributes> Attributes;
    UPROPERTY(EditDefaultsOnly, Category="GAS") TSubclassOf<UHBGA_TurretFire> TurretAbilityClass;
    EHBStructure Kind = EHBStructure::EHBS_Gate;
    TWeakObjectPtr<AHBGameMode> Mode;
    TWeakObjectPtr<AHBEnemyCharacter> Target;
    FTimerHandle FireTimer;
    FGameplayAbilitySpecHandle FireAbility;
};
