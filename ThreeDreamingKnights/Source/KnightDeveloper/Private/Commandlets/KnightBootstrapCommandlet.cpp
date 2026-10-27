#include "Commandlets/KnightBootstrapCommandlet.h"

#include "Commandlets/KnightCombatAssetAuthoring.h"
#include "Commandlets/KnightAIAssetAuthoring.h"
#include "Commandlets/KnightRPGAssetAuthoring.h"
#include "Commandlets/KnightGearAssetAuthoring.h"
#include "Commandlets/KnightStoryAssetAuthoring.h"
#include "Commandlets/KnightCampaignAssetAuthoring.h"
#include "Commandlets/KnightEncounterQualityAuthoring.h"
#include "Assets/KnightDefinition.h"
#include "Abilities/KnightReviveAbility.h"
#include "Abilities/KnightTestDamageAbility.h"
#include "AbilityYard/KnightAbilityDummy.h"
#include "AbilityYard/KnightAbilityStation.h"
#include "CombatChallenge/KnightCombatFixture.h"
#include "CombatChallenge/KnightTraversalObstacle.h"
#include "Definitions/KnightAbilitySetDefinition.h"
#include "Definitions/KnightHeroDefinition.h"
#include "Definitions/KnightPartyDefinition.h"
#include "Tags/KnightGameplayTags.h"
#include "Tasks/KnightFollowTask.h"
#include "SkillLab/KnightFrozenSurface.h"
#include "SkillLab/KnightRunestone.h"
#include "Definitions/KnightRuneDefinition.h"

#include "ActorFactories/ActorFactory.h"
#include "AssetCompilingManager.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Builders/CubeBuilder.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/StateTreeAIComponentSchema.h"
#include "CrowdManagerBase.h"
#include "Engine/DirectionalLight.h"
#include "Engine/Level.h"
#include "Engine/SkyLight.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/WorldSettings.h"
#include "HAL/FileManager.h"
#include "Materials/MaterialInterface.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "NavigationData.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "StateTree.h"
#include "StateTreeCompiler.h"
#include "StateTreeCompilerLog.h"
#include "StateTreeEditorData.h"
#include "StateTreeEditorModule.h"
#include "StateTreeEditorSchema.h"
#include "StateTreeState.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h"

DEFINE_LOG_CATEGORY_STATIC(LogKnightBootstrap, Log, All);

namespace KnightBootstrap
{
	const TCHAR* MapPackageName = TEXT("/Game/Knight/Maps/Playables/L_M00_Boot");
	const TCHAR* DefinitionPackageName = TEXT("/Game/Knight/Definitions/DA_Knight_Foundation");
	const TCHAR* BootGameModePath = TEXT("/Script/KnightFramework.KnightBootGameMode");
	const TCHAR* TrainingMapPackageName = TEXT("/Game/Knight/Maps/Playables/L_M01_TrainingYard");
	const TCHAR* TrainingGameModePath = TEXT("/Script/ThreeDreamingKnights.KnightGameMode");
	const TCHAR* AbilityMapPackageName = TEXT("/Game/Knight/Maps/Playables/L_M02_AbilityYard");
	const TCHAR* AbilityGameModePath = TEXT("/Script/ThreeDreamingKnights.KnightAbilityYardGameMode");
	const TCHAR* CombatMapPackageName = TEXT("/Game/Knight/Maps/Playables/L_M03_CombatChallenge");
	const TCHAR* CombatGameModePath = TEXT("/Script/ThreeDreamingKnights.KnightCombatChallengeGameMode");
	const TCHAR* ArenaMapPackageName = TEXT("/Game/Knight/Maps/Playables/L_M04_PartyArena");
	const TCHAR* ArenaGameModePath = TEXT("/Script/ThreeDreamingKnights.KnightPartyArenaGameMode");
	const TCHAR* RPGMapPackageName = TEXT("/Game/Knight/Maps/Playables/L_M05_RPGLoop");
	const TCHAR* RPGGameModePath = TEXT("/Script/ThreeDreamingKnights.KnightRPGLoopGameMode");
	const TCHAR* SkillLabMapPackageName = TEXT("/Game/Knight/Maps/Playables/L_M05_5_SkillLab");
	const TCHAR* SkillLabGameModePath = TEXT("/Script/ThreeDreamingKnights.KnightSkillLabGameMode");
	const TCHAR* GearLabMapPackageName = TEXT("/Game/Knight/Maps/Playables/L_M05_6_GearLab");
	const TCHAR* CatalogLabMapPackageName = TEXT("/Game/Knight/Maps/Playables/L_M05_6_CatalogLab");
	const TCHAR* FollowTreePackageName = TEXT("/Game/Knight/AI/ST_Knight_Follow");
	const FName FoundationId(TEXT("Foundation"));
	const FVector FloorScale(30.0, 30.0, 0.25);
	const FVector FloorLocation(0.0, 0.0, -12.5);
	const FVector PlayerStartLocation(0.0, -450.0, 100.0);
	const FRotator PlayerStartRotation(0.0, 90.0, 0.0);
	const FRotator SunRotation(-40.0, -30.0, 0.0);
	constexpr float SunIntensity = 5.0f;
	constexpr float SkyIntensity = 1.0f;
	constexpr double TrainingWallEdge = 1950.0;
	constexpr double TrainingWallHeight = 400.0;
	constexpr float TrainingKillZ = -2000.0f;

	bool SaveAsset(UPackage* Package, UObject* Asset, const FString& Extension)
	{
		const FString Filename = FPackageName::LongPackageNameToFilename(Package->GetName(), Extension);
		if (!IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true))
		{
			UE_LOG(LogKnightBootstrap, Error, TEXT("Could not create output directory for %s."), *Filename);
			return false;
		}

		Asset->SetFlags(RF_Public | RF_Standalone);
		Package->MarkPackageDirty();
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		SaveArgs.SaveFlags = SAVE_NoError;
		SaveArgs.bSlowTask = false;
		const bool bSaved = UPackage::SavePackage(Package, Asset, *Filename, SaveArgs);
		if (!bSaved || IFileManager::Get().FileSize(*Filename) <= 0)
		{
			UE_LOG(LogKnightBootstrap, Error, TEXT("Failed to save valid Unreal package %s."), *Filename);
			return false;
		}

