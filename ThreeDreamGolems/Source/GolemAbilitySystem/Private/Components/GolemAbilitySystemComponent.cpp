#include "Components/GolemAbilitySystemComponent.h"

#include "AbilitySystemGlobals.h"
#include "Abilities/GolemGameplayAbility.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/GolemLifeComponent.h"
#include "Cues/GolemCueVisualActor.h"
#include "Cues/GolemGameplayCueNotify.h"
#include "Effects/GolemDamageEffect.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Events/GolemEventSubsystem.h"
#include "GameplayCueManager.h"
#include "GameplayCueSet.h"
#include "Math/NumericLimits.h"
#include "Net/UnrealNetwork.h"
#include "Tags/GolemGameplayTags.h"

namespace
{
	FGolemResult GolemGASFailure(EGolemError Error, const TCHAR* Context)
	{
		FText Message = NSLOCTEXT("GolemGAS", "ActionStateUnavailable", "This action is unavailable in the current state.");
		if (Error == EGolemError::OutOfRange)
		{
			Message = NSLOCTEXT("GolemGAS", "ActionRange", "Move closer to the target.");
		}
		else if (Error == EGolemError::NotFound)
		{
			Message = NSLOCTEXT("GolemGAS", "ActionNotFound", "No available target or granted action was found.");
		}
		else if (Error == EGolemError::Conflict)
		{
			Message = NSLOCTEXT("GolemGAS", "ActionConflict", "Another action already owns this request.");
		}
		else if (Error == EGolemError::NotAuthority)
		{
			Message = NSLOCTEXT("GolemGAS", "ActionAuthority", "The host must validate this action.");
		}
		else if (Error == EGolemError::NotOwned)
		{
			Message = NSLOCTEXT("GolemGAS", "ActionOwnership", "This request no longer belongs to the current source.");
		}
		return FGolemResult::Failure(Error, Context, Message);
	}
}

UGolemAbilitySystemComponent::UGolemAbilitySystemComponent()
{
	SetIsReplicatedByDefault(true);
	SetReplicationMode(EGameplayEffectReplicationMode::Mixed);
}

void UGolemAbilitySystemComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UGolemAbilitySystemComponent, OwnedGrantCount);
}

FGolemResult UGolemAbilitySystemComponent::InitializeGolemActor(AActor* Golem, UGolemAttributeSet* InAttributes,
	UGolemLifeComponent* InLife, const FGolemEntityId& Identity, const FGolemAttributeDefaults& Defaults,
	bool bPartyGolem)
{
	if (!IsValid(Golem) || Golem != GetOwner() || InAttributes == nullptr || !IsValid(InLife) ||
		InLife->GetOwner() != Golem || !Identity.IsValid())
	{
		return GolemGASFailure(EGolemError::InvalidData, TEXT("InitializeGolemASC"));
	}
	if (EntityId.IsValid() && (!(EntityId == Identity) || Attributes != InAttributes || LifeComponent != InLife))
	{
		return GolemGASFailure(EGolemError::Conflict, TEXT("ImmutableGolemASCIdentity"));
	}
	const FGolemResult Validation = Defaults.Validate();
	if (!Validation.IsSuccess())
	{
		return Validation;
	}
	EntityId = Identity;
	Attributes = InAttributes;
	LifeComponent = InLife;
	InitAbilityActorInfo(Golem, Golem);
	if (!GetSpawnedAttributes().Contains(InAttributes))
	{
		AddAttributeSetSubobject(InAttributes);
	}
	RegisterNativeGameplayCues();
	if (Golem->HasAuthority())
	{
		SetLooseGameplayTagCount(GolemGameplayTags::Role_Party_Golem, bPartyGolem ? 1 : 0,
			EGameplayTagReplicationState::TagAndCountToAll);
		if (!bAttributesInitialized)
		{
			InitialDefaults = Defaults;
			const FGolemResult Applied = ApplyAttributeDefaults();
			if (!Applied.IsSuccess())
			{
				return Applied;
			}
			bAttributesInitialized = true;
		}
	}
	return InLife->InitializeLife(this);
}

