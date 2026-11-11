#include "Components/GolemAbilitySystemComponent.h"

#include "Abilities/GolemGameplayAbility.h"
#include "Assets/GolemAssetManager.h"
#include "Assets/GolemDefinition.h"
#include "Components/GolemLifeComponent.h"
#include "Effects/GolemSourceProjectionEffect.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Skills/GolemSkillRuntimeComponent.h"

namespace
{
	enum : uint8
	{
		GolemProjectionNone,
		GolemProjectionPrepared,
		GolemProjectionStaging,
		GolemProjectionStaged,
		GolemProjectionCommitted,
		GolemProjectionPublished
	};

	FGolemResult GolemProjectionFailure(EGolemError Error, FName Context)
	{
		return FGolemResult::Failure(Error, Context,
			NSLOCTEXT("GolemProjection", "TransactionUnavailable", "The loadout transaction could not be completed."));
	}
	FName GolemProjectionGrantKey(FName SourceKey, const FPrimaryAssetId& SetId)
	{
		return FName(*FString::Printf(TEXT("Projection.%s.%s"), *SourceKey.ToString(), *SetId.ToString()));
	}
	bool GolemProjectionAttribute(const FGameplayAttribute& Attribute)
	{
		return Attribute == UGolemAttributeSet::GetMaxHealthAttribute()
			|| Attribute == UGolemAttributeSet::GetMaxStaminaAttribute()
			|| Attribute == UGolemAttributeSet::GetMaxRuneEnergyAttribute()
			|| Attribute == UGolemAttributeSet::GetAttackPowerAttribute()
			|| Attribute == UGolemAttributeSet::GetDefenseAttribute()
			|| Attribute == UGolemAttributeSet::GetCritChanceAttribute()
			|| Attribute == UGolemAttributeSet::GetHasteAttribute()
			|| Attribute == UGolemAttributeSet::GetCooldownRecoveryAttribute()
			|| Attribute == UGolemAttributeSet::GetMasteryBonusAttribute()
			|| Attribute == UGolemAttributeSet::GetVersatilityAttribute()
			|| Attribute == UGolemAttributeSet::GetPoiseBonusAttribute();
	}
}

FGolemResult UGolemAbilitySystemComponent::ValidateProjectionOwner() const
{
	if (bProjectionEndingPlay || !IsValid(GetOwner()) || GetOwner()->IsActorBeingDestroyed()
		|| GetWorld() == nullptr || GetWorld()->bIsTearingDown || !EntityId.IsValid()
		|| !AbilityActorInfo.IsValid() || GetOwnerActor() != GetOwner() || GetAvatarActor() != GetOwner())
	{
		return GolemProjectionFailure(EGolemError::ShuttingDown, TEXT("ProjectionOwner"));
	}
	if (!GetOwner()->HasAuthority())
	{
		return GolemProjectionFailure(EGolemError::NotAuthority, TEXT("ProjectionAuthority"));
	}
	if (Attributes == nullptr || !LifeComponent.IsValid() || !LifeComponent->IsAlive())
	{
		return GolemProjectionFailure(EGolemError::InvalidState, TEXT("ProjectionLife"));
	}
	return FGolemResult::Success();
}

FGolemResult UGolemAbilitySystemComponent::BeginMutationFence(FGuid RequestId, FGolemMutationFence& OutFence)
{
	OutFence = FGolemMutationFence();
	const FGolemResult OwnerResult = ValidateProjectionOwner();
	if (!OwnerResult.IsSuccess())
	{
		return OwnerResult;
	}
	if (!RequestId.IsValid())
	{
		return GolemProjectionFailure(EGolemError::InvalidId, TEXT("ProjectionRequest"));
	}
	if (MutationFence.IsValid() || bProjectionOperationInProgress)
	{
		return GolemProjectionFailure(EGolemError::Conflict, TEXT("ProjectionFenceBusy"));
	}
	for (const FGameplayAbilitySpec& Spec : GetActivatableAbilities())
	{
		if (Spec.IsActive())
		{
			return GolemProjectionFailure(EGolemError::Conflict, TEXT("ProjectionActionActive"));
		}
	}
	MutationFence.Token = FGuid::NewGuid();
	MutationFence.EntityId = EntityId;
	MutationFence.RequestId = RequestId;
	FenceOwner = GetOwnerActor();
	FenceAvatar = GetAvatarActor();
	FenceController = Cast<APawn>(GetOwner()) != nullptr ? Cast<APawn>(GetOwner())->GetController() : nullptr;
	FenceGeneration = ++ProjectionGeneration;
	OutFence = MutationFence;
	return FGolemResult::Success();
}

FGolemResult UGolemAbilitySystemComponent::ValidateItemMutationFence(const FGolemMutationFence& Fence) const
{
	return ValidateMutationFence(Fence);
}

