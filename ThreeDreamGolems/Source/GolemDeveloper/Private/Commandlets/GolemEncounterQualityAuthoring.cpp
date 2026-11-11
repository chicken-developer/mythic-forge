#include "Commandlets/GolemEncounterQualityAuthoring.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Campaign/GolemCampaignEntry.h"
#include "Campaign/GolemEncounterQualityDefinition.h"
#include "Catalog/GolemGearCatalog.h"
#include "Definitions/GolemAIArchetypeDefinition.h"
#include "Definitions/GolemCampaignDefinition.h"
#include "Definitions/GolemStoryWorldDefinition.h"
#include "Engine/World.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"
#include "HAL/FileManager.h"
#include "Interaction/GolemStoryAnchor.h"
#include "Interaction/GolemStoryGate.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "SkillLab/GolemFrozenSurface.h"
#include "SkillLab/GolemRunestone.h"
#include "Story/GolemStoryProfile.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

namespace
{
	const FString QualityRoot(TEXT("/Game/Knight/Definitions/Encounter/"));
	const FString TrialMapPackageName(TEXT("/Game/Knight/Maps/Playables/L_M07_5_ForgeTrial"));
	template <typename T>
	T* Placed(UWorld* World, const FString& Name, const FVector& Position)
	{
		T* Result = nullptr;
		for (TActorIterator<T> It(World); It; ++It)
		{
			if (It->GetFName() == FName(*Name))
			{
				Result = *It;
				break;
			}
		}
		if (Result == nullptr)
		{
			FActorSpawnParameters Parameters;
			Parameters.Name = FName(*Name);
			Result = World->SpawnActor<T>(Parameters);
		}
		if (Result != nullptr)
		{
			Result->SetActorLocation(Position);
		}
		return Result;
	}

	template <typename T>
	T* Asset(const FString& Name, const TCHAR* Type, const TCHAR* Id)
	{
		const FString Path = QualityRoot + Name;
		UPackage* Package = FPackageName::DoesPackageExist(Path) ? LoadPackage(nullptr, *Path, LOAD_None) : CreatePackage(*Path);
		T* Result = Package != nullptr ? FindObject<T>(Package, *Name) : nullptr;
		if (Result == nullptr && Package != nullptr)
		{
			Result = NewObject<T>(Package, FName(*Name), RF_Public | RF_Standalone);
			FAssetRegistryModule::AssetCreated(Result);
		}
		return Result != nullptr && Result->TrySetIdentity(FName(Id), FName(Type)) ? Result : nullptr;
	}

	bool SaveQualityAsset(UObject* Object)
	{
		if (Object == nullptr)
		{
			return false;
		}
		UPackage* Package = Object->GetOutermost();
		Package->MarkPackageDirty();
		const FString File = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(File), true);
		FSavePackageArgs Args;
		Args.TopLevelFlags = RF_Public | RF_Standalone;
		return UPackage::SavePackage(Package, Object, *File, Args);
	}
}

