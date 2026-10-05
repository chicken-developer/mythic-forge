#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "KnightRPGUpgradeStation.generated.h"

class UStaticMeshComponent;
class UTextRenderComponent;

/** A physical safe-zone marker; domain policy also requires an inactive encounter. */
UCLASS()
class THREEDREAMINGKNIGHTS_API AKnightRPGUpgradeStation : public AActor
{
	GENERATED_BODY()

public:
	AKnightRPGUpgradeStation();
	float GetInteractionRange() const;

private:
	UPROPERTY(VisibleAnywhere, Category = "Knight|RPG|Station")
	TObjectPtr<UStaticMeshComponent> Platform;

	UPROPERTY(VisibleAnywhere, Category = "Knight|RPG|Station")
	TObjectPtr<UTextRenderComponent> Label;

	UPROPERTY(EditDefaultsOnly, Category = "Knight|RPG|Station", meta = (ClampMin = "100", Units = "cm"))
	float InteractionRange = 350.0f;
};
