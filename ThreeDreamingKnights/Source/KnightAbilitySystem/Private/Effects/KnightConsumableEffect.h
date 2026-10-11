#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "KnightConsumableEffect.generated.h"

/** Instant item recovery bypasses all ability-heal and specialization proc entry points. */
UCLASS()
class UKnightConsumableEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UKnightConsumableEffect();
	static const FName HealthMagnitude;
	static const FName ResourceMagnitude;
};