UGolemAttributeSet* UGolemAbilitySystemComponent::GetGolemAttributes() const
{
	return Attributes;
}
UGolemLifeComponent* UGolemAbilitySystemComponent::GetLifeComponent() const
{
	return LifeComponent.Get();
}
float UGolemAbilitySystemComponent::GetInitialPower() const
{
	return InitialDefaults.Power;
}

FGolemEntityId UGolemAbilitySystemComponent::GetEntityId() const
{
	return EntityId;
}

void UGolemAbilitySystemComponent::RefreshReplicatedMontagePresentation()
{
	if (IsValid(GetOwner()) && !GetOwner()->HasAuthority() && EntityId.IsValid()
		&& GetWorld() != nullptr && !GetWorld()->bIsTearingDown && !GetOwner()->IsActorBeingDestroyed())
	{
		RefreshAbilityActorInfo();
		OnRep_ReplicatedAnimMontage();
	}
}

void UGolemAbilitySystemComponent::OnRep_ReplicatedAnimMontage()
{
	// UE skips locally controlled montage replication because predicted abilities normally play it locally.
	// Native Golem actions are ServerOnly, so their owner also consumes the authoritative GAS montage state.
	const UGameplayAbility* LocalAbility = GetAnimatingAbility();
	if (!AbilityActorInfo.IsValid() || !AbilityActorInfo->IsLocallyControlled()
		|| IsOwnerActorAuthoritative()
		|| (LocalAbility != nullptr && LocalAbility->GetNetExecutionPolicy() != EGameplayAbilityNetExecutionPolicy::ServerOnly))
	{
		Super::OnRep_ReplicatedAnimMontage();
		return;
	}
	if (!IsValid(GetOwner()) || GetOwner()->IsActorBeingDestroyed()
		|| GetWorld() == nullptr || GetWorld()->bIsTearingDown)
	{
		return;
	}
	UAnimInstance* Animation = AbilityActorInfo->GetAnimInstance();
	if (Animation == nullptr || !IsReadyForReplicatedMontage())
	{
		bPendingMontageRep = true;
		return;
	}
	bPendingMontageRep = false;
	const FGameplayAbilityRepAnimMontage& Replicated = GetRepAnimMontageInfo();
	const float PlayRate = Replicated.bSkipPlayRate ? 1.0f : Replicated.PlayRate;
	if (Replicated.Animation == nullptr)
	{
		if (LocalAnimMontageInfo.AnimMontage != nullptr)
		{
			CurrentMontageStop(0.1f);
		}
		return;
	}
	if (!FMath::IsFinite(PlayRate) || !FMath::IsFinite(Replicated.Position)
		|| !FMath::IsFinite(Replicated.BlendTime))
	{
		return;
	}
	UAnimSequenceBase* LocalAnimation = LocalAnimMontageInfo.AnimMontage != nullptr
		&& LocalAnimMontageInfo.AnimMontage->IsDynamicMontage()
		? LocalAnimMontageInfo.AnimMontage->GetFirstAnimReference() : LocalAnimMontageInfo.AnimMontage.Get();
	const bool bRestart = !Replicated.IsStopped && LocalAnimMontageInfo.AnimMontage != nullptr
		&& Animation->Montage_GetIsStopped(LocalAnimMontageInfo.AnimMontage);
	if (LocalAnimation != Replicated.Animation || LocalAnimMontageInfo.PlayInstanceId != Replicated.PlayInstanceId || bRestart)
	{
		if (UAnimMontage* Montage = Cast<UAnimMontage>(Replicated.Animation))
		{
			PlayMontageSimulated(Montage, PlayRate);
		}
		else
		{
			PlaySlotAnimationAsDynamicMontage_WithFractionalLoopsSimulated(Replicated.Animation, Replicated.SlotName,
				Replicated.BlendTime, Replicated.BlendOutTime, PlayRate, Replicated.PlayCount);
		}
		LocalAnimMontageInfo.PlayInstanceId = Replicated.PlayInstanceId;
	}
	UAnimMontage* Montage = LocalAnimMontageInfo.AnimMontage;
	if (Montage == nullptr)
	{
		bPendingMontageRep = true;
		return;
	}
	Animation->Montage_SetPlayRate(Montage, PlayRate);
	if (Replicated.IsStopped)
	{
		if (!Animation->Montage_GetIsStopped(Montage))
		{
			CurrentMontageStop(FMath::Max(0.0f, Replicated.BlendTime));
		}
		return;
	}
	const int32 Section = Replicated.SkipPositionCorrection
		? static_cast<int32>(Replicated.SectionIdToPlay) - 1 : Montage->GetSectionIndexFromPosition(Replicated.Position);
	if (Section != INDEX_NONE)
	{
		const int32 NextSection = static_cast<int32>(Replicated.NextSectionID) - 1;
		Animation->Montage_SetNextSection(Montage->GetSectionName(Section), Montage->GetSectionName(NextSection), Montage);
		if (Replicated.SkipPositionCorrection
			&& Montage->GetSectionIndexFromPosition(Animation->Montage_GetPosition(Montage)) != Section)
		{
			Animation->Montage_JumpToSection(Montage->GetSectionName(Section), Montage);
		}
	}
	if (!Replicated.SkipPositionCorrection)
	{
		const float CurrentPosition = Animation->Montage_GetPosition(Montage);
		const float DesiredPosition = FMath::Clamp(Replicated.Position, 0.0f, Montage->GetPlayLength());
		if (FMath::Abs(DesiredPosition - CurrentPosition) > 0.1f)
		{
			if (FAnimMontageInstance* Instance = Animation->GetActiveInstanceForMontage(Montage);
				Instance != nullptr && DesiredPosition > CurrentPosition && PlayRate > KINDA_SMALL_NUMBER)
			{
				const float DeltaTime = (DesiredPosition - CurrentPosition) / PlayRate;
				Instance->UpdateWeight(DeltaTime);
				Instance->HandleEvents(CurrentPosition, DesiredPosition, nullptr);
				Animation->TriggerAnimNotifies(DeltaTime, UAnimInstance::ENotifyTriggerMode::ForceMontageOnly);
			}
			Animation->Montage_SetPosition(Montage, DesiredPosition);
		}
	}
}

