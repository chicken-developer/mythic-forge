#pragma once

#include "CoreMinimal.h"
#include "Assets/GolemDefinition.h"
#include "GolemEncounterQualityDefinition.generated.h"

USTRUCT()
struct FGolemEncounterPressure
{
	GENERATED_BODY()
	UPROPERTY(EditDefaultsOnly)
	FPrimaryAssetId ArchetypeId;
	UPROPERTY(EditDefaultsOnly)
	FVector Position = FVector::ZeroVector;
};

/** Encounter composition and coordination cost. Human count never changes authored health. */
UCLASS()
class THREEDREAMGOLEMS_API UGolemEncounterQualityDefinition : public UGolemDefinition
{
	GENERATED_BODY()

public:
	UGolemEncounterQualityDefinition();
	FGolemResult ValidateEncounter() const;
	UPROPERTY(EditDefaultsOnly)
	TArray<FGolemEncounterPressure> Wave;
	UPROPERTY(EditDefaultsOnly)
	FPrimaryAssetId BossArchetypeId;
	UPROPERTY(EditDefaultsOnly)
	FPrimaryAssetId BossPhaseId;
	UPROPERTY(EditDefaultsOnly)
	int32 StoryAttackBudget = 1;
	UPROPERTY(EditDefaultsOnly)
	int32 StandardAttackBudget = 2;
};
