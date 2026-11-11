#pragma once

#include "CoreMinimal.h"
#include "Assets/GolemDefinition.h"
#include "Map/GolemCampaignMapTypes.h"
#include "GolemStoryWorldDefinition.generated.h"

UENUM()
enum class EGolemStoryAction : uint8
{
	Fact,
	Conversation,
	Forge,
	Reward,
	Checkpoint,
	Travel,
	Encounter,
	Scene,
	Deliver,
	Branch
};

USTRUCT()
struct GOLEMWORLD_API FGolemStoryInteraction
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly)
	FName AnchorId;
	UPROPERTY(EditDefaultsOnly)
	FText Label;
	UPROPERTY(EditDefaultsOnly)
	EGolemStoryAction Action = EGolemStoryAction::Fact;
	UPROPERTY(EditDefaultsOnly)
	FName EventName = TEXT("Interacted");
	UPROPERTY(EditDefaultsOnly)
	FName SubjectId;
	UPROPERTY(EditDefaultsOnly)
	FName ValueId;
	UPROPERTY(EditDefaultsOnly)
	TArray<FName> AllowedChoices;
	UPROPERTY(EditDefaultsOnly)
	FPrimaryAssetId DeliveryItemId;
	UPROPERTY(EditDefaultsOnly)
	int32 DeliveryCount = 1;
	UPROPERTY(EditDefaultsOnly)
	FName RequiredQuestId;
	UPROPERTY(EditDefaultsOnly)
	FName RequiredObjectiveId;
	UPROPERTY(EditDefaultsOnly)
	FName RequiredBranchId;
	UPROPERTY(EditDefaultsOnly)
	FName RequiredBranchChoice;
	UPROPERTY(EditDefaultsOnly)
	FPrimaryAssetId ConversationId;
	UPROPERTY(EditDefaultsOnly)
	FName RewardQuestId;
	UPROPERTY(EditDefaultsOnly)
	TSoftObjectPtr<UWorld> Destination;
	UPROPERTY(EditDefaultsOnly)
	float InteractionRange = 260.0f;
};

UCLASS()
class GOLEMWORLD_API UGolemStoryWorldDefinition : public UGolemDefinition
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly)
	TArray<FGolemStoryInteraction> Interactions;
	UPROPERTY(EditDefaultsOnly)
	TArray<FGolemMapRegion> MapRegions;
	UPROPERTY(EditDefaultsOnly)
	TArray<FGolemMapMarker> MapMarkers;
	UPROPERTY(EditDefaultsOnly)
	FPrimaryAssetId EncounterQualityDefinitionId;
	FGolemResult ValidateWorld() const;
	const FGolemStoryInteraction* FindInteraction(FName AnchorId) const;
	const FGolemMapRegion* FindRegion(FName RegionId) const;
	const FGolemMapMarker* FindMarker(FName MarkerId) const;
};
