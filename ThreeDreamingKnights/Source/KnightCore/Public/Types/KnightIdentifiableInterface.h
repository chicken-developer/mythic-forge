#pragma once

#include "CoreMinimal.h"
#include "Types/KnightStableIds.h"
#include "UObject/Interface.h"
#include "KnightIdentifiableInterface.generated.h"

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UKnightIdentifiableInterface : public UInterface
{
	GENERATED_BODY()
};

/** Read-only identity contract, independent of actor/controller ownership or loaded presentation assets. */
class KNIGHTCORE_API IKnightIdentifiableInterface
{
	GENERATED_BODY()

public:
	virtual FKnightEntityId GetKnightEntityId() const = 0;
};