FGolemResult UGolemAbilitySystemComponent::ApplyAttributeDefaults()
{
	if (!IsValid(GetOwner()) || !GetOwner()->HasAuthority() || Attributes == nullptr)
	{
		return GolemGASFailure(EGolemError::NotAuthority, TEXT("InitializeAttributes"));
	}
	UGameplayEffect* Effect = NewObject<UGameplayEffect>(GetTransientPackage());
	Effect->DurationPolicy = EGameplayEffectDurationType::Instant;
	const auto AddOverride = [Effect](const FGameplayAttribute& Attribute, float Value)
	{
		FGameplayModifierInfo& Modifier = Effect->Modifiers.AddDefaulted_GetRef();
		Modifier.Attribute = Attribute;
		Modifier.ModifierOp = EGameplayModOp::Override;
		Modifier.ModifierMagnitude = FScalableFloat(Value);
	};
	AddOverride(UGolemAttributeSet::GetMaxHealthAttribute(), InitialDefaults.MaxHealth);
	AddOverride(UGolemAttributeSet::GetHealthAttribute(), InitialDefaults.MaxHealth);
	AddOverride(UGolemAttributeSet::GetPowerAttribute(), InitialDefaults.Power);
	AddOverride(UGolemAttributeSet::GetAgilityAttribute(), InitialDefaults.Agility);
	AddOverride(UGolemAttributeSet::GetColorHoldAttribute(), InitialDefaults.ColorHold);
	const FGameplayEffectSpec Spec(Effect, MakeEffectContext(), 1.0f);
	return ApplyGameplayEffectSpecToSelf(Spec).WasSuccessfullyApplied()
		? FGolemResult::Success() : GolemGASFailure(EGolemError::InvalidState, TEXT("InitializeAttributesGE"));
}

FGolemResult UGolemAbilitySystemComponent::ResetAttributesForCheckpoint()
{
	// The defaults effect overrides both MaxHealth and Health, so a retry starts at full health.
	return ApplyAttributeDefaults();
}