		UE_LOG(LogKnightBootstrap, Display, TEXT("Saved %s (%lld bytes)."),
			*Filename, IFileManager::Get().FileSize(*Filename));
		return true;
	}

	bool CreateDefinition()
	{
		const FString PackageName(DefinitionPackageName);
		const FName AssetName(*FPackageName::GetLongPackageAssetName(PackageName));
		const bool bPackageExists = FPackageName::DoesPackageExist(PackageName);
		UPackage* Package = bPackageExists
			? LoadPackage(nullptr, *PackageName, LOAD_None)
			: CreatePackage(*PackageName);
		if (Package == nullptr)
		{
			UE_LOG(LogKnightBootstrap, Error, TEXT("Could not open definition package %s."), *PackageName);
			return false;
		}

		UObject* ExistingAsset = FindObject<UObject>(Package, *AssetName.ToString());
		UKnightDefinition* Definition = Cast<UKnightDefinition>(ExistingAsset);
		if (ExistingAsset != nullptr && Definition == nullptr)
		{
			UE_LOG(LogKnightBootstrap, Error, TEXT("Existing fixture has an incompatible class: %s."),
				*ExistingAsset->GetPathName());
			return false;
		}
		if (bPackageExists && Definition == nullptr)
		{
			UE_LOG(LogKnightBootstrap, Error, TEXT("Existing definition package has no expected asset."));
			return false;
		}

		if (Definition == nullptr)
		{
			Definition = NewObject<UKnightDefinition>(Package, AssetName, RF_Public | RF_Standalone);
			if (!Definition->TrySetIdentity(FoundationId))
			{
				UE_LOG(LogKnightBootstrap, Error, TEXT("Foundation identity authoring was rejected."));
				return false;
			}
			Definition->SetDisplayName(NSLOCTEXT("KnightFoundation", "FoundationFixture", "Foundation fixture"));
			FAssetRegistryModule::AssetCreated(Definition);
		}

		const FPrimaryAssetId ExpectedId(FName(TEXT("KnightDefinition")), FoundationId);
		if (!Definition->ValidateIdentity().IsSuccess() || Definition->GetPrimaryAssetId() != ExpectedId)
		{
			UE_LOG(LogKnightBootstrap, Error,
				TEXT("Existing fixture identity %s differs from expected %s. Preserving authored data."),
				*Definition->GetPrimaryAssetId().ToString(), *ExpectedId.ToString());
			return false;
		}

		return SaveAsset(Package, Definition, FPackageName::GetAssetPackageExtension());
	}

	template <typename ActorType>
	ActorType* FindOrCreateActor(UWorld* World, const FName ActorName, const FTransform& Transform,
		UClass* ActorClass = ActorType::StaticClass())
	{
		AActor* ExistingActor = FindObject<AActor>(World->PersistentLevel, *ActorName.ToString());
		if (ExistingActor != nullptr)
		{
			ActorType* TypedActor = Cast<ActorType>(ExistingActor);
			if (TypedActor == nullptr || !ExistingActor->IsA(ActorClass))
			{
				UE_LOG(LogKnightBootstrap, Error, TEXT("Actor %s has an incompatible class."), *ActorName.ToString());
				return nullptr;
			}
			return TypedActor;
		}

		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Name = ActorName;
		SpawnParameters.OverrideLevel = World->PersistentLevel;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ActorType* Actor = World->SpawnActor<ActorType>(ActorClass, Transform, SpawnParameters);
		if (Actor == nullptr)
		{
			UE_LOG(LogKnightBootstrap, Error, TEXT("Could not spawn actor %s."), *ActorName.ToString());
			return nullptr;
		}
		Actor->SetActorLabel(ActorName.ToString());
		return Actor;
	}

	template <typename DefinitionType>
	DefinitionType* FindOrCreateDefinition(const FString& PackageName, bool& bOutCreated)
	{
		bOutCreated = false;
		const bool bPackageExists = FPackageName::DoesPackageExist(PackageName);
		UPackage* Package = bPackageExists
			? LoadPackage(nullptr, *PackageName, LOAD_None) : CreatePackage(*PackageName);
		if (Package == nullptr)
		{
			UE_LOG(LogKnightBootstrap, Error, TEXT("Could not open %s."), *PackageName);
			return nullptr;
		}
		const FName AssetName(*FPackageName::GetLongPackageAssetName(PackageName));
		UObject* ExistingAsset = FindObject<UObject>(Package, *AssetName.ToString());
		DefinitionType* Definition = Cast<DefinitionType>(ExistingAsset);
		if ((ExistingAsset != nullptr || bPackageExists) && Definition == nullptr)
		{
			UE_LOG(LogKnightBootstrap, Error, TEXT("Existing fixture has an incompatible asset: %s."), *PackageName);
			return nullptr;
		}
		if (Definition == nullptr)
		{
			Definition = NewObject<DefinitionType>(Package, AssetName, RF_Public | RF_Standalone);
			bOutCreated = true;
			FAssetRegistryModule::AssetCreated(Definition);
		}
		return Definition;
	}

	bool CreateFoundationAbilitySet()
	{
		bool bCreated = false;
		UKnightAbilitySetDefinition* Set = FindOrCreateDefinition<UKnightAbilitySetDefinition>(
			TEXT("/Game/Knight/Definitions/DA_Knight_AbilitySet_Foundation"), bCreated);
		if (Set == nullptr)
		{
			return false;
		}
		if (bCreated)
		{
			FKnightAbilitySetEntry Pulse;
			Pulse.Ability = UKnightTestDamageAbility::StaticClass();
			Pulse.InputTag = KnightGameplayTags::Input_Ability_Test;
			FKnightAbilitySetEntry Revive;
			Revive.Ability = UKnightReviveAbility::StaticClass();
			Revive.InputTag = KnightGameplayTags::Input_Party_Revive;
			FKnightTagRelationship PulseRelationship;
			PulseRelationship.AbilityTag = KnightGameplayTags::Ability_Test_Pulse;
			PulseRelationship.ActivationRequiredTags.AddTag(KnightGameplayTags::State_Life_Alive);
			PulseRelationship.ActivationBlockedTags.AddTag(KnightGameplayTags::State_Life_Downed);
			PulseRelationship.ActivationBlockedTags.AddTag(KnightGameplayTags::State_Life_Dead);
			PulseRelationship.ActivationBlockedTags.AddTag(KnightGameplayTags::State_Life_Reviving);
			FKnightTagRelationship ReviveRelationship;
			ReviveRelationship.AbilityTag = KnightGameplayTags::Ability_Party_Revive;
			ReviveRelationship.ActivationRequiredTags.AddTag(KnightGameplayTags::State_Life_Alive);
			ReviveRelationship.ActivationBlockedTags.AddTag(KnightGameplayTags::State_Life_Downed);
			ReviveRelationship.ActivationBlockedTags.AddTag(KnightGameplayTags::State_Life_Dead);
			ReviveRelationship.AbilityTagsToBlock.AddTag(KnightGameplayTags::Ability_Test_Pulse);
			ReviveRelationship.AbilityTagsToBlock.AddTag(KnightGameplayTags::Ability_Party_Revive);
			ReviveRelationship.AbilityTagsToCancel.AddTag(KnightGameplayTags::Ability_Test_Pulse);
			if (!Set->TrySetIdentity(FoundationId, TEXT("KnightAbilitySet"))
				|| !Set->TrySetAbilities({ Pulse, Revive }).IsSuccess()
				|| !Set->TrySetRelationships({ PulseRelationship, ReviveRelationship }).IsSuccess())
			{
				UE_LOG(LogKnightBootstrap, Error, TEXT("Foundation ability set authoring failed."));
				return false;
			}
			Set->SetDisplayName(NSLOCTEXT("KnightAbilityYard", "FoundationSet", "Foundation abilities"));
		}
		if (Set->GetPrimaryAssetId() != FPrimaryAssetId(TEXT("KnightAbilitySet"), FoundationId)
			|| !Set->ValidateAbilitySet().IsSuccess())
		{
			UE_LOG(LogKnightBootstrap, Error, TEXT("Existing Foundation ability set is invalid; preserving authored data."));
			return false;
		}
		bool bHasPulse = false;
		bool bHasRevive = false;
		for (const FKnightAbilitySetEntry& Entry : Set->GetAbilities())
		{
			bHasPulse |= Entry.Ability == UKnightTestDamageAbility::StaticClass()
				&& Entry.InputTag == KnightGameplayTags::Input_Ability_Test;
			bHasRevive |= Entry.Ability == UKnightReviveAbility::StaticClass()
				&& Entry.InputTag == KnightGameplayTags::Input_Party_Revive;
		}
		if (!bHasPulse || !bHasRevive)
		{
			UE_LOG(LogKnightBootstrap, Error, TEXT("Foundation ability set lacks the mapped pulse or revive grant."));
			return false;
		}
		return SaveAsset(Set->GetPackage(), Set, FPackageName::GetAssetPackageExtension());
	}

	bool CreatePartyDefinitions()
	{
		const UMaterialInterface* BodyMaterial = LoadObject<UMaterialInterface>(nullptr,
			TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
		FLinearColor DefaultBodyColor;
		if (BodyMaterial == nullptr
			|| !BodyMaterial->GetVectorParameterValue(FName(TEXT("Color")), DefaultBodyColor))
		{
			UE_LOG(LogKnightBootstrap, Error, TEXT("Graybox body material does not expose the Color vector parameter."));
			return false;
		}
		UE_LOG(LogKnightBootstrap, Display, TEXT("Graybox body material Color parameter validated: %s."),
			*DefaultBodyColor.ToString());
		const TArray<FName> HeroNames = { TEXT("Training_Red"), TEXT("Training_Blue"), TEXT("Training_Green") };
		const TArray<FLinearColor> HeroColors =
		{
			FLinearColor(0.9f, 0.1f, 0.08f), FLinearColor(0.08f, 0.25f, 0.95f), FLinearColor(0.1f, 0.85f, 0.2f)
		};
		const TArray<FText> HeroDisplayNames =
		{
			NSLOCTEXT("KnightTraining", "RedKnight", "Red Knight"),
			NSLOCTEXT("KnightTraining", "BlueKnight", "Blue Knight"),
			NSLOCTEXT("KnightTraining", "GreenKnight", "Green Knight")
		};
		const TArray<FVector> FormationOffsets =
		{
			FVector(-280.0, -180.0, 0.0), FVector(-280.0, 180.0, 0.0), FVector(-280.0, -350.0, 0.0)
		};
		TArray<FPrimaryAssetId> HeroIds;
		for (int32 Index = 0; Index < HeroNames.Num(); ++Index)
		{
			const FString HeroPackage = FString::Printf(TEXT("/Game/Knight/Definitions/DA_Knight_%s"),
				*HeroNames[Index].ToString());
			bool bCreated = false;
			UKnightHeroDefinition* Hero = FindOrCreateDefinition<UKnightHeroDefinition>(HeroPackage, bCreated);
			if (Hero == nullptr)
			{
				return false;
			}
			if (bCreated)
			{
				if (!Hero->TrySetIdentity(HeroNames[Index], TEXT("KnightHero"))
					|| !Hero->TrySetPresentation(HeroColors[Index], 450.0f, 700.0f,
						FormationOffsets[Index], 120.0f).IsSuccess())
				{
					UE_LOG(LogKnightBootstrap, Error, TEXT("Training hero definition authoring failed."));
					return false;
				}
				Hero->SetDisplayName(HeroDisplayNames[Index]);
			}
			const FPrimaryAssetId ExpectedId(TEXT("KnightHero"), HeroNames[Index]);
			if (Hero->GetPrimaryAssetId() != ExpectedId || !Hero->ValidateHero().IsSuccess()
				|| !SaveAsset(Hero->GetPackage(), Hero, FPackageName::GetAssetPackageExtension()))
			{
				UE_LOG(LogKnightBootstrap, Error, TEXT("Training hero identity/save validation failed: %s."),
					*HeroPackage);
				return false;
			}
			HeroIds.Add(ExpectedId);
		}

		bool bCreated = false;
		UKnightPartyDefinition* Party = FindOrCreateDefinition<UKnightPartyDefinition>(
			TEXT("/Game/Knight/Definitions/DA_Knight_TrainingParty"), bCreated);
		if (Party == nullptr)
		{
			return false;
		}
		if (bCreated)
		{
			if (!Party->TrySetIdentity(TEXT("TrainingParty"), TEXT("KnightParty"))
				|| !Party->TrySetHeroDefinitions(HeroIds).IsSuccess())
			{
				UE_LOG(LogKnightBootstrap, Error, TEXT("Training party definition authoring failed."));
				return false;
			}
			Party->SetDisplayName(NSLOCTEXT("KnightFoundation", "TrainingParty", "Training party"));
		}
		if (Party->GetPrimaryAssetId() != FPrimaryAssetId(TEXT("KnightParty"), TEXT("TrainingParty"))
			|| !Party->ValidateParty().IsSuccess() || Party->GetHeroDefinitions() != HeroIds)
		{
			UE_LOG(LogKnightBootstrap, Error, TEXT("Existing training party has an incompatible identity."));
			return false;
		}
		return SaveAsset(Party->GetPackage(), Party, FPackageName::GetAssetPackageExtension());
	}

	bool CreateFollowTree()
	{
		bool bCreated = false;
		UStateTree* Tree = FindOrCreateDefinition<UStateTree>(FollowTreePackageName, bCreated);
		if (Tree == nullptr)
		{
			return false;
		}
		if (bCreated)
		{
			FStateTreeEditorModule& EditorModule = FStateTreeEditorModule::GetModule();
			const TNonNullSubclassOf<UStateTreeEditorData> EditorDataClass =
				EditorModule.GetEditorDataClass(UStateTreeAIComponentSchema::StaticClass());
			UStateTreeEditorData* EditorData = NewObject<UStateTreeEditorData>(
				Tree, EditorDataClass, NAME_None, RF_Transactional);
			Tree->EditorData = EditorData;
			EditorData->Schema = NewObject<UStateTreeAIComponentSchema>(EditorData, NAME_None, RF_Transactional);
			const TNonNullSubclassOf<UStateTreeEditorSchema> EditorSchemaClass =
				EditorModule.GetEditorSchemaClass(UStateTreeAIComponentSchema::StaticClass());
			EditorData->EditorSchema = NewObject<UStateTreeEditorSchema>(
				EditorData, EditorSchemaClass, NAME_None, RF_Transactional);
			UStateTreeState& Root = EditorData->AddRootState();
			Root.Name = TEXT("FollowFormation");
			Root.AddTask<FKnightFollowTask>().SetNodeName(FName(TEXT("FollowHumanKnight")));
		}
		const UStateTreeEditorData* EditorData = Cast<UStateTreeEditorData>(Tree->EditorData);
		if (EditorData == nullptr || EditorData->Schema == nullptr
			|| !EditorData->Schema->IsA<UStateTreeAIComponentSchema>())
		{
			UE_LOG(LogKnightBootstrap, Error, TEXT("Follow StateTree has an incompatible schema."));
			return false;
		}
		bool bHasFollowTask = false;
		EditorData->VisitHierarchy([&bHasFollowTask](UStateTreeState& State, UStateTreeState* ParentState)
		{
			for (const FStateTreeEditorNode& Task : State.Tasks)
			{
				bHasFollowTask |= Task.Node.GetScriptStruct() == FKnightFollowTask::StaticStruct();
			}
			return EStateTreeVisitor::Continue;
		});
		if (!bHasFollowTask)
		{
			UE_LOG(LogKnightBootstrap, Error, TEXT("Existing follow StateTree lacks the native formation task."));
			return false;
		}
		FStateTreeCompilerLog CompilerLog;
		FStateTreeCompiler Compiler(CompilerLog);
		const bool bCompiled = Compiler.Compile(*Tree);
		CompilerLog.DumpToLog(Tree, LogKnightBootstrap);
		if (!bCompiled || !Tree->IsReadyToRun())
		{
			UE_LOG(LogKnightBootstrap, Error, TEXT("Follow StateTree compilation/ready validation failed."));
			return false;
		}
		return SaveAsset(Tree->GetPackage(), Tree, FPackageName::GetAssetPackageExtension());
	}

	bool CreateTrainingNavigation(UWorld* World)
	{
		ANavMeshBoundsVolume* Bounds = FindOrCreateActor<ANavMeshBoundsVolume>(World,
			FName(TEXT("KnightTraining_NavBounds")), FTransform(FVector(0.0, 0.0, 200.0)));
		if (Bounds == nullptr)
		{
			return false;
		}
		if (Bounds->Brush == nullptr)
		{
			UCubeBuilder* Builder = NewObject<UCubeBuilder>();
			Builder->X = 3900.0f;
			Builder->Y = 3900.0f;
			Builder->Z = 600.0f;
			UActorFactory::CreateBrushForVolumeActor(Bounds, Builder);
		}
		const FBox BoundsBox = Bounds->GetComponentsBoundingBox(true);
		if (Bounds->Brush == nullptr || !BoundsBox.IsValid || BoundsBox.GetSize().GetMin() <= 0.0)
		{
			UE_LOG(LogKnightBootstrap, Error,
				TEXT("Training navigation bounds have no valid brush geometry: model %s, registered %s, box %s."),
				Bounds->Brush != nullptr ? TEXT("present") : TEXT("missing"),
				Bounds->HasActorRegisteredAllComponents() ? TEXT("true") : TEXT("false"), *BoundsBox.ToString());
			return false;
		}
		UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
		if (Navigation == nullptr)
		{
			UE_LOG(LogKnightBootstrap, Error, TEXT("Training world navigation system was not initialized."));
			return false;
		}
		Navigation->OnNavigationBoundsUpdated(Bounds);
		FlushAsyncLoading();
		FAssetCompilingManager::Get().FinishAllCompilation();
		Navigation->OnWorldInitDone(FNavigationSystemRunMode::EditorMode);
		if (FAssetCompilingManager::Get().GetNumRemainingAssets() != 0)
		{
			UE_LOG(LogKnightBootstrap, Error, TEXT("Training navigation assets have not finished compilation."));
			return false;
		}
		// Commandlets do not run the editor's delayed unlock ticker after synchronous asset compilation.
		Navigation->RemoveNavigationBuildLock(ENavigationBuildLock::AsyncLoadLock,
			UNavigationSystemV1::ELockRemovalRebuildAction::NoRebuild);
		Navigation->Build();
		ANavigationData* NavigationData = Navigation->GetDefaultNavDataInstance(FNavigationSystem::DontCreate);
		if (NavigationData == nullptr)
		{
			UE_LOG(LogKnightBootstrap, Error, TEXT("Training navigation build produced no navigation data."));
			return false;
		}
		NavigationData->EnsureBuildCompletion();
		FNavLocation ProjectedLocation;
		if (!Navigation->ProjectPointToNavigation(FVector(0.0, -900.0, 50.0), ProjectedLocation,
			FVector(100.0, 100.0, 200.0)))
		{
			UE_LOG(LogKnightBootstrap, Error, TEXT("Training navmesh cannot project the party spawn point."));
			return false;
		}
		UE_LOG(LogKnightBootstrap, Display, TEXT("Training navigation compiled: spawn projects to %s."),
			*ProjectedLocation.Location.ToString());
		return true;
	}

	bool CreateTrainingBoundary(UWorld* World, UStaticMesh* WallMesh)
	{
		const TArray<FName> WallNames =
		{
			TEXT("KnightTraining_WallEast"), TEXT("KnightTraining_WallWest"),
			TEXT("KnightTraining_WallNorth"), TEXT("KnightTraining_WallSouth")
		};
		const TArray<FVector> WallLocations =
		{
			FVector(TrainingWallEdge, 0.0, TrainingWallHeight * 0.5),
			FVector(-TrainingWallEdge, 0.0, TrainingWallHeight * 0.5),
			FVector(0.0, TrainingWallEdge, TrainingWallHeight * 0.5),
			FVector(0.0, -TrainingWallEdge, TrainingWallHeight * 0.5)
		};
		const TArray<FVector> WallScales =
		{
			FVector(1.0, 40.0, 4.0), FVector(1.0, 40.0, 4.0),
			FVector(40.0, 1.0, 4.0), FVector(40.0, 1.0, 4.0)
		};
		for (int32 Index = 0; Index < WallNames.Num(); ++Index)
		{
			const bool bCreateWall = FindObject<AActor>(World->PersistentLevel, *WallNames[Index].ToString()) == nullptr;
			AStaticMeshActor* Wall = FindOrCreateActor<AStaticMeshActor>(World, WallNames[Index],
				FTransform(FQuat::Identity, WallLocations[Index], WallScales[Index]));
			if (Wall == nullptr)
			{
				return false;
			}
			UStaticMeshComponent* MeshComponent = Wall->GetStaticMeshComponent();
			if (bCreateWall)
			{
				MeshComponent->SetStaticMesh(WallMesh);
				MeshComponent->SetCollisionProfileName(FName(TEXT("BlockAll")));
				Wall->SetMobility(EComponentMobility::Static);
			}
			const FBox WallBounds = Wall->GetComponentsBoundingBox(true);
			if (MeshComponent->GetStaticMesh() == nullptr
				|| MeshComponent->GetCollisionEnabled() != ECollisionEnabled::QueryAndPhysics
				|| MeshComponent->GetCollisionResponseToChannel(ECC_Pawn) != ECR_Block
				|| !WallBounds.IsValid || WallBounds.GetSize().Z < TrainingWallHeight - KINDA_SMALL_NUMBER)
			{
				UE_LOG(LogKnightBootstrap, Error,
					TEXT("Training wall %s requires a 400 cm blocking physical mesh; preserving existing authored data."),
					*WallNames[Index].ToString());
				return false;
			}
		}
		UE_LOG(LogKnightBootstrap, Display, TEXT("Training perimeter validated: four physical 400 cm walls."));
		return true;
	}

	bool CreateTrainingMarkers(UWorld* World)
	{
		UClass* MarkerClass = LoadClass<AActor>(nullptr, TEXT("/Script/ThreeDreamingKnights.KnightTrainingMarker"));
		FIntProperty* IndexProperty = MarkerClass != nullptr
			? FindFProperty<FIntProperty>(MarkerClass, TEXT("MarkerIndex")) : nullptr;
		FStructProperty* ColorProperty = MarkerClass != nullptr
			? FindFProperty<FStructProperty>(MarkerClass, TEXT("MarkerColor")) : nullptr;
		if (IndexProperty == nullptr || ColorProperty == nullptr
			|| ColorProperty->Struct != TBaseStructure<FLinearColor>::Get())
		{
			UE_LOG(LogKnightBootstrap, Error, TEXT("Native training marker class/properties are unavailable."));
			return false;
		}
		const TArray<FVector> Locations =
		{
			FVector(450.0, -250.0, 100.0), FVector(800.0, 300.0, 100.0), FVector(1100.0, -150.0, 100.0)
		};
		const TArray<FLinearColor> Colors = { FLinearColor::Red, FLinearColor::Blue, FLinearColor::Green };
		for (int32 Index = 0; Index < Locations.Num(); ++Index)
		{
			const FName MarkerName(*FString::Printf(TEXT("KnightTraining_Marker%d"), Index));
			AActor* Marker = FindOrCreateActor<AActor>(World, MarkerName, FTransform(Locations[Index]), MarkerClass);
			if (Marker == nullptr)
			{
				return false;
			}
			IndexProperty->SetPropertyValue_InContainer(Marker, Index);
			*ColorProperty->ContainerPtrToValuePtr<FLinearColor>(Marker) = Colors[Index];
			Marker->RerunConstructionScripts();
		}
		return true;
	}

	bool CreateAbilityFixtures(UWorld* World)
	{
		AKnightAbilityDummy* Dummy = FindOrCreateActor<AKnightAbilityDummy>(World,
			FName(TEXT("KnightAbility_Dummy")), FTransform(FVector(400.0, -1000.0, 100.0)));
		const TArray<FName> StationNames = { TEXT("KnightAbility_DamageStation"), TEXT("KnightAbility_ReviveStation") };
		const TArray<FVector> Locations = { FVector(400.0, -450.0, 50.0), FVector(900.0, 350.0, 50.0) };
		const TArray<EKnightAbilityStationKind> Kinds =
		{
			EKnightAbilityStationKind::DamagePlayer, EKnightAbilityStationKind::DownCompanion
		};
		if (Dummy == nullptr)
		{
			return false;
		}
		for (int32 Index = 0; Index < StationNames.Num(); ++Index)
		{
			const bool bCreated = FindObject<AActor>(World->PersistentLevel, *StationNames[Index].ToString()) == nullptr;
			AKnightAbilityStation* Station = FindOrCreateActor<AKnightAbilityStation>(World,
				StationNames[Index], FTransform(Locations[Index]));
			if (Station == nullptr)
			{
				return false;
			}
			if (bCreated)
			{
				Station->SetStationKind(Kinds[Index]);
			}
			if (Station->GetStationKind() != Kinds[Index])
			{
				UE_LOG(LogKnightBootstrap, Error, TEXT("Ability station kind differs from its expected fixture."));
				return false;
			}
		}
		return true;
	}

	bool CreateCombatFixtures(UWorld* World)
	{
		const TArray<FName> Names =
		{
			TEXT("KnightCombat_MeleeTarget"), TEXT("KnightCombat_ScriptedAttacker"), TEXT("KnightCombat_RuneTarget")
		};
		const TArray<FVector> Locations =
		{
			FVector(-1050.0, -300.0, 100.0), FVector(-900.0, 650.0, 100.0), FVector(900.0, 1000.0, 100.0)
		};
		const TArray<EKnightCombatFixtureKind> Kinds =
		{
			EKnightCombatFixtureKind::MeleeTarget, EKnightCombatFixtureKind::ScriptedAttacker,
			EKnightCombatFixtureKind::RuneTarget
		};
		for (int32 Index = 0; Index < Names.Num(); ++Index)
		{
			const bool bCreated = FindObject<AActor>(World->PersistentLevel, *Names[Index].ToString()) == nullptr;
			AKnightCombatFixture* Fixture = FindOrCreateActor<AKnightCombatFixture>(World, Names[Index],
				FTransform(FRotator(0.0, 180.0, 0.0), Locations[Index]));
			if (Fixture == nullptr)
			{
				return false;
			}
			if (bCreated)
			{
				Fixture->SetFixtureKind(Kinds[Index]);
			}
			if (Fixture->GetFixtureKind() != Kinds[Index])
			{
				UE_LOG(LogKnightBootstrap, Error, TEXT("Existing combat fixture has another role; preserving authored data."));
				return false;
			}
		}
		AKnightTraversalObstacle* Obstacle = FindOrCreateActor<AKnightTraversalObstacle>(World,
			TEXT("KnightCombat_TraversalObstacle"), FTransform(FVector(-1050.0, 1200.0, 45.0)));
		if (Obstacle == nullptr || Obstacle->GetObstacleMesh()->GetCollisionResponseToChannel(ECC_Pawn) != ECR_Block)
		{
			return false;
		}
		const FBox Bounds = Obstacle->GetObstacleMesh()->Bounds.GetBox();
		if (!Bounds.IsValid || Bounds.GetSize().Z < 80.0 || Bounds.GetSize().Z > 130.0
			|| Bounds.GetSize().X > 220.0)
		{
			UE_LOG(LogKnightBootstrap, Error, TEXT("Traversal fixture must retain a valid low physical hurdle."));
			return false;
		}
		return true;
	}

	bool CreateArenaFixtures(UWorld* World, UStaticMesh* ObstacleMesh)
	{
		UClass* MarkerClass = LoadClass<AActor>(nullptr,
			TEXT("/Script/ThreeDreamingKnights.KnightPartyArenaStartMarker"));
		if (MarkerClass == nullptr)
		{
			UE_LOG(LogKnightBootstrap, Error, TEXT("Native arena Start marker is unavailable."));
			return false;
		}
		AActor* StartMarker = FindOrCreateActor<AActor>(World, TEXT("KnightArena_Start"),
			FTransform(FVector(0.0, -650.0, 20.0)), MarkerClass);
		const bool bCreateObstacle = FindObject<AActor>(World->PersistentLevel, TEXT("KnightArena_LOSBlocker"))
			== nullptr;
		AStaticMeshActor* Obstacle = FindOrCreateActor<AStaticMeshActor>(World, TEXT("KnightArena_LOSBlocker"),
			FTransform(FQuat::Identity, FVector(0.0, 0.0, 150.0), FVector(3.0, 2.0, 3.0)));
		if (StartMarker == nullptr || Obstacle == nullptr)
		{
			return false;
		}
		UStaticMeshComponent* Mesh = Obstacle->GetStaticMeshComponent();
		if (bCreateObstacle)
		{
			Mesh->SetStaticMesh(ObstacleMesh);
			Mesh->SetCollisionProfileName(TEXT("BlockAll"));
			Obstacle->SetMobility(EComponentMobility::Static);
		}
		const FBox Bounds = Obstacle->GetComponentsBoundingBox(true);
		if (Mesh->GetStaticMesh() == nullptr || Mesh->GetCollisionEnabled() != ECollisionEnabled::QueryAndPhysics
			|| Mesh->GetCollisionResponseToChannel(ECC_Pawn) != ECR_Block
			|| Mesh->GetCollisionResponseToChannel(ECC_Visibility) != ECR_Block
			|| !Bounds.IsValid || Bounds.GetSize().Z < 250.0)
		{
			UE_LOG(LogKnightBootstrap, Error, TEXT("Arena LOS blocker needs genuine blocking physical geometry."));
			return false;
		}
		return true;
	}

	bool ValidateArenaNavigation(UWorld* World)
	{
		UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
		if (Navigation == nullptr)
		{
			return false;
		}
		const FVector Start(0.0, -650.0, 50.0);
		const TArray<FVector> EncounterAnchors =
		{
			FVector(-600.0, 250.0, 50.0), FVector(600.0, 250.0, 50.0), FVector(0.0, 850.0, 50.0)
		};
		for (const FVector& Anchor : EncounterAnchors)
		{
			FNavLocation Projected;
			if (!Navigation->ProjectPointToNavigation(Anchor, Projected, FVector(60.0, 60.0, 150.0)))
			{
				UE_LOG(LogKnightBootstrap, Error, TEXT("Arena spawn anchor has no real navigation: %s."), *Anchor.ToString());
				return false;
			}
			UNavigationPath* Path = Navigation->FindPathToLocationSynchronously(World, Start, Projected.Location);
			if (Path == nullptr || !Path->IsValid() || Path->IsPartial() || Path->PathPoints.Num() < 2)
			{
				UE_LOG(LogKnightBootstrap, Error, TEXT("Arena encounter anchor is unreachable from Start: %s."),
					*Anchor.ToString());
				return false;
			}
		}
		FHitResult Obstruction;
		if (!World->LineTraceSingleByChannel(Obstruction, FVector(0.0, -500.0, 100.0),
			FVector(0.0, 500.0, 100.0), ECC_Visibility)
			|| Obstruction.GetActor() == nullptr
			|| Obstruction.GetActor()->GetFName() != FName(TEXT("KnightArena_LOSBlocker")))
		{
			UE_LOG(LogKnightBootstrap, Error, TEXT("Arena blocker does not obstruct the real center LOS trace."));
			return false;
		}
		UE_LOG(LogKnightBootstrap, Display, TEXT("Arena navigation: all three encounter anchors reachable; center LOS blocked."));
		return true;
	}

	bool CreateRPGFixtures(UWorld* World)
	{
		UClass* StationClass = LoadClass<AActor>(nullptr, TEXT("/Script/ThreeDreamingKnights.KnightRPGUpgradeStation"));
		if (StationClass == nullptr)
		{
			UE_LOG(LogKnightBootstrap, Error, TEXT("Native RPG upgrade station is unavailable."));
			return false;
		}
		AActor* Station = FindOrCreateActor<AActor>(World, TEXT("KnightRPG_UpgradeStation"),
			FTransform(FVector(-1500.0, -1000.0, 20.0)), StationClass);
		return Station != nullptr;
	}

	bool CreateMap(const int32 Milestone)
	{
		const bool bQuality = Milestone == 75;
		const bool bCampaign = Milestone == 7 || Milestone == 70;
		const bool bStory = Milestone == 6 || (Milestone >= 60 && Milestone <= 63) || bCampaign || bQuality;
		const bool bGearLab = Milestone == 56 || Milestone == 57;
		const bool bSkillLab = Milestone == 55 || bGearLab;
		const bool bTraining = Milestone > 0;
		const bool bAbilityYard = Milestone == 2 || Milestone == 3;
		const bool bCombatChallenge = Milestone == 3;
		const bool bRPG = Milestone == 5 || bSkillLab || bStory;
		const bool bArena = Milestone >= 4;
		const TCHAR* GameModePath = bQuality ? TEXT("/Script/ThreeDreamingKnights.KnightEncounterQualityGameMode")
			: bCampaign ? TEXT("/Script/ThreeDreamingKnights.KnightCampaignGameMode")
			: bStory ? TEXT("/Script/ThreeDreamingKnights.KnightStoryGameMode") : bGearLab ? (Milestone == 57
			? TEXT("/Script/ThreeDreamingKnights.KnightGearCatalogGameMode")
			: TEXT("/Script/ThreeDreamingKnights.KnightGearLabGameMode"))
			: (bSkillLab ? SkillLabGameModePath : (bRPG ? RPGGameModePath : (bArena ? ArenaGameModePath
			: (bCombatChallenge ? CombatGameModePath : (bAbilityYard ? AbilityGameModePath
				: (bTraining ? TrainingGameModePath : BootGameModePath))))));
		UClass* BootGameMode = LoadClass<AGameModeBase>(nullptr, GameModePath);
		UStaticMesh* FloorMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
		if (BootGameMode == nullptr || FloorMesh == nullptr)
		{
			UE_LOG(LogKnightBootstrap, Error, TEXT("Boot GameMode or engine cube mesh could not be loaded."));
			return false;
		}

		const FString PackageName(bQuality ? TEXT("/Game/Knight/Maps/Playables/L_M07_5_ForgeTrial") : bCampaign ? (Milestone == 70
			? TEXT("/Game/Knight/Maps/Playables/L_M07_Courtyard") : TEXT("/Game/Knight/Maps/Playables/L_M07_Campaign")) : bStory ? (Milestone == 60
			? TEXT("/Game/Knight/Maps/Playables/L_M06_StoryCourtyard") : Milestone == 63
			? TEXT("/Game/Knight/Maps/Playables/L_M06_StoryV1") : Milestone == 61
			? TEXT("/Game/Knight/Maps/Playables/L_M06_CaravanFixture") : Milestone == 62
			? TEXT("/Game/Knight/Maps/Playables/L_M06_LifecycleFixture")
			: TEXT("/Game/Knight/Maps/Playables/L_M06_StoryFrameworkDemo")) : bGearLab ? (Milestone == 57 ? CatalogLabMapPackageName : GearLabMapPackageName)
			: (bSkillLab ? SkillLabMapPackageName : (bRPG ? RPGMapPackageName : (bArena ? ArenaMapPackageName
			: (bCombatChallenge ? CombatMapPackageName : (bAbilityYard ? AbilityMapPackageName
				: (bTraining ? TrainingMapPackageName : MapPackageName)))))));
		const bool bPackageExists = FPackageName::DoesPackageExist(PackageName);
		UPackage* Package = bPackageExists
			? LoadPackage(nullptr, *PackageName, LOAD_None)
			: CreatePackage(*PackageName);
		if (Package == nullptr)
		{
			UE_LOG(LogKnightBootstrap, Error, TEXT("Could not open boot map package."));
			return false;
		}

		UWorld* World = UWorld::FindWorldInPackage(Package);
		const bool bOwnWorldInitialization = World == nullptr || !World->IsInitialized();
		if (bPackageExists && World == nullptr)
		{
			UE_LOG(LogKnightBootstrap, Error, TEXT("Existing boot map package does not contain a world."));
			return false;
		}
		const UWorld::InitializationValues Initialization = UWorld::InitializationValues()
			.AllowAudioPlayback(false).CreatePhysicsScene(true).ShouldSimulatePhysics(false)
			.EnableTraceCollision(true).CreateNavigation(bTraining).CreateAISystem(false).CreateFXSystem(false);
		if (World == nullptr)
		{
			const FName WorldName(*FPackageName::GetLongPackageAssetName(PackageName));
			World = UWorld::CreateWorld(EWorldType::Editor, false, WorldName, Package, true,
				ERHIFeatureLevel::Num, &Initialization);
			if (World == nullptr)
			{
				UE_LOG(LogKnightBootstrap, Error, TEXT("Unreal could not create the boot world."));
				return false;
			}
			World->SetFlags(RF_Public | RF_Standalone);
			FAssetRegistryModule::AssetCreated(World);
		}
		else if (!World->IsInitialized())
		{
			World->WorldType = EWorldType::Editor;
			World->InitWorld(Initialization);
		}
		ON_SCOPE_EXIT
		{
			if (bOwnWorldInitialization && World != GWorld)
			{
				if (UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World))
				{
					// Release the transient crowd manager before the last navmesh unregisters during teardown.
					if (UCrowdManagerBase* Crowd = Navigation->GetCrowdManager())
					{
						// This manager belongs only to the authored world, whose final teardown starts here.
						// Invalidating its weak navigation reference prevents a search for replacement nav data.
						Crowd->RemoveFromRoot();
						Crowd->MarkAsGarbage();
					}
				}
				World->DestroyWorld(false);
			}
		};

		AWorldSettings* WorldSettings = World->GetWorldSettings();
		if (WorldSettings == nullptr || World->PersistentLevel == nullptr)
		{
			UE_LOG(LogKnightBootstrap, Error, TEXT("Boot world lacks required level or world settings."));
			return false;
		}
		if (bPackageExists && WorldSettings->DefaultGameMode != nullptr
			&& WorldSettings->DefaultGameMode.Get() != BootGameMode)
		{
			UE_LOG(LogKnightBootstrap, Error, TEXT("Existing boot map has another GameMode; preserving it."));
			return false;
		}
		WorldSettings->DefaultGameMode = BootGameMode;
		if (bTraining && bOwnWorldInitialization)
		{
			WorldSettings->KillZ = TrainingKillZ;
		}
		// InitWorld prepares a loaded world's systems but does not register its saved actor components.
		World->UpdateWorldComponents(false, false);

		const FString ActorPrefix(bTraining ? TEXT("KnightTraining") : TEXT("KnightBoot"));
		AStaticMeshActor* Floor = FindOrCreateActor<AStaticMeshActor>(World, FName(*(ActorPrefix + TEXT("_Floor"))),
			FTransform(FQuat::Identity, FloorLocation, bTraining ? FVector(40.0, 40.0, 0.25) : FloorScale));
		APlayerStart* PlayerStart = FindOrCreateActor<APlayerStart>(World,
			FName(*(ActorPrefix + TEXT("_PlayerStart"))),
			FTransform(PlayerStartRotation, bTraining ? FVector(0.0, -900.0, 100.0) : PlayerStartLocation));
		ADirectionalLight* Sun = FindOrCreateActor<ADirectionalLight>(World, FName(*(ActorPrefix + TEXT("_Sun"))),
			FTransform(SunRotation, FVector(0.0, 0.0, 500.0)));
		ASkyLight* SkyLight = FindOrCreateActor<ASkyLight>(World, FName(*(ActorPrefix + TEXT("_SkyLight"))),
			FTransform(FVector(0.0, 0.0, 500.0)));
		ASkyAtmosphere* SkyAtmosphere = FindOrCreateActor<ASkyAtmosphere>(World,
			FName(*(ActorPrefix + TEXT("_SkyAtmosphere"))), FTransform::Identity);
		if (Floor == nullptr || PlayerStart == nullptr || Sun == nullptr
			|| SkyLight == nullptr || SkyAtmosphere == nullptr)
		{
			return false;
		}

		Floor->GetStaticMeshComponent()->SetStaticMesh(FloorMesh);
		Floor->GetStaticMeshComponent()->SetCollisionProfileName(FName(TEXT("BlockAll")));
		Floor->SetMobility(EComponentMobility::Static);
		UDirectionalLightComponent* SunComponent = Cast<UDirectionalLightComponent>(Sun->GetLightComponent());
		if (SunComponent == nullptr)
		{
			UE_LOG(LogKnightBootstrap, Error, TEXT("Boot directional light lacks a directional component."));
			return false;
		}
		SunComponent->SetMobility(EComponentMobility::Movable);
		SunComponent->SetIntensity(SunIntensity);
		SunComponent->SetAtmosphereSunLight(true);
		SkyLight->GetLightComponent()->SetMobility(EComponentMobility::Movable);
		SkyLight->GetLightComponent()->SetIntensity(SkyIntensity);
		SkyLight->GetLightComponent()->bLowerHemisphereIsBlack = false;
		if (bTraining && (!CreateTrainingBoundary(World, FloorMesh) || !CreateTrainingMarkers(World)))
		{
			return false;
		}
		if (bAbilityYard && !CreateAbilityFixtures(World))
		{
			return false;
		}
		if (bCombatChallenge && !CreateCombatFixtures(World))
		{
			return false;
		}
		if (bArena && !CreateArenaFixtures(World, FloorMesh))
		{
			return false;
		}
		if (bRPG && !CreateRPGFixtures(World))
		{
			return false;
		}
		if (bCampaign && !KnightCampaignAssetAuthoring::PopulateMap(World, Milestone == 70))
		{
			return false;
		}
		if (bQuality && !KnightEncounterQualityAuthoring::PopulateMap(World))
		{
			return false;
		}
		if (bStory && !bCampaign && !bQuality && !KnightStoryAssetAuthoring::PopulateStoryMap(World, Milestone))
		{
			return false;
		}
		if (bSkillLab)
		{
			if (FindOrCreateActor<AKnightAbilityDummy>(World, TEXT("KnightSkillLab_CombatDummy"),
				FTransform(FVector(-1200.0, 0.0, 100.0))) == nullptr)
			{
				return false;
			}
			const TArray<EKnightRunestoneKind> Kinds{EKnightRunestoneKind::Healing,
				EKnightRunestoneKind::Crystal, EKnightRunestoneKind::Frost};
			for (int32 Index = 0; Index < Kinds.Num(); ++Index)
			{
				AKnightRunestone* Stone = FindOrCreateActor<AKnightRunestone>(World,
					FName(*FString::Printf(TEXT("KnightSkillLab_Runestone%d"), Index)),
					FTransform(FVector(-900.0 + 900.0 * Index, 1100.0, 60.0)));
				if (Stone == nullptr)
				{
					return false;
				}
				Stone->ConfigureFixture(Kinds[Index], FGuid());
			}
			if (FindOrCreateActor<AKnightFrozenSurface>(World, TEXT("KnightSkillLab_FrozenSurface"),
				FTransform(FVector(1200.0, 500.0, 55.0))) == nullptr)
			{
				return false;
			}
		}
		if (bTraining && !CreateTrainingNavigation(World))
		{
			return false;
		}
		if (bArena && !ValidateArenaNavigation(World))
		{
			return false;
		}
		if (bQuality)
		{
			UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
			ANavigationData* Data = Navigation != nullptr ? Navigation->GetDefaultNavDataInstance(FNavigationSystem::DontCreate) : nullptr;
			FProperty* Generation = FindFProperty<FProperty>(ANavigationData::StaticClass(), TEXT("RuntimeGeneration"));
			if (Data == nullptr || Generation == nullptr || Generation->ImportText_Direct(TEXT("Dynamic"),
				Generation->ContainerPtrToValuePtr<void>(Data), Data, PPF_None) == nullptr
				|| Data->GetRuntimeGenerationMode() != ERuntimeGenerationType::Dynamic)
			{
				UE_LOG(LogKnightBootstrap, Error, TEXT("The practice gate requires an actual dynamic navmesh."));
				return false;
			}
		}
		if (bRPG)
		{
			UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
			FNavLocation StationPoint;
			if (Navigation == nullptr || !Navigation->ProjectPointToNavigation(FVector(-1500.0, -1000.0, 50.0),
				StationPoint, FVector(100.0, 100.0, 150.0)))
			{
				UE_LOG(LogKnightBootstrap, Error, TEXT("RPG upgrade station lacks genuine walkable navigation."));
				return false;
			}
		}

		return SaveAsset(Package, World, FPackageName::GetMapPackageExtension());
	}
}

