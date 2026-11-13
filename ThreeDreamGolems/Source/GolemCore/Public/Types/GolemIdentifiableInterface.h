#pragma once

#include "CoreMinimal.h"
#include "Types/GolemStableIds.h"
#include "UObject/Interface.h"
#include "GolemIdentifiableInterface.generated.h"

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UGolemIdentifiableInterface : public UInterface
{
	GENERATED_BODY()
};

/** Read-only identity contract, independent of actor/controller ownership or loaded presentation assets. */
class GOLEMCORE_API IGolemIdentifiableInterface
{
	GENERATED_BODY()

public:
	virtual FGolemEntityId GetGolemEntityId() const = 0;
};