FGolemResult UGolemAbilitySystemComponent::GrantAbilitySet(const UGolemAbilitySetDefinition* Definition,
	FName SourceKey, FGolemAbilitySetHandle& OutHandle)
{
	OutHandle = FGolemAbilitySetHandle();
	if (!IsValid(GetOwner()) || !GetOwner()->HasAuthority())
	{
		return GolemGASFailure(EGolemError::NotAuthority, TEXT("GrantAbilitySet"));
	}
	if (!IsValid(Definition) || SourceKey.IsNone())
	{
		return GolemGASFailure(EGolemError::InvalidData, TEXT("GrantAbilitySet"));
	}
	const FGolemResult Validation = Definition->ValidateAbilitySet();
	if (!Validation.IsSuccess())
	{
		return Validation;
	}
	if (const FGolemOwnedAbilityGrant* Existing = OwnedGrants.Find(SourceKey))
	{
		if (Existing->Handle.AbilitySetId == Definition->GetPrimaryAssetId())
		{
			OutHandle = Existing->Handle;
			return FGolemResult::Success();
		}
		return GolemGASFailure(EGolemError::Conflict, TEXT("AbilityGrantSourceConflict"));
	}
	FGolemOwnedAbilityGrant Grant;
	Grant.Handle.GrantId = FGuid::NewGuid();
	Grant.Handle.SourceKey = SourceKey;
	Grant.Handle.AbilitySetId = Definition->GetPrimaryAssetId();
	Grant.Definition = const_cast<UGolemAbilitySetDefinition*>(Definition);
	for (const FGolemAbilitySetEntry& Entry : Definition->GetAbilities())
	{
		FGameplayAbilitySpec Spec(Entry.Ability, Entry.Level, INDEX_NONE, Grant.Definition);
		Spec.GetDynamicSpecSourceTags().AddTag(Entry.InputTag);
		const FGameplayAbilitySpecHandle Handle = GiveAbility(Spec);
		if (!Handle.IsValid())
		{
			for (const FGameplayAbilitySpecHandle Previous : Grant.AbilityHandles)
			{
				ClearAbility(Previous);
			}
			return GolemGASFailure(EGolemError::InvalidState, TEXT("AbilityGrantRejected"));
		}
		Grant.AbilityHandles.Add(Handle);
	}
	for (const TSubclassOf<UGameplayEffect>& EffectClass : Definition->GetEffects())
	{
		const FGameplayEffectSpecHandle Spec = MakeOutgoingSpec(EffectClass, 1.0f, MakeEffectContext());
		const FActiveGameplayEffectHandle Handle = Spec.IsValid()
			? ApplyGameplayEffectSpecToSelf(*Spec.Data.Get()) : FActiveGameplayEffectHandle();
		if (!Handle.IsValid())
		{
			for (const FGameplayAbilitySpecHandle Previous : Grant.AbilityHandles)
			{
				ClearAbility(Previous);
			}
			for (const FActiveGameplayEffectHandle Previous : Grant.EffectHandles)
			{
				RemoveActiveGameplayEffect(Previous);
			}
			return GolemGASFailure(EGolemError::InvalidState, TEXT("AbilitySetEffectRejected"));
		}
		Grant.EffectHandles.Add(Handle);
	}
	OutHandle = Grant.Handle;
	OwnedGrants.Add(SourceKey, MoveTemp(Grant));
	OwnedGrantCount = OwnedGrants.Num();
	OnAbilityGrantsChanged.Broadcast();
	GetOwner()->ForceNetUpdate();
	return FGolemResult::Success();
}

FGolemResult UGolemAbilitySystemComponent::RevokeAbilitySet(const FGolemAbilitySetHandle& Handle)
{
	if (!IsValid(GetOwner()) || !GetOwner()->HasAuthority())
	{
		return GolemGASFailure(EGolemError::NotAuthority, TEXT("RevokeAbilitySet"));
	}
	if (const FGolemAbilitySetHandle* Revoked = LastRevokedGrants.Find(Handle.SourceKey))
	{
		if (Handle.IsValid() && Revoked->GrantId == Handle.GrantId && Revoked->AbilitySetId == Handle.AbilitySetId)
		{
			return FGolemResult::Success();
		}
	}
	FGolemOwnedAbilityGrant* Grant = OwnedGrants.Find(Handle.SourceKey);
	if (!Handle.IsValid() || Grant == nullptr || Grant->Handle.GrantId != Handle.GrantId ||
		Grant->Handle.AbilitySetId != Handle.AbilitySetId)
	{
		return GolemGASFailure(EGolemError::NotOwned, TEXT("RevokeAbilitySetToken"));
	}
	const FGolemOwnedAbilityGrant Removed = *Grant;
	// Keep relationship data until active abilities unwind their block counts during cancellation.
	for (const FGameplayAbilitySpecHandle AbilityHandle : Removed.AbilityHandles)
	{
		CancelAbilityHandle(AbilityHandle);
		ClearAbility(AbilityHandle);
	}
	for (const FActiveGameplayEffectHandle EffectHandle : Removed.EffectHandles)
	{
		RemoveActiveGameplayEffect(EffectHandle);
	}
	OwnedGrants.Remove(Handle.SourceKey);
	LastRevokedGrants.Add(Handle.SourceKey, Handle);
	OwnedGrantCount = OwnedGrants.Num();
	OnAbilityGrantsChanged.Broadcast();
	GetOwner()->ForceNetUpdate();
	return FGolemResult::Success();
}

