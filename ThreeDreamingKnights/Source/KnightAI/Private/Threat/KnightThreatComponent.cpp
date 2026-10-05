#include "Threat/KnightThreatComponent.h"

#include "AbilitySystemGlobals.h"
#include "Components/KnightAbilitySystemComponent.h"
#include "Components/KnightLifeComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Tags/KnightGameplayTags.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogKnightThreat, Log, All);

namespace
{
	FKnightResult KnightThreatFailure(EKnightError Error, FName Context)
	{
		return FKnightResult::Failure(Error, Context,
			NSLOCTEXT("KnightAI", "ThreatUnavailable", "Threat awareness could not be updated."));
	}
	UKnightAbilitySystemComponent* KnightThreatASC(AActor* Actor)
	{
		return IsValid(Actor) ? Cast<UKnightAbilitySystemComponent>(
			UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Actor, true)) : nullptr;
	}
	bool KnightThreatIdLess(const FKnightEntityId& Left, const FKnightEntityId& Right)
	{
		if (Left.Value.A != Right.Value.A)
		{
			return Left.Value.A < Right.Value.A;
		}
		if (Left.Value.B != Right.Value.B)
		{
			return Left.Value.B < Right.Value.B;
		}
		if (Left.Value.C != Right.Value.C)
		{
			return Left.Value.C < Right.Value.C;
		}
		return Left.Value.D < Right.Value.D;
	}
}

UKnightThreatComponent::UKnightThreatComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UKnightThreatComponent::BeginPlay()
{
	Super::BeginPlay();
	if (GetOwner() != nullptr && GetOwner()->HasAuthority())
	{
		const FKnightResult Result = InitializeThreat();
		if (!Result.IsSuccess())
		{
			UE_LOG(LogKnightThreat, Verbose, TEXT("Threat awaits actor-owned ASC initialization: %s"),
				*Result.Context.ToString());
		}
	}
}

FKnightResult UKnightThreatComponent::InitializeThreat()
{
	if (bEndingPlay || GetWorld() == nullptr || GetWorld()->bIsTearingDown)
	{
		return KnightThreatFailure(EKnightError::ShuttingDown, TEXT("InitializeThreat"));
	}
	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !Owner->HasAuthority())
	{
		return KnightThreatFailure(EKnightError::NotAuthority, TEXT("InitializeThreat"));
	}
	UKnightAbilitySystemComponent* ASC = KnightThreatASC(Owner);
	if (ASC == nullptr || ASC->GetOwnerActor() != Owner || ASC->GetAvatarActor() != Owner ||
		!ASC->GetEntityId().IsValid() || ASC->GetLifeComponent() == nullptr)
	{
		return KnightThreatFailure(EKnightError::InvalidState, TEXT("ThreatActorComposition"));
	}
	if (OwnerAbilitySystem.IsValid())
	{
		return OwnerAbilitySystem.Get() == ASC && OwnerId == ASC->GetEntityId() && DamageSubscription.IsValid()
			? FKnightResult::Success() : KnightThreatFailure(EKnightError::Conflict, TEXT("ThreatOwnerIdentity"));
	}
	UKnightEventSubsystem* Bus = GetWorld()->GetSubsystem<UKnightEventSubsystem>();
	if (Bus == nullptr || !Bus->IsReady())
	{
		return KnightThreatFailure(EKnightError::InvalidState, TEXT("ThreatEventBus"));
	}
	OwnerAbilitySystem = ASC;
	OwnerLife = ASC->GetLifeComponent();
	OwnerId = ASC->GetEntityId();
	const TWeakObjectPtr<UKnightThreatComponent> WeakThis(this);
	DamageSubscription = Bus->Subscribe<FKnightDamageConfirmedEvent>(this,
		[WeakThis](const FKnightDamageConfirmedEvent& Event)
		{
			if (WeakThis.IsValid())
			{
				WeakThis->HandleDamageConfirmed(Event);
			}
		});
	if (!DamageSubscription.IsValid())
	{
		OwnerAbilitySystem.Reset();
		OwnerLife.Reset();
		OwnerId = FKnightEntityId();
		return KnightThreatFailure(EKnightError::InvalidState, TEXT("ThreatSubscription"));
	}
	OwnerLifeHandle = OwnerLife->OnLifeStateChanged.AddUObject(this, &ThisClass::HandleOwnerLifeChanged);
	return FKnightResult::Success();
}

