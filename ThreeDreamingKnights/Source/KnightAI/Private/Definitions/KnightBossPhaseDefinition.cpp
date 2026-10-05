#include "Definitions/KnightBossPhaseDefinition.h"

#include "Context/KnightAIResult.h"
#include "Misc/DataValidation.h"
#include "Types/KnightStableIds.h"

UKnightBossPhaseDefinition::UKnightBossPhaseDefinition()
{
	DefinitionType = TEXT("KnightBossPhase");
}

const TArray<FKnightBossPhaseEntry>& UKnightBossPhaseDefinition::GetPhases() const
{
	return Phases;
}

FKnightResult UKnightBossPhaseDefinition::ValidatePhases(const TArray<FKnightBossPhaseEntry>& InPhases)
{
	if (InPhases.Num() < 2 || InPhases.Num() > 16)
	{
		return KnightAIFailure(EKnightError::InvalidData, TEXT("BossPhaseCount"));
	}
	TSet<FName> Keys;
	float PreviousFraction = 2.0f;
	for (int32 Index = 0; Index < InPhases.Num(); ++Index)
	{
		const FKnightBossPhaseEntry& Entry = InPhases[Index];
		if (!KnightIdentity::IsValidIdentifier(Entry.PhaseKey) || Keys.Contains(Entry.PhaseKey) ||
			!FMath::IsFinite(Entry.HealthFraction) || Entry.HealthFraction <= 0.0f ||
			Entry.HealthFraction > 1.0f || Entry.HealthFraction >= PreviousFraction ||
			(Index == 0 && Entry.HealthFraction != 1.0f) || !KnightIdentity::IsValidDefinitionId(Entry.ActionSetId) ||
			Entry.ActionSetId.PrimaryAssetType != FPrimaryAssetType(TEXT("KnightAIActionSet")) ||
			!FMath::IsFinite(Entry.DecisionInterval) || Entry.DecisionInterval < 0.05f || Entry.DecisionInterval > 2.0f)
		{
			return KnightAIFailure(EKnightError::InvalidData, TEXT("BossPhaseEntry"));
		}
		Keys.Add(Entry.PhaseKey);
		PreviousFraction = Entry.HealthFraction;
	}
	return FKnightResult::Success();
}

FKnightResult UKnightBossPhaseDefinition::ValidateBossPhases() const
{
	const FKnightResult Identity = ValidateIdentity();
	if (!Identity.IsSuccess())
	{
		return Identity;
	}
	if (GetDefinitionType() != FName(TEXT("KnightBossPhase")))
	{
		return KnightAIFailure(EKnightError::TypeMismatch, TEXT("BossPhaseType"));
	}
	return ValidatePhases(Phases);
}

FKnightResult UKnightBossPhaseDefinition::TrySetPhases(const TArray<FKnightBossPhaseEntry>& InPhases)
{
	const FKnightResult Result = ValidatePhases(InPhases);
	if (Result.IsSuccess())
	{
		Phases = InPhases;
	}
	return Result;
}

#if WITH_EDITOR
EDataValidationResult UKnightBossPhaseDefinition::IsDataValid(FDataValidationContext& Context) const
{
	const FKnightResult Result = ValidateBossPhases();
	if (!Result.IsSuccess())
	{
		Context.AddError(Result.Message);
		return EDataValidationResult::Invalid;
	}
	return Super::IsDataValid(Context) == EDataValidationResult::Invalid
		? EDataValidationResult::Invalid : EDataValidationResult::Valid;
}
#endif