int32 UGolemAbilitySystemComponent::GetOwnedGrantCount() const
{
	return OwnedGrantCount;
}
void UGolemAbilitySystemComponent::OnRep_OwnedGrantCount()
{
	OnAbilityGrantsChanged.Broadcast();
}

void UGolemAbilitySystemComponent::HandleControllerChanged()
{
	const bool bHasCancellationContext = AbilityActorInfo.IsValid() && AbilityActorInfo->OwnerActor.IsValid(true) &&
		AbilityActorInfo->AbilitySystemComponent.IsValid(true);
	if (bHasCancellationContext)
	{
		CancelAllAbilities();
	}
	ABILITYLIST_SCOPE_LOCK();
	for (FGameplayAbilitySpec& Spec : GetActivatableAbilities())
	{
		Spec.InputPressed = false;
	}
	const UWorld* World = GetWorld();
	if (World != nullptr && !World->bIsTearingDown && AbilityActorInfo.IsValid() &&
		AbilityActorInfo->OwnerActor.IsValid() && AbilityActorInfo->AvatarActor.IsValid() &&
		!AbilityActorInfo->OwnerActor->IsActorBeingDestroyed() && !AbilityActorInfo->AvatarActor->IsActorBeingDestroyed())
	{
		RefreshAbilityActorInfo();
	}
}

void UGolemAbilitySystemComponent::ClearActorInfo()
{
	if (AbilityActorInfo.IsValid() && AbilityActorInfo->OwnerActor.IsValid(true) &&
		AbilityActorInfo->AbilitySystemComponent.IsValid(true))
	{
		CancelAllAbilities();
	}
	ABILITYLIST_SCOPE_LOCK();
	for (FGameplayAbilitySpec& Spec : GetActivatableAbilities())
	{
		Spec.InputPressed = false;
	}
	if (AbilityActorInfo.IsValid())
	{
		Super::ClearActorInfo();
	}
}

