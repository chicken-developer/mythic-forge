#include "Definitions/GolemRuneDefinition.h"

#define LOCTEXT_NAMESPACE "KnightRuneDefinition"

UGolemRuneDefinition::UGolemRuneDefinition()
{
	TrySetItemTuning(EGolemItemKind::Rune, 1);
}

const FGolemRuneTuning& UGolemRuneDefinition::GetRuneTuning() const
{
	return RuneTuning;
}

FGolemResult UGolemRuneDefinition::TrySetRuneTuning(const FGolemRuneTuning& InTuning)
{
	const FGolemResult Result = InTuning.Validate();
	if (Result.IsSuccess())
	{
		RuneTuning = InTuning;
	}
	return Result;
}

FGolemResult UGolemRuneDefinition::ValidateItem() const
{
	const FGolemResult Parent = Super::ValidateItem();
	if (!Parent.IsSuccess())
	{
		return Parent;
	}
	if (GetItemKind() != EGolemItemKind::Rune)
	{
		return FGolemResult::Failure(EGolemError::TypeMismatch, GetDefinitionId(),
			LOCTEXT("RuneKind", "A rune definition must retain its unstackable Rune item kind."));
	}
	if (!AttachmentRuneId.IsNone() && (!(RuneTuning == FGolemRuneTuning())
		|| !GetStatBonuses().IsEmpty() || GetAbilitySetId().IsValid()
		|| (AttachmentRuneId != TEXT("Weapon.Aftershock") && AttachmentRuneId != TEXT("Skill.Convergence")
			&& AttachmentRuneId != TEXT("Skill.Propagation") && AttachmentRuneId != TEXT("Skill.Mobile"))))
	{
		return FGolemResult::Failure(EGolemError::InvalidData, GetDefinitionId(),
			LOCTEXT("AttachmentIdentity", "Attachment runes require an actual seed modifier and identity legacy tuning."));
	}
	return RuneTuning.Validate();
}

FName UGolemRuneDefinition::GetAttachmentRuneId() const
{
	return AttachmentRuneId;
}

FGolemResult UGolemRuneDefinition::TrySetAttachmentRune(FName InRuneId)
{
	const FName PreviousId = AttachmentRuneId;
	AttachmentRuneId = InRuneId;
	const FGolemResult Result = ValidateItem();
	if (!Result.IsSuccess())
	{
		AttachmentRuneId = PreviousId;
	}
	return Result;
}

#undef LOCTEXT_NAMESPACE
