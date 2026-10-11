#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "KnightPowerCrystalEffect.generated.h"

/** Replicable collector-only modifier. The skill runtime owns its handle and refresh deadline. */
UCLASS()
class UKnightPowerCrystalEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UKnightPowerCrystalEffect();
	static const FName PowerMultiplierMagnitude;
};
