#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "GolemArenaResourceRecoveryEffect.generated.h"

/** Persistent M4 encounter recovery; owned by the pawn rather than its current controller. */
UCLASS()
class GOLEMABILITYSYSTEM_API UGolemArenaResourceRecoveryEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGolemArenaResourceRecoveryEffect();
};