bool UGolemAbilitySystemComponent::IsMutationFenced() const
{
	return MutationFence.IsValid() || bProjectionOperationInProgress || bProjectionEndingPlay;
}

FGolemResult UGolemAbilitySystemComponent::ValidateMutationFence(const FGolemMutationFence& Fence) const
{
	if (!Fence.IsValid() || !(MutationFence == Fence))
	{
		return GolemProjectionFailure(EGolemError::NotOwned, TEXT("ProjectionFenceToken"));
	}
	const FGolemResult OwnerResult = ValidateProjectionOwner();
	if (!OwnerResult.IsSuccess())
	{
		return OwnerResult;
	}
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (FenceGeneration != ProjectionGeneration || !(Fence.EntityId == EntityId)
		|| FenceOwner.Get() != GetOwnerActor() || FenceAvatar.Get() != GetAvatarActor()
		|| (Pawn != nullptr && FenceController.Get() != Pawn->GetController()))
	{
		return GolemProjectionFailure(EGolemError::Cancelled, TEXT("ProjectionOwnershipChanged"));
	}
	return FGolemResult::Success();
}

FGolemResult UGolemAbilitySystemComponent::ValidateProjectionCapability(const FGolemMutationFence& Fence,
	const FGolemProjectionCapability& Capability) const
{
	const FGolemResult FenceResult = ValidateMutationFence(Fence);
	if (!FenceResult.IsSuccess())
	{
		return FenceResult;
	}
	return Capability.IsValid() && ProjectionCapability == Capability ? FGolemResult::Success()
		: GolemProjectionFailure(EGolemError::NotOwned, TEXT("ProjectionCapability"));
}

FGolemResult UGolemAbilitySystemComponent::ValidateProjectionAbilities(const FGolemGASProjectionPlan& Plan,
	const TArray<FGolemOwnedAbilityGrant>& OptionalGrants) const
{
	TSet<FGameplayTag> InputTags;
	TMap<FGameplayTag, int32> Actions;
	const FGolemProjectionRuntime* Old = ProjectionSources.Find(Plan.SourceKey);
	for (const FGameplayAbilitySpec& Spec : GetActivatableAbilities())
	{
		bool bOldSpec = false;
		if (Old != nullptr)
		{
			for (const FGolemOwnedAbilityGrant& Grant : Old->OptionalGrants)
			{
				bOldSpec |= Grant.AbilityHandles.Contains(Spec.Handle);
			}
		}
		if (bOldSpec)
		{
			continue;
		}
		if (Spec.Ability == nullptr)
		{
			continue;
		}
		for (FGameplayTag Tag : Spec.GetDynamicSpecSourceTags())
		{
			InputTags.Add(Tag);
		}
		for (FGameplayTag Tag : Spec.Ability->GetAssetTags())
		{
			++Actions.FindOrAdd(Tag);
		}
	}
	for (const FGolemOwnedAbilityGrant& Grant : OptionalGrants)
	{
		const UGolemAbilitySetDefinition* OptionalSet = Grant.Definition;
		for (const FGolemAbilitySetEntry& Entry : OptionalSet->GetAbilities())
		{
			if (InputTags.Contains(Entry.InputTag))
			{
				return GolemProjectionFailure(EGolemError::Conflict, TEXT("ProjectionDuplicateInput"));
			}
			InputTags.Add(Entry.InputTag);
			for (FGameplayTag Tag : Entry.Ability.GetDefaultObject()->GetAssetTags())
			{
				++Actions.FindOrAdd(Tag);
			}
		}
		for (TSubclassOf<UGameplayEffect> EffectClass : OptionalSet->GetEffects())
		{
			const UGameplayEffect* Effect = EffectClass.GetDefaultObject();
			if (Effect == nullptr || Effect->DurationPolicy != EGameplayEffectDurationType::Infinite
				|| Effect->Period.GetValueAtLevel(1.0f) != 0.0f || !Effect->Executions.IsEmpty()
				|| Effect->GetStackingType() != EGameplayEffectStackingType::None)
			{
				return GolemProjectionFailure(EGolemError::InvalidData, TEXT("ProjectionIrreversibleEffect"));
			}
			for (const FGameplayModifierInfo& Modifier : Effect->Modifiers)
			{
				float Magnitude = 0.0f;
				if (!GolemProjectionAttribute(Modifier.Attribute) || Modifier.ModifierOp != EGameplayModOp::Additive
					|| !Modifier.ModifierMagnitude.GetStaticMagnitudeIfPossible(1.0f, Magnitude)
					|| !FMath::IsFinite(Magnitude) || Magnitude < 0.0f || Magnitude > 10000.0f)
				{
					return GolemProjectionFailure(EGolemError::InvalidData, TEXT("ProjectionUnsafePassive"));
				}
			}
		}
	}
	for (const FGolemActionModifier& Modifier : Plan.ActionModifiers)
	{
		const int32* Count = Actions.Find(Modifier.AbilityTag);
		if (Count == nullptr || *Count != 1)
		{
			return GolemProjectionFailure(Count == nullptr ? EGolemError::NotFound : EGolemError::Conflict,
				TEXT("ProjectionActionSpec"));
		}
	}
	return FGolemResult::Success();
}

