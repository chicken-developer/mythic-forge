#include "Definitions/KnightRuneDefinition.h"

#define LOCTEXT_NAMESPACE "KnightRuneDefinition"

UKnightRuneDefinition::UKnightRuneDefinition()
{
	TrySetItemTuning(EKnightItemKind::Rune, 1);
}

const FKnightRuneTuning& UKnightRuneDefinition::GetRuneTuning() const
{
	return RuneTuning;
}

FKnightResult UKnightRuneDefinition::TrySetRuneTuning(const FKnightRuneTuning& InTuning)
{
	const FKnightResult Result = InTuning.Validate();
	if (Result.IsSuccess())
	{
		RuneTuning = InTuning;
	}
	return Result;
}

FKnightResult UKnightRuneDefinition::ValidateItem() const
{
	const FKnightResult Parent = Super::ValidateItem();
	if (!Parent.IsSuccess())
	{
		return Parent;
	}
	if (GetItemKind() != EKnightItemKind::Rune)
	{
		return FKnightResult::Failure(EKnightError::TypeMismatch, GetDefinitionId(),
			LOCTEXT("RuneKind", "A rune definition must retain its unstackable Rune item kind."));
	}
	if (!AttachmentRuneId.IsNone() && (!(RuneTuning == FKnightRuneTuning())
		|| !GetStatBonuses().IsEmpty() || GetAbilitySetId().IsValid()
		|| (AttachmentRuneId != TEXT("Weapon.Aftershock") && AttachmentRuneId != TEXT("Skill.Convergence")
			&& AttachmentRuneId != TEXT("Skill.Propagation") && AttachmentRuneId != TEXT("Skill.Mobile"))))
	{
		return FKnightResult::Failure(EKnightError::InvalidData, GetDefinitionId(),
			LOCTEXT("AttachmentIdentity", "Attachment runes require an actual seed modifier and identity legacy tuning."));
	}
	return RuneTuning.Validate();
}

FName UKnightRuneDefinition::GetAttachmentRuneId() const
{
	return AttachmentRuneId;
}

FKnightResult UKnightRuneDefinition::TrySetAttachmentRune(FName InRuneId)
{
	const FName PreviousId = AttachmentRuneId;
	AttachmentRuneId = InRuneId;
	const FKnightResult Result = ValidateItem();
	if (!Result.IsSuccess())
	{
		AttachmentRuneId = PreviousId;
	}
	return Result;
}

#undef LOCTEXT_NAMESPACE
