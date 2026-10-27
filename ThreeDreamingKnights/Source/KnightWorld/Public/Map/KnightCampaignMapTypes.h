#pragma once

#include "CoreMinimal.h"
#include "Types/KnightStableIds.h"
#include "KnightCampaignMapTypes.generated.h"

UENUM()
enum class EKnightMapMarkerKind : uint8
{
	Landmark,
	Quest,
	Checkpoint,
	Travel,
	Hazard
};

USTRUCT()
struct KNIGHTWORLD_API FKnightMapRegion
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
struct KNIGHTWORLD_API FKnightMapMarker
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
	EKnightMapMarkerKind Kind = EKnightMapMarkerKind::Landmark;
	UPROPERTY(EditDefaultsOnly)
	FVector WorldPosition = FVector::ZeroVector;
	UPROPERTY(EditDefaultsOnly)
	float DiscoveryRadius = 550.0f;
	UPROPERTY(EditDefaultsOnly)
	FName TravelRegionId;
};

USTRUCT()
struct KNIGHTWORLD_API FKnightMapPing
{
	GENERATED_BODY()

	UPROPERTY()
	FGuid PingId;
	UPROPERTY()
	FKnightEntityId KnightId;
	UPROPERTY()
	FName RegionId;
	UPROPERTY()
	FName TargetAnchorId;
	UPROPERTY()
	FVector WorldPosition = FVector::ZeroVector;
	UPROPERTY()
	double ExpiresAt = 0.0;
};