FGolemResult UGolemAbilitySystemComponent::PrepareSourceProjection(const FGolemMutationFence& Fence,
	const FGolemGASProjectionPlan& Plan, int64 ExpectedRevision, FGolemProjectionCapability& OutCapability)
{
	OutCapability = FGolemProjectionCapability();
	const FGolemResult FenceResult = ValidateMutationFence(Fence);
	if (!FenceResult.IsSuccess())
	{
		return FenceResult;
	}
	if (bProjectionOperationInProgress || ProjectionStage != GolemProjectionNone)
	{
		return GolemProjectionFailure(EGolemError::Conflict, TEXT("ProjectionPrepareBusy"));
	}
	const FGolemResult PlanResult = Plan.Validate();
	if (!PlanResult.IsSuccess())
	{
		return PlanResult;
	}
	UGolemSkillRuntimeComponent* Skills = GetOwner()->FindComponentByClass<UGolemSkillRuntimeComponent>();
	if (Plan.SourceKey == GolemProjection::RPGSourceKey
		&& (((!Plan.SkillBuild.SpecId.IsNone() || Plan.EquipmentCombatBuild.bEnabled) && Skills == nullptr)
			|| (Skills != nullptr && Skills->GetBuild().SpecId != Plan.SkillBuild.SpecId && Skills->HasDeferredDamage())))
	{
		return GolemProjectionFailure(EGolemError::Conflict, TEXT("ProjectionSkillRuntime"));
	}
	const FGolemProjectionRuntime* Old = ProjectionSources.Find(Plan.SourceKey);
	const int64 CurrentRevision = Old != nullptr ? Old->Snapshot.Revision : 0;
	if (ExpectedRevision != CurrentRevision || ExpectedRevision < 0 || ExpectedRevision >= MAX_int64)
	{
		return GolemProjectionFailure(EGolemError::InvalidRevision, TEXT("ProjectionRevision"));
	}
	if (Old == nullptr && ProjectionSources.Num() >= 8)
	{
		return GolemProjectionFailure(EGolemError::CapacityExceeded, TEXT("ProjectionSources"));
	}
	TGuardValue<bool> OperationGuard(bProjectionOperationInProgress, true);
	UGolemAssetManager* Manager = UGolemAssetManager::GetGolemAssetManager();
	UGolemDefinition* OwnerDefinition = nullptr;
	FGolemResult Result = Manager != nullptr ? Manager->ResolveDefinition(Plan.DefinitionId, OwnerDefinition)
		: GolemProjectionFailure(EGolemError::InvalidState, TEXT("ProjectionAssetManager"));
	if (!Result.IsSuccess())
	{
		return Result;
	}
	if (!ValidateMutationFence(Fence).IsSuccess())
	{
		return GolemProjectionFailure(EGolemError::Cancelled, TEXT("ProjectionResolveCancelled"));
	}
	TArray<FGolemOwnedAbilityGrant> Grants;
	TArray<FPrimaryAssetId> SetIds = Plan.OptionalAbilitySetIds;
	if (Plan.OptionalAbilitySetId.IsValid())
	{
		SetIds.Add(Plan.OptionalAbilitySetId);
	}
	for (const FPrimaryAssetId& SetId : SetIds)
	{
		UGolemDefinition* Definition = nullptr;
		Result = Manager->ResolveDefinition(SetId, UGolemAbilitySetDefinition::StaticClass(), Definition);
		if (!Result.IsSuccess())
		{
			return Result;
		}
		UGolemAbilitySetDefinition* Set = CastChecked<UGolemAbilitySetDefinition>(Definition);
		Result = Set->ValidateAbilitySet();
		if (!Result.IsSuccess())
		{
			return Result;
		}
		if (!ValidateMutationFence(Fence).IsSuccess())
		{
			return GolemProjectionFailure(EGolemError::Cancelled, TEXT("ProjectionSetResolveCancelled"));
		}
		FGolemOwnedAbilityGrant Grant;
		Grant.Definition = Set;
		Grant.Handle.GrantId = FGuid::NewGuid();
		Grant.Handle.SourceKey = GolemProjectionGrantKey(Plan.SourceKey, SetId);
		Grant.Handle.AbilitySetId = SetId;
		if (const FGolemOwnedAbilityGrant* Existing = OwnedGrants.Find(Grant.Handle.SourceKey))
		{
			bool bOwnExisting = false;
			if (Old != nullptr)
			{
				for (const FGolemOwnedAbilityGrant& Previous : Old->OptionalGrants)
				{
					bOwnExisting |= Previous.Handle.GrantId == Existing->Handle.GrantId;
				}
			}
			if (!bOwnExisting)
			{
				return GolemProjectionFailure(EGolemError::Conflict, TEXT("ProjectionForeignSourceCollision"));
			}
		}
		Grants.Add(Grant);
	}
	Result = ValidateProjectionAbilities(Plan, Grants);
	if (!Result.IsSuccess() || !ValidateMutationFence(Fence).IsSuccess())
	{
		return Result.IsSuccess() ? GolemProjectionFailure(EGolemError::Cancelled, TEXT("ProjectionPrepareCancelled")) : Result;
	}
	PendingProjection = FGolemProjectionRuntime();
	PendingProjection.OwnerDefinition = OwnerDefinition;
	PendingProjection.Snapshot.EntityId = EntityId;
	PendingProjection.Snapshot.Revision = ExpectedRevision + 1;
	PendingProjection.Snapshot.Plan = Plan;
	PendingProjection.OptionalGrants = MoveTemp(Grants);
	ProjectionCapability.Token = FGuid::NewGuid();
	ProjectionCapability.EntityId = EntityId;
	ProjectionCapability.SourceKey = Plan.SourceKey;
	ProjectionCapability.ExpectedRevision = ExpectedRevision;
	ProjectionStage = GolemProjectionPrepared;
	OutCapability = ProjectionCapability;
	return FGolemResult::Success();
}

