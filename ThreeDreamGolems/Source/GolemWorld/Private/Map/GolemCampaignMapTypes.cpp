#include "Map/GolemCampaignMapTypes.h"

bool FGolemMapRegion::IsValid() const
{
	return GolemIdentity::IsValidIdentifier(RegionId) && !Label.IsEmpty() && !Map.IsNull()
		&& !WorldCenter.ContainsNaN() && !HalfExtent.ContainsNaN() && FMath::IsFinite(NorthYaw)
		&& HalfExtent.X > 1.0 && HalfExtent.Y > 1.0;
}

bool FGolemMapRegion::Project(const FVector& WorldPosition, FVector2D& OutUV) const
{
	OutUV = FVector2D::ZeroVector;
	if (!IsValid() || WorldPosition.ContainsNaN())
	{
		return false;
	}
	const FVector2D Local = (FVector2D(WorldPosition.X, WorldPosition.Y) - WorldCenter).GetRotated(-NorthYaw);
	OutUV = FVector2D(0.5 + Local.X / (2.0 * HalfExtent.X), 0.5 - Local.Y / (2.0 * HalfExtent.Y));
	return OutUV.X >= 0.0 && OutUV.X <= 1.0 && OutUV.Y >= 0.0 && OutUV.Y <= 1.0;
}

bool FGolemMapRegion::Unproject(const FVector2D& UV, FVector2D& OutWorldPosition) const
{
	OutWorldPosition = FVector2D::ZeroVector;
	if (!IsValid() || UV.ContainsNaN() || UV.X < 0.0 || UV.X > 1.0 || UV.Y < 0.0 || UV.Y > 1.0)
	{
		return false;
	}
	OutWorldPosition = WorldCenter + FVector2D((UV.X - 0.5) * 2.0 * HalfExtent.X,
		(0.5 - UV.Y) * 2.0 * HalfExtent.Y).GetRotated(NorthYaw);
	return true;
}
