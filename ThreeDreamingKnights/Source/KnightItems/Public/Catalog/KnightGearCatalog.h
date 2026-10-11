#pragma once

#include "CoreMinimal.h"
#include "Models/KnightGearModels.h"

class UKnightItemDefinition;

struct KNIGHTITEMS_API FKnightGearSeed
{
	FName CatalogId;
	FPrimaryAssetId DefinitionId;
	FText Name;
	FText Description;
	FKnightItemMetadata Metadata;
	FKnightItemStatBonuses Stats;
	int32 MaximumStack = 1;
};

/** One shared native seed manifest is consumed by legitimate editor authoring and domain fixtures. */
struct KNIGHTITEMS_API FKnightGearCatalog
{
	static const TArray<FKnightGearSeed>& GetSeeds();
	static FPrimaryAssetId DefinitionId(FName CatalogId);
	static FKnightResult ConfigureDefinition(UKnightItemDefinition* Definition, const FKnightGearSeed& Seed);
	static FKnightResult ValidateCatalog(TConstArrayView<UKnightItemDefinition*> Definitions);
	static FKnightGearRewardBundle Reward(FName Encounter, const FGuid& DemoRunId, const FGuid& EncounterId);
};
