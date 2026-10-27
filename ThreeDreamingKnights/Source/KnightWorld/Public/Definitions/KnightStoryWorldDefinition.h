#pragma once

#include "CoreMinimal.h"
#include "Assets/KnightDefinition.h"
#include "Map/KnightCampaignMapTypes.h"
#include "KnightStoryWorldDefinition.generated.h"

UENUM()
enum class EKnightStoryAction : uint8
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
struct KNIGHTWORLD_API FKnightStoryInteraction
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly)
	FName AnchorId;
	UPROPERTY(EditDefaultsOnly)
	FText Label;
	UPROPERTY(EditDefaultsOnly)
	EKnightStoryAction Action = EKnightStoryAction::Fact;
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
class KNIGHTWORLD_API UKnightStoryWorldDefinition : public UKnightDefinition
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly)
	TArray<FKnightStoryInteraction> Interactions;
	UPROPERTY(EditDefaultsOnly)
	TArray<FKnightMapRegion> MapRegions;
	UPROPERTY(EditDefaultsOnly)
	TArray<FKnightMapMarker> MapMarkers;
	UPROPERTY(EditDefaultsOnly)
	FPrimaryAssetId EncounterQualityDefinitionId;
	FKnightResult ValidateWorld() const;
	const FKnightStoryInteraction* FindInteraction(FName AnchorId) const;
	const FKnightMapRegion* FindRegion(FName RegionId) const;
	const FKnightMapMarker* FindMarker(FName MarkerId) const;
};
