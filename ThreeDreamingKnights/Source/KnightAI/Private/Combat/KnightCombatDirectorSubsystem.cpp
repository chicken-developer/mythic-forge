#include "Combat/KnightCombatDirectorSubsystem.h"

#include "AbilitySystemGlobals.h"
#include "Components/KnightAbilitySystemComponent.h"
#include "Components/KnightLifeComponent.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Tags/KnightGameplayTags.h"
#include "TimerManager.h"

namespace
{
	FKnightResult KnightDirectorFailure(EKnightError Error, FName Context)
	{
		return FKnightResult::Failure(Error, Context,
			NSLOCTEXT("KnightAI", "AttackLeaseUnavailable", "An attack opportunity is unavailable right now."));
	}
	UKnightAbilitySystemComponent* KnightDirectorASC(AActor* Actor)
	{
		return IsValid(Actor) ? Cast<UKnightAbilitySystemComponent>(
			UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Actor, true)) : nullptr;
	}
	bool KnightDirectorHostile(const UKnightAbilitySystemComponent* Source, const UKnightAbilitySystemComponent* Target)
	{
		return (Source->HasMatchingGameplayTag(KnightGameplayTags::Role_Enemy) &&
			Target->HasMatchingGameplayTag(KnightGameplayTags::Role_Party_Knight)) ||
			(Source->HasMatchingGameplayTag(KnightGameplayTags::Role_Party_Knight) &&
			Target->HasMatchingGameplayTag(KnightGameplayTags::Role_Enemy));
	}
}

bool FKnightAttackLease::IsValid() const
{
	return LeaseId.IsValid() && SourceId.IsValid() && TargetId.IsValid();
}

bool FKnightAttackLease::operator==(const FKnightAttackLease& Other) const
{
	return LeaseId == Other.LeaseId && SourceId == Other.SourceId && TargetId == Other.TargetId;
}

bool UKnightCombatDirectorSubsystem::DoesSupportWorldType(EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UKnightCombatDirectorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	bReady = true;
}

FKnightResult UKnightCombatDirectorSubsystem::ValidateAuthority() const
{
	if (!bReady || GetWorld() == nullptr || GetWorld()->bIsTearingDown)
	{
		return KnightDirectorFailure(EKnightError::ShuttingDown, TEXT("AttackDirectorLifecycle"));
	}
	return GetWorld()->GetNetMode() != NM_Client ? FKnightResult::Success()
		: KnightDirectorFailure(EKnightError::NotAuthority, TEXT("AttackDirectorAuthority"));
}

FKnightResult UKnightCombatDirectorSubsystem::ValidateActor(AActor* Actor,
	UKnightAbilitySystemComponent*& OutAbilitySystem) const
{
	OutAbilitySystem = nullptr;
	if (!IsValid(Actor) || Actor->IsActorBeingDestroyed() || Actor->GetWorld() != GetWorld())
	{
		return KnightDirectorFailure(EKnightError::InvalidData, TEXT("AttackParticipantWorld"));
	}
	if (!Actor->HasAuthority())
	{
		return KnightDirectorFailure(EKnightError::NotAuthority, TEXT("AttackParticipantAuthority"));
	}
	UKnightAbilitySystemComponent* ASC = KnightDirectorASC(Actor);
	if (ASC == nullptr || !ASC->GetEntityId().IsValid() || ASC->GetOwnerActor() != Actor ||
		ASC->GetAvatarActor() != Actor || ASC->GetLifeComponent() == nullptr || !ASC->GetLifeComponent()->IsAlive())
	{
		return KnightDirectorFailure(EKnightError::InvalidState, TEXT("AttackParticipantLifeIdentity"));
	}
	for (const TPair<TWeakObjectPtr<AActor>, FKnightAttackParticipant>& Participant : Participants)
	{
		if (Participant.Value.Id == ASC->GetEntityId() && Participant.Value.Actor.Get() != Actor)
		{
			return KnightDirectorFailure(EKnightError::DuplicateId, TEXT("AttackParticipantDuplicateIdentity"));
		}
	}
	OutAbilitySystem = ASC;
	return FKnightResult::Success();
}

