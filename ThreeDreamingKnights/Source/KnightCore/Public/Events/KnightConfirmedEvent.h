#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Types/KnightStableIds.h"
#include "KnightConfirmedEvent.generated.h"

/** Published only after a domain commits a mutation; the bus never performs transactions or replication. */
USTRUCT(BlueprintType)
struct KNIGHTCORE_API FKnightConfirmedEvent
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Knight|Core|Events")
	FGuid ConfirmationId;

	UPROPERTY(BlueprintReadOnly, Category = "Knight|Core|Events")
	int64 Revision = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Knight|Core|Events")
	FKnightEntityId SourceId;
};

USTRUCT(BlueprintType)
struct KNIGHTCORE_API FKnightFoundationEvent : public FKnightConfirmedEvent
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Knight|Core|Events")
	FGameplayTag Stage;

	UPROPERTY(BlueprintReadOnly, Category = "Knight|Core|Events")
	FPrimaryAssetId DefinitionId;
};
