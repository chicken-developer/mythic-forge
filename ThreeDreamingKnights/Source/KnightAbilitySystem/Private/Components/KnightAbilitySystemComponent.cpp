#include "Components/KnightAbilitySystemComponent.h"

#include "AbilitySystemGlobals.h"
#include "Abilities/KnightGameplayAbility.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/KnightLifeComponent.h"
#include "Cues/KnightCueVisualActor.h"
#include "Cues/KnightGameplayCueNotify.h"
#include "Effects/KnightDamageEffect.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Events/KnightEventSubsystem.h"
#include "GameplayCueManager.h"
#include "GameplayCueSet.h"
#include "Math/NumericLimits.h"
#include "Net/UnrealNetwork.h"
#include "Tags/KnightGameplayTags.h"

namespace
{
	FKnightResult KnightGASFailure(EKnightError Error, const TCHAR* Context)
	{
		FText Message = NSLOCTEXT("KnightGAS", "ActionStateUnavailable", "This action is unavailable in the current state.");
		if (Error == EKnightError::OutOfRange)
		{
			Message = NSLOCTEXT("KnightGAS", "ActionRange", "Move closer to the target.");
		}
		else if (Error == EKnightError::NotFound)
		{
			Message = NSLOCTEXT("KnightGAS", "ActionNotFound", "No available target or granted action was found.");
		}
		else if (Error == EKnightError::Conflict)
		{
			Message = NSLOCTEXT("KnightGAS", "ActionConflict", "Another action already owns this request.");
		}
		else if (Error == EKnightError::NotAuthority)
		{
			Message = NSLOCTEXT("KnightGAS", "ActionAuthority", "The host must validate this action.");
		}
		else if (Error == EKnightError::NotOwned)
		{
			Message = NSLOCTEXT("KnightGAS", "ActionOwnership", "This request no longer belongs to the current source.");
		}
		return FKnightResult::Failure(Error, Context, Message);
	}
}

UKnightAbilitySystemComponent::UKnightAbilitySystemComponent()
{
	SetIsReplicatedByDefault(true);
	SetReplicationMode(EGameplayEffectReplicationMode::Mixed);
}

void UKnightAbilitySystemComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UKnightAbilitySystemComponent, OwnedGrantCount);
	DOREPLIFETIME(UKnightAbilitySystemComponent, PublishedProjections);
}

FKnightResult UKnightAbilitySystemComponent::InitializeKnightActor(AActor* Knight, UKnightAttributeSet* InAttributes,
	UKnightLifeComponent* InLife, const FKnightEntityId& Identity, const FKnightAttributeDefaults& Defaults,
	bool bPartyKnight)
{
	if (!IsValid(Knight) || Knight != GetOwner() || InAttributes == nullptr || !IsValid(InLife) ||
		InLife->GetOwner() != Knight || !Identity.IsValid())
	{
		return KnightGASFailure(EKnightError::InvalidData, TEXT("InitializeKnightASC"));
	}
	if (EntityId.IsValid() && (!(EntityId == Identity) || Attributes != InAttributes || LifeComponent != InLife))
	{
		return KnightGASFailure(EKnightError::Conflict, TEXT("ImmutableKnightASCIdentity"));
	}
	const FKnightResult Validation = Defaults.Validate();
	if (!Validation.IsSuccess())
	{
		return Validation;
	}
	EntityId = Identity;
	Attributes = InAttributes;
	LifeComponent = InLife;
	InitAbilityActorInfo(Knight, Knight);
	if (!GetSpawnedAttributes().Contains(InAttributes))
	{
		AddAttributeSetSubobject(InAttributes);
	}
	RegisterNativeGameplayCues();
	if (Knight->HasAuthority())
	{
		SetLooseGameplayTagCount(KnightGameplayTags::Role_Party_Knight, bPartyKnight ? 1 : 0,
			EGameplayTagReplicationState::TagAndCountToAll);
		if (!bAttributesInitialized)
		{
			InitialDefaults = Defaults;
			const FKnightResult Applied = ApplyAttributeDefaults();
			if (!Applied.IsSuccess())
			{
				return Applied;
			}
			bAttributesInitialized = true;
		}
	}
	return InLife->InitializeLife(this);
}

