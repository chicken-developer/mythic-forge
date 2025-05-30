/*
* Copyright (c) <2021> Side Effects Software Inc.
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
* 1. Redistributions of source code must retain the above copyright notice,
*    this list of conditions and the following disclaimer.
*
* 2. The name of Side Effects Software may not be used to endorse or
*    promote products derived from this software without specific prior
*    written permission.
*
* THIS SOFTWARE IS PROVIDED BY SIDE EFFECTS SOFTWARE "AS IS" AND ANY EXPRESS
* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
* OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
* NO EVENT SHALL SIDE EFFECTS SOFTWARE BE LIABLE FOR ANY DIRECT, INDIRECT,
* INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
* OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
* LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
* NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
* EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "HoudiniApi.h"
#include "HoudiniEngineRuntimePrivatePCH.h"
#include "HoudiniEnginePrivatePCH.h"
#include "HoudiniEngine.h"
#include "HoudiniRuntimeSettings.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniEngineString.h"
#include "HoudiniInput.h"

#include "UnrealLandscapeTranslator.h"

#include "HoudiniDataLayerUtils.h"
#include "HoudiniEngineAttributes.h"
#include "HoudiniGeoPartObject.h"

#include "Landscape.h"
#include "LandscapeStreamingProxy.h"
#include "LandscapeDataAccess.h"
#include "LandscapeEdit.h"
#include "LightMap.h"
#include "Engine/MapBuildDataRegistry.h"
#include "PhysicalMaterials/PhysicalMaterial.h"

#include "UnrealObjectInputRuntimeTypes.h"
#include "UnrealObjectInputRuntimeUtils.h"
#include "UnrealObjectInputUtils.h"
#include "HoudiniEngineRuntimeUtils.h"
#include "HoudiniHLODLayerUtils.h"
#include "HoudiniLandscapeUtils.h"


bool 
FUnrealLandscapeTranslator::CreateMeshOrPointsFromLandscape(
	ALandscapeProxy* LandscapeProxy, 
	HAPI_NodeId& CreatedNodeId, 
	const FString& InputNodeNameString,
	const bool bExportGeometryAsMesh, 
	const bool bExportTileUVs,
	const bool bExportNormalizedUVs,
	const bool bExportLighting,
	const bool bExportMaterials,
	const bool bApplyWorldTransform,
	const HAPI_NodeId& ParentNodeId)
{
	//--------------------------------------------------------------------------------------------------
	// 1. Create an input node
	//--------------------------------------------------------------------------------------------------
	HAPI_NodeId InputNodeId = -1;
	// Create the curve SOP Node
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::CreateNode(ParentNodeId, TEXT("null"), InputNodeNameString, true, &InputNodeId), false);

	// Check if we have a valid id for this new input asset.
	if (!FHoudiniEngineUtils::IsHoudiniNodeValid(InputNodeId))
		return false;

	// We now have a valid id.
	CreatedNodeId = InputNodeId;

	if (!FHoudiniEngineUtils::HapiCookNode(InputNodeId, nullptr, true))
		return false;
	
	//--------------------------------------------------------------------------------------------------
    // 2. Set the part info
    //--------------------------------------------------------------------------------------------------
	int32 ComponentSizeQuads = ((LandscapeProxy->ComponentSizeQuads + 1) >> LandscapeProxy->ExportLOD) - 1;
	float ScaleFactor = (float)LandscapeProxy->ComponentSizeQuads / (float)ComponentSizeQuads;

	//int32 NumComponents = bExportOnlySelected ? SelectedComponents.Num() : LandscapeProxy->LandscapeComponents.Num();
	int32 NumComponents = LandscapeProxy->LandscapeComponents.Num();
	int32 VertexCountPerComponent = FMath::Square(ComponentSizeQuads + 1);
	int32 VertexCount = NumComponents * VertexCountPerComponent;

	if (!VertexCount)
		return false;
	

	int32 TriangleCount = NumComponents * FMath::Square(ComponentSizeQuads) * 2;
	int32 QuadCount = NumComponents * FMath::Square(ComponentSizeQuads);
	int32 IndexCount = QuadCount * 4;

	// Create part info
	HAPI_PartInfo Part;
	FHoudiniApi::PartInfo_Init(&Part);
	//FMemory::Memzero< HAPI_PartInfo >(Part);
	Part.id = 0;
	Part.nameSH = 0;
	Part.attributeCounts[HAPI_ATTROWNER_POINT] = 0;
	Part.attributeCounts[HAPI_ATTROWNER_PRIM] = 0;
	Part.attributeCounts[HAPI_ATTROWNER_VERTEX] = 0;
	Part.attributeCounts[HAPI_ATTROWNER_DETAIL] = 0;
	Part.vertexCount = 0;
	Part.faceCount = 0;
	Part.pointCount = VertexCount;
	Part.type = HAPI_PARTTYPE_MESH;

	// If we are exporting to a mesh, we need vertices and faces
	if (bExportGeometryAsMesh)
	{
		Part.vertexCount = IndexCount;
		Part.faceCount = QuadCount;
	}

	// Set the part infos
	HAPI_GeoInfo DisplayGeoInfo;
	FHoudiniApi::GeoInfo_Init(&DisplayGeoInfo);
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetDisplayGeoInfo(
		FHoudiniEngine::Get().GetSession(), CreatedNodeId, &DisplayGeoInfo), false);

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetPartInfo(
		FHoudiniEngine::Get().GetSession(), DisplayGeoInfo.nodeId, 0, &Part), false);

	//--------------------------------------------------------------------------------------------------
	// 3. Extract the landscape data
	//--------------------------------------------------------------------------------------------------
	// Array for the position data
	TArray<FVector3f> LandscapePositionArray;
	// Array for the normals
	TArray<FVector3f> LandscapeNormalArray;
	// Array for the UVs
	TArray<FVector3f> LandscapeUVArray;
	// Array for the vertex index of each point in its component
	TArray<FIntPoint> LandscapeComponentVertexIndicesArray;
	// Array for the tile names per point
	TArray<const char *> LandscapeComponentNameArray;
	// Array for the lightmap values
	TArray<FLinearColor> LandscapeLightmapValues;
	// Selected components set to all components in current landscape proxy
	TSet<ULandscapeComponent*> SelectedComponents;
	SelectedComponents.Append(ToRawPtrTArrayUnsafe(LandscapeProxy->LandscapeComponents));

	// Extract all the data from the landscape to the arrays
	if (!ExtractLandscapeData(
		LandscapeProxy, SelectedComponents,
		bExportLighting, bExportTileUVs, bExportNormalizedUVs,
		bApplyWorldTransform,
		LandscapePositionArray, LandscapeNormalArray,
		LandscapeUVArray, LandscapeComponentVertexIndicesArray,
		LandscapeComponentNameArray, LandscapeLightmapValues))
		return false;

	//--------------------------------------------------------------------------------------------------
    // 3. Set the corresponding attributes in Houdini
    //--------------------------------------------------------------------------------------------------

    // Create point attribute info containing positions.
	if (!AddLandscapePositionAttribute(DisplayGeoInfo.nodeId, LandscapePositionArray))
		return false;

	// Create point attribute info containing normals.
	if (!AddLandscapeNormalAttribute(DisplayGeoInfo.nodeId, LandscapeNormalArray))
		return false;

	// Create point attribute info containing UVs.
	if (!AddLandscapeUVAttribute(DisplayGeoInfo.nodeId, LandscapeUVArray))
		return false;

	// Create point attribute containing landscape component vertex indices (indices of vertices within the grid - x,y).
	if (!AddLandscapeComponentVertexIndicesAttribute(DisplayGeoInfo.nodeId, LandscapeComponentVertexIndicesArray))
		return false;

	// Create point attribute containing landscape component name.
	if (!AddLandscapeComponentNameAttribute(DisplayGeoInfo.nodeId, LandscapeComponentNameArray))
		return false;

	// Create point attribute info containing lightmap information.
	if (bExportLighting)
	{
		if (!AddLandscapeLightmapColorAttribute(DisplayGeoInfo.nodeId, LandscapeLightmapValues))
			return false;
	}

	// Set indices if we are exporting full geometry.
	if (bExportGeometryAsMesh)
	{
		if (!AddLandscapeMeshIndicesAndMaterialsAttribute(
			DisplayGeoInfo.nodeId,
			bExportMaterials,
			ComponentSizeQuads,
			QuadCount,
			LandscapeProxy,
			SelectedComponents))
			return false;
	}

	// If we are marshalling material information.
	if (bExportMaterials)
	{
		if (!AddLandscapeGlobalMaterialAttribute(DisplayGeoInfo.nodeId, LandscapeProxy))
			return false;
	}

	/*
	// TODO: Move this to ExtractLandscapeData()
	//--------------------------------------------------------------------------------------------------
	// 4. Extract and convert all the layers
	//--------------------------------------------------------------------------------------------------
	ULandscapeInfo* LandscapeInfo = LandscapeProxy->GetLandscapeInfo();
	if (!LandscapeInfo)
		return false;

	// Get the landscape X/Y Size
	int32 MinX = MAX_int32;
	int32 MinY = MAX_int32;
	int32 MaxX = -MAX_int32;
	int32 MaxY = -MAX_int32;
	if (!LandscapeInfo->GetLandscapeExtent(MinX, MinY, MaxX, MaxY))
		return false;

	// Calc the X/Y size in points
	int32 XSize = (MaxX - MinX + 1);
	int32 YSize = (MaxY - MinY + 1);
	if ((XSize < 2) || (YSize < 2))
		return false;

	bool MaskInitialized = false;
	int32 NumLayers = LandscapeInfo->Layers.Num();
	for (int32 n = 0; n < NumLayers; n++)
	{
		// 1. Extract the uint8 values from the layer
		TArray<uint8> CurrentLayerIntData;
		FLinearColor LayerUsageDebugColor;
		FString LayerName;
		if (!GetLandscapeTargetLayerData(
			LandscapeInfo, n,
			MinX, MinY, MaxX, MaxY,
			CurrentLayerIntData, LayerUsageDebugColor, LayerName))
			continue;

		// 2. Convert unreal uint8 values to floats
		// If the layer came from Houdini, additional info might have been stored in the DebugColor to convert the data back to float
		HAPI_VolumeInfo CurrentLayerVolumeInfo;
		FHoudiniApi::VolumeInfo_Init(&CurrentLayerVolumeInfo);
		TArray<float> CurrentLayerFloatData;
		if (!ConvertLandscapeLayerDataToHeightfieldData(
			CurrentLayerIntData, XSize, YSize, LayerUsageDebugColor,
			CurrentLayerFloatData, CurrentLayerVolumeInfo))
			continue;

		if (!AddLandscapeLayerAttribute(
			DisplayGeoInfo.nodeId, CurrentLayerFloatData, LayerName))
			continue;
	}
	*/

	// Commit the geo.
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiCommitGeo(DisplayGeoInfo.nodeId), false);

	return FHoudiniEngineUtils::HapiCookNode(InputNodeId, nullptr, true);
}

