#include "Threat/GolemThreatComponent.h"

#include "AbilitySystemGlobals.h"
#include "Components/GolemAbilitySystemComponent.h"
#include "Components/GolemLifeComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Tags/GolemGameplayTags.h"
#include "Skills/GolemSkillRuntimeComponent.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogGolemThreat, Log, All);

namespace
{
	FGolemResult GolemThreatFailure(EGolemError Error, FName Context)
	{
		return FGolemResult::Failure(Error, Context,
			NSLOCTEXT("GolemAI", "ThreatUnavailable", "Threat awareness could not be updated."));
	}
	UGolemAbilitySystemComponent* GolemThreatASC(AActor* Actor)
	{
		return IsValid(Actor) ? Cast<UGolemAbilitySystemComponent>(
			UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Actor, true)) : nullptr;
	}
	bool GolemThreatIdLess(const FGolemEntityId& Left, const FGolemEntityId& Right)
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

UGolemThreatComponent::UGolemThreatComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UGolemThreatComponent::BeginPlay()
{
	Super::BeginPlay();
	if (GetOwner() != nullptr && GetOwner()->HasAuthority())
	{
		const FGolemResult Result = InitializeThreat();
		if (!Result.IsSuccess())
		{
			UE_LOG(LogGolemThreat, Verbose, TEXT("Threat awaits actor-owned ASC initialization: %s"),
				*Result.Context.ToString());
		}
	}
}

FGolemResult UGolemThreatComponent::InitializeThreat()
{
	if (bEndingPlay || GetWorld() == nullptr || GetWorld()->bIsTearingDown)
	{
		return GolemThreatFailure(EGolemError::ShuttingDown, TEXT("InitializeThreat"));
	}
	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !Owner->HasAuthority())
	{
		return GolemThreatFailure(EGolemError::NotAuthority, TEXT("InitializeThreat"));
	}
	UGolemAbilitySystemComponent* ASC = GolemThreatASC(Owner);
	if (ASC == nullptr || ASC->GetOwnerActor() != Owner || ASC->GetAvatarActor() != Owner ||
		!ASC->GetEntityId().IsValid() || ASC->GetLifeComponent() == nullptr)
	{
		return GolemThreatFailure(EGolemError::InvalidState, TEXT("ThreatActorComposition"));
	}
	if (OwnerAbilitySystem.IsValid())
	{
		return OwnerAbilitySystem.Get() == ASC && OwnerId == ASC->GetEntityId() && DamageSubscription.IsValid()
			? FGolemResult::Success() : GolemThreatFailure(EGolemError::Conflict, TEXT("ThreatOwnerIdentity"));
	}
	UGolemEventSubsystem* Bus = GetWorld()->GetSubsystem<UGolemEventSubsystem>();
	if (Bus == nullptr || !Bus->IsReady())
	{
		return GolemThreatFailure(EGolemError::InvalidState, TEXT("ThreatEventBus"));
	}
	OwnerAbilitySystem = ASC;
	OwnerLife = ASC->GetLifeComponent();
	OwnerId = ASC->GetEntityId();
	const TWeakObjectPtr<UGolemThreatComponent> WeakThis(this);
	DamageSubscription = Bus->Subscribe<FGolemDamageConfirmedEvent>(this,
		[WeakThis](const FGolemDamageConfirmedEvent& Event)
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
		OwnerId = FGolemEntityId();
		return GolemThreatFailure(EGolemError::InvalidState, TEXT("ThreatSubscription"));
	}
	OwnerLifeHandle = OwnerLife->OnLifeStateChanged.AddUObject(this, &ThisClass::HandleOwnerLifeChanged);
	return FGolemResult::Success();
}