FGolemResult UGolemAbilitySystemComponent::RequestAbilityInput(const FGameplayTag& InputTag,
	bool bPressed, AActor* Target)
{
	if (!IsValid(GetOwner()) || !GetOwner()->HasAuthority())
	{
		return GolemGASFailure(EGolemError::NotAuthority, TEXT("AbilityInput"));
	}
	if (!InputTag.IsValid())
	{
		return GolemGASFailure(EGolemError::InvalidData, TEXT("AbilityInputTag"));
	}
	ABILITYLIST_SCOPE_LOCK();
	for (FGameplayAbilitySpec& Spec : GetActivatableAbilities())
	{
		if (!Spec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
		{
			continue;
		}
		if (!bPressed)
		{
			Spec.InputPressed = false;
			AbilitySpecInputReleased(Spec);
			// The live instance answers: one ability can be a held move for one Golem and a tap for another.
			const UGameplayAbility* Live = Spec.GetPrimaryInstance() != nullptr ? Spec.GetPrimaryInstance() : Spec.Ability.Get();
			const UGolemGameplayAbility* Ability = Cast<UGolemGameplayAbility>(Live);
			if (Spec.IsActive() && (Ability == nullptr || Ability->ShouldCancelOnInputRelease()))
			{
				CancelAbilityHandle(Spec.Handle);
			}
			return FGolemResult::Success();
		}
		if (LifeComponent == nullptr || !LifeComponent->IsAlive())
		{
			return GolemGASFailure(EGolemError::InvalidState, TEXT("AbilitySourceNotAlive"));
		}
		if (Spec.InputPressed && Spec.IsActive())
		{
			return FGolemResult::Success();
		}
		Spec.InputPressed = true;
		if (Spec.IsActive())
		{
			AbilitySpecInputPressed(Spec);
			return FGolemResult::Success();
		}
		FGameplayEventData Payload;
		Payload.EventTag = InputTag;
		Payload.Instigator = GetOwner();
		Payload.Target = IsValid(Target) ? Target : FindAbilityTarget(InputTag);
		LastAbilityResult = GolemGASFailure(EGolemError::InvalidState, TEXT("AbilityActivationRejected"));
		if (!InternalTryActivateAbility(Spec.Handle, FPredictionKey(), nullptr, nullptr, &Payload))
		{
			Spec.InputPressed = false;
			ReportAbilityResult(LastAbilityResult);
		}
		return LastAbilityResult;
	}
	return GolemGASFailure(EGolemError::NotFound, TEXT("InputAbilityNotGranted"));
}

AActor* UGolemAbilitySystemComponent::FindAbilityTarget(const FGameplayTag& InputTag) const
{
	if (!IsValid(GetOwner()) || GetWorld() == nullptr)
	{
		return nullptr;
	}
	const bool bRevive = InputTag == GolemGameplayTags::Input_Party_Revive;
	float BestDistanceSquared = TNumericLimits<float>::Max();
	AActor* Best = nullptr;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* Candidate = *It;
		if (Candidate == GetOwner())
		{
			continue;
		}
		const UGolemAbilitySystemComponent* CandidateASC = Cast<UGolemAbilitySystemComponent>(
			UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Candidate, true));
		if (CandidateASC == nullptr || CandidateASC->GetLifeComponent() == nullptr)
		{
			continue;
		}
		const bool bEligible = bRevive
			? CandidateASC->HasMatchingGameplayTag(GolemGameplayTags::Role_Party_Golem) &&
				CandidateASC->GetLifeComponent()->IsDowned()
			: CandidateASC->HasMatchingGameplayTag(GolemGameplayTags::Role_Enemy) &&
				!CandidateASC->GetLifeComponent()->IsDead();
		const float DistanceSquared = FVector::DistSquared(GetOwner()->GetActorLocation(), Candidate->GetActorLocation());
		const float CandidateRange = bRevive ? CandidateASC->GetLifeComponent()->GetReviveRange() : 1000.0f;
		if (bEligible && DistanceSquared <= FMath::Square(CandidateRange) && DistanceSquared <= BestDistanceSquared)
		{
			Best = Candidate;
			BestDistanceSquared = DistanceSquared;
		}
	}
	return Best;
}

FGolemResult UGolemAbilitySystemComponent::ApplyDamage(AActor* Source, AActor* Target, float Amount)
{
	return ApplyCombatDamage(Source, Target, Amount, 0.0f, false);
}

FGolemResult UGolemAbilitySystemComponent::ApplyCombatDamage(AActor* Source, AActor* Target,
	float Amount, float PoiseDamage, bool bCanDefend, const FHitResult* Hit)
{
	if (!IsValid(Source) || !IsValid(Target) || Source->GetWorld() != Target->GetWorld() ||
		!FMath::IsFinite(Amount) || Amount <= 0.0f || Amount > 1000000.0f ||
		!FMath::IsFinite(PoiseDamage) || PoiseDamage < 0.0f || PoiseDamage > 1000000.0f ||
		(Hit != nullptr && Hit->GetActor() != Target))
	{
		return GolemGASFailure(EGolemError::InvalidData, TEXT("DamageRequest"));
	}
	if (!Source->HasAuthority() || !Target->HasAuthority())
	{
		return GolemGASFailure(EGolemError::NotAuthority, TEXT("DamageAuthority"));
	}
	UGolemAbilitySystemComponent* SourceASC = Cast<UGolemAbilitySystemComponent>(
		UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Source, true));
	UGolemAbilitySystemComponent* TargetASC = Cast<UGolemAbilitySystemComponent>(
		UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target, true));
	if (SourceASC == nullptr || TargetASC == nullptr || TargetASC->GetLifeComponent() == nullptr ||
		TargetASC->GetLifeComponent()->IsDead())
	{
		return GolemGASFailure(EGolemError::InvalidState, TEXT("DamageTargetUnavailable"));
	}
	FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
	if (Hit != nullptr)
	{
		Context.AddHitResult(*Hit, true);
	}
	FGameplayEffectSpecHandle Spec = SourceASC->MakeOutgoingSpec(UGolemDamageEffect::StaticClass(), 1.0f, Context);
	if (!Spec.IsValid())
	{
		return GolemGASFailure(EGolemError::InvalidState, TEXT("DamageSpec"));
	}
	Spec.Data->SetSetByCallerMagnitude(GolemGameplayTags::Data_Damage, Amount);
	Spec.Data->SetSetByCallerMagnitude(GolemGameplayTags::Data_DamagePoise, PoiseDamage);
	Spec.Data->SetSetByCallerMagnitude(GolemGameplayTags::Data_DamageCanDefend, bCanDefend ? 1.0f : 0.0f);
	return SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC).WasSuccessfullyApplied()
		? FGolemResult::Success() : GolemGASFailure(EGolemError::InvalidState, TEXT("DamageEffectRejected"));
}

