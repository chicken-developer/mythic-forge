#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "KnightSourceProjectionEffect.generated.h"

/** Five additive passive modifiers. The ASC owns and removes its exact active handle. */
UCLASS()
class KNIGHTABILITYSYSTEM_API UKnightSourceProjectionEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UKnightSourceProjectionEffect();
	static const FName MaxHealthMagnitude;
	static const FName MaxStaminaMagnitude;
	static const FName MaxRuneEnergyMagnitude;
	static const FName AttackPowerMagnitude;
	static const FName DefenseMagnitude;
};
