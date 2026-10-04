#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/ActorComponent.h"
#include "HBIndicator.generated.h"
class UInstancedStaticMeshComponent;

UCLASS(NotBlueprintable)
class HIDEINTHEBOX_API AHBIndicator : public AActor
{
    GENERATED_BODY()
public:
    AHBIndicator();
    void ShowAt(const FVector& Position, float fRadius, float fThickness, const FLinearColor& Color);
protected:
    virtual void BeginPlay() override;
private:
    UPROPERTY(VisibleAnywhere) TObjectPtr<UInstancedStaticMeshComponent> Segments;
    UPROPERTY(Transient) TObjectPtr<class UMaterialInstanceDynamic> Material;
    float fLastThickness = -1.0f;
};

/** Presentation only. Commands are owned by the character. */
UCLASS(ClassGroup=(Feedback), meta=(BlueprintSpawnableComponent))
class HIDEINTHEBOX_API UHBFeedbackComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UHBFeedbackComponent();
    void Present(bool bMoveActive, const FVector& Destination, AActor* Hover, AActor* Target);
    void HideAll();
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
private:
    UPROPERTY(Transient) TObjectPtr<AHBIndicator> MoveRing;
    UPROPERTY(Transient) TObjectPtr<AHBIndicator> HoverRing;
    UPROPERTY(Transient) TObjectPtr<AHBIndicator> TargetRing;
    AHBIndicator* CreateRing();
    void ShowActorRing(AHBIndicator* Ring, AActor* Target, float fThickness, const FLinearColor& Color);
};
