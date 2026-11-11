#include "Runtime/GolemStoryWorldComponent.h"

#include "Engine/World.h"
#include "Events/GolemEventSubsystem.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Interaction/GolemStoryAnchor.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

UGolemStoryWorldComponent::UGolemStoryWorldComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UGolemStoryWorldComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UGolemStoryWorldComponent, Snapshot);
	DOREPLIFETIME(UGolemStoryWorldComponent, ActiveRegionId);
	DOREPLIFETIME(UGolemStoryWorldComponent, MapPings);
}

FGolemResult UGolemStoryWorldComponent::InitializeWorld(const FName CampaignId, const FGuid& RunId)
{
	if (!GetOwner()->HasAuthority() || !Snapshot.CampaignId.IsNone()
		|| !GolemIdentity::IsValidIdentifier(CampaignId) || !RunId.IsValid())
	{
		return FGolemResult::Failure(EGolemError::InvalidState, TEXT("StoryWorldInitialize"), FText::GetEmpty());
	}
	Snapshot.CampaignId = CampaignId;
	Snapshot.RunId = RunId;
	PublishSnapshot();
	return FGolemResult::Success();
}

FGolemResult UGolemStoryWorldComponent::BindAnchor(AGolemStoryAnchor* Anchor)
{
	if (!IsValid(Anchor) || Anchor->GetWorld() != GetWorld() || !GolemIdentity::IsValidIdentifier(Anchor->AnchorId))
	{
		return FGolemResult::Failure(EGolemError::InvalidId, TEXT("WorldBinding"), FText::GetEmpty());
	}
	if (AGolemStoryAnchor* Previous = FindAnchor(Anchor->AnchorId))
	{
		if (Previous != Anchor)
		{
			return FGolemResult::Failure(EGolemError::DuplicateId, Anchor->AnchorId, FText::GetEmpty());
		}
	}
	Anchors.Add(Anchor->AnchorId, Anchor);
	return FGolemResult::Success();
}

AGolemStoryAnchor* UGolemStoryWorldComponent::FindAnchor(const FName AnchorId) const
{
	const TWeakObjectPtr<AGolemStoryAnchor>* Entry = Anchors.Find(AnchorId);
	return Entry != nullptr && Entry->IsValid() && !Entry->Get()->IsActorBeingDestroyed() ? Entry->Get() : nullptr;
}

FGolemResult UGolemStoryWorldComponent::ReserveInteraction(const FName AnchorId, APawn* Pawn,
	const FGuid& RequestId, const int64 ExpectedRevision, const float Range, FGuid& OutToken)
{
	OutToken.Invalidate();
	PruneReservations();
	if (!GetOwner()->HasAuthority() || !IsValid(Pawn) || Pawn->GetWorld() != GetWorld()
		|| Pawn->GetController() == nullptr || !RequestId.IsValid() || Range <= 0.0f || Range > 1000.0f
		|| Snapshot.Revision != ExpectedRevision)
	{
		return FGolemResult::Failure(EGolemError::InvalidRevision, AnchorId, FText::GetEmpty());
	}
	for (const TPair<FGuid, FReservation>& Entry : Reservations)
	{
		if (Entry.Value.AnchorId == AnchorId)
		{
			if (Entry.Value.Pawn == Pawn && Entry.Value.RequestId == RequestId)
			{
				OutToken = Entry.Key;
				return ValidateReservation(OutToken);
			}
			return FGolemResult::Failure(EGolemError::Conflict, AnchorId,
				NSLOCTEXT("GolemWorld", "Reserved", "This station is in use by another Golem."));
		}
	}
	FReservation Reservation;
	Reservation.AnchorId = AnchorId;
	Reservation.Pawn = Pawn;
	Reservation.Controller = Pawn->GetController();
	Reservation.RequestId = RequestId;
	Reservation.Revision = Snapshot.Revision;
	Reservation.Range = Range;
	Reservation.ExpiresAt = GetWorld()->GetTimeSeconds() + 8.0;
	const FGuid Token = FGuid::NewGuid();
	Reservations.Add(Token, Reservation);
	FGolemResult Result = ValidateReservation(Token);
	if (!Result.IsSuccess())
	{
		Reservations.Remove(Token);
		return Result;
	}
	OutToken = Token;
	GetWorld()->GetTimerManager().SetTimer(LeaseTimer, this, &ThisClass::PruneReservations, 0.25f, true);
	return FGolemResult::Success();
}

