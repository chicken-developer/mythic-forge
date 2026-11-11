#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GolemRPGUpgradeStation.generated.h"

class UStaticMeshComponent;
class UTextRenderComponent;

/** A physical safe-zone marker; domain policy also requires an inactive encounter. */
UCLASS()
class THREEDREAMGOLEMS_API AGolemRPGUpgradeStation : public AActor
{
	GENERATED_BODY()

public:
	AGolemRPGUpgradeStation();
	float GetInteractionRange() const;

private:
	UPROPERTY(VisibleAnywhere, Category = "Golem|RPG|Station")
	TObjectPtr<UStaticMeshComponent> Platform;

	UPROPERTY(VisibleAnywhere, Category = "Golem|RPG|Station")
	TObjectPtr<UTextRenderComponent> Label;

	UPROPERTY(EditDefaultsOnly, Category = "Golem|RPG|Station", meta = (ClampMin = "100", Units = "cm"))
	float InteractionRange = 350.0f;
};
