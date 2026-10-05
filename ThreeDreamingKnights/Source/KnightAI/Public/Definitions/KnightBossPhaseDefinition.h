#pragma once

#include "CoreMinimal.h"
#include "Assets/KnightDefinition.h"
#include "KnightBossPhaseDefinition.generated.h"

USTRUCT(BlueprintType)
struct KNIGHTAI_API FKnightBossPhaseEntry
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Knight|AI|Boss")
	FName PhaseKey;
	/** First phase starts at 1; later phases enter at this descending health fraction. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Knight|AI|Boss")
	float HealthFraction = 1.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Knight|AI|Boss")
	FPrimaryAssetId ActionSetId;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Knight|AI|Boss", meta = (Units = "s"))
	float DecisionInterval = 0.15f;
};

/** Phase swaps select already-granted shared GAS actions and never reinitialize combat identity/resources. */
UCLASS(BlueprintType)
class KNIGHTAI_API UKnightBossPhaseDefinition : public UKnightDefinition
{
	GENERATED_BODY()

public:
	UKnightBossPhaseDefinition();
	const TArray<FKnightBossPhaseEntry>& GetPhases() const;
	FKnightResult ValidateBossPhases() const;
	FKnightResult TrySetPhases(const TArray<FKnightBossPhaseEntry>& InPhases);
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

private:
	static FKnightResult ValidatePhases(const TArray<FKnightBossPhaseEntry>& InPhases);
	UPROPERTY(EditDefaultsOnly, Category = "Knight|AI|Boss")
	TArray<FKnightBossPhaseEntry> Phases;
};
