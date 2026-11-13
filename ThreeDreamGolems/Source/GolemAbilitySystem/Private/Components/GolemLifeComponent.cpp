#include "Components/GolemLifeComponent.h"

#include "AbilitySystemGlobals.h"
#include "Components/GolemAbilitySystemComponent.h"
#include "Engine/World.h"
#include "Events/GolemEventSubsystem.h"
#include "GameplayEffect.h"
#include "Misc/ScopeExit.h"
#include "Net/UnrealNetwork.h"
#include "Tags/GolemGameplayTags.h"
#include "TimerManager.h"

namespace
{
	FGolemResult GolemLifeFailure(EGolemError Error, FName Context)
	{
		FText Message = NSLOCTEXT("GolemGAS", "LifeCommandRejected", "The requested life-state action is unavailable.");
		if (Error == EGolemError::OutOfRange)
		{
			Message = NSLOCTEXT("GolemGAS", "ReviveMoveCloser", "Move closer to the downed companion.");
		}
		else if (Error == EGolemError::Conflict)
		{
			Message = NSLOCTEXT("GolemGAS", "ReviveReserved", "Another companion is already reviving this target.");
		}
		else if (Error == EGolemError::NotOwned)
		{
			Message = NSLOCTEXT("GolemGAS", "ReviveReservationLost", "This revive channel no longer owns the target.");
		}
		else if (Context == FName(TEXT("ReviveLineOfSight")))
		{
			Message = NSLOCTEXT("GolemGAS", "ReviveVisibilityBlocked", "An obstacle blocks the path to the downed companion.");
		}
		else if (Context == FName(TEXT("ReviveDuration")))
		{
			Message = NSLOCTEXT("GolemGAS", "ReviveKeepHolding", "Keep holding revive until the channel completes.");
		}
		return FGolemResult::Failure(Error, Context, Message);
	}
}

UGolemLifeComponent::UGolemLifeComponent()
{
	SetIsReplicatedByDefault(true);
	PrimaryComponentTick.bCanEverTick = false;
}

void UGolemLifeComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UGolemLifeComponent, LifeState);
	DOREPLIFETIME(UGolemLifeComponent, BleedoutEndServerTime);
}

FGolemResult UGolemLifeComponent::InitializeLife(UGolemAbilitySystemComponent* InAbilitySystem)
{
	if (!IsValid(InAbilitySystem) || InAbilitySystem->GetOwner() != GetOwner() ||
		InAbilitySystem->GetGolemAttributes() == nullptr)
	{
		return GolemLifeFailure(EGolemError::InvalidData, TEXT("InitializeLife"));
	}
	if (AbilitySystem.IsValid() && AbilitySystem != InAbilitySystem)
	{
		return GolemLifeFailure(EGolemError::Conflict, TEXT("ImmutableLifeASC"));
	}
	AbilitySystem = InAbilitySystem;
	if (!HealthChangedHandle.IsValid())
	{
		HealthChangedHandle = InAbilitySystem->GetGameplayAttributeValueChangeDelegate(
			UGolemAttributeSet::GetHealthAttribute()).AddUObject(this, &ThisClass::HandleHealthChanged);
	}
	if (GetOwner()->HasAuthority())
	{
		SynchronizeLifeTags();
		if (LifeState == EGolemLifeState::Alive && InAbilitySystem->GetGolemAttributes()->GetHealth() <= 0.0f)
		{
			ChangeLifeState(InAbilitySystem->HasMatchingGameplayTag(GolemGameplayTags::Role_Party_Golem)
				? EGolemLifeState::Downed : EGolemLifeState::Dead);
		}
	}
	return FGolemResult::Success();
}

EGolemLifeState UGolemLifeComponent::GetLifeState() const
{
	return LifeState;
}
bool UGolemLifeComponent::IsAlive() const
{
	return LifeState == EGolemLifeState::Alive;
}
bool UGolemLifeComponent::IsDowned() const
{
	return LifeState == EGolemLifeState::Downed;
}
bool UGolemLifeComponent::IsDead() const
{
	return LifeState == EGolemLifeState::Dead;
}
float UGolemLifeComponent::GetReviveDuration() const
{
	return ReviveDuration;
}
float UGolemLifeComponent::GetReviveRange() const
{
	return ReviveRange;
}
double UGolemLifeComponent::GetBleedoutEndServerTime() const
{
	return BleedoutEndServerTime;
}
bool UGolemLifeComponent::IsReviveInProgress() const
{
	return ReviveToken.IsValid() && ActiveReviver.IsValid();
}
AActor* UGolemLifeComponent::GetActiveReviver() const
{
	return ActiveReviver.Get();
}

