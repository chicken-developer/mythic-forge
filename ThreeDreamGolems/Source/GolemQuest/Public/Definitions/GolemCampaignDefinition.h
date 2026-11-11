#pragma once

#include "CoreMinimal.h"
#include "Assets/GolemDefinition.h"
#include "GolemCampaignDefinition.generated.h"

UENUM()
enum class EGolemObjectivePolicy : uint8
{
	AfterActivation,
	CurrentState
};

USTRUCT()
struct GOLEMQUEST_API FGolemObjectiveDefinition
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
	EGolemObjectivePolicy Policy = EGolemObjectivePolicy::AfterActivation;
	UPROPERTY(EditDefaultsOnly)
	bool bEncounterScoped = false;
};

USTRUCT()
struct GOLEMQUEST_API FGolemQuestDefinition
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
	TArray<FGolemObjectiveDefinition> Objectives;
	UPROPERTY(EditDefaultsOnly)
	bool bOrdered = false;
	UPROPERTY(EditDefaultsOnly)
	bool bAutoActivate = true;
	UPROPERTY(EditDefaultsOnly)
	FName RewardKey;
};

UCLASS()
class GOLEMQUEST_API UGolemCampaignDefinition : public UGolemDefinition
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
	TArray<FGolemQuestDefinition> Quests;

	FGolemResult ValidateCampaign() const;
	const FGolemQuestDefinition* FindQuest(FName QuestId) const;
};
