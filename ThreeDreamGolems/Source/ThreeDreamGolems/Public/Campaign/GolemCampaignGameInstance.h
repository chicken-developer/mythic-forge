#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Engine/EngineBaseTypes.h"
#include "GolemCampaignGameInstance.generated.h"

class UNetDriver;
DECLARE_MULTICAST_DELEGATE_OneParam(FOnGolemCampaignTransportFailure, const FText&);

/** Transient presentation lifecycle only. Campaign data remains in its domain/save owners. */
UCLASS()
class THREEDREAMGOLEMS_API UGolemCampaignGameInstance : public UGameInstance
{
	GENERATED_BODY()
public:
	virtual void Init() override;
	virtual void Shutdown() override;
	bool bEnteredCampaign = false;
	FText TravelFeedback;
	FOnGolemCampaignTransportFailure OnTransportFailure;
private:
	void HandleTravelFailure(UWorld* World, ETravelFailure::Type Type, const FString& Error);
	void HandleNetworkFailure(UWorld* World, UNetDriver* Driver, ENetworkFailure::Type Type, const FString& Error);
	void ReportFailure(UWorld* World, const FString& Error);
	FDelegateHandle TravelFailureHandle;
	FDelegateHandle NetworkFailureHandle;
};
