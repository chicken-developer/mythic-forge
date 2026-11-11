#pragma once

#include "CoreMinimal.h"
#include "Story/GolemStoryEntry.h"
#include "GolemCampaignEntry.generated.h"

UCLASS()
class THREEDREAMGOLEMS_API AGolemCampaignEntry : public AGolemStoryEntry
{
	GENERATED_BODY()
public:
	UPROPERTY(EditInstanceOnly, Category = "Golem|Campaign")
	FName RegionId;
	UPROPERTY(EditInstanceOnly, Category = "Golem|Campaign")
	bool bFrontEndEntry = false;
};
