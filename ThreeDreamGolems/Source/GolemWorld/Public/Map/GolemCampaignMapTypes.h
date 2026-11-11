#pragma once

#include "CoreMinimal.h"
#include "Types/GolemStableIds.h"
#include "GolemCampaignMapTypes.generated.h"

UENUM()
enum class EGolemMapMarkerKind : uint8
{
	Landmark,
	Quest,
	Checkpoint,
	Travel,
	Hazard
};

USTRUCT()
struct GOLEMWORLD_API FGolemMapRegion
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly)
	FName RegionId;
	UPROPERTY(EditDefaultsOnly)
	FText Label;
	UPROPERTY(EditDefaultsOnly)
	TSoftObjectPtr<UWorld> Map;
	UPROPERTY(EditDefaultsOnly)
	FVector2D WorldCenter = FVector2D::ZeroVector;
	UPROPERTY(EditDefaultsOnly)
	FVector2D HalfExtent = FVector2D(2500.0, 2500.0);
	UPROPERTY(EditDefaultsOnly)
	float NorthYaw = 0.0f;

	bool Project(const FVector& WorldPosition, FVector2D& OutUV) const;
	bool Unproject(const FVector2D& UV, FVector2D& OutWorldPosition) const;
	bool IsValid() const;
};

USTRUCT()
struct GOLEMWORLD_API FGolemMapMarker
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly)
	FName MarkerId;
	UPROPERTY(EditDefaultsOnly)
	FName RegionId;
	UPROPERTY(EditDefaultsOnly)
	FName AnchorId;
	UPROPERTY(EditDefaultsOnly)
	FText Label;
	UPROPERTY(EditDefaultsOnly)
	EGolemMapMarkerKind Kind = EGolemMapMarkerKind::Landmark;
	UPROPERTY(EditDefaultsOnly)
	FVector WorldPosition = FVector::ZeroVector;
	UPROPERTY(EditDefaultsOnly)
	float DiscoveryRadius = 550.0f;
	UPROPERTY(EditDefaultsOnly)
	FName TravelRegionId;
};

USTRUCT()
struct GOLEMWORLD_API FGolemMapPing
{
	GENERATED_BODY()

	UPROPERTY()
	FGuid PingId;
	UPROPERTY()
	FGolemEntityId GolemId;
	UPROPERTY()
	FName RegionId;
	UPROPERTY()
	FName TargetAnchorId;
	UPROPERTY()
	FVector WorldPosition = FVector::ZeroVector;
	UPROPERTY()
	double ExpiresAt = 0.0;
};