FGolemResult UGolemThreatComponent::ValidateOwner() const
{
	if (bEndingPlay || GetWorld() == nullptr || GetWorld()->bIsTearingDown)
	{
		return GolemThreatFailure(EGolemError::ShuttingDown, TEXT("ThreatOwner"));
	}
	if (!IsValid(GetOwner()) || !GetOwner()->HasAuthority())
	{
		return GolemThreatFailure(EGolemError::NotAuthority, TEXT("ThreatOwner"));
	}
	if (!OwnerAbilitySystem.IsValid() || !OwnerLife.IsValid() || !OwnerLife->IsAlive() ||
		OwnerAbilitySystem->GetOwnerActor() != GetOwner() || OwnerAbilitySystem->GetAvatarActor() != GetOwner() ||
		!(OwnerAbilitySystem->GetEntityId() == OwnerId))
	{
		return GolemThreatFailure(EGolemError::InvalidState, TEXT("ThreatOwnerLifeIdentity"));
	}
	return FGolemResult::Success();
}

FGolemResult UGolemThreatComponent::ValidateTarget(AActor* Target,
	UGolemAbilitySystemComponent*& OutAbilitySystem) const
{
	OutAbilitySystem = nullptr;
	const FGolemResult OwnerResult = ValidateOwner();
	if (!OwnerResult.IsSuccess())
	{
		return OwnerResult;
	}
	if (!IsValid(Target) || Target == GetOwner() || Target->IsActorBeingDestroyed() ||
		Target->GetWorld() != GetWorld() || !Target->HasAuthority())
	{
		return GolemThreatFailure(EGolemError::InvalidData, TEXT("ThreatTargetWorld"));
	}
	UGolemAbilitySystemComponent* ASC = GolemThreatASC(Target);
	if (ASC == nullptr || !ASC->GetEntityId().IsValid() || ASC->GetOwnerActor() != Target ||
		ASC->GetAvatarActor() != Target || ASC->GetLifeComponent() == nullptr || !ASC->GetLifeComponent()->IsAlive())
	{
		return GolemThreatFailure(EGolemError::InvalidState, TEXT("ThreatTargetLifeIdentity"));
	}
	if (ASC->GetEntityId() == OwnerId)
	{
		return GolemThreatFailure(EGolemError::DuplicateId, TEXT("ThreatTargetOwnerIdentity"));
	}
	const bool bHostile = (OwnerAbilitySystem->HasMatchingGameplayTag(GolemGameplayTags::Role_Enemy) &&
		ASC->HasMatchingGameplayTag(GolemGameplayTags::Role_Party_Golem)) ||
		(OwnerAbilitySystem->HasMatchingGameplayTag(GolemGameplayTags::Role_Party_Golem) &&
		ASC->HasMatchingGameplayTag(GolemGameplayTags::Role_Enemy));
	if (!bHostile)
	{
		return GolemThreatFailure(EGolemError::NotOwned, TEXT("ThreatFriendlyTarget"));
	}
	OutAbilitySystem = ASC;
	return FGolemResult::Success();
}

FGolemResult UGolemThreatComponent::TrySetThreatTuning(float InDecayPerSecond, float InDamageScale,
	float InMaxThreat)
{
	if (!FMath::IsFinite(InDecayPerSecond) || InDecayPerSecond < 0.0f || InDecayPerSecond > 10000.0f ||
		!FMath::IsFinite(InDamageScale) || InDamageScale <= 0.0f || InDamageScale > 1000.0f ||
		!FMath::IsFinite(InMaxThreat) || InMaxThreat < 1.0f || InMaxThreat > 1000000.0f)
	{
		return GolemThreatFailure(EGolemError::InvalidData, TEXT("ThreatTuning"));
	}
	if (bEndingPlay || GetWorld() == nullptr || GetWorld()->bIsTearingDown)
	{
		return GolemThreatFailure(EGolemError::ShuttingDown, TEXT("ThreatTuning"));
	}
	if (!IsValid(GetOwner()) || !GetOwner()->HasAuthority())
	{
		return GolemThreatFailure(EGolemError::NotAuthority, TEXT("ThreatTuning"));
	}
	if (!Entries.IsEmpty())
	{
		return GolemThreatFailure(EGolemError::Conflict, TEXT("ThreatTuningActive"));
	}
	DecayPerSecond = InDecayPerSecond;
	DamageScale = InDamageScale;
	MaxThreat = InMaxThreat;
	return FGolemResult::Success();
}

