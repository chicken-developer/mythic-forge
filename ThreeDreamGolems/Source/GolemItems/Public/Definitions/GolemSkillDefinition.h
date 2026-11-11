#pragma once

#include "Assets/GolemDefinition.h"
#include "Models/GolemRPGModels.h"
#include "GolemSkillDefinition.generated.h"

UCLASS(BlueprintType)
class GOLEMITEMS_API UGolemSkillDefinition : public UGolemDefinition
{
	GENERATED_BODY()

public:
	UGolemSkillDefinition();
	int32 GetPointCost() const;
	const TArray<FGolemMaterialCost>& GetMaterialCosts() const;
	const TArray<FPrimaryAssetId>& GetPrerequisites() const;
	const FGolemItemStatBonuses& GetStatBonuses() const;
	const FGolemRuneTuning& GetRuneTuning() const;
	FPrimaryAssetId GetAbilitySetId() const;
	FGolemResult TrySetSkillTuning(int32 InPointCost, const TArray<FGolemMaterialCost>& InCosts,
		const TArray<FPrimaryAssetId>& InPrerequisites, const FGolemItemStatBonuses& InBonuses,
		const FGolemRuneTuning& InRuneTuning = FGolemRuneTuning(), FPrimaryAssetId InAbilitySetId = FPrimaryAssetId());
	FGolemResult ValidateSkill() const;
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

private:
	UPROPERTY(EditDefaultsOnly, Category = "Golem|RPG|Skills")
	int32 PointCost = 1;
	UPROPERTY(EditDefaultsOnly, Category = "Golem|RPG|Skills")
	TArray<FGolemMaterialCost> MaterialCosts;
	UPROPERTY(EditDefaultsOnly, Category = "Golem|RPG|Skills")
	TArray<FPrimaryAssetId> Prerequisites;
	UPROPERTY(EditDefaultsOnly, Category = "Golem|RPG|Skills")
	FGolemItemStatBonuses StatBonuses;
	UPROPERTY(EditDefaultsOnly, Category = "Golem|RPG|Skills")
	FGolemRuneTuning RuneTuning;
	UPROPERTY(EditDefaultsOnly, Category = "Golem|RPG|Skills")
	FPrimaryAssetId AbilitySetId;
};
