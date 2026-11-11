#include "Commandlets/GolemRPGAssetAuthoring.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Assets/GolemAssetManager.h"
#include "Components/StaticMeshComponent.h"
#include "Definitions/GolemItemDefinition.h"
#include "Definitions/GolemLootTableDefinition.h"
#include "Definitions/GolemRuneDefinition.h"
#include "Definitions/GolemSkillDefinition.h"
#include "Definitions/GolemSkillTreeDefinition.h"
#include "Engine/Level.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/WorldSettings.h"
#include "Misc/AutomationTest.h"
#include "Models/GolemRPGModels.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "NavMesh/RecastNavMesh.h"
#include "UObject/StrongObjectPtr.h"
#include <limits>

namespace GolemRPGAssetTests
{
	template <typename DefinitionType>
	DefinitionType* Resolve(FAutomationTestBase* Test, FName Type, FName Name)
	{
		UGolemDefinition* Definition = nullptr;
		UGolemAssetManager* Manager = UGolemAssetManager::GetGolemAssetManager();
		const FPrimaryAssetId Id(Type, Name);
		if (!Test->TestTrue(*FString::Printf(TEXT("Actual typed catalog resolves %s"), *Id.ToString()),
			Manager != nullptr && Manager->ResolveDefinition(Id, DefinitionType::StaticClass(), Definition).IsSuccess()))
		{
			return nullptr;
		}
		return CastChecked<DefinitionType>(Definition);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGolemRPGSavedCatalogTest, "Golem.RPG.Assets.SavedCatalogAndSeededLoot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGolemRPGSavedCatalogTest::RunTest(const FString& Parameters)
{
	if (!TestTrue(TEXT("Genuine saved RPG assets and their typed dependencies validate"), GolemRPGAssetAuthoring::ValidateRPGAssets()))
	{
		AddError(TEXT("Run actual GolemBootstrap -Milestone=5 before the cold-load RPG asset gate."));
		return false;
	}
	UGolemItemDefinition* Shard = GolemRPGAssetTests::Resolve<UGolemItemDefinition>(this, TEXT("GolemItem"), TEXT("ForgeShard"));
	UGolemItemDefinition* Hammer = GolemRPGAssetTests::Resolve<UGolemItemDefinition>(this, TEXT("GolemItem"), TEXT("HammerUpgrade"));
	UGolemRuneDefinition* Rune = GolemRPGAssetTests::Resolve<UGolemRuneDefinition>(this, TEXT("GolemItem"), TEXT("RuneUpgrade"));
	UGolemSkillDefinition* Force = GolemRPGAssetTests::Resolve<UGolemSkillDefinition>(this, TEXT("GolemSkill"), TEXT("HammerForce"));
	UGolemSkillDefinition* Focus = GolemRPGAssetTests::Resolve<UGolemSkillDefinition>(this, TEXT("GolemSkill"), TEXT("RuneFocus"));
	UGolemSkillDefinition* Impossible = GolemRPGAssetTests::Resolve<UGolemSkillDefinition>(this, TEXT("GolemSkill"), TEXT("ImpossibleCost"));
	UGolemSkillTreeDefinition* Tree = GolemRPGAssetTests::Resolve<UGolemSkillTreeDefinition>(this, TEXT("GolemSkillTree"), TEXT("Foundation"));
	UGolemLootTableDefinition* Loot = GolemRPGAssetTests::Resolve<UGolemLootTableDefinition>(this, TEXT("GolemLootTable"), TEXT("EncounterA"));
	if (Shard == nullptr || Hammer == nullptr || Rune == nullptr || Force == nullptr || Focus == nullptr
		|| Impossible == nullptr || Tree == nullptr || Loot == nullptr)
	{
		return false;
	}
	TestEqual(TEXT("Shard is a shared material"), Shard->GetItemKind(), EGolemItemKind::Material);
	TestEqual(TEXT("Shard uses bounded stack9999"), Shard->GetMaximumStack(), 9999);
	TestEqual(TEXT("Hammer is a single equipment instance"), Hammer->GetItemKind(), EGolemItemKind::Equipment);
	TestEqual(TEXT("Hammer is unstackable"), Hammer->GetMaximumStack(), 1);
	TestEqual(TEXT("Hammer authored AP bonus"), Hammer->GetStatBonuses().AttackPower, 10.0f);
	TestEqual(TEXT("Hammer authored max-health bonus"), Hammer->GetStatBonuses().MaxHealth, 20.0f);
	TestTrue(TEXT("Rune remains a stable GolemItem"),
		Rune->GetPrimaryAssetId().PrimaryAssetType == FPrimaryAssetType(TEXT("GolemItem")));
	TestEqual(TEXT("Rune tunes existing action damage"), Rune->GetRuneTuning().DamageScale, 1.2f);
	TestEqual(TEXT("Rune tunes existing action cost"), Rune->GetRuneTuning().CostScale, 0.5f);
	TestEqual(TEXT("Rune tunes existing actual status duration"), Rune->GetRuneTuning().DurationScale, 1.5f);
	TestFalse(TEXT("Rune does not author duplicate input ability set"), Rune->GetAbilitySetId().IsValid());
	TestEqual(TEXT("Hammer Force costs one point"), Force->GetPointCost(), 1);
	TestEqual(TEXT("Hammer Force grants measurable AP"), Force->GetStatBonuses().AttackPower, 5.0f);
	TestEqual(TEXT("Rune Focus costs one point"), Focus->GetPointCost(), 1);
	TestTrue(TEXT("Rune Focus requires actual Hammer Force definition"),
		Focus->GetPrerequisites() == TArray<FPrimaryAssetId> { Force->GetPrimaryAssetId() });
	TestEqual(TEXT("Rune Focus adds duration to the existing Rune"), Focus->GetRuneTuning().DurationScale, 1.5f);
	TestEqual(TEXT("ImpossibleCost is valid costly content"), Impossible->GetPointCost(), 50);
	for (const UGolemSkillDefinition* Skill : { Force, Focus, Impossible })
	{
		if (TestEqual(TEXT("Every skill has one actual material cost"), Skill->GetMaterialCosts().Num(), 1))
		{
			TestTrue(TEXT("Skill cost references actual material definition"), Skill->GetMaterialCosts()[0].DefinitionId == Shard->GetPrimaryAssetId());
			TestEqual(TEXT("Skill material cost is authored"), Skill->GetMaterialCosts()[0].Count, Skill == Impossible ? 100 : 2);
		}
	}
	const TArray<const UGolemSkillDefinition*> Nodes { Force, Focus, Impossible };
	TestTrue(TEXT("Actual three-node prerequisite DAG validates"), Tree->ValidateGraph(Nodes).IsSuccess());
	TestEqual(TEXT("Skill tree contains all three definitions"), Tree->GetSkills().Num(), 3);
	const FGuid Reward(305, 5, 10, 1);
	TArray<FGolemItemInstance> First;
	TArray<FGolemItemInstance> Repeated;
	TArray<FGolemItemInstance> AnotherReward;
	TestTrue(TEXT("Real typed loot generation succeeds"), Loot->GenerateLoot(305, Reward, First).IsSuccess());
	TestTrue(TEXT("Repeated seed and reward succeed"), Loot->GenerateLoot(305, Reward, Repeated).IsSuccess());
	TestTrue(TEXT("Same reward yields identical stable item instances"), First == Repeated);
	TestTrue(TEXT("Different actual reward generates"), Loot->GenerateLoot(305, FGuid(305, 5, 10, 2), AnotherReward).IsSuccess());
	TestEqual(TEXT("Guaranteed loot contains three entries"), First.Num(), 3);
	TSet<FGuid> Instances;
	TSet<FPrimaryAssetId> Definitions;
	for (const FGolemItemInstance& Item : First)
	{
		TestTrue(TEXT("Generated instance has real valid identity and count"), Item.ValidateIdentityAndCount().IsSuccess());
		TestFalse(TEXT("Reward never repeats an instance identity"), Instances.Contains(Item.InstanceId));
		Instances.Add(Item.InstanceId);
		Definitions.Add(Item.DefinitionId);
		TestEqual(TEXT("Guaranteed loot count respects definition"), Item.Count,
			Item.DefinitionId == Shard->GetPrimaryAssetId() ? 10 : 1);
	}
	TestTrue(TEXT("Guaranteed loot supplies hammer, rune and shards"), Definitions.Contains(Hammer->GetPrimaryAssetId())
		&& Definitions.Contains(Rune->GetPrimaryAssetId()) && Definitions.Contains(Shard->GetPrimaryAssetId()));
	for (const FGolemItemInstance& Item : AnotherReward)
	{
		TestFalse(TEXT("Separate reward cannot reuse earlier item GUID"), Instances.Contains(Item.InstanceId));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGolemRPGSavedMapTest, "Golem.RPG.Assets.PlayableMapComposition",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGolemRPGSavedMapTest::RunTest(const FString& Parameters)
{
	UWorld* World = LoadObject<UWorld>(nullptr, TEXT("/Game/Golem/Maps/Playables/L_M05_RPGLoop.L_M05_RPGLoop"));
	if (!TestNotNull(TEXT("Genuine saved RPG playable map cold-loads"), World)
		|| !TestNotNull(TEXT("Saved map contains persistent level"), World->PersistentLevel.Get()))
	{
		return false;
	}
	AWorldSettings* Settings = World->GetWorldSettings();
	UClass* ExpectedGameMode = LoadClass<AGameModeBase>(nullptr, TEXT("/Script/ThreeDreamGolems.GolemRPGLoopGameMode"));
	TestTrue(TEXT("Map wires actual native RPGLoop GameMode"), ExpectedGameMode != nullptr && Settings != nullptr
		&& Settings->DefaultGameMode.Get() == ExpectedGameMode);
	UClass* StationClass = LoadClass<AActor>(nullptr, TEXT("/Script/ThreeDreamGolems.GolemRPGUpgradeStation"));
	AActor* Station = FindObject<AActor>(World->PersistentLevel, TEXT("GolemRPG_UpgradeStation"));
	TestTrue(TEXT("Map contains actual native upgrade station"), Station != nullptr && StationClass != nullptr && Station->IsA(StationClass));
	if (Station != nullptr)
	{
		TestTrue(TEXT("Upgrade station retains reachable safe-zone placement"),
			Station->GetActorLocation().Equals(FVector(-1500, -1000, 20), 1.0));
	}
	TestNotNull(TEXT("Map keeps actual arena start marker"), FindObject<AActor>(World->PersistentLevel, TEXT("GolemArena_Start")));
	AStaticMeshActor* Floor = FindObject<AStaticMeshActor>(World->PersistentLevel, TEXT("GolemTraining_Floor"));
	TestTrue(TEXT("Map has a genuine physical floor"), Floor != nullptr && Floor->GetStaticMeshComponent()->GetStaticMesh() != nullptr
		&& Floor->GetStaticMeshComponent()->GetCollisionEnabled() == ECollisionEnabled::QueryAndPhysics);
	TestNotNull(TEXT("Map keeps actual center LOS obstacle"), FindObject<AActor>(World->PersistentLevel, TEXT("GolemArena_LOSBlocker")));
	ANavMeshBoundsVolume* Bounds = FindObject<ANavMeshBoundsVolume>(World->PersistentLevel, TEXT("GolemTraining_NavBounds"));
	TestTrue(TEXT("Map saves genuine volume brush navigation bounds"), Bounds != nullptr && Bounds->Brush != nullptr);
	bool bPopulatedNavigation = false;
	for (AActor* Actor : World->PersistentLevel->Actors)
	{
		if (const ARecastNavMesh* Navigation = Cast<ARecastNavMesh>(Actor))
		{
			bPopulatedNavigation |= Navigation->GetNumActiveTiles() > 0;
		}
	}
	TestTrue(TEXT("Cold saved map contains populated Recast navigation tiles"), bPopulatedNavigation);
	TestTrue(TEXT("Map retains authored KillZ recovery boundary"), Settings != nullptr && Settings->KillZ == -2000.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGolemRPGAssetInvalidMutationTest, "Golem.RPG.Assets.AuthoringRejection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGolemRPGAssetInvalidMutationTest::RunTest(const FString& Parameters)
{
	UGolemSkillDefinition* Force = GolemRPGAssetTests::Resolve<UGolemSkillDefinition>(this, TEXT("GolemSkill"), TEXT("HammerForce"));
	UGolemSkillDefinition* Focus = GolemRPGAssetTests::Resolve<UGolemSkillDefinition>(this, TEXT("GolemSkill"), TEXT("RuneFocus"));
	UGolemSkillDefinition* Impossible = GolemRPGAssetTests::Resolve<UGolemSkillDefinition>(this, TEXT("GolemSkill"), TEXT("ImpossibleCost"));
	UGolemSkillTreeDefinition* Tree = GolemRPGAssetTests::Resolve<UGolemSkillTreeDefinition>(this, TEXT("GolemSkillTree"), TEXT("Foundation"));
	UGolemLootTableDefinition* Loot = GolemRPGAssetTests::Resolve<UGolemLootTableDefinition>(this, TEXT("GolemLootTable"), TEXT("EncounterA"));
	if (Force == nullptr || Focus == nullptr || Impossible == nullptr || Tree == nullptr || Loot == nullptr)
	{
		return false;
	}
	TStrongObjectPtr<UGolemSkillDefinition> Copy(DuplicateObject<UGolemSkillDefinition>(Force, GetTransientPackage()));
	FGolemItemStatBonuses Invalid = Copy->GetStatBonuses();
	Invalid.AttackPower = std::numeric_limits<float>::quiet_NaN();
	TestEqual(TEXT("NaN skill upgrade rejected atomically"), Copy->TrySetSkillTuning(Copy->GetPointCost(), Copy->GetMaterialCosts(),
		Copy->GetPrerequisites(), Invalid).Error, EGolemError::InvalidData);
	TestEqual(TEXT("Rejected skill tuning retains actual AP"), Copy->GetStatBonuses().AttackPower, 5.0f);
	TestTrue(TEXT("Locally valid prerequisite can be authored for graph validation"), Copy->TrySetSkillTuning(Copy->GetPointCost(),
		Copy->GetMaterialCosts(), { Focus->GetPrimaryAssetId() }, Copy->GetStatBonuses()).IsSuccess());
	const TArray<const UGolemSkillDefinition*> CyclicNodes { Copy.Get(), Focus, Impossible };
	TestEqual(TEXT("Actual prerequisite cycle rejected by complete typed graph"), Tree->ValidateGraph(CyclicNodes).Error, EGolemError::InvalidData);
	TestTrue(TEXT("Saved graph remains unchanged after transient rejection"),
		Tree->ValidateGraph(TArray<const UGolemSkillDefinition*> { Force, Focus, Impossible }).IsSuccess());
	TStrongObjectPtr<UGolemLootTableDefinition> Edited(DuplicateObject<UGolemLootTableDefinition>(Loot, GetTransientPackage()));
	TArray<FGolemLootEntry> Entries = Edited->GetEntries();
	for (FGolemLootEntry& Entry : Entries)
	{
		if (Entry.DefinitionId == FPrimaryAssetId(TEXT("GolemItem"), TEXT("ForgeShard")))
		{
			Entry.MinimumCount = Entry.MaximumCount = 10000;
		}
	}
	TestTrue(TEXT("Pure table bounds allow domain maximum before typed content validation"), Edited->TrySetEntries(Entries).IsSuccess());
	TArray<FGolemItemInstance> Output;
	TestEqual(TEXT("Real resolved shard stack9999 rejects oversized loot"),
		Edited->GenerateLoot(305, FGuid(5, 3, 0, 2), Output).Error, EGolemError::InvalidData);
	TestTrue(TEXT("Failed typed generation publishes no partial drops"), Output.IsEmpty());
	TestEqual(TEXT("Saved loot remains valid and unchanged"), Loot->GetEntries().Num(), 3);
	return true;
}

#endif
