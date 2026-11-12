#pragma once

#include "CoreMinimal.h"
#include "Assets/GolemDefinition.h"
#include "Attributes/GolemAttributeSet.h"
#include "GolemAIArchetypeDefinition.generated.h"

class UEnvQuery;
class UStateTree;

/** How a companion helps with colour (DEV_SPEC section 12). Starting values; tune them in the archetype asset. */
USTRUCT(BlueprintType)
struct GOLEMAI_API FGolemColorAssistTuning
{
	GENERATED_BODY()

	/** A signalled void that needs this Golem's colour gets a heavy attack after a delay picked in this range. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|AI|Colour", meta = (ClampMin = "0.0", Units = "s"))
	float SignalDelayMin = 0.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|AI|Colour", meta = (ClampMin = "0.0", Units = "s"))
	float SignalDelayMax = 1.0f;

	/** A Primed void still missing this Golem's colour gets one after this long, without being asked. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|AI|Colour", meta = (ClampMin = "0.0", Units = "s"))
	float FollowUpDelay = 1.5f;

	/** Voids farther than this from the companion are left to the players. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|AI|Colour", meta = (ClampMin = "100.0", Units = "cm"))
	float MaxRange = 3000.0f;

	/** Gap between the surface of the void and a melee companion when it swings. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|AI|Colour", meta = (ClampMin = "10.0", Units = "cm"))
	float MeleeStandDistance = 60.0f;

	/** Where a ranged companion stands, measured out from the void, when it has no clear shot. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|AI|Colour", meta = (ClampMin = "100.0", Units = "cm"))
	float RangedStandDistance = 650.0f;

	/** A companion gives up on one void after this long and returns to ordinary fighting. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|AI|Colour", meta = (ClampMin = "1.0", Units = "s"))
	float GiveUpSeconds = 6.0f;

	FGolemResult Validate() const;
};

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
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|AI|Colour")
	FGolemColorAssistTuning ColorAssist;

	FGolemResult Validate() const;
};

/** Actor composition resolves these stable IDs; the definition does not spawn or mutate a pawn. */
UCLASS(BlueprintType)
class GOLEMAI_API UGolemAIArchetypeDefinition : public UGolemDefinition
{
	GENERATED_BODY()

public:
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
