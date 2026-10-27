#pragma once

#include "CoreMinimal.h"
#include "Assets/KnightDefinition.h"
#include "KnightCampaignDefinition.generated.h"

UENUM()
enum class EKnightObjectivePolicy : uint8
{
	AfterActivation,
	CurrentState
};

USTRUCT()
struct KNIGHTQUEST_API FKnightObjectiveDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly)
	FName ObjectiveId;
	UPROPERTY(EditDefaultsOnly)
	FText Label;
	UPROPERTY(EditDefaultsOnly)
	FName EventName;
	UPROPERTY(EditDefaultsOnly)
	FName SubjectId;
	UPROPERTY(EditDefaultsOnly)
	FName ValueId;
	UPROPERTY(EditDefaultsOnly)
	int32 RequiredCount = 1;
	UPROPERTY(EditDefaultsOnly)
	EKnightObjectivePolicy Policy = EKnightObjectivePolicy::AfterActivation;
	UPROPERTY(EditDefaultsOnly)
	bool bEncounterScoped = false;
};

USTRUCT()
struct KNIGHTQUEST_API FKnightQuestDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly)
	FName QuestId;
	UPROPERTY(EditDefaultsOnly)
	int32 IntroducedRevision = 1;
	UPROPERTY(EditDefaultsOnly)
	FName ActId;
	UPROPERTY(EditDefaultsOnly)
	FText Title;
	UPROPERTY(EditDefaultsOnly)
	TArray<FName> Prerequisites;
	UPROPERTY(EditDefaultsOnly)
	TArray<FKnightObjectiveDefinition> Objectives;
	UPROPERTY(EditDefaultsOnly)
	bool bOrdered = false;
	UPROPERTY(EditDefaultsOnly)
	bool bAutoActivate = true;
	UPROPERTY(EditDefaultsOnly)
	FName RewardKey;
};

UCLASS()
class KNIGHTQUEST_API UKnightCampaignDefinition : public UKnightDefinition
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly)
	FName CampaignId;
	UPROPERTY(EditDefaultsOnly)
	FName ScenarioKind;
	UPROPERTY(EditDefaultsOnly)
	int32 ContentRevision = 1;
	UPROPERTY(EditDefaultsOnly)
	TArray<FName> ActIds;
	UPROPERTY(EditDefaultsOnly)
	TArray<FKnightQuestDefinition> Quests;

	FKnightResult ValidateCampaign() const;
	const FKnightQuestDefinition* FindQuest(FName QuestId) const;
};