UKnightAttributeSet* UKnightAbilitySystemComponent::GetKnightAttributes() const
{
	return Attributes;
}
UKnightLifeComponent* UKnightAbilitySystemComponent::GetLifeComponent() const
{
	return LifeComponent.Get();
}
FKnightEntityId UKnightAbilitySystemComponent::GetEntityId() const
{
	return EntityId;
}

void UKnightAbilitySystemComponent::RefreshReplicatedMontagePresentation()
{
	if (IsValid(GetOwner()) && !GetOwner()->HasAuthority() && EntityId.IsValid()
		&& GetWorld() != nullptr && !GetWorld()->bIsTearingDown && !GetOwner()->IsActorBeingDestroyed())
	{
		RefreshAbilityActorInfo();
		OnRep_ReplicatedAnimMontage();
	}
}

void UKnightAbilitySystemComponent::OnRep_ReplicatedAnimMontage()
{
	// UE skips locally controlled montage replication because predicted abilities normally play it locally.
	// Native Knight actions are ServerOnly, so their owner also consumes the authoritative GAS montage state.
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

FKnightResult UKnightAbilitySystemComponent::ApplyAttributeDefaults()
{
	if (!IsValid(GetOwner()) || !GetOwner()->HasAuthority() || Attributes == nullptr)
	{
		return KnightGASFailure(EKnightError::NotAuthority, TEXT("InitializeAttributes"));
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
	AddOverride(UKnightAttributeSet::GetMaxHealthAttribute(), InitialDefaults.MaxHealth);
	AddOverride(UKnightAttributeSet::GetMaxStaminaAttribute(), InitialDefaults.MaxStamina);
	AddOverride(UKnightAttributeSet::GetMaxRuneEnergyAttribute(), InitialDefaults.MaxRuneEnergy);
	AddOverride(UKnightAttributeSet::GetHealthAttribute(), InitialDefaults.MaxHealth);
	AddOverride(UKnightAttributeSet::GetStaminaAttribute(), InitialDefaults.MaxStamina);
	AddOverride(UKnightAttributeSet::GetRuneEnergyAttribute(), InitialDefaults.MaxRuneEnergy);
	AddOverride(UKnightAttributeSet::GetAttackPowerAttribute(), InitialDefaults.AttackPower);
	AddOverride(UKnightAttributeSet::GetDefenseAttribute(), InitialDefaults.Defense);
	const FGameplayEffectSpec Spec(Effect, MakeEffectContext(), 1.0f);
	return ApplyGameplayEffectSpecToSelf(Spec).WasSuccessfullyApplied()
		? FKnightResult::Success() : KnightGASFailure(EKnightError::InvalidState, TEXT("InitializeAttributesGE"));
}

FKnightResult UKnightAbilitySystemComponent::ResetAttributesForCheckpoint()
{
	if (IsMutationFenced())
	{
		return KnightGASFailure(EKnightError::Conflict, TEXT("CheckpointProjectionFence"));
	}
	const FKnightResult DefaultsResult = ApplyAttributeDefaults();
	if (!DefaultsResult.IsSuccess())
	{
		return DefaultsResult;
	}
	// Passive max bonuses remain on the same exact active handles. Retry fills their projected maxima.
	UGameplayEffect* Fill = NewObject<UGameplayEffect>(GetTransientPackage());
	Fill->DurationPolicy = EGameplayEffectDurationType::Instant;
	const FGameplayAttribute Resources[] { UKnightAttributeSet::GetHealthAttribute(),
		UKnightAttributeSet::GetStaminaAttribute(), UKnightAttributeSet::GetRuneEnergyAttribute() };
	const float Maxima[] { Attributes->GetMaxHealth(), Attributes->GetMaxStamina(), Attributes->GetMaxRuneEnergy() };
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Resources); ++Index)
	{
		FGameplayModifierInfo& Modifier = Fill->Modifiers.AddDefaulted_GetRef();
		Modifier.Attribute = Resources[Index];
		Modifier.ModifierOp = EGameplayModOp::Override;
		Modifier.ModifierMagnitude = FScalableFloat(Maxima[Index]);
	}
	const FGameplayEffectSpec Spec(Fill, MakeEffectContext(), 1.0f);
	return ApplyGameplayEffectSpecToSelf(Spec).WasSuccessfullyApplied() ? FKnightResult::Success()
		: KnightGASFailure(EKnightError::InvalidState, TEXT("CheckpointProjectedResources"));
}

