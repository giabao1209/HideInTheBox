#include "HBIndicator.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"

AHBIndicator::AHBIndicator()
{
    this->PrimaryActorTick.bCanEverTick = true;
    this->PrimaryActorTick.bStartWithTickEnabled = false;
    this->Segments = this->CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("RingSegments"));
    this->SetRootComponent(this->Segments);
    this->Segments->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    this->Segments->SetCanEverAffectNavigation(false);
    this->Segments->SetCastShadow(false);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> MeshAsset(TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (MeshAsset.Succeeded())
    {
        this->Segments->SetStaticMesh(MeshAsset.Object);
    }
    constexpr int32 iSegmentCount = 32;
    for (int32 iIndex = 0; iIndex < iSegmentCount; ++iIndex)
    {
        const float fAngle = 2.0f * PI * iIndex / iSegmentCount;
        const FVector Location(FMath::Cos(fAngle), FMath::Sin(fAngle), 0.0f);
        this->Segments->AddInstance(FTransform(FRotator(0.0f, FMath::RadiansToDegrees(fAngle) + 90.0f, 0.0f), Location, FVector(0.0021f, 0.001f, 0.02f)));
    }
    this->SetActorHiddenInGame(true);
}

void AHBIndicator::BeginPlay()
{
    Super::BeginPlay();
    this->Material = this->Segments->CreateAndSetMaterialInstanceDynamic(0);
}

void AHBIndicator::ShowAt(const FVector& Position, float fRadius, float fThickness, const FLinearColor& Color)
{
    this->SetActorLocation(Position + FVector(0.0f, 0.0f, 3.0f));
    this->SetActorScale3D(FVector(fRadius, fRadius, 1.0f));
    if (!FMath::IsNearlyEqual(this->fLastThickness, fThickness))
    {
        this->fLastThickness = fThickness;
        const int32 iCount = this->Segments->GetInstanceCount();
        for (int32 iIndex = 0; iIndex < iCount; ++iIndex)
        {
            FTransform Transform;
            this->Segments->GetInstanceTransform(iIndex, Transform, false);
            Transform.SetScale3D(FVector(0.0021f, 0.001f * fThickness, 0.02f));
            this->Segments->UpdateInstanceTransform(iIndex, Transform, false, iIndex == iCount - 1, true);
        }
    }
    if (this->Material)
    {
        this->Material->SetVectorParameterValue(TEXT("Color"), Color);
    }
    this->SetActorHiddenInGame(false);
}

UHBFeedbackComponent::UHBFeedbackComponent()
{
    this->PrimaryComponentTick.bCanEverTick = false;
}

AHBIndicator* UHBFeedbackComponent::CreateRing()
{
    FActorSpawnParameters Parameters;
    Parameters.Owner = this->GetOwner();
    Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AHBIndicator* Ring = this->GetWorld()->SpawnActor<AHBIndicator>(AHBIndicator::StaticClass(), FTransform::Identity, Parameters);
    if (Ring)
    {
        Ring->SetRingMaterial(this->RingMaterial);
    }
    return Ring;
}

void UHBFeedbackComponent::BeginPlay()
{
    Super::BeginPlay();
    this->MoveRing = this->CreateRing();
    this->HoverRing = this->CreateRing();
    this->TargetRing = this->CreateRing();
}

void UHBFeedbackComponent::ShowActorRing(AHBIndicator* Ring, AActor* Target, float fThickness, const FLinearColor& Color)
{
    if (!IsValid(Ring))
    {
        return;
    }
    if (!IsValid(Target))
    {
        Ring->SetActorHiddenInGame(true);
        return;
    }
    FVector Position = Target->GetActorLocation();
    const ACharacter* Character = Cast<ACharacter>(Target);
    Position.Z -= Character ? Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 0.0f;
    Ring->ShowAt(Position, 58.0f, fThickness, Color);
}

void UHBFeedbackComponent::Present(bool bMoveActive, const FVector& Destination, AActor* Hover, AActor* Target)
{
    if (IsValid(this->MoveRing))
    {
        if (bMoveActive)
        {
            this->MoveRing->ShowAt(Destination, 40.0f, 1.0f, FLinearColor(0.1f, 0.9f, 1.0f));
        }
        else
        {
            this->MoveRing->SetActorHiddenInGame(true);
        }
    }
    this->ShowActorRing(this->HoverRing, Hover != Target ? Hover : nullptr, 1.0f, FLinearColor::Yellow);
    this->ShowActorRing(this->TargetRing, Target, 3.0f, FLinearColor::Red);
}

void UHBFeedbackComponent::HideAll()
{
    this->Present(false, FVector::ZeroVector, nullptr, nullptr);
}

void UHBFeedbackComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    for (const TWeakObjectPtr<AHBIndicator>& Effect : this->Effects)
    {
        if (Effect.IsValid())
        {
            Effect->Destroy();
        }
    }
    this->Effects.Reset();
    if (IsValid(this->MoveRing))
    {
        this->MoveRing->Destroy();
    }
    if (IsValid(this->HoverRing))
    {
        this->HoverRing->Destroy();
    }
    if (IsValid(this->TargetRing))
    {
        this->TargetRing->Destroy();
    }
    Super::EndPlay(EndPlayReason);
}

void AHBIndicator::SetRingMaterial(UMaterialInterface* NewMaterial)
{
    if (NewMaterial)
    {
        this->Segments->SetMaterial(0, NewMaterial);
        this->Material = this->Segments->CreateAndSetMaterialInstanceDynamic(0);
    }
}

void AHBIndicator::PlayBurst(const FVector& Position, float fRadius, const FLinearColor& Color, float fDuration)
{
    this->fBurstRadius = fRadius;
    this->fBurstDuration = FMath::Max(0.05f, fDuration);
    this->fBurstElapsed = 0.0f;
    this->ShowAt(Position, fRadius * 0.3f, 3.0f, Color);
    this->SetActorTickEnabled(true);
    this->SetLifeSpan(this->fBurstDuration);
}

void AHBIndicator::Tick(float fDeltaSeconds)
{
    Super::Tick(fDeltaSeconds);
    this->fBurstElapsed += fDeltaSeconds;
    const float fAlpha = FMath::Clamp(this->fBurstElapsed / this->fBurstDuration, 0.0f, 1.0f);
    const float fRadius = this->fBurstRadius * FMath::Lerp(0.3f, 1.0f, fAlpha);
    this->SetActorScale3D(FVector(fRadius, fRadius, 1.0f));
    if (this->Material)
    {
        this->Material->SetScalarParameterValue(TEXT("Opacity"), 1.0f - fAlpha);
    }
}

void UHBFeedbackComponent::PlayEffect(const FVector& Position, float fRadius, const FLinearColor& Color)
{
    for (int32 iIndex = this->Effects.Num() - 1; iIndex >= 0; --iIndex)
    {
        if (!this->Effects[iIndex].IsValid())
        {
            this->Effects.RemoveAtSwap(iIndex);
        }
    }
    AHBIndicator* Effect = this->CreateRing();
    if (Effect)
    {
        this->Effects.Add(Effect);
        Effect->PlayBurst(Position, fRadius, Color, 0.35f);
    }
}