FGolemResult UGolemThreatComponent::AddThreat(AActor* Target, float Amount)
{
	if (!FMath::IsFinite(Amount) || Amount <= 0.0f || Amount > 1000000.0f)
	{
		return GolemThreatFailure(EGolemError::InvalidData, TEXT("ThreatAmount"));
	}
	UGolemAbilitySystemComponent* TargetASC = nullptr;
	const FGolemResult Result = ValidateTarget(Target, TargetASC);
	if (!Result.IsSuccess())
	{
		return Result;
	}
	const FGolemEntityId Id = TargetASC->GetEntityId();
	FGolemThreatEntry* Existing = Entries.Find(Id);
	if (Existing != nullptr && Existing->Actor.Get() != Target)
	{
		return GolemThreatFailure(EGolemError::DuplicateId, TEXT("ThreatDuplicateIdentity"));
	}
	if (Existing == nullptr && Entries.Num() >= 64)
	{
		return GolemThreatFailure(EGolemError::CapacityExceeded, TEXT("ThreatTargetCapacity"));
	}
	FGolemThreatEntry& Entry = Entries.FindOrAdd(Id);
	if (Existing == nullptr)
	{
		Entry.Actor = Target;
		Entry.AbilitySystem = TargetASC;
		Entry.Life = TargetASC->GetLifeComponent();
		Target->OnEndPlay.AddUniqueDynamic(this, &ThisClass::HandleTargetEndPlay);
		const TWeakObjectPtr<UGolemThreatComponent> WeakThis(this);
		Entry.LifeHandle = Entry.Life->OnLifeStateChanged.AddWeakLambda(this,
			[WeakThis, Id](EGolemLifeState Previous, EGolemLifeState Current)
			{
				if (Current != EGolemLifeState::Alive && WeakThis.IsValid())
				{
					WeakThis->HandleTargetUnavailable(Id);
				}
			});
	}
	Entry.Amount = FMath::Min(MaxThreat, CalculateThreat(Entry) + Amount);
	Entry.UpdatedAt = GetWorld()->GetTimeSeconds();
	UpdateMaintenanceTimer();
	OnThreatChanged.Broadcast();
	return FGolemResult::Success();
}

FGolemResult UGolemThreatComponent::ObserveTarget(AActor* Target, float Awareness)
{
	return AddThreat(Target, Awareness);
}

FGolemResult UGolemThreatComponent::ForgetTarget(AActor* Target)
{
	const FGolemResult Result = ValidateOwner();
	if (!Result.IsSuccess())
	{
		return Result;
	}
	const UGolemAbilitySystemComponent* ASC = GolemThreatASC(Target);
	if (ASC == nullptr || Target->GetWorld() != GetWorld())
	{
		return GolemThreatFailure(EGolemError::InvalidData, TEXT("ForgetThreatTarget"));
	}
	const FGolemThreatEntry* Entry = Entries.Find(ASC->GetEntityId());
	if (Entry != nullptr && Entry->Actor.Get() != Target)
	{
		return GolemThreatFailure(EGolemError::NotOwned, TEXT("ForgetThreatIdentity"));
	}
	if (Entry != nullptr)
	{
		RemoveEntry(ASC->GetEntityId());
		UpdateMaintenanceTimer();
		OnThreatChanged.Broadcast();
	}
	return FGolemResult::Success();
}

FGolemResult UGolemThreatComponent::ClearThreat()
{
	if (bEndingPlay || !IsValid(GetOwner()) || GetWorld() == nullptr || GetWorld()->bIsTearingDown)
	{
		return GolemThreatFailure(EGolemError::ShuttingDown, TEXT("ClearThreat"));
	}
	if (!GetOwner()->HasAuthority())
	{
		return GolemThreatFailure(EGolemError::NotAuthority, TEXT("ClearThreat"));
	}
	ResetEntries(true);
	return FGolemResult::Success();
}

bool UGolemThreatComponent::IsEntryValid(const FGolemEntityId& Id, const FGolemThreatEntry& Entry) const
{
	UGolemAbilitySystemComponent* CurrentAbilitySystem = nullptr;
	return ValidateTarget(Entry.Actor.Get(), CurrentAbilitySystem).IsSuccess() &&
		CurrentAbilitySystem == Entry.AbilitySystem.Get() && Entry.Life.IsValid() &&
		CurrentAbilitySystem->GetLifeComponent() == Entry.Life.Get() && CurrentAbilitySystem->GetEntityId() == Id;
}