bool 
FUnrealLandscapeTranslator::CreateHeightfieldFromLandscape(
	ALandscapeProxy* LandscapeProxy,
	const FHoudiniLandscapeExportOptions& Options,
	HAPI_NodeId& CreatedHeightfieldNodeId, 
	const FString& InputNodeNameStr,
	HAPI_NodeId ParentNodeId,
	const bool bSetObjectTransformToWorldTransform) 
{
  	if (!LandscapeProxy)
		return false;

	// Export the whole landscape and its layer as a single heightfield.
	FString NodeName = InputNodeNameStr;

	//--------------------------------------------------------------------------------------------------
	// Extracting the height data
	//--------------------------------------------------------------------------------------------------
	TArray<uint16> HeightData;
	int32 XSize, YSize;
	FVector Min, Max;
	if (!GetLandscapeData(LandscapeProxy, HeightData, XSize, YSize, Min, Max))
		return false;

	//--------------------------------------------------------------------------------------------------
	// Convert the height uint16 data to float
	//--------------------------------------------------------------------------------------------------
	TArray<float> HeightfieldFloatValues;
	HAPI_VolumeInfo HeightfieldVolumeInfo;
	FHoudiniApi::VolumeInfo_Init(&HeightfieldVolumeInfo);

	// NOTE: The landscape actor's transform cannot be directly used to position the heightfield in Houdini since only a
	// part of the full landscape may be loaded. We need to calculate the center of all the loaded tiles / landscape
	// components.
	FTransform LandscapeTransform = FHoudiniEngineRuntimeUtils::CalculateHoudiniLandscapeTransform(LandscapeProxy);

	FTransform LandscapeActorTransform = LandscapeProxy->GetLandscapeActor()->GetActorTransform();
	LandscapeTransform.SetScale3D(FVector::One());

	FVector CenterOffset = FVector::ZeroVector;
	if (!ConvertLandscapeDataToHeightFieldData(
		HeightData, 
		XSize, YSize, 
		Min, Max, 
		LandscapeActorTransform.GetScale3D(),
		HeightfieldFloatValues, 
		HeightfieldVolumeInfo))
	{
		return false;
	}
	//--------------------------------------------------------------------------------------------------
	// Create the Heightfield Input Node
	//-------------------------------------------------------------------------------------------------- 
	HAPI_NodeId HeightFieldId = -1;
	HAPI_NodeId HeightId = -1;
	HAPI_NodeId MaskId = -1;
	HAPI_NodeId MergeId = -1;
	if (!CreateHeightfieldInputNode(NodeName, XSize, YSize, HeightFieldId, HeightId, MaskId, MergeId, ParentNodeId))
		return false;


	//--------------------------------------------------------------------------------------------------
	// Set the HeightfieldData in Houdini
	//--------------------------------------------------------------------------------------------------    
	// Set the Height volume's data
	HAPI_PartId PartId = 0;
	if (!SetHeightfieldData(HeightId, PartId, HeightfieldFloatValues, HeightfieldVolumeInfo, TEXT("height")))
		return false;

	// Apply attributes to the heightfield
	ApplyAttributesToHeightfieldNode(HeightId, PartId, LandscapeProxy);

	// Commit the height volume
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiCommitGeo(HeightId), false);

	//--------------------------------------------------------------------------------------------------
	// Add Data Layers and HLODS
	//--------------------------------------------------------------------------------------------------

	if (ParentNodeId != -1)
	{
		int PrevNode = FHoudiniHLODLayerUtils::AddHLODAttributes(LandscapeProxy, ParentNodeId, HeightFieldId);
		FHoudiniDataLayerUtils::AddGroupsFromDataLayers(LandscapeProxy, ParentNodeId, PrevNode);
	}

	//--------------------------------------------------------------------------------------------------
	// Define merge lambda, used below.
	//--------------------------------------------------------------------------------------------------

	int32 MergeInputIndex = 2;

	auto MergeInputFn = [&MergeInputIndex](const HAPI_NodeId MergeId, const HAPI_NodeId NodeId) -> HAPI_Result
	{
		// We had to create a new volume for this layer, so we need to connect it to the HF's merge node
		HAPI_Result Result = FHoudiniApi::ConnectNodeInput(
			FHoudiniEngine::Get().GetSession(),
			MergeId, MergeInputIndex, NodeId, 0);

		if (Result == HAPI_RESULT_SUCCESS)
		{
			MergeInputIndex++;
		}
		return Result;
	};

	//--------------------------------------------------------------------------------------------------
	// Send target layer data to Houdini.
	//--------------------------------------------------------------------------------------------------

	if (!SendTargetLayersToHoudini(LandscapeProxy, HeightFieldId, PartId, MergeId, MaskId, Options, HeightfieldVolumeInfo, XSize, YSize, MergeInputIndex))
		return false;

	//--------------------------------------------------------------------------------------------------
	// Create height field input for each editable landscape layer
	//--------------------------------------------------------------------------------------------------

	// We need a valid landscape actor to get the edit layers
	ALandscape* Landscape = LandscapeProxy->GetLandscapeActor();
	if(IsValid(Landscape) && Options.bExportHeightDataPerEditLayer)
	{
		HAPI_VolumeInfo LayerVolumeInfo;
		FHoudiniApi::VolumeInfo_Init(&HeightfieldVolumeInfo);
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5	
		for (const FLandscapeLayer& Layer : Landscape->GetLayers())
#else
		for(FLandscapeLayer& Layer : Landscape->LandscapeLayers)
#endif
		{
			const FString LayerVolumeName = FString::Format(TEXT("landscapelayer_{0}"), {Layer.Name.ToString()});
			
			HAPI_NodeId LandscapeLayerNodeId = -1;
			
			HOUDINI_LANDSCAPE_MESSAGE(TEXT("[FUnrealLandscapeTranslator::CreateHeightfieldFromLandscape] Creating input node for editable landscape layer: %s"), *LayerVolumeName);

			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::CreateHeightfieldInputVolumeNode(
				FHoudiniEngine::Get().GetSession(),
				HeightFieldId,
				&LandscapeLayerNodeId,
				TCHAR_TO_UTF8(*LayerVolumeName),
				XSize, YSize,
				1.f
				), false);

			// Create a volume visualization node
			const FString VisualizationName = FString::Format(TEXT("visualization_{0}"), {Layer.Name.ToString()});
			HAPI_NodeId VisualizationNodeId = -1;
			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::CreateNode(
				FHoudiniEngine::Get().GetSession(),
				HeightFieldId,
				"volumevisualization",
				TCHAR_TO_UTF8(*VisualizationName),
				false,
				&VisualizationNodeId
				), false);

			// Set Visualization Mode to Height Field
			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetParmIntValue(
				FHoudiniEngine::Get().GetSession(),
				VisualizationNodeId,
				"vismode",
				0, 2
				), false);
			
			// Set Density Field to '*'.
			HAPI_ParmId DensityFieldParmId = -1;
			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetParmIdFromName(
				FHoudiniEngine::Get().GetSession(),
				VisualizationNodeId,
				"densityfield",
				&DensityFieldParmId
				), false);
			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetParmStringValue(
				FHoudiniEngine::Get().GetSession(),
				VisualizationNodeId,
				"*",
				DensityFieldParmId, 0
				), false);

			// Create a visibility node
			const FString VisibilityName = FString::Format(TEXT("visibility_{0}"), {Layer.Name.ToString()});
			HAPI_NodeId VisibilityNodeId = -1;
			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::CreateNode(
				FHoudiniEngine::Get().GetSession(),
				HeightFieldId,
				"visibility",
				TCHAR_TO_UTF8(*VisibilityName),
				false,
				&VisibilityNodeId
				), false);

			// Connect landscape layer to visualization
			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::ConnectNodeInput(
				FHoudiniEngine::Get().GetSession(),
				VisualizationNodeId, 0, LandscapeLayerNodeId, 0), false);

			// Connect visualization to visibility
			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::ConnectNodeInput(
				FHoudiniEngine::Get().GetSession(),
				VisibilityNodeId, 0, VisualizationNodeId, 0), false);

			// Connect the visibility node to the merge input
			HOUDINI_CHECK_ERROR_RETURN(MergeInputFn(MergeId, VisibilityNodeId), false);

			FScopedSetLandscapeEditingLayer Scope(Landscape, Layer.Guid ); // Scope landscape access to the current layer

			//--------------------------------------------------------------------------------------------------
			// Extracting height data
			//--------------------------------------------------------------------------------------------------

			TArray<uint16> LayerHeightData;
			if (!GetLandscapeData(LandscapeProxy, LayerHeightData, XSize, YSize, Min, Max))
				return false;

			//--------------------------------------------------------------------------------------------------
			// Convert the height uint16 data to float
			//--------------------------------------------------------------------------------------------------
			TArray<float> LayerHeightFloatData;
			if (!ConvertLandscapeDataToHeightFieldData(
				LayerHeightData, XSize, YSize, Min, Max, LandscapeActorTransform.GetScale3D(),
				LayerHeightFloatData, LayerVolumeInfo))
				return false;

			HAPI_PartId LayerPartId = 0;
			SetHeightfieldData(LandscapeLayerNodeId, LayerPartId, LayerHeightFloatData, LayerVolumeInfo, LayerVolumeName);

			// Apply attributes to the heightfield input node
			ApplyAttributesToHeightfieldNode(LandscapeLayerNodeId, 0, LandscapeProxy);

			// Commit the volume's geo
			HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiCommitGeo(LandscapeLayerNodeId), false);
		}
	}

	HAPI_TransformEuler HAPIObjectTransform;
	FHoudiniApi::TransformEuler_Init(&HAPIObjectTransform);
	LandscapeTransform.SetScale3D(FVector::OneVector);
	
	if (bSetObjectTransformToWorldTransform)
		FHoudiniEngineUtils::TranslateUnrealTransform(LandscapeTransform, HAPIObjectTransform);
	else
		FHoudiniEngineUtils::TranslateUnrealTransform(FTransform::Identity, HAPIObjectTransform);
	// HAPIObjectTransform.position[1] = 0.0f;

	HAPI_NodeId ParentObjNodeId = FHoudiniEngineUtils::HapiGetParentNodeId(HeightFieldId);
	FHoudiniApi::SetObjectTransform(FHoudiniEngine::Get().GetSession(), ParentObjNodeId, &HAPIObjectTransform);

	// Finally, cook the Height field node
	if(!FHoudiniEngineUtils::HapiCookNode(HeightFieldId, nullptr, true))
		return false;

	CreatedHeightfieldNodeId = HeightFieldId;

	return true;
}

bool 
FUnrealLandscapeTranslator::CreateHeightfieldFromLandscapeComponentArray(
	ALandscapeProxy* LandscapeProxy,
	const TSet<ULandscapeComponent*>& SelectedComponents,
	const FHoudiniLandscapeExportOptions& Options,
	HAPI_NodeId& CreatedHeightfieldNodeId,
	const FString& InputNodeNameStr,
	const HAPI_NodeId& ParentNodeId,
	const bool bSetObjectTransformToWorldTransform)
{
	if ( SelectedComponents.Num() <= 0 )
		return false;
		
	ULandscapeInfo* LandscapeInfo = LandscapeProxy->GetLandscapeInfo();
	if (!IsValid(LandscapeInfo))
		return false;

	//--------------------------------------------------------------------------------------------------
	//  Each selected component will be exported as tiled volumes in a single heightfield
	//--------------------------------------------------------------------------------------------------
	//FTransform LandscapeTransform = FTransform::Identity; // The offset will be done in the component side
	const FTransform LandscapeTM = LandscapeProxy->LandscapeActorToWorld();
	const FTransform ProxyRelativeTM(FVector(LandscapeProxy->LandscapeSectionOffset));
	FTransform LandscapeTransform = ProxyRelativeTM * LandscapeTM;
	
	HAPI_NodeId HeightfieldNodeId = -1;
	HAPI_NodeId HeightfieldeMergeId = -1;

	int32 MergeInputIndex = 0;
	bool bAllComponentCreated = true;

	int32 ComponentIdx = 0;

	LandscapeInfo->ForAllLandscapeComponents([&](ULandscapeComponent* CurrentComponent)
	{
		if ( !CurrentComponent )
			return;
	
		if ( !SelectedComponents.Contains( CurrentComponent ) )
			return;
		
		if ( !CreateHeightfieldFromLandscapeComponent(LandscapeProxy, CurrentComponent, ComponentIdx, HeightfieldNodeId, HeightfieldeMergeId, MergeInputIndex, Options, InputNodeNameStr, LandscapeTransform, ParentNodeId) )
			bAllComponentCreated = false;
		
		ComponentIdx++;
	});
	
	// Check that we have a valid id for the input Heightfield.
	if ( FHoudiniEngineUtils::IsHoudiniNodeValid( HeightfieldNodeId ) )
	    CreatedHeightfieldNodeId = HeightfieldNodeId;

	// Set the HF's parent OBJ's tranform to the Landscape's transform
	HAPI_TransformEuler HAPIObjectTransform;
	FHoudiniApi::TransformEuler_Init(&HAPIObjectTransform);
	//FMemory::Memzero< HAPI_TransformEuler >( HAPIObjectTransform );
	LandscapeTransform.SetScale3D( FVector::OneVector );

	// In the new input system we'll apply the calculated transform later in the process on a reference node
	if (bSetObjectTransformToWorldTransform)
		FHoudiniEngineUtils::TranslateUnrealTransform( LandscapeTransform, HAPIObjectTransform );
	else
		FHoudiniEngineUtils::TranslateUnrealTransform(FTransform::Identity, HAPIObjectTransform);
	HAPIObjectTransform.position[ 1 ] = 0.0f;

	HAPI_NodeId ParentObjNodeId = FHoudiniEngineUtils::HapiGetParentNodeId( HeightfieldNodeId );
	FHoudiniApi::SetObjectTransform( FHoudiniEngine::Get().GetSession(), ParentObjNodeId, &HAPIObjectTransform );

	return bAllComponentCreated;
}

bool
FUnrealLandscapeTranslator::CreateHeightfieldFromLandscapeComponent(
	ALandscapeProxy* LandscapeProxy, 
	ULandscapeComponent* LandscapeComponent,
	const int32& ComponentIndex,
	HAPI_NodeId& HeightFieldId, 
	HAPI_NodeId& MergeId, 
	int32& MergeInputIndex,
	const FHoudiniLandscapeExportOptions& Options,
	const FString& InputNodeNameStr, 
	const FTransform & ParentTransform,
	const HAPI_NodeId& ParentNodeId)
{
	if ( !LandscapeComponent )
		return false;

	FString NodeName = InputNodeNameStr;
	
	//--------------------------------------------------------------------------------------------------
	// 1. Extract the height data
	//--------------------------------------------------------------------------------------------------
	int32 MinX = MAX_int32;
	int32 MinY = MAX_int32;
	int32 MaxX = -MAX_int32;
	int32 MaxY = -MAX_int32;
	LandscapeComponent->GetComponentExtent( MinX, MinY, MaxX, MaxY );
	
	ULandscapeInfo* LandscapeInfo = LandscapeComponent->GetLandscapeInfo();
	if ( !LandscapeInfo )
		return false;

	TArray<uint16> HeightData;
	int32 XSize, YSize;
	if ( !GetLandscapeData( LandscapeInfo, MinX, MinY, MaxX, MaxY, HeightData, XSize, YSize ) )
		return false;

	FVector Origin = LandscapeComponent->Bounds.Origin;
	FVector Extents = LandscapeComponent->Bounds.BoxExtent;
	FVector Min = Origin - Extents;
	FVector Max = Origin + Extents;

	//--------------------------------------------------------------------------------------------------
	// 2. Convert the landscape's combined height uint16 data to float
	//--------------------------------------------------------------------------------------------------
	TArray<float> HeightfieldFloatValues;
	HAPI_VolumeInfo HeightfieldVolumeInfo;
	FHoudiniApi::VolumeInfo_Init(&HeightfieldVolumeInfo);
	FTransform LandscapeComponentTransform = LandscapeComponent->GetComponentTransform();

	FVector CenterOffset = FVector::ZeroVector;
	if ( !ConvertLandscapeDataToHeightFieldData(
		HeightData, XSize, YSize, Min, Max, LandscapeComponentTransform.GetScale3D(),
		HeightfieldFloatValues, HeightfieldVolumeInfo) )
		return false;

	// We need to modify the Volume's position to the Component's position relative to the Landscape's position
	FVector RelativePosition = LandscapeComponent->GetRelativeTransform().GetLocation();
	HeightfieldVolumeInfo.transform.position[1] = RelativePosition.X;
	HeightfieldVolumeInfo.transform.position[0] = RelativePosition.Y;
	HeightfieldVolumeInfo.transform.position[2] = 0.0f;

	ALandscapeProxy * Proxy = LandscapeComponent->GetLandscapeProxy();
	if (Proxy)
	{
		FTransform LandscapeTM = Proxy->LandscapeActorToWorld();
		FTransform ProxyRelativeTM(FVector(Proxy->LandscapeSectionOffset));

		// For landscapes that live in streaming proxies, need to account for both the parent transform and
		// the current transform.
		// For single actor landscapes, the parent proxy transform is equal to this proxy transform
		// Either way, we want to multiply by the inverse of the parent transform to get the relative
		FTransform LandscapeTransform = ParentTransform.Inverse() * ProxyRelativeTM * LandscapeTM;
		FVector Location = LandscapeTransform.GetLocation();
		
		HeightfieldVolumeInfo.transform.position[1] += Location.X / HAPI_UNREAL_SCALE_FACTOR_POSITION; 
		HeightfieldVolumeInfo.transform.position[0] += Location.Y / HAPI_UNREAL_SCALE_FACTOR_POSITION;
	}

	//--------------------------------------------------------------------------------------------------
	// 3. Create the Heightfield Input Node
	//--------------------------------------------------------------------------------------------------
	HAPI_NodeId HeightId = -1;
	HAPI_NodeId MaskId = -1;
	bool CreatedHeightfieldNode = false;
	if ( HeightFieldId < 0 || MergeId < 0 )
	{
		if (!CreateHeightfieldInputNode(NodeName, XSize, YSize, HeightFieldId, HeightId, MaskId, MergeId, ParentNodeId))
			return false;

		MergeInputIndex = 2;
		CreatedHeightfieldNode = true;
	}
	else
	{
	    // Heightfield node was previously created, create additional height and a mask volumes for it
	    FHoudiniApi::CreateHeightfieldInputVolumeNode(
	        FHoudiniEngine::Get().GetSession(),
	        HeightFieldId, &HeightId, "height", XSize, YSize, 1.0f );

	    FHoudiniApi::CreateHeightfieldInputVolumeNode(
	        FHoudiniEngine::Get().GetSession(),
	        HeightFieldId, &MaskId, "mask", XSize, YSize, 1.0f );

	    // Connect the two newly created volumes to the HF's merge node
	    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::ConnectNodeInput(
	        FHoudiniEngine::Get().GetSession(),
	        MergeId, MergeInputIndex++, HeightId, 0 ), false );

	    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::ConnectNodeInput(
	        FHoudiniEngine::Get().GetSession(),
	        MergeId, MergeInputIndex++, MaskId, 0 ), false );
	}

	//--------------------------------------------------------------------------------------------------
	// 4. Set the Height field data in Houdini
	//--------------------------------------------------------------------------------------------------    
	// Set the Height volume's data
	HAPI_PartId PartId = 0;
	if (!SetHeightfieldData(HeightId, PartId, HeightfieldFloatValues, HeightfieldVolumeInfo, TEXT("height")))
		return false;

	// Apply tile attribute
	AddLandscapeTileAttribute(HeightId, PartId, ComponentIndex);
	
	// Apply attributes to the heightfield
	ApplyAttributesToHeightfieldNode(HeightId, PartId, LandscapeProxy);
	
	// Commit the height volume
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiCommitGeo(HeightId), false);

	//--------------------------------------------------------------------------------------------------
	// 5. Extract and convert all the layers to HF masks
	//--------------------------------------------------------------------------------------------------
	if (!SendTargetLayersToHoudini(LandscapeProxy, HeightFieldId, PartId, MergeId, MaskId, Options, HeightfieldVolumeInfo, XSize, YSize, MergeInputIndex))
		return false;

	// Finally, cook the Heightfield node
	if (!FHoudiniEngineUtils::HapiCookNode(HeightFieldId, nullptr, true))
		return false;

	return true;
}