bool GolemEncounterQualityAuthoring::CreateAssets()
{
	UGolemAIArchetypeDefinition* Base = LoadObject<UGolemAIArchetypeDefinition>(nullptr,
		TEXT("/Game/Knight/Definitions/AI/DA_Knight_Archetype_ArenaEnemy.DA_Knight_Archetype_ArenaEnemy"));
	if (Base == nullptr)
	{
		return false;
	}
	const TCHAR* Names[]{TEXT("TrialMelee"), TEXT("TrialRanged"), TEXT("TrialSupport")};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Names); ++Index)
	{
		UGolemAIArchetypeDefinition* Archetype = Asset<UGolemAIArchetypeDefinition>(
			FString(TEXT("DA_Knight_Archetype_")) + Names[Index], TEXT("KnightAIArchetype"), Names[Index]);
		if (Archetype == nullptr)
		{
			return false;
		}
		FGolemAIArchetypeTuning Tuning = Base->GetTuning();
		Tuning.Attributes.MaxHealth = Index == 0 ? 150.0f : 110.0f;
		Tuning.Attributes.AttackPower = 8.0f;
		Archetype->Specialization = FGolemSkillBuild();
		Archetype->Specialization.SpecId = Index == 1 ? FName(TEXT("Pluto")) : Index == 2 ? FName(TEXT("Terra")) : NAME_None;
		Archetype->SetDisplayName(Index == 0 ? NSLOCTEXT("KnightTrial", "Melee", "Hammer guard")
			: Index == 1 ? NSLOCTEXT("KnightTrial", "Ranged", "Frost caster") : NSLOCTEXT("KnightTrial", "Support", "Earth mender"));
		if (!Archetype->TrySetIds(Base->GetCombatProfileId(), Base->GetActionSetId()).IsSuccess()
			|| !Archetype->TrySetTuning(Tuning).IsSuccess()
			|| !Archetype->TrySetDecisionAssets(Base->GetStateTree(), Base->GetRepositionQuery()).IsSuccess()
			|| !Archetype->ValidateArchetype().IsSuccess() || !SaveQualityAsset(Archetype))
		{
			return false;
		}
	}
	UGolemEncounterQualityDefinition* Encounter = Asset<UGolemEncounterQualityDefinition>(TEXT("DA_Knight_Encounter_ForgeTrial"),
		TEXT("KnightEncounterQuality"), TEXT("ForgeTrial"));
	UGolemCampaignDefinition* Campaign = Asset<UGolemCampaignDefinition>(TEXT("DA_Knight_Campaign_ForgeTrial"),
		TEXT("KnightCampaign"), TEXT("ForgeTrial"));
	UGolemStoryWorldDefinition* Definition = Asset<UGolemStoryWorldDefinition>(TEXT("DA_Knight_World_ForgeTrial"),
		TEXT("KnightStoryWorld"), TEXT("ForgeTrial"));
	UGolemStoryProfile* Profile = Asset<UGolemStoryProfile>(TEXT("DA_Knight_Profile_ForgeTrial"),
		TEXT("KnightStoryProfile"), TEXT("ForgeTrial"));
	if (Encounter == nullptr || Campaign == nullptr || Definition == nullptr || Profile == nullptr)
	{
		return false;
	}
	Encounter->Wave.Reset();
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Names); ++Index)
	{
		FGolemEncounterPressure& Pressure = Encounter->Wave.AddDefaulted_GetRef();
		Pressure.ArchetypeId = FPrimaryAssetId(TEXT("KnightAIArchetype"), Names[Index]);
		Pressure.Position = FVector(-650.0 + 650.0 * Index, Index == 0 ? 250.0 : 650.0, 100.0);
	}
	Encounter->BossArchetypeId = FPrimaryAssetId(TEXT("KnightAIArchetype"), TEXT("ArenaBoss"));
	Encounter->BossPhaseId = FPrimaryAssetId(TEXT("KnightBossPhase"), TEXT("ArenaBoss"));
	Encounter->StoryAttackBudget = 1;
	Encounter->StandardAttackBudget = 2;
	Encounter->SetDisplayName(NSLOCTEXT("KnightTrial", "Title", "Forge trial — pressure and Runestone counterplay"));
	Campaign->CampaignId = TEXT("Fixture_M75_ForgeTrial");
	Campaign->ScenarioKind = TEXT("DeveloperFixture");
	Campaign->ContentRevision = 1;
	Campaign->ActIds = {TEXT("Trial")};
	Campaign->Quests.Reset();
	FGolemQuestDefinition& Quest = Campaign->Quests.AddDefaulted_GetRef();
	Quest.QuestId = TEXT("ForgeTrial");
	Quest.ActId = TEXT("Trial");
	Quest.Title = Encounter->GetDisplayName();
	FGolemObjectiveDefinition& Goal = Quest.Objectives.AddDefaulted_GetRef();
	Goal.ObjectiveId = TEXT("ClearTrial");
	Goal.Label = NSLOCTEXT("KnightTrial", "Goal", "Defeat the guard, caster and mender, then break the boss armor with any Runestone or heavy strike.");
	Goal.EventName = TEXT("EncounterCompleted");
	Goal.SubjectId = TEXT("ForgeTrial");
	Goal.bEncounterScoped = true;
	Definition->Interactions.Reset();
	FGolemStoryInteraction& Start = Definition->Interactions.AddDefaulted_GetRef();
	Start.AnchorId = TEXT("Trial_Start");
	Start.Label = NSLOCTEXT("KnightTrial", "Start", "START FORGE TRIAL");
	Start.Action = EGolemStoryAction::Encounter;
	Start.SubjectId = TEXT("ForgeTrial");
	FGolemStoryInteraction& Checkpoint = Definition->Interactions.AddDefaulted_GetRef();
	Checkpoint.AnchorId = TEXT("Trial_Checkpoint");
	Checkpoint.Label = NSLOCTEXT("KnightTrial", "Checkpoint", "SANCTUARY — build, save, Story / Standard difficulty in menu");
	Checkpoint.Action = EGolemStoryAction::Checkpoint;
	Checkpoint.SubjectId = TEXT("Trial_Checkpoint");
	for (const bool bNorth : {false, true})
	{
		FGolemStoryInteraction& Lever = Definition->Interactions.AddDefaulted_GetRef();
		Lever.AnchorId = bNorth ? TEXT("Trial_LeverNorth") : TEXT("Trial_LeverSouth");
		Lever.Label = NSLOCTEXT("KnightTrial", "Lever", "OPEN PRACTICE GATE — companions must wait for the lever");
		Lever.Action = EGolemStoryAction::Branch;
		Lever.SubjectId = TEXT("TrialGate");
		Lever.ValueId = TEXT("Open");
		Lever.AllowedChoices = {TEXT("Open")};
	}
	Definition->MapRegions.Reset();
	FGolemMapRegion& Region = Definition->MapRegions.AddDefaulted_GetRef();
	Region.RegionId = TEXT("ForgeTrial");
	Region.Label = Encounter->GetDisplayName();
	Region.Map = TSoftObjectPtr<UWorld>(FSoftObjectPath(TrialMapPackageName));
	Definition->EncounterQualityDefinitionId = Encounter->GetPrimaryAssetId();
	Definition->MapMarkers.Reset();
	for (int32 Index = 0; Index < Definition->Interactions.Num(); ++Index)
	{
		FGolemMapMarker& Marker = Definition->MapMarkers.AddDefaulted_GetRef();
		Marker.MarkerId = Definition->Interactions[Index].AnchorId;
		Marker.AnchorId = Marker.MarkerId;
		Marker.Label = Definition->Interactions[Index].Label;
		Marker.RegionId = Region.RegionId;
		Marker.WorldPosition = Index < 2 ? FVector(Index == 0 ? 0.0 : -700.0, -1000.0, 60.0)
			: FVector(-1550.0, Index == 2 ? -1000.0 : 1700.0, 60.0);
	}
	Profile->CampaignDefinitionId = Campaign->GetPrimaryAssetId();
	Profile->WorldDefinitionId = Definition->GetPrimaryAssetId();
	Profile->bDeveloperFixture = true;
	Profile->DeveloperLevel = 8;
	Profile->bDeveloperStarterEquipment = true;
	Profile->Rewards.Reset();
	Profile->DeveloperBuilds.Reset();
	const TCHAR* Heroes[]{TEXT("Training_Red"), TEXT("Training_Blue"), TEXT("Training_Green")};
	const TCHAR* Specs[]{TEXT("Uranus"), TEXT("Pluto"), TEXT("Terra")};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Heroes); ++Index)
	{
		FGolemSkillBuild Build;
		Build.SpecId = FName(Specs[Index]);
		Profile->DeveloperBuilds.Add(FPrimaryAssetId(TEXT("KnightHero"), Heroes[Index]), Build);
	}
	Profile->DeveloperSeedItems.Reset();
	for (int32 Index = 0; Index < 8; ++Index)
	{
		FGolemGearRewardItem& Item = Profile->DeveloperSeedItems.AddDefaulted_GetRef();
		Item.DefinitionId = FGolemGearCatalog::DefinitionId(Index == 0 ? FName(TEXT("W01"))
			: FName(*FString::Printf(TEXT("A%02d"), Index)));
		Item.Count = 3;
	}
	for (const TCHAR* Name : {TEXT("Aftershock"), TEXT("Convergence"), TEXT("Propagation"), TEXT("Mobile")})
	{
		FGolemGearRewardItem& Item = Profile->DeveloperSeedItems.AddDefaulted_GetRef();
		Item.DefinitionId = FPrimaryAssetId(TEXT("KnightItem"), Name);
		Item.Count = 1;
	}
	return Encounter->ValidateEncounter().IsSuccess() && Campaign->ValidateCampaign().IsSuccess()
		&& Definition->ValidateWorld().IsSuccess() && SaveQualityAsset(Encounter) && SaveQualityAsset(Campaign) && SaveQualityAsset(Definition) && SaveQualityAsset(Profile);
}

