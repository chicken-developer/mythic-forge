#pragma once
#include "CoreMinimal.h"
#include "Engine/StaticMeshActor.h"
#include "ThanhABlockoutMeshActor.generated.h"
class UThanhADimensionComponent;

// A native design root survives map loading and construction-script reconstruction.
UCLASS()
class TDKKINGDOMMAP_API AThanhABlockoutMeshActor : public AStaticMeshActor
{
    GENERATED_BODY()
public:
    AThanhABlockoutMeshActor();
    UPROPERTY(VisibleAnywhere, Category="Blockout") TObjectPtr<UThanhADimensionComponent> Dimensions;
};
