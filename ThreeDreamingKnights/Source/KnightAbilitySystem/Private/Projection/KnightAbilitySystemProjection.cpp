#include "Components/KnightAbilitySystemComponent.h"

#include "Abilities/KnightGameplayAbility.h"
#include "Assets/KnightAssetManager.h"
#include "Assets/KnightDefinition.h"
#include "Components/KnightLifeComponent.h"
#include "Effects/KnightSourceProjectionEffect.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"

namespace
{
	enum : uint8
	{
		KnightProjectionNone,
		KnightProjectionPrepared,
		KnightProjectionStaging,
		KnightProjectionStaged,
		KnightProjectionCommitted,
		KnightProjectionPublished
	};

	FKnightResult KnightProjectionFailure(EKnightError Error, FName Context)
	{
		return FKnightResult::Failure(Error, Context,
			NSLOCTEXT("KnightProjection", "TransactionUnavailable", "The loadout transaction could not be completed."));
	}
	FName KnightProjectionGrantKey(FName SourceKey, const FPrimaryAssetId& SetId)
	{
		return FName(*FString::Printf(TEXT("Projection.%s.%s"), *SourceKey.ToString(), *SetId.ToString()));
	}
	bool KnightProjectionAttribute(const FGameplayAttribute& Attribute)
	{
		return Attribute == UKnightAttributeSet::GetMaxHealthAttribute()
			|| Attribute == UKnightAttributeSet::GetMaxStaminaAttribute()
			|| Attribute == UKnightAttributeSet::GetMaxRuneEnergyAttribute()
			|| Attribute == UKnightAttributeSet::GetAttackPowerAttribute()
			|| Attribute == UKnightAttributeSet::GetDefenseAttribute();
	}
}

FKnightResult UKnightAbilitySystemComponent::ValidateProjectionOwner() const
{
	if (bProjectionEndingPlay || !IsValid(GetOwner()) || GetOwner()->IsActorBeingDestroyed()
		|| GetWorld() == nullptr || GetWorld()->bIsTearingDown || !EntityId.IsValid()
		|| !AbilityActorInfo.IsValid() || GetOwnerActor() != GetOwner() || GetAvatarActor() != GetOwner())
	{
		return KnightProjectionFailure(EKnightError::ShuttingDown, TEXT("ProjectionOwner"));
	}
	if (!GetOwner()->HasAuthority())
	{
		return KnightProjectionFailure(EKnightError::NotAuthority, TEXT("ProjectionAuthority"));
	}
	if (Attributes == nullptr || !LifeComponent.IsValid() || !LifeComponent->IsAlive())
	{
		return KnightProjectionFailure(EKnightError::InvalidState, TEXT("ProjectionLife"));
	}
	return FKnightResult::Success();
}

