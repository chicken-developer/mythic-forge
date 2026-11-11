#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GolemTauntResponse.generated.h"

UINTERFACE(MinimalAPI)
class UGolemTauntResponse : public UInterface
{
	GENERATED_BODY()
};

/** Threat focus is temporary and never cancels an already committed boss mechanic. */
class GOLEMABILITYSYSTEM_API IGolemTauntResponse
{
	GENERATED_BODY()

public:
	virtual void ApplySkillTaunt(AActor* Source, float Duration) = 0;
};