bool 
FUnrealLandscapeTranslator::CreateInputNodeForLandscape(
	ALandscapeProxy* LandscapeProxy,
	const FString& InputNodeNameStr,
	const FString& HeightFieldName,
	const FTransform& LandscapeTransform,
	HAPI_NodeId& HeightId,
	HAPI_PartId& PartId,
	HAPI_NodeId& HeightFieldId,
	HAPI_NodeId& MaskId,
	HAPI_NodeId& MergeId,
	TArray<uint16>& HeightData,
	HAPI_VolumeInfo& HeightfieldVolumeInfo,
	int32& XSize, int32& YSize,
	const HAPI_NodeId& ParentNodeId)
{
	//--------------------------------------------------------------------------------------------------
	// 1. Extracting the height data
	//--------------------------------------------------------------------------------------------------
	
	FVector Min, Max;
	
	if (!GetLandscapeData(LandscapeProxy, HeightData, XSize, YSize, Min, Max))
		return false;

	//--------------------------------------------------------------------------------------------------
	// 2. Convert the height uint16 data to float
	//--------------------------------------------------------------------------------------------------
	TArray<float> HeightfieldFloatValues;
	
	if (!ConvertLandscapeDataToHeightFieldData(
		HeightData, XSize, YSize, Min, Max, LandscapeTransform.GetScale3D(),
		HeightfieldFloatValues, HeightfieldVolumeInfo))
		return false;

	//--------------------------------------------------------------------------------------------------
	// 3. Create the Heightfield Input Node
	//-------------------------------------------------------------------------------------------------- 
	if (!CreateHeightfieldInputNode(InputNodeNameStr, XSize, YSize, HeightFieldId, HeightId, MaskId, MergeId, ParentNodeId))
		return false;

	//--------------------------------------------------------------------------------------------------
	// 4. Set the HeightfieldData in Houdini
	//--------------------------------------------------------------------------------------------------
	// Set the Height volume's data
	if (!SetHeightfieldData(HeightId, PartId, HeightfieldFloatValues, HeightfieldVolumeInfo, HeightFieldName))
		return false;

	return true;
}


bool
FUnrealLandscapeTranslator::CreateInputNodeForLandscapeObject(
	ALandscapeProxy* InLandscape,
	UHoudiniInput* InInput,
	HAPI_NodeId& InputNodeId,
	const FString& InputNodeName,
	FUnrealObjectInputHandle& OutHandle,
	const bool& bInputNodesCanBeDeleted)
{
	FString FinalInputNodeName = InputNodeName;
	EHoudiniLandscapeExportType ExportType = InInput->GetLandscapeExportType();

	FUnrealObjectInputIdentifier Identifier;
	
	const FHoudiniInputObjectSettings& InputSettings = InInput->GetInputSettings();

	const bool bApplyWorldTransformToMeshOrPointCloudData = false;
	const bool bSetObjectTransformToWorldTransform = false;

	bool bExportSelectionOnly = InputSettings.bLandscapeExportSelectionOnly;
	bool bLandscapeAutoSelectComponent = InputSettings.bLandscapeAutoSelectComponent;

	// Get selected components if bLandscapeExportSelectionOnly or bLandscapeAutoSelectComponent is true
	TSet<TObjectPtr<ULandscapeComponent>> SelectedComponents;
	if (bExportSelectionOnly)
	{
		InInput->UpdateLandscapeInputSelection();
		SelectedComponents = InInput->GetLandscapeSelectedComponents();
	}

	FUnrealObjectInputHandle ParentHandle;
	HAPI_NodeId ParentNodeId = -1;

	{
		const FUnrealObjectInputOptions Options = FUnrealObjectInputOptions::MakeOptionsForLandscapeData(
			InputSettings, bExportSelectionOnly ? &SelectedComponents : nullptr);
		Identifier = FUnrealObjectInputIdentifier(InLandscape, Options, true);

		FUnrealObjectInputHandle Handle;
		if (FUnrealObjectInputUtils::NodeExistsAndIsNotDirty(Identifier, Handle))
		{
			HAPI_NodeId NodeId = -1;
			if (FUnrealObjectInputUtils::GetHAPINodeId(Handle, NodeId))
			{
				if (!bInputNodesCanBeDeleted)
					FUnrealObjectInputUtils::UpdateInputNodeCanBeDeleted(Handle, bInputNodesCanBeDeleted);

				OutHandle = Handle;
				InputNodeId = NodeId;
				return true;
			}
		}

		FUnrealObjectInputUtils::GetDefaultInputNodeName(Identifier, FinalInputNodeName);
		// Create any parent/container nodes that we would need, and get the node id of the immediate parent
		if (FUnrealObjectInputUtils::EnsureParentsExist(Identifier, ParentHandle, bInputNodesCanBeDeleted) && ParentHandle.IsValid())
			FUnrealObjectInputUtils::GetHAPINodeId(ParentHandle, ParentNodeId);

		// Set InputNodeId to the current NodeId associated with Handle, since that is what we are replacing.
		// (Option changes could mean that InputNodeId is associated with a completely different entry, albeit for
		// the same asset, in the manager)
		if (Handle.IsValid())
		{
			if (!FUnrealObjectInputUtils::GetHAPINodeId(Handle, InputNodeId))
				InputNodeId = -1;
		}
		else
		{
			InputNodeId = -1;
		}

		HAPI_NodeId GeoObjNodeId = -1;

		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::CreateNode(
			FHoudiniEngine::Get().GetSession(), ParentNodeId, "geo", TCHAR_TO_ANSI(*FinalInputNodeName), true, &GeoObjNodeId), false);

		ParentNodeId = GeoObjNodeId;

		// Delete the previous nodes, if valid
		if (InputNodeId >= 0 && FHoudiniEngineUtils::IsHoudiniNodeValid(InputNodeId))
		{
			// Get the parent OBJ node ID before deleting!
			HAPI_NodeId PreviousInputOBJNode = FHoudiniEngineUtils::HapiGetParentNodeId(InputNodeId);

			if (HAPI_RESULT_SUCCESS != FHoudiniApi::DeleteNode(
				FHoudiniEngine::Get().GetSession(), InputNodeId))
			{
				HOUDINI_LOG_WARNING(TEXT("Failed to cleanup the previous input node for %s."), *FinalInputNodeName);
			}

			InputNodeId = -1;

			if (PreviousInputOBJNode >= 0)
			{
				if (HAPI_RESULT_SUCCESS != FHoudiniApi::DeleteNode(
					FHoudiniEngine::Get().GetSession(), PreviousInputOBJNode))
				{
					HOUDINI_LOG_WARNING(TEXT("Failed to cleanup the previous input OBJ node for %s."), *FinalInputNodeName);
				}
			}
		}

		switch (ExportType)
		{
			case EHoudiniLandscapeExportType::Heightfield:
			{
				FinalInputNodeName = "heightfield";
				break;
			}

			case EHoudiniLandscapeExportType::Mesh:
			{
				FinalInputNodeName = "mesh";
				break;
			}

			case EHoudiniLandscapeExportType::Points:
			{
				FinalInputNodeName = "points";
				break;
			}
		}
	}

	bool bSuccess = false;
	if (ExportType == EHoudiniLandscapeExportType::Heightfield)
	{
		// Ensure we destroy any (Houdini) input nodes before clobbering this object with a new heightfield.
		//DestroyInputNodes(InInput, InInput->GetInputType());

		FHoudiniLandscapeExportOptions Options;
		Options.bExportHeightDataPerEditLayer = InInput->IsEditLayerHeightExportEnabled();
		Options.bExportMergedPaintLayers = InInput->IsMergedPaintLayerExportEnabled();
		Options.bExportPaintLayersPerEditLayer = InInput->IsPaintLayerPerEditLayerExportEnabled();

		int32 NumComponents = InLandscape->LandscapeComponents.Num();
		if(!bExportSelectionOnly || (SelectedComponents.Num() == NumComponents))
		{
			// Export the whole landscape and its layer as a single heightfield node
			bSuccess = FUnrealLandscapeTranslator::CreateHeightfieldFromLandscape(
				InLandscape,
				Options,
				InputNodeId,
				FinalInputNodeName,
				ParentNodeId,
				bSetObjectTransformToWorldTransform);
		}
		else
		{
			TSet<ULandscapeComponent*> SelectedLandscapeComponents = FHoudiniEngineUtils::RemoveObjectPtr(SelectedComponents);

			// Each selected landscape component will be exported as separate volumes in a single heightfield
			bSuccess = FUnrealLandscapeTranslator::CreateHeightfieldFromLandscapeComponentArray(
				InLandscape,
				SelectedLandscapeComponents,
				Options,
				InputNodeId,
				FinalInputNodeName,
				ParentNodeId,
				bSetObjectTransformToWorldTransform);
		}
	}
	else
	{
		bool bExportLighting = InputSettings.bLandscapeExportLighting;
		bool bExportMaterials = InputSettings.bLandscapeExportMaterials;
		bool bExportNormalizedUVs = InputSettings.bLandscapeExportNormalizedUVs;
		bool bExportTileUVs = InputSettings.bLandscapeExportTileUVs;
		bool bExportAsMesh = InputSettings.LandscapeExportType == EHoudiniLandscapeExportType::Mesh;

		bSuccess = FUnrealLandscapeTranslator::CreateMeshOrPointsFromLandscape(
			InLandscape, InputNodeId, FinalInputNodeName,
			bExportAsMesh, bExportTileUVs, bExportNormalizedUVs, bExportLighting, bExportMaterials,
			bApplyWorldTransformToMeshOrPointCloudData, ParentNodeId);
	}

	if (!bSuccess)
		return false;

	{
		FUnrealObjectInputHandle Handle;
		HAPI_NodeId InputObjectNodeId = FHoudiniEngineUtils::HapiGetParentNodeId(InputNodeId);
		if (FUnrealObjectInputUtils::AddNodeOrUpdateNode(Identifier, InputNodeId, Handle, InputObjectNodeId, nullptr, bInputNodesCanBeDeleted))
			OutHandle = Handle;
	}

	return true;
}