FGolemResult UGolemAbilitySystemComponent::StageSourceProjection(const FGolemMutationFence& Fence,
	const FGolemProjectionCapability& Capability)
{
	const FGolemResult Validation = ValidateProjectionCapability(Fence, Capability);
	if (!Validation.IsSuccess())
	{
		return Validation;
	}
	if (ProjectionStage == GolemProjectionStaged)
	{
		return FGolemResult::Success();
	}
	if (bProjectionOperationInProgress || ProjectionStage != GolemProjectionPrepared)
	{
		return GolemProjectionFailure(EGolemError::Conflict, TEXT("ProjectionStageBusy"));
	}
	TGuardValue<bool> OperationGuard(bProjectionOperationInProgress, true);
	ProjectionStage = GolemProjectionStaging;
	FGolemProjectionRuntime Candidate = PendingProjection;
	const auto FailStage = [this, &Candidate, &Capability](EGolemError Error, FName Context)
	{
		if (Candidate.Snapshot.Plan.SourceKey == GolemProjection::RPGSourceKey)
		{
			if (UGolemSkillRuntimeComponent* Skills = GetOwner()->FindComponentByClass<UGolemSkillRuntimeComponent>())
			{
				Skills->AbortPreparedBuild();
				Skills->AbortPreparedEquipmentBuild();
			}
		}
		RemoveProjectionRuntime(Candidate);
		if (ProjectionCapability == Capability)
		{
			PendingProjection = FGolemProjectionRuntime();
			LastAbortedProjection = Capability;
			ProjectionCapability = FGolemProjectionCapability();
			ProjectionStage = GolemProjectionNone;
		}
		return GolemProjectionFailure(Error, Context);
	};
	const FGolemGASProjectionPlan Plan = Candidate.Snapshot.Plan;
	if (Plan.SourceKey == GolemProjection::RPGSourceKey)
	{
		if (UGolemSkillRuntimeComponent* Skills = GetOwner()->FindComponentByClass<UGolemSkillRuntimeComponent>())
		{
			const FGolemResult SkillResult = Skills->PrepareBuild(Plan.SkillBuild);
			if (!SkillResult.IsSuccess())
			{
				return FailStage(SkillResult.Error, SkillResult.Context);
			}
			const FGolemResult EquipmentResult = Skills->PrepareEquipmentBuild(Plan.EquipmentCombatBuild);
			if (!EquipmentResult.IsSuccess())
			{
				return FailStage(EquipmentResult.Error, EquipmentResult.Context);
			}
		}
	}
	for (FGolemOwnedAbilityGrant& Grant : Candidate.OptionalGrants)
	{
		for (const FGolemAbilitySetEntry& Entry : Grant.Definition->GetAbilities())
		{
			FGameplayAbilitySpec Spec(Entry.Ability, Entry.Level, INDEX_NONE, Grant.Definition);
			Spec.GetDynamicSpecSourceTags().AddTag(Entry.InputTag);
			const FGameplayAbilitySpecHandle Handle = GiveAbility(Spec);
			Grant.AbilityHandles.Add(Handle);
			if (!ValidateProjectionCapability(Fence, Capability).IsSuccess())
			{
				return FailStage(EGolemError::Cancelled, TEXT("ProjectionGrantCallbackCancelled"));
			}
			if (!Handle.IsValid() || FindAbilitySpecFromHandle(Handle) == nullptr)
			{
				return FailStage(EGolemError::InvalidState, TEXT("ProjectionGrantRejected"));
			}
		}
		for (TSubclassOf<UGameplayEffect> EffectClass : Grant.Definition->GetEffects())
		{
			const FGameplayEffectSpecHandle Spec = MakeOutgoingSpec(EffectClass, 1.0f, MakeEffectContext());
			const FActiveGameplayEffectHandle Handle = Spec.IsValid() ? ApplyGameplayEffectSpecToSelf(*Spec.Data.Get())
				: FActiveGameplayEffectHandle();
			Grant.EffectHandles.Add(Handle);
			if (!ValidateProjectionCapability(Fence, Capability).IsSuccess())
			{
				return FailStage(EGolemError::Cancelled, TEXT("ProjectionPassiveCallbackCancelled"));
			}
			if (!Handle.WasSuccessfullyApplied() || !Handle.IsValid() || GetActiveGameplayEffect(Handle) == nullptr)
			{
				return FailStage(EGolemError::InvalidState, TEXT("ProjectionPassiveRejected"));
			}
		}
	}
	if (Plan.HasStatBonuses())
	{
		FGameplayEffectSpecHandle Spec = MakeOutgoingSpec(UGolemSourceProjectionEffect::StaticClass(),
			1.0f, MakeEffectContext());
		if (!Spec.IsValid())
		{
			return FailStage(EGolemError::InvalidState, TEXT("ProjectionStatSpec"));
		}
		Spec.Data->SetSetByCallerMagnitude(UGolemSourceProjectionEffect::MaxHealthMagnitude, Plan.MaxHealthBonus);
		Spec.Data->SetSetByCallerMagnitude(UGolemSourceProjectionEffect::MaxStaminaMagnitude, Plan.MaxStaminaBonus);
		Spec.Data->SetSetByCallerMagnitude(UGolemSourceProjectionEffect::MaxRuneEnergyMagnitude, Plan.MaxRuneEnergyBonus);
		Spec.Data->SetSetByCallerMagnitude(UGolemSourceProjectionEffect::AttackPowerMagnitude, Plan.AttackPowerBonus);
		Spec.Data->SetSetByCallerMagnitude(UGolemSourceProjectionEffect::DefenseMagnitude, Plan.DefenseBonus);
		Spec.Data->SetSetByCallerMagnitude(UGolemSourceProjectionEffect::CritChanceMagnitude, Plan.CritChanceBonus);
		Spec.Data->SetSetByCallerMagnitude(UGolemSourceProjectionEffect::HasteMagnitude, Plan.HasteBonus);
		Spec.Data->SetSetByCallerMagnitude(UGolemSourceProjectionEffect::CooldownRecoveryMagnitude, Plan.CooldownRecoveryBonus);
		Spec.Data->SetSetByCallerMagnitude(UGolemSourceProjectionEffect::MasteryMagnitude, Plan.MasteryBonus);
		Spec.Data->SetSetByCallerMagnitude(UGolemSourceProjectionEffect::VersatilityMagnitude, Plan.VersatilityBonus);
		Spec.Data->SetSetByCallerMagnitude(UGolemSourceProjectionEffect::PoiseMagnitude, Plan.MaxPoiseBonus);
		Candidate.StatEffect = ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
		if (!ValidateProjectionCapability(Fence, Capability).IsSuccess())
		{
			return FailStage(EGolemError::Cancelled, TEXT("ProjectionStatCallbackCancelled"));
		}
		if (!Candidate.StatEffect.WasSuccessfullyApplied() || !Candidate.StatEffect.IsValid()
			|| GetActiveGameplayEffect(Candidate.StatEffect) == nullptr)
		{
			return FailStage(EGolemError::InvalidState, TEXT("ProjectionStatRejected"));
		}
	}
	PendingProjection = MoveTemp(Candidate);
	ProjectionStage = GolemProjectionStaged;
	return FGolemResult::Success();
}

