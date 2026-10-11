#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "KnightSourceProjectionEffect.generated.h"

/** Additive RPG stat projection. The ASC owns and removes its exact active handle. */
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
	static const FName CritChanceMagnitude;
	static const FName HasteMagnitude;
	static const FName CooldownRecoveryMagnitude;
	static const FName MasteryMagnitude;
	static const FName VersatilityMagnitude;
	static const FName PoiseMagnitude;
};