FKnightResult UKnightAbilitySystemComponent::GrantAbilitySet(const UKnightAbilitySetDefinition* Definition,
	FName SourceKey, FKnightAbilitySetHandle& OutHandle)
{
	OutHandle = FKnightAbilitySetHandle();
	if (IsMutationFenced())
	{
		return KnightGASFailure(EKnightError::Conflict, TEXT("GrantProjectionFence"));
	}
	if (!IsValid(GetOwner()) || !GetOwner()->HasAuthority())
	{
		return KnightGASFailure(EKnightError::NotAuthority, TEXT("GrantAbilitySet"));
	}
	if (!IsValid(Definition) || SourceKey.IsNone())
	{
		return KnightGASFailure(EKnightError::InvalidData, TEXT("GrantAbilitySet"));
	}
	const FKnightResult Validation = Definition->ValidateAbilitySet();
	if (!Validation.IsSuccess())
	{
		return Validation;
	}
	if (const FKnightOwnedAbilityGrant* Existing = OwnedGrants.Find(SourceKey))
	{
		if (Existing->Handle.AbilitySetId == Definition->GetPrimaryAssetId())
		{
			OutHandle = Existing->Handle;
			return FKnightResult::Success();
		}
		return KnightGASFailure(EKnightError::Conflict, TEXT("AbilityGrantSourceConflict"));
	}
	FKnightOwnedAbilityGrant Grant;
	Grant.Handle.GrantId = FGuid::NewGuid();
	Grant.Handle.SourceKey = SourceKey;
	Grant.Handle.AbilitySetId = Definition->GetPrimaryAssetId();
	Grant.Definition = const_cast<UKnightAbilitySetDefinition*>(Definition);
	for (const FKnightAbilitySetEntry& Entry : Definition->GetAbilities())
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
			return KnightGASFailure(EKnightError::InvalidState, TEXT("AbilityGrantRejected"));
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
			return KnightGASFailure(EKnightError::InvalidState, TEXT("AbilitySetEffectRejected"));
		}
		Grant.EffectHandles.Add(Handle);
	}
	OutHandle = Grant.Handle;
	OwnedGrants.Add(SourceKey, MoveTemp(Grant));
	OwnedGrantCount = OwnedGrants.Num();
	OnAbilityGrantsChanged.Broadcast();
	GetOwner()->ForceNetUpdate();
	return FKnightResult::Success();
}

FKnightResult UKnightAbilitySystemComponent::RevokeAbilitySet(const FKnightAbilitySetHandle& Handle)
{
	if (IsMutationFenced() && !bProjectionEndingPlay)
	{
		return KnightGASFailure(EKnightError::Conflict, TEXT("RevokeProjectionFence"));
	}
	if (!IsValid(GetOwner()) || !GetOwner()->HasAuthority())
	{
		return KnightGASFailure(EKnightError::NotAuthority, TEXT("RevokeAbilitySet"));
	}
	if (const FKnightAbilitySetHandle* Revoked = LastRevokedGrants.Find(Handle.SourceKey))
	{
		if (Handle.IsValid() && Revoked->GrantId == Handle.GrantId && Revoked->AbilitySetId == Handle.AbilitySetId)
		{
			return FKnightResult::Success();
		}
	}
	FKnightOwnedAbilityGrant* Grant = OwnedGrants.Find(Handle.SourceKey);
	if (!Handle.IsValid() || Grant == nullptr || Grant->Handle.GrantId != Handle.GrantId ||
		Grant->Handle.AbilitySetId != Handle.AbilitySetId)
	{
		return KnightGASFailure(EKnightError::NotOwned, TEXT("RevokeAbilitySetToken"));
	}
	const FKnightOwnedAbilityGrant Removed = *Grant;
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
	return FKnightResult::Success();
}

int32 UKnightAbilitySystemComponent::GetOwnedGrantCount() const
{
	return OwnedGrantCount;
}
void UKnightAbilitySystemComponent::OnRep_OwnedGrantCount()
{
	OnAbilityGrantsChanged.Broadcast();
}

