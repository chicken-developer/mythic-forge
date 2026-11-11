#include "Definitions/GolemBossPhaseDefinition.h"

#include "Context/GolemAIResult.h"
#include "Misc/DataValidation.h"
#include "Types/GolemStableIds.h"

UGolemBossPhaseDefinition::UGolemBossPhaseDefinition()
{
	DefinitionType = TEXT("KnightBossPhase");
}

const TArray<FGolemBossPhaseEntry>& UGolemBossPhaseDefinition::GetPhases() const
{
	return Phases;
}

FGolemResult UGolemBossPhaseDefinition::ValidatePhases(const TArray<FGolemBossPhaseEntry>& InPhases)
{
	if (InPhases.Num() < 2 || InPhases.Num() > 16)
	{
		return GolemAIFailure(EGolemError::InvalidData, TEXT("BossPhaseCount"));
	}
	TSet<FName> Keys;
	float PreviousFraction = 2.0f;
	for (int32 Index = 0; Index < InPhases.Num(); ++Index)
	{
		const FGolemBossPhaseEntry& Entry = InPhases[Index];
		if (!GolemIdentity::IsValidIdentifier(Entry.PhaseKey) || Keys.Contains(Entry.PhaseKey) ||
			!FMath::IsFinite(Entry.HealthFraction) || Entry.HealthFraction <= 0.0f ||
			Entry.HealthFraction > 1.0f || Entry.HealthFraction >= PreviousFraction ||
			(Index == 0 && Entry.HealthFraction != 1.0f) || !GolemIdentity::IsValidDefinitionId(Entry.ActionSetId) ||
			Entry.ActionSetId.PrimaryAssetType != FPrimaryAssetType(TEXT("KnightAIActionSet")) ||
			!FMath::IsFinite(Entry.DecisionInterval) || Entry.DecisionInterval < 0.05f || Entry.DecisionInterval > 2.0f)
		{
			return GolemAIFailure(EGolemError::InvalidData, TEXT("BossPhaseEntry"));
		}
		Keys.Add(Entry.PhaseKey);
		PreviousFraction = Entry.HealthFraction;
	}
	return FGolemResult::Success();
}

FGolemResult UGolemBossPhaseDefinition::ValidateBossPhases() const
{
	const FGolemResult Identity = ValidateIdentity();
	if (!Identity.IsSuccess())
	{
		return Identity;
	}
	if (GetDefinitionType() != FName(TEXT("KnightBossPhase")))
	{
		return GolemAIFailure(EGolemError::TypeMismatch, TEXT("BossPhaseType"));
	}
	return ValidatePhases(Phases);
}

FGolemResult UGolemBossPhaseDefinition::TrySetPhases(const TArray<FGolemBossPhaseEntry>& InPhases)
{
	const FGolemResult Result = ValidatePhases(InPhases);
	if (Result.IsSuccess())
	{
		Phases = InPhases;
	}
	return Result;
}

#if WITH_EDITOR
EDataValidationResult UGolemBossPhaseDefinition::IsDataValid(FDataValidationContext& Context) const
{
	const FGolemResult Result = ValidateBossPhases();
	if (!Result.IsSuccess())
	{
		Context.AddError(Result.Message);
		return EDataValidationResult::Invalid;
	}
	return Super::IsDataValid(Context) == EDataValidationResult::Invalid
		? EDataValidationResult::Invalid : EDataValidationResult::Valid;
}
#endif