FKnightResult UKnightCombatDirectorSubsystem::TrySetBudgets(int32 InGlobalBudget, int32 InPerTargetBudget)
{
	const FKnightResult Authority = ValidateAuthority();
	if (!Authority.IsSuccess())
	{
		return Authority;
	}
	if (InGlobalBudget < 1 || InGlobalBudget > 64 || InPerTargetBudget < 1 || InPerTargetBudget > InGlobalBudget)
	{
		return KnightDirectorFailure(EKnightError::InvalidData, TEXT("AttackBudgets"));
	}
	MaintainLeases();
	const FKnightResult Lifecycle = ValidateAuthority();
	if (!Lifecycle.IsSuccess())
	{
		return Lifecycle;
	}
	if (!ActiveLeases.IsEmpty())
	{
		return KnightDirectorFailure(EKnightError::Conflict, TEXT("AttackBudgetsActive"));
	}
	GlobalBudget = InGlobalBudget;
	PerTargetBudget = InPerTargetBudget;
	PartyBudget = HostileBudget = InGlobalBudget;
	return FKnightResult::Success();
}

int32 UKnightCombatDirectorSubsystem::GetHostileBudget() const
{
	return HostileBudget;
}

FKnightResult UKnightCombatDirectorSubsystem::TrySetTeamBudgets(const int32 InPartyBudget,
	const int32 InHostileBudget, const int32 InPerTargetBudget)
{
	if (InPartyBudget < 1 || InHostileBudget < 1 || InPartyBudget > 32 || InHostileBudget > 32)
	{
		return KnightDirectorFailure(EKnightError::InvalidData, TEXT("AttackTeamBudgets"));
	}
	const FKnightResult Result = TrySetBudgets(InPartyBudget + InHostileBudget, InPerTargetBudget);
	if (Result.IsSuccess())
	{
		PartyBudget = InPartyBudget;
		HostileBudget = InHostileBudget;
	}
	return Result;
}

FKnightResult UKnightCombatDirectorSubsystem::TryAcquireAttackToken(AActor* Source, AActor* Target,
	float Duration, FKnightAttackLease& OutLease)
{
	OutLease = FKnightAttackLease();
	const FKnightResult Authority = ValidateAuthority();
	if (!Authority.IsSuccess())
	{
		return Authority;
	}
	if (!FMath::IsFinite(Duration) || Duration < 0.05f || Duration > 60.0f || Source == Target)
	{
		return KnightDirectorFailure(EKnightError::InvalidData, TEXT("AttackLeaseRequest"));
	}
	MaintainLeases();
	const FKnightResult Lifecycle = ValidateAuthority();
	if (!Lifecycle.IsSuccess())
	{
		return Lifecycle;
	}
	UKnightAbilitySystemComponent* SourceASC = nullptr;
	UKnightAbilitySystemComponent* TargetASC = nullptr;
	FKnightResult Result = ValidateActor(Source, SourceASC);
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
		return KnightDirectorFailure(EKnightError::DuplicateId, TEXT("AttackLeaseIdentity"));
	}
	if (!KnightDirectorHostile(SourceASC, TargetASC))
	{
		return KnightDirectorFailure(EKnightError::NotOwned, TEXT("AttackLeaseFriendlyTarget"));
	}
	for (const TPair<FGuid, FKnightActiveAttackLease>& Entry : ActiveLeases)
	{
		if (Entry.Value.Capability.SourceId == SourceASC->GetEntityId())
		{
			return KnightDirectorFailure(EKnightError::Conflict, TEXT("AttackLeaseSourceActive"));
		}
	}
	const bool bHostile = SourceASC->HasMatchingGameplayTag(KnightGameplayTags::Role_Enemy);
	int32 SideLeases = 0;
	for (const TPair<FGuid, FKnightActiveAttackLease>& Entry : ActiveLeases)
	{
		if (const UKnightAbilitySystemComponent* ExistingASC = KnightDirectorASC(Entry.Value.Source.Get()))
		{
			SideLeases += ExistingASC->HasMatchingGameplayTag(KnightGameplayTags::Role_Enemy) == bHostile ? 1 : 0;
		}
	}
	if (ActiveLeases.Num() >= GlobalBudget || SideLeases >= (bHostile ? HostileBudget : PartyBudget)
		|| GetActiveLeaseCountForTarget(TargetASC->GetEntityId()) >= PerTargetBudget)
	{
		return KnightDirectorFailure(EKnightError::CapacityExceeded, TEXT("AttackLeaseBudgetExhausted"));
	}
	FKnightActiveAttackLease Entry;
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
	return FKnightResult::Success();
}

