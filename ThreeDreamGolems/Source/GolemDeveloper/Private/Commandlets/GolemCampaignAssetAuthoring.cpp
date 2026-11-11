#include "Commandlets/GolemCampaignAssetAuthoring.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Campaign/GolemCampaignEntry.h"
#include "Commandlets/GolemStoryAssetAuthoring.h"
#include "Definitions/GolemStoryWorldDefinition.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/FileManager.h"
#include "Interaction/GolemStoryAnchor.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"
#include "Story/GolemStoryProfile.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

namespace
{
	const FString CampaignRoot(TEXT("/Game/Golem/Definitions/Campaign/"));
	const FString SchoolMap(TEXT("/Game/Golem/Maps/Playables/L_M07_Campaign"));
	const FString CourtMap(TEXT("/Game/Golem/Maps/Playables/L_M07_Courtyard"));

	template <typename T>
	T* Asset(const FString& Path)
	{
		UPackage* Package = FPackageName::DoesPackageExist(Path)
			? LoadPackage(nullptr, *Path, LOAD_None) : CreatePackage(*Path);
		if (Package == nullptr)
		{
			return nullptr;
		}
		const FName Name(*FPackageName::GetLongPackageAssetName(Path));
		T* Result = FindObject<T>(Package, *Name.ToString());
		if (Result == nullptr)
		{
			Result = NewObject<T>(Package, Name, RF_Public | RF_Standalone);
			FAssetRegistryModule::AssetCreated(Result);
		}
		return Result;
	}

	bool Save(UObject* Object)
	{
		if (Object == nullptr)
		{
			return false;
		}
		UPackage* Package = Object->GetOutermost();
		Package->MarkPackageDirty();
		const FString File = FPackageName::LongPackageNameToFilename(Package->GetName(),
			FPackageName::GetAssetPackageExtension());
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(File), true);
		FSavePackageArgs Args;
		Args.TopLevelFlags = RF_Public | RF_Standalone;
		return UPackage::SavePackage(Package, Object, *File, Args);
	}
}

