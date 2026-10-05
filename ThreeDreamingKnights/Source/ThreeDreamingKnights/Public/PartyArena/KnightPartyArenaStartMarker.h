#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "KnightPartyArenaStartMarker.generated.h"

class UStaticMeshComponent;
class UTextRenderComponent;

UCLASS()
class THREEDREAMINGKNIGHTS_API AKnightPartyArenaStartMarker : public AActor
{
	GENERATED_BODY()

public:
	AKnightPartyArenaStartMarker();
	float GetInteractionRange() const;

private:
	UPROPERTY(VisibleAnywhere, Category = "Knight|Arena")
	TObjectPtr<UStaticMeshComponent> MarkerMesh;
	UPROPERTY(VisibleAnywhere, Category = "Knight|Arena")
	TObjectPtr<UTextRenderComponent> Label;
	UPROPERTY(EditDefaultsOnly, Category = "Knight|Arena", meta = (ClampMin = "1", Units = "cm"))
	float InteractionRange = 300.0f;
};
