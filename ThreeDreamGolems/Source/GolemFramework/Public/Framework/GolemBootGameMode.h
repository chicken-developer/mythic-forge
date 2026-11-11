#pragma once

#include "CoreMinimal.h"
#include "Events/GolemEventSubsystem.h"
#include "GameFramework/GameModeBase.h"
#include "GolemBootGameMode.generated.h"

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnGolemFoundationReady, const TArray<FText>&, bool);

/** Foundation entry point. Gameplay milestones compose their own game mode on this verified boot. */
UCLASS()
class GOLEMFRAMEWORK_API AGolemBootGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AGolemBootGameMode();
	virtual void StartPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	FOnGolemFoundationReady OnFoundationReady;

	bool IsFoundationReady() const
	{
		return bIsFoundationReady;
	}

	const TArray<FText>& GetFoundationStatus() const
	{
		return FoundationStatus;
	}

private:
	UPROPERTY(Transient)
	TArray<FText> FoundationStatus;

	bool bIsFoundationReady = false;
	int32 ReceivedFoundationEvents = 0;
	FGolemEventSubscription FoundationSubscription;
};
