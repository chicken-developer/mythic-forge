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
	return RuneTuning.Validate();
}

#undef LOCTEXT_NAMESPACE
