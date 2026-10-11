#include "ThanhABlockoutMeshActor.h"
#include "ThanhADimensionComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"

AThanhABlockoutMeshActor::AThanhABlockoutMeshActor()
{
    auto* Pivot=CreateDefaultSubobject<USceneComponent>(TEXT("DesignOrigin")); Pivot->SetMobility(EComponentMobility::Static);
    SetRootComponent(Pivot); GetStaticMeshComponent()->SetupAttachment(Pivot);
    Dimensions=CreateDefaultSubobject<UThanhADimensionComponent>(TEXT("BlockoutDimensions"));
}
