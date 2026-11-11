#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GolemArmorResponse.generated.h"

UINTERFACE(MinimalAPI)
class UGolemArmorResponse : public UInterface
{
	GENERATED_BODY()
};

/** Optional hit-zone adapter. Zero output suppresses a duplicate positive-damage packet, including Poise. */
class GOLEMABILITYSYSTEM_API IGolemArmorResponse
{
	GENERATED_BODY()

public:
	virtual void ResolveArmorHit(const FHitResult& Hit, const FGuid& ActivationId, int32 HitIndex,
		bool bHeavy, float RawDamage, float& OutDamage) = 0;
};
