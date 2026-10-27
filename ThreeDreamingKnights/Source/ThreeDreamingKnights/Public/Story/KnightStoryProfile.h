#pragma once

#include "CoreMinimal.h"
#include "Assets/KnightDefinition.h"
#include "Models/KnightGearModels.h"
#include "Skills/KnightSkillTypes.h"
#include "KnightStoryProfile.generated.h"

USTRUCT()
struct THREEDREAMINGKNIGHTS_API FKnightStoryReward
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly)
	FName QuestId;
	UPROPERTY(EditDefaultsOnly)
	FName RewardKey;
	UPROPERTY(EditDefaultsOnly)
	TArray<FKnightGearRewardItem> Items;
	UPROPERTY(EditDefaultsOnly)
	int64 Currency = 0;
	UPROPERTY(EditDefaultsOnly)
	bool bReuseExistingHammers = false;
};

/** Production and sandbox profiles use the same runtime with distinct save namespaces. */
UCLASS()
class THREEDREAMINGKNIGHTS_API UKnightStoryProfile : public UKnightDefinition
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly)
	FPrimaryAssetId CampaignDefinitionId;
	UPROPERTY(EditDefaultsOnly)
	FPrimaryAssetId WorldDefinitionId;
	UPROPERTY(EditDefaultsOnly)
	TArray<FKnightStoryReward> Rewards;
	UPROPERTY(EditDefaultsOnly)
	bool bDeveloperFixture = false;
	UPROPERTY(EditDefaultsOnly)
	TArray<FKnightGearRewardItem> DeveloperSeedItems;
	UPROPERTY(EditDefaultsOnly)
	TMap<FPrimaryAssetId, FKnightSkillBuild> DeveloperBuilds;
	UPROPERTY(EditDefaultsOnly)
	int32 DeveloperLevel = 1;
	UPROPERTY(EditDefaultsOnly)
	bool bDeveloperStarterEquipment = false;
};
