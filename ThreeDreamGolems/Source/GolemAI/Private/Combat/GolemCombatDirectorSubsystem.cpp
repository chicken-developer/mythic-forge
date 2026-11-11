#include "Combat/GolemCombatDirectorSubsystem.h"

#include "AbilitySystemGlobals.h"
#include "Components/GolemAbilitySystemComponent.h"
#include "Components/GolemLifeComponent.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Tags/GolemGameplayTags.h"
#include "TimerManager.h"

namespace
{
	FGolemResult GolemDirectorFailure(EGolemError Error, FName Context)
	{
		return FGolemResult::Failure(Error, Context,
			NSLOCTEXT("GolemAI", "AttackLeaseUnavailable", "An attack opportunity is unavailable right now."));
	}
	UGolemAbilitySystemComponent* GolemDirectorASC(AActor* Actor)
	{
		return IsValid(Actor) ? Cast<UGolemAbilitySystemComponent>(
			UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Actor, true)) : nullptr;
	}
	bool GolemDirectorHostile(const UGolemAbilitySystemComponent* Source, const UGolemAbilitySystemComponent* Target)
	{
		return (Source->HasMatchingGameplayTag(GolemGameplayTags::Role_Enemy) &&
			Target->HasMatchingGameplayTag(GolemGameplayTags::Role_Party_Golem)) ||
			(Source->HasMatchingGameplayTag(GolemGameplayTags::Role_Party_Golem) &&
			Target->HasMatchingGameplayTag(GolemGameplayTags::Role_Enemy));
	}
}

bool FGolemAttackLease::IsValid() const
{
	return LeaseId.IsValid() && SourceId.IsValid() && TargetId.IsValid();
}

bool FGolemAttackLease::operator==(const FGolemAttackLease& Other) const
{
	return LeaseId == Other.LeaseId && SourceId == Other.SourceId && TargetId == Other.TargetId;
}

bool UGolemCombatDirectorSubsystem::DoesSupportWorldType(EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UGolemCombatDirectorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	bReady = true;
}

FGolemResult UGolemCombatDirectorSubsystem::ValidateAuthority() const
{
	if (!bReady || GetWorld() == nullptr || GetWorld()->bIsTearingDown)
	{
		return GolemDirectorFailure(EGolemError::ShuttingDown, TEXT("AttackDirectorLifecycle"));
	}
	return GetWorld()->GetNetMode() != NM_Client ? FGolemResult::Success()
		: GolemDirectorFailure(EGolemError::NotAuthority, TEXT("AttackDirectorAuthority"));
}

FGolemResult UGolemCombatDirectorSubsystem::ValidateActor(AActor* Actor,
	UGolemAbilitySystemComponent*& OutAbilitySystem) const
{
	OutAbilitySystem = nullptr;
	if (!IsValid(Actor) || Actor->IsActorBeingDestroyed() || Actor->GetWorld() != GetWorld())
	{
		return GolemDirectorFailure(EGolemError::InvalidData, TEXT("AttackParticipantWorld"));
	}
	if (!Actor->HasAuthority())
	{
		return GolemDirectorFailure(EGolemError::NotAuthority, TEXT("AttackParticipantAuthority"));
	}
	UGolemAbilitySystemComponent* ASC = GolemDirectorASC(Actor);
	if (ASC == nullptr || !ASC->GetEntityId().IsValid() || ASC->GetOwnerActor() != Actor ||
		ASC->GetAvatarActor() != Actor || ASC->GetLifeComponent() == nullptr || !ASC->GetLifeComponent()->IsAlive())
	{
		return GolemDirectorFailure(EGolemError::InvalidState, TEXT("AttackParticipantLifeIdentity"));
	}
	for (const TPair<TWeakObjectPtr<AActor>, FGolemAttackParticipant>& Participant : Participants)
	{
		if (Participant.Value.Id == ASC->GetEntityId() && Participant.Value.Actor.Get() != Actor)
		{
			return GolemDirectorFailure(EGolemError::DuplicateId, TEXT("AttackParticipantDuplicateIdentity"));
		}
	}
	OutAbilitySystem = ASC;
	return FGolemResult::Success();
}

