#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "KnightArmorResponse.generated.h"

UINTERFACE(MinimalAPI)
class UKnightArmorResponse : public UInterface
{
	GENERATED_BODY()
};

/** Optional hit-zone adapter. Zero output suppresses a duplicate positive-damage packet, including Poise. */
class KNIGHTABILITYSYSTEM_API IKnightArmorResponse
{
	GENERATED_BODY()

public:
	virtual void ResolveArmorHit(const FHitResult& Hit, const FGuid& ActivationId, int32 HitIndex,
		bool bHeavy, float RawDamage, float& OutDamage) = 0;
};