void UKnightAbilitySystemComponent::HandleControllerChanged()
{
	CancelPendingProjection();
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

void UKnightAbilitySystemComponent::ClearActorInfo()
{
	CancelPendingProjection();
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

FKnightResult UKnightAbilitySystemComponent::RequestAbilityInput(const FGameplayTag& InputTag,
	bool bPressed, AActor* Target)
{
	if (!IsValid(GetOwner()) || !GetOwner()->HasAuthority())
	{
		return KnightGASFailure(EKnightError::NotAuthority, TEXT("AbilityInput"));
	}
	if (!InputTag.IsValid())
	{
		return KnightGASFailure(EKnightError::InvalidData, TEXT("AbilityInputTag"));
	}
	if (bPressed && IsMutationFenced())
	{
		return KnightGASFailure(EKnightError::Conflict, TEXT("AbilityProjectionFence"));
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
			const UKnightGameplayAbility* Ability = Cast<UKnightGameplayAbility>(Spec.Ability);
			if (Spec.IsActive() && (Ability == nullptr || Ability->ShouldCancelOnInputRelease()))
			{
				CancelAbilityHandle(Spec.Handle);
			}
			return FKnightResult::Success();
		}
		if (LifeComponent == nullptr || !LifeComponent->IsAlive())
		{
			return KnightGASFailure(EKnightError::InvalidState, TEXT("AbilitySourceNotAlive"));
		}
		if (Spec.InputPressed && Spec.IsActive())
		{
			return FKnightResult::Success();
		}
		Spec.InputPressed = true;
		if (Spec.IsActive())
		{
			AbilitySpecInputPressed(Spec);
			return FKnightResult::Success();
		}
		FGameplayEventData Payload;
		Payload.EventTag = InputTag;
		Payload.Instigator = GetOwner();
		Payload.Target = IsValid(Target) ? Target : FindAbilityTarget(InputTag);
		LastAbilityResult = KnightGASFailure(EKnightError::InvalidState, TEXT("AbilityActivationRejected"));
		if (!InternalTryActivateAbility(Spec.Handle, FPredictionKey(), nullptr, nullptr, &Payload))
		{
			Spec.InputPressed = false;
			ReportAbilityResult(LastAbilityResult);
		}
		return LastAbilityResult;
	}
	return KnightGASFailure(EKnightError::NotFound, TEXT("InputAbilityNotGranted"));
}

AActor* UKnightAbilitySystemComponent::FindAbilityTarget(const FGameplayTag& InputTag) const
{
	if (!IsValid(GetOwner()) || GetWorld() == nullptr)
	{
		return nullptr;
	}
	const bool bRevive = InputTag == KnightGameplayTags::Input_Party_Revive;
	float BestDistanceSquared = TNumericLimits<float>::Max();
	AActor* Best = nullptr;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* Candidate = *It;
		if (Candidate == GetOwner())
		{
			continue;
		}
		const UKnightAbilitySystemComponent* CandidateASC = Cast<UKnightAbilitySystemComponent>(
			UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Candidate, true));
		if (CandidateASC == nullptr || CandidateASC->GetLifeComponent() == nullptr)
		{
			continue;
		}
		const bool bEligible = bRevive
			? CandidateASC->HasMatchingGameplayTag(KnightGameplayTags::Role_Party_Knight) &&
				CandidateASC->GetLifeComponent()->IsDowned()
			: CandidateASC->HasMatchingGameplayTag(KnightGameplayTags::Role_Enemy) &&
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

FKnightResult UKnightAbilitySystemComponent::ApplyDamage(AActor* Source, AActor* Target, float Amount)
{
	return ApplyCombatDamage(Source, Target, Amount, 0.0f, false);
}

FKnightResult UKnightAbilitySystemComponent::ApplyCombatDamage(AActor* Source, AActor* Target,
	float Amount, float PoiseDamage, bool bCanDefend, const FHitResult* Hit)
{
	if (!IsValid(Source) || !IsValid(Target) || Source->GetWorld() != Target->GetWorld() ||
		!FMath::IsFinite(Amount) || Amount <= 0.0f || Amount > 1000000.0f ||
		!FMath::IsFinite(PoiseDamage) || PoiseDamage < 0.0f || PoiseDamage > 1000000.0f ||
		(Hit != nullptr && Hit->GetActor() != Target))
	{
		return KnightGASFailure(EKnightError::InvalidData, TEXT("DamageRequest"));
	}
	if (!Source->HasAuthority() || !Target->HasAuthority())
	{
		return KnightGASFailure(EKnightError::NotAuthority, TEXT("DamageAuthority"));
	}
	UKnightAbilitySystemComponent* SourceASC = Cast<UKnightAbilitySystemComponent>(
		UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Source, true));
	UKnightAbilitySystemComponent* TargetASC = Cast<UKnightAbilitySystemComponent>(
		UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target, true));
	if (SourceASC == nullptr || TargetASC == nullptr || TargetASC->GetLifeComponent() == nullptr ||
		TargetASC->GetLifeComponent()->IsDead())
	{
		return KnightGASFailure(EKnightError::InvalidState, TEXT("DamageTargetUnavailable"));
	}
	if (SourceASC->IsMutationFenced() || TargetASC->IsMutationFenced())
	{
		return KnightGASFailure(EKnightError::Conflict, TEXT("DamageProjectionFence"));
	}
	FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
	if (Hit != nullptr)
	{
		Context.AddHitResult(*Hit, true);
	}
	FGameplayEffectSpecHandle Spec = SourceASC->MakeOutgoingSpec(UKnightDamageEffect::StaticClass(), 1.0f, Context);
	if (!Spec.IsValid())
	{
		return KnightGASFailure(EKnightError::InvalidState, TEXT("DamageSpec"));
	}
	Spec.Data->SetSetByCallerMagnitude(KnightGameplayTags::Data_Damage, Amount);
	Spec.Data->SetSetByCallerMagnitude(KnightGameplayTags::Data_DamagePoise, PoiseDamage);
	Spec.Data->SetSetByCallerMagnitude(KnightGameplayTags::Data_DamageCanDefend, bCanDefend ? 1.0f : 0.0f);
	return SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC).WasSuccessfullyApplied()
		? FKnightResult::Success() : KnightGASFailure(EKnightError::InvalidState, TEXT("DamageEffectRejected"));
}