FGolemResult UGolemCombatDirectorSubsystem::TrySetBudgets(int32 InGlobalBudget, int32 InPerTargetBudget)
{
	const FGolemResult Authority = ValidateAuthority();
	if (!Authority.IsSuccess())
	{
		return Authority;
	}
	if (InGlobalBudget < 1 || InGlobalBudget > 64 || InPerTargetBudget < 1 || InPerTargetBudget > InGlobalBudget)
	{
		return GolemDirectorFailure(EGolemError::InvalidData, TEXT("AttackBudgets"));
	}
	MaintainLeases();
	const FGolemResult Lifecycle = ValidateAuthority();
	if (!Lifecycle.IsSuccess())
	{
		return Lifecycle;
	}
	if (!ActiveLeases.IsEmpty())
	{
		return GolemDirectorFailure(EGolemError::Conflict, TEXT("AttackBudgetsActive"));
	}
	GlobalBudget = InGlobalBudget;
	PerTargetBudget = InPerTargetBudget;
	PartyBudget = HostileBudget = InGlobalBudget;
	return FGolemResult::Success();
}

int32 UGolemCombatDirectorSubsystem::GetHostileBudget() const
{
	return HostileBudget;
}

FGolemResult UGolemCombatDirectorSubsystem::TrySetTeamBudgets(const int32 InPartyBudget,
	const int32 InHostileBudget, const int32 InPerTargetBudget)
{
	if (InPartyBudget < 1 || InHostileBudget < 1 || InPartyBudget > 32 || InHostileBudget > 32)
	{
		return GolemDirectorFailure(EGolemError::InvalidData, TEXT("AttackTeamBudgets"));
	}
	const FGolemResult Result = TrySetBudgets(InPartyBudget + InHostileBudget, InPerTargetBudget);
	if (Result.IsSuccess())
	{
		PartyBudget = InPartyBudget;
		HostileBudget = InHostileBudget;
	}
	return Result;
}

FGolemResult UGolemCombatDirectorSubsystem::TryAcquireAttackToken(AActor* Source, AActor* Target,
	float Duration, FGolemAttackLease& OutLease)
{
	OutLease = FGolemAttackLease();
	const FGolemResult Authority = ValidateAuthority();
	if (!Authority.IsSuccess())
	{
		return Authority;
	}
	if (!FMath::IsFinite(Duration) || Duration < 0.05f || Duration > 60.0f || Source == Target)
	{
		return GolemDirectorFailure(EGolemError::InvalidData, TEXT("AttackLeaseRequest"));
	}
	MaintainLeases();
	const FGolemResult Lifecycle = ValidateAuthority();
	if (!Lifecycle.IsSuccess())
	{
		return Lifecycle;
	}
	UGolemAbilitySystemComponent* SourceASC = nullptr;
	UGolemAbilitySystemComponent* TargetASC = nullptr;
	FGolemResult Result = ValidateActor(Source, SourceASC);
	if (!Result.IsSuccess())
	{
		return Result;
	}
	Result = ValidateActor(Target, TargetASC);
	if (!Result.IsSuccess())
	{
		return Result;
	}
	if (SourceASC->GetEntityId() == TargetASC->GetEntityId())
	{
		return GolemDirectorFailure(EGolemError::DuplicateId, TEXT("AttackLeaseIdentity"));
	}
	if (!GolemDirectorHostile(SourceASC, TargetASC))
	{
		return GolemDirectorFailure(EGolemError::NotOwned, TEXT("AttackLeaseFriendlyTarget"));
	}
	for (const TPair<FGuid, FGolemActiveAttackLease>& Entry : ActiveLeases)
	{
		if (Entry.Value.Capability.SourceId == SourceASC->GetEntityId())
		{
			return GolemDirectorFailure(EGolemError::Conflict, TEXT("AttackLeaseSourceActive"));
		}
	}
	const bool bHostile = SourceASC->HasMatchingGameplayTag(GolemGameplayTags::Role_Enemy);
	int32 SideLeases = 0;
	for (const TPair<FGuid, FGolemActiveAttackLease>& Entry : ActiveLeases)
	{
		if (const UGolemAbilitySystemComponent* ExistingASC = GolemDirectorASC(Entry.Value.Source.Get()))
		{
			SideLeases += ExistingASC->HasMatchingGameplayTag(GolemGameplayTags::Role_Enemy) == bHostile ? 1 : 0;
		}
	}
	if (ActiveLeases.Num() >= GlobalBudget || SideLeases >= (bHostile ? HostileBudget : PartyBudget)
		|| GetActiveLeaseCountForTarget(TargetASC->GetEntityId()) >= PerTargetBudget)
	{
		return GolemDirectorFailure(EGolemError::CapacityExceeded, TEXT("AttackLeaseBudgetExhausted"));
	}
	FGolemActiveAttackLease Entry;
	Entry.Capability.LeaseId = FGuid::NewGuid();
	Entry.Capability.SourceId = SourceASC->GetEntityId();
	Entry.Capability.TargetId = TargetASC->GetEntityId();
	Entry.Source = Source;
	Entry.Target = Target;
	if (const APawn* Pawn = Cast<APawn>(Source))
	{
		Entry.SourceController = Pawn->GetController();
	}
	Entry.ExpiresAt = GetWorld()->GetTimeSeconds() + Duration;
	RegisterParticipant(Source, SourceASC);
	RegisterParticipant(Target, TargetASC);
	ActiveLeases.Add(Entry.Capability.LeaseId, Entry);
	OutLease = Entry.Capability;
	UpdateMaintenanceTimer();
	return FGolemResult::Success();
}

