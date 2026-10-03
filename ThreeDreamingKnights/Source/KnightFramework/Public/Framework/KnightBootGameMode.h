#pragma once

#include "CoreMinimal.h"
#include "Events/KnightEventSubsystem.h"
#include "GameFramework/GameModeBase.h"
#include "KnightBootGameMode.generated.h"

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnKnightFoundationReady, const TArray<FText>&, bool);

/** Foundation entry point. Gameplay milestones compose their own game mode on this verified boot. */
UCLASS()
class KNIGHTFRAMEWORK_API AKnightBootGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AKnightBootGameMode();
	virtual void StartPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	FOnKnightFoundationReady OnFoundationReady;

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
	FKnightEventSubscription FoundationSubscription;
};
