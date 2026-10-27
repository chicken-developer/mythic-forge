#include "Runtime/KnightStoryWorldComponent.h"

#include "Engine/World.h"
#include "Events/KnightEventSubsystem.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Interaction/KnightStoryAnchor.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

UKnightStoryWorldComponent::UKnightStoryWorldComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UKnightStoryWorldComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UKnightStoryWorldComponent, Snapshot);
	DOREPLIFETIME(UKnightStoryWorldComponent, ActiveRegionId);
	DOREPLIFETIME(UKnightStoryWorldComponent, MapPings);
}

FKnightResult UKnightStoryWorldComponent::InitializeWorld(const FName CampaignId, const FGuid& RunId)
{
	if (!GetOwner()->HasAuthority() || !Snapshot.CampaignId.IsNone()
		|| !KnightIdentity::IsValidIdentifier(CampaignId) || !RunId.IsValid())
	{
		return FKnightResult::Failure(EKnightError::InvalidState, TEXT("StoryWorldInitialize"), FText::GetEmpty());
	}
	Snapshot.CampaignId = CampaignId;
	Snapshot.RunId = RunId;
	PublishSnapshot();
	return FKnightResult::Success();
}

FKnightResult UKnightStoryWorldComponent::BindAnchor(AKnightStoryAnchor* Anchor)
{
	if (!IsValid(Anchor) || Anchor->GetWorld() != GetWorld() || !KnightIdentity::IsValidIdentifier(Anchor->AnchorId))
	{
		return FKnightResult::Failure(EKnightError::InvalidId, TEXT("WorldBinding"), FText::GetEmpty());
	}
	if (AKnightStoryAnchor* Previous = FindAnchor(Anchor->AnchorId))
	{
		if (Previous != Anchor)
		{
			return FKnightResult::Failure(EKnightError::DuplicateId, Anchor->AnchorId, FText::GetEmpty());
		}
	}
	Anchors.Add(Anchor->AnchorId, Anchor);
	return FKnightResult::Success();
}

AKnightStoryAnchor* UKnightStoryWorldComponent::FindAnchor(const FName AnchorId) const
{
	const TWeakObjectPtr<AKnightStoryAnchor>* Entry = Anchors.Find(AnchorId);
	return Entry != nullptr && Entry->IsValid() && !Entry->Get()->IsActorBeingDestroyed() ? Entry->Get() : nullptr;
}

FKnightResult UKnightStoryWorldComponent::ReserveInteraction(const FName AnchorId, APawn* Pawn,
	const FGuid& RequestId, const int64 ExpectedRevision, const float Range, FGuid& OutToken)
{
	OutToken.Invalidate();
	PruneReservations();
	if (!GetOwner()->HasAuthority() || !IsValid(Pawn) || Pawn->GetWorld() != GetWorld()
		|| Pawn->GetController() == nullptr || !RequestId.IsValid() || Range <= 0.0f || Range > 1000.0f
		|| Snapshot.Revision != ExpectedRevision)
	{
		return FKnightResult::Failure(EKnightError::InvalidRevision, AnchorId, FText::GetEmpty());
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
			return FKnightResult::Failure(EKnightError::Conflict, AnchorId,
				NSLOCTEXT("KnightWorld", "Reserved", "This station is in use by another Knight."));
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
	FKnightResult Result = ValidateReservation(Token);
	if (!Result.IsSuccess())
	{
		Reservations.Remove(Token);
		return Result;
	}
	OutToken = Token;
	GetWorld()->GetTimerManager().SetTimer(LeaseTimer, this, &ThisClass::PruneReservations, 0.25f, true);
	return FKnightResult::Success();
}

FKnightResult UKnightStoryWorldComponent::ValidateReservation(const FGuid& Token) const
{
	const FReservation* Reservation = Reservations.Find(Token);
	AKnightStoryAnchor* Anchor = Reservation != nullptr ? FindAnchor(Reservation->AnchorId) : nullptr;
	APawn* Pawn = Reservation != nullptr ? Reservation->Pawn.Get() : nullptr;
	if (Reservation == nullptr || !GetOwner()->HasAuthority() || Anchor == nullptr || !IsValid(Pawn)
		|| Pawn->IsActorBeingDestroyed() || !Reservation->Controller.IsValid()
		|| Pawn->GetController() != Reservation->Controller.Get() || GetWorld()->bIsTearingDown
		|| Snapshot.Revision != Reservation->Revision || GetWorld()->GetTimeSeconds() > Reservation->ExpiresAt
		|| FVector::DistSquared(Pawn->GetActorLocation(), Anchor->GetActorLocation())
			> FMath::Square(Reservation->Range))
	{
		return FKnightResult::Failure(EKnightError::OutOfRange, TEXT("InteractionLease"),
			NSLOCTEXT("KnightWorld", "LeaseInvalid", "Move close to the visible station and try again."));
	}
	FHitResult Hit;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(KnightStoryInteraction), false, Pawn);
	Query.AddIgnoredActor(Anchor);
	if (GetWorld()->LineTraceSingleByChannel(Hit, Pawn->GetActorLocation(),
		Anchor->GetActorLocation() + FVector(0.0, 0.0, 60.0), ECC_Visibility, Query))
	{
		return FKnightResult::Failure(EKnightError::OutOfRange, Reservation->AnchorId,
			NSLOCTEXT("KnightWorld", "StationOccluded", "The station must be visible."));
	}
	return FKnightResult::Success();
}