// Converts Unreal uint16 values to Houdini Float
bool
FUnrealLandscapeTranslator::ConvertLandscapeLayerDataToHeightfieldData(
	const TArray<uint8>& IntHeightData,
	int32 UnrealXSize, int32 UnrealYSize,
	const FLinearColor& LayerUsageDebugColor,
	TArray<float>& LayerFloatValues)
{
	int HoudiniXSize = UnrealYSize;
	int HoudiniYSize = UnrealXSize;

	LayerFloatValues.Empty();

	int32 SizeInPoints = HoudiniXSize * HoudiniYSize;
	if ((HoudiniXSize < 2) || (HoudiniYSize < 2))
		return false;

	if (IntHeightData.Num() != SizeInPoints)
		return false;

	//--------------------------------------------------------------------------------------------------
	// 1. Convert values to float
	//--------------------------------------------------------------------------------------------------

	// By default, values are converted from unreal [0 255] uint8 to Houdini [0 1] float	
	// uint8 min/max
	uint8 IntMin = 0;
	uint8 IntMax = UINT8_MAX;
	// The range in Digits	
	double DigitRange = (double)UINT8_MAX;

	// By default, the values will be converted to [0, 1]
	float LayerMin = 0.0f;
	float LayerMax = 1.0f;
	float LayerSpacing = 1.0f / DigitRange;
	
	// If this layer came from Houdini, its alpha value should be PI
	// This indicates that we can extract additional infos stored its debug usage color
	// so we can reconstruct the original source values (float) more accurately
	if (LayerUsageDebugColor.A == PI)
	{
		// We need the ZMin / ZMax uint8 values
		IntMin = IntHeightData[0];
		IntMax = IntMin;
		for (int n = 0; n < IntHeightData.Num(); n++)
		{
			if (IntHeightData[n] < IntMin)
				IntMin = IntHeightData[n];
			if (IntHeightData[n] > IntMax)
				IntMax = IntHeightData[n];
		}

		DigitRange = (double)IntMax - (double)IntMin;

		// Read the original min/max and spacing stored in the debug color
		LayerMin = LayerUsageDebugColor.R;
		LayerMax = LayerUsageDebugColor.G;
		LayerSpacing = LayerUsageDebugColor.B;
	}

	// Convert the Int data to Float
	LayerFloatValues.SetNumUninitialized(SizeInPoints);
	for (int32 nY = 0; nY < HoudiniYSize; nY++)
	{
		for (int32 nX = 0; nX < HoudiniXSize; nX++)
		{
			// We need to invert X/Y when reading the value from Unreal
			int32 nHoudini = nX + nY * HoudiniXSize;
			int32 nUnreal = nY + nX * HoudiniYSize;

			// Convert the int values to meter
			// Unreal's digit value have a zero value of 32768
			double DoubleValue = ((double)IntHeightData[nUnreal] - (double)IntMin) * LayerSpacing + LayerMin;
			LayerFloatValues[nHoudini] = (float)DoubleValue;
		}
	}

	/*
	// Verifying the converted ZMin / ZMax
	float FloatMin = LayerFloatValues[0];
	float FloatMax = FloatMin;
	for (int32 n = 0; n < LayerFloatValues.Num(); n++)
	{
		if (LayerFloatValues[n] < FloatMin)
			FloatMin = LayerFloatValues[n];
		if (LayerFloatValues[n] > FloatMax)
			FloatMax = LayerFloatValues[n];
	}
	*/

	return true;
}

bool
FUnrealLandscapeTranslator::GetLandscapeData(
	ALandscapeProxy* LandscapeProxy,
	TArray<uint16>& HeightData,
	int32& XSize, int32& YSize,
	FVector3d& Min, FVector3d& Max)
{
	if (!LandscapeProxy)
		return false;

	ULandscapeInfo* LandscapeInfo = LandscapeProxy->GetLandscapeInfo();
	if (!LandscapeInfo)
		return false;

	// Get the landscape extents to get its size
	int32 MinX = MAX_int32;
	int32 MinY = MAX_int32;
	int32 MaxX = -MAX_int32;
	int32 MaxY = -MAX_int32;
	
	ALandscape* Landscape = LandscapeProxy->GetLandscapeActor();
	if (LandscapeProxy == Landscape)
	{
		// The proxy is a landscape actor, so we have to use the landscape extent (landscape components
		// may have been moved to proxies and may not be present on this actor).
		LandscapeInfo->GetLandscapeExtent(MinX, MinY, MaxX, MaxY);
	}
	else
	{
		// We only want to get the data for this landscape proxy.
		// To handle streaming proxies correctly, get the extents via all the components,
		// not by calling GetLandscapeExtent or we'll end up sending ALL the streaming proxies.
		for (const ULandscapeComponent* Comp : LandscapeProxy->LandscapeComponents)
		{
			Comp->GetComponentExtent(MinX, MinY, MaxX, MaxY);
		}
	}

	if (!GetLandscapeData(LandscapeInfo, MinX, MinY, MaxX, MaxY, HeightData, XSize, YSize))
		return false;

	// Get the landscape Min/Max values
	// Do not use Landscape->GetActorBounds() here as instanced geo
	// (due to grass layers for example) can cause it to return incorrect bounds!
	FVector3d Origin, Extent;
	GetLandscapeProxyBounds(LandscapeProxy, Origin, Extent);

	// Get the landscape Min/Max values
	Min = Origin - Extent;
	Max = Origin + Extent;

	return true;
}

bool
FUnrealLandscapeTranslator::GetLandscapeData(
	ULandscapeInfo* LandscapeInfo,
	const int32& MinX, const int32& MinY,
	const int32& MaxX, const int32& MaxY,
	TArray<uint16>& HeightData,
	int32& XSize, int32& YSize)
{
	if (!LandscapeInfo)
		return false;

	// Get the X/Y size in points
	XSize = (MaxX - MinX + 1);
	YSize = (MaxY - MinY + 1);

	if ((XSize < 2) || (YSize < 2))
		return false;
	
	// Extracting the uint16 values from the landscape 
	FLandscapeEditDataInterface LandscapeEdit(LandscapeInfo);
	// Ensure we're not triggering a checkout, as we're just reading data
	LandscapeEdit.SetShouldDirtyPackage(false);

	HeightData.AddZeroed(XSize * YSize);
	LandscapeEdit.GetHeightDataFast(MinX, MinY, MaxX, MaxY, HeightData.GetData(), 0);

	return true;
}


void
FUnrealLandscapeTranslator::GetLandscapeProxyBounds(
	ALandscapeProxy* LandscapeProxy, FVector3d& Origin, FVector3d& Extents)
{
	// Iterate only on the landscape components
	FBox Bounds(ForceInit);
	for (const UActorComponent* ActorComponent : LandscapeProxy->GetComponents())
	{
		const ULandscapeComponent* LandscapeComp = Cast<const ULandscapeComponent>(ActorComponent);
		if (LandscapeComp && LandscapeComp->IsRegistered())
			Bounds += LandscapeComp->Bounds.GetBox();
	}

	// Convert the bounds to origin/offset vectors
	Bounds.GetCenterAndExtents(Origin, Extents);
}

void
FUnrealLandscapeTranslator::ApplyAttributesToHeightfieldNode(
	const HAPI_NodeId HeightId,
	const HAPI_PartId PartId,
	ALandscapeProxy* LandscapeProxy)
{
	UMaterialInterface* LandscapeMat = LandscapeProxy->GetLandscapeMaterial();
	UMaterialInterface* LandscapeHoleMat = LandscapeProxy->GetLandscapeHoleMaterial();
	UPhysicalMaterial* LandscapePhysMat = LandscapeProxy->DefaultPhysMaterial;

	AddLandscapeMaterialAttributesToVolume(HeightId, PartId, LandscapeMat, LandscapeHoleMat, LandscapePhysMat);

	// Add the landscape's actor tags as prim attributes if we have any    
	FHoudiniEngineUtils::CreateAttributesFromTags(HeightId, PartId, LandscapeProxy->Tags);

	// Add the unreal_actor_path attribute
	FHoudiniEngineUtils::AddActorPathAttribute(HeightId, PartId, LandscapeProxy, 1);

	// Add the unreal_level_path attribute
	ULevel* Level = LandscapeProxy->GetLevel();
	if (Level)
	{
		FHoudiniEngineUtils::AddLevelPathAttribute(HeightId, PartId, Level, 1);
		/*
		LevelPath = Level->GetPathName();

		// We just want the path up to the first point
		int32 DotIndex;
		if (LevelPath.FindChar('.', DotIndex))
			LevelPath.LeftInline(DotIndex, false);

		AddLevelPathAttributeToVolume(HeightId, PartId, LevelPath);
		*/
	}

	// Add streaming poxy attribute
	FHoudiniEngineUtils::AddLandscapeTypeAttribute(HeightId, PartId, LandscapeProxy, 1);
}


bool
FUnrealLandscapeTranslator::ConvertLandscapeDataToHeightFieldData(
	const TArray<uint16>& IntHeightData,
	int32 XSize,
	int32 YSize,
	FVector Min,
	FVector Max,
	const FVector3d & LandscapeActorScale,
	TArray<float>& HeightfieldFloatValues,
	HAPI_VolumeInfo& HeightfieldVolumeInfo)
{
	HeightfieldFloatValues.Empty(); 

	int32 HoudiniXSize = YSize;
	int32 HoudiniYSize = XSize;
	int32 SizeInPoints = HoudiniXSize * HoudiniYSize;
	if ((HoudiniXSize < 2) || (HoudiniYSize < 2))
		return false;

	if (IntHeightData.Num() != SizeInPoints)
		return false;

	// Use default unreal scaling for marshalling landscapes
	// A lot of precision will be lost in order to keep the same transform as the landscape input
	bool bUseDefaultUE4Scaling = false;
	const UHoudiniRuntimeSettings * HoudiniRuntimeSettings = GetDefault< UHoudiniRuntimeSettings >();
	if (HoudiniRuntimeSettings && HoudiniRuntimeSettings->MarshallingLandscapesUseDefaultUnrealScaling)
		bUseDefaultUE4Scaling = HoudiniRuntimeSettings->MarshallingLandscapesUseDefaultUnrealScaling;

	//--------------------------------------------------------------------------------------------------
	// Convert values to float
	//--------------------------------------------------------------------------------------------------

	// Convert the min/max values from cm to meters
	Min /= 100.0;
	Max /= 100.0;

	// Unreal's landscape uses 16bits precision and range from -256m to 256m with the default scale of 100.0
	// To convert the uint16 values to float "metric" values, offset the int by 32768 to center it,
	// then scale it

	// Spacing used to convert from uint16 to meters
	double ZSpacing = 512.0 / ((double)UINT16_MAX);
	ZSpacing *= LandscapeActorScale.Z / 100.0;

	// Center value in meters (Landscape ranges from [-255:257] meters at default scale
	double ZCenterOffset = 32767;

	// Convert the Int data to Float
	HeightfieldFloatValues.SetNumUninitialized(SizeInPoints);
	for (int32 nY = 0; nY < HoudiniYSize; nY++)
	{
		for (int32 nX = 0; nX < HoudiniXSize; nX++)
		{
			// We need to invert X/Y when reading the value from Unreal
			int32 nHoudini = nX + nY * HoudiniXSize;
			int32 nUnreal = nY + nX * XSize;

			// Convert the int values to meter
			// Unreal's digit value have a zero value of 32768
			// Don't apply z-position offsets to the data. This offset will be applied to the
			// heighfield primitive itself in Houdini.
			double DoubleValue = ((double)IntHeightData[nUnreal] - ZCenterOffset) * ZSpacing;
			HeightfieldFloatValues[nHoudini] = (float)DoubleValue;
		}
	}

	//--------------------------------------------------------------------------------------------------
	// Set the Hapi Transform. Houdini expects the scale to be set here, but we set the position
	// and rotation on the Geometry nodes, so clear here.
	//--------------------------------------------------------------------------------------------------

	HAPI_Transform HapiTransform;
	FHoudiniApi::Transform_Init(&HapiTransform);

	HapiTransform.rotationQuaternion[0] = 0;
	HapiTransform.rotationQuaternion[1] = 0;
	HapiTransform.rotationQuaternion[2] = 0;
	HapiTransform.rotationQuaternion[3] = 1;
	HapiTransform.position[1] = 0.0f;
	HapiTransform.position[0] = 0.0f;
	HapiTransform.position[2] = 0.0f;

	HapiTransform.scale[0] = LandscapeActorScale.Y * 0.5f * HoudiniXSize / 100.0f;
	HapiTransform.scale[1] = LandscapeActorScale.X * 0.5f * HoudiniYSize / 100.0f;
	HapiTransform.scale[2] = 0.5f;
	if (bUseDefaultUE4Scaling)
			HapiTransform.scale[2] *= LandscapeActorScale.Z;

	HapiTransform.shear[0] = 0.0f;
	HapiTransform.shear[1] = 0.0f;
	HapiTransform.shear[2] = 0.0f;
	
	//--------------------------------------------------------------------------------------------------
	// Fill the volume info
	//--------------------------------------------------------------------------------------------------
	HeightfieldVolumeInfo.xLength = HoudiniXSize;
	HeightfieldVolumeInfo.yLength = HoudiniYSize;
	HeightfieldVolumeInfo.zLength = 1;

	HeightfieldVolumeInfo.minX = 0;
	HeightfieldVolumeInfo.minY = 0;
	HeightfieldVolumeInfo.minZ = 0;

	HeightfieldVolumeInfo.transform = HapiTransform;

	HeightfieldVolumeInfo.type = HAPI_VOLUMETYPE_HOUDINI;
	HeightfieldVolumeInfo.storage = HAPI_STORAGETYPE_FLOAT;
	HeightfieldVolumeInfo.tupleSize = 1;
	HeightfieldVolumeInfo.tileSize = 1;

	HeightfieldVolumeInfo.hasTaper = false;
	HeightfieldVolumeInfo.xTaper = 0.0;
	HeightfieldVolumeInfo.yTaper = 0.0;

	return true;
}

bool
FUnrealLandscapeTranslator::CreateHeightfieldInputNode(
	const FString& NodeName,
	const int32& XSize,
	const int32& YSize,
	HAPI_NodeId& HeightfieldNodeId, 
	HAPI_NodeId& HeightNodeId, 
	HAPI_NodeId& MaskNodeId, 
	HAPI_NodeId& MergeNodeId,
	HAPI_NodeId ParentNodeId)
{
	// Make sure the Heightfield node doesnt already exists
	if (HeightfieldNodeId != -1)
		return false;

	// Convert the node's name
	std::string NameStr;
	FHoudiniEngineUtils::ConvertUnrealString(NodeName, NameStr);
	
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::CreateHeightFieldInput(
		FHoudiniEngine::Get().GetSession(),
		ParentNodeId, NameStr.c_str(), YSize, XSize, 1.0f, HAPI_HeightFieldSampling::HAPI_HEIGHTFIELD_SAMPLING_CORNER,
		&HeightfieldNodeId, &HeightNodeId, &MaskNodeId, &MergeNodeId), false);

	// Cook it
	return FHoudiniEngineUtils::HapiCookNode(HeightfieldNodeId, nullptr, true);
}

