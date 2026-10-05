#pragma once

#include "Assets/KnightDefinition.h"
#include "Models/KnightRPGModels.h"
#include "KnightLootTableDefinition.generated.h"

USTRUCT(BlueprintType)
struct KNIGHTITEMS_API FKnightLootEntry
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, Category = "Knight|Items|Loot")
	FPrimaryAssetId DefinitionId;
	UPROPERTY(EditDefaultsOnly, Category = "Knight|Items|Loot")
	int32 MinimumCount = 1;
	UPROPERTY(EditDefaultsOnly, Category = "Knight|Items|Loot")
	int32 MaximumCount = 1;
	UPROPERTY(EditDefaultsOnly, Category = "Knight|Items|Loot")
	float Probability = 1.0f;
};

UCLASS(BlueprintType)
class KNIGHTITEMS_API UKnightLootTableDefinition : public UKnightDefinition
{
	GENERATED_BODY()

public:
	UKnightLootTableDefinition();
	const TArray<FKnightLootEntry>& GetEntries() const;
	FKnightResult TrySetEntries(const TArray<FKnightLootEntry>& InEntries);
	FKnightResult ValidateLootTable() const;
	FKnightResult GenerateLoot(int32 Seed, const FGuid& RewardId, TArray<FKnightItemInstance>& OutItems) const;
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

private:
	UPROPERTY(EditDefaultsOnly, Category = "Knight|Items|Loot")
	TArray<FKnightLootEntry> Entries;
};
