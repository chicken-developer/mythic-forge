#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "GolemConsumableEffect.generated.h"

/** Instant item recovery bypasses all ability-heal and specialization proc entry points. */
UCLASS()
class UGolemConsumableEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGolemConsumableEffect();
	static const FName HealthMagnitude;
	static const FName ResourceMagnitude;
};
