#include "Commandlets/GolemBootstrapCommandlet.h"

#include "Commandlets/GolemCombatAssetAuthoring.h"
#include "Commandlets/GolemAIAssetAuthoring.h"
#include "Assets/GolemDefinition.h"
#include "Abilities/GolemReviveAbility.h"
#include "Abilities/GolemTestDamageAbility.h"
#include "AbilityYard/GolemAbilityDummy.h"
#include "AbilityYard/GolemAbilityStation.h"
#include "Definitions/GolemAbilitySetDefinition.h"
#include "Definitions/GolemHeroDefinition.h"
#include "Definitions/GolemPartyDefinition.h"
#include "Tags/GolemGameplayTags.h"
#include "Xenon/GolemXenonDefinition.h"
#include "Tasks/GolemFollowTask.h"

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

DEFINE_LOG_CATEGORY_STATIC(LogGolemBootstrap, Log, All);

namespace GolemBootstrap
{
	const TCHAR* MapPackageName = TEXT("/Game/Golem/Maps/Playables/L_M00_Boot");
	const TCHAR* DefinitionPackageName = TEXT("/Game/Golem/Definitions/DA_Golem_Foundation");
	const TCHAR* BootGameModePath = TEXT("/Script/GolemFramework.GolemBootGameMode");
	const TCHAR* TrainingMapPackageName = TEXT("/Game/Golem/Maps/Playables/L_M01_TrainingYard");
	const TCHAR* TrainingGameModePath = TEXT("/Script/ThreeDreamGolems.GolemGameMode");
	const TCHAR* AbilityMapPackageName = TEXT("/Game/Golem/Maps/Playables/L_M02_AbilityYard");
	const TCHAR* AbilityGameModePath = TEXT("/Script/ThreeDreamGolems.GolemAbilityYardGameMode");
	const TCHAR* ArenaMapPackageName = TEXT("/Game/Golem/Maps/Playables/L_M04_PartyArena");
	const TCHAR* ArenaGameModePath = TEXT("/Script/ThreeDreamGolems.GolemPartyArenaGameMode");
	const TCHAR* ColorArenaMapPackageName = TEXT("/Game/Golem/Maps/Playables/L_M0_ColorArena");
	const TCHAR* ColorArenaGameModePath = TEXT("/Script/ThreeDreamGolems.GolemColorArenaGameMode");
	const TCHAR* FollowTreePackageName = TEXT("/Game/Golem/AI/ST_Golem_Follow");
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
			UE_LOG(LogGolemBootstrap, Error, TEXT("Could not create output directory for %s."), *Filename);
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
			UE_LOG(LogGolemBootstrap, Error, TEXT("Failed to save valid Unreal package %s."), *Filename);
			return false;
		}

		UE_LOG(LogGolemBootstrap, Display, TEXT("Saved %s (%lld bytes)."),
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
			UE_LOG(LogGolemBootstrap, Error, TEXT("Could not open definition package %s."), *PackageName);
			return false;
		}

		UObject* ExistingAsset = FindObject<UObject>(Package, *AssetName.ToString());
		UGolemDefinition* Definition = Cast<UGolemDefinition>(ExistingAsset);
		if (ExistingAsset != nullptr && Definition == nullptr)
		{
			UE_LOG(LogGolemBootstrap, Error, TEXT("Existing fixture has an incompatible class: %s."),
				*ExistingAsset->GetPathName());
			return false;
		}
		if (bPackageExists && Definition == nullptr)
		{
			UE_LOG(LogGolemBootstrap, Error, TEXT("Existing definition package has no expected asset."));
			return false;
		}

		if (Definition == nullptr)
		{
			Definition = NewObject<UGolemDefinition>(Package, AssetName, RF_Public | RF_Standalone);
			if (!Definition->TrySetIdentity(FoundationId))
			{
				UE_LOG(LogGolemBootstrap, Error, TEXT("Foundation identity authoring was rejected."));
				return false;
			}
			Definition->SetDisplayName(NSLOCTEXT("GolemFoundation", "FoundationFixture", "Foundation fixture"));
			FAssetRegistryModule::AssetCreated(Definition);
		}

		const FPrimaryAssetId ExpectedId(FName(TEXT("GolemDefinition")), FoundationId);
		if (!Definition->ValidateIdentity().IsSuccess() || Definition->GetPrimaryAssetId() != ExpectedId)
		{
			UE_LOG(LogGolemBootstrap, Error,
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
				UE_LOG(LogGolemBootstrap, Error, TEXT("Actor %s has an incompatible class."), *ActorName.ToString());
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
			UE_LOG(LogGolemBootstrap, Error, TEXT("Could not spawn actor %s."), *ActorName.ToString());
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
			UE_LOG(LogGolemBootstrap, Error, TEXT("Could not open %s."), *PackageName);
			return nullptr;
		}
		const FName AssetName(*FPackageName::GetLongPackageAssetName(PackageName));
		UObject* ExistingAsset = FindObject<UObject>(Package, *AssetName.ToString());
		DefinitionType* Definition = Cast<DefinitionType>(ExistingAsset);
		if ((ExistingAsset != nullptr || bPackageExists) && Definition == nullptr)
		{
			UE_LOG(LogGolemBootstrap, Error, TEXT("Existing fixture has an incompatible asset: %s."), *PackageName);
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
		UGolemAbilitySetDefinition* Set = FindOrCreateDefinition<UGolemAbilitySetDefinition>(
			TEXT("/Game/Golem/Definitions/DA_Golem_AbilitySet_Foundation"), bCreated);
		if (Set == nullptr)
		{
			return false;
		}
		if (bCreated)
		{
			FGolemAbilitySetEntry Pulse;
			Pulse.Ability = UGolemTestDamageAbility::StaticClass();
			Pulse.InputTag = GolemGameplayTags::Input_Ability_Test;
			FGolemAbilitySetEntry Revive;
			Revive.Ability = UGolemReviveAbility::StaticClass();
			Revive.InputTag = GolemGameplayTags::Input_Party_Revive;
			FGolemTagRelationship PulseRelationship;
			PulseRelationship.AbilityTag = GolemGameplayTags::Ability_Test_Pulse;
			PulseRelationship.ActivationRequiredTags.AddTag(GolemGameplayTags::State_Life_Alive);
			PulseRelationship.ActivationBlockedTags.AddTag(GolemGameplayTags::State_Life_Downed);
			PulseRelationship.ActivationBlockedTags.AddTag(GolemGameplayTags::State_Life_Dead);
			PulseRelationship.ActivationBlockedTags.AddTag(GolemGameplayTags::State_Life_Reviving);
			FGolemTagRelationship ReviveRelationship;
			ReviveRelationship.AbilityTag = GolemGameplayTags::Ability_Party_Revive;
			ReviveRelationship.ActivationRequiredTags.AddTag(GolemGameplayTags::State_Life_Alive);
			ReviveRelationship.ActivationBlockedTags.AddTag(GolemGameplayTags::State_Life_Downed);
			ReviveRelationship.ActivationBlockedTags.AddTag(GolemGameplayTags::State_Life_Dead);
			ReviveRelationship.AbilityTagsToBlock.AddTag(GolemGameplayTags::Ability_Test_Pulse);
			ReviveRelationship.AbilityTagsToBlock.AddTag(GolemGameplayTags::Ability_Party_Revive);
			ReviveRelationship.AbilityTagsToCancel.AddTag(GolemGameplayTags::Ability_Test_Pulse);
			if (!Set->TrySetIdentity(FoundationId, TEXT("GolemAbilitySet"))
				|| !Set->TrySetAbilities({ Pulse, Revive }).IsSuccess()
				|| !Set->TrySetRelationships({ PulseRelationship, ReviveRelationship }).IsSuccess())
			{
				UE_LOG(LogGolemBootstrap, Error, TEXT("Foundation ability set authoring failed."));
				return false;
			}
			Set->SetDisplayName(NSLOCTEXT("GolemAbilityYard", "FoundationSet", "Foundation abilities"));
		}
		if (Set->GetPrimaryAssetId() != FPrimaryAssetId(TEXT("GolemAbilitySet"), FoundationId)
			|| !Set->ValidateAbilitySet().IsSuccess())
		{
			UE_LOG(LogGolemBootstrap, Error, TEXT("Existing Foundation ability set is invalid; preserving authored data."));
			return false;
		}
		bool bHasPulse = false;
		bool bHasRevive = false;
		for (const FGolemAbilitySetEntry& Entry : Set->GetAbilities())
		{
			bHasPulse |= Entry.Ability == UGolemTestDamageAbility::StaticClass()
				&& Entry.InputTag == GolemGameplayTags::Input_Ability_Test;
			bHasRevive |= Entry.Ability == UGolemReviveAbility::StaticClass()
				&& Entry.InputTag == GolemGameplayTags::Input_Party_Revive;
		}
		if (!bHasPulse || !bHasRevive)
		{
			UE_LOG(LogGolemBootstrap, Error, TEXT("Foundation ability set lacks the mapped pulse or revive grant."));
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
			UE_LOG(LogGolemBootstrap, Error, TEXT("Graybox body material does not expose the Color vector parameter."));
			return false;
		}
		UE_LOG(LogGolemBootstrap, Display, TEXT("Graybox body material Color parameter validated: %s."),
			*DefaultBodyColor.ToString());
		const TArray<FName> HeroNames = { TEXT("Mars"), TEXT("Jupiter"), TEXT("Neptune") };
		const TArray<FGameplayTag> HeroElements = { GolemGameplayTags::Element_Earth, GolemGameplayTags::Element_Sky,
			GolemGameplayTags::Element_Sea };
		const TArray<FGameplayTag> HeroClasses = { GolemGameplayTags::Class_Warrior, GolemGameplayTags::Class_Hunter,
			GolemGameplayTags::Class_Mage };
		// Mars 140/12/90/0, Jupiter 90/10/110/0, Neptune 80/9/100/0.5; heavy charge 0.8 s, Mars 1.0 s.
		TArray<FGolemAttributeDefaults> HeroStats;
		HeroStats.SetNum(3);
		HeroStats[0].MaxHealth = 140.0f; HeroStats[0].Power = 12.0f; HeroStats[0].Agility = 90.0f; HeroStats[0].ColorHold = 0.0f;
		HeroStats[1].MaxHealth = 90.0f; HeroStats[1].Power = 10.0f; HeroStats[1].Agility = 110.0f; HeroStats[1].ColorHold = 0.0f;
		HeroStats[2].MaxHealth = 80.0f; HeroStats[2].Power = 9.0f; HeroStats[2].Agility = 100.0f; HeroStats[2].ColorHold = 0.5f;
		TArray<FGolemHeroMoveTuning> HeroMoves;
		HeroMoves.SetNum(3);
		HeroMoves[0].HeavyChargeSeconds = 1.0f;
		HeroMoves[1].HeavyChargeSeconds = 0.8f;
		HeroMoves[2].HeavyChargeSeconds = 0.8f;
		// Bow and staff shoot straight for 25 m; the hammer swings.
		HeroMoves[1].bHeavyIsProjectile = true;
		HeroMoves[2].bHeavyIsProjectile = true;
		// Class moves: Warrior guards (80% less from the front, 3 s at most, 6 s recovery), Hunter leaps
		// back 5 m and shoots for 1.5 x Power (8 s), Mage shields everyone within 4 m against one hit for 2 s (12 s).
		HeroMoves[0].ClassMove = EGolemClassMove::Guard;
		HeroMoves[0].ClassMoveCooldownSeconds = 6.0f;
		HeroMoves[0].GuardDamageFraction = 0.2f;
		HeroMoves[0].GuardMaxSeconds = 3.0f;
		HeroMoves[1].ClassMove = EGolemClassMove::BackflipShot;
		HeroMoves[1].ClassMoveCooldownSeconds = 8.0f;
		HeroMoves[1].BackflipDistance = 500.0f;
		HeroMoves[1].BackflipShotPowerScale = 1.5f;
		HeroMoves[2].ClassMove = EGolemClassMove::Barrier;
		HeroMoves[2].ClassMoveCooldownSeconds = 12.0f;
		HeroMoves[2].BarrierRadius = 400.0f;
		HeroMoves[2].BarrierSeconds = 2.0f;
		const TArray<FLinearColor> HeroColors =
		{
			FLinearColor(0.9f, 0.1f, 0.08f), FLinearColor(0.95f, 0.8f, 0.1f), FLinearColor(0.08f, 0.25f, 0.95f)
		};
		const TArray<FText> HeroDisplayNames =
		{
			NSLOCTEXT("GolemHeroes", "Mars", "Mars"),
			NSLOCTEXT("GolemHeroes", "Jupiter", "Jupiter"),
			NSLOCTEXT("GolemHeroes", "Neptune", "Neptune")
		};
		const TArray<FVector> FormationOffsets =
		{
			FVector(-280.0, -180.0, 0.0), FVector(-280.0, 180.0, 0.0), FVector(-280.0, -350.0, 0.0)
		};
		TArray<FPrimaryAssetId> HeroIds;
		for (int32 Index = 0; Index < HeroNames.Num(); ++Index)
		{
			const FString HeroPackage = FString::Printf(TEXT("/Game/Golem/Definitions/DA_Golem_Hero_%s"),
				*HeroNames[Index].ToString());
			bool bCreated = false;
			UGolemHeroDefinition* Hero = FindOrCreateDefinition<UGolemHeroDefinition>(HeroPackage, bCreated);
			if (Hero == nullptr)
			{
				return false;
			}
			if (bCreated)
			{
				if (!Hero->TrySetIdentity(HeroNames[Index], TEXT("GolemHero"))
					|| !Hero->TrySetPresentation(HeroColors[Index], 450.0f, 700.0f,
						FormationOffsets[Index], 120.0f).IsSuccess()
					|| !Hero->TrySetArchetype(HeroElements[Index], HeroClasses[Index]).IsSuccess())
				{
					UE_LOG(LogGolemBootstrap, Error, TEXT("Training hero definition authoring failed."));
					return false;
				}
				Hero->SetDisplayName(HeroDisplayNames[Index]);
			}
			// Design numbers (DEV_SPEC section 4) are reapplied on every run: a changed draft is a rerun.
			if (!Hero->TrySetCombatData(HeroStats[Index], HeroMoves[Index]).IsSuccess())
			{
				UE_LOG(LogGolemBootstrap, Error, TEXT("Hero combat data rejected: %s."), *HeroNames[Index].ToString());
				return false;
			}
			const FPrimaryAssetId ExpectedId(TEXT("GolemHero"), HeroNames[Index]);
			if (Hero->GetPrimaryAssetId() != ExpectedId || !Hero->ValidateHero().IsSuccess()
				|| !SaveAsset(Hero->GetPackage(), Hero, FPackageName::GetAssetPackageExtension()))
			{
				UE_LOG(LogGolemBootstrap, Error, TEXT("Training hero identity/save validation failed: %s."),
					*HeroPackage);
				return false;
			}
			HeroIds.Add(ExpectedId);
		}

		bool bCreated = false;
		UGolemPartyDefinition* Party = FindOrCreateDefinition<UGolemPartyDefinition>(
			TEXT("/Game/Golem/Definitions/DA_Golem_Party_ThreeGolems"), bCreated);
		if (Party == nullptr)
		{
			return false;
		}
		if (bCreated)
		{
			if (!Party->TrySetIdentity(TEXT("ThreeGolems"), TEXT("GolemParty"))
				|| !Party->TrySetHeroDefinitions(HeroIds).IsSuccess())
			{
				UE_LOG(LogGolemBootstrap, Error, TEXT("Training party definition authoring failed."));
				return false;
			}
			Party->SetDisplayName(NSLOCTEXT("GolemFoundation", "ThreeGolems", "Three Golems"));
		}
		if (Party->GetPrimaryAssetId() != FPrimaryAssetId(TEXT("GolemParty"), TEXT("ThreeGolems"))
			|| !Party->ValidateParty().IsSuccess() || Party->GetHeroDefinitions() != HeroIds)
		{
			UE_LOG(LogGolemBootstrap, Error, TEXT("Existing training party has an incompatible identity."));
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
			Root.AddTask<FGolemFollowTask>().SetNodeName(FName(TEXT("FollowHumanGolem")));
		}
		const UStateTreeEditorData* EditorData = Cast<UStateTreeEditorData>(Tree->EditorData);
		if (EditorData == nullptr || EditorData->Schema == nullptr
			|| !EditorData->Schema->IsA<UStateTreeAIComponentSchema>())
		{
			UE_LOG(LogGolemBootstrap, Error, TEXT("Follow StateTree has an incompatible schema."));
			return false;
		}
		bool bHasFollowTask = false;
		EditorData->VisitHierarchy([&bHasFollowTask](UStateTreeState& State, UStateTreeState* ParentState)
		{
			for (const FStateTreeEditorNode& Task : State.Tasks)
			{
				bHasFollowTask |= Task.Node.GetScriptStruct() == FGolemFollowTask::StaticStruct();
			}
			return EStateTreeVisitor::Continue;
		});
		if (!bHasFollowTask)
		{
			UE_LOG(LogGolemBootstrap, Error, TEXT("Existing follow StateTree lacks the native formation task."));
			return false;
		}
		FStateTreeCompilerLog CompilerLog;
		FStateTreeCompiler Compiler(CompilerLog);
		const bool bCompiled = Compiler.Compile(*Tree);
		CompilerLog.DumpToLog(Tree, LogGolemBootstrap);
		if (!bCompiled || !Tree->IsReadyToRun())
		{
			UE_LOG(LogGolemBootstrap, Error, TEXT("Follow StateTree compilation/ready validation failed."));
			return false;
		}
		return SaveAsset(Tree->GetPackage(), Tree, FPackageName::GetAssetPackageExtension());
	}

	bool CreateTrainingNavigation(UWorld* World)
	{
		ANavMeshBoundsVolume* Bounds = FindOrCreateActor<ANavMeshBoundsVolume>(World,
			FName(TEXT("GolemTraining_NavBounds")), FTransform(FVector(0.0, 0.0, 200.0)));
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
			UE_LOG(LogGolemBootstrap, Error,
				TEXT("Training navigation bounds have no valid brush geometry: model %s, registered %s, box %s."),
				Bounds->Brush != nullptr ? TEXT("present") : TEXT("missing"),
				Bounds->HasActorRegisteredAllComponents() ? TEXT("true") : TEXT("false"), *BoundsBox.ToString());
			return false;
		}
		UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
		if (Navigation == nullptr)
		{
			UE_LOG(LogGolemBootstrap, Error, TEXT("Training world navigation system was not initialized."));
			return false;
		}
		Navigation->OnNavigationBoundsUpdated(Bounds);
		FlushAsyncLoading();
		FAssetCompilingManager::Get().FinishAllCompilation();
		Navigation->OnWorldInitDone(FNavigationSystemRunMode::EditorMode);
		if (FAssetCompilingManager::Get().GetNumRemainingAssets() != 0)
		{
			UE_LOG(LogGolemBootstrap, Error, TEXT("Training navigation assets have not finished compilation."));
			return false;
		}
		// Commandlets do not run the editor's delayed unlock ticker after synchronous asset compilation.
		Navigation->RemoveNavigationBuildLock(ENavigationBuildLock::AsyncLoadLock,
			UNavigationSystemV1::ELockRemovalRebuildAction::NoRebuild);
		Navigation->Build();
		ANavigationData* NavigationData = Navigation->GetDefaultNavDataInstance(FNavigationSystem::DontCreate);
		if (NavigationData == nullptr)
		{
			UE_LOG(LogGolemBootstrap, Error, TEXT("Training navigation build produced no navigation data."));
			return false;
		}
		NavigationData->EnsureBuildCompletion();
		FNavLocation ProjectedLocation;
		if (!Navigation->ProjectPointToNavigation(FVector(0.0, -900.0, 50.0), ProjectedLocation,
			FVector(100.0, 100.0, 200.0)))
		{
			UE_LOG(LogGolemBootstrap, Error, TEXT("Training navmesh cannot project the party spawn point."));
			return false;
		}
		UE_LOG(LogGolemBootstrap, Display, TEXT("Training navigation compiled: spawn projects to %s."),
			*ProjectedLocation.Location.ToString());
		return true;
	}

	bool CreateTrainingBoundary(UWorld* World, UStaticMesh* WallMesh)
	{
		const TArray<FName> WallNames =
		{
			TEXT("GolemTraining_WallEast"), TEXT("GolemTraining_WallWest"),
			TEXT("GolemTraining_WallNorth"), TEXT("GolemTraining_WallSouth")
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
				UE_LOG(LogGolemBootstrap, Error,
					TEXT("Training wall %s requires a 400 cm blocking physical mesh; preserving existing authored data."),
					*WallNames[Index].ToString());
				return false;
			}
		}
		UE_LOG(LogGolemBootstrap, Display, TEXT("Training perimeter validated: four physical 400 cm walls."));
		return true;
	}

	bool CreateTrainingMarkers(UWorld* World)
	{
		UClass* MarkerClass = LoadClass<AActor>(nullptr, TEXT("/Script/ThreeDreamGolems.GolemTrainingMarker"));
		FIntProperty* IndexProperty = MarkerClass != nullptr
			? FindFProperty<FIntProperty>(MarkerClass, TEXT("MarkerIndex")) : nullptr;
		FStructProperty* ColorProperty = MarkerClass != nullptr
			? FindFProperty<FStructProperty>(MarkerClass, TEXT("MarkerColor")) : nullptr;
		if (IndexProperty == nullptr || ColorProperty == nullptr
			|| ColorProperty->Struct != TBaseStructure<FLinearColor>::Get())
		{
			UE_LOG(LogGolemBootstrap, Error, TEXT("Native training marker class/properties are unavailable."));
			return false;
		}
		const TArray<FVector> Locations =
		{
			FVector(450.0, -250.0, 100.0), FVector(800.0, 300.0, 100.0), FVector(1100.0, -150.0, 100.0)
		};
		const TArray<FLinearColor> Colors = { FLinearColor::Red, FLinearColor::Blue, FLinearColor::Green };
		for (int32 Index = 0; Index < Locations.Num(); ++Index)
		{
			const FName MarkerName(*FString::Printf(TEXT("GolemTraining_Marker%d"), Index));
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
		AGolemAbilityDummy* Dummy = FindOrCreateActor<AGolemAbilityDummy>(World,
			FName(TEXT("GolemAbility_Dummy")), FTransform(FVector(400.0, -1000.0, 100.0)));
		const TArray<FName> StationNames = { TEXT("GolemAbility_DamageStation"), TEXT("GolemAbility_ReviveStation") };
		const TArray<FVector> Locations = { FVector(400.0, -450.0, 50.0), FVector(900.0, 350.0, 50.0) };
		const TArray<EGolemAbilityStationKind> Kinds =
		{
			EGolemAbilityStationKind::DamagePlayer, EGolemAbilityStationKind::DownCompanion
		};
		if (Dummy == nullptr)
		{
			return false;
		}
		for (int32 Index = 0; Index < StationNames.Num(); ++Index)
		{
			const bool bCreated = FindObject<AActor>(World->PersistentLevel, *StationNames[Index].ToString()) == nullptr;
			AGolemAbilityStation* Station = FindOrCreateActor<AGolemAbilityStation>(World,
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
				UE_LOG(LogGolemBootstrap, Error, TEXT("Ability station kind differs from its expected fixture."));
				return false;
			}
		}
		return true;
	}

	bool CreateArenaFixtures(UWorld* World, UStaticMesh* ObstacleMesh)
	{
		UClass* MarkerClass = LoadClass<AActor>(nullptr,
			TEXT("/Script/ThreeDreamGolems.GolemPartyArenaStartMarker"));
		if (MarkerClass == nullptr)
		{
			UE_LOG(LogGolemBootstrap, Error, TEXT("Native arena Start marker is unavailable."));
			return false;
		}
		AActor* StartMarker = FindOrCreateActor<AActor>(World, TEXT("GolemArena_Start"),
			FTransform(FVector(0.0, -650.0, 20.0)), MarkerClass);
		const bool bCreateObstacle = FindObject<AActor>(World->PersistentLevel, TEXT("GolemArena_LOSBlocker"))
			== nullptr;
		AStaticMeshActor* Obstacle = FindOrCreateActor<AStaticMeshActor>(World, TEXT("GolemArena_LOSBlocker"),
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
			UE_LOG(LogGolemBootstrap, Error, TEXT("Arena LOS blocker needs genuine blocking physical geometry."));
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
				UE_LOG(LogGolemBootstrap, Error, TEXT("Arena spawn anchor has no real navigation: %s."), *Anchor.ToString());
				return false;
			}
			UNavigationPath* Path = Navigation->FindPathToLocationSynchronously(World, Start, Projected.Location);
			if (Path == nullptr || !Path->IsValid() || Path->IsPartial() || Path->PathPoints.Num() < 2)
			{
				UE_LOG(LogGolemBootstrap, Error, TEXT("Arena encounter anchor is unreachable from Start: %s."),
					*Anchor.ToString());
				return false;
			}
		}
		FHitResult Obstruction;
		if (!World->LineTraceSingleByChannel(Obstruction, FVector(0.0, -500.0, 100.0),
			FVector(0.0, 500.0, 100.0), ECC_Visibility)
			|| Obstruction.GetActor() == nullptr
			|| Obstruction.GetActor()->GetFName() != FName(TEXT("GolemArena_LOSBlocker")))
		{
			UE_LOG(LogGolemBootstrap, Error, TEXT("Arena blocker does not obstruct the real center LOS trace."));
			return false;
		}
		UE_LOG(LogGolemBootstrap, Display, TEXT("Arena navigation: all three encounter anchors reachable; center LOS blocked."));
		return true;
	}

	/** DEV_SPEC section 6 starting values for the two Xenons of milestone M0. */
	bool CreateXenonDefinitions()
	{
		struct FXenonSeed
		{
			const TCHAR* Name;
			FText DisplayName;
			float MaxHealth;
			float ContactDamage;
			int32 VoidsToFill;
			bool bFirstVoidBasic;
			bool bShield;
		};
		const FXenonSeed Seeds[] =
		{
			{ TEXT("Grunt"), NSLOCTEXT("GolemXenon", "Grunt", "Grunt"), 300.0f, 20.0f, 3, true, false },
			{ TEXT("Shield"), NSLOCTEXT("GolemXenon", "Shield", "Shield"), 400.0f, 25.0f, 4, false, true }
		};
		for (const FXenonSeed& Seed : Seeds)
		{
			const FString PackageName = FString::Printf(TEXT("/Game/Golem/Definitions/Xenon/DA_Golem_Xenon_%s"), Seed.Name);
			bool bCreated = false;
			UGolemXenonDefinition* Xenon = FindOrCreateDefinition<UGolemXenonDefinition>(PackageName, bCreated);
			if (Xenon == nullptr)
			{
				return false;
			}
			if (bCreated)
			{
				if (!Xenon->TrySetIdentity(FName(Seed.Name), TEXT("GolemXenon")))
				{
					UE_LOG(LogGolemBootstrap, Error, TEXT("Xenon definition authoring failed: %s."), *PackageName);
					return false;
				}
				Xenon->SetDisplayName(Seed.DisplayName);
			}
			{
				// Design numbers (DEV_SPEC sections 5 and 6) are reapplied on every run: a changed draft is a rerun.
				FGolemVoidHostTuning Voids;
				Voids.VoidsToFill = Seed.VoidsToFill;
				Voids.MaxOpenVoids = 1;
				// Front and back, so consecutive voids alternate sides: the Shield's shield, then its back.
				// Local centimetres before the 2.5 body scale: 105 cm out from the centre and about
				// 135 cm above the floor, where a hammer swing and a level shot both reach.
				TArray<FGolemXenonVoidSlot> Slots;
				Slots.SetNum(2);
				Slots[0].Offset = FVector(42.0, 0.0, -36.0);
				Slots[0].Radius = 22.0f;
				Slots[1].Offset = FVector(-42.0, 0.0, -36.0);
				Slots[1].Radius = 22.0f;
				// Grunt: a slow, wide sweep. Shield: a narrow shove that throws Golems back, and a slow turn
				// so its back can be reached. Every attack is shown first; 0.6 s is the minimum the design allows.
				FGolemXenonBehavior Behavior;
				FGolemXenonMove& Move = Behavior.Moves.AddDefaulted_GetRef();
				if (Seed.bShield)
				{
					Behavior.MoveSpeed = 170.0f;
					Behavior.TurnRateDegrees = 70.0f;
					Move.MoveId = TEXT("ShieldShove");
					Move.StartRange = 260.0f;
					Move.TelegraphSeconds = 0.8f;
					Move.RecoverSeconds = 1.2f;
					Move.Range = 300.0f;
					Move.HalfAngleDegrees = 40.0f;
					Move.Knockback = 900.0f;
				}
				else
				{
					Behavior.MoveSpeed = 200.0f;
					Behavior.TurnRateDegrees = 110.0f;
					Move.MoveId = TEXT("GruntSweep");
					Move.StartRange = 290.0f;
					Move.TelegraphSeconds = 1.0f;
					Move.RecoverSeconds = 1.4f;
					Move.Range = 340.0f;
					Move.HalfAngleDegrees = 80.0f;
				}
				if (!Xenon->TrySetXenon(Seed.MaxHealth, Seed.ContactDamage, 2.5f, Voids, Slots, 0.3f,
						Seed.bFirstVoidBasic).IsSuccess() || !Xenon->TrySetBehavior(Behavior).IsSuccess())
				{
					UE_LOG(LogGolemBootstrap, Error, TEXT("Xenon definition authoring failed: %s."), *PackageName);
					return false;
				}
			}
			if (Xenon->GetPrimaryAssetId() != FPrimaryAssetId(TEXT("GolemXenon"), FName(Seed.Name))
				|| !Xenon->ValidateXenon().IsSuccess()
				|| !SaveAsset(Xenon->GetPackage(), Xenon, FPackageName::GetAssetPackageExtension()))
			{
				UE_LOG(LogGolemBootstrap, Error, TEXT("Xenon definition identity/save validation failed: %s."), *PackageName);
				return false;
			}
		}
		return true;
	}

	bool CreateMap(const int32 Milestone)
	{
		const bool bTraining = Milestone > 0;
		const bool bAbilityYard = Milestone == 2;
		const bool bArena = Milestone >= 4;
		const bool bColorArena = Milestone == 10;
		const TCHAR* GameModePath = bColorArena ? ColorArenaGameModePath : bArena ? ArenaGameModePath
			: (bAbilityYard ? AbilityGameModePath : (bTraining ? TrainingGameModePath : BootGameModePath));
		UClass* BootGameMode = LoadClass<AGameModeBase>(nullptr, GameModePath);
		UStaticMesh* FloorMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
		if (BootGameMode == nullptr || FloorMesh == nullptr)
		{
			UE_LOG(LogGolemBootstrap, Error, TEXT("Boot GameMode or engine cube mesh could not be loaded."));
			return false;
		}

		const FString PackageName(bColorArena ? ColorArenaMapPackageName : bArena ? ArenaMapPackageName
			: (bAbilityYard ? AbilityMapPackageName : (bTraining ? TrainingMapPackageName : MapPackageName)));
		const bool bPackageExists = FPackageName::DoesPackageExist(PackageName);
		UPackage* Package = bPackageExists
			? LoadPackage(nullptr, *PackageName, LOAD_None)
			: CreatePackage(*PackageName);
		if (Package == nullptr)
		{
			UE_LOG(LogGolemBootstrap, Error, TEXT("Could not open boot map package."));
			return false;
		}

		UWorld* World = UWorld::FindWorldInPackage(Package);
		const bool bOwnWorldInitialization = World == nullptr || !World->IsInitialized();
		if (bPackageExists && World == nullptr)
		{
			UE_LOG(LogGolemBootstrap, Error, TEXT("Existing boot map package does not contain a world."));
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
				UE_LOG(LogGolemBootstrap, Error, TEXT("Unreal could not create the boot world."));
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
			UE_LOG(LogGolemBootstrap, Error, TEXT("Boot world lacks required level or world settings."));
			return false;
		}
		if (bPackageExists && WorldSettings->DefaultGameMode != nullptr
			&& WorldSettings->DefaultGameMode.Get() != BootGameMode)
		{
			UE_LOG(LogGolemBootstrap, Error, TEXT("Existing boot map has another GameMode; preserving it."));
			return false;
		}
		WorldSettings->DefaultGameMode = BootGameMode;
		if (bTraining && bOwnWorldInitialization)
		{
			WorldSettings->KillZ = TrainingKillZ;
		}
		// InitWorld prepares a loaded world's systems but does not register its saved actor components.
		World->UpdateWorldComponents(false, false);

		const FString ActorPrefix(bTraining ? TEXT("GolemTraining") : TEXT("GolemBoot"));
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
			UE_LOG(LogGolemBootstrap, Error, TEXT("Boot directional light lacks a directional component."));
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
		if (bArena && !CreateArenaFixtures(World, FloorMesh))
		{
			return false;
		}
		if (bTraining && !CreateTrainingNavigation(World))
		{
			return false;
		}
		if (bArena && !ValidateArenaNavigation(World))
		{
			return false;
		}

		return SaveAsset(Package, World, FPackageName::GetMapPackageExtension());
	}
}

