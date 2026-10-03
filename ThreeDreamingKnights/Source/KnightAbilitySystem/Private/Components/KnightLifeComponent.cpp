#include "Components/KnightLifeComponent.h"

#include "AbilitySystemGlobals.h"
#include "Components/KnightAbilitySystemComponent.h"
#include "Engine/World.h"
#include "Events/KnightEventSubsystem.h"
#include "GameplayEffect.h"
#include "Net/UnrealNetwork.h"
#include "Tags/KnightGameplayTags.h"
#include "TimerManager.h"

namespace
{
	FKnightResult KnightLifeFailure(EKnightError Error, FName Context)
	{
		FText Message = NSLOCTEXT("KnightGAS", "LifeCommandRejected", "The requested life-state action is unavailable.");
		if (Error == EKnightError::OutOfRange)
		{
			Message = NSLOCTEXT("KnightGAS", "ReviveMoveCloser", "Move closer to the downed companion.");
		}
		else if (Error == EKnightError::Conflict)
		{
			Message = NSLOCTEXT("KnightGAS", "ReviveReserved", "Another companion is already reviving this target.");
		}
		else if (Error == EKnightError::NotOwned)
		{
			Message = NSLOCTEXT("KnightGAS", "ReviveReservationLost", "This revive channel no longer owns the target.");
		}
		else if (Context == FName(TEXT("ReviveLineOfSight")))
		{
			Message = NSLOCTEXT("KnightGAS", "ReviveVisibilityBlocked", "An obstacle blocks the path to the downed companion.");
		}
		else if (Context == FName(TEXT("ReviveDuration")))
		{
			Message = NSLOCTEXT("KnightGAS", "ReviveKeepHolding", "Keep holding revive until the channel completes.");
		}
		return FKnightResult::Failure(Error, Context, Message);
	}
}

UKnightLifeComponent::UKnightLifeComponent()
{
	SetIsReplicatedByDefault(true);
	PrimaryComponentTick.bCanEverTick = false;
}

void UKnightLifeComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UKnightLifeComponent, LifeState);
	DOREPLIFETIME(UKnightLifeComponent, BleedoutEndServerTime);
}

FKnightResult UKnightLifeComponent::InitializeLife(UKnightAbilitySystemComponent* InAbilitySystem)
{
	if (!IsValid(InAbilitySystem) || InAbilitySystem->GetOwner() != GetOwner() ||
		InAbilitySystem->GetKnightAttributes() == nullptr)
	{
		return KnightLifeFailure(EKnightError::InvalidData, TEXT("InitializeLife"));
	}
	if (AbilitySystem.IsValid() && AbilitySystem != InAbilitySystem)
	{
		return KnightLifeFailure(EKnightError::Conflict, TEXT("ImmutableLifeASC"));
	}
	AbilitySystem = InAbilitySystem;
	if (!HealthChangedHandle.IsValid())
	{
		HealthChangedHandle = InAbilitySystem->GetGameplayAttributeValueChangeDelegate(
			UKnightAttributeSet::GetHealthAttribute()).AddUObject(this, &ThisClass::HandleHealthChanged);
	}
	if (GetOwner()->HasAuthority())
	{
		SynchronizeLifeTags();
		if (LifeState == EKnightLifeState::Alive && InAbilitySystem->GetKnightAttributes()->GetHealth() <= 0.0f)
		{
			ChangeLifeState(InAbilitySystem->HasMatchingGameplayTag(KnightGameplayTags::Role_Party_Knight)
				? EKnightLifeState::Downed : EKnightLifeState::Dead);
		}
	}
	return FKnightResult::Success();
}

EKnightLifeState UKnightLifeComponent::GetLifeState() const
{
	return LifeState;
}
bool UKnightLifeComponent::IsAlive() const
{
	return LifeState == EKnightLifeState::Alive;
}
bool UKnightLifeComponent::IsDowned() const
{
	return LifeState == EKnightLifeState::Downed;
}
bool UKnightLifeComponent::IsDead() const
{
	return LifeState == EKnightLifeState::Dead;
}
float UKnightLifeComponent::GetReviveDuration() const
{
	return ReviveDuration;
}
float UKnightLifeComponent::GetReviveRange() const
{
	return ReviveRange;
}
double UKnightLifeComponent::GetBleedoutEndServerTime() const
{
	return BleedoutEndServerTime;
}
bool UKnightLifeComponent::IsReviveInProgress() const
{
	return ReviveToken.IsValid() && ActiveReviver.IsValid();
}
AActor* UKnightLifeComponent::GetActiveReviver() const
{
	return ActiveReviver.Get();
}

