#include "Definitions/KnightAbilitySetDefinition.h"

#include "Abilities/KnightGameplayAbility.h"
#include "GameplayEffect.h"
#include "Misc/DataValidation.h"

bool FKnightAbilitySetHandle::IsValid() const
{
	return GrantId.IsValid() && !SourceKey.IsNone() && AbilitySetId.IsValid();
}

UKnightAbilitySetDefinition::UKnightAbilitySetDefinition()
{
	DefinitionType = TEXT("KnightAbilitySet");
}

const TArray<FKnightAbilitySetEntry>& UKnightAbilitySetDefinition::GetAbilities() const
{
	return Abilities;
}

const TArray<TSubclassOf<UGameplayEffect>>& UKnightAbilitySetDefinition::GetEffects() const
{
	return Effects;
}

const TArray<FKnightTagRelationship>& UKnightAbilitySetDefinition::GetRelationships() const
{
	return Relationships;
}

FKnightResult UKnightAbilitySetDefinition::ValidateAbilitySet() const
{
	const FKnightResult Identity = ValidateIdentity();
	if (!Identity.IsSuccess())
	{
		return Identity;
	}
	if (GetDefinitionType() != FName(TEXT("KnightAbilitySet")))
	{
		return FKnightResult::Failure(EKnightError::TypeMismatch, GetDefinitionId(),
			NSLOCTEXT("KnightGAS", "AbilitySetType", "An ability set must use the KnightAbilitySet primary asset type."));
	}
	const FKnightResult AbilityValidation = ValidateAbilities(Abilities);
	if (!AbilityValidation.IsSuccess())
	{
		return AbilityValidation;
	}
	const FKnightResult EffectValidation = ValidateEffects(Effects);
	return EffectValidation.IsSuccess()
		? UKnightTagRelationshipMapping::ValidateRelationships(Relationships) : EffectValidation;
}

FKnightResult UKnightAbilitySetDefinition::TrySetAbilities(const TArray<FKnightAbilitySetEntry>& InAbilities)
{
	const FKnightResult Validation = ValidateAbilities(InAbilities);
	if (Validation.IsSuccess())
	{
		Abilities = InAbilities;
	}
	return Validation;
}

FKnightResult UKnightAbilitySetDefinition::TrySetEffects(const TArray<TSubclassOf<UGameplayEffect>>& InEffects)
{
	const FKnightResult Validation = ValidateEffects(InEffects);
	if (Validation.IsSuccess())
	{
		Effects = InEffects;
	}
	return Validation;
}

FKnightResult UKnightAbilitySetDefinition::TrySetRelationships(const TArray<FKnightTagRelationship>& InRelationships)
{
	const FKnightResult Validation = UKnightTagRelationshipMapping::ValidateRelationships(InRelationships);
	if (Validation.IsSuccess())
	{
		Relationships = InRelationships;
	}
	return Validation;
}

FKnightResult UKnightAbilitySetDefinition::ValidateAbilities(const TArray<FKnightAbilitySetEntry>& InAbilities)
{
	if (InAbilities.IsEmpty())
	{
		return FKnightResult::Failure(EKnightError::InvalidData, TEXT("AbilitySet"),
			NSLOCTEXT("KnightGAS", "EmptyAbilitySet", "An ability set requires at least one valid ability."));
	}
	TSet<UClass*> Classes;
	TSet<FGameplayTag> Inputs;
	for (const FKnightAbilitySetEntry& Entry : InAbilities)
	{
		if (Entry.Ability == nullptr || Entry.Ability->HasAnyClassFlags(CLASS_Abstract)
			|| !Entry.InputTag.IsValid() || Entry.Level < 1 || Entry.Level > 100)
		{
			return FKnightResult::Failure(EKnightError::InvalidData, TEXT("AbilitySet"),
				NSLOCTEXT("KnightGAS", "InvalidAbilityGrant", "Abilities require a concrete class, input tag and level from 1 through 100."));
		}
		if (Classes.Contains(Entry.Ability.Get()) || Inputs.Contains(Entry.InputTag))
		{
			return FKnightResult::Failure(EKnightError::DuplicateId, TEXT("AbilitySet"),
				NSLOCTEXT("KnightGAS", "DuplicateAbilityGrant", "Ability classes and input tags must be unique within one set."));
		}
		Classes.Add(Entry.Ability.Get());
		Inputs.Add(Entry.InputTag);
	}
	return FKnightResult::Success();
}

FKnightResult UKnightAbilitySetDefinition::ValidateEffects(const TArray<TSubclassOf<UGameplayEffect>>& InEffects)
{
	TSet<UClass*> Seen;
	for (TSubclassOf<UGameplayEffect> Effect : InEffects)
	{
		if (Effect == nullptr || Effect->HasAnyClassFlags(CLASS_Abstract)
			|| Effect->GetDefaultObject<UGameplayEffect>()->DurationPolicy == EGameplayEffectDurationType::Instant)
		{
			return FKnightResult::Failure(EKnightError::InvalidData, TEXT("AbilitySet"),
				NSLOCTEXT("KnightGAS", "NonReversibleGrantEffect", "Grant effects must be persistent so their source can revoke them."));
		}
		if (Seen.Contains(Effect.Get()))
		{
			return FKnightResult::Failure(EKnightError::DuplicateId, TEXT("AbilitySet"),
				NSLOCTEXT("KnightGAS", "DuplicateGrantEffect", "One set cannot grant the same persistent effect twice."));
		}
		Seen.Add(Effect.Get());
	}
	return FKnightResult::Success();
}

#if WITH_EDITOR
EDataValidationResult UKnightAbilitySetDefinition::IsDataValid(FDataValidationContext& Context) const
{
	const EDataValidationResult Parent = Super::IsDataValid(Context);
	const FKnightResult Validation = ValidateAbilitySet();
	if (!Validation.IsSuccess())
	{
		Context.AddError(Validation.Message);
		return EDataValidationResult::Invalid;
	}
	return Parent == EDataValidationResult::Invalid ? Parent : EDataValidationResult::Valid;
}
#endif