void UGolemAbilitySystemComponent::GetAdditionalActivationTagRequirements(const FGameplayTagContainer& AbilityTags,
	FGameplayTagContainer& OutRequiredTags, FGameplayTagContainer& OutBlockedTags) const
{
	for (const TPair<FName, FGolemOwnedAbilityGrant>& Entry : OwnedGrants)
	{
		for (const FGolemTagRelationship& Relationship : Entry.Value.Definition->GetRelationships())
		{
			if (AbilityTags.HasTag(Relationship.AbilityTag))
			{
				OutRequiredTags.AppendTags(Relationship.ActivationRequiredTags);
				OutBlockedTags.AppendTags(Relationship.ActivationBlockedTags);
			}
		}
	}
}

void UGolemAbilitySystemComponent::ApplyAbilityBlockAndCancelTags(const FGameplayTagContainer& AbilityTags,
	UGameplayAbility* RequestingAbility, bool bEnableBlockTags, const FGameplayTagContainer& BlockTags,
	bool bExecuteCancelTags, const FGameplayTagContainer& CancelTags)
{
	FGameplayTagContainer ExpandedBlock = BlockTags;
	FGameplayTagContainer ExpandedCancel = CancelTags;
	for (const TPair<FName, FGolemOwnedAbilityGrant>& Entry : OwnedGrants)
	{
		for (const FGolemTagRelationship& Relationship : Entry.Value.Definition->GetRelationships())
		{
			if (AbilityTags.HasTag(Relationship.AbilityTag))
			{
				ExpandedBlock.AppendTags(Relationship.AbilityTagsToBlock);
				ExpandedCancel.AppendTags(Relationship.AbilityTagsToCancel);
			}
		}
	}
	Super::ApplyAbilityBlockAndCancelTags(AbilityTags, RequestingAbility, bEnableBlockTags, ExpandedBlock,
		bExecuteCancelTags, ExpandedCancel);
}

void UGolemAbilitySystemComponent::ReportAbilityResult(const FGolemResult& Result)
{
	LastAbilityResult = Result;
	OnAbilityResult.Broadcast(Result);
}

void UGolemAbilitySystemComponent::RegisterNativeGameplayCues()
{
	UGameplayCueSet* Set = UAbilitySystemGlobals::Get().GetGameplayCueManager()->GetRuntimeCueSet();
	if (Set == nullptr)
	{
		return;
	}
	TArray<FGameplayCueReferencePair> Cues;
	for (const FGameplayTag Tag : {FGameplayTag(GolemGameplayTags::GameplayCue_Damage),
		FGameplayTag(GolemGameplayTags::GameplayCue_Downed), FGameplayTag(GolemGameplayTags::GameplayCue_Revive),
		FGameplayTag(GolemGameplayTags::GameplayCue_Parry), FGameplayTag(GolemGameplayTags::GameplayCue_Stagger)})
	{
		if (!Set->GameplayCueDataMap.Contains(Tag))
		{
			Cues.Emplace(Tag, FSoftObjectPath(UGolemGameplayCueNotify::StaticClass()));
		}
	}
	Set->AddCues(Cues);
}

