#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GolemTraversalObstacle.generated.h"

class UStaticMeshComponent;
class UTextRenderComponent;

/** Physical 90 cm hurdle with supporting floor on both sides for the actual Motion Warping exercise. */
UCLASS()
class THREEDREAMGOLEMS_API AGolemTraversalObstacle : public AActor
{
	GENERATED_BODY()

public:
	AGolemTraversalObstacle();
	UStaticMeshComponent* GetObstacleMesh() const;

private:
	UPROPERTY(VisibleAnywhere, Category = "Golem|CombatChallenge")
	TObjectPtr<UStaticMeshComponent> Obstacle;
	UPROPERTY(VisibleAnywhere, Category = "Golem|CombatChallenge")
	TObjectPtr<UTextRenderComponent> Label;
};
