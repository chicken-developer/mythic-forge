#pragma once

#include "CoreMinimal.h"
#include "Models/GolemGearModels.h"

class UGolemItemDefinition;

struct GOLEMITEMS_API FGolemGearSeed
{
	FName CatalogId;
	FPrimaryAssetId DefinitionId;
	FText Name;
	FText Description;
	FGolemItemMetadata Metadata;
	FGolemItemStatBonuses Stats;
	int32 MaximumStack = 1;
};

/** One shared native seed manifest is consumed by legitimate editor authoring and domain fixtures. */
struct GOLEMITEMS_API FGolemGearCatalog
{
	static const TArray<FGolemGearSeed>& GetSeeds();
	static FPrimaryAssetId DefinitionId(FName CatalogId);
	static FGolemResult ConfigureDefinition(UGolemItemDefinition* Definition, const FGolemGearSeed& Seed);
	static FGolemResult ValidateCatalog(TConstArrayView<UGolemItemDefinition*> Definitions);
	static FGolemGearRewardBundle Reward(FName Encounter, const FGuid& DemoRunId, const FGuid& EncounterId);
};
