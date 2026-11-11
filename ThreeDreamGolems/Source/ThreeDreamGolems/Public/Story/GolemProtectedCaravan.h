#pragma once

#include "CoreMinimal.h"
#include "AbilityYard/GolemAbilityDummy.h"
#include "GolemProtectedCaravan.generated.h"

/** Noncanonical protected world target; combat uses the existing GAS/life/damage path. */
UCLASS()
class THREEDREAMGOLEMS_API AGolemProtectedCaravan : public AGolemAbilityDummy
{
	GENERATED_BODY()

public:
	AGolemProtectedCaravan();
	FGolemResult ResetCaravan();

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(EditDefaultsOnly, Category = "Golem|Story", meta = (ClampMin = "1"))
	float ProtectedHealth = 600.0f;
};
