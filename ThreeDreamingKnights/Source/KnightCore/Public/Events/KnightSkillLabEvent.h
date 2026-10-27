#pragma once

#include "CoreMinimal.h"
#include "Events/KnightConfirmedEvent.h"
#include "KnightSkillLabEvent.generated.h"

/** M5.5 confirmed adapter facts; M6 may subscribe without owning this sandbox's state. */
USTRUCT(BlueprintType)
struct KNIGHTCORE_API FKnightSkillLabEvent : public FKnightConfirmedEvent
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Knight|Events")
	FName EventName;
	UPROPERTY(BlueprintReadOnly, Category = "Knight|Events")
	FGuid EncounterId;
	UPROPERTY(BlueprintReadOnly, Category = "Knight|Events")
	FGuid RequestId;
	UPROPERTY(BlueprintReadOnly, Category = "Knight|Events")
	FKnightEntityId TargetId;
	UPROPERTY(BlueprintReadOnly, Category = "Knight|Events")
	FName SubjectId;
	UPROPERTY(BlueprintReadOnly, Category = "Knight|Events")
	FName ValueId;
};
