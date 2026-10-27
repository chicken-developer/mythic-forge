#include "Runtime/KnightStoryWorldComponent.h"

#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Interaction/KnightStoryAnchor.h"
#include "Types/KnightIdentifiableInterface.h"
#include "TimerManager.h"

FKnightResult UKnightStoryWorldComponent::ConfigureMap(UKnightStoryWorldDefinition* Definition, const FName RegionId)
{
	if (!GetOwner()->HasAuthority() || Definition == nullptr || !Definition->ValidateWorld().IsSuccess()
		|| Definition->FindRegion(RegionId) == nullptr)
	{
		return FKnightResult::Failure(EKnightError::InvalidData, TEXT("MapRegion"), FText::GetEmpty());
	}
	MapDefinition = Definition;
	ActiveRegionId = RegionId;
	MapPings.Reset();
	PingTargets.Reset();
	LastPingTimes.Reset();
	GetOwner()->ForceNetUpdate();
	OnSnapshotChanged.Broadcast();
	return FKnightResult::Success();
}

bool UKnightStoryWorldComponent::IsMarkerDiscovered(const FName MarkerId) const
{
	return Snapshot.Facts.ContainsByPredicate([MarkerId](const FKnightWorldFact& Fact)
	{
		return Fact.EventName == TEXT("MapDiscovered") && Fact.SubjectId == MarkerId && Fact.bPersistent;
	});
}

bool UKnightStoryWorldComponent::IsRegionDiscovered(const FName RegionId) const
{
	return Snapshot.Facts.ContainsByPredicate([RegionId](const FKnightWorldFact& Fact)
	{
		return Fact.EventName == TEXT("MapDiscovered") && Fact.ValueId == RegionId && Fact.bPersistent;
	});
}

FName UKnightStoryWorldComponent::GetActiveRegionId() const
{
	return ActiveRegionId;
}

void UKnightStoryWorldComponent::RevealNearbyMarkers(const TConstArrayView<FVector> Positions)
{
	if (!GetOwner()->HasAuthority() || MapDefinition == nullptr)
	{
		return;
	}
	for (const FKnightMapMarker& Marker : MapDefinition->MapMarkers)
	{
		if (Marker.RegionId != ActiveRegionId || IsMarkerDiscovered(Marker.MarkerId))
		{
			continue;
		}
		for (const FVector& Position : Positions)
		{
			if (!Position.ContainsNaN()
				&& FVector::DistSquared(Position, Marker.WorldPosition) <= FMath::Square(Marker.DiscoveryRadius))
			{
				CommitFact(TEXT("MapDiscovered"), Marker.MarkerId, Marker.RegionId);
				break;
			}
		}
	}
}

FKnightResult UKnightStoryWorldComponent::RequestMapPing(APawn* Pawn, const FKnightEntityId& KnightId,
	const FName MarkerId)
{
	PruneMapPings();
	const IKnightIdentifiableInterface* Identity = Cast<IKnightIdentifiableInterface>(Pawn);
	const FKnightMapMarker* Marker = MapDefinition != nullptr ? MapDefinition->FindMarker(MarkerId) : nullptr;
	if (!GetOwner()->HasAuthority() || !IsValid(Pawn) || Pawn->GetWorld() != GetWorld()
		|| Pawn->GetController() == nullptr || !KnightId.IsValid()
		|| Identity == nullptr || Identity->GetKnightEntityId() != KnightId
		|| Marker == nullptr || !IsMarkerDiscovered(MarkerId) || Marker->RegionId != ActiveRegionId)
	{
		return FKnightResult::Failure(EKnightError::NotOwned, TEXT("MapPing"),
			NSLOCTEXT("KnightMap", "PingUnavailable", "Select a discovered marker in your current region."));
	}
	const double Now = GetWorld()->GetTimeSeconds();
	const double* Previous = LastPingTimes.Find(KnightId);
	if (Previous != nullptr && Now - *Previous < 1.0)
	{
		return FKnightResult::Failure(EKnightError::Conflict, TEXT("PingRateLimit"),
			NSLOCTEXT("KnightMap", "PingRate", "Wait one second before another ping."));
	}
	int32 Owned = 0;
	for (const FKnightMapPing& Existing : MapPings)
	{
		Owned += Existing.KnightId == KnightId ? 1 : 0;
	}
	if (Owned >= 3 || MapPings.Num() >= 8)
	{
		return FKnightResult::Failure(EKnightError::Conflict, TEXT("PingCapacity"),
			NSLOCTEXT("KnightMap", "PingCapacity", "Ping limit reached. Pings expire after eight seconds."));
	}
	FKnightMapPing& Ping = MapPings.AddDefaulted_GetRef();
	Ping.PingId = FGuid::NewGuid();
	Ping.KnightId = KnightId;
	Ping.RegionId = ActiveRegionId;
	Ping.TargetAnchorId = Marker->AnchorId;
	Ping.WorldPosition = Marker->WorldPosition;
	Ping.ExpiresAt = Now + 8.0;
	if (!Marker->AnchorId.IsNone())
	{
		AActor* Target = FindAnchor(Marker->AnchorId);
		if (Target == nullptr)
		{
			MapPings.Pop();
			return FKnightResult::Failure(EKnightError::NotFound, Marker->AnchorId, FText::GetEmpty());
		}
		PingTargets.Add(Ping.PingId, Target);
	}
	LastPingTimes.Add(KnightId, Now);
	GetWorld()->GetTimerManager().SetTimer(PingTimer, this, &ThisClass::PruneMapPings, 0.2f, true);
	GetOwner()->ForceNetUpdate();
	OnSnapshotChanged.Broadcast();
	return FKnightResult::Success();
}

void UKnightStoryWorldComponent::PruneMapPings()
{
	if (!GetOwner()->HasAuthority())
	{
		return;
	}
	bool bChanged = false;
	for (int32 Index = MapPings.Num() - 1; Index >= 0; --Index)
	{
		FKnightMapPing& Ping = MapPings[Index];
		const TWeakObjectPtr<AActor>* Target = PingTargets.Find(Ping.PingId);
		if (Ping.ExpiresAt <= GetWorld()->GetTimeSeconds()
			|| Target != nullptr && (!Target->IsValid() || Target->Get()->IsActorBeingDestroyed()))
		{
			PingTargets.Remove(Ping.PingId);
			MapPings.RemoveAt(Index);
			bChanged = true;
		}
		else if (Target != nullptr && Ping.WorldPosition != Target->Get()->GetActorLocation() + FVector(GetWorld()->OriginLocation))
		{
			Ping.WorldPosition = Target->Get()->GetActorLocation() + FVector(GetWorld()->OriginLocation);
			bChanged = true;
		}
	}
	if (MapPings.IsEmpty())
	{
		GetWorld()->GetTimerManager().ClearTimer(PingTimer);
	}
	if (bChanged)
	{
		GetOwner()->ForceNetUpdate();
		OnSnapshotChanged.Broadcast();
	}
}

const TArray<FKnightMapPing>& UKnightStoryWorldComponent::GetMapPings() const
{
	return MapPings;
}