void UKnightLifeComponent::HandleHealthChanged(const FOnAttributeChangeData& Change)
{
	if (GetOwner() == nullptr || !GetOwner()->HasAuthority() || !AbilitySystem.IsValid())
	{
		return;
	}
	if (Change.NewValue <= 0.0f && IsAlive())
	{
		ChangeLifeState(AbilitySystem->HasMatchingGameplayTag(KnightGameplayTags::Role_Party_Knight)
			? EKnightLifeState::Downed : EKnightLifeState::Dead);
	}
}

void UKnightLifeComponent::SynchronizeLifeTags()
{
	if (!AbilitySystem.IsValid() || GetOwner() == nullptr || !GetOwner()->HasAuthority())
	{
		return;
	}
	AbilitySystem->SetLooseGameplayTagCount(KnightGameplayTags::State_Life_Alive, IsAlive() ? 1 : 0,
		EGameplayTagReplicationState::TagAndCountToAll);
	AbilitySystem->SetLooseGameplayTagCount(KnightGameplayTags::State_Life_Downed, IsDowned() ? 1 : 0,
		EGameplayTagReplicationState::TagAndCountToAll);
	AbilitySystem->SetLooseGameplayTagCount(KnightGameplayTags::State_Life_Dead, IsDead() ? 1 : 0,
		EGameplayTagReplicationState::TagAndCountToAll);
}

void UKnightLifeComponent::ChangeLifeState(EKnightLifeState NewState)
{
	if (NewState == LifeState || !AbilitySystem.IsValid() || GetWorld() == nullptr ||
		GetOwner() == nullptr || !GetOwner()->HasAuthority() || GetWorld()->bIsTearingDown)
	{
		return;
	}
	const EKnightLifeState Previous = LifeState;
	LifeState = NewState;
	SynchronizeLifeTags();
	GetWorld()->GetTimerManager().ClearTimer(BleedoutTimer);
	BleedoutEndServerTime = 0.0;
	if (!IsAlive())
	{
		AbilitySystem->HandleControllerChanged();
	}
	if (Previous == EKnightLifeState::Downed)
	{
		AbilitySystem->RemoveGameplayCue(KnightGameplayTags::GameplayCue_Downed);
		ReleaseReviveReservation(NewState != EKnightLifeState::Alive);
	}
	if (IsDowned())
	{
		BleedoutEndServerTime = GetWorld()->GetTimeSeconds() + BleedoutDuration;
		GetWorld()->GetTimerManager().SetTimer(BleedoutTimer, this, &ThisClass::HandleBleedout,
			BleedoutDuration, false);
		AbilitySystem->AddGameplayCue(KnightGameplayTags::GameplayCue_Downed);
	}
	OnLifeStateChanged.Broadcast(Previous, LifeState);
	if (UKnightEventSubsystem* Bus = GetWorld()->GetSubsystem<UKnightEventSubsystem>())
	{
		FKnightLifeStateChangedEvent Event;
		Event.ConfirmationId = FGuid::NewGuid();
		Event.Revision = ++LifeRevision;
		Event.SourceId = AbilitySystem->GetEntityId();
		Event.Previous = Previous;
		Event.Current = LifeState;
		Bus->PublishConfirmed(Event);
	}
	GetOwner()->ForceNetUpdate();
}

void UKnightLifeComponent::HandleBleedout()
{
	if (IsDowned())
	{
		ChangeLifeState(EKnightLifeState::Dead);
	}
}

void UKnightLifeComponent::NotifyDamageWhileDowned()
{
	if (IsDowned())
	{
		ChangeLifeState(EKnightLifeState::Dead);
	}
}

