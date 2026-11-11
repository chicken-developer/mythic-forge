#include "Commandlets/GolemAIAssetAuthoring.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Assets/GolemAssetManager.h"
#include "Definitions/GolemAIActionSetDefinition.h"
#include "Definitions/GolemAIArchetypeDefinition.h"
#include "Definitions/GolemBossPhaseDefinition.h"
#include "EnvironmentQuery/EnvQuery.h"
#include "Misc/AutomationTest.h"
#include "StateTree.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGolemAIAuthoredAssetsTest, "Golem.AI.Assets.CompiledDecisionCatalog",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGolemAIAuthoredAssetsTest::RunTest(const FString& Parameters)
{
	if (!TestTrue(TEXT("Actual saved multi-state trees, bindings, EQS options and editor graph are valid"),
		GolemAIAssetAuthoring::ValidateAIAssets()))
	{
		AddError(TEXT("Run the integrated GolemBootstrap -Milestone=4 before this cold-load asset gate."));
		return false;
	}
	UGolemAssetManager* Manager = UGolemAssetManager::GetGolemAssetManager();
	if (!TestNotNull(TEXT("Project AssetManager exists"), Manager))
	{
		return false;
	}
	TestTrue(TEXT("Registry catalog validates all actual definitions"), Manager->ValidateDefinitionCatalog().IsSuccess());
	const TArray<FPrimaryAssetId> ArchetypeIds =
	{
		FPrimaryAssetId(TEXT("GolemAIArchetype"), TEXT("ArenaCompanion")),
		FPrimaryAssetId(TEXT("GolemAIArchetype"), TEXT("ArenaEnemy")),
		FPrimaryAssetId(TEXT("GolemAIArchetype"), TEXT("ArenaBoss"))
	};
	for (const FPrimaryAssetId& Id : ArchetypeIds)
	{
		UGolemDefinition* Definition = nullptr;
		if (!TestTrue(*FString::Printf(TEXT("Real typed archetype resolves %s"), *Id.ToString()),
			Manager->ResolveDefinition(Id, UGolemAIArchetypeDefinition::StaticClass(), Definition).IsSuccess()))
		{
			continue;
		}
		UGolemAIArchetypeDefinition* Archetype = CastChecked<UGolemAIArchetypeDefinition>(Definition);
		TestTrue(TEXT("Resolved archetype validates actual decision assets"), Archetype->ValidateArchetype().IsSuccess());
		UGolemDefinition* ActionSet = nullptr;
		TestTrue(TEXT("Archetype action set is an actual typed catalog definition"),
			Manager->ResolveDefinition(Archetype->GetActionSetId(),
				UGolemAIActionSetDefinition::StaticClass(), ActionSet).IsSuccess());
		UGolemDefinition* CombatProfile = nullptr;
		TestTrue(TEXT("Archetype retains existing real FoundationCombat identity"),
			Manager->ResolveDefinition(Archetype->GetCombatProfileId(), CombatProfile).IsSuccess());
	}
	UGolemDefinition* PhaseAsset = nullptr;
	if (TestTrue(TEXT("Two-phase boss definition resolves by stable ID"), Manager->ResolveDefinition(
		FPrimaryAssetId(TEXT("GolemBossPhase"), TEXT("ArenaBoss")),
		UGolemBossPhaseDefinition::StaticClass(), PhaseAsset).IsSuccess()))
	{
		UGolemBossPhaseDefinition* Phases = CastChecked<UGolemBossPhaseDefinition>(PhaseAsset);
		TestEqual(TEXT("Boss has exactly two authored phases"), Phases->GetPhases().Num(), 2);
		for (const FGolemBossPhaseEntry& Phase : Phases->GetPhases())
		{
			UGolemDefinition* ActionSet = nullptr;
			TestTrue(TEXT("Every phase references a real action set rather than a synthetic grant"),
				Manager->ResolveDefinition(Phase.ActionSetId,
					UGolemAIActionSetDefinition::StaticClass(), ActionSet).IsSuccess());
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGolemAIDefinitionRejectionTest, "Golem.AI.Assets.AuthoringRejectsInvalidData",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGolemAIDefinitionRejectionTest::RunTest(const FString& Parameters)
{
	UGolemAIArchetypeDefinition* SavedArchetype = LoadObject<UGolemAIArchetypeDefinition>(nullptr,
		TEXT("/Game/Golem/Definitions/AI/DA_Golem_Archetype_ArenaBoss.DA_Golem_Archetype_ArenaBoss"));
	UGolemBossPhaseDefinition* SavedPhases = LoadObject<UGolemBossPhaseDefinition>(nullptr,
		TEXT("/Game/Golem/Definitions/AI/DA_Golem_BossPhase_ArenaBoss.DA_Golem_BossPhase_ArenaBoss"));
	if (!TestNotNull(TEXT("Actual authored boss archetype exists"), SavedArchetype)
		|| !TestNotNull(TEXT("Actual authored boss phase definition exists"), SavedPhases))
	{
		return false;
	}
	TStrongObjectPtr<UGolemAIArchetypeDefinition> Archetype(
		DuplicateObject<UGolemAIArchetypeDefinition>(SavedArchetype, GetTransientPackage()));
	const FGolemAIArchetypeTuning OriginalTuning = Archetype->GetTuning();
	FGolemAIArchetypeTuning InvalidTuning = OriginalTuning;
	InvalidTuning.LoseSightRadius = OriginalTuning.SightRadius - 1.0f;
	TestFalse(TEXT("Invalid sight hysteresis is rejected"), Archetype->TrySetTuning(InvalidTuning).IsSuccess());
	TestEqual(TEXT("Rejected tuning preserves original lose-sight radius"),
		Archetype->GetTuning().LoseSightRadius, OriginalTuning.LoseSightRadius);
	InvalidTuning = OriginalTuning;
	InvalidTuning.AttackLeaseDuration = 0.0f;
	TestFalse(TEXT("Unbounded zero-duration attack capability is rejected"),
		Archetype->TrySetTuning(InvalidTuning).IsSuccess());
	const FPrimaryAssetId OriginalProfile = Archetype->GetCombatProfileId();
	const FPrimaryAssetId OriginalActionSet = Archetype->GetActionSetId();
	TestFalse(TEXT("Wrong definition type cannot replace a combat profile"), Archetype->TrySetIds(
		FPrimaryAssetId(TEXT("GolemHero"), TEXT("Training_Red")), OriginalActionSet).IsSuccess());
	TestTrue(TEXT("Rejected definition update preserves profile identity"), Archetype->GetCombatProfileId() == OriginalProfile);
	UStateTree* OriginalTree = Archetype->GetStateTree();
	TestFalse(TEXT("Absent decision asset cannot replace a compiled tree"),
		Archetype->TrySetDecisionAssets(nullptr, Archetype->GetRepositionQuery()).IsSuccess());
	TestTrue(TEXT("Rejected decision update preserves original tree"), Archetype->GetStateTree() == OriginalTree);
	TestTrue(TEXT("Archetype remains valid after all rejected mutations"), Archetype->ValidateArchetype().IsSuccess());
	TStrongObjectPtr<UGolemBossPhaseDefinition> Phases(
		DuplicateObject<UGolemBossPhaseDefinition>(SavedPhases, GetTransientPackage()));
	const TArray<FGolemBossPhaseEntry> OriginalPhases = Phases->GetPhases();
	TArray<FGolemBossPhaseEntry> InvalidPhases = OriginalPhases;
	InvalidPhases[1].HealthFraction = InvalidPhases[0].HealthFraction;
	TestFalse(TEXT("Equal boss thresholds are rejected"), Phases->TrySetPhases(InvalidPhases).IsSuccess());
	TestEqual(TEXT("Rejected thresholds preserve existing phase count"), Phases->GetPhases().Num(), OriginalPhases.Num());
	TestEqual(TEXT("Rejected thresholds preserve original second threshold"),
		Phases->GetPhases()[1].HealthFraction, OriginalPhases[1].HealthFraction);
	InvalidPhases = OriginalPhases;
	InvalidPhases[1].ActionSetId = FPrimaryAssetId(TEXT("GolemCombatProfile"), TEXT("FoundationCombat"));
	TestFalse(TEXT("Boss phase requires an AI action-set stable ID"), Phases->TrySetPhases(InvalidPhases).IsSuccess());
	TestTrue(TEXT("Rejected phase definition preserves its original stable action-set ID"),
		Phases->GetPhases()[1].ActionSetId == OriginalPhases[1].ActionSetId);
	TestTrue(TEXT("Boss remains valid after all rejected mutations"), Phases->ValidateBossPhases().IsSuccess());
	return true;
}

#endif
