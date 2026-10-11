#pragma once
#include "CoreMinimal.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "Dom/JsonObject.h"
class UStaticMesh;
// Operates on exact solid geometry, never deletes a piece based on bounds overlap alone.
bool ThanhATrimRoadSurfaces(const TSharedPtr<FJsonObject>& Scene,const TMap<FString,UStaticMesh*>& Sources,
    TMap<FString,UE::Geometry::FDynamicMesh3>& Results,TMap<FString,UE::Geometry::FDynamicMesh3>& CollisionSources,TSharedRef<FJsonObject> Receipt);
