#include "Projection/GolemSourceProjection.h"

#define LOCTEXT_NAMESPACE "GolemSourceProjection"

const FName GolemProjection::RPGSourceKey(TEXT("RPG"));

FGolemResult FGolemActionModifier::Validate() const
{
	const float Values[] { DamageScale, CostScale, DurationScale };
	if (!AbilityTag.IsValid())
	{
		return FGolemResult::Failure(EGolemError::InvalidId, TEXT("ProjectionAction"),
			LOCTEXT("InvalidAction", "A projection modifier requires a valid ability action tag."));
	}
	for (float Value : Values)
	{
		if (!FMath::IsFinite(Value) || Value < 0.1f || Value > 10.0f)
		{
			return FGolemResult::Failure(EGolemError::InvalidData, TEXT("ProjectionAction"),
				LOCTEXT("InvalidMultiplier", "Action multipliers must be finite and between 0.1 and 10."));
		}
	}
	return FGolemResult::Success();
}

bool FGolemActionModifier::operator==(const FGolemActionModifier& Other) const
{
	return AbilityTag == Other.AbilityTag && DamageScale == Other.DamageScale
		&& CostScale == Other.CostScale && DurationScale == Other.DurationScale;
}

FGolemResult FGolemGASProjectionPlan::Validate() const
{
	if (!FGolemSkillCatalog::ValidateDescriptor(SkillBuild).IsSuccess()
		|| !EquipmentCombatBuild.Validate().IsSuccess()
		|| (SourceKey != GolemProjection::RPGSourceKey && (!SkillBuild.SpecId.IsNone() || EquipmentCombatBuild.bEnabled)))
	{
		return FGolemResult::Failure(EGolemError::InvalidData, TEXT("ProjectionSkillBuild"),
			LOCTEXT("InvalidSkillBuild", "A skill projection requires a valid specialization and compatible rune."));
	}
	if (!GolemIdentity::IsValidIdentifier(SourceKey) || !GolemIdentity::IsValidDefinitionId(DefinitionId))
	{
		return FGolemResult::Failure(EGolemError::InvalidId, TEXT("ProjectionPlan"),
			LOCTEXT("InvalidSource", "A projection requires a source key and a stable owner definition ID."));
	}
	const float Values[] { MaxHealthBonus, MaxStaminaBonus, MaxRuneEnergyBonus, AttackPowerBonus, DefenseBonus,
		CritChanceBonus, HasteBonus, CooldownRecoveryBonus, MasteryBonus, VersatilityBonus, MaxPoiseBonus };
	for (float Value : Values)
	{
		if (!FMath::IsFinite(Value) || Value < 0.0f || Value > 10000.0f)
		{
			return FGolemResult::Failure(EGolemError::InvalidData, TEXT("ProjectionPlan"),
				LOCTEXT("InvalidBonus", "Projection bonuses must be finite and within the supported positive range."));
		}
	}
	if (ActionModifiers.Num() > 16)
	{
		return FGolemResult::Failure(EGolemError::CapacityExceeded, TEXT("ProjectionPlan"),
			LOCTEXT("TooManyActions", "A projection contains too many action modifiers."));
	}
	TSet<FGameplayTag> Tags;
	for (const FGolemActionModifier& Modifier : ActionModifiers)
	{
		const FGolemResult Result = Modifier.Validate();
		if (!Result.IsSuccess())
		{
			return Result;
		}
		if (Tags.Contains(Modifier.AbilityTag))
		{
			return FGolemResult::Failure(EGolemError::DuplicateId, TEXT("ProjectionPlan"),
				LOCTEXT("DuplicateAction", "Each ability action may appear only once in a projection."));
		}
		Tags.Add(Modifier.AbilityTag);
	}
	TArray<FPrimaryAssetId> Sets = OptionalAbilitySetIds;
	if (!OptionalAbilitySetId.IsValid() && OptionalAbilitySetId != FPrimaryAssetId())
	{
		return FGolemResult::Failure(EGolemError::InvalidId, TEXT("ProjectionPlan"),
			LOCTEXT("InvalidOptionalSet", "An optional ability set identity is incomplete."));
	}
	if (OptionalAbilitySetId.IsValid())
	{
		Sets.Add(OptionalAbilitySetId);
	}
	if (Sets.Num() > 8)
	{
		return FGolemResult::Failure(EGolemError::CapacityExceeded, TEXT("ProjectionPlan"),
			LOCTEXT("TooManySets", "A projection contains too many optional ability sets."));
	}
	TSet<FPrimaryAssetId> UniqueSets;
	for (const FPrimaryAssetId& Id : Sets)
	{
		if (!GolemIdentity::IsValidDefinitionId(Id)
			|| Id.PrimaryAssetType != FPrimaryAssetType(TEXT("GolemAbilitySet")))
		{
			return FGolemResult::Failure(EGolemError::TypeMismatch, TEXT("ProjectionPlan"),
				LOCTEXT("WrongSet", "An optional projection grant must reference a GolemAbilitySet definition."));
		}
		if (UniqueSets.Contains(Id))
		{
			return FGolemResult::Failure(EGolemError::DuplicateId, TEXT("ProjectionPlan"),
				LOCTEXT("DuplicateSet", "Each optional ability set may appear only once in a projection."));
		}
		UniqueSets.Add(Id);
	}
	return FGolemResult::Success();
}