void UGolemAbilitySystemComponent::RemoveProjectionRuntime(FGolemProjectionRuntime Runtime)
{
	for (const FGolemOwnedAbilityGrant& Grant : Runtime.OptionalGrants)
	{
		for (FGameplayAbilitySpecHandle Handle : Grant.AbilityHandles)
		{
			if (Handle.IsValid())
			{
				const FGameplayAbilitySpec* Spec = FindAbilitySpecFromHandle(Handle);
				if (Spec != nullptr && Spec->IsActive() && AbilityActorInfo.IsValid()
					&& AbilityActorInfo->OwnerActor.IsValid(true))
				{
					CancelAbilityHandle(Handle);
				}
				ClearAbility(Handle);
			}
		}
		for (FActiveGameplayEffectHandle Handle : Grant.EffectHandles)
		{
			if (Handle.IsValid())
			{
				RemoveActiveGameplayEffect(Handle);
			}
		}
	}
	if (Runtime.StatEffect.IsValid())
	{
		RemoveActiveGameplayEffect(Runtime.StatEffect);
	}
}

FGolemResult UGolemAbilitySystemComponent::ValidateStagedSourceProjection(const FGolemMutationFence& Fence,
	const FGolemProjectionCapability& Capability) const
{
	const FGolemResult Validation = ValidateProjectionCapability(Fence, Capability);
	if (!Validation.IsSuccess())
	{
		return Validation;
	}
	if (bProjectionOperationInProgress || ProjectionStage != GolemProjectionStaged)
	{
		return GolemProjectionFailure(EGolemError::Conflict, TEXT("ProjectionStagedValidation"));
	}
	if ((PendingProjection.StatEffect.IsValid() && GetActiveGameplayEffect(PendingProjection.StatEffect) == nullptr))
	{
		return GolemProjectionFailure(EGolemError::InvalidState, TEXT("ProjectionCandidateRemoved"));
	}
	for (const FGolemOwnedAbilityGrant& Grant : PendingProjection.OptionalGrants)
	{
		for (FGameplayAbilitySpecHandle Handle : Grant.AbilityHandles)
		{
			if (FindAbilitySpecFromHandle(Handle) == nullptr)
			{
				return GolemProjectionFailure(EGolemError::InvalidState, TEXT("ProjectionCandidateSpecRemoved"));
			}
		}
		for (FActiveGameplayEffectHandle Handle : Grant.EffectHandles)
		{
			if (GetActiveGameplayEffect(Handle) == nullptr)
			{
				return GolemProjectionFailure(EGolemError::InvalidState, TEXT("ProjectionCandidatePassiveRemoved"));
			}
		}
	}
	return FGolemResult::Success();
}