void UKnightStoryWorldComponent::ReleaseReservation(const FGuid& Token)
{
	Reservations.Remove(Token);
	if (Reservations.IsEmpty())
	{
		GetWorld()->GetTimerManager().ClearTimer(LeaseTimer);
	}
}

void UKnightStoryWorldComponent::ReleasePawnReservations(const APawn* Pawn)
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

void UKnightStoryWorldComponent::PruneReservations()
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

FKnightResult UKnightStoryWorldComponent::CommitFact(const FName EventName, const FName SubjectId,
	const FName ValueId, const bool bPersistent, const FGuid& ConfirmationId, const int32 Quantity)
{
	if (!GetOwner()->HasAuthority() || EventName.IsNone() || SubjectId.IsNone() || Quantity <= 0
		|| Snapshot.CampaignId.IsNone() || !Snapshot.RunId.IsValid())
	{
		return FKnightResult::Failure(EKnightError::InvalidData, SubjectId, FText::GetEmpty());
	}
	const FGuid Id = ConfirmationId.IsValid() ? ConfirmationId : FGuid::NewGuid();
	if (Snapshot.Confirmations.Contains(Id))
	{
		return FKnightResult::Success();
	}
	FKnightWorldFact* Existing = Snapshot.Facts.FindByPredicate([EventName, SubjectId](const FKnightWorldFact& Row)
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
	FKnightStoryEvent Fact;
	Fact.ConfirmationId = Id;
	Fact.Revision = Snapshot.FactRevision;
	Fact.SourceId = FKnightEntityId(Snapshot.RunId);
	Fact.CampaignId = Snapshot.CampaignId;
	Fact.RunId = Snapshot.RunId;
	Fact.EncounterRunId = Snapshot.EncounterRunId;
	Fact.EventName = EventName;
	Fact.SubjectId = SubjectId;
	Fact.ValueId = ValueId;
	Fact.Quantity = Quantity;
	PublishSnapshot();
	return GetWorld()->GetSubsystem<UKnightEventSubsystem>()->PublishConfirmed(Fact);
}

FKnightResult UKnightStoryWorldComponent::BeginEncounter()
{
	return BeginEncounter(FName(TEXT("Fixture")));
}

FKnightResult UKnightStoryWorldComponent::BeginEncounter(const FName EncounterId)
{
	if (!GetOwner()->HasAuthority() || Snapshot.EncounterPhase == EKnightStoryEncounterPhase::Active
		|| !KnightIdentity::IsValidIdentifier(EncounterId))
	{
		return FKnightResult::Failure(EKnightError::InvalidState, TEXT("EncounterStart"), FText::GetEmpty());
	}
	Snapshot.EncounterRunId = FGuid::NewGuid();
	Snapshot.EncounterId = EncounterId;
	Snapshot.EncounterPhase = EKnightStoryEncounterPhase::Active;
	// Persistent lessons/gates remain. Runestone buffs, surface and armor facts reset with the encounter.
	Snapshot.Facts.RemoveAll([](const FKnightWorldFact& Fact)
	{
		return !Fact.bPersistent;
	});
	PublishSnapshot();
	return FKnightResult::Success();
}

FKnightResult UKnightStoryWorldComponent::FinishEncounter(const bool bSuccess, const FGuid& ExpectedRunId)
{
	if (!GetOwner()->HasAuthority() || Snapshot.EncounterRunId != ExpectedRunId
		|| Snapshot.EncounterPhase != EKnightStoryEncounterPhase::Active)
	{
		return FKnightResult::Failure(EKnightError::InvalidState, TEXT("EncounterOutcome"), FText::GetEmpty());
	}
	Snapshot.EncounterPhase = bSuccess ? EKnightStoryEncounterPhase::Complete : EKnightStoryEncounterPhase::Failed;
	PublishSnapshot();
	return CommitFact(bSuccess ? FName(TEXT("EncounterCompleted")) : FName(TEXT("EncounterFailed")),
		Snapshot.EncounterId, NAME_None, false);
}

FKnightResult UKnightStoryWorldComponent::BeginScene(const FName SceneId)
{
	if (!GetOwner()->HasAuthority() || Snapshot.bSceneActive || SceneId.IsNone())
	{
		return FKnightResult::Failure(EKnightError::InvalidState, SceneId, FText::GetEmpty());
	}
	Snapshot.SceneId = SceneId;
	Snapshot.SceneRunId = FGuid::NewGuid();
	Snapshot.SceneOutcome = NAME_None;
	Snapshot.bSceneActive = true;
	PublishSnapshot();
	return FKnightResult::Success();
}

FKnightResult UKnightStoryWorldComponent::FinishScene(const FName Outcome, const FGuid& ExpectedSceneRun)
{
	if (!GetOwner()->HasAuthority() || Snapshot.SceneRunId != ExpectedSceneRun || Outcome.IsNone())
	{
		return FKnightResult::Failure(EKnightError::InvalidState, TEXT("SceneOutcome"), FText::GetEmpty());
	}
	if (!Snapshot.bSceneActive)
	{
		return Snapshot.SceneOutcome == Outcome ? FKnightResult::Success()
			: FKnightResult::Failure(EKnightError::Conflict, Snapshot.SceneId, FText::GetEmpty());
	}
	Snapshot.bSceneActive = false;
	Snapshot.SceneOutcome = Outcome;
	PublishSnapshot();
	return CommitFact(TEXT("SceneFinished"), Snapshot.SceneId, Outcome, true, Snapshot.SceneRunId);
}

FKnightResult UKnightStoryWorldComponent::SetCheckpoint(const FName AnchorId)
{
	if (!GetOwner()->HasAuthority() || FindAnchor(AnchorId) == nullptr)
	{
		return FKnightResult::Failure(EKnightError::NotFound, AnchorId, FText::GetEmpty());
	}
	Snapshot.CheckpointAnchorId = AnchorId;
	PublishSnapshot();
	return FKnightResult::Success();
}

FKnightResult UKnightStoryWorldComponent::ValidateSnapshot(const FKnightStoryWorldSnapshot& Candidate) const
{
	if (Candidate.CampaignId != Snapshot.CampaignId || !Candidate.RunId.IsValid()
		|| Candidate.Revision < 1 || Candidate.FactRevision < 0 || Candidate.bSceneActive
		|| Candidate.EncounterPhase == EKnightStoryEncounterPhase::Active
		|| Candidate.EncounterPhase > EKnightStoryEncounterPhase::Complete)
	{
		return FKnightResult::Failure(EKnightError::InvalidData, TEXT("WorldCheckpoint"), FText::GetEmpty());
	}
	TSet<FGuid> Ids;
	for (const FGuid Id : Candidate.Confirmations)
	{
		if (!Id.IsValid() || Ids.Contains(Id))
		{
			return FKnightResult::Failure(EKnightError::InvalidData, TEXT("WorldConfirmations"), FText::GetEmpty());
		}
		Ids.Add(Id);
	}
	return FKnightResult::Success();
}

FKnightResult UKnightStoryWorldComponent::RestoreSnapshot(const FKnightStoryWorldSnapshot& Candidate)
{
	FKnightResult Result = ValidateSnapshot(Candidate);
	if (!GetOwner()->HasAuthority() || !Result.IsSuccess())
	{
		return Result.IsSuccess() ? FKnightResult::Failure(EKnightError::NotAuthority,
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
	return FKnightResult::Success();
}

const FKnightStoryWorldSnapshot& UKnightStoryWorldComponent::GetSnapshot() const
{
	return Snapshot;
}

bool UKnightStoryWorldComponent::HasReservations() const
{
	return !Reservations.IsEmpty();
}

void UKnightStoryWorldComponent::PublishSnapshot()
{
	++Snapshot.Revision;
	GetOwner()->ForceNetUpdate();
	OnSnapshotChanged.Broadcast();
}

void UKnightStoryWorldComponent::OnRep_Snapshot()
{
	OnSnapshotChanged.Broadcast();
}

void UKnightStoryWorldComponent::EndPlay(const EEndPlayReason::Type Reason)
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
