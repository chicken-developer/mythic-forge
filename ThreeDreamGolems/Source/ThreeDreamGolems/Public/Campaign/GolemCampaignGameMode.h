#pragma once

#include "CoreMinimal.h"
#include "Story/GolemStoryGameMode.h"
#include "GolemCampaignGameMode.generated.h"

UCLASS()
class THREEDREAMGOLEMS_API AGolemCampaignGameMode : public AGolemStoryGameMode
{
	GENERATED_BODY()
public:
	AGolemCampaignGameMode();
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type Reason) override;
private:
	void RefreshDiscovery();
	FTimerHandle DiscoveryTimer;
};