void UGolemLifeComponent::HandleHealthChanged(const FOnAttributeChangeData& Change)
{
	if (GetOwner() == nullptr || !GetOwner()->HasAuthority() || !AbilitySystem.IsValid())
	{
		return;
	}
	if (Change.NewValue <= 0.0f && IsAlive())
	{
		ChangeLifeState(AbilitySystem->HasMatchingGameplayTag(GolemGameplayTags::Role_Party_Golem)
			? EGolemLifeState::Downed : EGolemLifeState::Dead);
	}
}

void UGolemLifeComponent::SynchronizeLifeTags()
{
	if (!AbilitySystem.IsValid() || GetOwner() == nullptr || !GetOwner()->HasAuthority())
	{
		return;
	}
	AbilitySystem->SetLooseGameplayTagCount(GolemGameplayTags::State_Life_Alive, IsAlive() ? 1 : 0,
		EGameplayTagReplicationState::TagAndCountToAll);
	AbilitySystem->SetLooseGameplayTagCount(GolemGameplayTags::State_Life_Downed, IsDowned() ? 1 : 0,
		EGameplayTagReplicationState::TagAndCountToAll);
	AbilitySystem->SetLooseGameplayTagCount(GolemGameplayTags::State_Life_Dead, IsDead() ? 1 : 0,
		EGameplayTagReplicationState::TagAndCountToAll);
}

void UGolemLifeComponent::ChangeLifeState(EGolemLifeState NewState)
{
	if (NewState == LifeState || !AbilitySystem.IsValid() || GetWorld() == nullptr ||
		GetOwner() == nullptr || !GetOwner()->HasAuthority() || GetWorld()->bIsTearingDown)
	{
		return;
	}
	const EGolemLifeState Previous = LifeState;
	LifeState = NewState;
	SynchronizeLifeTags();
	GetWorld()->GetTimerManager().ClearTimer(BleedoutTimer);
	BleedoutEndServerTime = 0.0;
	if (!IsAlive())
	{
		AbilitySystem->HandleControllerChanged();
	}
	if (Previous == EGolemLifeState::Downed)
	{
		AbilitySystem->RemoveGameplayCue(GolemGameplayTags::GameplayCue_Downed);
		ReleaseReviveReservation(NewState != EGolemLifeState::Alive);
	}
	if (IsDowned())
	{
		BleedoutEndServerTime = GetWorld()->GetTimeSeconds() + BleedoutDuration;
		GetWorld()->GetTimerManager().SetTimer(BleedoutTimer, this, &ThisClass::HandleBleedout,
			BleedoutDuration, false);
		AbilitySystem->AddGameplayCue(GolemGameplayTags::GameplayCue_Downed);
	}
	OnLifeStateChanged.Broadcast(Previous, LifeState);
	if (UGolemEventSubsystem* Bus = GetWorld()->GetSubsystem<UGolemEventSubsystem>())
	{
		FGolemLifeStateChangedEvent Event;
		Event.ConfirmationId = FGuid::NewGuid();
		Event.Revision = ++LifeRevision;
		Event.SourceId = AbilitySystem->GetEntityId();
		Event.Previous = Previous;
		Event.Current = LifeState;
		Bus->PublishConfirmed(Event);
	}
	GetOwner()->ForceNetUpdate();
}

void UGolemLifeComponent::HandleBleedout()
{
	if (IsDowned())
	{
		ChangeLifeState(EGolemLifeState::Dead);
	}
}

void UGolemLifeComponent::NotifyDamageWhileDowned()
{
	if (IsDowned())
	{
		ChangeLifeState(EGolemLifeState::Dead);
	}
}

