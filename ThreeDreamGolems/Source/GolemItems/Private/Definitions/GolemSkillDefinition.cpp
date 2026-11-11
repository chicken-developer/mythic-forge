#include "Definitions/GolemSkillDefinition.h"

#include "Misc/DataValidation.h"

#define LOCTEXT_NAMESPACE "GolemSkillDefinition"

namespace
{
	FGolemResult ValidateSkillTuning(int32 Points, const TArray<FGolemMaterialCost>& Costs,
		const TArray<FPrimaryAssetId>& Prerequisites, const FGolemItemStatBonuses& Bonuses,
		const FGolemRuneTuning& RuneTuning, const FPrimaryAssetId& AbilitySetId)
	{
		if (Points < 1 || Points > GolemRPG::MaximumSkillPoints || Costs.Num() > GolemItems::MaximumBatchOperations
			|| Prerequisites.Num() > GolemRPG::MaximumSkills)
		{
			return FGolemResult::Failure(EGolemError::InvalidData, TEXT("SkillTuning"),
				LOCTEXT("Bounds", "Skill points, material costs and prerequisites must respect their finite bounds."));
		}
		TSet<FPrimaryAssetId> Ids;
		for (const FGolemMaterialCost& Cost : Costs)
		{
			if (!GolemIdentity::IsValidDefinitionId(Cost.DefinitionId)
				|| Cost.DefinitionId.PrimaryAssetType != FPrimaryAssetType(TEXT("GolemItem"))
				|| Cost.Count < 1 || Cost.Count > GolemItems::MaximumStackCount || Ids.Contains(Cost.DefinitionId))
			{
				return FGolemResult::Failure(EGolemError::InvalidData, TEXT("SkillCosts"),
					LOCTEXT("Costs", "Each skill material cost requires one unique stable item identity and a bounded count."));
			}
			Ids.Add(Cost.DefinitionId);
		}
		Ids.Reset();
		for (const FPrimaryAssetId& Prerequisite : Prerequisites)
		{
			if (!GolemIdentity::IsValidDefinitionId(Prerequisite)
				|| Prerequisite.PrimaryAssetType != FPrimaryAssetType(TEXT("GolemSkill")) || Ids.Contains(Prerequisite))
			{
				return FGolemResult::Failure(EGolemError::InvalidId, TEXT("SkillPrerequisite"),
					LOCTEXT("Prerequisites", "Skill prerequisites require distinct stable GolemSkill identities."));
			}
			Ids.Add(Prerequisite);
		}
		if (AbilitySetId != FPrimaryAssetId() && (!GolemIdentity::IsValidDefinitionId(AbilitySetId)
			|| AbilitySetId.PrimaryAssetType != FPrimaryAssetType(TEXT("GolemAbilitySet"))))
		{
			return FGolemResult::Failure(EGolemError::InvalidId, TEXT("SkillAbilitySet"),
				LOCTEXT("AbilitySet", "An optional skill grant must identify an actual GolemAbilitySet."));
		}
		const FGolemResult Stats = Bonuses.Validate();
		return Stats.IsSuccess() ? RuneTuning.Validate() : Stats;
	}
}

UGolemSkillDefinition::UGolemSkillDefinition()
{
	DefinitionType = TEXT("GolemSkill");
}

int32 UGolemSkillDefinition::GetPointCost() const
{
	return PointCost;
}

const TArray<FGolemMaterialCost>& UGolemSkillDefinition::GetMaterialCosts() const
{
	return MaterialCosts;
}

const TArray<FPrimaryAssetId>& UGolemSkillDefinition::GetPrerequisites() const
{
	return Prerequisites;
}

const FGolemItemStatBonuses& UGolemSkillDefinition::GetStatBonuses() const
{
	return StatBonuses;
}

const FGolemRuneTuning& UGolemSkillDefinition::GetRuneTuning() const
{
	return RuneTuning;
}

FPrimaryAssetId UGolemSkillDefinition::GetAbilitySetId() const
{
	return AbilitySetId;
}

FGolemResult UGolemSkillDefinition::TrySetSkillTuning(int32 InPointCost, const TArray<FGolemMaterialCost>& InCosts,
	const TArray<FPrimaryAssetId>& InPrerequisites, const FGolemItemStatBonuses& InBonuses,
	const FGolemRuneTuning& InRuneTuning, FPrimaryAssetId InAbilitySetId)
{
	const FGolemResult Result = ValidateSkillTuning(InPointCost, InCosts, InPrerequisites,
		InBonuses, InRuneTuning, InAbilitySetId);
	if (Result.IsSuccess())
	{
		PointCost = InPointCost;
		MaterialCosts = InCosts;
		Prerequisites = InPrerequisites;
		StatBonuses = InBonuses;
		RuneTuning = InRuneTuning;
		AbilitySetId = InAbilitySetId;
	}
	return Result;
}

FGolemResult UGolemSkillDefinition::ValidateSkill() const
{
	const FGolemResult Identity = ValidateIdentity();
	if (!Identity.IsSuccess())
	{
		return Identity;
	}
	if (GetDefinitionType() != TEXT("GolemSkill") || Prerequisites.Contains(GetPrimaryAssetId()))
	{
		return FGolemResult::Failure(EGolemError::InvalidData, GetDefinitionId(),
			LOCTEXT("Identity", "A GolemSkill definition cannot require itself."));
	}
	return ValidateSkillTuning(PointCost, MaterialCosts, Prerequisites, StatBonuses, RuneTuning, AbilitySetId);
}

#if WITH_EDITOR
EDataValidationResult UGolemSkillDefinition::IsDataValid(FDataValidationContext& Context) const
{
	const EDataValidationResult Parent = Super::IsDataValid(Context);
	const FGolemResult Result = ValidateSkill();
	if (!Result.IsSuccess())
	{
		Context.AddError(Result.Message);
		return EDataValidationResult::Invalid;
	}
	return Parent == EDataValidationResult::Invalid ? Parent : EDataValidationResult::Valid;
}
#endif

#undef LOCTEXT_NAMESPACE
