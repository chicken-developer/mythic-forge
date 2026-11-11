#pragma once

#include "CoreMinimal.h"
#include "Assets/GolemDefinition.h"
#include "Models/GolemGearModels.h"
#include "Skills/GolemSkillTypes.h"
#include "GolemStoryProfile.generated.h"

USTRUCT()
struct THREEDREAMGOLEMS_API FGolemStoryReward
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly)
	FName QuestId;
	UPROPERTY(EditDefaultsOnly)
	FName RewardKey;
	UPROPERTY(EditDefaultsOnly)
	TArray<FGolemGearRewardItem> Items;
	UPROPERTY(EditDefaultsOnly)
	int64 Currency = 0;
	UPROPERTY(EditDefaultsOnly)
	bool bReuseExistingHammers = false;
};

/** Production and sandbox profiles use the same runtime with distinct save namespaces. */
UCLASS()
class THREEDREAMGOLEMS_API UGolemStoryProfile : public UGolemDefinition
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly)
	FPrimaryAssetId CampaignDefinitionId;
	UPROPERTY(EditDefaultsOnly)
	FPrimaryAssetId WorldDefinitionId;
	UPROPERTY(EditDefaultsOnly)
	TArray<FGolemStoryReward> Rewards;
	UPROPERTY(EditDefaultsOnly)
	bool bDeveloperFixture = false;
	UPROPERTY(EditDefaultsOnly)
	TArray<FGolemGearRewardItem> DeveloperSeedItems;
	UPROPERTY(EditDefaultsOnly)
	TMap<FPrimaryAssetId, FGolemSkillBuild> DeveloperBuilds;
	UPROPERTY(EditDefaultsOnly)
	int32 DeveloperLevel = 1;
	UPROPERTY(EditDefaultsOnly)
	bool bDeveloperStarterEquipment = false;
};
