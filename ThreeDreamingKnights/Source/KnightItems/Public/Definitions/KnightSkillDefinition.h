#pragma once

#include "Assets/KnightDefinition.h"
#include "Models/KnightRPGModels.h"
#include "KnightSkillDefinition.generated.h"

UCLASS(BlueprintType)
class KNIGHTITEMS_API UKnightSkillDefinition : public UKnightDefinition
{
	GENERATED_BODY()

public:
	UKnightSkillDefinition();
	int32 GetPointCost() const;
	const TArray<FKnightMaterialCost>& GetMaterialCosts() const;
	const TArray<FPrimaryAssetId>& GetPrerequisites() const;
	const FKnightItemStatBonuses& GetStatBonuses() const;
	const FKnightRuneTuning& GetRuneTuning() const;
	FPrimaryAssetId GetAbilitySetId() const;
	FKnightResult TrySetSkillTuning(int32 InPointCost, const TArray<FKnightMaterialCost>& InCosts,
		const TArray<FPrimaryAssetId>& InPrerequisites, const FKnightItemStatBonuses& InBonuses,
		const FKnightRuneTuning& InRuneTuning = FKnightRuneTuning(), FPrimaryAssetId InAbilitySetId = FPrimaryAssetId());
	FKnightResult ValidateSkill() const;
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

private:
	UPROPERTY(EditDefaultsOnly, Category = "Knight|RPG|Skills")
	int32 PointCost = 1;
	UPROPERTY(EditDefaultsOnly, Category = "Knight|RPG|Skills")
	TArray<FKnightMaterialCost> MaterialCosts;
	UPROPERTY(EditDefaultsOnly, Category = "Knight|RPG|Skills")
	TArray<FPrimaryAssetId> Prerequisites;
	UPROPERTY(EditDefaultsOnly, Category = "Knight|RPG|Skills")
	FKnightItemStatBonuses StatBonuses;
	UPROPERTY(EditDefaultsOnly, Category = "Knight|RPG|Skills")
	FKnightRuneTuning RuneTuning;
	UPROPERTY(EditDefaultsOnly, Category = "Knight|RPG|Skills")
	FPrimaryAssetId AbilitySetId;
};
