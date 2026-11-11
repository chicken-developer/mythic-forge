#pragma once

#include "Assets/GolemDefinition.h"
#include "GolemSkillTreeDefinition.generated.h"

class UGolemSkillDefinition;

UCLASS(BlueprintType)
class GOLEMITEMS_API UGolemSkillTreeDefinition : public UGolemDefinition
{
	GENERATED_BODY()

public:
	UGolemSkillTreeDefinition();
	const TArray<FPrimaryAssetId>& GetSkills() const;
	FGolemResult TrySetSkills(const TArray<FPrimaryAssetId>& InSkills);
	FGolemResult ValidateSkillTree() const;
	/** Complete typed nodes are required; unknown prerequisites and cycles are rejected. */
	FGolemResult ValidateGraph(TConstArrayView<const UGolemSkillDefinition*> Definitions) const;
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

private:
	UPROPERTY(EditDefaultsOnly, Category = "Golem|RPG|Skills")
	TArray<FPrimaryAssetId> Skills;
};