UKnightBootstrapCommandlet::UKnightBootstrapCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
	ShowErrorCount = true;
}

int32 UKnightBootstrapCommandlet::Main(const FString& Params)
{
	int32 Milestone = 0;
	FParse::Value(*Params, TEXT("Milestone="), Milestone);
	if (Milestone == 75)
	{
		return KnightEncounterQualityAuthoring::CreateAssets() && KnightBootstrap::CreateMap(75) ? 0 : 1;
	}
	if (Milestone == 7)
	{
		if (!KnightCampaignAssetAuthoring::CreateAssets() || !KnightBootstrap::CreateMap(7)
			|| !KnightBootstrap::CreateMap(70))
		{
			return 1;
		}
		UE_LOG(LogKnightBootstrap, Display, TEXT("M7 campaign profiles/worlds, audio buses and two playable maps authored."));
		return 0;
	}
	if (Milestone == 6)
	{
		if (!KnightStoryAssetAuthoring::CreateStoryAssets() || !KnightBootstrap::CreateMap(6)
			|| !KnightBootstrap::CreateMap(60) || !KnightBootstrap::CreateMap(63)
			|| !KnightBootstrap::CreateMap(61) || !KnightBootstrap::CreateMap(62))
		{
			return 1;
		}
		UE_LOG(LogKnightBootstrap, Display, TEXT("M6 story definitions and genuine demo/courtyard/V1 maps authored."));
		return 0;
	}
	if (Milestone == 56)
	{
		if (!KnightGearAssetAuthoring::CreateGearAssets() || !KnightBootstrap::CreateMap(56)
			|| !KnightBootstrap::CreateMap(57))
		{
			return 4;
		}
		UE_LOG(LogKnightBootstrap, Display, TEXT("M5.6 genuine 41-item manifest, GearLab and CatalogLab authored."));
		return 0;
	}
	if (Milestone == 55)
	{
		// Existing M0-M4 assets are prerequisites; this branch never reauthors their implementation.
		if (!KnightRPGAssetAuthoring::CreateRPGAssets())
		{
			return 4;
		}
		for (const FName RuneName : { FName(TEXT("Aftershock")), FName(TEXT("Convergence")),
			FName(TEXT("Propagation")), FName(TEXT("Mobile")) })
		{
			const FString Path = TEXT("/Game/Knight/Definitions/Items/DA_Knight_Rune_") + RuneName.ToString();
			const bool bExists = FPackageName::DoesPackageExist(Path);
			UPackage* Package = bExists ? LoadPackage(nullptr, *Path, LOAD_None) : CreatePackage(*Path);
			const FName AssetName(*FPackageName::GetLongPackageAssetName(Path));
			UKnightRuneDefinition* Rune = FindObject<UKnightRuneDefinition>(Package, *AssetName.ToString());
			if (Rune == nullptr && !bExists)
			{
				Rune = NewObject<UKnightRuneDefinition>(Package, AssetName, RF_Public | RF_Standalone);
				Rune->TrySetIdentity(RuneName, TEXT("KnightItem"));
				Rune->SetDisplayName(FText::FromName(RuneName));
				const FName LogicId(*(FString(RuneName == TEXT("Aftershock") ? TEXT("Weapon.") : TEXT("Skill."))
					+ RuneName.ToString()));
				if (!Rune->TrySetAttachmentRune(LogicId).IsSuccess())
				{
					return 4;
				}
				FAssetRegistryModule::AssetCreated(Rune);
			}
			if (Rune == nullptr || !Rune->ValidateItem().IsSuccess()
				|| !KnightBootstrap::SaveAsset(Package, Rune, FPackageName::GetAssetPackageExtension()))
			{
				return 4;
			}
		}
		if ((!FPackageName::DoesPackageExist(KnightBootstrap::RPGMapPackageName) && !KnightBootstrap::CreateMap(5))
			|| !KnightBootstrap::CreateMap(55))
		{
			return 4;
		}
		UE_LOG(LogKnightBootstrap, Display, TEXT("M5.5 genuine SkillLab map and attachment assets authored."));
		return 0;
	}
	if (Milestone < 0 || Milestone > 5)
	{
		UE_LOG(LogKnightBootstrap, Error, TEXT("Supported bootstrap milestones are 0 through 5."));
		return 3;
	}
	if (Milestone > 0)
	{
		if (!KnightBootstrap::CreateFoundationAbilitySet() || !KnightBootstrap::CreatePartyDefinitions()
			|| !KnightBootstrap::CreateFollowTree()
			|| (Milestone >= 3 && !KnightCombatAssetAuthoring::CreateCombatAssets())
			|| (Milestone >= 4 && !KnightAIAssetAuthoring::CreateAIAssets())
			|| (Milestone == 5 && !KnightRPGAssetAuthoring::CreateRPGAssets())
			|| !KnightBootstrap::CreateMap(Milestone))
		{
			return 4;
		}
		UE_LOG(LogKnightBootstrap, Display, TEXT("Milestone %d bootstrap complete: %s, real ability set and follow StateTree."),
			Milestone, Milestone == 5 ? KnightBootstrap::RPGMapPackageName : (Milestone == 4
				? KnightBootstrap::ArenaMapPackageName : (Milestone == 3
				? KnightBootstrap::CombatMapPackageName : (Milestone == 2 ? KnightBootstrap::AbilityMapPackageName
					: KnightBootstrap::TrainingMapPackageName))));
		return 0;
	}
	if (!KnightBootstrap::CreateDefinition())
	{
		return 1;
	}
	if (!KnightBootstrap::CreateMap(0))
	{
		return 2;
	}

	UE_LOG(LogKnightBootstrap, Display,
		TEXT("Foundation bootstrap complete: KnightDefinition:Foundation and %s."), KnightBootstrap::MapPackageName);
	return 0;
}