FKnightResult UKnightLifeComponent::ValidateReviveSource(AActor* Reviver) const
{
	if (!IsValid(GetOwner()) || !GetOwner()->HasAuthority() || !IsValid(Reviver) || !Reviver->HasAuthority())
	{
		return KnightLifeFailure(EKnightError::NotAuthority, TEXT("ReviveAuthority"));
	}
	if (GetWorld() == nullptr || GetWorld()->bIsTearingDown || Reviver->GetWorld() != GetWorld() ||
		Reviver == GetOwner() || !AbilitySystem.IsValid() || !IsDowned() ||
		!AbilitySystem->HasMatchingGameplayTag(KnightGameplayTags::Role_Party_Knight))
	{
		return KnightLifeFailure(EKnightError::InvalidState, TEXT("ReviveTarget"));
	}
	const UKnightAbilitySystemComponent* SourceASC = Cast<UKnightAbilitySystemComponent>(
		UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Reviver, true));
	if (SourceASC == nullptr || SourceASC->GetLifeComponent() == nullptr ||
		!SourceASC->GetLifeComponent()->IsAlive() ||
		!SourceASC->HasMatchingGameplayTag(KnightGameplayTags::Role_Party_Knight))
	{
		return KnightLifeFailure(EKnightError::InvalidState, TEXT("ReviveSource"));
	}
	if (FVector::DistSquared(Reviver->GetActorLocation(), GetOwner()->GetActorLocation()) > FMath::Square(ReviveRange))
	{
		return KnightLifeFailure(EKnightError::OutOfRange, TEXT("ReviveRange"));
	}
	FCollisionQueryParams Query(SCENE_QUERY_STAT(KnightReviveVisibility), false, Reviver);
	Query.AddIgnoredActor(GetOwner());
	FHitResult Hit;
	if (GetWorld()->LineTraceSingleByChannel(Hit, Reviver->GetActorLocation(), GetOwner()->GetActorLocation(),
		ECC_Visibility, Query))
	{
		return KnightLifeFailure(EKnightError::InvalidState, TEXT("ReviveLineOfSight"));
	}
	return FKnightResult::Success();
}

FKnightResult UKnightLifeComponent::TryBeginRevive(AActor* Reviver, FGuid& OutToken)
{
	OutToken.Invalidate();
	const FKnightResult Validation = ValidateReviveSource(Reviver);
	if (!Validation.IsSuccess())
	{
		return Validation;
	}
	if (ReviveToken.IsValid())
	{
		if (ActiveReviver.IsValid())
		{
			return KnightLifeFailure(EKnightError::Conflict, TEXT("ReviveReserved"));
		}
		ReleaseReviveReservation(false);
	}
	ReviveToken = FGuid::NewGuid();
	ActiveReviver = Reviver;
	ReviveStartedServerTime = GetWorld()->GetTimeSeconds();
	OutToken = ReviveToken;
	return FKnightResult::Success();
}

FKnightResult UKnightLifeComponent::TryCompleteRevive(AActor* Reviver, const FGuid& Token)
{
	if (!Token.IsValid() || Token != ReviveToken || ActiveReviver != Reviver)
	{
		return KnightLifeFailure(EKnightError::NotOwned, TEXT("ReviveReservationToken"));
	}
	const FKnightResult Validation = ValidateReviveSource(Reviver);
	if (!Validation.IsSuccess())
	{
		ReleaseReviveReservation(false);
		return Validation;
	}
	if (GetWorld()->GetTimeSeconds() - ReviveStartedServerTime + 0.001 < ReviveDuration)
	{
		return KnightLifeFailure(EKnightError::InvalidState, TEXT("ReviveDuration"));
	}
	UGameplayEffect* Effect = NewObject<UGameplayEffect>(GetTransientPackage());
	Effect->DurationPolicy = EGameplayEffectDurationType::Instant;
	FGameplayModifierInfo& Modifier = Effect->Modifiers.AddDefaulted_GetRef();
	Modifier.Attribute = UKnightAttributeSet::GetHealthAttribute();
	Modifier.ModifierOp = EGameplayModOp::Override;
	Modifier.ModifierMagnitude = FScalableFloat(AbilitySystem->GetKnightAttributes()->GetMaxHealth() * ReviveHealthFraction);
	const FGameplayEffectSpec Spec(Effect, AbilitySystem->MakeEffectContext(), 1.0f);
	if (!AbilitySystem->ApplyGameplayEffectSpecToSelf(Spec).WasSuccessfullyApplied())
	{
		ReleaseReviveReservation(false);
		return KnightLifeFailure(EKnightError::InvalidState, TEXT("ReviveHealRejected"));
	}
	ReleaseReviveReservation(false);
	ChangeLifeState(EKnightLifeState::Alive);
	AbilitySystem->ExecuteGameplayCue(KnightGameplayTags::GameplayCue_Revive);
	return FKnightResult::Success();
}

