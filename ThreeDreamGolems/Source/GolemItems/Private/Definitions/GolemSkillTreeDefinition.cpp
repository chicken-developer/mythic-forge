#include "Definitions/GolemSkillTreeDefinition.h"

#include "Definitions/GolemSkillDefinition.h"
#include "Misc/DataValidation.h"
#include "Types/GolemStableIds.h"

#define LOCTEXT_NAMESPACE "GolemSkillTreeDefinition"

namespace
{
	FGolemResult ValidateSkillIds(const TArray<FPrimaryAssetId>& Ids)
	{
		if (Ids.IsEmpty() || Ids.Num() > GolemRPG::MaximumSkills)
		{
			return FGolemResult::Failure(EGolemError::InvalidData, TEXT("SkillTree"),
				LOCTEXT("Count", "A skill tree requires a bounded nonempty set of skills."));
		}
		TSet<FPrimaryAssetId> Unique;
		for (const FPrimaryAssetId& Id : Ids)
		{
			if (!GolemIdentity::IsValidDefinitionId(Id) || Id.PrimaryAssetType != FPrimaryAssetType(TEXT("GolemSkill"))
				|| Unique.Contains(Id))
			{
				return FGolemResult::Failure(EGolemError::InvalidId, TEXT("SkillTree"),
					LOCTEXT("Ids", "Skill tree nodes require distinct stable GolemSkill identities."));
			}
			Unique.Add(Id);
		}
		return FGolemResult::Success();
	}
}

UGolemSkillTreeDefinition::UGolemSkillTreeDefinition()
{
	DefinitionType = TEXT("GolemSkillTree");
}

const TArray<FPrimaryAssetId>& UGolemSkillTreeDefinition::GetSkills() const
{
	return Skills;
}

FGolemResult UGolemSkillTreeDefinition::TrySetSkills(const TArray<FPrimaryAssetId>& InSkills)
{
	const FGolemResult Result = ValidateSkillIds(InSkills);
	if (Result.IsSuccess())
	{
		Skills = InSkills;
	}
	return Result;
}

FGolemResult UGolemSkillTreeDefinition::ValidateSkillTree() const
{
	const FGolemResult Identity = ValidateIdentity();
	if (!Identity.IsSuccess())
	{
		return Identity;
	}
	if (GetDefinitionType() != TEXT("GolemSkillTree"))
	{
		return FGolemResult::Failure(EGolemError::TypeMismatch, GetDefinitionId(),
			LOCTEXT("Type", "A skill tree requires the GolemSkillTree definition type."));
	}
	return ValidateSkillIds(Skills);
}

FGolemResult UGolemSkillTreeDefinition::ValidateGraph(TConstArrayView<const UGolemSkillDefinition*> Definitions) const
{
	const FGolemResult Validation = ValidateSkillTree();
	if (!Validation.IsSuccess())
	{
		return Validation;
	}
	TMap<FPrimaryAssetId, const UGolemSkillDefinition*> Nodes;
	for (const UGolemSkillDefinition* Definition : Definitions)
	{
		if (!IsValid(Definition) || !Definition->ValidateSkill().IsSuccess()
			|| !Skills.Contains(Definition->GetPrimaryAssetId()) || Nodes.Contains(Definition->GetPrimaryAssetId()))
		{
			return FGolemResult::Failure(EGolemError::InvalidData, GetDefinitionId(),
				LOCTEXT("Nodes", "The skill graph requires exactly one actual validated definition for every declared node."));
		}
		Nodes.Add(Definition->GetPrimaryAssetId(), Definition);
	}
	if (Nodes.Num() != Skills.Num())
	{
		return FGolemResult::Failure(EGolemError::NotFound, GetDefinitionId(),
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
		const UGolemSkillDefinition* const* Definition = Nodes.Find(Id);
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
			return FGolemResult::Failure(EGolemError::InvalidData, GetDefinitionId(),
				LOCTEXT("Cycle", "The skill graph contains an unknown prerequisite or a dependency cycle."));
		}
	}
	return FGolemResult::Success();
}

#if WITH_EDITOR
EDataValidationResult UGolemSkillTreeDefinition::IsDataValid(FDataValidationContext& Context) const
{
	const EDataValidationResult Parent = Super::IsDataValid(Context);
	const FGolemResult Result = ValidateSkillTree();
	if (!Result.IsSuccess())
	{
		Context.AddError(Result.Message);
		return EDataValidationResult::Invalid;
	}
	return Parent == EDataValidationResult::Invalid ? Parent : EDataValidationResult::Valid;
}
#endif

#undef LOCTEXT_NAMESPACE
