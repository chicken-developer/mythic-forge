#include "Commandlets/KnightCampaignAssetAuthoring.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Campaign/KnightCampaignEntry.h"
#include "Commandlets/KnightStoryAssetAuthoring.h"
#include "Definitions/KnightStoryWorldDefinition.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/FileManager.h"
#include "Interaction/KnightStoryAnchor.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"
#include "Story/KnightStoryProfile.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

namespace
{
	const FString Root(TEXT("/Game/Knight/Definitions/Campaign/"));
	const FString SchoolMap(TEXT("/Game/Knight/Maps/Playables/L_M07_Campaign"));
	const FString CourtMap(TEXT("/Game/Knight/Maps/Playables/L_M07_Courtyard"));

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

bool KnightCampaignAssetAuthoring::CreateAssets()
{
	UKnightStoryWorldDefinition* BaseWorld = LoadObject<UKnightStoryWorldDefinition>(nullptr,
		TEXT("/Game/Knight/Definitions/Story/DA_Knight_World_ForgeSchool.DA_Knight_World_ForgeSchool"));
	UKnightStoryProfile* BaseProfile = LoadObject<UKnightStoryProfile>(nullptr,
		TEXT("/Game/Knight/Definitions/Story/DA_Knight_Profile_DemoV2.DA_Knight_Profile_DemoV2"));
	if (BaseWorld == nullptr || BaseProfile == nullptr)
	{
		return false;
	}
	TArray<FKnightMapRegion> Regions;
	for (const bool bCourt : {false, true})
	{
		FKnightMapRegion& Region = Regions.AddDefaulted_GetRef();
		Region.RegionId = bCourt ? TEXT("Courtyard") : TEXT("School");
		Region.Label = bCourt ? NSLOCTEXT("KnightCampaign", "Courtyard", "Training courtyard")
			: NSLOCTEXT("KnightCampaign", "School", "Boro's forge school");
		Region.Map = TSoftObjectPtr<UWorld>(FSoftObjectPath(bCourt ? CourtMap : SchoolMap));
		Region.HalfExtent = FVector2D(2300.0, 2000.0);
		Region.NorthYaw = bCourt ? 90.0f : 0.0f;
	}
	TArray<FKnightMapMarker> Markers;
	for (const FKnightMapRegion& Region : Regions)
	{
		for (int32 Index = 0; Index < BaseWorld->Interactions.Num(); ++Index)
		{
			const FKnightStoryInteraction& Interaction = BaseWorld->Interactions[Index];
			FKnightMapMarker& Marker = Markers.AddDefaulted_GetRef();
			Marker.MarkerId = FName(*(Region.RegionId.ToString() + TEXT("_") + Interaction.AnchorId.ToString()));
			Marker.RegionId = Region.RegionId;
			Marker.AnchorId = Interaction.AnchorId;
			Marker.Label = FText::FromString(Interaction.Label.ToString().Replace(TEXT("[F]"), TEXT("")));
			Marker.WorldPosition = FVector(-1500.0 + (Index % 7) * 450.0, -1300.0 + (Index / 7) * 700.0, 60.0);
			Marker.Kind = Interaction.Action == EKnightStoryAction::Checkpoint ? EKnightMapMarkerKind::Checkpoint
				: Interaction.Action == EKnightStoryAction::Travel ? EKnightMapMarkerKind::Travel
				: Interaction.Action == EKnightStoryAction::Conversation ? EKnightMapMarkerKind::Quest : EKnightMapMarkerKind::Landmark;
			if (Interaction.Action == EKnightStoryAction::Travel)
			{
				Marker.TravelRegionId = Region.RegionId == TEXT("School") ? TEXT("Courtyard") : TEXT("School");
			}
		}
	}
	for (const bool bCourt : {false, true})
	{
		const FString Suffix = bCourt ? TEXT("Courtyard") : TEXT("School");
		const FName Id(*(TEXT("CampaignUX_") + Suffix));
		UKnightStoryWorldDefinition* Definition = Asset<UKnightStoryWorldDefinition>(Root + TEXT("DA_Knight_World_") + Suffix);
		UKnightStoryProfile* Profile = Asset<UKnightStoryProfile>(Root + TEXT("DA_Knight_Profile_") + Suffix);
		if (Definition == nullptr || Profile == nullptr || !Definition->TrySetIdentity(Id, TEXT("KnightStoryWorld"))
			|| !Profile->TrySetIdentity(Id, TEXT("KnightStoryProfile")))
		{
			return false;
		}
		Definition->SetDisplayName(bCourt ? Regions[1].Label : Regions[0].Label);
		Definition->Interactions = BaseWorld->Interactions;
		Definition->MapRegions = Regions;
		Definition->MapMarkers = Markers;
		for (FKnightStoryInteraction& Interaction : Definition->Interactions)
		{
			Interaction.Label = FText::FromString(Interaction.Label.ToString().Replace(TEXT("[F]"), TEXT("")));
			if (Interaction.Action == EKnightStoryAction::Travel)
			{
				Interaction.Destination = TSoftObjectPtr<UWorld>(FSoftObjectPath(bCourt ? SchoolMap : CourtMap));
				Interaction.Label = bCourt ? NSLOCTEXT("KnightCampaign", "ReturnSchool", "RETURN TO FORGE SCHOOL")
					: NSLOCTEXT("KnightCampaign", "VisitCourt", "VISIT COURTYARD");
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
	USoundClass* Master = Asset<USoundClass>(TEXT("/Game/Knight/Audio/SC_Knight_Master"));
	if (Master == nullptr)
	{
		return false;
	}
	Master->ChildClasses.Reset();
	for (const TCHAR* Name : {TEXT("Music"), TEXT("Effects"), TEXT("Voice")})
	{
		USoundClass* Child = Asset<USoundClass>(FString(TEXT("/Game/Knight/Audio/SC_Knight_")) + Name);
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
	return Save(Master) && Save(Asset<USoundMix>(TEXT("/Game/Knight/Audio/SMix_Knight_User")));
}

bool KnightCampaignAssetAuthoring::PopulateMap(UWorld* World, const bool bCourtyard)
{
	if (!KnightStoryAssetAuthoring::PopulateStoryMap(World, 6))
	{
		return false;
	}
	TArray<AKnightStoryEntry*> Replaced;
	for (TActorIterator<AKnightStoryEntry> It(World); It; ++It)
	{
		Replaced.Add(*It);
	}
	for (AKnightStoryEntry* Previous : Replaced)
	{
		World->DestroyActor(Previous);
	}
	AKnightCampaignEntry* Entry = World->SpawnActor<AKnightCampaignEntry>();
	if (Entry == nullptr)
	{
		return false;
	}
	Entry->RegionId = bCourtyard ? TEXT("Courtyard") : TEXT("School");
	Entry->bFrontEndEntry = !bCourtyard;
	Entry->ProfileId = FPrimaryAssetId(TEXT("KnightStoryProfile"),
		bCourtyard ? TEXT("CampaignUX_Courtyard") : TEXT("CampaignUX_School"));
	const FString Suffix = bCourtyard ? TEXT("Courtyard") : TEXT("School");
	const FString Path = Root + TEXT("DA_Knight_World_") + Suffix;
	UKnightStoryWorldDefinition* Definition = LoadObject<UKnightStoryWorldDefinition>(nullptr,
		*(Path + TEXT(".DA_Knight_World_") + Suffix));
	if (Definition == nullptr)
	{
		return false;
	}
	for (TActorIterator<AKnightStoryAnchor> It(World); It; ++It)
	{
		if (const FKnightStoryInteraction* Interaction = Definition->FindInteraction(It->AnchorId))
		{
			It->Label = Interaction->Label;
			It->RefreshPresentation();
		}
	}
	return true;
}
