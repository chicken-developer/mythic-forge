#include "Definitions/KnightSkillDefinition.h"

#include "Misc/DataValidation.h"

#define LOCTEXT_NAMESPACE "KnightSkillDefinition"

namespace
{
	FKnightResult ValidateSkillTuning(int32 Points, const TArray<FKnightMaterialCost>& Costs,
		const TArray<FPrimaryAssetId>& Prerequisites, const FKnightItemStatBonuses& Bonuses,
		const FKnightRuneTuning& RuneTuning, const FPrimaryAssetId& AbilitySetId)
	{
		if (Points < 1 || Points > KnightRPG::MaximumSkillPoints || Costs.Num() > KnightItems::MaximumBatchOperations
			|| Prerequisites.Num() > KnightRPG::MaximumSkills)
		{
			return FKnightResult::Failure(EKnightError::InvalidData, TEXT("SkillTuning"),
				LOCTEXT("Bounds", "Skill points, material costs and prerequisites must respect their finite bounds."));
		}
		TSet<FPrimaryAssetId> Ids;
		for (const FKnightMaterialCost& Cost : Costs)
		{
			if (!KnightIdentity::IsValidDefinitionId(Cost.DefinitionId)
				|| Cost.DefinitionId.PrimaryAssetType != FPrimaryAssetType(TEXT("KnightItem"))
				|| Cost.Count < 1 || Cost.Count > KnightItems::MaximumStackCount || Ids.Contains(Cost.DefinitionId))
			{
				return FKnightResult::Failure(EKnightError::InvalidData, TEXT("SkillCosts"),
					LOCTEXT("Costs", "Each skill material cost requires one unique stable item identity and a bounded count."));
			}
			Ids.Add(Cost.DefinitionId);
		}
		Ids.Reset();
		for (const FPrimaryAssetId& Prerequisite : Prerequisites)
		{
			if (!KnightIdentity::IsValidDefinitionId(Prerequisite)
				|| Prerequisite.PrimaryAssetType != FPrimaryAssetType(TEXT("KnightSkill")) || Ids.Contains(Prerequisite))
			{
				return FKnightResult::Failure(EKnightError::InvalidId, TEXT("SkillPrerequisite"),
					LOCTEXT("Prerequisites", "Skill prerequisites require distinct stable KnightSkill identities."));
			}
			Ids.Add(Prerequisite);
		}
		if (AbilitySetId != FPrimaryAssetId() && (!KnightIdentity::IsValidDefinitionId(AbilitySetId)
			|| AbilitySetId.PrimaryAssetType != FPrimaryAssetType(TEXT("KnightAbilitySet"))))
		{
			return FKnightResult::Failure(EKnightError::InvalidId, TEXT("SkillAbilitySet"),
				LOCTEXT("AbilitySet", "An optional skill grant must identify an actual KnightAbilitySet."));
		}
		const FKnightResult Stats = Bonuses.Validate();
		return Stats.IsSuccess() ? RuneTuning.Validate() : Stats;
	}
}

UKnightSkillDefinition::UKnightSkillDefinition()
{
	DefinitionType = TEXT("KnightSkill");
}

int32 UKnightSkillDefinition::GetPointCost() const
{
	return PointCost;
}

const TArray<FKnightMaterialCost>& UKnightSkillDefinition::GetMaterialCosts() const
{
	return MaterialCosts;
}

const TArray<FPrimaryAssetId>& UKnightSkillDefinition::GetPrerequisites() const
{
	return Prerequisites;
}

const FKnightItemStatBonuses& UKnightSkillDefinition::GetStatBonuses() const
{
	return StatBonuses;
}

const FKnightRuneTuning& UKnightSkillDefinition::GetRuneTuning() const
{
	return RuneTuning;
}

FPrimaryAssetId UKnightSkillDefinition::GetAbilitySetId() const
{
	return AbilitySetId;
}

FKnightResult UKnightSkillDefinition::TrySetSkillTuning(int32 InPointCost, const TArray<FKnightMaterialCost>& InCosts,
	const TArray<FPrimaryAssetId>& InPrerequisites, const FKnightItemStatBonuses& InBonuses,
	const FKnightRuneTuning& InRuneTuning, FPrimaryAssetId InAbilitySetId)
{
	const FKnightResult Result = ValidateSkillTuning(InPointCost, InCosts, InPrerequisites,
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

FKnightResult UKnightSkillDefinition::ValidateSkill() const
{
	const FKnightResult Identity = ValidateIdentity();
	if (!Identity.IsSuccess())
	{
		return Identity;
	}
	if (GetDefinitionType() != TEXT("KnightSkill") || Prerequisites.Contains(GetPrimaryAssetId()))
	{
		return FKnightResult::Failure(EKnightError::InvalidData, GetDefinitionId(),
			LOCTEXT("Identity", "A KnightSkill definition cannot require itself."));
	}
	return ValidateSkillTuning(PointCost, MaterialCosts, Prerequisites, StatBonuses, RuneTuning, AbilitySetId);
}

#if WITH_EDITOR
EDataValidationResult UKnightSkillDefinition::IsDataValid(FDataValidationContext& Context) const
{
	const EDataValidationResult Parent = Super::IsDataValid(Context);
	const FKnightResult Result = ValidateSkill();
	if (!Result.IsSuccess())
	{
		Context.AddError(Result.Message);
		return EDataValidationResult::Invalid;
	}
	return Parent == EDataValidationResult::Invalid ? Parent : EDataValidationResult::Valid;
}
#endif

#undef LOCTEXT_NAMESPACE
