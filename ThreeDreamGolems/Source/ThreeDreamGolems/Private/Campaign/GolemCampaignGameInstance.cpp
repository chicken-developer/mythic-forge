#include "Campaign/GolemCampaignGameInstance.h"

#include "Engine/Engine.h"
#include "Engine/World.h"

void UGolemCampaignGameInstance::Init()
{
	Super::Init();
	if (GEngine != nullptr)
	{
		TravelFailureHandle = GEngine->OnTravelFailure().AddUObject(this, &ThisClass::HandleTravelFailure);
		NetworkFailureHandle = GEngine->OnNetworkFailure().AddUObject(this, &ThisClass::HandleNetworkFailure);
	}
}

void UGolemCampaignGameInstance::Shutdown()
{
	if (GEngine != nullptr)
	{
		GEngine->OnTravelFailure().Remove(TravelFailureHandle);
		GEngine->OnNetworkFailure().Remove(NetworkFailureHandle);
	}
	OnTransportFailure.Clear();
	Super::Shutdown();
}

void UGolemCampaignGameInstance::ReportFailure(UWorld* World, const FString& Error)
{
	if (World == nullptr || World->GetGameInstance() != this)
	{
		return;
	}
	TravelFeedback = FText::Format(NSLOCTEXT("GolemCampaignUI", "TransportFailure",
		"Travel or connection failed: {0}. The last durable checkpoint remains available."), FText::FromString(Error));
	OnTransportFailure.Broadcast(TravelFeedback);
}

void UGolemCampaignGameInstance::HandleTravelFailure(UWorld* World, ETravelFailure::Type Type, const FString& Error)
{
	ReportFailure(World, Error);
}

void UGolemCampaignGameInstance::HandleNetworkFailure(UWorld* World, UNetDriver* Driver,
	ENetworkFailure::Type Type, const FString& Error)
{
	ReportFailure(World, Error);
}
