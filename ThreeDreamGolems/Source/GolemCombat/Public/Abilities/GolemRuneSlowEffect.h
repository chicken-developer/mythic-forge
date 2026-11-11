#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "GolemRuneSlowEffect.generated.h"

/** A native, serializable GAS status definition; its duration is set from the combat profile. */
UCLASS()
class GOLEMCOMBAT_API UGolemRuneSlowEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGolemRuneSlowEffect();
};