bool
FUnrealLandscapeTranslator::SetHeightfieldData(
	const HAPI_NodeId& VolumeNodeId,
	const HAPI_PartId& PartId,
	TArray<float>& FloatValues,
	const HAPI_VolumeInfo& VolumeInfo,
	const FString& HeightfieldName)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FUnrealLandscapeTranslator::SetHeightfieldData);

	// Cook the node to get proper infos on it
	if(!FHoudiniEngineUtils::HapiCookNode(VolumeNodeId, nullptr, true))
		return false;

	// Read the geo/part/volume info from the volume node
	HAPI_GeoInfo GeoInfo;
	FHoudiniApi::GeoInfo_Init(&GeoInfo);

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetGeoInfo(
		FHoudiniEngine::Get().GetSession(),
		VolumeNodeId, &GeoInfo), false);

	HAPI_PartInfo PartInfo;
	FHoudiniApi::PartInfo_Init(&PartInfo);
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetPartInfo(
		FHoudiniEngine::Get().GetSession(),
		GeoInfo.nodeId, PartId, &PartInfo), false);

	// Update the volume infos
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetVolumeInfo(
		FHoudiniEngine::Get().GetSession(),
		VolumeNodeId, PartInfo.id, &VolumeInfo), false);

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetHeightFieldData(
		GeoInfo.nodeId, PartInfo.id, FloatValues, HeightfieldName), false);

	return true;
}

bool FUnrealLandscapeTranslator::AddLandscapeMaterialAttributesToVolume(
	const HAPI_NodeId& VolumeNodeId, 
	const HAPI_PartId& PartId,
	UMaterialInterface* InLandscapeMaterial,
	UMaterialInterface* InLandscapeHoleMaterial,
	UPhysicalMaterial* InPhysicalMaterial)
{
	if (VolumeNodeId == -1)
		return false;

	// LANDSCAPE MATERIAL
	if (IsValid(InLandscapeMaterial))
	{
		// Extract the path name from the material interface
		FString InLandscapeMaterialString = InLandscapeMaterial->GetPathName();

		// Marshall in material names.
		HAPI_AttributeInfo AttributeInfoMaterial;
		FHoudiniApi::AttributeInfo_Init(&AttributeInfoMaterial);
		AttributeInfoMaterial.count = 1;
		AttributeInfoMaterial.tupleSize = 1;
		AttributeInfoMaterial.exists = true;
		AttributeInfoMaterial.owner = HAPI_ATTROWNER_PRIM;
		AttributeInfoMaterial.storage = HAPI_STORAGETYPE_STRING;
		AttributeInfoMaterial.originalOwner = HAPI_ATTROWNER_INVALID;

		HAPI_Result Result = FHoudiniApi::AddAttribute(
			FHoudiniEngine::Get().GetSession(), VolumeNodeId, PartId,
			HAPI_UNREAL_ATTRIB_MATERIAL, &AttributeInfoMaterial);

		if (HAPI_RESULT_SUCCESS == Result)
		{
			// Set the attribute's string data
			FHoudiniHapiAccessor Accessor(VolumeNodeId, PartId, HAPI_UNREAL_ATTRIB_MATERIAL);
			HOUDINI_CHECK_RETURN(Accessor.SetAttributeUniqueData(AttributeInfoMaterial, InLandscapeMaterialString), false);
		}

		if (Result != HAPI_RESULT_SUCCESS)
		{
			// Failed to create the attribute
			HOUDINI_LOG_WARNING(
				TEXT("Failed to upload unreal_material attribute for landscape: %s"),
				*FHoudiniEngineUtils::GetErrorDescription());
		}
	}

	// HOLE MATERIAL
	if (IsValid(InLandscapeHoleMaterial))
	{
		// Extract the path name from the material interface
		FString InLandscapeMaterialString = InLandscapeHoleMaterial->GetPathName();

		// Marshall in material names.
		HAPI_AttributeInfo AttributeInfoMaterial;
		FHoudiniApi::AttributeInfo_Init(&AttributeInfoMaterial);
		AttributeInfoMaterial.count = 1;
		AttributeInfoMaterial.tupleSize = 1;
		AttributeInfoMaterial.exists = true;
		AttributeInfoMaterial.owner = HAPI_ATTROWNER_PRIM;
		AttributeInfoMaterial.storage = HAPI_STORAGETYPE_STRING;
		AttributeInfoMaterial.originalOwner = HAPI_ATTROWNER_INVALID;

		HAPI_Result Result = FHoudiniApi::AddAttribute(
			FHoudiniEngine::Get().GetSession(), VolumeNodeId, PartId,
			HAPI_UNREAL_ATTRIB_MATERIAL_HOLE, &AttributeInfoMaterial);

		if (Result == HAPI_RESULT_SUCCESS)
		{
			// Set the attribute's string data
			FHoudiniHapiAccessor Accessor(VolumeNodeId, PartId, HAPI_UNREAL_ATTRIB_MATERIAL_HOLE);
			HOUDINI_CHECK_RETURN(Accessor.SetAttributeUniqueData(AttributeInfoMaterial, InLandscapeMaterialString), false);
		}

		if (Result != HAPI_RESULT_SUCCESS)
		{
			// Failed to create the attribute
			HOUDINI_LOG_WARNING(
				TEXT("Failed to upload unreal_hole_material attribute for landscape: %s"),
				*FHoudiniEngineUtils::GetErrorDescription());
		}
	}

	// PHYSICAL MATERIAL
	if (IsValid(InPhysicalMaterial))
	{
		// Extract the path name from the material interface
		FString InPhysMatlString = InPhysicalMaterial->GetPathName();

		// Marshall in material names.
		HAPI_AttributeInfo AttributeInfoMaterial;
		FHoudiniApi::AttributeInfo_Init(&AttributeInfoMaterial);
		AttributeInfoMaterial.count = 1;
		AttributeInfoMaterial.tupleSize = 1;
		AttributeInfoMaterial.exists = true;
		AttributeInfoMaterial.owner = HAPI_ATTROWNER_PRIM;
		AttributeInfoMaterial.storage = HAPI_STORAGETYPE_STRING;
		AttributeInfoMaterial.originalOwner = HAPI_ATTROWNER_INVALID;

		HAPI_Result Result = FHoudiniApi::AddAttribute(
			FHoudiniEngine::Get().GetSession(), VolumeNodeId, PartId,
			HAPI_UNREAL_ATTRIB_PHYSICAL_MATERIAL, &AttributeInfoMaterial);

		if (Result == HAPI_RESULT_SUCCESS)
		{
			// Set the attribute's string data
			FHoudiniHapiAccessor Accessor(VolumeNodeId, PartId, HAPI_UNREAL_ATTRIB_PHYSICAL_MATERIAL);
			HOUDINI_CHECK_RETURN(Accessor.SetAttributeUniqueData(AttributeInfoMaterial, InPhysMatlString), false);
		}

		if (Result != HAPI_RESULT_SUCCESS)
		{
			// Failed to create the attribute
			HOUDINI_LOG_WARNING(
				TEXT("Failed to upload unreal_physical_material attribute for landscape: %s"),
				*FHoudiniEngineUtils::GetErrorDescription());
		}
	}

	return true;
}


bool
FUnrealLandscapeTranslator::GetLandscapeTargetLayerData(
	ALandscapeProxy* LandscapeProxy,
	ULandscapeInfo* LandscapeInfo,
	int32 TargetLayerIndex,
	TArray<uint8>& TargetLayerData,
	FLinearColor& TargetLayerDebugColor,
	FString& TargetLayerName)
{
	if (!IsValid(LandscapeInfo) || !IsValid(LandscapeProxy))
		return false;

	// Get the landscape X/Y Size
	int32 MinX = MAX_int32;
	int32 MinY = MAX_int32;
	int32 MaxX = -MAX_int32;
	int32 MaxY = -MAX_int32;

	ALandscape* Landscape = LandscapeProxy->GetLandscapeActor();
	if (LandscapeProxy == Landscape)
	{
		// The proxy is a landscape actor, so we have to use the landscape extent (landscape components
		// may have been moved to proxies and may not be present on this actor).
		LandscapeInfo->GetLandscapeExtent(MinX, MinY, MaxX, MaxY);
	}
	else
	{
		// We only want to get the data for this landscape proxy.
		// To handle streaming proxies correctly, get the extents via all the components,
		// not by calling GetLandscapeExtent or we'll end up sending ALL the streaming proxies.
		for (const ULandscapeComponent* Comp : LandscapeProxy->LandscapeComponents)
		{
			Comp->GetComponentExtent(MinX, MinY, MaxX, MaxY);
		}
	}

	if(MinX == MAX_int32 || MinY == MAX_int32 || MaxX == -MAX_int32 || MaxY == -MAX_int32)
		return false;

	if (!GetLandscapeTargetLayerData(
		LandscapeInfo, TargetLayerIndex,
		MinX, MinY, MaxX, MaxY,
		TargetLayerData, TargetLayerDebugColor, TargetLayerName))
		return false;

	if (FName(TargetLayerName).Compare(ALandscape::VisibilityLayer->LayerName) ==0)
	{
		// If we encounter the visibility layer, make sure we name
		// it according to the plugin's expectations instead of using the internal `DataLayer__` name.
		TargetLayerName = HAPI_UNREAL_VISIBILITY_LAYER_NAME;
	}

	return true;
}

bool
FUnrealLandscapeTranslator::GetLandscapeTargetLayerData(
	ULandscapeInfo* LandscapeInfo,
	int32 TargetLayerIndex,
	int32 MinX, int32 MinY,
	int32 MaxX, int32 MaxY,
	TArray<uint8>& TargetLayerData,
	FLinearColor& TargetLayerUsageDebugColor,
	FString& TargetLayerName)
{
	if (!LandscapeInfo)
		return false;

	if (!LandscapeInfo->Layers.IsValidIndex(TargetLayerIndex))
		return false;

	FLandscapeInfoLayerSettings LayersSetting = LandscapeInfo->Layers[TargetLayerIndex];
	ULandscapeLayerInfoObject* LayerInfo = LayersSetting.LayerInfoObj;
	if (!LayerInfo)
		return false;

	// Calc the X/Y size in points
	int32 XSize = (MaxX - MinX + 1);
	int32 YSize = (MaxY - MinY + 1);
	if ((XSize < 2) || (YSize < 2))
		return false;

	// extracting the uint8 values from the layer
	FLandscapeEditDataInterface LandscapeEdit(LandscapeInfo);
	// Ensure we're not triggering a checkout, as we're just reading data
	LandscapeEdit.SetShouldDirtyPackage(false);

	TargetLayerData.AddZeroed(XSize * YSize);
	LandscapeEdit.GetWeightDataFast(LayerInfo, MinX, MinY, MaxX, MaxY, TargetLayerData.GetData(), 0);

	TargetLayerUsageDebugColor = LayerInfo->LayerUsageDebugColor;

	TargetLayerName = LayersSetting.GetLayerName().ToString();

	return true;
}

bool
FUnrealLandscapeTranslator::InitDefaultHeightfieldMask(
	const HAPI_VolumeInfo& HeightVolumeInfo,
	const HAPI_NodeId& MaskVolumeNodeId)
{
	// We need to have a mask layer as it is required for proper heightfield functionalities

	// Creating an array filled with 0.0
	TArray<float> MaskFloatData;
	MaskFloatData.SetNumUninitialized(HeightVolumeInfo.xLength * HeightVolumeInfo.yLength);
	for (int32 n = 0; n < MaskFloatData.Num(); n++)
		MaskFloatData[n] = 0.0f;

	// Creating the volume infos
	HAPI_VolumeInfo MaskVolumeInfo = HeightVolumeInfo;

	// Set the heighfield data in Houdini
	FString MaskName = TEXT("mask");
	HAPI_PartId PartId = 0;
	if (!SetHeightfieldData(MaskVolumeNodeId, PartId, MaskFloatData, MaskVolumeInfo, MaskName))
		return false;

	return true;
}

bool
FUnrealLandscapeTranslator::DestroyLandscapeAssetNode(HAPI_NodeId& ConnectedAssetId, TArray<HAPI_NodeId>& CreatedInputAssetIds)
{
	HAPI_AssetInfo NodeAssetInfo;
	FHoudiniApi::AssetInfo_Init(&NodeAssetInfo);
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetAssetInfo(
		FHoudiniEngine::Get().GetSession(), ConnectedAssetId, &NodeAssetInfo), false);

	FHoudiniEngineString AssetOpName(NodeAssetInfo.fullOpNameSH);
	FString OpName;
	if (!AssetOpName.ToFString(OpName))
		return false;

	if (!OpName.Contains(TEXT("xform")))
	{
		// Not a transform node, so not a Heightfield
		// We just need to destroy the landscape asset node
		return FHoudiniEngineUtils::DestroyHoudiniAsset(ConnectedAssetId);
	}

	// The landscape was marshalled as a heightfield, so we need to destroy and disconnect
	// the volvis nodes, all the merge node's input (each merge input is a volume for one 
	// of the layer/mask of the landscape )

	// Query the volvis node id
	// The volvis node is the fist input of the xform node
	HAPI_NodeId VolvisNodeId = -1;
	FHoudiniApi::QueryNodeInput(
		FHoudiniEngine::Get().GetSession(),
		ConnectedAssetId, 0, &VolvisNodeId);

	// First, destroy the merge node and its inputs
	// The merge node is in the first input of the volvis node
	HAPI_NodeId MergeNodeId = -1;
	FHoudiniApi::QueryNodeInput(
		FHoudiniEngine::Get().GetSession(),
		VolvisNodeId, 0, &MergeNodeId);

	if (MergeNodeId != -1)
	{
		// Get the merge node info
		HAPI_NodeInfo NodeInfo;
		FHoudiniApi::NodeInfo_Init(&NodeInfo);
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetNodeInfo(
			FHoudiniEngine::Get().GetSession(), MergeNodeId, &NodeInfo), false);

		for (int32 n = 0; n < NodeInfo.inputCount; n++)
		{
			// Get the Input node ID from the host ID
			HAPI_NodeId InputNodeId = -1;
			if (HAPI_RESULT_SUCCESS != FHoudiniApi::QueryNodeInput(
				FHoudiniEngine::Get().GetSession(),
				MergeNodeId, n, &InputNodeId))
				break;

			if (InputNodeId == -1)
				break;

			// Disconnect and Destroy that input
			FHoudiniEngineUtils::HapiDisconnectAsset(MergeNodeId, n);
			FHoudiniEngineUtils::DestroyHoudiniAsset(InputNodeId);
		}
	}

	// Second step, destroy all the volumes GEO assets
	for (HAPI_NodeId AssetNodeId : CreatedInputAssetIds)
	{
		FHoudiniEngineUtils::DestroyHoudiniAsset(AssetNodeId);
	}
	CreatedInputAssetIds.Empty();

	// Finally disconnect and destroy the xform, volvis and merge nodes, then destroy them
	FHoudiniEngineUtils::HapiDisconnectAsset(ConnectedAssetId, 0);
	FHoudiniEngineUtils::HapiDisconnectAsset(VolvisNodeId, 0);
	FHoudiniEngineUtils::DestroyHoudiniAsset(MergeNodeId);
	FHoudiniEngineUtils::DestroyHoudiniAsset(VolvisNodeId);

	return FHoudiniEngineUtils::DestroyHoudiniAsset(ConnectedAssetId);
}


