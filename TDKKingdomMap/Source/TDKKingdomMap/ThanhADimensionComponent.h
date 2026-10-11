#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ThanhADimensionComponent.generated.h"

// Serialized measurement metadata; no ticking, rendering, or editor module dependencies.
// Kept loadable in client commandlets; all visualization is in the Editor module.
UCLASS(ClassGroup=(Blockout))
class TDKKINGDOMMAP_API UThanhADimensionComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UThanhADimensionComponent() { PrimaryComponentTick.bCanEverTick=false; }
    UPROPERTY(VisibleAnywhere, Category="Blockout") FString StableId;
    UPROPERTY(VisibleAnywhere, Category="Blockout") FString BuildingId;
    UPROPERTY(VisibleAnywhere, Category="Blockout") FString Kind;
    UPROPERTY(VisibleAnywhere, Category="Blockout") FVector TargetSizeM=FVector::ZeroVector;
    UPROPERTY(VisibleAnywhere, Category="Blockout") FVector TargetCenterM=FVector::ZeroVector;
    UPROPERTY(VisibleAnywhere, Category="Blockout") FVector GroupTargetSizeM=FVector::ZeroVector;
    UPROPERTY(VisibleAnywhere, Category="Blockout") FString AxisActorId;
    UPROPERTY(VisibleAnywhere, Category="Blockout") FTransform GeneratedTransform=FTransform::Identity;
    UPROPERTY(VisibleAnywhere, Category="Blockout") TArray<FVector> RoutePointsCm;
    UPROPERTY(VisibleAnywhere, Category="Blockout") float RouteWidthCm=0;
    UPROPERTY(VisibleAnywhere, Category="Blockout") int32 StepCount=0;
    UPROPERTY(EditAnywhere, Category="Blockout") bool MeasureWholeBuilding=true;
};