FGolemResult UGolemAbilitySystemComponent::CommitSourceProjection(const FGolemMutationFence& Fence,
	const FGolemProjectionCapability& Capability)
{
	const FGolemResult Validation = ValidateProjectionCapability(Fence, Capability);
	if (!Validation.IsSuccess())
	{
		return Validation;
	}
	if (ProjectionStage == GolemProjectionCommitted || ProjectionStage == GolemProjectionPublished)
	{
		return FGolemResult::Success();
	}
	const FGolemResult StagedValidation = ValidateStagedSourceProjection(Fence, Capability);
	if (!StagedValidation.IsSuccess())
	{
		return StagedValidation;
	}
	TGuardValue<bool> OperationGuard(bProjectionOperationInProgress, true);
	const FName SourceKey = Capability.SourceKey;
	const FGolemProjectionRuntime* Existing = ProjectionSources.Find(SourceKey);
	RetiredProjection = Existing != nullptr ? *Existing : FGolemProjectionRuntime();
	// Value promotion has no engine callbacks. The coordinator commits every participant before publication.
	ProjectionSources.Add(SourceKey, PendingProjection);
	if (SourceKey == GolemProjection::RPGSourceKey)
	{
		if (UGolemSkillRuntimeComponent* Skills = GetOwner()->FindComponentByClass<UGolemSkillRuntimeComponent>())
		{
			Skills->CommitPreparedBuild();
			Skills->CommitPreparedEquipmentBuild();
		}
	}
	ProjectionStage = GolemProjectionCommitted;
	for (const FGolemOwnedAbilityGrant& Grant : RetiredProjection.OptionalGrants)
	{
		OwnedGrants.Remove(Grant.Handle.SourceKey);
	}
	for (const FGolemOwnedAbilityGrant& Grant : PendingProjection.OptionalGrants)
	{
		OwnedGrants.Add(Grant.Handle.SourceKey, Grant);
	}
	FGolemSourceProjection* Published = PublishedProjections.FindByPredicate([SourceKey](const FGolemSourceProjection& Entry)
		{
			return Entry.Plan.SourceKey == SourceKey;
		});
	if (Published != nullptr)
	{
		*Published = PendingProjection.Snapshot;
	}
	else
	{
		PublishedProjections.Add(PendingProjection.Snapshot);
	}
	bProjectionGrantsChanged = OwnedGrantCount != OwnedGrants.Num();
	OwnedGrantCount = OwnedGrants.Num();
	return FGolemResult::Success();
}