FKnightResult UKnightThreatComponent::ValidateOwner() const
{
	if (bEndingPlay || GetWorld() == nullptr || GetWorld()->bIsTearingDown)
	{
		return KnightThreatFailure(EKnightError::ShuttingDown, TEXT("ThreatOwner"));
	}
	if (!IsValid(GetOwner()) || !GetOwner()->HasAuthority())
	{
		return KnightThreatFailure(EKnightError::NotAuthority, TEXT("ThreatOwner"));
	}
	if (!OwnerAbilitySystem.IsValid() || !OwnerLife.IsValid() || !OwnerLife->IsAlive() ||
		OwnerAbilitySystem->GetOwnerActor() != GetOwner() || OwnerAbilitySystem->GetAvatarActor() != GetOwner() ||
		!(OwnerAbilitySystem->GetEntityId() == OwnerId))
	{
		return KnightThreatFailure(EKnightError::InvalidState, TEXT("ThreatOwnerLifeIdentity"));
	}
	return FKnightResult::Success();
}

FKnightResult UKnightThreatComponent::ValidateTarget(AActor* Target,
	UKnightAbilitySystemComponent*& OutAbilitySystem) const
{
	OutAbilitySystem = nullptr;
	const FKnightResult OwnerResult = ValidateOwner();
	if (!OwnerResult.IsSuccess())
	{
		return OwnerResult;
	}
	if (!IsValid(Target) || Target == GetOwner() || Target->IsActorBeingDestroyed() ||
		Target->GetWorld() != GetWorld() || !Target->HasAuthority())
	{
		return KnightThreatFailure(EKnightError::InvalidData, TEXT("ThreatTargetWorld"));
	}
	UKnightAbilitySystemComponent* ASC = KnightThreatASC(Target);
	if (ASC == nullptr || !ASC->GetEntityId().IsValid() || ASC->GetOwnerActor() != Target ||
		ASC->GetAvatarActor() != Target || ASC->GetLifeComponent() == nullptr || !ASC->GetLifeComponent()->IsAlive())
	{
		return KnightThreatFailure(EKnightError::InvalidState, TEXT("ThreatTargetLifeIdentity"));
	}
	if (ASC->GetEntityId() == OwnerId)
	{
		return KnightThreatFailure(EKnightError::DuplicateId, TEXT("ThreatTargetOwnerIdentity"));
	}
	const bool bHostile = (OwnerAbilitySystem->HasMatchingGameplayTag(KnightGameplayTags::Role_Enemy) &&
		ASC->HasMatchingGameplayTag(KnightGameplayTags::Role_Party_Knight)) ||
		(OwnerAbilitySystem->HasMatchingGameplayTag(KnightGameplayTags::Role_Party_Knight) &&
		ASC->HasMatchingGameplayTag(KnightGameplayTags::Role_Enemy));
	if (!bHostile)
	{
		return KnightThreatFailure(EKnightError::NotOwned, TEXT("ThreatFriendlyTarget"));
	}
	OutAbilitySystem = ASC;
	return FKnightResult::Success();
}

FKnightResult UKnightThreatComponent::TrySetThreatTuning(float InDecayPerSecond, float InDamageScale,
	float InMaxThreat)
{
	if (!FMath::IsFinite(InDecayPerSecond) || InDecayPerSecond < 0.0f || InDecayPerSecond > 10000.0f ||
		!FMath::IsFinite(InDamageScale) || InDamageScale <= 0.0f || InDamageScale > 1000.0f ||
		!FMath::IsFinite(InMaxThreat) || InMaxThreat < 1.0f || InMaxThreat > 1000000.0f)
	{
		return KnightThreatFailure(EKnightError::InvalidData, TEXT("ThreatTuning"));
	}
	if (bEndingPlay || GetWorld() == nullptr || GetWorld()->bIsTearingDown)
	{
		return KnightThreatFailure(EKnightError::ShuttingDown, TEXT("ThreatTuning"));
	}
	if (!IsValid(GetOwner()) || !GetOwner()->HasAuthority())
	{
		return KnightThreatFailure(EKnightError::NotAuthority, TEXT("ThreatTuning"));
	}
	if (!Entries.IsEmpty())
	{
		return KnightThreatFailure(EKnightError::Conflict, TEXT("ThreatTuningActive"));
	}
	DecayPerSecond = InDecayPerSecond;
	DamageScale = InDamageScale;
	MaxThreat = InMaxThreat;
	return FKnightResult::Success();
}

