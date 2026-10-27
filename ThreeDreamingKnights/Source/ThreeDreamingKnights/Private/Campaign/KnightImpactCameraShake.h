#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraShakeBase.h"
#include "KnightImpactCameraShake.generated.h"

UCLASS()
class UKnightImpactCameraShake : public UCameraShakeBase
{
	GENERATED_BODY()

public:
	UKnightImpactCameraShake(const FObjectInitializer& ObjectInitializer);
};
