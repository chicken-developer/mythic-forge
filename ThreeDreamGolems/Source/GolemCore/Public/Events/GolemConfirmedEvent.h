#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Types/GolemStableIds.h"
#include "GolemConfirmedEvent.generated.h"

/** Published only after a domain commits a mutation; the bus never performs transactions or replication. */
USTRUCT(BlueprintType)
struct GOLEMCORE_API FGolemConfirmedEvent
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Golem|Core|Events")
	FGuid ConfirmationId;

	UPROPERTY(BlueprintReadOnly, Category = "Golem|Core|Events")
	int64 Revision = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Golem|Core|Events")
	FGolemEntityId SourceId;
};

USTRUCT(BlueprintType)
struct GOLEMCORE_API FGolemFoundationEvent : public FGolemConfirmedEvent
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Golem|Core|Events")
	FGameplayTag Stage;

	UPROPERTY(BlueprintReadOnly, Category = "Golem|Core|Events")
	FPrimaryAssetId DefinitionId;
};
