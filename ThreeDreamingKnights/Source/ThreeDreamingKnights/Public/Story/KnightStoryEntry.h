#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "KnightStoryEntry.generated.h"

/** Authored map composition selects a profile; it never owns progression. */
UCLASS()
class THREEDREAMINGKNIGHTS_API AKnightStoryEntry : public AActor
{
	GENERATED_BODY()

public:
	UPROPERTY(EditInstanceOnly, Category = "Knight|Story")
	FPrimaryAssetId ProfileId;
};
