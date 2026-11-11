#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GolemPartyArenaStartMarker.generated.h"

class UStaticMeshComponent;
class UTextRenderComponent;

UCLASS()
class THREEDREAMGOLEMS_API AGolemPartyArenaStartMarker : public AActor
{
	GENERATED_BODY()

public:
	AGolemPartyArenaStartMarker();
	float GetInteractionRange() const;

private:
	UPROPERTY(VisibleAnywhere, Category = "Golem|Arena")
	TObjectPtr<UStaticMeshComponent> MarkerMesh;
	UPROPERTY(VisibleAnywhere, Category = "Golem|Arena")
	TObjectPtr<UTextRenderComponent> Label;
	UPROPERTY(EditDefaultsOnly, Category = "Golem|Arena", meta = (ClampMin = "1", Units = "cm"))
	float InteractionRange = 300.0f;
};