FKnightResult UKnightThreatComponent::AddThreat(AActor* Target, float Amount)
{
	if (!FMath::IsFinite(Amount) || Amount <= 0.0f || Amount > 1000000.0f)
	{
		return KnightThreatFailure(EKnightError::InvalidData, TEXT("ThreatAmount"));
	}
	UKnightAbilitySystemComponent* TargetASC = nullptr;
	const FKnightResult Result = ValidateTarget(Target, TargetASC);
	if (!Result.IsSuccess())
	{
		return Result;
	}
	const FKnightEntityId Id = TargetASC->GetEntityId();
	FKnightThreatEntry* Existing = Entries.Find(Id);
	if (Existing != nullptr && Existing->Actor.Get() != Target)
	{
		return KnightThreatFailure(EKnightError::DuplicateId, TEXT("ThreatDuplicateIdentity"));
	}
	if (Existing == nullptr && Entries.Num() >= 64)
	{
		return KnightThreatFailure(EKnightError::CapacityExceeded, TEXT("ThreatTargetCapacity"));
	}
	FKnightThreatEntry& Entry = Entries.FindOrAdd(Id);
	if (Existing == nullptr)
	{
		Entry.Actor = Target;
		Entry.AbilitySystem = TargetASC;
		Entry.Life = TargetASC->GetLifeComponent();
		Target->OnEndPlay.AddUniqueDynamic(this, &ThisClass::HandleTargetEndPlay);
		const TWeakObjectPtr<UKnightThreatComponent> WeakThis(this);
		Entry.LifeHandle = Entry.Life->OnLifeStateChanged.AddWeakLambda(this,
			[WeakThis, Id](EKnightLifeState Previous, EKnightLifeState Current)
			{
				if (Current != EKnightLifeState::Alive && WeakThis.IsValid())
				{
					WeakThis->HandleTargetUnavailable(Id);
				}
			});
	}
	Entry.Amount = FMath::Min(MaxThreat, CalculateThreat(Entry) + Amount);
	Entry.UpdatedAt = GetWorld()->GetTimeSeconds();
	UpdateMaintenanceTimer();
	OnThreatChanged.Broadcast();
	return FKnightResult::Success();
}

FKnightResult UKnightThreatComponent::ObserveTarget(AActor* Target, float Awareness)
{
	return AddThreat(Target, Awareness);
}

FKnightResult UKnightThreatComponent::ForgetTarget(AActor* Target)
{
	const FKnightResult Result = ValidateOwner();
	if (!Result.IsSuccess())
	{
		return Result;
	}
	const UKnightAbilitySystemComponent* ASC = KnightThreatASC(Target);
	if (ASC == nullptr || Target->GetWorld() != GetWorld())
	{
		return KnightThreatFailure(EKnightError::InvalidData, TEXT("ForgetThreatTarget"));
	}
	const FKnightThreatEntry* Entry = Entries.Find(ASC->GetEntityId());
	if (Entry != nullptr && Entry->Actor.Get() != Target)
	{
		return KnightThreatFailure(EKnightError::NotOwned, TEXT("ForgetThreatIdentity"));
	}
	if (Entry != nullptr)
	{
		RemoveEntry(ASC->GetEntityId());
		UpdateMaintenanceTimer();
		OnThreatChanged.Broadcast();
	}
	return FKnightResult::Success();
}

FKnightResult UKnightThreatComponent::ClearThreat()
{
	if (bEndingPlay || !IsValid(GetOwner()) || GetWorld() == nullptr || GetWorld()->bIsTearingDown)
	{
		return KnightThreatFailure(EKnightError::ShuttingDown, TEXT("ClearThreat"));
	}
	if (!GetOwner()->HasAuthority())
	{
		return KnightThreatFailure(EKnightError::NotAuthority, TEXT("ClearThreat"));
	}
	ResetEntries(true);
	return FKnightResult::Success();
}

