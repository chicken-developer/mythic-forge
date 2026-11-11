#pragma once

#include "CoreMinimal.h"
#include "Events/GolemConfirmedEvent.h"
#include "GolemSkillLabEvent.generated.h"

/** M5.5 confirmed adapter facts; M6 may subscribe without owning this sandbox's state. */
USTRUCT(BlueprintType)
struct GOLEMCORE_API FGolemSkillLabEvent : public FGolemConfirmedEvent
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Golem|Events")
	FName EventName;
	UPROPERTY(BlueprintReadOnly, Category = "Golem|Events")
	FGuid EncounterId;
	UPROPERTY(BlueprintReadOnly, Category = "Golem|Events")
	FGuid RequestId;
	UPROPERTY(BlueprintReadOnly, Category = "Golem|Events")
	FGolemEntityId TargetId;
	UPROPERTY(BlueprintReadOnly, Category = "Golem|Events")
	FName SubjectId;
	UPROPERTY(BlueprintReadOnly, Category = "Golem|Events")
	FName ValueId;
};