FGolemResult UGolemAbilitySystemComponent::AbortSourceProjection(const FGolemMutationFence& Fence,
	const FGolemProjectionCapability& Capability)
{
	const FGolemResult FenceResult = ValidateMutationFence(Fence);
	if (!FenceResult.IsSuccess())
	{
		return FenceResult;
	}
	if (Capability.IsValid() && LastAbortedProjection == Capability)
	{
		return FGolemResult::Success();
	}
	if (bProjectionOperationInProgress || ProjectionStage >= GolemProjectionCommitted)
	{
		return GolemProjectionFailure(EGolemError::Conflict, TEXT("ProjectionAbortState"));
	}
	const FGolemResult Validation = ValidateProjectionCapability(Fence, Capability);
	if (!Validation.IsSuccess())
	{
		return Validation;
	}
	TGuardValue<bool> OperationGuard(bProjectionOperationInProgress, true);
	const FGolemProjectionRuntime Candidate = PendingProjection;
	if (Candidate.Snapshot.Plan.SourceKey == GolemProjection::RPGSourceKey)
	{
		if (UGolemSkillRuntimeComponent* Skills = GetOwner()->FindComponentByClass<UGolemSkillRuntimeComponent>())
		{
			Skills->AbortPreparedBuild();
			Skills->AbortPreparedEquipmentBuild();
		}
	}
	LastAbortedProjection = Capability;
	ProjectionCapability = FGolemProjectionCapability();
	ProjectionStage = GolemProjectionNone;
	PendingProjection = FGolemProjectionRuntime();
	RemoveProjectionRuntime(Candidate);
	return FGolemResult::Success();
}

FGolemResult UGolemAbilitySystemComponent::PublishSourceProjection(const FGolemMutationFence& Fence,
	const FGolemProjectionCapability& Capability, bool bNotifySubscribers, AActor* InPublicationOwner)
{
	const FGolemResult Validation = ValidateProjectionCapability(Fence, Capability);
	if (!Validation.IsSuccess())
	{
		return Validation;
	}
	if (ProjectionStage == GolemProjectionPublished)
	{
		return FGolemResult::Success();
	}
	if (bProjectionOperationInProgress || ProjectionStage != GolemProjectionCommitted)
	{
		return GolemProjectionFailure(EGolemError::Conflict, TEXT("ProjectionPublishState"));
	}
	TGuardValue<bool> OperationGuard(bProjectionOperationInProgress, true);
	const FGolemSourceProjection Snapshot = PendingProjection.Snapshot;
	const TWeakObjectPtr<UGolemAbilitySystemComponent> WeakASC(this);
	const bool bHasPublicationOwner = InPublicationOwner != nullptr;
	const TWeakObjectPtr<AActor> PublicationOwner(InPublicationOwner);
	const TWeakObjectPtr<UWorld> PublicationWorld(GetWorld());
	const uint64 PublishedGeneration = ProjectionGeneration;
	const bool bGrantsChanged = bProjectionGrantsChanged;
	bProjectionGrantsChanged = false;
	ProjectionStage = GolemProjectionPublished;
	// Extract first: a removal callback may end this ASC and must never drain these handles twice.
	FGolemProjectionRuntime Retired = MoveTemp(RetiredProjection);
	RetiredProjection = FGolemProjectionRuntime();
	RemoveProjectionRuntime(MoveTemp(Retired));
	if (!WeakASC.IsValid() || PublishedGeneration != ProjectionGeneration || !ValidateMutationFence(Fence).IsSuccess())
	{
		return FGolemResult::Success();
	}
	if (Snapshot.Plan.SourceKey == GolemProjection::RPGSourceKey)
	{
		if (UGolemSkillRuntimeComponent* Skills = GetOwner()->FindComponentByClass<UGolemSkillRuntimeComponent>())
		{
			// Publish after old stat handles retire so shield caps and subscribers see the final aggregate.
			Skills->PublishPreparedBuild(bNotifySubscribers);
		}
	}
	if (!WeakASC.IsValid() || PublishedGeneration != ProjectionGeneration || !ValidateMutationFence(Fence).IsSuccess())
	{
		return FGolemResult::Success();
	}
	GetOwner()->ForceNetUpdate();
	if (!bNotifySubscribers || (bHasPublicationOwner && (!PublicationOwner.IsValid()
		|| PublicationOwner->IsActorBeingDestroyed() || PublicationOwner->GetWorld() != PublicationWorld.Get())))
	{
		return FGolemResult::Success();
	}
	OnSourceProjectionChanged.Broadcast(Snapshot);
	if (bGrantsChanged && WeakASC.IsValid() && PublishedGeneration == ProjectionGeneration
		&& ValidateMutationFence(Fence).IsSuccess() && (!bHasPublicationOwner || (PublicationOwner.IsValid()
			&& !PublicationOwner->IsActorBeingDestroyed() && PublicationOwner->GetWorld() == PublicationWorld.Get())))
	{
		OnAbilityGrantsChanged.Broadcast();
	}
	return FGolemResult::Success();
}