bool
FUnrealLandscapeTranslator::ExtractLandscapeData(
	ALandscapeProxy * LandscapeProxy, TSet<ULandscapeComponent *>& SelectedComponents,
	const bool& bExportLighting, const bool& bExportTileUVs, const bool& bExportNormalizedUVs,
	const bool bApplyWorldTransform,
	TArray<FVector3f>& LandscapePositionArray,
	TArray<FVector3f>& LandscapeNormalArray,
	TArray<FVector3f>& LandscapeUVArray,
	TArray<FIntPoint>& LandscapeComponentVertexIndicesArray,
	TArray<const char *>& LandscapeComponentNameArray,
	TArray<FLinearColor>& LandscapeLightmapValues)
{
	if (!LandscapeProxy)
		return false;

	if (SelectedComponents.Num() < 1)
		return false;

	// Get runtime settings.
	const UHoudiniRuntimeSettings * HoudiniRuntimeSettings = GetDefault< UHoudiniRuntimeSettings >();

	// Since we use GetWorldVertex when fetching the landscape vertices, we need to calculate the landscape world
	// transform here so that we can apply its inverse to the world-space vertices if we don't want to output the
	// landscape data in world space
	FTransform LandscapeTransform = FTransform::Identity;
	if (!bApplyWorldTransform)
	{
		LandscapeTransform = FHoudiniEngineRuntimeUtils::CalculateHoudiniLandscapeTransform(LandscapeProxy);
		LandscapeTransform.SetScale3D(FVector::OneVector);
	}

	// Calc all the needed sizes
	int32 ComponentSizeQuads = ((LandscapeProxy->ComponentSizeQuads + 1) >> LandscapeProxy->ExportLOD) - 1;
	float ScaleFactor = (float)LandscapeProxy->ComponentSizeQuads / (float)ComponentSizeQuads;

	int32 NumComponents = SelectedComponents.Num();
	bool bExportOnlySelected = NumComponents != LandscapeProxy->LandscapeComponents.Num();

	int32 VertexCountPerComponent = FMath::Square(ComponentSizeQuads + 1);
	int32 VertexCount = NumComponents * VertexCountPerComponent;
	if (!VertexCount)
		return false;

	// Initialize the data arrays    
	LandscapePositionArray.SetNumUninitialized(VertexCount);
	LandscapeNormalArray.SetNumUninitialized(VertexCount);
	LandscapeUVArray.SetNumUninitialized(VertexCount);
	LandscapeComponentNameArray.SetNumUninitialized(VertexCount);
	LandscapeComponentVertexIndicesArray.SetNumUninitialized(VertexCount);
	if (bExportLighting)
		LandscapeLightmapValues.SetNumUninitialized(VertexCount);

	//-----------------------------------------------------------------------------------------------------------------
	// EXTRACT THE LANDSCAPE DATA
	//-----------------------------------------------------------------------------------------------------------------
	FIntPoint IntPointMax = FIntPoint::ZeroValue;

	int32 AllPositionsIdx = 0;
	for (int32 ComponentIdx = 0; ComponentIdx < LandscapeProxy->LandscapeComponents.Num(); ComponentIdx++)
	{
		ULandscapeComponent * LandscapeComponent = LandscapeProxy->LandscapeComponents[ComponentIdx];
		if (bExportOnlySelected && !SelectedComponents.Contains(LandscapeComponent))
			continue;

		TArray64< uint8 > LightmapMipData;
		int32 LightmapMipSizeX = 0;
		int32 LightmapMipSizeY = 0;

		// See if we need to export lighting information.
		if (bExportLighting)
		{
			const FMeshMapBuildData* MapBuildData = LandscapeComponent->GetMeshMapBuildData();
			FLightMap2D* LightMap2D = MapBuildData && MapBuildData->LightMap ? MapBuildData->LightMap->GetLightMap2D() : nullptr;
			if (LightMap2D && LightMap2D->IsValid(0))
			{
				UTexture2D * TextureLightmap = LightMap2D->GetTexture(0);
				if (TextureLightmap)
				{
					if (TextureLightmap->Source.GetMipData(LightmapMipData, 0, 0, 0, nullptr))
					{
						LightmapMipSizeX = TextureLightmap->Source.GetSizeX();
						LightmapMipSizeY = TextureLightmap->Source.GetSizeY();
					}
					else
					{
						LightmapMipData.Empty();
					}
				}
			}
		}

		// Construct landscape component data interface to access raw data.
		FLandscapeComponentDataInterface CDI(LandscapeComponent, LandscapeProxy->ExportLOD);

		// Get name of this landscape component.
		const char * LandscapeComponentNameStr = FHoudiniEngineUtils::ExtractRawString(LandscapeComponent->GetName());
		for (int32 VertexIdx = 0; VertexIdx < VertexCountPerComponent; VertexIdx++)
		{
			int32 VertX = 0;
			int32 VertY = 0;
			CDI.VertexIndexToXY(VertexIdx, VertX, VertY);

			// Get position.
			FVector PositionVector = CDI.GetWorldVertex(VertX, VertY);
			if (!bApplyWorldTransform)
				PositionVector = LandscapeTransform.InverseTransformPosition(PositionVector);

			// Get normal / tangent / binormal.
			FVector Normal = FVector::ZeroVector;
			FVector TangentX = FVector::ZeroVector;
			FVector TangentY = FVector::ZeroVector;
			CDI.GetLocalTangentVectors(VertX, VertY, TangentX, TangentY, Normal);

			// Export UVs.
			FVector TextureUV = FVector::ZeroVector;
			if (bExportTileUVs)
			{
				// We want to export uvs per tile.
				TextureUV = FVector(VertX, VertY, 0.0f);

				// If we need to normalize UV space.
				if (bExportNormalizedUVs)
					TextureUV /= ComponentSizeQuads;
			}
			else
			{
				// We want to export global uvs (default).
				FIntPoint IntPoint = LandscapeComponent->GetSectionBase();
				TextureUV = FVector(VertX * ScaleFactor + IntPoint.X, VertY * ScaleFactor + IntPoint.Y, 0.0f);

				// Keep track of max offset.
				IntPointMax = IntPointMax.ComponentMax(IntPoint);
			}

			if (bExportLighting)
			{
				FLinearColor VertexLightmapColor(0.0f, 0.0f, 0.0f, 1.0f);
				if (LightmapMipData.Num() > 0)
				{
					FVector2D UVCoord(VertX, VertY);
					UVCoord /= (ComponentSizeQuads + 1);

					FColor LightmapColorRaw = PickVertexColorFromTextureMip(
						LightmapMipData.GetData(), UVCoord, LightmapMipSizeX, LightmapMipSizeY);

					VertexLightmapColor = LightmapColorRaw.ReinterpretAsLinear();
				}

				LandscapeLightmapValues[AllPositionsIdx] = VertexLightmapColor;
			}

			// Retrieve component transform.
			const FTransform & ComponentTransform = LandscapeComponent->GetComponentTransform();

			// Retrieve component scale.
			const FVector & ScaleVector = ComponentTransform.GetScale3D();

			// Perform normalization.
			Normal /= ScaleVector;
			Normal.Normalize();

			TangentX /= ScaleVector;
			TangentX.Normalize();

			TangentY /= ScaleVector;
			TangentY.Normalize();

			// Perform position scaling.
			FVector3f PositionTransformed = (FVector3f)PositionVector / HAPI_UNREAL_SCALE_FACTOR_POSITION;
			LandscapePositionArray[AllPositionsIdx].X = PositionTransformed.X;
			LandscapePositionArray[AllPositionsIdx].Y = PositionTransformed.Z;
			LandscapePositionArray[AllPositionsIdx].Z = PositionTransformed.Y;

			Swap(Normal.Y, Normal.Z);

			// Store landscape component name for this point.
			LandscapeComponentNameArray[AllPositionsIdx] = LandscapeComponentNameStr;

			// Store vertex index (x,y) for this point.
			LandscapeComponentVertexIndicesArray[AllPositionsIdx].X = VertX;
			LandscapeComponentVertexIndicesArray[AllPositionsIdx].Y = VertY;

			// Store point normal.
			LandscapeNormalArray[AllPositionsIdx] = (FVector3f)Normal;

			// Store uv.
			LandscapeUVArray[AllPositionsIdx] = (FVector3f)TextureUV;

			AllPositionsIdx++;
		}

		// Free the memory allocated for LandscapeComponentNameStr
		FHoudiniEngineUtils::FreeRawStringMemory(LandscapeComponentNameStr);
	}

	// If we need to normalize UV space and we are doing global UVs.
	if (!bExportTileUVs && bExportNormalizedUVs)
	{
		IntPointMax += FIntPoint(ComponentSizeQuads, ComponentSizeQuads);
		IntPointMax = IntPointMax.ComponentMax(FIntPoint(1, 1));

		for (int32 UVIdx = 0; UVIdx < VertexCount; ++UVIdx)
		{
			FVector3f& PositionUV = LandscapeUVArray[UVIdx];
			PositionUV.X /= IntPointMax.X;
			PositionUV.Y /= IntPointMax.Y;
		}
	}

	return true;
}

FColor
FUnrealLandscapeTranslator::PickVertexColorFromTextureMip(
	const uint8 * MipBytes, FVector2D & UVCoord, int32 MipWidth, int32 MipHeight)
{
	check(MipBytes);

	FColor ResultColor(0, 0, 0, 255);

	if (UVCoord.X >= 0.0f && UVCoord.X < 1.0f && UVCoord.Y >= 0.0f && UVCoord.Y < 1.0f)
	{
		const int32 X = MipWidth * UVCoord.X;
		const int32 Y = MipHeight * UVCoord.Y;

		const int32 Index = ((Y * MipWidth) + X) * 4;

		ResultColor.B = MipBytes[Index + 0];
		ResultColor.G = MipBytes[Index + 1];
		ResultColor.R = MipBytes[Index + 2];
		ResultColor.A = MipBytes[Index + 3];
	}

	return ResultColor;
}

bool 
FUnrealLandscapeTranslator::AddLandscapePositionAttribute(const HAPI_NodeId& NodeId, const TArray<FVector3f>& LandscapePositionArray)
{
	int32 VertexCount = LandscapePositionArray.Num();
	if (VertexCount < 3)
		return false;

	// Create point attribute info containing positions.    
	HAPI_AttributeInfo AttributeInfoPointPosition;
	FHoudiniApi::AttributeInfo_Init(&AttributeInfoPointPosition);
	//FMemory::Memzero< HAPI_AttributeInfo >( AttributeInfoPointPosition );
	AttributeInfoPointPosition.count = VertexCount;
	AttributeInfoPointPosition.tupleSize = 3;
	AttributeInfoPointPosition.exists = true;
	AttributeInfoPointPosition.owner = HAPI_ATTROWNER_POINT;
	AttributeInfoPointPosition.storage = HAPI_STORAGETYPE_FLOAT;
	AttributeInfoPointPosition.originalOwner = HAPI_ATTROWNER_INVALID;

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
		FHoudiniEngine::Get().GetSession(), NodeId, 0,
		HAPI_UNREAL_ATTRIB_POSITION, &AttributeInfoPointPosition), false);


	FHoudiniHapiAccessor Accessor(NodeId, 0, HAPI_UNREAL_ATTRIB_POSITION);
	bool bSuccess = Accessor.SetAttributeData(AttributeInfoPointPosition, (const float*)LandscapePositionArray.GetData());

	return bSuccess;
}

bool 
FUnrealLandscapeTranslator::AddLandscapeNormalAttribute(const HAPI_NodeId& NodeId, const TArray<FVector3f>& LandscapeNormalArray)
{
	int32 VertexCount = LandscapeNormalArray.Num();
	if (VertexCount < 3)
		return false;

	HAPI_AttributeInfo AttributeInfoPointNormal;
	FHoudiniApi::AttributeInfo_Init(&AttributeInfoPointNormal);
	AttributeInfoPointNormal.count = VertexCount;
	AttributeInfoPointNormal.tupleSize = 3;
	AttributeInfoPointNormal.exists = true;
	AttributeInfoPointNormal.owner = HAPI_ATTROWNER_POINT;
	AttributeInfoPointNormal.storage = HAPI_STORAGETYPE_FLOAT;
	AttributeInfoPointNormal.originalOwner = HAPI_ATTROWNER_INVALID;

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
		FHoudiniEngine::Get().GetSession(), NodeId,
		0, HAPI_UNREAL_ATTRIB_NORMAL, &AttributeInfoPointNormal), false);

	FHoudiniHapiAccessor Accessor(NodeId, 0, HAPI_UNREAL_ATTRIB_NORMAL);
	bool bSuccess = Accessor.SetAttributeData(AttributeInfoPointNormal, (const float*)LandscapeNormalArray.GetData());

	return bSuccess;
}