FGolemResult UGolemStoryWorldComponent::ValidateReservation(const FGuid& Token) const
{
	const FReservation* Reservation = Reservations.Find(Token);
	AGolemStoryAnchor* Anchor = Reservation != nullptr ? FindAnchor(Reservation->AnchorId) : nullptr;
	APawn* Pawn = Reservation != nullptr ? Reservation->Pawn.Get() : nullptr;
	if (Reservation == nullptr || !GetOwner()->HasAuthority() || Anchor == nullptr || !IsValid(Pawn)
		|| Pawn->IsActorBeingDestroyed() || !Reservation->Controller.IsValid()
		|| Pawn->GetController() != Reservation->Controller.Get() || GetWorld()->bIsTearingDown
		|| Snapshot.Revision != Reservation->Revision || GetWorld()->GetTimeSeconds() > Reservation->ExpiresAt
		|| FVector::DistSquared(Pawn->GetActorLocation(), Anchor->GetActorLocation())
			> FMath::Square(Reservation->Range))
	{
		return FGolemResult::Failure(EGolemError::OutOfRange, TEXT("InteractionLease"),
			NSLOCTEXT("GolemWorld", "LeaseInvalid", "Move close to the visible station and try again."));
	}
	FHitResult Hit;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(GolemStoryInteraction), false, Pawn);
	Query.AddIgnoredActor(Anchor);
	if (GetWorld()->LineTraceSingleByChannel(Hit, Pawn->GetActorLocation(),
		Anchor->GetActorLocation() + FVector(0.0, 0.0, 60.0), ECC_Visibility, Query))
	{
		return FGolemResult::Failure(EGolemError::OutOfRange, Reservation->AnchorId,
			NSLOCTEXT("GolemWorld", "StationOccluded", "The station must be visible."));
	}
	return FGolemResult::Success();
}

void UGolemStoryWorldComponent::ReleaseReservation(const FGuid& Token)
{
	Reservations.Remove(Token);
	if (Reservations.IsEmpty())
	{
		GetWorld()->GetTimerManager().ClearTimer(LeaseTimer);
	}
}

void UGolemStoryWorldComponent::ReleasePawnReservations(const APawn* Pawn)
{
	TArray<FGuid> Removed;
	for (const TPair<FGuid, FReservation>& Entry : Reservations)
	{
		if (Entry.Value.Pawn.Get() == Pawn)
		{
			Removed.Add(Entry.Key);
		}
	}
	for (const FGuid Token : Removed)
	{
		ReleaseReservation(Token);
	}
}

void UGolemStoryWorldComponent::PruneReservations()
{
	TArray<FGuid> Removed;
	for (const TPair<FGuid, FReservation>& Entry : Reservations)
	{
		if (!ValidateReservation(Entry.Key).IsSuccess())
		{
			Removed.Add(Entry.Key);
		}
	}
	for (const FGuid Token : Removed)
	{
		ReleaseReservation(Token);
	}
}

