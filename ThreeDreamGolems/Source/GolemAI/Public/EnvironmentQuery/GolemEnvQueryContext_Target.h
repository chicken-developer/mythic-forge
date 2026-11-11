#pragma once

#include "CoreMinimal.h"
#include "EnvironmentQuery/EnvQueryContext.h"
#include "GolemEnvQueryContext_Target.generated.h"

/** Supplies the validated target of the pawn's current AI controller to a real EQS query. */
UCLASS()
class GOLEMAI_API UGolemEnvQueryContext_Target : public UEnvQueryContext
{
	GENERATED_BODY()

public:
	virtual void ProvideContext(FEnvQueryInstance& QueryInstance, FEnvQueryContextData& ContextData) const override;
};
