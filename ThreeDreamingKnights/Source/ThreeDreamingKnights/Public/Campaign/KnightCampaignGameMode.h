#pragma once

#include "CoreMinimal.h"
#include "Story/KnightStoryGameMode.h"
#include "KnightCampaignGameMode.generated.h"

UCLASS()
class THREEDREAMINGKNIGHTS_API AKnightCampaignGameMode : public AKnightStoryGameMode
{
	GENERATED_BODY()
public:
	AKnightCampaignGameMode();
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type Reason) override;
private:
	void RefreshDiscovery();
	FTimerHandle DiscoveryTimer;
};