FGolemResult UGolemStoryWorldComponent::CommitFact(const FName EventName, const FName SubjectId,
	const FName ValueId, const bool bPersistent, const FGuid& ConfirmationId, const int32 Quantity)
{
	if (!GetOwner()->HasAuthority() || EventName.IsNone() || SubjectId.IsNone() || Quantity <= 0
		|| Snapshot.CampaignId.IsNone() || !Snapshot.RunId.IsValid())
	{
		return FGolemResult::Failure(EGolemError::InvalidData, SubjectId, FText::GetEmpty());
	}
	const FGuid Id = ConfirmationId.IsValid() ? ConfirmationId : FGuid::NewGuid();
	if (Snapshot.Confirmations.Contains(Id))
	{
		return FGolemResult::Success();
	}
	FGolemWorldFact* Existing = Snapshot.Facts.FindByPredicate([EventName, SubjectId](const FGolemWorldFact& Row)
	{
		return Row.EventName == EventName && Row.SubjectId == SubjectId;
	});
	if (Existing == nullptr)
	{
		Existing = &Snapshot.Facts.AddDefaulted_GetRef();
		Existing->EventName = EventName;
		Existing->SubjectId = SubjectId;
	}
	Existing->ValueId = ValueId;
	Existing->bPersistent = bPersistent;
	Snapshot.Confirmations.Add(Id);
	++Snapshot.FactRevision;
	FGolemStoryEvent Fact;
	Fact.ConfirmationId = Id;
	Fact.Revision = Snapshot.FactRevision;
	Fact.SourceId = FGolemEntityId(Snapshot.RunId);
	Fact.CampaignId = Snapshot.CampaignId;
	Fact.RunId = Snapshot.RunId;
	Fact.EncounterRunId = Snapshot.EncounterRunId;
	Fact.EventName = EventName;
	Fact.SubjectId = SubjectId;
	Fact.ValueId = ValueId;
	Fact.Quantity = Quantity;
	PublishSnapshot();
	return GetWorld()->GetSubsystem<UGolemEventSubsystem>()->PublishConfirmed(Fact);
}

FGolemResult UGolemStoryWorldComponent::BeginEncounter()
{
	return BeginEncounter(FName(TEXT("Fixture")));
}

FGolemResult UGolemStoryWorldComponent::BeginEncounter(const FName EncounterId)
{
	if (!GetOwner()->HasAuthority() || Snapshot.EncounterPhase == EGolemStoryEncounterPhase::Active
		|| !GolemIdentity::IsValidIdentifier(EncounterId))
	{
		return FGolemResult::Failure(EGolemError::InvalidState, TEXT("EncounterStart"), FText::GetEmpty());
	}
	Snapshot.EncounterRunId = FGuid::NewGuid();
	Snapshot.EncounterId = EncounterId;
	Snapshot.EncounterPhase = EGolemStoryEncounterPhase::Active;
	// Persistent lessons/gates remain. Runestone buffs, surface and armor facts reset with the encounter.
	Snapshot.Facts.RemoveAll([](const FGolemWorldFact& Fact)
	{
		return !Fact.bPersistent;
	});
	PublishSnapshot();
	return FGolemResult::Success();
}

FGolemResult UGolemStoryWorldComponent::FinishEncounter(const bool bSuccess, const FGuid& ExpectedRunId)
{
	if (!GetOwner()->HasAuthority() || Snapshot.EncounterRunId != ExpectedRunId
		|| Snapshot.EncounterPhase != EGolemStoryEncounterPhase::Active)
	{
		return FGolemResult::Failure(EGolemError::InvalidState, TEXT("EncounterOutcome"), FText::GetEmpty());
	}
	Snapshot.EncounterPhase = bSuccess ? EGolemStoryEncounterPhase::Complete : EGolemStoryEncounterPhase::Failed;
	PublishSnapshot();
	return CommitFact(bSuccess ? FName(TEXT("EncounterCompleted")) : FName(TEXT("EncounterFailed")),
		Snapshot.EncounterId, NAME_None, false);
}

FGolemResult UGolemStoryWorldComponent::BeginScene(const FName SceneId)
{
	if (!GetOwner()->HasAuthority() || Snapshot.bSceneActive || SceneId.IsNone())
	{
		return FGolemResult::Failure(EGolemError::InvalidState, SceneId, FText::GetEmpty());
	}
	Snapshot.SceneId = SceneId;
	Snapshot.SceneRunId = FGuid::NewGuid();
	Snapshot.SceneOutcome = NAME_None;
	Snapshot.bSceneActive = true;
	PublishSnapshot();
	return FGolemResult::Success();
}