FGolemResult UGolemCombatDirectorSubsystem::ReleaseAttackToken(AActor* Source, const FGolemAttackLease& Lease)
{
	const FGolemResult Authority = ValidateAuthority();
	if (!Authority.IsSuccess())
	{
		return Authority;
	}
	const UGolemAbilitySystemComponent* ASC = GolemDirectorASC(Source);
	if (!Lease.IsValid() || ASC == nullptr || !Source->HasAuthority() || Source->GetWorld() != GetWorld() ||
		!(ASC->GetEntityId() == Lease.SourceId))
	{
		return GolemDirectorFailure(EGolemError::NotOwned, TEXT("AttackLeaseReleaseOwner"));
	}
	MaintainLeases();
	const FGolemResult Lifecycle = ValidateAuthority();
	if (!Lifecycle.IsSuccess())
	{
		return Lifecycle;
	}
	if (const FGolemActiveAttackLease* Entry = ActiveLeases.Find(Lease.LeaseId))
	{
		if (Entry->Source.Get() != Source || !(Entry->Capability == Lease))
		{
			return GolemDirectorFailure(EGolemError::NotOwned, TEXT("AttackLeaseReleaseCapability"));
		}
		ReleaseLeaseInternal(Lease.LeaseId, EGolemAttackLeaseReleaseReason::Completed);
		return FGolemResult::Success();
	}
	for (const TPair<FGuid, FGolemActiveAttackLease>& Entry : ActiveLeases)
	{
		if (Entry.Value.Capability.SourceId == Lease.SourceId)
		{
			return GolemDirectorFailure(EGolemError::Conflict, TEXT("AttackLeaseReleaseStale"));
		}
	}
	const FGolemReleasedAttackLease* Released = LastReleasedLeases.Find(Lease.SourceId);
	return Released != nullptr && Released->Source.Get() == Source && Released->Capability == Lease
		? FGolemResult::Success() : GolemDirectorFailure(EGolemError::NotFound, TEXT("AttackLeaseReleaseMissing"));
}

bool UGolemCombatDirectorSubsystem::IsParticipantValid(const TWeakObjectPtr<AActor>& Actor,
	const FGolemEntityId& Id) const
{
	const FGolemAttackParticipant* Participant = Participants.Find(Actor);
	return Actor.IsValid() && !Actor->IsActorBeingDestroyed() && Actor->GetWorld() == GetWorld() &&
		Actor->HasAuthority() && Participant != nullptr && Participant->AbilitySystem.IsValid() &&
		Participant->Life.IsValid() && Participant->Life->IsAlive() && Participant->Id == Id &&
		GolemDirectorASC(Actor.Get()) == Participant->AbilitySystem.Get() &&
		Participant->AbilitySystem->GetLifeComponent() == Participant->Life.Get() &&
		Participant->AbilitySystem->GetEntityId() == Id && Participant->AbilitySystem->GetOwnerActor() == Actor.Get() &&
		Participant->AbilitySystem->GetAvatarActor() == Actor.Get();
}

bool UGolemCombatDirectorSubsystem::IsActiveEntryValid(const FGolemActiveAttackLease& Entry) const
{
	if (!ValidateAuthority().IsSuccess() || GetWorld()->GetTimeSeconds() >= Entry.ExpiresAt ||
		!IsParticipantValid(Entry.Source, Entry.Capability.SourceId) ||
		!IsParticipantValid(Entry.Target, Entry.Capability.TargetId))
	{
		return false;
	}
	if (const APawn* Pawn = Cast<APawn>(Entry.Source.Get()))
	{
		if (!(TWeakObjectPtr<AController>(Pawn->GetController()) == Entry.SourceController))
		{
			return false;
		}
	}
	const FGolemAttackParticipant* Source = Participants.Find(Entry.Source);
	const FGolemAttackParticipant* Target = Participants.Find(Entry.Target);
	return GolemDirectorHostile(Source->AbilitySystem.Get(), Target->AbilitySystem.Get());
}

