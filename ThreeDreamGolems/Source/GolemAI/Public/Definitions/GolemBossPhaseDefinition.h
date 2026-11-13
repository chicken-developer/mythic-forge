#pragma once

#include "CoreMinimal.h"
#include "Assets/GolemDefinition.h"
#include "GolemBossPhaseDefinition.generated.h"

USTRUCT(BlueprintType)
struct GOLEMAI_API FGolemBossPhaseEntry
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|AI|Boss")
	FName PhaseKey;
	/** First phase starts at 1; later phases enter at this descending health fraction. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|AI|Boss")
	float HealthFraction = 1.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|AI|Boss")
	FPrimaryAssetId ActionSetId;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|AI|Boss", meta = (Units = "s"))
	float DecisionInterval = 0.15f;
};

/** Phase swaps select already-granted shared GAS actions and never reinitialize combat identity/resources. */
UCLASS(BlueprintType)
class GOLEMAI_API UGolemBossPhaseDefinition : public UGolemDefinition
{
	GENERATED_BODY()

public:
	UGolemBossPhaseDefinition();
	const TArray<FGolemBossPhaseEntry>& GetPhases() const;
	FGolemResult ValidateBossPhases() const;
	FGolemResult TrySetPhases(const TArray<FGolemBossPhaseEntry>& InPhases);
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

private:
	static FGolemResult ValidatePhases(const TArray<FGolemBossPhaseEntry>& InPhases);
	UPROPERTY(EditDefaultsOnly, Category = "Golem|AI|Boss")
	TArray<FGolemBossPhaseEntry> Phases;
};
