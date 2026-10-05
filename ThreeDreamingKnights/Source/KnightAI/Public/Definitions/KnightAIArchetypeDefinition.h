#pragma once

#include "CoreMinimal.h"
#include "Assets/KnightDefinition.h"
#include "Attributes/KnightAttributeSet.h"
#include "KnightAIArchetypeDefinition.generated.h"

class UEnvQuery;
class UStateTree;

USTRUCT(BlueprintType)
struct KNIGHTAI_API FKnightAIArchetypeTuning
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Knight|AI|Attributes")
	FKnightAttributeDefaults Attributes;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Knight|AI|Perception", meta = (Units = "cm"))
	float SightRadius = 1800.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Knight|AI|Perception", meta = (Units = "cm"))
	float LoseSightRadius = 2200.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Knight|AI|Perception", meta = (Units = "deg"))
	float SightAngle = 100.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Knight|AI|Perception", meta = (Units = "cm"))
	float HearingRange = 2000.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Knight|AI|Perception", meta = (Units = "s"))
	float StimulusMaxAge = 5.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Knight|AI|Movement", meta = (Units = "cm/s"))
	float WalkSpeed = 400.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Knight|AI|Combat", meta = (Units = "s"))
	float AttackLeaseDuration = 4.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Knight|AI|Movement", meta = (Units = "s"))
	float RepathInterval = 0.35f;

	FKnightResult Validate() const;
};

/** Actor composition resolves these stable IDs; the definition does not spawn or mutate a pawn. */
UCLASS(BlueprintType)
class KNIGHTAI_API UKnightAIArchetypeDefinition : public UKnightDefinition
{
	GENERATED_BODY()

public:
	UKnightAIArchetypeDefinition();
	const FKnightAIArchetypeTuning& GetTuning() const;
	FPrimaryAssetId GetCombatProfileId() const;
	FPrimaryAssetId GetActionSetId() const;
	UStateTree* GetStateTree() const;
	UEnvQuery* GetRepositionQuery() const;
	FKnightResult ValidateArchetype() const;
	FKnightResult TrySetTuning(const FKnightAIArchetypeTuning& InTuning);
	FKnightResult TrySetIds(const FPrimaryAssetId& InCombatProfileId, const FPrimaryAssetId& InActionSetId);
	FKnightResult TrySetDecisionAssets(UStateTree* InStateTree, UEnvQuery* InRepositionQuery);
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

private:
	UPROPERTY(EditDefaultsOnly, Category = "Knight|AI|Tuning")
	FKnightAIArchetypeTuning Tuning;
	UPROPERTY(EditDefaultsOnly, Category = "Knight|AI|Definitions")
	FPrimaryAssetId CombatProfileId;
	UPROPERTY(EditDefaultsOnly, Category = "Knight|AI|Definitions")
	FPrimaryAssetId ActionSetId;
	UPROPERTY(EditDefaultsOnly, Category = "Knight|AI|Decision")
	TSoftObjectPtr<UStateTree> StateTree;
	UPROPERTY(EditDefaultsOnly, Category = "Knight|AI|Decision")
	TSoftObjectPtr<UEnvQuery> RepositionQuery;
};
