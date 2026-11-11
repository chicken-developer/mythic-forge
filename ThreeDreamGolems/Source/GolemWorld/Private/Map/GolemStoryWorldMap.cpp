#include "Runtime/GolemStoryWorldComponent.h"

#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Interaction/GolemStoryAnchor.h"
#include "Types/GolemIdentifiableInterface.h"
#include "TimerManager.h"

FGolemResult UGolemStoryWorldComponent::ConfigureMap(UGolemStoryWorldDefinition* Definition, const FName RegionId)
{
	if (!GetOwner()->HasAuthority() || Definition == nullptr || !Definition->ValidateWorld().IsSuccess()
		|| Definition->FindRegion(RegionId) == nullptr)
	{
		return FGolemResult::Failure(EGolemError::InvalidData, TEXT("MapRegion"), FText::GetEmpty());
	}
	MapDefinition = Definition;
	ActiveRegionId = RegionId;
	MapPings.Reset();
	PingTargets.Reset();
	LastPingTimes.Reset();
	GetOwner()->ForceNetUpdate();
	OnSnapshotChanged.Broadcast();
	return FGolemResult::Success();
}

bool UGolemStoryWorldComponent::IsMarkerDiscovered(const FName MarkerId) const
{
	return Snapshot.Facts.ContainsByPredicate([MarkerId](const FGolemWorldFact& Fact)
	{
		return Fact.EventName == TEXT("MapDiscovered") && Fact.SubjectId == MarkerId && Fact.bPersistent;
	});
}

bool UGolemStoryWorldComponent::IsRegionDiscovered(const FName RegionId) const
{
	return Snapshot.Facts.ContainsByPredicate([RegionId](const FGolemWorldFact& Fact)
	{
		return Fact.EventName == TEXT("MapDiscovered") && Fact.ValueId == RegionId && Fact.bPersistent;
	});
}

FName UGolemStoryWorldComponent::GetActiveRegionId() const
{
	return ActiveRegionId;
}

void UGolemStoryWorldComponent::RevealNearbyMarkers(const TConstArrayView<FVector> Positions)
{
	if (!GetOwner()->HasAuthority() || MapDefinition == nullptr)
	{
		return;
	}
	for (const FGolemMapMarker& Marker : MapDefinition->MapMarkers)
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

FGolemResult UGolemStoryWorldComponent::RequestMapPing(APawn* Pawn, const FGolemEntityId& GolemId,
	const FName MarkerId)
{
	PruneMapPings();
	const IGolemIdentifiableInterface* Identity = Cast<IGolemIdentifiableInterface>(Pawn);
	const FGolemMapMarker* Marker = MapDefinition != nullptr ? MapDefinition->FindMarker(MarkerId) : nullptr;
	if (!GetOwner()->HasAuthority() || !IsValid(Pawn) || Pawn->GetWorld() != GetWorld()
		|| Pawn->GetController() == nullptr || !GolemId.IsValid()
		|| Identity == nullptr || Identity->GetGolemEntityId() != GolemId
		|| Marker == nullptr || !IsMarkerDiscovered(MarkerId) || Marker->RegionId != ActiveRegionId)
	{
		return FGolemResult::Failure(EGolemError::NotOwned, TEXT("MapPing"),
			NSLOCTEXT("GolemMap", "PingUnavailable", "Select a discovered marker in your current region."));
	}
	const double Now = GetWorld()->GetTimeSeconds();
	const double* Previous = LastPingTimes.Find(GolemId);
	if (Previous != nullptr && Now - *Previous < 1.0)
	{
		return FGolemResult::Failure(EGolemError::Conflict, TEXT("PingRateLimit"),
			NSLOCTEXT("GolemMap", "PingRate", "Wait one second before another ping."));
	}
	int32 Owned = 0;
	for (const FGolemMapPing& Existing : MapPings)
	{
		Owned += Existing.GolemId == GolemId ? 1 : 0;
	}
	if (Owned >= 3 || MapPings.Num() >= 8)
	{
		return FGolemResult::Failure(EGolemError::Conflict, TEXT("PingCapacity"),
			NSLOCTEXT("GolemMap", "PingCapacity", "Ping limit reached. Pings expire after eight seconds."));
	}
	FGolemMapPing& Ping = MapPings.AddDefaulted_GetRef();
	Ping.PingId = FGuid::NewGuid();
	Ping.GolemId = GolemId;
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
			return FGolemResult::Failure(EGolemError::NotFound, Marker->AnchorId, FText::GetEmpty());
		}
		PingTargets.Add(Ping.PingId, Target);
	}
	LastPingTimes.Add(GolemId, Now);
	GetWorld()->GetTimerManager().SetTimer(PingTimer, this, &ThisClass::PruneMapPings, 0.2f, true);
	GetOwner()->ForceNetUpdate();
	OnSnapshotChanged.Broadcast();
	return FGolemResult::Success();
}

void UGolemStoryWorldComponent::PruneMapPings()
{
	if (!GetOwner()->HasAuthority())
	{
		return;
	}
	bool bChanged = false;
	for (int32 Index = MapPings.Num() - 1; Index >= 0; --Index)
	{
		FGolemMapPing& Ping = MapPings[Index];
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

const TArray<FGolemMapPing>& UGolemStoryWorldComponent::GetMapPings() const
{
	return MapPings;
}