void UKnightAbilitySystemComponent::GetAdditionalActivationTagRequirements(const FGameplayTagContainer& AbilityTags,
	FGameplayTagContainer& OutRequiredTags, FGameplayTagContainer& OutBlockedTags) const
{
	for (const TPair<FName, FKnightOwnedAbilityGrant>& Entry : OwnedGrants)
	{
		for (const FKnightTagRelationship& Relationship : Entry.Value.Definition->GetRelationships())
		{
			if (AbilityTags.HasTag(Relationship.AbilityTag))
			{
				OutRequiredTags.AppendTags(Relationship.ActivationRequiredTags);
				OutBlockedTags.AppendTags(Relationship.ActivationBlockedTags);
			}
		}
	}
}

void UKnightAbilitySystemComponent::ApplyAbilityBlockAndCancelTags(const FGameplayTagContainer& AbilityTags,
	UGameplayAbility* RequestingAbility, bool bEnableBlockTags, const FGameplayTagContainer& BlockTags,
	bool bExecuteCancelTags, const FGameplayTagContainer& CancelTags)
{
	FGameplayTagContainer ExpandedBlock = BlockTags;
	FGameplayTagContainer ExpandedCancel = CancelTags;
	for (const TPair<FName, FKnightOwnedAbilityGrant>& Entry : OwnedGrants)
	{
		for (const FKnightTagRelationship& Relationship : Entry.Value.Definition->GetRelationships())
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

void UKnightAbilitySystemComponent::ReportAbilityResult(const FKnightResult& Result)
{
	LastAbilityResult = Result;
	OnAbilityResult.Broadcast(Result);
}

void UKnightAbilitySystemComponent::RegisterNativeGameplayCues()
{
	UGameplayCueSet* Set = UAbilitySystemGlobals::Get().GetGameplayCueManager()->GetRuntimeCueSet();
	if (Set == nullptr)
	{
		return;
	}
	TArray<FGameplayCueReferencePair> Cues;
	for (const FGameplayTag Tag : {FGameplayTag(KnightGameplayTags::GameplayCue_Damage),
		FGameplayTag(KnightGameplayTags::GameplayCue_Downed), FGameplayTag(KnightGameplayTags::GameplayCue_Revive),
		FGameplayTag(KnightGameplayTags::GameplayCue_Parry), FGameplayTag(KnightGameplayTags::GameplayCue_Stagger),
		FGameplayTag(KnightGameplayTags::GameplayCue_Rune)})
	{
		if (!Set->GameplayCueDataMap.Contains(Tag))
		{
			Cues.Emplace(Tag, FSoftObjectPath(UKnightGameplayCueNotify::StaticClass()));
		}
	}
	Set->AddCues(Cues);
}

void UKnightAbilitySystemComponent::RecordGameplayCue(FGameplayTag Tag, EGameplayCueEvent::Type EventType)
{
	FKnightCueDiagnostics& Diagnostics = CueDiagnostics.FindOrAdd(Tag);
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

FKnightCueDiagnostics UKnightAbilitySystemComponent::GetCueDiagnostics(FGameplayTag Tag) const
{
	const FKnightCueDiagnostics* Diagnostics = CueDiagnostics.Find(Tag);
	return Diagnostics != nullptr ? *Diagnostics : FKnightCueDiagnostics();
}

void UKnightAbilitySystemComponent::PresentGameplayCue(FGameplayTag Tag, EGameplayCueEvent::Type EventType)
{
	if (GetWorld() == nullptr || GetWorld()->GetNetMode() == NM_DedicatedServer || !IsValid(GetOwner()))
	{
		return;
	}
	CueVisuals.RemoveAll([](const TObjectPtr<AKnightCueVisualActor>& Visual)
		{
			return !IsValid(Visual);
		});
	if (EventType == EGameplayCueEvent::Removed)
	{
		for (AKnightCueVisualActor* Visual : CueVisuals)
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
		for (const AKnightCueVisualActor* Visual : CueVisuals)
		{
			if (Visual->GetCueTag() == Tag && Visual->IsPersistent())
			{
				return;
			}
		}
	}
	AKnightCueVisualActor* Visual = GetWorld()->SpawnActor<AKnightCueVisualActor>();
	if (Visual != nullptr)
	{
		Visual->Configure(GetOwner(), Tag, bPersistent);
		CueVisuals.Add(Visual);
	}
}

void UKnightAbilitySystemComponent::PublishDamageConfirmation(AActor* Source, float AppliedDamage)
{
	if (GetOwner() == nullptr || !GetOwner()->HasAuthority() || GetWorld() == nullptr)
	{
		return;
	}
	UKnightEventSubsystem* Bus = GetWorld()->GetSubsystem<UKnightEventSubsystem>();
	if (Bus == nullptr)
	{
		return;
	}
	FKnightDamageConfirmedEvent Event;
	Event.ConfirmationId = FGuid::NewGuid();
	Event.Revision = ++DamageRevision;
	const UKnightAbilitySystemComponent* SourceASC = Cast<UKnightAbilitySystemComponent>(
		UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Source, true));
	Event.SourceId = SourceASC != nullptr ? SourceASC->GetEntityId() : EntityId;
	Event.TargetId = EntityId;
	Event.AppliedDamage = AppliedDamage;
	Event.RemainingHealth = Attributes != nullptr ? Attributes->GetHealth() : 0.0f;
	Bus->PublishConfirmed(Event);
}

void UKnightAbilitySystemComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bProjectionEndingPlay = true;
	OnSourceProjectionChanged.Clear();
	OnAbilityResult.Clear();
	OnAbilityGrantsChanged.Clear();
	OnGameplayCueProcessed.Clear();
	CancelPendingProjection();
	TArray<FKnightProjectionRuntime> Sources;
	ProjectionSources.GenerateValueArray(Sources);
	ProjectionSources.Reset();
	for (const FKnightProjectionRuntime& Source : Sources)
	{
		for (const FKnightOwnedAbilityGrant& Grant : Source.OptionalGrants)
		{
			OwnedGrants.Remove(Grant.Handle.SourceKey);
		}
		RemoveProjectionRuntime(Source);
	}
	PublishedProjections.Reset();
	CancelAllAbilities();
	TArray<FKnightAbilitySetHandle> Handles;
	for (const TPair<FName, FKnightOwnedAbilityGrant>& Entry : OwnedGrants)
	{
		Handles.Add(Entry.Value.Handle);
	}
	for (const FKnightAbilitySetHandle& Handle : Handles)
	{
		RevokeAbilitySet(Handle);
	}
	for (AKnightCueVisualActor* Visual : CueVisuals)
	{
		if (IsValid(Visual))
		{
			Visual->Destroy();
		}
	}
	CueVisuals.Reset();
	Super::EndPlay(EndPlayReason);
}