bool FGolemGASProjectionPlan::HasStatBonuses() const
{
	return MaxHealthBonus > 0.0f || MaxStaminaBonus > 0.0f || MaxRuneEnergyBonus > 0.0f
		|| AttackPowerBonus > 0.0f || DefenseBonus > 0.0f || CritChanceBonus > 0.0f || HasteBonus > 0.0f
		|| CooldownRecoveryBonus > 0.0f || MasteryBonus > 0.0f || VersatilityBonus > 0.0f || MaxPoiseBonus > 0.0f;
}

bool FGolemGASProjectionPlan::operator==(const FGolemGASProjectionPlan& Other) const
{
	return SourceKey == Other.SourceKey && DefinitionId == Other.DefinitionId
		&& MaxHealthBonus == Other.MaxHealthBonus && MaxStaminaBonus == Other.MaxStaminaBonus
		&& MaxRuneEnergyBonus == Other.MaxRuneEnergyBonus && AttackPowerBonus == Other.AttackPowerBonus
		&& DefenseBonus == Other.DefenseBonus && ActionModifiers == Other.ActionModifiers
		&& CritChanceBonus == Other.CritChanceBonus && HasteBonus == Other.HasteBonus
		&& CooldownRecoveryBonus == Other.CooldownRecoveryBonus && MasteryBonus == Other.MasteryBonus
		&& VersatilityBonus == Other.VersatilityBonus && MaxPoiseBonus == Other.MaxPoiseBonus
		&& OptionalAbilitySetId == Other.OptionalAbilitySetId && OptionalAbilitySetIds == Other.OptionalAbilitySetIds
		&& SkillBuild == Other.SkillBuild && EquipmentCombatBuild == Other.EquipmentCombatBuild;
}

bool FGolemMutationFence::IsValid() const
{
	return Token.IsValid() && EntityId.IsValid() && RequestId.IsValid();
}

bool FGolemMutationFence::operator==(const FGolemMutationFence& Other) const
{
	return Token == Other.Token && EntityId == Other.EntityId && RequestId == Other.RequestId;
}

bool FGolemProjectionCapability::IsValid() const
{
	return Token.IsValid() && EntityId.IsValid() && !SourceKey.IsNone() && ExpectedRevision >= 0;
}

bool FGolemProjectionCapability::operator==(const FGolemProjectionCapability& Other) const
{
	return Token == Other.Token && EntityId == Other.EntityId && SourceKey == Other.SourceKey
		&& ExpectedRevision == Other.ExpectedRevision;
}

#undef LOCTEXT_NAMESPACE
