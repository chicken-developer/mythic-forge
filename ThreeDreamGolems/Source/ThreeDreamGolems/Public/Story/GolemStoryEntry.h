#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GolemStoryEntry.generated.h"

/** Authored map composition selects a profile; it never owns progression. */
UCLASS()
class THREEDREAMGOLEMS_API AGolemStoryEntry : public AActor
{
	GENERATED_BODY()

public:
	UPROPERTY(EditInstanceOnly, Category = "Golem|Story")
	FPrimaryAssetId ProfileId;
};