bool UGolemCombatDirectorSubsystem::IsLeaseActive(const FGolemAttackLease& Lease) const
{
	const FGolemActiveAttackLease* Entry = ActiveLeases.Find(Lease.LeaseId);
	return Entry != nullptr && Entry->Capability == Lease && IsActiveEntryValid(*Entry);
}

int32 UGolemCombatDirectorSubsystem::GetActiveLeaseCount() const
{
	int32 Count = 0;
	for (const TPair<FGuid, FGolemActiveAttackLease>& Entry : ActiveLeases)
	{
		Count += IsActiveEntryValid(Entry.Value) ? 1 : 0;
	}
	return Count;
}

int32 UGolemCombatDirectorSubsystem::GetActiveLeaseCountForTarget(const FGolemEntityId& TargetId) const
{
	int32 Count = 0;
	for (const TPair<FGuid, FGolemActiveAttackLease>& Entry : ActiveLeases)
	{
		Count += Entry.Value.Capability.TargetId == TargetId && IsActiveEntryValid(Entry.Value) ? 1 : 0;
	}
	return Count;
}

int32 UGolemCombatDirectorSubsystem::GetGlobalBudget() const
{
	return GlobalBudget;
}

int32 UGolemCombatDirectorSubsystem::GetPerTargetBudget() const
{
	return PerTargetBudget;
}

bool UGolemCombatDirectorSubsystem::HasMaintenanceTimer() const
{
	return GetWorld() != nullptr && GetWorld()->GetTimerManager().IsTimerActive(MaintenanceTimer);
}

void UGolemCombatDirectorSubsystem::RegisterParticipant(AActor* Actor, UGolemAbilitySystemComponent* AbilitySystem)
{
	FGolemAttackParticipant& Participant = Participants.FindOrAdd(Actor);
	if (Participant.References++ > 0)
	{
		return;
	}
	Participant.Actor = Actor;
	Participant.AbilitySystem = AbilitySystem;
	Participant.Life = AbilitySystem->GetLifeComponent();
	Participant.Id = AbilitySystem->GetEntityId();
	Actor->OnEndPlay.AddUniqueDynamic(this, &ThisClass::HandleActorEndPlay);
	if (APawn* Pawn = Cast<APawn>(Actor))
	{
		Pawn->ReceiveControllerChangedDelegate.AddUniqueDynamic(this, &ThisClass::HandleControllerChanged);
	}
	const TWeakObjectPtr<UGolemCombatDirectorSubsystem> WeakThis(this);
	const TWeakObjectPtr<AActor> WeakActor(Actor);
	Participant.LifeHandle = Participant.Life->OnLifeStateChanged.AddWeakLambda(this,
		[WeakThis, WeakActor](EGolemLifeState Previous, EGolemLifeState Current)
		{
			if (Current != EGolemLifeState::Alive && WeakThis.IsValid() && WeakActor.IsValid())
			{
				WeakThis->ReleaseActorLeases(WeakActor.Get(), EGolemAttackLeaseReleaseReason::LifeUnavailable);
			}
		});
}

void UGolemCombatDirectorSubsystem::UnregisterParticipant(const TWeakObjectPtr<AActor>& Actor)
{
	FGolemAttackParticipant* Participant = Participants.Find(Actor);
	if (Participant == nullptr || --Participant->References > 0)
	{
		return;
	}
	const FGolemAttackParticipant Removed = *Participant;
	Participants.Remove(Actor);
	if (Removed.Life.IsValid())
	{
		Removed.Life->OnLifeStateChanged.Remove(Removed.LifeHandle);
	}
	if (Actor.IsValid())
	{
		Actor->OnEndPlay.RemoveDynamic(this, &ThisClass::HandleActorEndPlay);
		if (APawn* Pawn = Cast<APawn>(Actor.Get()))
		{
			Pawn->ReceiveControllerChangedDelegate.RemoveDynamic(this, &ThisClass::HandleControllerChanged);
		}
	}
}