FGolemResult UGolemLifeComponent::ValidateReviveSource(AActor* Reviver) const
{
	if (!IsValid(GetOwner()) || !GetOwner()->HasAuthority() || !IsValid(Reviver) || !Reviver->HasAuthority())
	{
		return GolemLifeFailure(EGolemError::NotAuthority, TEXT("ReviveAuthority"));
	}
	if (GetWorld() == nullptr || GetWorld()->bIsTearingDown || Reviver->GetWorld() != GetWorld() ||
		Reviver == GetOwner() || !AbilitySystem.IsValid() || !IsDowned() ||
		!AbilitySystem->HasMatchingGameplayTag(GolemGameplayTags::Role_Party_Golem))
	{
		return GolemLifeFailure(EGolemError::InvalidState, TEXT("ReviveTarget"));
	}
	const UGolemAbilitySystemComponent* SourceASC = Cast<UGolemAbilitySystemComponent>(
		UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Reviver, true));
	if (SourceASC == nullptr || SourceASC->GetLifeComponent() == nullptr ||
		!SourceASC->GetLifeComponent()->IsAlive() ||
		!SourceASC->HasMatchingGameplayTag(GolemGameplayTags::Role_Party_Golem))
	{
		return GolemLifeFailure(EGolemError::InvalidState, TEXT("ReviveSource"));
	}
	if (FVector::DistSquared(Reviver->GetActorLocation(), GetOwner()->GetActorLocation()) > FMath::Square(ReviveRange))
	{
		return GolemLifeFailure(EGolemError::OutOfRange, TEXT("ReviveRange"));
	}
	FCollisionQueryParams Query(SCENE_QUERY_STAT(GolemReviveVisibility), false, Reviver);
	Query.AddIgnoredActor(GetOwner());
	FHitResult Hit;
	if (GetWorld()->LineTraceSingleByChannel(Hit, Reviver->GetActorLocation(), GetOwner()->GetActorLocation(),
		ECC_Visibility, Query))
	{
		return GolemLifeFailure(EGolemError::InvalidState, TEXT("ReviveLineOfSight"));
	}
	return FGolemResult::Success();
}

FGolemResult UGolemLifeComponent::TryBeginRevive(AActor* Reviver, FGuid& OutToken)
{
	OutToken.Invalidate();
	const FGolemResult Validation = ValidateReviveSource(Reviver);
	if (!Validation.IsSuccess())
	{
		return Validation;
	}
	if (ReviveToken.IsValid())
	{
		if (ActiveReviver.IsValid())
		{
			return GolemLifeFailure(EGolemError::Conflict, TEXT("ReviveReserved"));
		}
		ReleaseReviveReservation(false);
	}
	ReviveToken = FGuid::NewGuid();
	ActiveReviver = Reviver;
	ReviveStartedServerTime = GetWorld()->GetTimeSeconds();
	OutToken = ReviveToken;
	return FGolemResult::Success();
}

FGolemResult UGolemLifeComponent::TryCompleteRevive(AActor* Reviver, const FGuid& Token)
{
	if (!Token.IsValid() || Token != ReviveToken || ActiveReviver != Reviver)
	{
		return GolemLifeFailure(EGolemError::NotOwned, TEXT("ReviveReservationToken"));
	}
	const FGolemResult Validation = ValidateReviveSource(Reviver);
	if (!Validation.IsSuccess())
	{
		ReleaseReviveReservation(false);
		return Validation;
	}
	if (GetWorld()->GetTimeSeconds() - ReviveStartedServerTime + 0.001 < ReviveDuration)
	{
		return GolemLifeFailure(EGolemError::InvalidState, TEXT("ReviveDuration"));
	}
	UGameplayEffect* Effect = NewObject<UGameplayEffect>(GetTransientPackage());
	Effect->DurationPolicy = EGameplayEffectDurationType::Instant;
	FGameplayModifierInfo& Modifier = Effect->Modifiers.AddDefaulted_GetRef();
	Modifier.Attribute = UGolemAttributeSet::GetHealthAttribute();
	Modifier.ModifierOp = EGameplayModOp::Override;
	Modifier.ModifierMagnitude = FScalableFloat(AbilitySystem->GetGolemAttributes()->GetMaxHealth() * ReviveHealthFraction);
	const FGameplayEffectSpec Spec(Effect, AbilitySystem->MakeEffectContext(), 1.0f);
	if (!AbilitySystem->ApplyGameplayEffectSpecToSelf(Spec).WasSuccessfullyApplied())
	{
		ReleaseReviveReservation(false);
		return GolemLifeFailure(EGolemError::InvalidState, TEXT("ReviveHealRejected"));
	}
	ReleaseReviveReservation(false);
	ChangeLifeState(EGolemLifeState::Alive);
	AbilitySystem->ExecuteGameplayCue(GolemGameplayTags::GameplayCue_Revive);
	return FGolemResult::Success();
}

