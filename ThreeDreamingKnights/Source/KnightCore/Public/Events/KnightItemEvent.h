#pragma once

#include "CoreMinimal.h"
#include "Events/KnightConfirmedEvent.h"
#include "UObject/PrimaryAssetId.h"
#include "KnightItemEvent.generated.h"

/** Committed item facts on the existing bus; ConfirmationId is the downstream idempotency EventId. */
USTRUCT(BlueprintType)
struct KNIGHTCORE_API FKnightItemEvent : public FKnightConfirmedEvent
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "Knight|Events")
	FName EventName;
	UPROPERTY(BlueprintReadOnly, Category = "Knight|Events")
	FKnightEntityId TargetId;
	UPROPERTY(BlueprintReadOnly, Category = "Knight|Events")
	FPrimaryAssetId DefinitionId;
	UPROPERTY(BlueprintReadOnly, Category = "Knight|Events")
	FGuid InstanceId;
	UPROPERTY(BlueprintReadOnly, Category = "Knight|Events")
	int32 Quantity = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Knight|Events")
	FGuid DemoRunId;
	UPROPERTY(BlueprintReadOnly, Category = "Knight|Events")
	FGuid EncounterId;
	UPROPERTY(BlueprintReadOnly, Category = "Knight|Events")
	FGuid RequestId;
};
