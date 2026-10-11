#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "KnightDamageResponse.generated.h"

class AActor;

struct FKnightIncomingHitContext
{
	FGuid ActivationId;
	bool bDirect = false;
	uint64 TargetItemGeneration = 0;
};

/** Authoritative damage extension implemented by an actor or one of its domain components. */
UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UKnightDamageResponse : public UInterface
{
	GENERATED_BODY()
};

class KNIGHTABILITYSYSTEM_API IKnightDamageResponse
{
	GENERATED_BODY()

public:
	/** Called once by the real GAS execution after defense capture, before health mutation. */
	virtual void ResolveIncomingDamage(AActor* Source, float InDamage, float PoiseDamage,
		bool bCanDefend, float& OutHealthDamage) = 0;
	virtual void ResolveIncomingDamageWithContext(AActor* Source, float InDamage, float PoiseDamage,
		bool bCanDefend, const FKnightIncomingHitContext& Context, float& OutHealthDamage)
	{
		ResolveIncomingDamage(Source, InDamage, PoiseDamage, bCanDefend, OutHealthDamage);
	}
};
