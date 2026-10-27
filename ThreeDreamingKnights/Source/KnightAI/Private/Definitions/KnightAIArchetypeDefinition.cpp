#include "Definitions/KnightAIArchetypeDefinition.h"

#include "Components/StateTreeAIComponentSchema.h"
#include "Context/KnightAIResult.h"
#include "EnvironmentQuery/EnvQuery.h"
#include "StateTree.h"
#include "Misc/DataValidation.h"
#include "Types/KnightStableIds.h"

FKnightResult FKnightAIArchetypeTuning::Validate() const
{
	const FKnightResult AttributeResult = Attributes.Validate();
	if (!AttributeResult.IsSuccess())
	{
		return AttributeResult;
	}
	if (!FMath::IsFinite(SightRadius) || SightRadius <= 0.0f || SightRadius > 100000.0f ||
		!FMath::IsFinite(LoseSightRadius) || LoseSightRadius < SightRadius || LoseSightRadius > 100000.0f ||
		!FMath::IsFinite(SightAngle) || SightAngle < 1.0f || SightAngle > 180.0f ||
		!FMath::IsFinite(HearingRange) || HearingRange < 0.0f || HearingRange > 100000.0f ||
		!FMath::IsFinite(StimulusMaxAge) || StimulusMaxAge <= 0.0f || StimulusMaxAge > 3600.0f ||
		!FMath::IsFinite(WalkSpeed) || WalkSpeed <= 0.0f || WalkSpeed > 3000.0f ||
		!FMath::IsFinite(AttackLeaseDuration) || AttackLeaseDuration < 0.1f || AttackLeaseDuration > 60.0f ||
		!FMath::IsFinite(RepathInterval) || RepathInterval < 0.05f || RepathInterval > 10.0f)
	{
		return KnightAIFailure(EKnightError::InvalidData, TEXT("AIArchetypeTuning"));
	}
	return FKnightResult::Success();
}

UKnightAIArchetypeDefinition::UKnightAIArchetypeDefinition()
{
	DefinitionType = TEXT("KnightAIArchetype");
}

const FKnightAIArchetypeTuning& UKnightAIArchetypeDefinition::GetTuning() const
{
	return Tuning;
}

FPrimaryAssetId UKnightAIArchetypeDefinition::GetCombatProfileId() const
{
	return CombatProfileId;
}

FPrimaryAssetId UKnightAIArchetypeDefinition::GetActionSetId() const
{
	return ActionSetId;
}

UStateTree* UKnightAIArchetypeDefinition::GetStateTree() const
{
	return StateTree.LoadSynchronous();
}

UEnvQuery* UKnightAIArchetypeDefinition::GetRepositionQuery() const
{
	return RepositionQuery.LoadSynchronous();
}

FKnightResult UKnightAIArchetypeDefinition::ValidateArchetype() const
{
	if (!Specialization.SpecId.IsNone() && !FKnightSkillCatalog::ValidateDescriptor(Specialization).IsSuccess())
	{
		return KnightAIFailure(EKnightError::InvalidData, TEXT("AIArchetypeSkills"));
	}
	const FKnightResult Identity = ValidateIdentity();
	if (!Identity.IsSuccess())
	{
		return Identity;
	}
	if (GetDefinitionType() != FName(TEXT("KnightAIArchetype")) || !KnightIdentity::IsValidDefinitionId(CombatProfileId) ||
		CombatProfileId.PrimaryAssetType != FPrimaryAssetType(TEXT("KnightCombatProfile")) ||
		!KnightIdentity::IsValidDefinitionId(ActionSetId) ||
		ActionSetId.PrimaryAssetType != FPrimaryAssetType(TEXT("KnightAIActionSet")))
	{
		return KnightAIFailure(EKnightError::InvalidId, TEXT("AIArchetypeDefinitions"));
	}
	const FKnightResult TuningResult = Tuning.Validate();
	if (!TuningResult.IsSuccess())
	{
		return TuningResult;
	}
	UStateTree* Tree = GetStateTree();
	UEnvQuery* Query = GetRepositionQuery();
	if (Tree == nullptr || !Tree->IsReadyToRun() ||
		Cast<UStateTreeAIComponentSchema>(Tree->GetSchema()) == nullptr ||
		Query == nullptr || Query->GetOptions().IsEmpty())
	{
		return KnightAIFailure(EKnightError::InvalidData, TEXT("AIArchetypeDecisionAssets"));
	}
	return FKnightResult::Success();
}

FKnightResult UKnightAIArchetypeDefinition::TrySetTuning(const FKnightAIArchetypeTuning& InTuning)
{
	const FKnightResult Result = InTuning.Validate();
	if (Result.IsSuccess())
	{
		Tuning = InTuning;
	}
	return Result;
}

FKnightResult UKnightAIArchetypeDefinition::TrySetIds(const FPrimaryAssetId& InCombatProfileId,
	const FPrimaryAssetId& InActionSetId)
{
	if (!KnightIdentity::IsValidDefinitionId(InCombatProfileId) ||
		InCombatProfileId.PrimaryAssetType != FPrimaryAssetType(TEXT("KnightCombatProfile")) ||
		!KnightIdentity::IsValidDefinitionId(InActionSetId) ||
		InActionSetId.PrimaryAssetType != FPrimaryAssetType(TEXT("KnightAIActionSet")))
	{
		return KnightAIFailure(EKnightError::InvalidId, TEXT("SetAIArchetypeDefinitions"));
	}
	CombatProfileId = InCombatProfileId;
	ActionSetId = InActionSetId;
	return FKnightResult::Success();
}

FKnightResult UKnightAIArchetypeDefinition::TrySetDecisionAssets(UStateTree* InStateTree,
	UEnvQuery* InRepositionQuery)
{
	if (!IsValid(InStateTree) || !InStateTree->IsReadyToRun() ||
		Cast<UStateTreeAIComponentSchema>(InStateTree->GetSchema()) == nullptr || !IsValid(InRepositionQuery) ||
		InRepositionQuery->GetOptions().IsEmpty())
	{
		return KnightAIFailure(EKnightError::InvalidData, TEXT("SetAIArchetypeDecisionAssets"));
	}
	StateTree = InStateTree;
	RepositionQuery = InRepositionQuery;
	return FKnightResult::Success();
}

#if WITH_EDITOR
EDataValidationResult UKnightAIArchetypeDefinition::IsDataValid(FDataValidationContext& Context) const
{
	const FKnightResult Result = ValidateArchetype();
	if (!Result.IsSuccess())
	{
		Context.AddError(Result.Message);
		return EDataValidationResult::Invalid;
	}
	return Super::IsDataValid(Context) == EDataValidationResult::Invalid
		? EDataValidationResult::Invalid : EDataValidationResult::Valid;
}
#endif
