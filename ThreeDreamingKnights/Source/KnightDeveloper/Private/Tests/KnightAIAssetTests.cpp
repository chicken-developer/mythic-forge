#include "Commandlets/KnightAIAssetAuthoring.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Assets/KnightAssetManager.h"
#include "Definitions/KnightAIActionSetDefinition.h"
#include "Definitions/KnightAIArchetypeDefinition.h"
#include "Definitions/KnightBossPhaseDefinition.h"
#include "EnvironmentQuery/EnvQuery.h"
#include "Misc/AutomationTest.h"
#include "StateTree.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKnightAIAuthoredAssetsTest, "Knight.AI.Assets.CompiledDecisionCatalog",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FKnightAIAuthoredAssetsTest::RunTest(const FString& Parameters)
{
	if (!TestTrue(TEXT("Actual saved multi-state trees, bindings, EQS options and editor graph are valid"),
		KnightAIAssetAuthoring::ValidateAIAssets()))
	{
		AddError(TEXT("Run the integrated KnightBootstrap -Milestone=4 before this cold-load asset gate."));
		return false;
	}
	UKnightAssetManager* Manager = UKnightAssetManager::GetKnightAssetManager();
	if (!TestNotNull(TEXT("Project AssetManager exists"), Manager))
	{
		return false;
	}
	TestTrue(TEXT("Registry catalog validates all actual definitions"), Manager->ValidateDefinitionCatalog().IsSuccess());
	const TArray<FPrimaryAssetId> ArchetypeIds =
	{
		FPrimaryAssetId(TEXT("KnightAIArchetype"), TEXT("ArenaCompanion")),
		FPrimaryAssetId(TEXT("KnightAIArchetype"), TEXT("ArenaEnemy")),
		FPrimaryAssetId(TEXT("KnightAIArchetype"), TEXT("ArenaBoss"))
	};
	for (const FPrimaryAssetId& Id : ArchetypeIds)
	{
		UKnightDefinition* Definition = nullptr;
		if (!TestTrue(*FString::Printf(TEXT("Real typed archetype resolves %s"), *Id.ToString()),
			Manager->ResolveDefinition(Id, UKnightAIArchetypeDefinition::StaticClass(), Definition).IsSuccess()))
		{
			continue;
		}
		UKnightAIArchetypeDefinition* Archetype = CastChecked<UKnightAIArchetypeDefinition>(Definition);
		TestTrue(TEXT("Resolved archetype validates actual decision assets"), Archetype->ValidateArchetype().IsSuccess());
		UKnightDefinition* ActionSet = nullptr;
		TestTrue(TEXT("Archetype action set is an actual typed catalog definition"),
			Manager->ResolveDefinition(Archetype->GetActionSetId(),
				UKnightAIActionSetDefinition::StaticClass(), ActionSet).IsSuccess());
		UKnightDefinition* CombatProfile = nullptr;
		TestTrue(TEXT("Archetype retains existing real FoundationCombat identity"),
			Manager->ResolveDefinition(Archetype->GetCombatProfileId(), CombatProfile).IsSuccess());
	}
	UKnightDefinition* PhaseAsset = nullptr;
	if (TestTrue(TEXT("Two-phase boss definition resolves by stable ID"), Manager->ResolveDefinition(
		FPrimaryAssetId(TEXT("KnightBossPhase"), TEXT("ArenaBoss")),
		UKnightBossPhaseDefinition::StaticClass(), PhaseAsset).IsSuccess()))
	{
		UKnightBossPhaseDefinition* Phases = CastChecked<UKnightBossPhaseDefinition>(PhaseAsset);
		TestEqual(TEXT("Boss has exactly two authored phases"), Phases->GetPhases().Num(), 2);
		for (const FKnightBossPhaseEntry& Phase : Phases->GetPhases())
		{
			UKnightDefinition* ActionSet = nullptr;
			TestTrue(TEXT("Every phase references a real action set rather than a synthetic grant"),
				Manager->ResolveDefinition(Phase.ActionSetId,
					UKnightAIActionSetDefinition::StaticClass(), ActionSet).IsSuccess());
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKnightAIDefinitionRejectionTest, "Knight.AI.Assets.AuthoringRejectsInvalidData",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FKnightAIDefinitionRejectionTest::RunTest(const FString& Parameters)
{
	UKnightAIArchetypeDefinition* SavedArchetype = LoadObject<UKnightAIArchetypeDefinition>(nullptr,
		TEXT("/Game/Knight/Definitions/AI/DA_Knight_Archetype_ArenaBoss.DA_Knight_Archetype_ArenaBoss"));
	UKnightBossPhaseDefinition* SavedPhases = LoadObject<UKnightBossPhaseDefinition>(nullptr,
		TEXT("/Game/Knight/Definitions/AI/DA_Knight_BossPhase_ArenaBoss.DA_Knight_BossPhase_ArenaBoss"));
	if (!TestNotNull(TEXT("Actual authored boss archetype exists"), SavedArchetype)
		|| !TestNotNull(TEXT("Actual authored boss phase definition exists"), SavedPhases))
	{
		return false;
	}
	TStrongObjectPtr<UKnightAIArchetypeDefinition> Archetype(
		DuplicateObject<UKnightAIArchetypeDefinition>(SavedArchetype, GetTransientPackage()));
	const FKnightAIArchetypeTuning OriginalTuning = Archetype->GetTuning();
	FKnightAIArchetypeTuning InvalidTuning = OriginalTuning;
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
		FPrimaryAssetId(TEXT("KnightHero"), TEXT("Training_Red")), OriginalActionSet).IsSuccess());
	TestTrue(TEXT("Rejected definition update preserves profile identity"), Archetype->GetCombatProfileId() == OriginalProfile);
	UStateTree* OriginalTree = Archetype->GetStateTree();
	TestFalse(TEXT("Absent decision asset cannot replace a compiled tree"),
		Archetype->TrySetDecisionAssets(nullptr, Archetype->GetRepositionQuery()).IsSuccess());
	TestTrue(TEXT("Rejected decision update preserves original tree"), Archetype->GetStateTree() == OriginalTree);
	TestTrue(TEXT("Archetype remains valid after all rejected mutations"), Archetype->ValidateArchetype().IsSuccess());
	TStrongObjectPtr<UKnightBossPhaseDefinition> Phases(
		DuplicateObject<UKnightBossPhaseDefinition>(SavedPhases, GetTransientPackage()));
	const TArray<FKnightBossPhaseEntry> OriginalPhases = Phases->GetPhases();
	TArray<FKnightBossPhaseEntry> InvalidPhases = OriginalPhases;
	InvalidPhases[1].HealthFraction = InvalidPhases[0].HealthFraction;
	TestFalse(TEXT("Equal boss thresholds are rejected"), Phases->TrySetPhases(InvalidPhases).IsSuccess());
	TestEqual(TEXT("Rejected thresholds preserve existing phase count"), Phases->GetPhases().Num(), OriginalPhases.Num());
	TestEqual(TEXT("Rejected thresholds preserve original second threshold"),
		Phases->GetPhases()[1].HealthFraction, OriginalPhases[1].HealthFraction);
	InvalidPhases = OriginalPhases;
	InvalidPhases[1].ActionSetId = FPrimaryAssetId(TEXT("KnightCombatProfile"), TEXT("FoundationCombat"));
	TestFalse(TEXT("Boss phase requires an AI action-set stable ID"), Phases->TrySetPhases(InvalidPhases).IsSuccess());
	TestTrue(TEXT("Rejected phase definition preserves its original stable action-set ID"),
		Phases->GetPhases()[1].ActionSetId == OriginalPhases[1].ActionSetId);
	TestTrue(TEXT("Boss remains valid after all rejected mutations"), Phases->ValidateBossPhases().IsSuccess());
	return true;
}

#endif
