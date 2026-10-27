#pragma once

#include "CoreMinimal.h"
#include "Story/KnightStoryEntry.h"
#include "KnightCampaignEntry.generated.h"

UCLASS()
class THREEDREAMINGKNIGHTS_API AKnightCampaignEntry : public AKnightStoryEntry
{
	GENERATED_BODY()
public:
	UPROPERTY(EditInstanceOnly, Category = "Knight|Campaign")
	FName RegionId;
	UPROPERTY(EditInstanceOnly, Category = "Knight|Campaign")
	bool bFrontEndEntry = false;
};