float UGolemThreatComponent::CalculateThreat(const FGolemThreatEntry& Entry) const
{
	const double Elapsed = GetWorld() != nullptr
		? FMath::Max(0.0, static_cast<double>(GetWorld()->GetTimeSeconds()) - Entry.UpdatedAt) : 0.0;
	return static_cast<float>(FMath::Max(0.0, static_cast<double>(Entry.Amount) - Elapsed * DecayPerSecond));
}

TArray<FGolemThreatSnapshot> UGolemThreatComponent::GetThreatSnapshot() const
{
	TArray<FGolemThreatSnapshot> Snapshot;
	if (!ValidateOwner().IsSuccess())
	{
		return Snapshot;
	}
	for (const TPair<FGolemEntityId, FGolemThreatEntry>& Entry : Entries)
	{
		const float Value = CalculateThreat(Entry.Value);
		if (Value > 0.0f && IsEntryValid(Entry.Key, Entry.Value))
		{
			FGolemThreatSnapshot& Item = Snapshot.AddDefaulted_GetRef();
			Item.TargetId = Entry.Key;
			Item.Target = Entry.Value.Actor;
			Item.Threat = Value;
		}
	}
	Snapshot.Sort([](const FGolemThreatSnapshot& Left, const FGolemThreatSnapshot& Right)
		{
			return Left.Threat == Right.Threat ? GolemThreatIdLess(Left.TargetId, Right.TargetId) : Left.Threat > Right.Threat;
		});
	return Snapshot;
}

AActor* UGolemThreatComponent::GetHighestThreatTarget() const
{
	if (TauntTarget.IsValid() && GetWorld() != nullptr && GetWorld()->GetTimeSeconds() < TauntEnd)
	{
		UGolemAbilitySystemComponent* TargetASC = nullptr;
		if (ValidateOwner().IsSuccess() && ValidateTarget(TauntTarget.Get(), TargetASC).IsSuccess())
		{
			return TauntTarget.Get();
		}
	}
	const TArray<FGolemThreatSnapshot> Snapshot = GetThreatSnapshot();
	return Snapshot.IsEmpty() ? nullptr : Snapshot[0].Target.Get();
}

void UGolemThreatComponent::ApplySkillTaunt(AActor* Source, float Duration)
{
	UGolemAbilitySystemComponent* TargetASC = nullptr;
	if (!ValidateOwner().IsSuccess() || !ValidateTarget(Source, TargetASC).IsSuccess()
		|| !FMath::IsFinite(Duration) || Duration <= 0.0f)
	{
		return;
	}
	TauntTarget = Source;
	TauntEnd = GetWorld()->GetTimeSeconds() + FMath::Min(3.0f, Duration);
	OnThreatChanged.Broadcast();
}

float UGolemThreatComponent::GetThreat(const FGolemEntityId& TargetId) const
{
	const FGolemThreatEntry* Entry = Entries.Find(TargetId);
	return ValidateOwner().IsSuccess() && Entry != nullptr && IsEntryValid(TargetId, *Entry)
		? CalculateThreat(*Entry) : 0.0f;
}

int32 UGolemThreatComponent::GetThreatCount() const
{
	return GetThreatSnapshot().Num();
}

int32 UGolemThreatComponent::GetTrackedEntryCount() const
{
	return Entries.Num();
}

bool UGolemThreatComponent::HasMaintenanceTimer() const
{
	return GetWorld() != nullptr && GetWorld()->GetTimerManager().IsTimerActive(MaintenanceTimer);
}