void UGolemCombatDirectorSubsystem::ReleaseLeaseInternal(const FGuid& LeaseId,
	EGolemAttackLeaseReleaseReason Reason, bool bNotify)
{
	FGolemActiveAttackLease Entry;
	if (!ActiveLeases.RemoveAndCopyValue(LeaseId, Entry))
	{
		return;
	}
	UnregisterParticipant(Entry.Source);
	UnregisterParticipant(Entry.Target);
	FGolemReleasedAttackLease& Released = LastReleasedLeases.FindOrAdd(Entry.Capability.SourceId);
	Released.Capability = Entry.Capability;
	Released.Source = Entry.Source;
	UpdateMaintenanceTimer();
	if (bReady && bNotify && GetWorld() != nullptr && !GetWorld()->bIsTearingDown)
	{
		OnAttackLeaseReleased.Broadcast(Entry.Capability, Reason);
	}
}

void UGolemCombatDirectorSubsystem::ReleaseActorLeases(AActor* Actor, EGolemAttackLeaseReleaseReason Reason)
{
	TArray<FGuid> Ids;
	for (const TPair<FGuid, FGolemActiveAttackLease>& Entry : ActiveLeases)
	{
		if (Entry.Value.Source.Get() == Actor || Entry.Value.Target.Get() == Actor)
		{
			Ids.Add(Entry.Key);
		}
	}
	for (const FGuid& Id : Ids)
	{
		ReleaseLeaseInternal(Id, Reason);
	}
}

void UGolemCombatDirectorSubsystem::HandleActorEndPlay(AActor* Actor, EEndPlayReason::Type Reason)
{
	ReleaseActorLeases(Actor, EGolemAttackLeaseReleaseReason::ActorEndPlay);
}

void UGolemCombatDirectorSubsystem::HandleControllerChanged(APawn* Pawn, AController* Previous,
	AController* Current)
{
	if (Previous != Current)
	{
		ReleaseActorLeases(Pawn, EGolemAttackLeaseReleaseReason::ControllerChanged);
	}
}

void UGolemCombatDirectorSubsystem::MaintainLeases()
{
	if (!ValidateAuthority().IsSuccess())
	{
		ShutdownLeases();
		return;
	}
	TArray<FGuid> Ids;
	ActiveLeases.GetKeys(Ids);
	for (const FGuid& Id : Ids)
	{
		const FGolemActiveAttackLease* Entry = ActiveLeases.Find(Id);
		if (Entry != nullptr && !IsActiveEntryValid(*Entry))
		{
			const EGolemAttackLeaseReleaseReason Reason = GetWorld()->GetTimeSeconds() >= Entry->ExpiresAt
				? EGolemAttackLeaseReleaseReason::Expired : EGolemAttackLeaseReleaseReason::InvalidParticipant;
			ReleaseLeaseInternal(Id, Reason);
		}
	}
	for (TMap<FGolemEntityId, FGolemReleasedAttackLease>::TIterator It = LastReleasedLeases.CreateIterator(); It; ++It)
	{
		if (!It.Value().Source.IsValid() || It.Value().Source->IsActorBeingDestroyed())
		{
			It.RemoveCurrent();
		}
	}
	UpdateMaintenanceTimer();
}

void UGolemCombatDirectorSubsystem::UpdateMaintenanceTimer()
{
	if (GetWorld() == nullptr)
	{
		return;
	}
	FTimerManager& Timers = GetWorld()->GetTimerManager();
	if (!bReady || ActiveLeases.IsEmpty())
	{
		Timers.ClearTimer(MaintenanceTimer);
	}
	else if (!Timers.IsTimerActive(MaintenanceTimer))
	{
		Timers.SetTimer(MaintenanceTimer, this, &ThisClass::MaintainLeases, 0.05f, true);
	}
}

void UGolemCombatDirectorSubsystem::ShutdownLeases()
{
	bReady = false;
	TArray<FGuid> Ids;
	ActiveLeases.GetKeys(Ids);
	for (const FGuid& Id : Ids)
	{
		ReleaseLeaseInternal(Id, EGolemAttackLeaseReleaseReason::WorldEndPlay, false);
	}
	LastReleasedLeases.Reset();
	UpdateMaintenanceTimer();
}

void UGolemCombatDirectorSubsystem::OnWorldEndPlay(UWorld& InWorld)
{
	ShutdownLeases();
	Super::OnWorldEndPlay(InWorld);
}

void UGolemCombatDirectorSubsystem::Deinitialize()
{
	ShutdownLeases();
	OnAttackLeaseReleased.Clear();
	Super::Deinitialize();
}
