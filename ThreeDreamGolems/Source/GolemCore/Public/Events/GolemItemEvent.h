#pragma once

#include "CoreMinimal.h"
#include "Events/GolemConfirmedEvent.h"
#include "UObject/PrimaryAssetId.h"
#include "GolemItemEvent.generated.h"

/** Committed item facts on the existing bus; ConfirmationId is the downstream idempotency EventId. */
USTRUCT(BlueprintType)
struct GOLEMCORE_API FGolemItemEvent : public FGolemConfirmedEvent
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "Golem|Events")
	FName EventName;
	UPROPERTY(BlueprintReadOnly, Category = "Golem|Events")
	FGolemEntityId TargetId;
	UPROPERTY(BlueprintReadOnly, Category = "Golem|Events")
	FPrimaryAssetId DefinitionId;
	UPROPERTY(BlueprintReadOnly, Category = "Golem|Events")
	FGuid InstanceId;
	UPROPERTY(BlueprintReadOnly, Category = "Golem|Events")
	int32 Quantity = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Golem|Events")
	FGuid DemoRunId;
	UPROPERTY(BlueprintReadOnly, Category = "Golem|Events")
	FGuid EncounterId;
	UPROPERTY(BlueprintReadOnly, Category = "Golem|Events")
	FGuid RequestId;
};
