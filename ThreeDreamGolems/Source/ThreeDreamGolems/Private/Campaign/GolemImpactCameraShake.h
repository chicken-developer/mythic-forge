#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraShakeBase.h"
#include "GolemImpactCameraShake.generated.h"

UCLASS()
class UGolemImpactCameraShake : public UCameraShakeBase
{
	GENERATED_BODY()

public:
	UGolemImpactCameraShake(const FObjectInitializer& ObjectInitializer);
};
