#include "Definitions/KnightSkillTreeDefinition.h"

#include "Definitions/KnightSkillDefinition.h"
#include "Misc/DataValidation.h"
#include "Types/KnightStableIds.h"

#define LOCTEXT_NAMESPACE "KnightSkillTreeDefinition"

namespace
{
	FKnightResult ValidateSkillIds(const TArray<FPrimaryAssetId>& Ids)
	{
		if (Ids.IsEmpty() || Ids.Num() > KnightRPG::MaximumSkills)
		{
			return FKnightResult::Failure(EKnightError::InvalidData, TEXT("SkillTree"),
				LOCTEXT("Count", "A skill tree requires a bounded nonempty set of skills."));
		}
		TSet<FPrimaryAssetId> Unique;
		for (const FPrimaryAssetId& Id : Ids)
		{
			if (!KnightIdentity::IsValidDefinitionId(Id) || Id.PrimaryAssetType != FPrimaryAssetType(TEXT("KnightSkill"))
				|| Unique.Contains(Id))
			{
				return FKnightResult::Failure(EKnightError::InvalidId, TEXT("SkillTree"),
					LOCTEXT("Ids", "Skill tree nodes require distinct stable KnightSkill identities."));
			}
			Unique.Add(Id);
		}
		return FKnightResult::Success();
	}
}

UKnightSkillTreeDefinition::UKnightSkillTreeDefinition()
{
	DefinitionType = TEXT("KnightSkillTree");
}

const TArray<FPrimaryAssetId>& UKnightSkillTreeDefinition::GetSkills() const
{
	return Skills;
}

FKnightResult UKnightSkillTreeDefinition::TrySetSkills(const TArray<FPrimaryAssetId>& InSkills)
{
	const FKnightResult Result = ValidateSkillIds(InSkills);
	if (Result.IsSuccess())
	{
		Skills = InSkills;
	}
	return Result;
}

FKnightResult UKnightSkillTreeDefinition::ValidateSkillTree() const
{
	const FKnightResult Identity = ValidateIdentity();
	if (!Identity.IsSuccess())
	{
		return Identity;
	}
	if (GetDefinitionType() != TEXT("KnightSkillTree"))
	{
		return FKnightResult::Failure(EKnightError::TypeMismatch, GetDefinitionId(),
			LOCTEXT("Type", "A skill tree requires the KnightSkillTree definition type."));
	}
	return ValidateSkillIds(Skills);
}

FKnightResult UKnightSkillTreeDefinition::ValidateGraph(TConstArrayView<const UKnightSkillDefinition*> Definitions) const
{
	const FKnightResult Validation = ValidateSkillTree();
	if (!Validation.IsSuccess())
	{
		return Validation;
	}
	TMap<FPrimaryAssetId, const UKnightSkillDefinition*> Nodes;
	for (const UKnightSkillDefinition* Definition : Definitions)
	{
		if (!IsValid(Definition) || !Definition->ValidateSkill().IsSuccess()
			|| !Skills.Contains(Definition->GetPrimaryAssetId()) || Nodes.Contains(Definition->GetPrimaryAssetId()))
		{
			return FKnightResult::Failure(EKnightError::InvalidData, GetDefinitionId(),
				LOCTEXT("Nodes", "The skill graph requires exactly one actual validated definition for every declared node."));
		}
		Nodes.Add(Definition->GetPrimaryAssetId(), Definition);
	}
	if (Nodes.Num() != Skills.Num())
	{
		return FKnightResult::Failure(EKnightError::NotFound, GetDefinitionId(),
			LOCTEXT("MissingNode", "A declared skill node has no actual typed definition."));
	}
	TMap<FPrimaryAssetId, uint8> Colors;
	TFunction<bool(const FPrimaryAssetId&)> Visit = [&](const FPrimaryAssetId& Id)
	{
		const uint8 Color = Colors.FindRef(Id);
		if (Color == 1)
		{
			return false;
		}
		if (Color == 2)
		{
			return true;
		}
		const UKnightSkillDefinition* const* Definition = Nodes.Find(Id);
		if (Definition == nullptr)
		{
			return false;
		}
		Colors.Add(Id, 1);
		for (const FPrimaryAssetId& Required : (*Definition)->GetPrerequisites())
		{
			if (!Visit(Required))
			{
				return false;
			}
		}
		Colors.Add(Id, 2);
		return true;
	};
	for (const FPrimaryAssetId& Id : Skills)
	{
		if (!Visit(Id))
		{
			return FKnightResult::Failure(EKnightError::InvalidData, GetDefinitionId(),
				LOCTEXT("Cycle", "The skill graph contains an unknown prerequisite or a dependency cycle."));
		}
	}
	return FKnightResult::Success();
}

#if WITH_EDITOR
EDataValidationResult UKnightSkillTreeDefinition::IsDataValid(FDataValidationContext& Context) const
{
	const EDataValidationResult Parent = Super::IsDataValid(Context);
	const FKnightResult Result = ValidateSkillTree();
	if (!Result.IsSuccess())
	{
		Context.AddError(Result.Message);
		return EDataValidationResult::Invalid;
	}
	return Parent == EDataValidationResult::Invalid ? Parent : EDataValidationResult::Valid;
}
#endif

#undef LOCTEXT_NAMESPACE