FGolemResult UGolemStoryWorldComponent::FinishScene(const FName Outcome, const FGuid& ExpectedSceneRun)
{
	if (!GetOwner()->HasAuthority() || Snapshot.SceneRunId != ExpectedSceneRun || Outcome.IsNone())
	{
		return FGolemResult::Failure(EGolemError::InvalidState, TEXT("SceneOutcome"), FText::GetEmpty());
	}
	if (!Snapshot.bSceneActive)
	{
		return Snapshot.SceneOutcome == Outcome ? FGolemResult::Success()
			: FGolemResult::Failure(EGolemError::Conflict, Snapshot.SceneId, FText::GetEmpty());
	}
	Snapshot.bSceneActive = false;
	Snapshot.SceneOutcome = Outcome;
	PublishSnapshot();
	return CommitFact(TEXT("SceneFinished"), Snapshot.SceneId, Outcome, true, Snapshot.SceneRunId);
}

FGolemResult UGolemStoryWorldComponent::SetCheckpoint(const FName AnchorId)
{
	if (!GetOwner()->HasAuthority() || FindAnchor(AnchorId) == nullptr)
	{
		return FGolemResult::Failure(EGolemError::NotFound, AnchorId, FText::GetEmpty());
	}
	Snapshot.CheckpointAnchorId = AnchorId;
	PublishSnapshot();
	return FGolemResult::Success();
}

FGolemResult UGolemStoryWorldComponent::ValidateSnapshot(const FGolemStoryWorldSnapshot& Candidate) const
{
	if (Candidate.CampaignId != Snapshot.CampaignId || !Candidate.RunId.IsValid()
		|| Candidate.Revision < 1 || Candidate.FactRevision < 0 || Candidate.bSceneActive
		|| Candidate.EncounterPhase == EGolemStoryEncounterPhase::Active
		|| Candidate.EncounterPhase > EGolemStoryEncounterPhase::Complete)
	{
		return FGolemResult::Failure(EGolemError::InvalidData, TEXT("WorldCheckpoint"), FText::GetEmpty());
	}
	TSet<FGuid> Ids;
	for (const FGuid Id : Candidate.Confirmations)
	{
		if (!Id.IsValid() || Ids.Contains(Id))
		{
			return FGolemResult::Failure(EGolemError::InvalidData, TEXT("WorldConfirmations"), FText::GetEmpty());
		}
		Ids.Add(Id);
	}
	return FGolemResult::Success();
}

FGolemResult UGolemStoryWorldComponent::RestoreSnapshot(const FGolemStoryWorldSnapshot& Candidate)
{
	FGolemResult Result = ValidateSnapshot(Candidate);
	if (!GetOwner()->HasAuthority() || !Result.IsSuccess())
	{
		return Result.IsSuccess() ? FGolemResult::Failure(EGolemError::NotAuthority,
			TEXT("WorldRestore"), FText::GetEmpty()) : Result;
	}
	Reservations.Reset();
	GetWorld()->GetTimerManager().ClearTimer(LeaseTimer);
	GetWorld()->GetTimerManager().ClearTimer(PingTimer);
	MapPings.Reset();
	PingTargets.Reset();
	LastPingTimes.Reset();
	Snapshot = Candidate;
	PublishSnapshot();
	return FGolemResult::Success();
}

const FGolemStoryWorldSnapshot& UGolemStoryWorldComponent::GetSnapshot() const
{
	return Snapshot;
}

bool UGolemStoryWorldComponent::HasReservations() const
{
	return !Reservations.IsEmpty();
}

void UGolemStoryWorldComponent::PublishSnapshot()
{
	++Snapshot.Revision;
	GetOwner()->ForceNetUpdate();
	OnSnapshotChanged.Broadcast();
}

void UGolemStoryWorldComponent::OnRep_Snapshot()
{
	OnSnapshotChanged.Broadcast();
}

void UGolemStoryWorldComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	GetWorld()->GetTimerManager().ClearTimer(LeaseTimer);
	GetWorld()->GetTimerManager().ClearTimer(PingTimer);
	MapPings.Reset();
	PingTargets.Reset();
	LastPingTimes.Reset();
	MapDefinition = nullptr;
	Reservations.Reset();
	Anchors.Reset();
	Super::EndPlay(Reason);
}
