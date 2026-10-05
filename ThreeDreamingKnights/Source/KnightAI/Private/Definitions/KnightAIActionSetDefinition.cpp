#include "Definitions/KnightAIActionSetDefinition.h"

#include "Misc/DataValidation.h"
#include "Types/KnightStableIds.h"

namespace
{
	constexpr int32 KnightMaxActions = 64;
	constexpr int32 KnightMaxContextEntries = 256;
	constexpr float KnightMaxActionRange = 100000.0f;
	constexpr float KnightMaxContextValue = 1000000.0f;
	constexpr float KnightMaxSelectionCooldown = 3600.0f;

	FKnightResult KnightActionFailure(EKnightError Error, FName Context)
	{
		return FKnightResult::Failure(Error, Context,
			NSLOCTEXT("KnightAI", "InvalidActionSelectionData", "The action selection data is invalid."));
	}
	bool KnightActionValueInRange(float Value, float Maximum)
	{
		return FMath::IsFinite(Value) && Value >= 0.0f && Value <= Maximum;
	}
	bool KnightValidActionTag(const FGameplayTag& Tag)
	{
		const FGameplayTag AbilityRoot = FGameplayTag::RequestGameplayTag(TEXT("Ability"), false);
		return Tag.IsValid() && AbilityRoot.IsValid() && Tag != AbilityRoot && Tag.MatchesTag(AbilityRoot);
	}
	bool KnightValidActionTags(const FGameplayTagContainer& Tags, bool bRequireAbilityTags)
	{
		if (Tags.Num() > KnightMaxContextEntries)
		{
			return false;
		}
		for (const FGameplayTag& Tag : Tags)
		{
			if (!Tag.IsValid() || (bRequireAbilityTags && !KnightValidActionTag(Tag)))
			{
				return false;
			}
		}
		return true;
	}
}

UKnightAIActionSetDefinition::UKnightAIActionSetDefinition()
{
	DefinitionType = TEXT("KnightAIActionSet");
}

const TArray<FKnightAIActionEntry>& UKnightAIActionSetDefinition::GetActions() const
{
	return Actions;
}

FKnightResult UKnightAIActionSetDefinition::ValidateActionSet() const
{
	const FKnightResult Identity = ValidateIdentity();
	if (!Identity.IsSuccess())
	{
		return Identity;
	}
	if (GetDefinitionType() != FName(TEXT("KnightAIActionSet")))
	{
		return FKnightResult::Failure(EKnightError::TypeMismatch, GetDefinitionId(),
			NSLOCTEXT("KnightAI", "ActionSetType", "An AI action set must use the KnightAIActionSet primary asset type."));
	}
	return ValidateActions(Actions);
}

FKnightResult UKnightAIActionSetDefinition::TrySetActions(const TArray<FKnightAIActionEntry>& InActions)
{
	const FKnightResult Result = ValidateActions(InActions);
	if (Result.IsSuccess())
	{
		Actions = InActions;
	}
	return Result;
}

FKnightResult UKnightAIActionSetDefinition::TrySelectAction(const FKnightAIActionSelectionContext& Context,
	FKnightAIActionEntry& OutAction) const
{
	OutAction = FKnightAIActionEntry();
	FKnightResult Result = ValidateActionSet();
	if (!Result.IsSuccess())
	{
		return Result;
	}
	Result = ValidateSelectionContext(Context);
	if (!Result.IsSuccess())
	{
		return Result;
	}
	const FKnightAIActionEntry* Selected = nullptr;
	for (const FKnightAIActionEntry& Action : Actions)
	{
		const float* RemainingCooldown = Context.RemainingCooldowns.Find(Action.ActionKey);
		if (!Context.OwnedAbilityActionTags.HasTagExact(Action.ActionTag) ||
			Context.Distance < Action.MinRange || Context.Distance > Action.MaxRange ||
			Context.Stamina < Action.RequiredStamina || Context.RuneEnergy < Action.RequiredRuneEnergy ||
			!Context.StateTags.HasAll(Action.RequiredStateTags) || Context.StateTags.HasAny(Action.BlockedStateTags) ||
			(RemainingCooldown != nullptr && *RemainingCooldown > 0.0f))
		{
			continue;
		}
		if (Selected == nullptr || Action.Priority > Selected->Priority ||
			(Action.Priority == Selected->Priority && Action.ActionKey.LexicalLess(Selected->ActionKey)))
		{
			Selected = &Action;
		}
	}
	if (Selected == nullptr)
	{
		return FKnightResult::Failure(EKnightError::NotFound, GetDefinitionId(),
			NSLOCTEXT("KnightAI", "NoEligibleAction", "No action is available in the current state."));
	}
	OutAction = *Selected;
	return FKnightResult::Success();
}