bool GolemEncounterQualityAuthoring::PopulateMap(UWorld* World)
{
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (Cube == nullptr)
	{
		return false;
	}
	const auto Geometry = [World, Cube](const FString& Name, const FVector& Position, const FVector& Scale)
	{
		AStaticMeshActor* Actor = Placed<AStaticMeshActor>(World, Name, Position);
		if (Actor == nullptr)
		{
			return false;
		}
		Actor->GetStaticMeshComponent()->SetStaticMesh(Cube);
		Actor->GetStaticMeshComponent()->SetMobility(EComponentMobility::Static);
		Actor->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
		Actor->GetStaticMeshComponent()->SetCanEverAffectNavigation(true);
		Actor->SetActorScale3D(Scale);
		return true;
	};
	// The west route has a physical gate, a single-file raised bridge and step-accessible verticality.
	if (!Geometry(TEXT("KnightTraining_Floor"), FVector(400.0, 0.0, 0.0), FVector(32.0, 40.0, 0.25))
		|| !Geometry(TEXT("KnightTrial_SouthFloor"), FVector(-1525.0, -700.0, 0.0), FVector(6.5, 26.0, 0.25))
		|| !Geometry(TEXT("KnightTrial_NorthFloor"), FVector(-1525.0, 1750.0, 0.0), FVector(6.5, 5.0, 0.25))
		|| !Geometry(TEXT("KnightTrial_WestWall"), FVector(-1850.0, 0.0, 200.0), FVector(1.0, 40.0, 4.0))
		|| !Geometry(TEXT("KnightTrial_EastWallSouth"), FVector(-1250.0, -1675.0, 200.0), FVector(1.0, 6.5, 4.0))
		|| !Geometry(TEXT("KnightTrial_EastWallNorth"), FVector(-1250.0, 475.0, 200.0), FVector(1.0, 30.5, 4.0))
		|| !Geometry(TEXT("KnightTrial_Bridge"), FVector(-1550.0, 1050.0, 90.0), FVector(2.0, 3.0, 0.25))
		|| !Geometry(TEXT("KnightTrial_RailWest"), FVector(-1690.0, 1050.0, 140.0), FVector(0.5, 3.0, 1.0))
		|| !Geometry(TEXT("KnightTrial_RailEast"), FVector(-1410.0, 1050.0, 140.0), FVector(0.5, 3.0, 1.0)))
	{
		return false;
	}
	for (int32 Step = 0; Step < 3; ++Step)
	{
		if (!Geometry(FString::Printf(TEXT("KnightTrial_StepUp%d"), Step), FVector(-1550.0, 650.0 + 100.0 * Step, 27.5 + 30.0 * Step), FVector(2.0, 1.0, 0.3))
			|| !Geometry(FString::Printf(TEXT("KnightTrial_StepDown%d"), Step), FVector(-1550.0, 1250.0 + 100.0 * Step, 87.5 - 30.0 * Step), FVector(2.0, 1.0, 0.3)))
		{
			return false;
		}
	}
	AGolemStoryGate* Gate = Placed<AGolemStoryGate>(World, TEXT("KnightTrial_ConfirmedGate"), FVector(-1550.0, 0.0, 200.0));
	if (Gate == nullptr)
	{
		return false;
	}
	Gate->SetActorScale3D(FVector(6.0, 0.4, 4.0));
	Gate->RequiredBranchId = TEXT("TrialGate");
	const FString WorldPath = QualityRoot + TEXT("DA_Knight_World_ForgeTrial.DA_Knight_World_ForgeTrial");
	UGolemStoryWorldDefinition* Definition = LoadObject<UGolemStoryWorldDefinition>(nullptr, *WorldPath);
	if (Definition == nullptr)
	{
		return false;
	}
	AGolemCampaignEntry* Entry = Placed<AGolemCampaignEntry>(World, TEXT("KnightTrial_Entry"), FVector::ZeroVector);
	if (Entry == nullptr)
	{
		return false;
	}
	Entry->RegionId = TEXT("ForgeTrial");
	Entry->bFrontEndEntry = false;
	Entry->ProfileId = FPrimaryAssetId(TEXT("KnightStoryProfile"), TEXT("ForgeTrial"));
	for (const FGolemMapMarker& Marker : Definition->MapMarkers)
	{
		AGolemStoryAnchor* Anchor = Placed<AGolemStoryAnchor>(World, TEXT("KnightTrial_") + Marker.AnchorId.ToString(), Marker.WorldPosition);
		if (Anchor == nullptr)
		{
			return false;
		}
		Anchor->AnchorId = Marker.AnchorId;
		Anchor->Label = Marker.Label;
		Anchor->SetActorLocation(Marker.WorldPosition);
		Anchor->RefreshPresentation();
	}
	const EGolemRunestoneKind Kinds[]{EGolemRunestoneKind::Healing, EGolemRunestoneKind::Crystal, EGolemRunestoneKind::Frost};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Kinds); ++Index)
	{
		AGolemRunestone* Stone = Placed<AGolemRunestone>(World, FString::Printf(TEXT("KnightTrial_Runestone%d"), Index),
			FVector(-900.0 + 900.0 * Index, 1100.0, 60.0));
		if (Stone == nullptr)
		{
			return false;
		}
		Stone->ConfigureFixture(Kinds[Index], FGuid());
	}
	return Placed<AGolemFrozenSurface>(World, TEXT("KnightTrial_FrozenSurface"), FVector(1200.0, 500.0, 55.0)) != nullptr;
}
