#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "KnightRuneSlowEffect.generated.h"

/** A native, serializable GAS status definition; its duration is set from the combat profile. */
UCLASS()
class KNIGHTCOMBAT_API UKnightRuneSlowEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UKnightRuneSlowEffect();
};