FKnightResult UKnightLifeComponent::CancelRevive(AActor* Reviver, const FGuid& Token)
{
	if (GetOwner() == nullptr || !GetOwner()->HasAuthority())
	{
		return KnightLifeFailure(EKnightError::NotAuthority, TEXT("CancelRevive"));
	}
	if (!Token.IsValid() || Token != ReviveToken || ActiveReviver != Reviver)
	{
		return KnightLifeFailure(EKnightError::NotOwned, TEXT("CancelReviveToken"));
	}
	ReleaseReviveReservation(false);
	return FKnightResult::Success();
}

void UKnightLifeComponent::ReleaseReviveReservation(bool bCancelSourceAbility)
{
	AActor* Source = ActiveReviver.Get();
	ActiveReviver.Reset();
	ReviveToken.Invalidate();
	ReviveStartedServerTime = 0.0;
	if (bCancelSourceAbility && IsValid(Source))
	{
		if (UAbilitySystemComponent* SourceASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Source, true))
		{
			FGameplayTagContainer ReviveTags(KnightGameplayTags::Ability_Party_Revive);
			SourceASC->CancelAbilities(&ReviveTags);
		}
	}
}

FKnightResult UKnightLifeComponent::ResetForCheckpoint()
{
	if (!IsValid(GetOwner()) || !GetOwner()->HasAuthority() || !AbilitySystem.IsValid() || GetWorld() == nullptr ||
		GetWorld()->bIsTearingDown)
	{
		return KnightLifeFailure(EKnightError::InvalidState, TEXT("ResetLifeCheckpoint"));
	}
	ReleaseReviveReservation(true);
	AbilitySystem->HandleControllerChanged();
	const FKnightResult Reset = AbilitySystem->ResetAttributesForCheckpoint();
	if (!Reset.IsSuccess())
	{
		return Reset;
	}
	GetWorld()->GetTimerManager().ClearTimer(BleedoutTimer);
	BleedoutEndServerTime = 0.0;
	ChangeLifeState(EKnightLifeState::Alive);
	SynchronizeLifeTags();
	return FKnightResult::Success();
}

FKnightResult UKnightLifeComponent::TrySetLifeTuning(float InBleedoutDuration, float InReviveDuration,
	float InReviveRange, float InReviveHealthFraction)
{
	if (!FMath::IsFinite(InBleedoutDuration) || InBleedoutDuration < 0.1f ||
		!FMath::IsFinite(InReviveDuration) || InReviveDuration < 0.1f ||
		!FMath::IsFinite(InReviveRange) || InReviveRange < 1.0f ||
		!FMath::IsFinite(InReviveHealthFraction) || InReviveHealthFraction <= 0.0f || InReviveHealthFraction > 1.0f)
	{
		return KnightLifeFailure(EKnightError::InvalidData, TEXT("LifeTuning"));
	}
	if (!IsAlive() || IsReviveInProgress())
	{
		return KnightLifeFailure(EKnightError::InvalidState, TEXT("LifeTuningInProgress"));
	}
	BleedoutDuration = InBleedoutDuration;
	ReviveDuration = InReviveDuration;
	ReviveRange = InReviveRange;
	ReviveHealthFraction = InReviveHealthFraction;
	return FKnightResult::Success();
}

void UKnightLifeComponent::OnRep_LifeState(EKnightLifeState Previous)
{
	OnLifeStateChanged.Broadcast(Previous, LifeState);
}

void UKnightLifeComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	OnLifeStateChanged.Clear();
	if (GetWorld() != nullptr)
	{
		GetWorld()->GetTimerManager().ClearTimer(BleedoutTimer);
	}
	if (AbilitySystem.IsValid() && HealthChangedHandle.IsValid())
	{
		AbilitySystem->GetGameplayAttributeValueChangeDelegate(UKnightAttributeSet::GetHealthAttribute()).Remove(HealthChangedHandle);
	}
	HealthChangedHandle.Reset();
	ReleaseReviveReservation(true);
	AbilitySystem.Reset();
	Super::EndPlay(EndPlayReason);
}
