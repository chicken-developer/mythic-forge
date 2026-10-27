#pragma once

#include "CoreMinimal.h"
#include "Assets/KnightDefinition.h"
#include "KnightEncounterQualityDefinition.generated.h"

USTRUCT()
struct FKnightEncounterPressure
{
	GENERATED_BODY()
	UPROPERTY(EditDefaultsOnly)
	FPrimaryAssetId ArchetypeId;
	UPROPERTY(EditDefaultsOnly)
	FVector Position = FVector::ZeroVector;
};

/** Encounter composition and coordination cost. Human count never changes authored health. */
UCLASS()
class THREEDREAMINGKNIGHTS_API UKnightEncounterQualityDefinition : public UKnightDefinition
{
	GENERATED_BODY()

public:
	UKnightEncounterQualityDefinition();
	FKnightResult ValidateEncounter() const;
	UPROPERTY(EditDefaultsOnly)
	TArray<FKnightEncounterPressure> Wave;
	UPROPERTY(EditDefaultsOnly)
	FPrimaryAssetId BossArchetypeId;
	UPROPERTY(EditDefaultsOnly)
	FPrimaryAssetId BossPhaseId;
	UPROPERTY(EditDefaultsOnly)
	int32 StoryAttackBudget = 1;
	UPROPERTY(EditDefaultsOnly)
	int32 StandardAttackBudget = 2;
};
