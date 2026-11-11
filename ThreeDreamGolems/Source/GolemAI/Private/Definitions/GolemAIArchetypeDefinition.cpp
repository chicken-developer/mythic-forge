#include "Definitions/GolemAIArchetypeDefinition.h"

#include "Components/StateTreeAIComponentSchema.h"
#include "Context/GolemAIResult.h"
#include "EnvironmentQuery/EnvQuery.h"
#include "StateTree.h"
#include "Misc/DataValidation.h"
#include "Types/GolemStableIds.h"

FGolemResult FGolemAIArchetypeTuning::Validate() const
{
	const FGolemResult AttributeResult = Attributes.Validate();
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
		return GolemAIFailure(EGolemError::InvalidData, TEXT("AIArchetypeTuning"));
	}
	return FGolemResult::Success();
}

UGolemAIArchetypeDefinition::UGolemAIArchetypeDefinition()
{
	DefinitionType = TEXT("GolemAIArchetype");
}

const FGolemAIArchetypeTuning& UGolemAIArchetypeDefinition::GetTuning() const
{
	return Tuning;
}

FPrimaryAssetId UGolemAIArchetypeDefinition::GetCombatProfileId() const
{
	return CombatProfileId;
}

FPrimaryAssetId UGolemAIArchetypeDefinition::GetActionSetId() const
{
	return ActionSetId;
}

UStateTree* UGolemAIArchetypeDefinition::GetStateTree() const
{
	return StateTree.LoadSynchronous();
}

UEnvQuery* UGolemAIArchetypeDefinition::GetRepositionQuery() const
{
	return RepositionQuery.LoadSynchronous();
}

FGolemResult UGolemAIArchetypeDefinition::ValidateArchetype() const
{
	if (!Specialization.SpecId.IsNone() && !FGolemSkillCatalog::ValidateDescriptor(Specialization).IsSuccess())
	{
		return GolemAIFailure(EGolemError::InvalidData, TEXT("AIArchetypeSkills"));
	}
	const FGolemResult Identity = ValidateIdentity();
	if (!Identity.IsSuccess())
	{
		return Identity;
	}
	if (GetDefinitionType() != FName(TEXT("GolemAIArchetype")) || !GolemIdentity::IsValidDefinitionId(CombatProfileId) ||
		CombatProfileId.PrimaryAssetType != FPrimaryAssetType(TEXT("GolemCombatProfile")) ||
		!GolemIdentity::IsValidDefinitionId(ActionSetId) ||
		ActionSetId.PrimaryAssetType != FPrimaryAssetType(TEXT("GolemAIActionSet")))
	{
		return GolemAIFailure(EGolemError::InvalidId, TEXT("AIArchetypeDefinitions"));
	}
	const FGolemResult TuningResult = Tuning.Validate();
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
		return GolemAIFailure(EGolemError::InvalidData, TEXT("AIArchetypeDecisionAssets"));
	}
	return FGolemResult::Success();
}

FGolemResult UGolemAIArchetypeDefinition::TrySetTuning(const FGolemAIArchetypeTuning& InTuning)
{
	const FGolemResult Result = InTuning.Validate();
	if (Result.IsSuccess())
	{
		Tuning = InTuning;
	}
	return Result;
}

FGolemResult UGolemAIArchetypeDefinition::TrySetIds(const FPrimaryAssetId& InCombatProfileId,
	const FPrimaryAssetId& InActionSetId)
{
	if (!GolemIdentity::IsValidDefinitionId(InCombatProfileId) ||
		InCombatProfileId.PrimaryAssetType != FPrimaryAssetType(TEXT("GolemCombatProfile")) ||
		!GolemIdentity::IsValidDefinitionId(InActionSetId) ||
		InActionSetId.PrimaryAssetType != FPrimaryAssetType(TEXT("GolemAIActionSet")))
	{
		return GolemAIFailure(EGolemError::InvalidId, TEXT("SetAIArchetypeDefinitions"));
	}
	CombatProfileId = InCombatProfileId;
	ActionSetId = InActionSetId;
	return FGolemResult::Success();
}

FGolemResult UGolemAIArchetypeDefinition::TrySetDecisionAssets(UStateTree* InStateTree,
	UEnvQuery* InRepositionQuery)
{
	if (!IsValid(InStateTree) || !InStateTree->IsReadyToRun() ||
		Cast<UStateTreeAIComponentSchema>(InStateTree->GetSchema()) == nullptr || !IsValid(InRepositionQuery) ||
		InRepositionQuery->GetOptions().IsEmpty())
	{
		return GolemAIFailure(EGolemError::InvalidData, TEXT("SetAIArchetypeDecisionAssets"));
	}
	StateTree = InStateTree;
	RepositionQuery = InRepositionQuery;
	return FGolemResult::Success();
}

#if WITH_EDITOR
EDataValidationResult UGolemAIArchetypeDefinition::IsDataValid(FDataValidationContext& Context) const
{
	const FGolemResult Result = ValidateArchetype();
	if (!Result.IsSuccess())
	{
		Context.AddError(Result.Message);
		return EDataValidationResult::Invalid;
	}
	return Super::IsDataValid(Context) == EDataValidationResult::Invalid
		? EDataValidationResult::Invalid : EDataValidationResult::Valid;
}
#endif