void UGolemThreatComponent::HandleDamageConfirmed(const FGolemDamageConfirmedEvent& Event)
{
	if (!ValidateOwner().IsSuccess() || !(Event.TargetId == OwnerId) ||
		!FMath::IsFinite(Event.AppliedDamage) || Event.AppliedDamage <= 0.0f || !Event.SourceId.IsValid())
	{
		return;
	}
	AActor* Source = nullptr;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		const UGolemAbilitySystemComponent* ASC = GolemThreatASC(*It);
		if (ASC != nullptr && ASC->GetEntityId() == Event.SourceId)
		{
			if (Source != nullptr)
			{
				UE_LOG(LogGolemThreat, Warning, TEXT("Ambiguous confirmed damage identity %s"), *Event.SourceId.Value.ToString());
				return;
			}
			Source = *It;
		}
	}
	const UGolemSkillRuntimeComponent* Skills = IsValid(Source)
		? Source->FindComponentByClass<UGolemSkillRuntimeComponent>() : nullptr;
	const float RoleScale = Skills != nullptr && !Skills->GetBuild().SpecId.IsNone()
		&& FGolemSkillCatalog::Role(Skills->GetBuild().SpecId) == EGolemSkillRole::Tank ? 1.5f : 1.0f;
	const float Amount = static_cast<float>(FMath::Min(static_cast<double>(MaxThreat),
		static_cast<double>(Event.AppliedDamage) * DamageScale * RoleScale));
	const FGolemResult Result = AddThreat(Source, Amount);
	if (!Result.IsSuccess())
	{
		UE_LOG(LogGolemThreat, Verbose, TEXT("Confirmed damage threat ignored for %s: %s"),
			*OwnerId.Value.ToString(), *Result.Context.ToString());
	}
}

void UGolemThreatComponent::RemoveEntry(const FGolemEntityId& Id)
{
	FGolemThreatEntry Entry;
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

void UGolemThreatComponent::ResetEntries(bool bNotify)
{
	TauntTarget.Reset();
	TauntEnd = 0.0;
	const bool bChanged = !Entries.IsEmpty();
	TArray<FGolemEntityId> Ids;
	Entries.GetKeys(Ids);
	for (const FGolemEntityId& Id : Ids)
	{
		RemoveEntry(Id);
	}
	UpdateMaintenanceTimer();
	if (bChanged && bNotify && !bEndingPlay)
	{
		OnThreatChanged.Broadcast();
	}
}

void UGolemThreatComponent::HandleOwnerLifeChanged(EGolemLifeState Previous, EGolemLifeState Current)
{
	if (Current != EGolemLifeState::Alive)
	{
		ResetEntries(!bEndingPlay);
	}
}

void UGolemThreatComponent::HandleTargetUnavailable(const FGolemEntityId& Id)
{
	if (!bEndingPlay && Entries.Contains(Id))
	{
		RemoveEntry(Id);
		UpdateMaintenanceTimer();
		OnThreatChanged.Broadcast();
	}
}

void UGolemThreatComponent::HandleTargetEndPlay(AActor* Actor, EEndPlayReason::Type Reason)
{
	TArray<FGolemEntityId> Ids;
	for (const TPair<FGolemEntityId, FGolemThreatEntry>& Entry : Entries)
	{
		if (Entry.Value.Actor.Get() == Actor)
		{
			Ids.Add(Entry.Key);
		}
	}
	for (const FGolemEntityId& Id : Ids)
	{
		HandleTargetUnavailable(Id);
	}
}

void UGolemThreatComponent::MaintainThreat()
{
	if (!ValidateOwner().IsSuccess())
	{
		ResetEntries(!bEndingPlay);
		return;
	}
	TArray<FGolemEntityId> Ids;
	Entries.GetKeys(Ids);
	bool bChanged = false;
	for (const FGolemEntityId& Id : Ids)
	{
		FGolemThreatEntry* Entry = Entries.Find(Id);
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

void UGolemThreatComponent::UpdateMaintenanceTimer()
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

void UGolemThreatComponent::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	bEndingPlay = true;
	ResetEntries(false);
	if (OwnerLife.IsValid())
	{
		OwnerLife->OnLifeStateChanged.Remove(OwnerLifeHandle);
	}
	if (UGolemEventSubsystem* Bus = DamageSubscription.Bus.Get())
	{
		Bus->Unsubscribe(DamageSubscription);
	}
	DamageSubscription = FGolemEventSubscription();
	OwnerLife.Reset();
	OwnerAbilitySystem.Reset();
	OnThreatChanged.Clear();
	Super::EndPlay(EndPlayReason);
}
