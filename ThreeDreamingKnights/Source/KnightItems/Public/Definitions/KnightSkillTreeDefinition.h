#pragma once

#include "Assets/KnightDefinition.h"
#include "KnightSkillTreeDefinition.generated.h"

class UKnightSkillDefinition;

UCLASS(BlueprintType)
class KNIGHTITEMS_API UKnightSkillTreeDefinition : public UKnightDefinition
{
	GENERATED_BODY()

public:
	UKnightSkillTreeDefinition();
	const TArray<FPrimaryAssetId>& GetSkills() const;
	FKnightResult TrySetSkills(const TArray<FPrimaryAssetId>& InSkills);
	FKnightResult ValidateSkillTree() const;
	/** Complete typed nodes are required; unknown prerequisites and cycles are rejected. */
	FKnightResult ValidateGraph(TConstArrayView<const UKnightSkillDefinition*> Definitions) const;
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

private:
	UPROPERTY(EditDefaultsOnly, Category = "Knight|RPG|Skills")
	TArray<FPrimaryAssetId> Skills;
};
