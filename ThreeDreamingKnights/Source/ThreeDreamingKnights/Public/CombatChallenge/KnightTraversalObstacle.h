#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "KnightTraversalObstacle.generated.h"

class UStaticMeshComponent;
class UTextRenderComponent;

/** Physical 90 cm hurdle with supporting floor on both sides for the actual Motion Warping exercise. */
UCLASS()
class THREEDREAMINGKNIGHTS_API AKnightTraversalObstacle : public AActor
{
	GENERATED_BODY()

public:
	AKnightTraversalObstacle();
	UStaticMeshComponent* GetObstacleMesh() const;

private:
	UPROPERTY(VisibleAnywhere, Category = "Knight|CombatChallenge")
	TObjectPtr<UStaticMeshComponent> Obstacle;
	UPROPERTY(VisibleAnywhere, Category = "Knight|CombatChallenge")
	TObjectPtr<UTextRenderComponent> Label;
};
