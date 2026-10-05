#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "KnightArenaResourceRecoveryEffect.generated.h"

/** Persistent M4 encounter recovery; owned by the pawn rather than its current controller. */
UCLASS()
class KNIGHTABILITYSYSTEM_API UKnightArenaResourceRecoveryEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UKnightArenaResourceRecoveryEffect();
};