FKnightResult UKnightAIActionSetDefinition::ValidateActions(const TArray<FKnightAIActionEntry>& InActions)
{
	if (InActions.IsEmpty() || InActions.Num() > KnightMaxActions)
	{
		return KnightActionFailure(EKnightError::InvalidData, TEXT("ActionSetCount"));
	}
	TSet<FName> ActionKeys;
	for (const FKnightAIActionEntry& Action : InActions)
	{
		if (!KnightIdentity::IsValidIdentifier(Action.ActionKey) || !KnightValidActionTag(Action.ActionTag) ||
			!KnightActionValueInRange(Action.MinRange, KnightMaxActionRange) ||
			!KnightActionValueInRange(Action.MaxRange, KnightMaxActionRange) || Action.MinRange > Action.MaxRange ||
			!KnightActionValueInRange(Action.RequiredStamina, KnightMaxContextValue) ||
			!KnightActionValueInRange(Action.RequiredRuneEnergy, KnightMaxContextValue) ||
			!KnightActionValueInRange(Action.CooldownSeconds, KnightMaxSelectionCooldown) ||
			Action.Priority < -10000 || Action.Priority > 10000 ||
			!KnightValidActionTags(Action.RequiredStateTags, false) || !KnightValidActionTags(Action.BlockedStateTags, false) ||
			Action.RequiredStateTags.HasAny(Action.BlockedStateTags))
		{
			return KnightActionFailure(EKnightError::InvalidData, Action.ActionKey);
		}
		if (ActionKeys.Contains(Action.ActionKey))
		{
			return FKnightResult::Failure(EKnightError::DuplicateId, Action.ActionKey,
				NSLOCTEXT("KnightAI", "DuplicateActionKey", "Action keys must be unique within one AI action set."));
		}
		ActionKeys.Add(Action.ActionKey);
	}
	return FKnightResult::Success();
}

FKnightResult UKnightAIActionSetDefinition::ValidateSelectionContext(const FKnightAIActionSelectionContext& Context)
{
	if (!KnightActionValueInRange(Context.Distance, KnightMaxContextValue) ||
		!KnightActionValueInRange(Context.Stamina, KnightMaxContextValue) ||
		!KnightActionValueInRange(Context.RuneEnergy, KnightMaxContextValue) ||
		!KnightValidActionTags(Context.OwnedAbilityActionTags, true) || !KnightValidActionTags(Context.StateTags, false) ||
		Context.RemainingCooldowns.Num() > KnightMaxContextEntries)
	{
		return KnightActionFailure(EKnightError::InvalidData, TEXT("ActionSelectionContext"));
	}
	for (const TPair<FName, float>& Cooldown : Context.RemainingCooldowns)
	{
		if (!KnightIdentity::IsValidIdentifier(Cooldown.Key) ||
			!KnightActionValueInRange(Cooldown.Value, KnightMaxSelectionCooldown))
		{
			return KnightActionFailure(EKnightError::InvalidData, TEXT("ActionSelectionCooldown"));
		}
	}
	return FKnightResult::Success();
}

#if WITH_EDITOR
EDataValidationResult UKnightAIActionSetDefinition::IsDataValid(FDataValidationContext& Context) const
{
	const EDataValidationResult Parent = Super::IsDataValid(Context);
	const FKnightResult Result = ValidateActionSet();
	if (!Result.IsSuccess())
	{
		Context.AddError(Result.Message);
		return EDataValidationResult::Invalid;
	}
	return Parent == EDataValidationResult::Invalid ? Parent : EDataValidationResult::Valid;
}
#endif
