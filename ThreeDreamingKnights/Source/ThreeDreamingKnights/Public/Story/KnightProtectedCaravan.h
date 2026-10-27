#pragma once

#include "CoreMinimal.h"
#include "AbilityYard/KnightAbilityDummy.h"
#include "KnightProtectedCaravan.generated.h"

/** Noncanonical protected world target; combat uses the existing GAS/life/damage path. */
UCLASS()
class THREEDREAMINGKNIGHTS_API AKnightProtectedCaravan : public AKnightAbilityDummy
{
	GENERATED_BODY()

public:
	AKnightProtectedCaravan();
	FKnightResult ResetCaravan();

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(EditDefaultsOnly, Category = "Knight|Story", meta = (ClampMin = "1"))
	float ProtectedHealth = 600.0f;
};
