#pragma once

#include "CoreMinimal.h"
#include "GearLab/GolemGearLabGameMode.h"
#include "GolemGearCatalogGameMode.generated.h"

/** Explicit QA entry; fixture-only grants do not touch a campaign. */
UCLASS()
class THREEDREAMGOLEMS_API AGolemGearCatalogGameMode : public AGolemGearLabGameMode
{
	GENERATED_BODY()
protected:
	virtual bool IsCatalogLab() const override;
};