FKnightResult UKnightCombatDirectorSubsystem::ReleaseAttackToken(AActor* Source, const FKnightAttackLease& Lease)
{
	const FKnightResult Authority = ValidateAuthority();
	if (!Authority.IsSuccess())
	{
		return Authority;
	}
	const UKnightAbilitySystemComponent* ASC = KnightDirectorASC(Source);
	if (!Lease.IsValid() || ASC == nullptr || !Source->HasAuthority() || Source->GetWorld() != GetWorld() ||
		!(ASC->GetEntityId() == Lease.SourceId))
	{
		return KnightDirectorFailure(EKnightError::NotOwned, TEXT("AttackLeaseReleaseOwner"));
	}
	MaintainLeases();
	const FKnightResult Lifecycle = ValidateAuthority();
	if (!Lifecycle.IsSuccess())
	{
		return Lifecycle;
	}
	if (const FKnightActiveAttackLease* Entry = ActiveLeases.Find(Lease.LeaseId))
	{
		if (Entry->Source.Get() != Source || !(Entry->Capability == Lease))
		{
			return KnightDirectorFailure(EKnightError::NotOwned, TEXT("AttackLeaseReleaseCapability"));
		}
		ReleaseLeaseInternal(Lease.LeaseId, EKnightAttackLeaseReleaseReason::Completed);
		return FKnightResult::Success();
	}
	for (const TPair<FGuid, FKnightActiveAttackLease>& Entry : ActiveLeases)
	{
		if (Entry.Value.Capability.SourceId == Lease.SourceId)
		{
			return KnightDirectorFailure(EKnightError::Conflict, TEXT("AttackLeaseReleaseStale"));
		}
	}
	const FKnightReleasedAttackLease* Released = LastReleasedLeases.Find(Lease.SourceId);
	return Released != nullptr && Released->Source.Get() == Source && Released->Capability == Lease
		? FKnightResult::Success() : KnightDirectorFailure(EKnightError::NotFound, TEXT("AttackLeaseReleaseMissing"));
}

bool UKnightCombatDirectorSubsystem::IsParticipantValid(const TWeakObjectPtr<AActor>& Actor,
	const FKnightEntityId& Id) const
{
	const FKnightAttackParticipant* Participant = Participants.Find(Actor);
	return Actor.IsValid() && !Actor->IsActorBeingDestroyed() && Actor->GetWorld() == GetWorld() &&
		Actor->HasAuthority() && Participant != nullptr && Participant->AbilitySystem.IsValid() &&
		Participant->Life.IsValid() && Participant->Life->IsAlive() && Participant->Id == Id &&
		KnightDirectorASC(Actor.Get()) == Participant->AbilitySystem.Get() &&
		Participant->AbilitySystem->GetLifeComponent() == Participant->Life.Get() &&
		Participant->AbilitySystem->GetEntityId() == Id && Participant->AbilitySystem->GetOwnerActor() == Actor.Get() &&
		Participant->AbilitySystem->GetAvatarActor() == Actor.Get();
}

bool UKnightCombatDirectorSubsystem::IsActiveEntryValid(const FKnightActiveAttackLease& Entry) const
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
	const FKnightAttackParticipant* Source = Participants.Find(Entry.Source);
	const FKnightAttackParticipant* Target = Participants.Find(Entry.Target);
	return KnightDirectorHostile(Source->AbilitySystem.Get(), Target->AbilitySystem.Get());
}

bool UKnightCombatDirectorSubsystem::IsLeaseActive(const FKnightAttackLease& Lease) const
{
	const FKnightActiveAttackLease* Entry = ActiveLeases.Find(Lease.LeaseId);
	return Entry != nullptr && Entry->Capability == Lease && IsActiveEntryValid(*Entry);
}

int32 UKnightCombatDirectorSubsystem::GetActiveLeaseCount() const
{
	int32 Count = 0;
	for (const TPair<FGuid, FKnightActiveAttackLease>& Entry : ActiveLeases)
	{
		Count += IsActiveEntryValid(Entry.Value) ? 1 : 0;
	}
	return Count;
}

int32 UKnightCombatDirectorSubsystem::GetActiveLeaseCountForTarget(const FKnightEntityId& TargetId) const
{
	int32 Count = 0;
	for (const TPair<FGuid, FKnightActiveAttackLease>& Entry : ActiveLeases)
	{
		Count += Entry.Value.Capability.TargetId == TargetId && IsActiveEntryValid(Entry.Value) ? 1 : 0;
	}
	return Count;
}

int32 UKnightCombatDirectorSubsystem::GetGlobalBudget() const
{
	return GlobalBudget;
}

int32 UKnightCombatDirectorSubsystem::GetPerTargetBudget() const
{
	return PerTargetBudget;
}

bool UKnightCombatDirectorSubsystem::HasMaintenanceTimer() const
{
	return GetWorld() != nullptr && GetWorld()->GetTimerManager().IsTimerActive(MaintenanceTimer);
}