UGolemBootstrapCommandlet::UGolemBootstrapCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
	ShowErrorCount = true;
}

int32 UGolemBootstrapCommandlet::Main(const FString& Params)
{
	int32 Milestone = 0;
	FParse::Value(*Params, TEXT("Milestone="), Milestone);
	if ((Milestone < 0 || Milestone > 4 || Milestone == 3) && Milestone != 10)
	{
		UE_LOG(LogGolemBootstrap, Error, TEXT("Supported bootstrap milestones are 0, 1, 2, 4 and 10 (colour arena)."));
		return 3;
	}
	if (Milestone > 0)
	{
		if (!GolemBootstrap::CreateFoundationAbilitySet() || !GolemBootstrap::CreatePartyDefinitions()
			|| !GolemBootstrap::CreateFollowTree()
			|| (Milestone >= 3 && !GolemCombatAssetAuthoring::CreateCombatAssets())
			|| (Milestone >= 4 && !GolemAIAssetAuthoring::CreateAIAssets())
			|| (Milestone == 10 && !GolemBootstrap::CreateXenonDefinitions())
			|| !GolemBootstrap::CreateMap(Milestone))
		{
			return 4;
		}
		UE_LOG(LogGolemBootstrap, Display, TEXT("Milestone %d bootstrap complete: %s, real ability set and follow StateTree."),
			Milestone, Milestone == 10 ? GolemBootstrap::ColorArenaMapPackageName : Milestone == 4 ? GolemBootstrap::ArenaMapPackageName
				: (Milestone == 2 ? GolemBootstrap::AbilityMapPackageName : GolemBootstrap::TrainingMapPackageName));
		return 0;
	}
	if (!GolemBootstrap::CreateDefinition())
	{
		return 1;
	}
	if (!GolemBootstrap::CreateMap(0))
	{
		return 2;
	}

	UE_LOG(LogGolemBootstrap, Display,
		TEXT("Foundation bootstrap complete: GolemDefinition:Foundation and %s."), GolemBootstrap::MapPackageName);
	return 0;
}