FGolemResult UGolemAbilitySystemComponent::EndMutationFence(const FGolemMutationFence& Fence)
{
	if (Fence.IsValid() && LastEndedFence == Fence)
	{
		return FGolemResult::Success();
	}
	if (!Fence.IsValid() || !(MutationFence == Fence))
	{
		return GolemProjectionFailure(EGolemError::NotOwned, TEXT("ProjectionEndFence"));
	}
	if (bProjectionOperationInProgress || (ProjectionStage != GolemProjectionNone
		&& ProjectionStage != GolemProjectionPublished))
	{
		return GolemProjectionFailure(EGolemError::Conflict, TEXT("ProjectionUnfinishedFence"));
	}
	LastEndedFence = Fence;
	MutationFence = FGolemMutationFence();
	ProjectionCapability = FGolemProjectionCapability();
	PendingProjection = FGolemProjectionRuntime();
	RetiredProjection = FGolemProjectionRuntime();
	ProjectionStage = GolemProjectionNone;
	bProjectionGrantsChanged = false;
	FenceOwner.Reset();
	FenceAvatar.Reset();
	FenceController.Reset();
	return FGolemResult::Success();
}

FGolemSourceProjection UGolemAbilitySystemComponent::GetSourceProjection(FName SourceKey) const
{
	const FGolemSourceProjection* Found = PublishedProjections.FindByPredicate([SourceKey](const FGolemSourceProjection& Entry)
		{
			return Entry.Plan.SourceKey == SourceKey;
		});
	return Found != nullptr ? *Found : FGolemSourceProjection();
}

FGolemActionModifier UGolemAbilitySystemComponent::GetActionModifier(FGameplayTag AbilityTag) const
{
	FGolemActionModifier Result;
	Result.AbilityTag = AbilityTag;
	TArray<const FGolemSourceProjection*> Sorted;
	for (const FGolemSourceProjection& Entry : PublishedProjections)
	{
		Sorted.Add(&Entry);
	}
	Sorted.Sort([](const FGolemSourceProjection& Left, const FGolemSourceProjection& Right)
		{
			return Left.Plan.SourceKey.LexicalLess(Right.Plan.SourceKey);
		});
	for (const FGolemSourceProjection* Entry : Sorted)
	{
		for (const FGolemActionModifier& Modifier : Entry->Plan.ActionModifiers)
		{
			if (Modifier.AbilityTag == AbilityTag)
			{
				Result.DamageScale = FMath::Clamp(Result.DamageScale * Modifier.DamageScale, 0.1f, 10.0f);
				Result.CostScale = FMath::Clamp(Result.CostScale * Modifier.CostScale, 0.1f, 10.0f);
				Result.DurationScale = FMath::Clamp(Result.DurationScale * Modifier.DurationScale, 0.1f, 10.0f);
			}
		}
	}
	return Result;
}

float UGolemAbilitySystemComponent::GetInitialAttackPower() const
{
	return InitialDefaults.AttackPower;
}

void UGolemAbilitySystemComponent::OnRep_SourceProjections()
{
	const TArray<FGolemSourceProjection> Snapshot = PublishedProjections;
	for (const FGolemSourceProjection& Entry : Snapshot)
	{
		OnSourceProjectionChanged.Broadcast(Entry);
	}
}

void UGolemAbilitySystemComponent::CancelPendingProjection()
{
	TGuardValue<bool> OperationGuard(bProjectionOperationInProgress, true);
	++ProjectionGeneration;
	const FGolemProjectionRuntime Candidate = PendingProjection;
	FGolemProjectionRuntime Retired = MoveTemp(RetiredProjection);
	const bool bRemoveCandidate = ProjectionStage < GolemProjectionCommitted;
	PendingProjection = FGolemProjectionRuntime();
	RetiredProjection = FGolemProjectionRuntime();
	ProjectionCapability = FGolemProjectionCapability();
	ProjectionStage = GolemProjectionNone;
	MutationFence = FGolemMutationFence();
	bProjectionGrantsChanged = false;
	FenceOwner.Reset();
	FenceAvatar.Reset();
	FenceController.Reset();
	if (bRemoveCandidate)
	{
		if (UGolemSkillRuntimeComponent* Skills = GetOwner()->FindComponentByClass<UGolemSkillRuntimeComponent>())
		{
			Skills->AbortPreparedBuild();
			Skills->AbortPreparedEquipmentBuild();
		}
		RemoveProjectionRuntime(Candidate);
	}
	RemoveProjectionRuntime(MoveTemp(Retired));
}
