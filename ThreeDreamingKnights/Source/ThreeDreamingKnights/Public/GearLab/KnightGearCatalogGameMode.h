#pragma once

#include "CoreMinimal.h"
#include "GearLab/KnightGearLabGameMode.h"
#include "KnightGearCatalogGameMode.generated.h"

/** Explicit QA entry; fixture-only grants do not touch a campaign. */
UCLASS()
class THREEDREAMINGKNIGHTS_API AKnightGearCatalogGameMode : public AKnightGearLabGameMode
{
	GENERATED_BODY()
protected:
	virtual bool IsCatalogLab() const override;
};
