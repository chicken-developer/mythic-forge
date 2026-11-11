#pragma once

#include "CoreMinimal.h"
#include "Events/GolemConfirmedEvent.h"
#include "GolemStoryEvent.generated.h"

/** Occurrences carry campaign and encounter generations; requests never become facts. */
USTRUCT(BlueprintType)
struct GOLEMCORE_API FGolemStoryEvent : public FGolemConfirmedEvent
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	FName CampaignId;
	UPROPERTY(SaveGame)
	FGuid RunId;
	UPROPERTY(SaveGame)
	FGuid EncounterRunId;
	UPROPERTY(SaveGame)
	FName EventName;
	UPROPERTY(SaveGame)
	FName SubjectId;
	UPROPERTY(SaveGame)
	FName ValueId;
	UPROPERTY(SaveGame)
	int32 Quantity = 1;
};