bool UKnightThreatComponent::IsEntryValid(const FKnightEntityId& Id, const FKnightThreatEntry& Entry) const
{
	UKnightAbilitySystemComponent* CurrentAbilitySystem = nullptr;
	return ValidateTarget(Entry.Actor.Get(), CurrentAbilitySystem).IsSuccess() &&
		CurrentAbilitySystem == Entry.AbilitySystem.Get() && Entry.Life.IsValid() &&
		CurrentAbilitySystem->GetLifeComponent() == Entry.Life.Get() && CurrentAbilitySystem->GetEntityId() == Id;
}

float UKnightThreatComponent::CalculateThreat(const FKnightThreatEntry& Entry) const
{
	const double Elapsed = GetWorld() != nullptr
		? FMath::Max(0.0, static_cast<double>(GetWorld()->GetTimeSeconds()) - Entry.UpdatedAt) : 0.0;
	return static_cast<float>(FMath::Max(0.0, static_cast<double>(Entry.Amount) - Elapsed * DecayPerSecond));
}

TArray<FKnightThreatSnapshot> UKnightThreatComponent::GetThreatSnapshot() const
{
	TArray<FKnightThreatSnapshot> Snapshot;
	if (!ValidateOwner().IsSuccess())
	{
		return Snapshot;
	}
	for (const TPair<FKnightEntityId, FKnightThreatEntry>& Entry : Entries)
	{
		const float Value = CalculateThreat(Entry.Value);
		if (Value > 0.0f && IsEntryValid(Entry.Key, Entry.Value))
		{
			FKnightThreatSnapshot& Item = Snapshot.AddDefaulted_GetRef();
			Item.TargetId = Entry.Key;
			Item.Target = Entry.Value.Actor;
			Item.Threat = Value;
		}
	}
	Snapshot.Sort([](const FKnightThreatSnapshot& Left, const FKnightThreatSnapshot& Right)
		{
			return Left.Threat == Right.Threat ? KnightThreatIdLess(Left.TargetId, Right.TargetId) : Left.Threat > Right.Threat;
		});
	return Snapshot;
}

AActor* UKnightThreatComponent::GetHighestThreatTarget() const
{
	const TArray<FKnightThreatSnapshot> Snapshot = GetThreatSnapshot();
	return Snapshot.IsEmpty() ? nullptr : Snapshot[0].Target.Get();
}

float UKnightThreatComponent::GetThreat(const FKnightEntityId& TargetId) const
{
	const FKnightThreatEntry* Entry = Entries.Find(TargetId);
	return ValidateOwner().IsSuccess() && Entry != nullptr && IsEntryValid(TargetId, *Entry)
		? CalculateThreat(*Entry) : 0.0f;
}

int32 UKnightThreatComponent::GetThreatCount() const
{
	return GetThreatSnapshot().Num();
}

int32 UKnightThreatComponent::GetTrackedEntryCount() const
{
	return Entries.Num();
}

bool UKnightThreatComponent::HasMaintenanceTimer() const
{
	return GetWorld() != nullptr && GetWorld()->GetTimerManager().IsTimerActive(MaintenanceTimer);
}

void UKnightThreatComponent::HandleDamageConfirmed(const FKnightDamageConfirmedEvent& Event)
{
	if (!ValidateOwner().IsSuccess() || !(Event.TargetId == OwnerId) ||
		!FMath::IsFinite(Event.AppliedDamage) || Event.AppliedDamage <= 0.0f || !Event.SourceId.IsValid())
	{
		return;
	}
	AActor* Source = nullptr;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		const UKnightAbilitySystemComponent* ASC = KnightThreatASC(*It);
		if (ASC != nullptr && ASC->GetEntityId() == Event.SourceId)
		{
			if (Source != nullptr)
			{
				UE_LOG(LogKnightThreat, Warning, TEXT("Ambiguous confirmed damage identity %s"), *Event.SourceId.Value.ToString());
				return;
			}
			Source = *It;
		}
	}
	const float Amount = static_cast<float>(FMath::Min(static_cast<double>(MaxThreat),
		static_cast<double>(Event.AppliedDamage) * DamageScale));
	const FKnightResult Result = AddThreat(Source, Amount);
	if (!Result.IsSuccess())
	{
		UE_LOG(LogKnightThreat, Verbose, TEXT("Confirmed damage threat ignored for %s: %s"),
			*OwnerId.Value.ToString(), *Result.Context.ToString());
	}
}

