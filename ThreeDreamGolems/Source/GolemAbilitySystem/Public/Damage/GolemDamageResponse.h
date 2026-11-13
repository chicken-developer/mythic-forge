#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GolemDamageResponse.generated.h"

class AActor;

/** Authoritative damage extension implemented by an actor or one of its domain components. */
UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UGolemDamageResponse : public UInterface
{
	GENERATED_BODY()
};

class GOLEMABILITYSYSTEM_API IGolemDamageResponse
{
	GENERATED_BODY()

public:
	/** Called once by the real GAS execution after defense capture, before health mutation. */
	virtual void ResolveIncomingDamage(AActor* Source, float InDamage, bool bCanDefend, float& OutHealthDamage) = 0;
};