bool 
FUnrealLandscapeTranslator::AddLandscapeUVAttribute(const HAPI_NodeId& NodeId, const TArray<FVector3f>& LandscapeUVArray)
{
	int32 VertexCount = LandscapeUVArray.Num();
	if (VertexCount < 3)
		return false;

	HAPI_AttributeInfo AttributeInfoPointUV;
	FHoudiniApi::AttributeInfo_Init(&AttributeInfoPointUV);
	AttributeInfoPointUV.count = VertexCount;
	AttributeInfoPointUV.tupleSize = 3;
	AttributeInfoPointUV.exists = true;
	AttributeInfoPointUV.owner = HAPI_ATTROWNER_POINT;
	AttributeInfoPointUV.storage = HAPI_STORAGETYPE_FLOAT;
	AttributeInfoPointUV.originalOwner = HAPI_ATTROWNER_INVALID;

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
		FHoudiniEngine::Get().GetSession(), NodeId,
		0, HAPI_UNREAL_ATTRIB_UV, &AttributeInfoPointUV), false);

	FHoudiniHapiAccessor Accessor(NodeId, 0, HAPI_UNREAL_ATTRIB_UV);
	bool bSuccess = Accessor.SetAttributeData(AttributeInfoPointUV, (const float*)LandscapeUVArray.GetData());

	return bSuccess;
}

bool 
FUnrealLandscapeTranslator::AddLandscapeComponentVertexIndicesAttribute(const HAPI_NodeId& NodeId, const TArray<FIntPoint>& LandscapeComponentVertexIndicesArray)
{
	int32 VertexCount = LandscapeComponentVertexIndicesArray.Num();
	if (VertexCount < 3)
		return false;

	HAPI_AttributeInfo AttributeInfoPointLandscapeComponentVertexIndices;
	FHoudiniApi::AttributeInfo_Init(&AttributeInfoPointLandscapeComponentVertexIndices);
	AttributeInfoPointLandscapeComponentVertexIndices.count = VertexCount;
	AttributeInfoPointLandscapeComponentVertexIndices.tupleSize = 2;
	AttributeInfoPointLandscapeComponentVertexIndices.exists = true;
	AttributeInfoPointLandscapeComponentVertexIndices.owner = HAPI_ATTROWNER_POINT;
	AttributeInfoPointLandscapeComponentVertexIndices.storage = HAPI_STORAGETYPE_INT;
	AttributeInfoPointLandscapeComponentVertexIndices.originalOwner = HAPI_ATTROWNER_INVALID;

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
		FHoudiniEngine::Get().GetSession(), NodeId,
		0, HAPI_UNREAL_ATTRIB_LANDSCAPE_VERTEX_INDEX,
		&AttributeInfoPointLandscapeComponentVertexIndices), false);

	FHoudiniHapiAccessor Accessor(NodeId, 0, HAPI_UNREAL_ATTRIB_LANDSCAPE_VERTEX_INDEX);
	HOUDINI_CHECK_RETURN(Accessor.SetAttributeData(AttributeInfoPointLandscapeComponentVertexIndices, (const int*)LandscapeComponentVertexIndicesArray.GetData()), false);

	return true;
}

bool 
FUnrealLandscapeTranslator::AddLandscapeComponentNameAttribute(const HAPI_NodeId& NodeId, const TArray<const char *>& LandscapeComponentNameArray)
{
	int32 VertexCount = LandscapeComponentNameArray.Num();
	if (VertexCount < 3)
		return false;

	// Create point attribute containing landscape component name.
	HAPI_AttributeInfo AttributeInfoPointLandscapeComponentNames;
	FHoudiniApi::AttributeInfo_Init(&AttributeInfoPointLandscapeComponentNames);
	AttributeInfoPointLandscapeComponentNames.count = VertexCount;
	AttributeInfoPointLandscapeComponentNames.tupleSize = 1;
	AttributeInfoPointLandscapeComponentNames.exists = true;
	AttributeInfoPointLandscapeComponentNames.owner = HAPI_ATTROWNER_POINT;
	AttributeInfoPointLandscapeComponentNames.storage = HAPI_STORAGETYPE_STRING;
	AttributeInfoPointLandscapeComponentNames.originalOwner = HAPI_ATTROWNER_INVALID;

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
		FHoudiniEngine::Get().GetSession(), NodeId, 0,
		HAPI_UNREAL_ATTRIB_LANDSCAPE_TILE_NAME,
		&AttributeInfoPointLandscapeComponentNames), false);

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetAttributeStringData(
		FHoudiniEngine::Get().GetSession(), NodeId, 0,
		HAPI_UNREAL_ATTRIB_LANDSCAPE_TILE_NAME,
		&AttributeInfoPointLandscapeComponentNames,
		(const char**)LandscapeComponentNameArray.GetData(),
		0, AttributeInfoPointLandscapeComponentNames.count), false);

	return true;
}

bool FUnrealLandscapeTranslator::AddLandscapeTileAttribute(
    const HAPI_NodeId& NodeId, const HAPI_PartId& PartId, const int32& TileIdx )
{
	HAPI_AttributeInfo AttributeInfoTileIndex;
	FHoudiniApi::AttributeInfo_Init(&AttributeInfoTileIndex);
	
	AttributeInfoTileIndex.count = 1;
	AttributeInfoTileIndex.tupleSize = 1;
	AttributeInfoTileIndex.exists = true;
	AttributeInfoTileIndex.owner = HAPI_ATTROWNER_PRIM;
	AttributeInfoTileIndex.storage = HAPI_STORAGETYPE_INT;
	AttributeInfoTileIndex.originalOwner = HAPI_ATTROWNER_INVALID;

	HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::AddAttribute(
		FHoudiniEngine::Get().GetSession(), NodeId,
		PartId, HAPI_UNREAL_ATTRIB_LANDSCAPE_TILE, &AttributeInfoTileIndex ), false );

	HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::SetAttributeIntData(
		FHoudiniEngine::Get().GetSession(),
		NodeId, PartId, HAPI_UNREAL_ATTRIB_LANDSCAPE_TILE, &AttributeInfoTileIndex,
		(const int *)&TileIdx, 0, AttributeInfoTileIndex.count ), false );

	return true;
}

bool 
FUnrealLandscapeTranslator::AddLandscapeLightmapColorAttribute(const HAPI_NodeId& NodeId, const TArray<FLinearColor>& LandscapeLightmapValues)
{
	int32 VertexCount = LandscapeLightmapValues.Num();

	HAPI_AttributeInfo AttributeInfoPointLightmapColor;
	FHoudiniApi::AttributeInfo_Init(&AttributeInfoPointLightmapColor);
	//FMemory::Memzero< HAPI_AttributeInfo >( AttributeInfoPointLightmapColor );
	AttributeInfoPointLightmapColor.count = VertexCount;
	AttributeInfoPointLightmapColor.tupleSize = 4;
	AttributeInfoPointLightmapColor.exists = true;
	AttributeInfoPointLightmapColor.owner = HAPI_ATTROWNER_POINT;
	AttributeInfoPointLightmapColor.storage = HAPI_STORAGETYPE_FLOAT;
	AttributeInfoPointLightmapColor.originalOwner = HAPI_ATTROWNER_INVALID;

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
		FHoudiniEngine::Get().GetSession(), NodeId,
		0, HAPI_UNREAL_ATTRIB_LIGHTMAP_COLOR, &AttributeInfoPointLightmapColor), false);

	FHoudiniHapiAccessor Accessor(NodeId, 0, HAPI_UNREAL_ATTRIB_LIGHTMAP_COLOR);
	bool bSuccess = Accessor.SetAttributeData(AttributeInfoPointLightmapColor, (const float*)LandscapeLightmapValues.GetData());

	return true;
}

bool 
FUnrealLandscapeTranslator::AddLandscapeMeshIndicesAndMaterialsAttribute(
	const HAPI_NodeId& NodeId, const bool& bExportMaterials,
	const int32& ComponentSizeQuads, const int32& QuadCount,
	ALandscapeProxy * LandscapeProxy,
	const TSet< ULandscapeComponent * >& SelectedComponents)
{
	if (!LandscapeProxy)
		return false;

	// Compute number of necessary indices.
	int32 IndexCount = QuadCount * 4;
	if (IndexCount < 0)
		return false;

	int32 VertexCountPerComponent = FMath::Square(ComponentSizeQuads + 1);

	// Array holding indices data.
	TArray<int32> LandscapeIndices;
	LandscapeIndices.SetNumUninitialized(IndexCount);

	// Allocate space for face names.
	// The LandscapeMaterial and HoleMaterial per point
	FHoudiniEngineIndexedStringMap FaceMaterials;
    FHoudiniEngineIndexedStringMap FaceHoleMaterials;

	int32 VertIdx = 0;
	int32 QuadIdx = 0;

	FString MatrialName;
    FString HoleMaterialName;

	const int32 QuadComponentCount = ComponentSizeQuads + 1;
	for (int32 ComponentIdx = 0; ComponentIdx < LandscapeProxy->LandscapeComponents.Num(); ComponentIdx++)
	{
		ULandscapeComponent * LandscapeComponent = LandscapeProxy->LandscapeComponents[ComponentIdx];
		if (!SelectedComponents.Contains(LandscapeComponent))
			continue;

		if (bExportMaterials)
		{
			// If component has an override material, we need to get the raw name (if exporting materials).
			if (LandscapeComponent->OverrideMaterial)
			{
				MatrialName = LandscapeComponent->OverrideMaterial->GetName();
			}

			// If component has an override hole material, we need to get the raw name (if exporting materials).
			if (LandscapeComponent->OverrideHoleMaterial)
			{
				HoleMaterialName = LandscapeComponent->OverrideHoleMaterial->GetName();
			}
		}

		int32 BaseVertIndex = ComponentIdx * VertexCountPerComponent;
		for (int32 YIdx = 0; YIdx < ComponentSizeQuads; YIdx++)
		{
			for (int32 XIdx = 0; XIdx < ComponentSizeQuads; XIdx++)
			{
				LandscapeIndices[VertIdx + 0] = BaseVertIndex + (XIdx + 0) + (YIdx + 0) * QuadComponentCount;
				LandscapeIndices[VertIdx + 1] = BaseVertIndex + (XIdx + 1) + (YIdx + 0) * QuadComponentCount;
				LandscapeIndices[VertIdx + 2] = BaseVertIndex + (XIdx + 1) + (YIdx + 1) * QuadComponentCount;
				LandscapeIndices[VertIdx + 3] = BaseVertIndex + (XIdx + 0) + (YIdx + 1) * QuadComponentCount;

				// Store override materials (if exporting materials).
				if (bExportMaterials)
				{
					FaceMaterials.SetString(QuadIdx, MatrialName);
					FaceHoleMaterials.SetString(QuadIdx, HoleMaterialName);
				}

				VertIdx += 4;
				QuadIdx++;
			}
		}
	}

	// We can now set vertex list.
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetVertexList(
		LandscapeIndices, NodeId, 0), false);

	// We need to generate array of face counts.
	TArray<int32> LandscapeFaces;
	LandscapeFaces.SetNumUninitialized(QuadCount);
	for (int32 n = 0; n < QuadCount; n++)
		LandscapeFaces[n] = 4;

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiSetFaceCounts(
		LandscapeFaces, NodeId, 0), false);

	if (bExportMaterials)
	{
        if (FaceMaterials.HasEntries())
        {
            // Marshall in override primitive material names.
            HAPI_AttributeInfo AttributeInfoPrimitiveMaterial;
            FHoudiniApi::AttributeInfo_Init(&AttributeInfoPrimitiveMaterial);
            AttributeInfoPrimitiveMaterial.count = FaceMaterials.GetIds().Num();
            AttributeInfoPrimitiveMaterial.tupleSize = 1;
            AttributeInfoPrimitiveMaterial.exists = true;
            AttributeInfoPrimitiveMaterial.owner = HAPI_ATTROWNER_PRIM;
            AttributeInfoPrimitiveMaterial.storage = HAPI_STORAGETYPE_STRING;
            AttributeInfoPrimitiveMaterial.originalOwner = HAPI_ATTROWNER_INVALID;

            HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
                                           FHoudiniEngine::Get().GetSession(),
                                           NodeId, 0, HAPI_UNREAL_ATTRIB_MATERIAL, &AttributeInfoPrimitiveMaterial),
                                       false);

			FHoudiniHapiAccessor Accessor(NodeId, 0, HAPI_UNREAL_ATTRIB_MATERIAL);
			HOUDINI_CHECK_RETURN(Accessor.SetAttributeStringMap(AttributeInfoPrimitiveMaterial, FaceMaterials), false);
        }

        if (FaceHoleMaterials.HasEntries())
        {
            // Marshall in override primitive material hole names.
            HAPI_AttributeInfo AttributeInfoPrimitiveMaterialHole;
            FHoudiniApi::AttributeInfo_Init(&AttributeInfoPrimitiveMaterialHole);
            AttributeInfoPrimitiveMaterialHole.count = FaceHoleMaterials.GetIds().Num();
            AttributeInfoPrimitiveMaterialHole.tupleSize = 1;
            AttributeInfoPrimitiveMaterialHole.exists = true;
            AttributeInfoPrimitiveMaterialHole.owner = HAPI_ATTROWNER_PRIM;
            AttributeInfoPrimitiveMaterialHole.storage = HAPI_STORAGETYPE_STRING;
            AttributeInfoPrimitiveMaterialHole.originalOwner = HAPI_ATTROWNER_INVALID;

            HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
                                           FHoudiniEngine::Get().GetSession(),
                                           NodeId, 0, HAPI_UNREAL_ATTRIB_MATERIAL_HOLE,
                                           &AttributeInfoPrimitiveMaterialHole),
                                       false);

			FHoudiniHapiAccessor Accessor(NodeId, 0, HAPI_UNREAL_ATTRIB_MATERIAL_HOLE);
			HOUDINI_CHECK_RETURN(Accessor.SetAttributeStringMap(AttributeInfoPrimitiveMaterialHole, FaceHoleMaterials), false);
        }
	}		

    return true;
}