FKnightResult UKnightAbilitySystemComponent::BeginMutationFence(FGuid RequestId, FKnightMutationFence& OutFence)
{
	OutFence = FKnightMutationFence();
	const FKnightResult OwnerResult = ValidateProjectionOwner();
	if (!OwnerResult.IsSuccess())
	{
		return OwnerResult;
	}
	if (!RequestId.IsValid())
	{
		return KnightProjectionFailure(EKnightError::InvalidId, TEXT("ProjectionRequest"));
	}
	if (MutationFence.IsValid() || bProjectionOperationInProgress)
	{
		return KnightProjectionFailure(EKnightError::Conflict, TEXT("ProjectionFenceBusy"));
	}
	for (const FGameplayAbilitySpec& Spec : GetActivatableAbilities())
	{
		if (Spec.IsActive())
		{
			return KnightProjectionFailure(EKnightError::Conflict, TEXT("ProjectionActionActive"));
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
	return FKnightResult::Success();
}

bool UKnightAbilitySystemComponent::IsMutationFenced() const
{
	return MutationFence.IsValid() || bProjectionOperationInProgress || bProjectionEndingPlay;
}

FKnightResult UKnightAbilitySystemComponent::ValidateMutationFence(const FKnightMutationFence& Fence) const
{
	if (!Fence.IsValid() || !(MutationFence == Fence))
	{
		return KnightProjectionFailure(EKnightError::NotOwned, TEXT("ProjectionFenceToken"));
	}
	const FKnightResult OwnerResult = ValidateProjectionOwner();
	if (!OwnerResult.IsSuccess())
	{
		return OwnerResult;
	}
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (FenceGeneration != ProjectionGeneration || !(Fence.EntityId == EntityId)
		|| FenceOwner.Get() != GetOwnerActor() || FenceAvatar.Get() != GetAvatarActor()
		|| (Pawn != nullptr && FenceController.Get() != Pawn->GetController()))
	{
		return KnightProjectionFailure(EKnightError::Cancelled, TEXT("ProjectionOwnershipChanged"));
	}
	return FKnightResult::Success();
}

FKnightResult UKnightAbilitySystemComponent::ValidateProjectionCapability(const FKnightMutationFence& Fence,
	const FKnightProjectionCapability& Capability) const
{
	const FKnightResult FenceResult = ValidateMutationFence(Fence);
	if (!FenceResult.IsSuccess())
	{
		return FenceResult;
	}
	return Capability.IsValid() && ProjectionCapability == Capability ? FKnightResult::Success()
		: KnightProjectionFailure(EKnightError::NotOwned, TEXT("ProjectionCapability"));
}

FKnightResult UKnightAbilitySystemComponent::ValidateProjectionAbilities(const FKnightGASProjectionPlan& Plan,
	const TArray<FKnightOwnedAbilityGrant>& OptionalGrants) const
{
	TSet<FGameplayTag> InputTags;
	TMap<FGameplayTag, int32> Actions;
	const FKnightProjectionRuntime* Old = ProjectionSources.Find(Plan.SourceKey);
	for (const FGameplayAbilitySpec& Spec : GetActivatableAbilities())
	{
		bool bOldSpec = false;
		if (Old != nullptr)
		{
			for (const FKnightOwnedAbilityGrant& Grant : Old->OptionalGrants)
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
	for (const FKnightOwnedAbilityGrant& Grant : OptionalGrants)
	{
		const UKnightAbilitySetDefinition* OptionalSet = Grant.Definition;
		for (const FKnightAbilitySetEntry& Entry : OptionalSet->GetAbilities())
		{
			if (InputTags.Contains(Entry.InputTag))
			{
				return KnightProjectionFailure(EKnightError::Conflict, TEXT("ProjectionDuplicateInput"));
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
				return KnightProjectionFailure(EKnightError::InvalidData, TEXT("ProjectionIrreversibleEffect"));
			}
			for (const FGameplayModifierInfo& Modifier : Effect->Modifiers)
			{
				float Magnitude = 0.0f;
				if (!KnightProjectionAttribute(Modifier.Attribute) || Modifier.ModifierOp != EGameplayModOp::Additive
					|| !Modifier.ModifierMagnitude.GetStaticMagnitudeIfPossible(1.0f, Magnitude)
					|| !FMath::IsFinite(Magnitude) || Magnitude < 0.0f || Magnitude > 10000.0f)
				{
					return KnightProjectionFailure(EKnightError::InvalidData, TEXT("ProjectionUnsafePassive"));
				}
			}
		}
	}
	for (const FKnightActionModifier& Modifier : Plan.ActionModifiers)
	{
		const int32* Count = Actions.Find(Modifier.AbilityTag);
		if (Count == nullptr || *Count != 1)
		{
			return KnightProjectionFailure(Count == nullptr ? EKnightError::NotFound : EKnightError::Conflict,
				TEXT("ProjectionActionSpec"));
		}
	}
	return FKnightResult::Success();
}

FKnightResult UKnightAbilitySystemComponent::PrepareSourceProjection(const FKnightMutationFence& Fence,
	const FKnightGASProjectionPlan& Plan, int64 ExpectedRevision, FKnightProjectionCapability& OutCapability)
{
	OutCapability = FKnightProjectionCapability();
	const FKnightResult FenceResult = ValidateMutationFence(Fence);
	if (!FenceResult.IsSuccess())
	{
		return FenceResult;
	}
	if (bProjectionOperationInProgress || ProjectionStage != KnightProjectionNone)
	{
		return KnightProjectionFailure(EKnightError::Conflict, TEXT("ProjectionPrepareBusy"));
	}
	const FKnightResult PlanResult = Plan.Validate();
	if (!PlanResult.IsSuccess())
	{
		return PlanResult;
	}
	const FKnightProjectionRuntime* Old = ProjectionSources.Find(Plan.SourceKey);
	const int64 CurrentRevision = Old != nullptr ? Old->Snapshot.Revision : 0;
	if (ExpectedRevision != CurrentRevision || ExpectedRevision < 0 || ExpectedRevision >= MAX_int64)
	{
		return KnightProjectionFailure(EKnightError::InvalidRevision, TEXT("ProjectionRevision"));
	}
	if (Old == nullptr && ProjectionSources.Num() >= 8)
	{
		return KnightProjectionFailure(EKnightError::CapacityExceeded, TEXT("ProjectionSources"));
	}
	TGuardValue<bool> OperationGuard(bProjectionOperationInProgress, true);
	UKnightAssetManager* Manager = UKnightAssetManager::GetKnightAssetManager();
	UKnightDefinition* OwnerDefinition = nullptr;
	FKnightResult Result = Manager != nullptr ? Manager->ResolveDefinition(Plan.DefinitionId, OwnerDefinition)
		: KnightProjectionFailure(EKnightError::InvalidState, TEXT("ProjectionAssetManager"));
	if (!Result.IsSuccess())
	{
		return Result;
	}
	if (!ValidateMutationFence(Fence).IsSuccess())
	{
		return KnightProjectionFailure(EKnightError::Cancelled, TEXT("ProjectionResolveCancelled"));
	}
	TArray<FKnightOwnedAbilityGrant> Grants;
	TArray<FPrimaryAssetId> SetIds = Plan.OptionalAbilitySetIds;
	if (Plan.OptionalAbilitySetId.IsValid())
	{
		SetIds.Add(Plan.OptionalAbilitySetId);
	}
	for (const FPrimaryAssetId& SetId : SetIds)
	{
		UKnightDefinition* Definition = nullptr;
		Result = Manager->ResolveDefinition(SetId, UKnightAbilitySetDefinition::StaticClass(), Definition);
		if (!Result.IsSuccess())
		{
			return Result;
		}
		UKnightAbilitySetDefinition* Set = CastChecked<UKnightAbilitySetDefinition>(Definition);
		Result = Set->ValidateAbilitySet();
		if (!Result.IsSuccess())
		{
			return Result;
		}
		if (!ValidateMutationFence(Fence).IsSuccess())
		{
			return KnightProjectionFailure(EKnightError::Cancelled, TEXT("ProjectionSetResolveCancelled"));
		}
		FKnightOwnedAbilityGrant Grant;
		Grant.Definition = Set;
		Grant.Handle.GrantId = FGuid::NewGuid();
		Grant.Handle.SourceKey = KnightProjectionGrantKey(Plan.SourceKey, SetId);
		Grant.Handle.AbilitySetId = SetId;
		if (const FKnightOwnedAbilityGrant* Existing = OwnedGrants.Find(Grant.Handle.SourceKey))
		{
			bool bOwnExisting = false;
			if (Old != nullptr)
			{
				for (const FKnightOwnedAbilityGrant& Previous : Old->OptionalGrants)
				{
					bOwnExisting |= Previous.Handle.GrantId == Existing->Handle.GrantId;
				}
			}
			if (!bOwnExisting)
			{
				return KnightProjectionFailure(EKnightError::Conflict, TEXT("ProjectionForeignSourceCollision"));
			}
		}
		Grants.Add(Grant);
	}
	Result = ValidateProjectionAbilities(Plan, Grants);
	if (!Result.IsSuccess() || !ValidateMutationFence(Fence).IsSuccess())
	{
		return Result.IsSuccess() ? KnightProjectionFailure(EKnightError::Cancelled, TEXT("ProjectionPrepareCancelled")) : Result;
	}
	PendingProjection = FKnightProjectionRuntime();
	PendingProjection.OwnerDefinition = OwnerDefinition;
	PendingProjection.Snapshot.EntityId = EntityId;
	PendingProjection.Snapshot.Revision = ExpectedRevision + 1;
	PendingProjection.Snapshot.Plan = Plan;
	PendingProjection.OptionalGrants = MoveTemp(Grants);
	ProjectionCapability.Token = FGuid::NewGuid();
	ProjectionCapability.EntityId = EntityId;
	ProjectionCapability.SourceKey = Plan.SourceKey;
	ProjectionCapability.ExpectedRevision = ExpectedRevision;
	ProjectionStage = KnightProjectionPrepared;
	OutCapability = ProjectionCapability;
	return FKnightResult::Success();
}

FKnightResult UKnightAbilitySystemComponent::StageSourceProjection(const FKnightMutationFence& Fence,
	const FKnightProjectionCapability& Capability)
{
	const FKnightResult Validation = ValidateProjectionCapability(Fence, Capability);
	if (!Validation.IsSuccess())
	{
		return Validation;
	}
	if (ProjectionStage == KnightProjectionStaged)
	{
		return FKnightResult::Success();
	}
	if (bProjectionOperationInProgress || ProjectionStage != KnightProjectionPrepared)
	{
		return KnightProjectionFailure(EKnightError::Conflict, TEXT("ProjectionStageBusy"));
	}
	TGuardValue<bool> OperationGuard(bProjectionOperationInProgress, true);
	ProjectionStage = KnightProjectionStaging;
	FKnightProjectionRuntime Candidate = PendingProjection;
	const auto FailStage = [this, &Candidate, &Capability](EKnightError Error, FName Context)
	{
		RemoveProjectionRuntime(Candidate);
		if (ProjectionCapability == Capability)
		{
			PendingProjection = FKnightProjectionRuntime();
			LastAbortedProjection = Capability;
			ProjectionCapability = FKnightProjectionCapability();
			ProjectionStage = KnightProjectionNone;
		}
		return KnightProjectionFailure(Error, Context);
	};
	const FKnightGASProjectionPlan Plan = Candidate.Snapshot.Plan;
	for (FKnightOwnedAbilityGrant& Grant : Candidate.OptionalGrants)
	{
		for (const FKnightAbilitySetEntry& Entry : Grant.Definition->GetAbilities())
		{
			FGameplayAbilitySpec Spec(Entry.Ability, Entry.Level, INDEX_NONE, Grant.Definition);
			Spec.GetDynamicSpecSourceTags().AddTag(Entry.InputTag);
			const FGameplayAbilitySpecHandle Handle = GiveAbility(Spec);
			Grant.AbilityHandles.Add(Handle);
			if (!ValidateProjectionCapability(Fence, Capability).IsSuccess())
			{
				return FailStage(EKnightError::Cancelled, TEXT("ProjectionGrantCallbackCancelled"));
			}
			if (!Handle.IsValid() || FindAbilitySpecFromHandle(Handle) == nullptr)
			{
				return FailStage(EKnightError::InvalidState, TEXT("ProjectionGrantRejected"));
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
				return FailStage(EKnightError::Cancelled, TEXT("ProjectionPassiveCallbackCancelled"));
			}
			if (!Handle.WasSuccessfullyApplied() || !Handle.IsValid() || GetActiveGameplayEffect(Handle) == nullptr)
			{
				return FailStage(EKnightError::InvalidState, TEXT("ProjectionPassiveRejected"));
			}
		}
	}
	if (Plan.HasStatBonuses())
	{
		FGameplayEffectSpecHandle Spec = MakeOutgoingSpec(UKnightSourceProjectionEffect::StaticClass(),
			1.0f, MakeEffectContext());
		if (!Spec.IsValid())
		{
			return FailStage(EKnightError::InvalidState, TEXT("ProjectionStatSpec"));
		}
		Spec.Data->SetSetByCallerMagnitude(UKnightSourceProjectionEffect::MaxHealthMagnitude, Plan.MaxHealthBonus);
		Spec.Data->SetSetByCallerMagnitude(UKnightSourceProjectionEffect::MaxStaminaMagnitude, Plan.MaxStaminaBonus);
		Spec.Data->SetSetByCallerMagnitude(UKnightSourceProjectionEffect::MaxRuneEnergyMagnitude, Plan.MaxRuneEnergyBonus);
		Spec.Data->SetSetByCallerMagnitude(UKnightSourceProjectionEffect::AttackPowerMagnitude, Plan.AttackPowerBonus);
		Spec.Data->SetSetByCallerMagnitude(UKnightSourceProjectionEffect::DefenseMagnitude, Plan.DefenseBonus);
		Candidate.StatEffect = ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
		if (!ValidateProjectionCapability(Fence, Capability).IsSuccess())
		{
			return FailStage(EKnightError::Cancelled, TEXT("ProjectionStatCallbackCancelled"));
		}
		if (!Candidate.StatEffect.WasSuccessfullyApplied() || !Candidate.StatEffect.IsValid()
			|| GetActiveGameplayEffect(Candidate.StatEffect) == nullptr)
		{
			return FailStage(EKnightError::InvalidState, TEXT("ProjectionStatRejected"));
		}
	}
	PendingProjection = MoveTemp(Candidate);
	ProjectionStage = KnightProjectionStaged;
	return FKnightResult::Success();
}

void UKnightAbilitySystemComponent::RemoveProjectionRuntime(FKnightProjectionRuntime Runtime)
{
	for (const FKnightOwnedAbilityGrant& Grant : Runtime.OptionalGrants)
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

FKnightResult UKnightAbilitySystemComponent::ValidateStagedSourceProjection(const FKnightMutationFence& Fence,
	const FKnightProjectionCapability& Capability) const
{
	const FKnightResult Validation = ValidateProjectionCapability(Fence, Capability);
	if (!Validation.IsSuccess())
	{
		return Validation;
	}
	if (bProjectionOperationInProgress || ProjectionStage != KnightProjectionStaged)
	{
		return KnightProjectionFailure(EKnightError::Conflict, TEXT("ProjectionStagedValidation"));
	}
	if ((PendingProjection.StatEffect.IsValid() && GetActiveGameplayEffect(PendingProjection.StatEffect) == nullptr))
	{
		return KnightProjectionFailure(EKnightError::InvalidState, TEXT("ProjectionCandidateRemoved"));
	}
	for (const FKnightOwnedAbilityGrant& Grant : PendingProjection.OptionalGrants)
	{
		for (FGameplayAbilitySpecHandle Handle : Grant.AbilityHandles)
		{
			if (FindAbilitySpecFromHandle(Handle) == nullptr)
			{
				return KnightProjectionFailure(EKnightError::InvalidState, TEXT("ProjectionCandidateSpecRemoved"));
			}
		}
		for (FActiveGameplayEffectHandle Handle : Grant.EffectHandles)
		{
			if (GetActiveGameplayEffect(Handle) == nullptr)
			{
				return KnightProjectionFailure(EKnightError::InvalidState, TEXT("ProjectionCandidatePassiveRemoved"));
			}
		}
	}
	return FKnightResult::Success();
}

FKnightResult UKnightAbilitySystemComponent::CommitSourceProjection(const FKnightMutationFence& Fence,
	const FKnightProjectionCapability& Capability)
{
	const FKnightResult Validation = ValidateProjectionCapability(Fence, Capability);
	if (!Validation.IsSuccess())
	{
		return Validation;
	}
	if (ProjectionStage == KnightProjectionCommitted || ProjectionStage == KnightProjectionPublished)
	{
		return FKnightResult::Success();
	}
	const FKnightResult StagedValidation = ValidateStagedSourceProjection(Fence, Capability);
	if (!StagedValidation.IsSuccess())
	{
		return StagedValidation;
	}
	TGuardValue<bool> OperationGuard(bProjectionOperationInProgress, true);
	const FName SourceKey = Capability.SourceKey;
	const FKnightProjectionRuntime* Existing = ProjectionSources.Find(SourceKey);
	RetiredProjection = Existing != nullptr ? *Existing : FKnightProjectionRuntime();
	// Value promotion has no engine callbacks. The coordinator commits every participant before publication.
	ProjectionSources.Add(SourceKey, PendingProjection);
	ProjectionStage = KnightProjectionCommitted;
	for (const FKnightOwnedAbilityGrant& Grant : RetiredProjection.OptionalGrants)
	{
		OwnedGrants.Remove(Grant.Handle.SourceKey);
	}
	for (const FKnightOwnedAbilityGrant& Grant : PendingProjection.OptionalGrants)
	{
		OwnedGrants.Add(Grant.Handle.SourceKey, Grant);
	}
	FKnightSourceProjection* Published = PublishedProjections.FindByPredicate([SourceKey](const FKnightSourceProjection& Entry)
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
	return FKnightResult::Success();
}

FKnightResult UKnightAbilitySystemComponent::AbortSourceProjection(const FKnightMutationFence& Fence,
	const FKnightProjectionCapability& Capability)
{
	const FKnightResult FenceResult = ValidateMutationFence(Fence);
	if (!FenceResult.IsSuccess())
	{
		return FenceResult;
	}
	if (Capability.IsValid() && LastAbortedProjection == Capability)
	{
		return FKnightResult::Success();
	}
	if (bProjectionOperationInProgress || ProjectionStage >= KnightProjectionCommitted)
	{
		return KnightProjectionFailure(EKnightError::Conflict, TEXT("ProjectionAbortState"));
	}
	const FKnightResult Validation = ValidateProjectionCapability(Fence, Capability);
	if (!Validation.IsSuccess())
	{
		return Validation;
	}
	TGuardValue<bool> OperationGuard(bProjectionOperationInProgress, true);
	const FKnightProjectionRuntime Candidate = PendingProjection;
	LastAbortedProjection = Capability;
	ProjectionCapability = FKnightProjectionCapability();
	ProjectionStage = KnightProjectionNone;
	PendingProjection = FKnightProjectionRuntime();
	RemoveProjectionRuntime(Candidate);
	return FKnightResult::Success();
}

FKnightResult UKnightAbilitySystemComponent::PublishSourceProjection(const FKnightMutationFence& Fence,
	const FKnightProjectionCapability& Capability, bool bNotifySubscribers, AActor* InPublicationOwner)
{
	const FKnightResult Validation = ValidateProjectionCapability(Fence, Capability);
	if (!Validation.IsSuccess())
	{
		return Validation;
	}
	if (ProjectionStage == KnightProjectionPublished)
	{
		return FKnightResult::Success();
	}
	if (bProjectionOperationInProgress || ProjectionStage != KnightProjectionCommitted)
	{
		return KnightProjectionFailure(EKnightError::Conflict, TEXT("ProjectionPublishState"));
	}
	TGuardValue<bool> OperationGuard(bProjectionOperationInProgress, true);
	const FKnightSourceProjection Snapshot = PendingProjection.Snapshot;
	const TWeakObjectPtr<UKnightAbilitySystemComponent> WeakASC(this);
	const bool bHasPublicationOwner = InPublicationOwner != nullptr;
	const TWeakObjectPtr<AActor> PublicationOwner(InPublicationOwner);
	const TWeakObjectPtr<UWorld> PublicationWorld(GetWorld());
	const uint64 PublishedGeneration = ProjectionGeneration;
	const bool bGrantsChanged = bProjectionGrantsChanged;
	bProjectionGrantsChanged = false;
	ProjectionStage = KnightProjectionPublished;
	// Extract first: a removal callback may end this ASC and must never drain these handles twice.
	FKnightProjectionRuntime Retired = MoveTemp(RetiredProjection);
	RetiredProjection = FKnightProjectionRuntime();
	RemoveProjectionRuntime(MoveTemp(Retired));
	if (!WeakASC.IsValid() || PublishedGeneration != ProjectionGeneration || !ValidateMutationFence(Fence).IsSuccess())
	{
		return FKnightResult::Success();
	}
	GetOwner()->ForceNetUpdate();
	if (!bNotifySubscribers || (bHasPublicationOwner && (!PublicationOwner.IsValid()
		|| PublicationOwner->IsActorBeingDestroyed() || PublicationOwner->GetWorld() != PublicationWorld.Get())))
	{
		return FKnightResult::Success();
	}
	OnSourceProjectionChanged.Broadcast(Snapshot);
	if (bGrantsChanged && WeakASC.IsValid() && PublishedGeneration == ProjectionGeneration
		&& ValidateMutationFence(Fence).IsSuccess() && (!bHasPublicationOwner || (PublicationOwner.IsValid()
			&& !PublicationOwner->IsActorBeingDestroyed() && PublicationOwner->GetWorld() == PublicationWorld.Get())))
	{
		OnAbilityGrantsChanged.Broadcast();
	}
	return FKnightResult::Success();
}

FKnightResult UKnightAbilitySystemComponent::EndMutationFence(const FKnightMutationFence& Fence)
{
	if (Fence.IsValid() && LastEndedFence == Fence)
	{
		return FKnightResult::Success();
	}
	if (!Fence.IsValid() || !(MutationFence == Fence))
	{
		return KnightProjectionFailure(EKnightError::NotOwned, TEXT("ProjectionEndFence"));
	}
	if (bProjectionOperationInProgress || (ProjectionStage != KnightProjectionNone
		&& ProjectionStage != KnightProjectionPublished))
	{
		return KnightProjectionFailure(EKnightError::Conflict, TEXT("ProjectionUnfinishedFence"));
	}
	LastEndedFence = Fence;
	MutationFence = FKnightMutationFence();
	ProjectionCapability = FKnightProjectionCapability();
	PendingProjection = FKnightProjectionRuntime();
	RetiredProjection = FKnightProjectionRuntime();
	ProjectionStage = KnightProjectionNone;
	bProjectionGrantsChanged = false;
	FenceOwner.Reset();
	FenceAvatar.Reset();
	FenceController.Reset();
	return FKnightResult::Success();
}

FKnightSourceProjection UKnightAbilitySystemComponent::GetSourceProjection(FName SourceKey) const
{
	const FKnightSourceProjection* Found = PublishedProjections.FindByPredicate([SourceKey](const FKnightSourceProjection& Entry)
		{
			return Entry.Plan.SourceKey == SourceKey;
		});
	return Found != nullptr ? *Found : FKnightSourceProjection();
}

FKnightActionModifier UKnightAbilitySystemComponent::GetActionModifier(FGameplayTag AbilityTag) const
{
	FKnightActionModifier Result;
	Result.AbilityTag = AbilityTag;
	TArray<const FKnightSourceProjection*> Sorted;
	for (const FKnightSourceProjection& Entry : PublishedProjections)
	{
		Sorted.Add(&Entry);
	}
	Sorted.Sort([](const FKnightSourceProjection& Left, const FKnightSourceProjection& Right)
		{
			return Left.Plan.SourceKey.LexicalLess(Right.Plan.SourceKey);
		});
	for (const FKnightSourceProjection* Entry : Sorted)
	{
		for (const FKnightActionModifier& Modifier : Entry->Plan.ActionModifiers)
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

float UKnightAbilitySystemComponent::GetInitialAttackPower() const
{
	return InitialDefaults.AttackPower;
}

void UKnightAbilitySystemComponent::OnRep_SourceProjections()
{
	const TArray<FKnightSourceProjection> Snapshot = PublishedProjections;
	for (const FKnightSourceProjection& Entry : Snapshot)
	{
		OnSourceProjectionChanged.Broadcast(Entry);
	}
}

void UKnightAbilitySystemComponent::CancelPendingProjection()
{
	TGuardValue<bool> OperationGuard(bProjectionOperationInProgress, true);
	++ProjectionGeneration;
	const FKnightProjectionRuntime Candidate = PendingProjection;
	FKnightProjectionRuntime Retired = MoveTemp(RetiredProjection);
	const bool bRemoveCandidate = ProjectionStage < KnightProjectionCommitted;
	PendingProjection = FKnightProjectionRuntime();
	RetiredProjection = FKnightProjectionRuntime();
	ProjectionCapability = FKnightProjectionCapability();
	ProjectionStage = KnightProjectionNone;
	MutationFence = FKnightMutationFence();
	bProjectionGrantsChanged = false;
	FenceOwner.Reset();
	FenceAvatar.Reset();
	FenceController.Reset();
	if (bRemoveCandidate)
	{
		RemoveProjectionRuntime(Candidate);
	}
	RemoveProjectionRuntime(MoveTemp(Retired));
}