void UKnightCombatDirectorSubsystem::RegisterParticipant(AActor* Actor, UKnightAbilitySystemComponent* AbilitySystem)
{
	FKnightAttackParticipant& Participant = Participants.FindOrAdd(Actor);
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
	const TWeakObjectPtr<UKnightCombatDirectorSubsystem> WeakThis(this);
	const TWeakObjectPtr<AActor> WeakActor(Actor);
	Participant.LifeHandle = Participant.Life->OnLifeStateChanged.AddWeakLambda(this,
		[WeakThis, WeakActor](EKnightLifeState Previous, EKnightLifeState Current)
		{
			if (Current != EKnightLifeState::Alive && WeakThis.IsValid() && WeakActor.IsValid())
			{
				WeakThis->ReleaseActorLeases(WeakActor.Get(), EKnightAttackLeaseReleaseReason::LifeUnavailable);
			}
		});
}

void UKnightCombatDirectorSubsystem::UnregisterParticipant(const TWeakObjectPtr<AActor>& Actor)
{
	FKnightAttackParticipant* Participant = Participants.Find(Actor);
	if (Participant == nullptr || --Participant->References > 0)
	{
		return;
	}
	const FKnightAttackParticipant Removed = *Participant;
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

void UKnightCombatDirectorSubsystem::ReleaseLeaseInternal(const FGuid& LeaseId,
	EKnightAttackLeaseReleaseReason Reason, bool bNotify)
{
	FKnightActiveAttackLease Entry;
	if (!ActiveLeases.RemoveAndCopyValue(LeaseId, Entry))
	{
		return;
	}
	UnregisterParticipant(Entry.Source);
	UnregisterParticipant(Entry.Target);
	FKnightReleasedAttackLease& Released = LastReleasedLeases.FindOrAdd(Entry.Capability.SourceId);
	Released.Capability = Entry.Capability;
	Released.Source = Entry.Source;
	UpdateMaintenanceTimer();
	if (bReady && bNotify && GetWorld() != nullptr && !GetWorld()->bIsTearingDown)
	{
		OnAttackLeaseReleased.Broadcast(Entry.Capability, Reason);
	}
}

void UKnightCombatDirectorSubsystem::ReleaseActorLeases(AActor* Actor, EKnightAttackLeaseReleaseReason Reason)
{
	TArray<FGuid> Ids;
	for (const TPair<FGuid, FKnightActiveAttackLease>& Entry : ActiveLeases)
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

void UKnightCombatDirectorSubsystem::HandleActorEndPlay(AActor* Actor, EEndPlayReason::Type Reason)
{
	ReleaseActorLeases(Actor, EKnightAttackLeaseReleaseReason::ActorEndPlay);
}

void UKnightCombatDirectorSubsystem::HandleControllerChanged(APawn* Pawn, AController* Previous,
	AController* Current)
{
	if (Previous != Current)
	{
		ReleaseActorLeases(Pawn, EKnightAttackLeaseReleaseReason::ControllerChanged);
	}
}

void UKnightCombatDirectorSubsystem::MaintainLeases()
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
		const FKnightActiveAttackLease* Entry = ActiveLeases.Find(Id);
		if (Entry != nullptr && !IsActiveEntryValid(*Entry))
		{
			const EKnightAttackLeaseReleaseReason Reason = GetWorld()->GetTimeSeconds() >= Entry->ExpiresAt
				? EKnightAttackLeaseReleaseReason::Expired : EKnightAttackLeaseReleaseReason::InvalidParticipant;
			ReleaseLeaseInternal(Id, Reason);
		}
	}
	for (TMap<FKnightEntityId, FKnightReleasedAttackLease>::TIterator It = LastReleasedLeases.CreateIterator(); It; ++It)
	{
		if (!It.Value().Source.IsValid() || It.Value().Source->IsActorBeingDestroyed())
		{
			It.RemoveCurrent();
		}
	}
	UpdateMaintenanceTimer();
}

void UKnightCombatDirectorSubsystem::UpdateMaintenanceTimer()
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

void UKnightCombatDirectorSubsystem::ShutdownLeases()
{
	bReady = false;
	TArray<FGuid> Ids;
	ActiveLeases.GetKeys(Ids);
	for (const FGuid& Id : Ids)
	{
		ReleaseLeaseInternal(Id, EKnightAttackLeaseReleaseReason::WorldEndPlay, false);
	}
	LastReleasedLeases.Reset();
	UpdateMaintenanceTimer();
}

void UKnightCombatDirectorSubsystem::OnWorldEndPlay(UWorld& InWorld)
{
	ShutdownLeases();
	Super::OnWorldEndPlay(InWorld);
}

void UKnightCombatDirectorSubsystem::Deinitialize()
{
	ShutdownLeases();
	OnAttackLeaseReleased.Clear();
	Super::Deinitialize();
}