void UGolemAbilitySystemComponent::RecordGameplayCue(FGameplayTag Tag, EGameplayCueEvent::Type EventType)
{
	FGolemCueDiagnostics& Diagnostics = CueDiagnostics.FindOrAdd(Tag);
	if (EventType == EGameplayCueEvent::Executed)
	{
		++Diagnostics.ExecutedCount;
	}
	else if (EventType == EGameplayCueEvent::OnActive || EventType == EGameplayCueEvent::WhileActive)
	{
		Diagnostics.ActiveCount = 1;
	}
	else if (EventType == EGameplayCueEvent::Removed)
	{
		Diagnostics.ActiveCount = 0;
		++Diagnostics.RemovedCount;
	}
	OnGameplayCueProcessed.Broadcast(Tag, EventType);
}

FGolemCueDiagnostics UGolemAbilitySystemComponent::GetCueDiagnostics(FGameplayTag Tag) const
{
	const FGolemCueDiagnostics* Diagnostics = CueDiagnostics.Find(Tag);
	return Diagnostics != nullptr ? *Diagnostics : FGolemCueDiagnostics();
}

void UGolemAbilitySystemComponent::PresentGameplayCue(FGameplayTag Tag, EGameplayCueEvent::Type EventType)
{
	if (GetWorld() == nullptr || GetWorld()->GetNetMode() == NM_DedicatedServer || !IsValid(GetOwner()))
	{
		return;
	}
	CueVisuals.RemoveAll([](const TObjectPtr<AGolemCueVisualActor>& Visual)
		{
			return !IsValid(Visual);
		});
	if (EventType == EGameplayCueEvent::Removed)
	{
		for (AGolemCueVisualActor* Visual : CueVisuals)
		{
			if (Visual->GetCueTag() == Tag && Visual->IsPersistent())
			{
				Visual->Destroy();
			}
		}
		return;
	}
	const bool bPersistent = EventType != EGameplayCueEvent::Executed;
	if (bPersistent)
	{
		for (const AGolemCueVisualActor* Visual : CueVisuals)
		{
			if (Visual->GetCueTag() == Tag && Visual->IsPersistent())
			{
				return;
			}
		}
	}
	AGolemCueVisualActor* Visual = GetWorld()->SpawnActor<AGolemCueVisualActor>();
	if (Visual != nullptr)
	{
		Visual->Configure(GetOwner(), Tag, bPersistent);
		CueVisuals.Add(Visual);
	}
}

void UGolemAbilitySystemComponent::PublishDamageConfirmation(AActor* Source, float AppliedDamage)
{
	if (GetOwner() == nullptr || !GetOwner()->HasAuthority() || GetWorld() == nullptr)
	{
		return;
	}
	UGolemEventSubsystem* Bus = GetWorld()->GetSubsystem<UGolemEventSubsystem>();
	if (Bus == nullptr)
	{
		return;
	}
	FGolemDamageConfirmedEvent Event;
	Event.ConfirmationId = FGuid::NewGuid();
	Event.Revision = ++DamageRevision;
	const UGolemAbilitySystemComponent* SourceASC = Cast<UGolemAbilitySystemComponent>(
		UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Source, true));
	Event.SourceId = SourceASC != nullptr ? SourceASC->GetEntityId() : EntityId;
	Event.TargetId = EntityId;
	Event.AppliedDamage = AppliedDamage;
	Event.RemainingHealth = Attributes != nullptr ? Attributes->GetHealth() : 0.0f;
	Bus->PublishConfirmed(Event);
}

void UGolemAbilitySystemComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	OnAbilityResult.Clear();
	OnAbilityGrantsChanged.Clear();
	OnGameplayCueProcessed.Clear();
	CancelAllAbilities();
	TArray<FGolemAbilitySetHandle> Handles;
	for (const TPair<FName, FGolemOwnedAbilityGrant>& Entry : OwnedGrants)
	{
		Handles.Add(Entry.Value.Handle);
	}
	for (const FGolemAbilitySetHandle& Handle : Handles)
	{
		RevokeAbilitySet(Handle);
	}
	for (AGolemCueVisualActor* Visual : CueVisuals)
	{
		if (IsValid(Visual))
		{
			Visual->Destroy();
		}
	}
	CueVisuals.Reset();
	Super::EndPlay(EndPlayReason);
}
