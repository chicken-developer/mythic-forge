#pragma once

#include "Assets/GolemDefinition.h"
#include "Models/GolemRPGModels.h"
#include "GolemLootTableDefinition.generated.h"

USTRUCT(BlueprintType)
struct GOLEMITEMS_API FGolemLootEntry
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, Category = "Golem|Items|Loot")
	FPrimaryAssetId DefinitionId;
	UPROPERTY(EditDefaultsOnly, Category = "Golem|Items|Loot")
	int32 MinimumCount = 1;
	UPROPERTY(EditDefaultsOnly, Category = "Golem|Items|Loot")
	int32 MaximumCount = 1;
	UPROPERTY(EditDefaultsOnly, Category = "Golem|Items|Loot")
	float Probability = 1.0f;
};

UCLASS(BlueprintType)
class GOLEMITEMS_API UGolemLootTableDefinition : public UGolemDefinition
{
	GENERATED_BODY()

public:
	UGolemLootTableDefinition();
	const TArray<FGolemLootEntry>& GetEntries() const;
	FGolemResult TrySetEntries(const TArray<FGolemLootEntry>& InEntries);
	FGolemResult ValidateLootTable() const;
	FGolemResult GenerateLoot(int32 Seed, const FGuid& RewardId, TArray<FGolemItemInstance>& OutItems) const;
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

private:
	UPROPERTY(EditDefaultsOnly, Category = "Golem|Items|Loot")
	TArray<FGolemLootEntry> Entries;
};
