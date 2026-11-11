#include "Definitions/GolemAbilitySetDefinition.h"

#include "Abilities/GolemGameplayAbility.h"
#include "GameplayEffect.h"
#include "Misc/DataValidation.h"

bool FGolemAbilitySetHandle::IsValid() const
{
	return GrantId.IsValid() && !SourceKey.IsNone() && AbilitySetId.IsValid();
}

UGolemAbilitySetDefinition::UGolemAbilitySetDefinition()
{
	DefinitionType = TEXT("KnightAbilitySet");
}

const TArray<FGolemAbilitySetEntry>& UGolemAbilitySetDefinition::GetAbilities() const
{
	return Abilities;
}

const TArray<TSubclassOf<UGameplayEffect>>& UGolemAbilitySetDefinition::GetEffects() const
{
	return Effects;
}

const TArray<FGolemTagRelationship>& UGolemAbilitySetDefinition::GetRelationships() const
{
	return Relationships;
}

FGolemResult UGolemAbilitySetDefinition::ValidateAbilitySet() const
{
	const FGolemResult Identity = ValidateIdentity();
	if (!Identity.IsSuccess())
	{
		return Identity;
	}
	if (GetDefinitionType() != FName(TEXT("KnightAbilitySet")))
	{
		return FGolemResult::Failure(EGolemError::TypeMismatch, GetDefinitionId(),
			NSLOCTEXT("GolemGAS", "AbilitySetType", "An ability set must use the GolemAbilitySet primary asset type."));
	}
	const FGolemResult AbilityValidation = ValidateAbilities(Abilities);
	if (!AbilityValidation.IsSuccess())
	{
		return AbilityValidation;
	}
	const FGolemResult EffectValidation = ValidateEffects(Effects);
	return EffectValidation.IsSuccess()
		? UGolemTagRelationshipMapping::ValidateRelationships(Relationships) : EffectValidation;
}

FGolemResult UGolemAbilitySetDefinition::TrySetAbilities(const TArray<FGolemAbilitySetEntry>& InAbilities)
{
	const FGolemResult Validation = ValidateAbilities(InAbilities);
	if (Validation.IsSuccess())
	{
		Abilities = InAbilities;
	}
	return Validation;
}

FGolemResult UGolemAbilitySetDefinition::TrySetEffects(const TArray<TSubclassOf<UGameplayEffect>>& InEffects)
{
	const FGolemResult Validation = ValidateEffects(InEffects);
	if (Validation.IsSuccess())
	{
		Effects = InEffects;
	}
	return Validation;
}

FGolemResult UGolemAbilitySetDefinition::TrySetRelationships(const TArray<FGolemTagRelationship>& InRelationships)
{
	const FGolemResult Validation = UGolemTagRelationshipMapping::ValidateRelationships(InRelationships);
	if (Validation.IsSuccess())
	{
		Relationships = InRelationships;
	}
	return Validation;
}

FGolemResult UGolemAbilitySetDefinition::ValidateAbilities(const TArray<FGolemAbilitySetEntry>& InAbilities)
{
	if (InAbilities.IsEmpty())
	{
		return FGolemResult::Failure(EGolemError::InvalidData, TEXT("AbilitySet"),
			NSLOCTEXT("GolemGAS", "EmptyAbilitySet", "An ability set requires at least one valid ability."));
	}
	TSet<UClass*> Classes;
	TSet<FGameplayTag> Inputs;
	for (const FGolemAbilitySetEntry& Entry : InAbilities)
	{
		if (Entry.Ability == nullptr || Entry.Ability->HasAnyClassFlags(CLASS_Abstract)
			|| !Entry.InputTag.IsValid() || Entry.Level < 1 || Entry.Level > 100)
		{
			return FGolemResult::Failure(EGolemError::InvalidData, TEXT("AbilitySet"),
				NSLOCTEXT("GolemGAS", "InvalidAbilityGrant", "Abilities require a concrete class, input tag and level from 1 through 100."));
		}
		if (Classes.Contains(Entry.Ability.Get()) || Inputs.Contains(Entry.InputTag))
		{
			return FGolemResult::Failure(EGolemError::DuplicateId, TEXT("AbilitySet"),
				NSLOCTEXT("GolemGAS", "DuplicateAbilityGrant", "Ability classes and input tags must be unique within one set."));
		}
		Classes.Add(Entry.Ability.Get());
		Inputs.Add(Entry.InputTag);
	}
	return FGolemResult::Success();
}

FGolemResult UGolemAbilitySetDefinition::ValidateEffects(const TArray<TSubclassOf<UGameplayEffect>>& InEffects)
{
	TSet<UClass*> Seen;
	for (TSubclassOf<UGameplayEffect> Effect : InEffects)
	{
		if (Effect == nullptr || Effect->HasAnyClassFlags(CLASS_Abstract)
			|| Effect->GetDefaultObject<UGameplayEffect>()->DurationPolicy == EGameplayEffectDurationType::Instant)
		{
			return FGolemResult::Failure(EGolemError::InvalidData, TEXT("AbilitySet"),
				NSLOCTEXT("GolemGAS", "NonReversibleGrantEffect", "Grant effects must be persistent so their source can revoke them."));
		}
		if (Seen.Contains(Effect.Get()))
		{
			return FGolemResult::Failure(EGolemError::DuplicateId, TEXT("AbilitySet"),
				NSLOCTEXT("GolemGAS", "DuplicateGrantEffect", "One set cannot grant the same persistent effect twice."));
		}
		Seen.Add(Effect.Get());
	}
	return FGolemResult::Success();
}

#if WITH_EDITOR
EDataValidationResult UGolemAbilitySetDefinition::IsDataValid(FDataValidationContext& Context) const
{
	const EDataValidationResult Parent = Super::IsDataValid(Context);
	const FGolemResult Validation = ValidateAbilitySet();
	if (!Validation.IsSuccess())
	{
		Context.AddError(Validation.Message);
		return EDataValidationResult::Invalid;
	}
	return Parent == EDataValidationResult::Invalid ? Parent : EDataValidationResult::Valid;
}
#endif