bool 
FUnrealLandscapeTranslator::AddLandscapeGlobalMaterialAttribute(
	const HAPI_NodeId& NodeId, ALandscapeProxy * LandscapeProxy)
{
	if (!LandscapeProxy)
		return false;

	// If there's a global landscape material, we marshall it as detail.
	UMaterialInterface * MaterialInterface = LandscapeProxy->GetLandscapeMaterial();

	HAPI_AttributeInfo AttributeInfoDetailMaterial;
	FHoudiniApi::AttributeInfo_Init(&AttributeInfoDetailMaterial);
	AttributeInfoDetailMaterial.count = 1;
	AttributeInfoDetailMaterial.tupleSize = 1;
	AttributeInfoDetailMaterial.exists = true;
	AttributeInfoDetailMaterial.owner = HAPI_ATTROWNER_DETAIL;
	AttributeInfoDetailMaterial.storage = HAPI_STORAGETYPE_STRING;
	AttributeInfoDetailMaterial.originalOwner = HAPI_ATTROWNER_INVALID;

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
		FHoudiniEngine::Get().GetSession(), NodeId, 0,
		HAPI_UNREAL_ATTRIB_MATERIAL, &AttributeInfoDetailMaterial), false);

	FHoudiniHapiAccessor Accessor(NodeId, 0, HAPI_UNREAL_ATTRIB_MATERIAL);
	HOUDINI_CHECK_RETURN(Accessor.SetAttributeUniqueData(AttributeInfoDetailMaterial, MaterialInterface->GetPathName()), false);

	// If there's a global landscape hole material, we marshall it as detail.
	UMaterialInterface * HoleMaterialInterface = LandscapeProxy->GetLandscapeHoleMaterial();

	HAPI_AttributeInfo AttributeInfoDetailMaterialHole;
	FHoudiniApi::AttributeInfo_Init(&AttributeInfoDetailMaterialHole);
	AttributeInfoDetailMaterialHole.count = 1;
	AttributeInfoDetailMaterialHole.tupleSize = 1;
	AttributeInfoDetailMaterialHole.exists = true;
	AttributeInfoDetailMaterialHole.owner = HAPI_ATTROWNER_DETAIL;
	AttributeInfoDetailMaterialHole.storage = HAPI_STORAGETYPE_STRING;
	AttributeInfoDetailMaterialHole.originalOwner = HAPI_ATTROWNER_INVALID;

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
		FHoudiniEngine::Get().GetSession(),
		NodeId, 0, HAPI_UNREAL_ATTRIB_MATERIAL_HOLE,
		&AttributeInfoDetailMaterialHole), false);

	Accessor.Init(NodeId, 0, HAPI_UNREAL_ATTRIB_MATERIAL_HOLE);
	HOUDINI_CHECK_RETURN(Accessor.SetAttributeUniqueData(AttributeInfoDetailMaterialHole, HoleMaterialInterface->GetPathName()), false);

	return true;
}


bool
FUnrealLandscapeTranslator::AddLandscapeLayerAttribute(
	const HAPI_NodeId& NodeId, const TArray<float>& LandscapeLayerArray, const FString& LayerName)
{
	int32 VertexCount = LandscapeLayerArray.Num();
	if (VertexCount < 3)
		return false;

	HAPI_AttributeInfo AttributeInfoLayer;
	FHoudiniApi::AttributeInfo_Init(&AttributeInfoLayer);
	AttributeInfoLayer.count = VertexCount;
	AttributeInfoLayer.tupleSize = 1;
	AttributeInfoLayer.exists = true;
	AttributeInfoLayer.owner = HAPI_ATTROWNER_POINT;
	AttributeInfoLayer.storage = HAPI_STORAGETYPE_FLOAT;
	AttributeInfoLayer.originalOwner = HAPI_ATTROWNER_INVALID;

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
		FHoudiniEngine::Get().GetSession(),
		NodeId, 0,
		TCHAR_TO_ANSI(*LayerName),
		&AttributeInfoLayer), false);

	FHoudiniHapiAccessor Accessor(NodeId, 0, TCHAR_TO_ANSI(*LayerName));
	HOUDINI_CHECK_RETURN(Accessor.SetAttributeData(AttributeInfoLayer, LandscapeLayerArray), false);

	return true;
}

HAPI_NodeId
FUnrealLandscapeTranslator::CreateVolumeLayer(ALandscapeProxy* LandscapeProxy, 
		const FString & VolumeNameLayer,
		const HAPI_Transform & NodeTransform, 
		HAPI_NodeId HeightFieldId,
		HAPI_PartId PartId,
		HAPI_PartId MaskId,
		int XSize, 
		int YSize, 
		TArray<float> & Data)
{
	// Create the volume

	HAPI_VolumeInfo CurrentLayerVolumeInfo;
	FHoudiniApi::VolumeInfo_Init(&CurrentLayerVolumeInfo);
	CurrentLayerVolumeInfo.transform = NodeTransform;
	CurrentLayerVolumeInfo.xLength = XSize;
	CurrentLayerVolumeInfo.yLength = YSize;
	CurrentLayerVolumeInfo.zLength = 1;
	CurrentLayerVolumeInfo.minX = 0;
	CurrentLayerVolumeInfo.minY = 0;
	CurrentLayerVolumeInfo.minZ = 0;
	CurrentLayerVolumeInfo.type = HAPI_VOLUMETYPE_HOUDINI;
	CurrentLayerVolumeInfo.storage = HAPI_STORAGETYPE_FLOAT;
	CurrentLayerVolumeInfo.tupleSize = 1;
	CurrentLayerVolumeInfo.tileSize = 1;
	CurrentLayerVolumeInfo.hasTaper = false;
	CurrentLayerVolumeInfo.xTaper = 0.0;
	CurrentLayerVolumeInfo.yTaper = 0.0;

	// 3. See if we need to create an input volume, or can reuse the HF's default mask volume
	bool IsMask = VolumeNameLayer.Equals(TEXT("mask"), ESearchCase::IgnoreCase);

	HAPI_NodeId LayerVolumeNodeId = -1;
	if (!IsMask)
	{
		// Current layer is not mask, so we need to create a new input volume
		std::string TargetLayerNameString;
		FHoudiniEngineUtils::ConvertUnrealString(VolumeNameLayer, TargetLayerNameString);
		FHoudiniApi::CreateHeightfieldInputVolumeNode(FHoudiniEngine::Get().GetSession(), HeightFieldId, &LayerVolumeNodeId, TargetLayerNameString.c_str(), XSize, YSize, 1.0f);
	}
	else
	{
		// Current Layer is mask, so we simply reuse the mask volume node created by default by the heightfield node
		LayerVolumeNodeId = MaskId;
	}

	// Check if we have a valid id for the input volume.
	if (!FHoudiniEngineUtils::IsHoudiniNodeValid(LayerVolumeNodeId))
		return -1;

	// 4. Set the layer/mask height field data in Houdini
	HAPI_PartId CurrentPartId = 0;
	if (!SetHeightfieldData(LayerVolumeNodeId, PartId, Data, CurrentLayerVolumeInfo, VolumeNameLayer))
		return -1;

	// Apply attributes to the height field input node
	ApplyAttributesToHeightfieldNode(LayerVolumeNodeId, PartId, LandscapeProxy);

	// Commit the volume's geo
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiCommitGeo(LayerVolumeNodeId), -1);

	return LayerVolumeNodeId;

}

bool FUnrealLandscapeTranslator::SendTargetLayersToHoudini(
	ALandscapeProxy* LandscapeProxy,
	HAPI_NodeId HeightFieldId,
	HAPI_PartId PartId,
	HAPI_NodeId MergeId,
	HAPI_NodeId MaskId,
	const FHoudiniLandscapeExportOptions& Options,
	const HAPI_VolumeInfo& HeightFieldVolumeInfo,
	int32 XSize,
	int32 YSize,
	int32& OutMergeInputIndex)
{

	bool bSuccess = true;

	// Send target layers to Houdini. Note, we always need to create and commit a Mask or the Volume Info won't function
	// correctly.
	if (Options.bExportMergedPaintLayers)
	{
		bSuccess &= SendCombinedTargetLayersToHoudini(LandscapeProxy, HeightFieldId, PartId, MergeId, MaskId, HeightFieldVolumeInfo, XSize, YSize, OutMergeInputIndex);
	}
	else
	{
		InitDefaultHeightfieldMask(HeightFieldVolumeInfo, MaskId);
		ApplyAttributesToHeightfieldNode(MaskId, PartId, LandscapeProxy);
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiCommitGeo(MaskId), false);
	}

	if (Options.bExportPaintLayersPerEditLayer)
		bSuccess &= SendAllEditLayerTargetLayersToHoudini(LandscapeProxy, HeightFieldId, PartId, MergeId, MaskId, HeightFieldVolumeInfo, XSize, YSize, OutMergeInputIndex);

	return bSuccess;
}

bool FUnrealLandscapeTranslator::SendCombinedTargetLayersToHoudini(
	ALandscapeProxy* LandscapeProxy,
	HAPI_NodeId HeightFieldId,
	HAPI_PartId PartId,
	HAPI_NodeId MergeId,
	HAPI_NodeId MaskId,
	const HAPI_VolumeInfo& HeightfieldVolumeInfo,
	int32 XSize,
	int32 YSize,
	int32 & OutMergeInputIndex)
{
	// This function sends the combined target (paint) layers to Houdini.
	// "Combined" means that all target layers in all edit layers are combined.

	ULandscapeInfo* LandscapeInfo = LandscapeProxy->GetLandscapeInfo();
	if (!LandscapeInfo)
		return false;

	bool MaskInitialized = false;

	int32 NumTargetLayers = LandscapeInfo->Layers.Num();
	for (int32 TargetLayerIndex = 0; TargetLayerIndex < NumTargetLayers; TargetLayerIndex++)
	{
		// 1. Extract the uint8 values from the layer
		TArray<uint8> LayerData;
		FLinearColor TargetLayerDebugColor;
		FString TargetLayerName;
		if (!GetLandscapeTargetLayerData(LandscapeProxy, LandscapeInfo, TargetLayerIndex, LayerData, TargetLayerDebugColor, TargetLayerName))
		{
			continue;
		}

		// 2. Convert unreal uint8 values to floats
		// If the layer came from Houdini, additional info might have been stored in the DebugColor to convert the data back to float

		TArray<float> CurrentLayerFloatData;
		if (!ConvertLandscapeLayerDataToHeightfieldData(LayerData, XSize, YSize, TargetLayerDebugColor, CurrentLayerFloatData))
		{
			continue;
		}

		int LayerVolumeNodeId = CreateVolumeLayer(LandscapeProxy,
			TargetLayerName,
			HeightfieldVolumeInfo.transform,
			HeightFieldId,
			PartId,
			MaskId,
			YSize,
			XSize,
			CurrentLayerFloatData);

		if (LayerVolumeNodeId == -1)
			return false;

		if (!TargetLayerName.Equals(TEXT("mask"), ESearchCase::IgnoreCase))
		{
			// We had to create a new volume for this layer, so we need to connect it to the HF's merge node
			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::ConnectNodeInput(FHoudiniEngine::Get().GetSession(), MergeId, OutMergeInputIndex, LayerVolumeNodeId, 0),false);
			OutMergeInputIndex++;
		}
		else
		{
			MaskInitialized = true;
		}
	}

	// We need to have a mask layer as it is required for proper height field functionality
	// Setting the volume info on the mask is needed for the HF to have proper transform in H!
	// If we didn't create a mask volume before, send a default one now
	if (!MaskInitialized)
	{
		InitDefaultHeightfieldMask(HeightfieldVolumeInfo, MaskId);

		ApplyAttributesToHeightfieldNode(MaskId, PartId, LandscapeProxy);

		// Commit the mask volume's geo.
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniEngineUtils::HapiCommitGeo(MaskId), false);		
	}

	return true;
}

bool FUnrealLandscapeTranslator::SendAllEditLayerTargetLayersToHoudini(
	ALandscapeProxy* LandscapeProxy,
	HAPI_NodeId HeightFieldId,
	HAPI_PartId PartId,
	HAPI_NodeId MergeId,
	HAPI_NodeId MaskId,
	const HAPI_VolumeInfo& HeightfieldVolumeInfo,
	int32 XSize,
	int32 YSize,
	int32& OutMergeInputIndex)
{
	// This function sends the Target (paint) layer for each Edit Layer separately to Houdini.

	ULandscapeInfo* LandscapeInfo = LandscapeProxy->GetLandscapeInfo();
	if (!LandscapeInfo)
		return false;

	ALandscape * Landscape = LandscapeProxy->GetLandscapeActor();
	for(int EditLayerIndex = 0; EditLayerIndex < Landscape->GetLayerCount(); EditLayerIndex++)
	{
		int32 NumTargetLayers = LandscapeInfo->Layers.Num();
		for (int32 TargetLayerIndex = 0; TargetLayerIndex < NumTargetLayers; TargetLayerIndex++)
		{
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
			const FName & EditLayerName = LandscapeProxy->GetLandscapeActor()->GetLayerConst(EditLayerIndex)->Name;
#else
			const FName & EditLayerName = LandscapeProxy->GetLandscapeActor()->GetLayer(EditLayerIndex)->Name;
#endif
			const FName & TargetLayerName = LandscapeInfo->Layers[TargetLayerIndex].GetLayerName();

			FHoudiniExtents Extents = FHoudiniLandscapeUtils::GetLandscapeExtents(LandscapeProxy);
			TArray<uint8_t> LayerData = FHoudiniLandscapeUtils::GetLayerData(Landscape, Extents, EditLayerName, TargetLayerName);

			ULandscapeLayerInfoObject * LayerInfoObject = Landscape->GetLandscapeInfo()->GetLayerInfoByName(TargetLayerName);

			FLinearColor Color = LayerInfoObject ? LayerInfoObject->LayerUsageDebugColor : FLinearColor::White;

			TArray<float> CurrentLayerFloatData;
			if (!ConvertLandscapeLayerDataToHeightfieldData(LayerData, XSize, YSize, Color, CurrentLayerFloatData))
			{
				continue;
			}

			FString LayerName = FString::Format(TEXT("landscapelayer_{0}_{1}"), { EditLayerName.ToString(), TargetLayerName.ToString() });

			int LayerVolumeNodeId = CreateVolumeLayer(LandscapeProxy,
				LayerName,
				HeightfieldVolumeInfo.transform,
				HeightFieldId,
				PartId,
				MaskId,
				YSize,
				XSize,
				CurrentLayerFloatData);

			if (LayerVolumeNodeId == -1)
				return false;

			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::ConnectNodeInput(FHoudiniEngine::Get().GetSession(), MergeId, OutMergeInputIndex, LayerVolumeNodeId, 0), false);
			OutMergeInputIndex++;

		}
	}

	return true;
}

