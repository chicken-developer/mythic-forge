#include "Definitions/GolemAIActionSetDefinition.h"

#include "Misc/DataValidation.h"
#include "Types/GolemStableIds.h"

namespace
{
	constexpr int32 GolemMaxActions = 64;
	constexpr int32 GolemMaxContextEntries = 256;
	constexpr float GolemMaxActionRange = 100000.0f;
	constexpr float GolemMaxContextValue = 1000000.0f;
	constexpr float GolemMaxSelectionCooldown = 3600.0f;

	FGolemResult GolemActionFailure(EGolemError Error, FName Context)
	{
		return FGolemResult::Failure(Error, Context,
			NSLOCTEXT("GolemAI", "InvalidActionSelectionData", "The action selection data is invalid."));
	}
	bool GolemActionValueInRange(float Value, float Maximum)
	{
		return FMath::IsFinite(Value) && Value >= 0.0f && Value <= Maximum;
	}
	bool GolemValidActionTag(const FGameplayTag& Tag)
	{
		const FGameplayTag AbilityRoot = FGameplayTag::RequestGameplayTag(TEXT("Ability"), false);
		return Tag.IsValid() && AbilityRoot.IsValid() && Tag != AbilityRoot && Tag.MatchesTag(AbilityRoot);
	}
	bool GolemValidActionTags(const FGameplayTagContainer& Tags, bool bRequireAbilityTags)
	{
		if (Tags.Num() > GolemMaxContextEntries)
		{
			return false;
		}
		for (const FGameplayTag& Tag : Tags)
		{
			if (!Tag.IsValid() || (bRequireAbilityTags && !GolemValidActionTag(Tag)))
			{
				return false;
			}
		}
		return true;
	}
}

UGolemAIActionSetDefinition::UGolemAIActionSetDefinition()
{
	DefinitionType = TEXT("GolemAIActionSet");
}

const TArray<FGolemAIActionEntry>& UGolemAIActionSetDefinition::GetActions() const
{
	return Actions;
}

FGolemResult UGolemAIActionSetDefinition::ValidateActionSet() const
{
	const FGolemResult Identity = ValidateIdentity();
	if (!Identity.IsSuccess())
	{
		return Identity;
	}
	if (GetDefinitionType() != FName(TEXT("GolemAIActionSet")))
	{
		return FGolemResult::Failure(EGolemError::TypeMismatch, GetDefinitionId(),
			NSLOCTEXT("GolemAI", "ActionSetType", "An AI action set must use the GolemAIActionSet primary asset type."));
	}
	return ValidateActions(Actions);
}

FGolemResult UGolemAIActionSetDefinition::TrySetActions(const TArray<FGolemAIActionEntry>& InActions)
{
	const FGolemResult Result = ValidateActions(InActions);
	if (Result.IsSuccess())
	{
		Actions = InActions;
	}
	return Result;
}

FGolemResult UGolemAIActionSetDefinition::TrySelectAction(const FGolemAIActionSelectionContext& Context,
	FGolemAIActionEntry& OutAction) const
{
	OutAction = FGolemAIActionEntry();
	FGolemResult Result = ValidateActionSet();
	if (!Result.IsSuccess())
	{
		return Result;
	}
	Result = ValidateSelectionContext(Context);
	if (!Result.IsSuccess())
	{
		return Result;
	}
	const FGolemAIActionEntry* Selected = nullptr;
	for (const FGolemAIActionEntry& Action : Actions)
	{
		const float* RemainingCooldown = Context.RemainingCooldowns.Find(Action.ActionKey);
		if (!Context.OwnedAbilityActionTags.HasTagExact(Action.ActionTag) ||
			Context.Distance < Action.MinRange || Context.Distance > Action.MaxRange ||
			
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
		return FGolemResult::Failure(EGolemError::NotFound, GetDefinitionId(),
			NSLOCTEXT("GolemAI", "NoEligibleAction", "No action is available in the current state."));
	}
	OutAction = *Selected;
	return FGolemResult::Success();
}

FGolemResult UGolemAIActionSetDefinition::ValidateActions(const TArray<FGolemAIActionEntry>& InActions)
{
	if (InActions.IsEmpty() || InActions.Num() > GolemMaxActions)
	{
		return GolemActionFailure(EGolemError::InvalidData, TEXT("ActionSetCount"));
	}
	TSet<FName> ActionKeys;
	for (const FGolemAIActionEntry& Action : InActions)
	{
		if (!GolemIdentity::IsValidIdentifier(Action.ActionKey) || !GolemValidActionTag(Action.ActionTag) ||
			!GolemActionValueInRange(Action.MinRange, GolemMaxActionRange) ||
			!GolemActionValueInRange(Action.MaxRange, GolemMaxActionRange) || Action.MinRange > Action.MaxRange ||
			!GolemActionValueInRange(Action.CooldownSeconds, GolemMaxSelectionCooldown) ||
			Action.Priority < -10000 || Action.Priority > 10000 ||
			!GolemValidActionTags(Action.RequiredStateTags, false) || !GolemValidActionTags(Action.BlockedStateTags, false) ||
			Action.RequiredStateTags.HasAny(Action.BlockedStateTags))
		{
			return GolemActionFailure(EGolemError::InvalidData, Action.ActionKey);
		}
		if (ActionKeys.Contains(Action.ActionKey))
		{
			return FGolemResult::Failure(EGolemError::DuplicateId, Action.ActionKey,
				NSLOCTEXT("GolemAI", "DuplicateActionKey", "Action keys must be unique within one AI action set."));
		}
		ActionKeys.Add(Action.ActionKey);
	}
	return FGolemResult::Success();
}

FGolemResult UGolemAIActionSetDefinition::ValidateSelectionContext(const FGolemAIActionSelectionContext& Context)
{
	if (!GolemActionValueInRange(Context.Distance, GolemMaxContextValue) ||
		!GolemValidActionTags(Context.OwnedAbilityActionTags, true) || !GolemValidActionTags(Context.StateTags, false) ||
		Context.RemainingCooldowns.Num() > GolemMaxContextEntries)
	{
		return GolemActionFailure(EGolemError::InvalidData, TEXT("ActionSelectionContext"));
	}
	for (const TPair<FName, float>& Cooldown : Context.RemainingCooldowns)
	{
		if (!GolemIdentity::IsValidIdentifier(Cooldown.Key) ||
			!GolemActionValueInRange(Cooldown.Value, GolemMaxSelectionCooldown))
		{
			return GolemActionFailure(EGolemError::InvalidData, TEXT("ActionSelectionCooldown"));
		}
	}
	return FGolemResult::Success();
}

#if WITH_EDITOR
EDataValidationResult UGolemAIActionSetDefinition::IsDataValid(FDataValidationContext& Context) const
{
	const EDataValidationResult Parent = Super::IsDataValid(Context);
	const FGolemResult Result = ValidateActionSet();
	if (!Result.IsSuccess())
	{
		Context.AddError(Result.Message);
		return EDataValidationResult::Invalid;
	}
	return Parent == EDataValidationResult::Invalid ? Parent : EDataValidationResult::Valid;
}
#endif
