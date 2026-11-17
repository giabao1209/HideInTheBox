#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/ActorComponent.h"
#include "HBIndicator.generated.h"
class UInstancedStaticMeshComponent;
class UMaterialInterface;

UCLASS(NotBlueprintable)
class HIDEINTHEBOX_API AHBIndicator : public AActor
{
    GENERATED_BODY()
public:
    AHBIndicator();
    virtual void Tick(float fDeltaSeconds) override;
    void SetRingMaterial(class UMaterialInterface* NewMaterial);
    void PlayBurst(const FVector& Position, float fRadius, const FLinearColor& Color, float fDuration);
    void PlayBolt(const FVector& Origin, const FVector& Impact, UMaterialInterface* BoltMaterial, float Duration);
    void PlaySprite(const FVector& Position, UMaterialInterface* SpriteMaterial, const FLinearColor& Color, float Size, float Duration);
    void ShowAt(const FVector& Position, float fRadius, float fThickness, const FLinearColor& Color);
    void SetHexCells(const TArray<FVector>& Cells, float Radius, float Width, bool bFenceGuide = false);
    void SetColor(const FLinearColor& Color);
protected:
    virtual void BeginPlay() override;
private:
    void AddBoltSegment(const FVector& Start, const FVector& End, float Thickness, float Brightness);
    UPROPERTY(VisibleAnywhere) TObjectPtr<UInstancedStaticMeshComponent> Segments;
    UPROPERTY(Transient) TObjectPtr<class UMaterialInstanceDynamic> Material;
    float fLastThickness = -1.0f;
    float fBurstRadius = 1.0f;
    float fBurstDuration = 0.25f;
    float fBurstElapsed = 0.0f;
    bool bExpandBurst = true;
};

/** Presentation only. Commands are owned by the character. */
UCLASS(ClassGroup=(Feedback), meta=(BlueprintSpawnableComponent))
class HIDEINTHEBOX_API UHBFeedbackComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UHBFeedbackComponent();
    void Present(bool bMoveActive, const FVector& Destination, AActor* Hover, AActor* Target);
    bool HasRingMaterial() const { return this->RingMaterial != nullptr; }
    void HideAll();
    void PlayEffect(const FVector& Position, float fRadius, const FLinearColor& Color);
    void PlayBolt(const FVector& Origin, const FVector& Impact, UMaterialInterface* BoltMaterial, float Duration);
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    UPROPERTY(EditDefaultsOnly, Category="Feedback") TObjectPtr<class UMaterialInterface> RingMaterial;
private:
    UPROPERTY(Transient) TObjectPtr<AHBIndicator> MoveRing;
    UPROPERTY(Transient) TObjectPtr<AHBIndicator> HoverRing;
    UPROPERTY(Transient) TObjectPtr<AHBIndicator> TargetRing;
    TArray<TWeakObjectPtr<AHBIndicator>> Effects;
    AHBIndicator* CreateRing();
    AHBIndicator* CreateEffect();
    void ShowActorRing(AHBIndicator* Ring, AActor* Target, float fThickness, const FLinearColor& Color);
};