FGolemResult UGolemLifeComponent::CancelRevive(AActor* Reviver, const FGuid& Token)
{
	if (GetOwner() == nullptr || !GetOwner()->HasAuthority())
	{
		return GolemLifeFailure(EGolemError::NotAuthority, TEXT("CancelRevive"));
	}
	if (!Token.IsValid() || Token != ReviveToken || ActiveReviver != Reviver)
	{
		return GolemLifeFailure(EGolemError::NotOwned, TEXT("CancelReviveToken"));
	}
	ReleaseReviveReservation(false);
	return FGolemResult::Success();
}

void UGolemLifeComponent::ReleaseReviveReservation(bool bCancelSourceAbility)
{
	AActor* Source = ActiveReviver.Get();
	ActiveReviver.Reset();
	ReviveToken.Invalidate();
	ReviveStartedServerTime = 0.0;
	if (bCancelSourceAbility && IsValid(Source))
	{
		if (UAbilitySystemComponent* SourceASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Source, true))
		{
			FGameplayTagContainer ReviveTags(GolemGameplayTags::Ability_Party_Revive);
			SourceASC->CancelAbilities(&ReviveTags);
		}
	}
}

FGolemResult UGolemLifeComponent::ResetForCheckpoint()
{
	if (!IsValid(GetOwner()) || !GetOwner()->HasAuthority() || !AbilitySystem.IsValid() || GetWorld() == nullptr ||
		GetWorld()->bIsTearingDown)
	{
		return GolemLifeFailure(EGolemError::InvalidState, TEXT("ResetLifeCheckpoint"));
	}
	ReleaseReviveReservation(true);
	AbilitySystem->HandleControllerChanged();
	const FGolemResult Reset = AbilitySystem->ResetAttributesForCheckpoint();
	if (!Reset.IsSuccess())
	{
		return Reset;
	}
	GetWorld()->GetTimerManager().ClearTimer(BleedoutTimer);
	BleedoutEndServerTime = 0.0;
	ChangeLifeState(EGolemLifeState::Alive);
	SynchronizeLifeTags();
	return FGolemResult::Success();
}

FGolemResult UGolemLifeComponent::ResetForCheckpointWithDeferredControllerRestart()
{
	if (bDeferControllerRestart)
	{
		return GolemLifeFailure(EGolemError::Conflict, TEXT("ResetLifeCheckpoint"));
	}
	bDeferControllerRestart = true;
	ON_SCOPE_EXIT
	{
		bDeferControllerRestart = false;
	};
	return ResetForCheckpoint();
}

bool UGolemLifeComponent::ShouldDeferControllerRestart() const
{
	return bDeferControllerRestart;
}

FGolemResult UGolemLifeComponent::TrySetLifeTuning(float InBleedoutDuration, float InReviveDuration,
	float InReviveRange, float InReviveHealthFraction)
{
	if (!FMath::IsFinite(InBleedoutDuration) || InBleedoutDuration < 0.1f ||
		!FMath::IsFinite(InReviveDuration) || InReviveDuration < 0.1f ||
		!FMath::IsFinite(InReviveRange) || InReviveRange < 1.0f ||
		!FMath::IsFinite(InReviveHealthFraction) || InReviveHealthFraction <= 0.0f || InReviveHealthFraction > 1.0f)
	{
		return GolemLifeFailure(EGolemError::InvalidData, TEXT("LifeTuning"));
	}
	if (!IsAlive() || IsReviveInProgress())
	{
		return GolemLifeFailure(EGolemError::InvalidState, TEXT("LifeTuningInProgress"));
	}
	BleedoutDuration = InBleedoutDuration;
	ReviveDuration = InReviveDuration;
	ReviveRange = InReviveRange;
	ReviveHealthFraction = InReviveHealthFraction;
	return FGolemResult::Success();
}

void UGolemLifeComponent::OnRep_LifeState(EGolemLifeState Previous)
{
	OnLifeStateChanged.Broadcast(Previous, LifeState);
}

void UGolemLifeComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	OnLifeStateChanged.Clear();
	if (GetWorld() != nullptr)
	{
		GetWorld()->GetTimerManager().ClearTimer(BleedoutTimer);
	}
	if (AbilitySystem.IsValid() && HealthChangedHandle.IsValid())
	{
		AbilitySystem->GetGameplayAttributeValueChangeDelegate(UGolemAttributeSet::GetHealthAttribute()).Remove(HealthChangedHandle);
	}
	HealthChangedHandle.Reset();
	ReleaseReviveReservation(true);
	AbilitySystem.Reset();
	Super::EndPlay(EndPlayReason);
}