void UKnightThreatComponent::RemoveEntry(const FKnightEntityId& Id)
{
	FKnightThreatEntry Entry;
	if (!Entries.RemoveAndCopyValue(Id, Entry))
	{
		return;
	}
	if (Entry.Life.IsValid())
	{
		Entry.Life->OnLifeStateChanged.Remove(Entry.LifeHandle);
	}
	if (Entry.Actor.IsValid())
	{
		Entry.Actor->OnEndPlay.RemoveDynamic(this, &ThisClass::HandleTargetEndPlay);
	}
}

void UKnightThreatComponent::ResetEntries(bool bNotify)
{
	const bool bChanged = !Entries.IsEmpty();
	TArray<FKnightEntityId> Ids;
	Entries.GetKeys(Ids);
	for (const FKnightEntityId& Id : Ids)
	{
		RemoveEntry(Id);
	}
	UpdateMaintenanceTimer();
	if (bChanged && bNotify && !bEndingPlay)
	{
		OnThreatChanged.Broadcast();
	}
}

void UKnightThreatComponent::HandleOwnerLifeChanged(EKnightLifeState Previous, EKnightLifeState Current)
{
	if (Current != EKnightLifeState::Alive)
	{
		ResetEntries(!bEndingPlay);
	}
}

void UKnightThreatComponent::HandleTargetUnavailable(const FKnightEntityId& Id)
{
	if (!bEndingPlay && Entries.Contains(Id))
	{
		RemoveEntry(Id);
		UpdateMaintenanceTimer();
		OnThreatChanged.Broadcast();
	}
}

void UKnightThreatComponent::HandleTargetEndPlay(AActor* Actor, EEndPlayReason::Type Reason)
{
	TArray<FKnightEntityId> Ids;
	for (const TPair<FKnightEntityId, FKnightThreatEntry>& Entry : Entries)
	{
		if (Entry.Value.Actor.Get() == Actor)
		{
			Ids.Add(Entry.Key);
		}
	}
	for (const FKnightEntityId& Id : Ids)
	{
		HandleTargetUnavailable(Id);
	}
}

void UKnightThreatComponent::MaintainThreat()
{
	if (!ValidateOwner().IsSuccess())
	{
		ResetEntries(!bEndingPlay);
		return;
	}
	TArray<FKnightEntityId> Ids;
	Entries.GetKeys(Ids);
	bool bChanged = false;
	for (const FKnightEntityId& Id : Ids)
	{
		FKnightThreatEntry* Entry = Entries.Find(Id);
		const float Value = Entry != nullptr ? CalculateThreat(*Entry) : 0.0f;
		if (Entry == nullptr || !IsEntryValid(Id, *Entry) || Value <= 0.0f)
		{
			RemoveEntry(Id);
			bChanged = true;
		}
		else if (Value != Entry->Amount)
		{
			Entry->Amount = Value;
			Entry->UpdatedAt = GetWorld()->GetTimeSeconds();
			bChanged = true;
		}
	}
	UpdateMaintenanceTimer();
	if (bChanged)
	{
		OnThreatChanged.Broadcast();
	}
}

void UKnightThreatComponent::UpdateMaintenanceTimer()
{
	if (GetWorld() == nullptr)
	{
		return;
	}
	FTimerManager& Timers = GetWorld()->GetTimerManager();
	if (bEndingPlay || Entries.IsEmpty())
	{
		Timers.ClearTimer(MaintenanceTimer);
	}
	else if (!Timers.IsTimerActive(MaintenanceTimer))
	{
		Timers.SetTimer(MaintenanceTimer, this, &ThisClass::MaintainThreat, 0.25f, true);
	}
}

void UKnightThreatComponent::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	bEndingPlay = true;
	ResetEntries(false);
	if (OwnerLife.IsValid())
	{
		OwnerLife->OnLifeStateChanged.Remove(OwnerLifeHandle);
	}
	if (UKnightEventSubsystem* Bus = DamageSubscription.Bus.Get())
	{
		Bus->Unsubscribe(DamageSubscription);
	}
	DamageSubscription = FKnightEventSubscription();
	OwnerLife.Reset();
	OwnerAbilitySystem.Reset();
	OnThreatChanged.Clear();
	Super::EndPlay(EndPlayReason);
}