bool GolemCampaignAssetAuthoring::CreateAssets()
{
	UGolemStoryWorldDefinition* BaseWorld = LoadObject<UGolemStoryWorldDefinition>(nullptr,
		TEXT("/Game/Golem/Definitions/Story/DA_Golem_World_ForgeSchool.DA_Golem_World_ForgeSchool"));
	UGolemStoryProfile* BaseProfile = LoadObject<UGolemStoryProfile>(nullptr,
		TEXT("/Game/Golem/Definitions/Story/DA_Golem_Profile_DemoV2.DA_Golem_Profile_DemoV2"));
	if (BaseWorld == nullptr || BaseProfile == nullptr)
	{
		return false;
	}
	TArray<FGolemMapRegion> Regions;
	for (const bool bCourt : {false, true})
	{
		FGolemMapRegion& Region = Regions.AddDefaulted_GetRef();
		Region.RegionId = bCourt ? TEXT("Courtyard") : TEXT("School");
		Region.Label = bCourt ? NSLOCTEXT("GolemCampaign", "Courtyard", "Training courtyard")
			: NSLOCTEXT("GolemCampaign", "School", "Boro's forge school");
		Region.Map = TSoftObjectPtr<UWorld>(FSoftObjectPath(bCourt ? CourtMap : SchoolMap));
		Region.HalfExtent = FVector2D(2300.0, 2000.0);
		Region.NorthYaw = bCourt ? 90.0f : 0.0f;
	}
	TArray<FGolemMapMarker> Markers;
	for (const FGolemMapRegion& Region : Regions)
	{
		for (int32 Index = 0; Index < BaseWorld->Interactions.Num(); ++Index)
		{
			const FGolemStoryInteraction& Interaction = BaseWorld->Interactions[Index];
			FGolemMapMarker& Marker = Markers.AddDefaulted_GetRef();
			Marker.MarkerId = FName(*(Region.RegionId.ToString() + TEXT("_") + Interaction.AnchorId.ToString()));
			Marker.RegionId = Region.RegionId;
			Marker.AnchorId = Interaction.AnchorId;
			Marker.Label = FText::FromString(Interaction.Label.ToString().Replace(TEXT("[F]"), TEXT("")));
			Marker.WorldPosition = FVector(-1500.0 + (Index % 7) * 450.0, -1300.0 + (Index / 7) * 700.0, 60.0);
			Marker.Kind = Interaction.Action == EGolemStoryAction::Checkpoint ? EGolemMapMarkerKind::Checkpoint
				: Interaction.Action == EGolemStoryAction::Travel ? EGolemMapMarkerKind::Travel
				: Interaction.Action == EGolemStoryAction::Conversation ? EGolemMapMarkerKind::Quest : EGolemMapMarkerKind::Landmark;
			if (Interaction.Action == EGolemStoryAction::Travel)
			{
				Marker.TravelRegionId = Region.RegionId == TEXT("School") ? TEXT("Courtyard") : TEXT("School");
			}
		}
	}
	for (const bool bCourt : {false, true})
	{
		const FString Suffix = bCourt ? TEXT("Courtyard") : TEXT("School");
		const FName Id(*(TEXT("CampaignUX_") + Suffix));
		UGolemStoryWorldDefinition* Definition = Asset<UGolemStoryWorldDefinition>(CampaignRoot + TEXT("DA_Golem_World_") + Suffix);
		UGolemStoryProfile* Profile = Asset<UGolemStoryProfile>(CampaignRoot + TEXT("DA_Golem_Profile_") + Suffix);
		if (Definition == nullptr || Profile == nullptr || !Definition->TrySetIdentity(Id, TEXT("GolemStoryWorld"))
			|| !Profile->TrySetIdentity(Id, TEXT("GolemStoryProfile")))
		{
			return false;
		}
		Definition->SetDisplayName(bCourt ? Regions[1].Label : Regions[0].Label);
		Definition->Interactions = BaseWorld->Interactions;
		Definition->MapRegions = Regions;
		Definition->MapMarkers = Markers;
		for (FGolemStoryInteraction& Interaction : Definition->Interactions)
		{
			Interaction.Label = FText::FromString(Interaction.Label.ToString().Replace(TEXT("[F]"), TEXT("")));
			if (Interaction.Action == EGolemStoryAction::Travel)
			{
				Interaction.Destination = TSoftObjectPtr<UWorld>(FSoftObjectPath(bCourt ? SchoolMap : CourtMap));
				Interaction.Label = bCourt ? NSLOCTEXT("GolemCampaign", "ReturnSchool", "RETURN TO FORGE SCHOOL")
					: NSLOCTEXT("GolemCampaign", "VisitCourt", "VISIT COURTYARD");
			}
		}
		Profile->CampaignDefinitionId = BaseProfile->CampaignDefinitionId;
		Profile->WorldDefinitionId = Definition->GetPrimaryAssetId();
		Profile->Rewards = BaseProfile->Rewards;
		Profile->bDeveloperFixture = false;
		Profile->DeveloperSeedItems.Reset();
		Profile->SetDisplayName(Definition->GetDisplayName());
		if (!Definition->ValidateWorld().IsSuccess() || !Save(Definition) || !Save(Profile))
		{
			return false;
		}
	}
	USoundClass* Master = Asset<USoundClass>(TEXT("/Game/Golem/Audio/SC_Golem_Master"));
	if (Master == nullptr)
	{
		return false;
	}
	Master->ChildClasses.Reset();
	for (const TCHAR* Name : {TEXT("Music"), TEXT("Effects"), TEXT("Voice")})
	{
		USoundClass* Child = Asset<USoundClass>(FString(TEXT("/Game/Golem/Audio/SC_Golem_")) + Name);
		if (Child == nullptr)
		{
			return false;
		}
		Child->ParentClass = Master;
		Master->ChildClasses.Add(Child);
		if (!Save(Child))
		{
			return false;
		}
	}
	return Save(Master) && Save(Asset<USoundMix>(TEXT("/Game/Golem/Audio/SMix_Golem_User")));
}

bool GolemCampaignAssetAuthoring::PopulateMap(UWorld* World, const bool bCourtyard)
{
	if (!GolemStoryAssetAuthoring::PopulateStoryMap(World, 6))
	{
		return false;
	}
	TArray<AGolemStoryEntry*> Replaced;
	for (TActorIterator<AGolemStoryEntry> It(World); It; ++It)
	{
		Replaced.Add(*It);
	}
	for (AGolemStoryEntry* Previous : Replaced)
	{
		World->DestroyActor(Previous);
	}
	AGolemCampaignEntry* Entry = World->SpawnActor<AGolemCampaignEntry>();
	if (Entry == nullptr)
	{
		return false;
	}
	Entry->RegionId = bCourtyard ? TEXT("Courtyard") : TEXT("School");
	Entry->bFrontEndEntry = !bCourtyard;
	Entry->ProfileId = FPrimaryAssetId(TEXT("GolemStoryProfile"),
		bCourtyard ? TEXT("CampaignUX_Courtyard") : TEXT("CampaignUX_School"));
	const FString Suffix = bCourtyard ? TEXT("Courtyard") : TEXT("School");
	const FString Path = CampaignRoot + TEXT("DA_Golem_World_") + Suffix;
	UGolemStoryWorldDefinition* Definition = LoadObject<UGolemStoryWorldDefinition>(nullptr,
		*(Path + TEXT(".DA_Golem_World_") + Suffix));
	if (Definition == nullptr)
	{
		return false;
	}
	for (TActorIterator<AGolemStoryAnchor> It(World); It; ++It)
	{
		if (const FGolemStoryInteraction* Interaction = Definition->FindInteraction(It->AnchorId))
		{
			It->Label = Interaction->Label;
			It->RefreshPresentation();
		}
	}
	return true;
}
