#pragma once

#include "CoreMinimal.h"
#include "Assets/GolemDefinition.h"
#include "Attributes/GolemAttributeSet.h"
#include "Skills/GolemSkillTypes.h"
#include "GolemAIArchetypeDefinition.generated.h"

class UEnvQuery;
class UStateTree;

USTRUCT(BlueprintType)
struct GOLEMAI_API FGolemAIArchetypeTuning
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|AI|Attributes")
	FGolemAttributeDefaults Attributes;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|AI|Perception", meta = (Units = "cm"))
	float SightRadius = 1800.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|AI|Perception", meta = (Units = "cm"))
	float LoseSightRadius = 2200.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|AI|Perception", meta = (Units = "deg"))
	float SightAngle = 100.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|AI|Perception", meta = (Units = "cm"))
	float HearingRange = 2000.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|AI|Perception", meta = (Units = "s"))
	float StimulusMaxAge = 5.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|AI|Movement", meta = (Units = "cm/s"))
	float WalkSpeed = 400.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|AI|Combat", meta = (Units = "s"))
	float AttackLeaseDuration = 4.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|AI|Movement", meta = (Units = "s"))
	float RepathInterval = 0.35f;

	FGolemResult Validate() const;
};

/** Actor composition resolves these stable IDs; the definition does not spawn or mutate a pawn. */
UCLASS(BlueprintType)
class GOLEMAI_API UGolemAIArchetypeDefinition : public UGolemDefinition
{
	GENERATED_BODY()

public:
	/** Optional kit executed by the same StateTree/GAS action used by companions. */
	UPROPERTY(EditDefaultsOnly, Category = "Golem|AI|Skills")
	FGolemSkillBuild Specialization;
	UGolemAIArchetypeDefinition();
	const FGolemAIArchetypeTuning& GetTuning() const;
	FPrimaryAssetId GetCombatProfileId() const;
	FPrimaryAssetId GetActionSetId() const;
	UStateTree* GetStateTree() const;
	UEnvQuery* GetRepositionQuery() const;
	FGolemResult ValidateArchetype() const;
	FGolemResult TrySetTuning(const FGolemAIArchetypeTuning& InTuning);
	FGolemResult TrySetIds(const FPrimaryAssetId& InCombatProfileId, const FPrimaryAssetId& InActionSetId);
	FGolemResult TrySetDecisionAssets(UStateTree* InStateTree, UEnvQuery* InRepositionQuery);
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

private:
	UPROPERTY(EditDefaultsOnly, Category = "Golem|AI|Tuning")
	FGolemAIArchetypeTuning Tuning;
	UPROPERTY(EditDefaultsOnly, Category = "Golem|AI|Definitions")
	FPrimaryAssetId CombatProfileId;
	UPROPERTY(EditDefaultsOnly, Category = "Golem|AI|Definitions")
	FPrimaryAssetId ActionSetId;
	UPROPERTY(EditDefaultsOnly, Category = "Golem|AI|Decision")
	TSoftObjectPtr<UStateTree> StateTree;
	UPROPERTY(EditDefaultsOnly, Category = "Golem|AI|Decision")
	TSoftObjectPtr<UEnvQuery> RepositionQuery;
};
