#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "GolemPowerCrystalEffect.generated.h"

/** Replicable collector-only modifier. The skill runtime owns its handle and refresh deadline. */
UCLASS()
class UGolemPowerCrystalEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGolemPowerCrystalEffect();
	static const FName PowerMultiplierMagnitude;
};
