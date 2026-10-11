#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "KnightTauntResponse.generated.h"

UINTERFACE(MinimalAPI)
class UKnightTauntResponse : public UInterface
{
	GENERATED_BODY()
};

/** Threat focus is temporary and never cancels an already committed boss mechanic. */
class KNIGHTABILITYSYSTEM_API IKnightTauntResponse
{
	GENERATED_BODY()

public:
	virtual void ApplySkillTaunt(AActor* Source, float Duration) = 0;
};
