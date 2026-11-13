#include "Commandlets/GolemAIAssetAuthoring.h"

#include "Assets/GolemAssetManager.h"
#include "Assets/GolemDefinition.h"
#include "Definitions/GolemAIActionSetDefinition.h"
#include "Definitions/GolemAIArchetypeDefinition.h"
#include "Definitions/GolemBossPhaseDefinition.h"
#include "StateTree/GolemAIStateTreeNodes.h"
#include "Tags/GolemGameplayTags.h"

#include "AIGraph.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "AssetToolsModule.h"
#include "Components/StateTreeAIComponentSchema.h"
#include "Conditions/StateTreeCommonConditions.h"
#include "EdGraph/EdGraphSchema.h"
#include "EnvironmentQuery/Contexts/EnvQueryContext_Querier.h"
#include "EnvironmentQuery/EnvQuery.h"
#include "EnvironmentQuery/EnvQueryGenerator.h"
#include "EnvironmentQuery/EnvQueryOption.h"
#include "EnvironmentQuery/Generators/EnvQueryGenerator_PathingGrid.h"
#include "EnvironmentQuery/Tests/EnvQueryTest_Distance.h"
#include "EnvironmentQuery/Tests/EnvQueryTest_Pathfinding.h"
#include "EnvironmentQuery/Tests/EnvQueryTest_Trace.h"
#include "Engine/EngineTypes.h"
#include "HAL/FileManager.h"
#include "IAssetTools.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "PropertyBindingBindingCollection.h"
#include "StateTree.h"
#include "StateTreeCompiler.h"
#include "StateTreeCompilerLog.h"
#include "StateTreeEditorData.h"
#include "StateTreeEditorModule.h"
#include "StateTreeEditorSchema.h"
#include "StateTreeState.h"
#include "UObject/ObjectRedirector.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

DEFINE_LOG_CATEGORY_STATIC(LogGolemAIAssetAuthoring, Log, All);

namespace GolemAIAssetAuthoring
{
	const TCHAR* QueryPackageName = TEXT("/Game/Golem/AI/EQS_Golem_CombatPosition");
	const TCHAR* CompanionTreePackageName = TEXT("/Game/Golem/AI/ST_Golem_Companion");
	const TCHAR* EnemyTreePackageName = TEXT("/Game/Golem/AI/ST_Golem_Enemy");
	const TCHAR* BossTreePackageName = TEXT("/Game/Golem/AI/ST_Golem_Boss");
	const TCHAR* LifecycleActionSetPackageName =
		TEXT("/Game/Golem/Definitions/AI/Tests/DA_Golem_AIActionSet_LifecyclePulse");
	const FPrimaryAssetId LifecycleActionSetId(TEXT("GolemAIActionSet"), TEXT("LifecyclePulse"));
	const FPrimaryAssetId CombatProfileId(TEXT("GolemCombatProfile"), TEXT("FoundationCombat"));
	constexpr float PositionGridHalfExtent = 650.0f;
	constexpr float PositionGridSpacing = 100.0f;
	constexpr float PositionMinimumRange = 120.0f;
	constexpr float PositionMaximumRange = 500.0f;
	constexpr float PositionPreferredRange = 125.0f;
	constexpr float RepositionAcceptanceRadius = 15.0f;

	bool SaveAsset(UObject* Asset)
	{
		UPackage* Package = Asset->GetPackage();
		const FString Filename = FPackageName::LongPackageNameToFilename(Package->GetName(),
			FPackageName::GetAssetPackageExtension());
		if (!IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true))
		{
			UE_LOG(LogGolemAIAssetAuthoring, Error, TEXT("Could not create output directory %s."), *Filename);
			return false;
		}
		Asset->SetFlags(RF_Public | RF_Standalone);
		Package->MarkPackageDirty();
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		SaveArgs.SaveFlags = SAVE_NoError;
		SaveArgs.bSlowTask = false;
		if (!UPackage::SavePackage(Package, Asset, *Filename, SaveArgs)
			|| IFileManager::Get().FileSize(*Filename) <= 0)
		{
			UE_LOG(LogGolemAIAssetAuthoring, Error, TEXT("Could not save genuine AI asset %s."), *Filename);
			return false;
		}
		UE_LOG(LogGolemAIAssetAuthoring, Display, TEXT("Saved %s (%lld bytes)."),
			*Filename, IFileManager::Get().FileSize(*Filename));
		return true;
	}

	template <typename TAsset>
	TAsset* FindOrCreateAsset(const TCHAR* PackageName, bool& bOutCreated)
	{
		bOutCreated = false;
		const bool bExists = FPackageName::DoesPackageExist(PackageName);
		UPackage* Package = bExists ? LoadPackage(nullptr, PackageName, LOAD_None) : CreatePackage(PackageName);
		if (Package == nullptr)
		{
			UE_LOG(LogGolemAIAssetAuthoring, Error, TEXT("Could not load package %s."), PackageName);
			return nullptr;
		}
		const FName AssetName(*FPackageName::GetLongPackageAssetName(PackageName));
		UObject* Existing = FindObject<UObject>(Package, *AssetName.ToString());
		if (Existing != nullptr || bExists)
		{
			TAsset* Asset = Cast<TAsset>(Existing);
			if (Asset == nullptr)
			{
				UE_LOG(LogGolemAIAssetAuthoring, Error, TEXT("Package %s has an incompatible/missing asset."), PackageName);
			}
			return Asset;
		}
		TAsset* Asset = NewObject<TAsset>(Package, AssetName, RF_Public | RF_Standalone);
		bOutCreated = Asset != nullptr;
		if (bOutCreated)
		{
			FAssetRegistryModule::AssetCreated(Asset);
		}
		return Asset;
	}

	bool ValidateCombatPositionQuery(const UEnvQuery* Query)
	{
		if (Query == nullptr || Query->GetOptions().Num() != 1)
		{
			UE_LOG(LogGolemAIAssetAuthoring, Error, TEXT("Combat position query needs exactly one complete option."));
			return false;
		}
		const UEnvQueryOption* Option = Query->GetOptions()[0];
		const UEnvQueryGenerator_PathingGrid* Grid = Option != nullptr
			? Cast<UEnvQueryGenerator_PathingGrid>(Option->Generator) : nullptr;
		if (Grid == nullptr || !Grid->IsValidGenerator() || Grid->GenerateAround == nullptr
			|| Grid->GridSize.DefaultValue <= 0.0f || Grid->SpaceBetween.DefaultValue < 50.0f
			|| Option->Tests.Num() != 3)
		{
			UE_LOG(LogGolemAIAssetAuthoring, Error, TEXT("Combat position query has invalid pathing-grid data."));
			return false;
		}
		const UEnvQueryTest_Pathfinding* Path = Cast<UEnvQueryTest_Pathfinding>(Option->Tests[0]);
		const UEnvQueryTest_Distance* Distance = Cast<UEnvQueryTest_Distance>(Option->Tests[1]);
		const UEnvQueryTest_Trace* Trace = Cast<UEnvQueryTest_Trace>(Option->Tests[2]);
		if (Path == nullptr || Distance == nullptr || Trace == nullptr
			|| Path->TestMode != EEnvTestPathfinding::PathExist || !Path->BoolValue.DefaultValue
			|| Path->TestPurpose != EEnvTestPurpose::Filter || Path->Context != UEnvQueryContext_Querier::StaticClass()
			|| Distance->TestMode != EEnvTestDistance::Distance2D
			|| Distance->TestPurpose != EEnvTestPurpose::FilterAndScore
			|| Distance->FloatValueMin.DefaultValue < 80.0f
			|| Distance->FloatValueMax.DefaultValue <= Distance->FloatValueMin.DefaultValue
			|| Distance->VerNum < EnvQueryTestVersion::ReferenceValueFix || !Distance->bDefineReferenceValue
			|| !FMath::IsFinite(Distance->ReferenceValue.DefaultValue)
			|| Distance->ReferenceValue.DefaultValue < Distance->FloatValueMin.DefaultValue
			|| Distance->ReferenceValue.DefaultValue > Distance->FloatValueMax.DefaultValue
			|| !FMath::IsFinite(Distance->ScoringFactor.DefaultValue) || Distance->ScoringFactor.DefaultValue <= 0.0f
			|| Distance->ScoringEquation != EEnvTestScoreEquation::Linear
			|| Distance->DistanceTo != Grid->GenerateAround || Trace->Context != Grid->GenerateAround
			|| Trace->TestPurpose != EEnvTestPurpose::Filter || Trace->BoolValue.DefaultValue
			|| Trace->TraceData.TraceMode != EEnvQueryTrace::GeometryByChannel
			|| Trace->TraceData.TraceChannel != UEngineTypes::ConvertToTraceType(ECC_Visibility))
		{
			UE_LOG(LogGolemAIAssetAuthoring, Error, TEXT("Combat position query lost path/range/LOS filters."));
			return false;
		}
		for (const UEnvQueryTest* Test : Option->Tests)
		{
			if (Test == nullptr || !Test->IsSupportedItem(Grid->ItemType) || !Test->IsIn(Query))
			{
				UE_LOG(LogGolemAIAssetAuthoring, Error, TEXT("Combat position test has invalid item type/ownership."));
				return false;
			}
		}
		if (Query->EdGraph == nullptr || Query->EdGraph->Nodes.Num() < 2)
		{
			UE_LOG(LogGolemAIAssetAuthoring, Error, TEXT("Combat position query lacks its genuine editor graph."));
			return false;
		}
		return true;
	}

	UEnvQuery* CreateCombatPositionQuery()
	{
		bool bCreated = false;
		UEnvQuery* Query = FindOrCreateAsset<UEnvQuery>(QueryPackageName, bCreated);
		if (Query == nullptr)
		{
			return nullptr;
		}
		if (bCreated)
		{
			UClass* TargetContext = LoadClass<UEnvQueryContext>(nullptr,
				TEXT("/Script/GolemAI.GolemEnvQueryContext_Target"));
			if (TargetContext == nullptr)
			{
				UE_LOG(LogGolemAIAssetAuthoring, Error, TEXT("Native AI target EQS context is unavailable."));
				return nullptr;
			}
			UEnvQueryOption* Option = NewObject<UEnvQueryOption>(Query, TEXT("CombatPosition"), RF_Transactional);
			UEnvQueryGenerator_PathingGrid* Grid = NewObject<UEnvQueryGenerator_PathingGrid>(
				Option, TEXT("ReachableTargetGrid"), RF_Transactional);
			Grid->GridSize.DefaultValue = PositionGridHalfExtent;
			Grid->SpaceBetween.DefaultValue = PositionGridSpacing;
			Grid->GenerateAround = TargetContext;
			Grid->PathToItem.DefaultValue = true;
			Grid->ScanRangeMultiplier.DefaultValue = 2.0f;
			Grid->ProjectionData.TraceMode = EEnvQueryTrace::Navigation;
			Grid->ProjectionData.ProjectDown = 150.0f;
			Grid->ProjectionData.ProjectUp = 150.0f;
			Option->Generator = Grid;
			UEnvQueryTest_Pathfinding* Path = NewObject<UEnvQueryTest_Pathfinding>(Option,
				TEXT("PathFromQuerier"), RF_Transactional);
			Path->TestOrder = 0;
			Path->TestMode = EEnvTestPathfinding::PathExist;
			Path->Context = UEnvQueryContext_Querier::StaticClass();
			Path->PathFromContext.DefaultValue = true;
			Path->SkipUnreachable.DefaultValue = true;
			Path->BoolValue.DefaultValue = true;
			Path->TestPurpose = EEnvTestPurpose::Filter;
			Path->FilterType = EEnvTestFilterType::Match;
			UEnvQueryTest_Distance* Distance = NewObject<UEnvQueryTest_Distance>(Option,
				TEXT("TargetMeleeRing"), RF_Transactional);
			Distance->TestOrder = 1;
			Distance->TestMode = EEnvTestDistance::Distance2D;
			Distance->DistanceTo = TargetContext;
			Distance->TestPurpose = EEnvTestPurpose::FilterAndScore;
			Distance->FilterType = EEnvTestFilterType::Range;
			Distance->FloatValueMin.DefaultValue = PositionMinimumRange;
			Distance->FloatValueMax.DefaultValue = PositionMaximumRange;
			Distance->bDefineReferenceValue = true;
			Distance->ReferenceValue.DefaultValue = PositionPreferredRange;
			Distance->ScoringFactor.DefaultValue = 1.0f;
			Distance->ScoringEquation = EEnvTestScoreEquation::Linear;
			UEnvQueryTest_Trace* Trace = NewObject<UEnvQueryTest_Trace>(Option,
				TEXT("UnobstructedTargetLOS"), RF_Transactional);
			Trace->TestOrder = 2;
			Trace->Context = TargetContext;
			Trace->TestPurpose = EEnvTestPurpose::Filter;
			Trace->FilterType = EEnvTestFilterType::Match;
			Trace->BoolValue.DefaultValue = false;
			Trace->TraceFromContext.DefaultValue = false;
			Trace->ItemHeightOffset.DefaultValue = 70.0f;
			Trace->ContextHeightOffset.DefaultValue = 0.0f;
			Trace->TraceData.TraceMode = EEnvQueryTrace::GeometryByChannel;
			Trace->TraceData.TraceShape = EEnvTraceShape::Line;
			Trace->TraceData.TraceChannel = UEngineTypes::ConvertToTraceType(ECC_Visibility);
			Option->Tests = { Path, Distance, Trace };
			// Existing NodeInstance bypasses the graph's new-node initialization; persist current scoring semantics.
			for (UEnvQueryTest* Test : Option->Tests)
			{
				Test->UpdateNodeVersion();
			}
			Query->GetOptionsMutable().Add(Option);
			FModuleManager::Get().LoadModuleChecked<IModuleInterface>(TEXT("EnvironmentQueryEditor"));
			UClass* GraphClass = LoadClass<UAIGraph>(nullptr,
				TEXT("/Script/EnvironmentQueryEditor.EnvironmentQueryGraph"));
			if (GraphClass == nullptr)
			{
				UE_LOG(LogGolemAIAssetAuthoring, Error, TEXT("Installed EQS editor graph factory class is unavailable."));
				return nullptr;
			}
			UAIGraph* Graph = NewObject<UAIGraph>(Query, GraphClass, NAME_None, RF_Transactional);
			Query->EdGraph = Graph;
			const UEdGraphSchema* Schema = Graph->GetSchema();
			if (Schema == nullptr)
			{
				UE_LOG(LogGolemAIAssetAuthoring, Error, TEXT("EQS graph has no native schema."));
				return nullptr;
			}
			Schema->CreateDefaultNodesForGraph(*Graph);
			Graph->OnCreated();
			Graph->Initialize();
			Graph->UpdateAsset();
		}
		if (!ValidateCombatPositionQuery(Query) || !SaveAsset(Query))
		{
			return nullptr;
		}
		return Query;
	}

	void AddCondition(UStateTreeState& State, const EGolemAIContextConditionKind Kind,
		const bool bDesiredValue = true)
	{
		TStateTreeEditorNode<FGolemAIContextCondition>& Condition = State.AddEnterCondition<FGolemAIContextCondition>();
		Condition.GetNode().Kind = Kind;
		Condition.GetNode().bDesiredValue = bDesiredValue;
	}

	void AddInterrupt(UStateTreeState& State, UStateTreeState& Root, const EGolemAIContextConditionKind Kind,
		const bool bDesiredValue = true)
	{
		FStateTreeTransition& Transition = State.AddTransition(EStateTreeTransitionTrigger::OnTick,
			EStateTreeTransitionType::GotoState, &Root);
		Transition.Priority = EStateTreeTransitionPriority::High;
		TStateTreeEditorNode<FGolemAIContextCondition>& Condition =
			Transition.AddConditionWithOuter<FGolemAIContextCondition>(&State);
		Condition.GetNode().Kind = Kind;
		Condition.GetNode().bDesiredValue = bDesiredValue;
	}

	void AddCompletion(UStateTreeState& State, UStateTreeState& Root, UStateTreeState& Cooldown)
	{
		State.AddTransition(EStateTreeTransitionTrigger::OnStateSucceeded,
			EStateTreeTransitionType::GotoState, &Root);
		State.AddTransition(EStateTreeTransitionTrigger::OnStateFailed,
			EStateTreeTransitionType::GotoState, &Cooldown);
	}

	bool ValidateDecisionTree(UStateTree* Tree, const bool bCompanion)
	{
		const UStateTreeEditorData* Data = Tree != nullptr ? Cast<UStateTreeEditorData>(Tree->EditorData) : nullptr;
		if (Data == nullptr || Data->Schema == nullptr || !Data->Schema->IsA<UStateTreeAIComponentSchema>()
			|| Data->Evaluators.Num() != 1
			|| Data->Evaluators[0].Node.GetScriptStruct() != FGolemAIDecisionEvaluator::StaticStruct()
			|| !Tree->IsReadyToRun() || Tree->GetStates().Num() < (bCompanion ? 10 : 6)
			|| static_cast<const FPropertyBindingBindingCollection&>(Tree->GetPropertyBindings()).GetNumBindings() < 1)
		{
			UE_LOG(LogGolemAIAssetAuthoring, Error, TEXT("Decision tree lost compiled states/evaluator/bindings/schema."));
			return false;
		}
		bool bHasAction = false;
		bool bHasReposition = false;
		bool bHasAcquire = false;
		bool bHasRevive = false;
		bool bHasFormation = false;
		bool bHasCooldown = false;
		bool bHasConditionalTransition = false;
		Data->VisitHierarchy([&](UStateTreeState& State, UStateTreeState* Parent)
		{
			for (const FStateTreeEditorNode& Task : State.Tasks)
			{
				const UScriptStruct* Type = Task.Node.GetScriptStruct();
				bHasReposition |= Type == FGolemAIRepositionTask::StaticStruct();
				bHasAcquire |= Type == FGolemAISelectTargetTask::StaticStruct();
				bHasFormation |= Type == FGolemAIFormationTask::StaticStruct();
				bHasCooldown |= Type == FGolemAIWaitTask::StaticStruct();
				if (Type == FGolemAIExecuteActionTask::StaticStruct())
				{
					bHasRevive |= Task.Node.Get<FGolemAIExecuteActionTask>().bRevive;
					bHasAction |= !Task.Node.Get<FGolemAIExecuteActionTask>().bRevive;
				}
			}
			for (const FStateTreeTransition& Transition : State.Transitions)
			{
				bHasConditionalTransition |= !Transition.Conditions.IsEmpty();
			}
			return EStateTreeVisitor::Continue;
		});
		if (!bHasAction || !bHasReposition || !bHasAcquire || !bHasCooldown
			|| (bCompanion && (!bHasRevive || !bHasFormation || !bHasConditionalTransition)))
		{
			UE_LOG(LogGolemAIAssetAuthoring, Error, TEXT("Decision tree lacks required native decision/action branches."));
			return false;
		}
		return true;
	}

	UStateTree* CreateDecisionTree(const TCHAR* PackageName, const bool bCompanion)
	{
		bool bCreated = false;
		UStateTree* Tree = FindOrCreateAsset<UStateTree>(PackageName, bCreated);
		if (Tree == nullptr)
		{
			return nullptr;
		}
		if (bCreated)
		{
			FStateTreeEditorModule& Module = FStateTreeEditorModule::GetModule();
			UStateTreeEditorData* Data = NewObject<UStateTreeEditorData>(Tree,
				Module.GetEditorDataClass(UStateTreeAIComponentSchema::StaticClass()), NAME_None, RF_Transactional);
			Tree->EditorData = Data;
			Data->Schema = NewObject<UStateTreeAIComponentSchema>(Data, NAME_None, RF_Transactional);
			Data->EditorSchema = NewObject<UStateTreeEditorSchema>(Data,
				Module.GetEditorSchemaClass(UStateTreeAIComponentSchema::StaticClass()), NAME_None, RF_Transactional);
			TStateTreeEditorNode<FGolemAIDecisionEvaluator>& Evaluator = Data->AddEvaluator<FGolemAIDecisionEvaluator>();
			Evaluator.SetNodeName(TEXT("ObserveControlledPawn"));
			const FGuid EvaluatorId = Evaluator.ID;
			UStateTreeState& Root = Data->AddRootState();
			Root.Name = TEXT("SelectDecision");
			Root.SelectionBehavior = EStateTreeStateSelectionBehavior::TrySelectChildrenInOrder;
			TStateTreeEditorNode<FStateTreeCompareBoolCondition>& Alive = Root.AddEnterCondition<FStateTreeCompareBoolCondition>();
			Alive.GetInstanceData().bRight = true;
			// Bind real evaluator output rather than duplicating sensor reads in the authored root condition.
			FPropertyBindingPath SourcePath(EvaluatorId);
			FPropertyBindingPath TargetPath(Alive.ID);
			if (!SourcePath.FromString(TEXT("Snapshot.bAlive")) || !TargetPath.FromString(TEXT("bLeft")))
			{
				UE_LOG(LogGolemAIAssetAuthoring, Error, TEXT("Could not create typed alive property binding."));
				return nullptr;
			}
			Data->AddPropertyBinding(SourcePath, TargetPath);
			UStateTreeState& Stagger = Root.AddChildState(TEXT("StaggerWait"));
			AddCondition(Stagger, EGolemAIContextConditionKind::Staggered);
			Stagger.AddTask<FGolemAIWaitTask>().GetNode().Duration = 0.2f;
			UStateTreeState* Revive = nullptr;
			UStateTreeState* ApproachRevive = nullptr;
			UStateTreeState* Regroup = nullptr;
			if (bCompanion)
			{
				Revive = &Root.AddChildState(TEXT("RevivePartyGolem"));
				AddCondition(*Revive, EGolemAIContextConditionKind::CanRevive);
				TStateTreeEditorNode<FGolemAIExecuteActionTask>& ReviveTask = Revive->AddTask<FGolemAIExecuteActionTask>();
				ReviveTask.GetNode().bRevive = true;
				ReviveTask.GetNode().bUseAttackLease = false;
				ApproachRevive = &Root.AddChildState(TEXT("ApproachDownedPartyGolem"));
				AddCondition(*ApproachRevive, EGolemAIContextConditionKind::FriendlyDownedTarget);
				TStateTreeEditorNode<FGolemAIRepositionTask>& ApproachTask =
					ApproachRevive->AddTask<FGolemAIRepositionTask>();
				ApproachTask.GetNode().bMoveToReviveTarget = true;
				ApproachTask.GetNode().AcceptanceRadius = RepositionAcceptanceRadius;
				Regroup = &Root.AddChildState(TEXT("RegroupFormation"));
				AddCondition(*Regroup, EGolemAIContextConditionKind::NeedsRegroup);
				Regroup->AddTask<FGolemAIFormationTask>().GetNode().bCompleteWhenReached = true;
			}
			UStateTreeState& Attack = Root.AddChildState(TEXT("ExecuteSharedCombatAbility"));
			AddCondition(Attack, EGolemAIContextConditionKind::CanSelectAction);
			Attack.AddTask<FGolemAIExecuteActionTask>();
			UStateTreeState& Reposition = Root.AddChildState(TEXT("FindReachableCombatPosition"));
			AddCondition(Reposition, EGolemAIContextConditionKind::HasTarget);
			AddCondition(Reposition, EGolemAIContextConditionKind::HoldPosition, false);
			Reposition.AddTask<FGolemAIRepositionTask>().GetNode().AcceptanceRadius = RepositionAcceptanceRadius;
			UStateTreeState* Hold = nullptr;
			UStateTreeState* Follow = nullptr;
			if (bCompanion)
			{
				Hold = &Root.AddChildState(TEXT("HoldCommandPosition"));
				AddCondition(*Hold, EGolemAIContextConditionKind::HoldPosition);
				Hold->AddTask<FGolemAIWaitTask>().GetNode().Duration = 0.25f;
				Follow = &Root.AddChildState(TEXT("FollowPartyFormation"));
				AddCondition(*Follow, EGolemAIContextConditionKind::HasFormationGoal);
				Follow->AddTask<FGolemAIFormationTask>();
			}
			UStateTreeState& Acquire = Root.AddChildState(TEXT("AcquirePerceivedTarget"));
			Acquire.AddTask<FGolemAISelectTargetTask>();
			Acquire.AddTask<FGolemAIWaitTask>().GetNode().Duration = 0.35f;
			UStateTreeState& Cooldown = Root.AddChildState(TEXT("BoundedFailureCooldown"));
			Cooldown.AddTask<FGolemAIWaitTask>().GetNode().Duration = 0.35f;
			for (UStateTreeState* State : Root.Children)
			{
				AddCompletion(*State, Root, Cooldown);
			}
			if (bCompanion)
			{
				AddInterrupt(*Follow, Root, EGolemAIContextConditionKind::FriendlyDownedTarget);
				AddInterrupt(*Follow, Root, EGolemAIContextConditionKind::HasTarget);
				AddInterrupt(*Follow, Root, EGolemAIContextConditionKind::HoldPosition);
				AddInterrupt(*Follow, Root, EGolemAIContextConditionKind::NeedsRegroup);
				AddInterrupt(*Regroup, Root, EGolemAIContextConditionKind::FriendlyDownedTarget);
				AddInterrupt(*ApproachRevive, Root, EGolemAIContextConditionKind::CanRevive);
				AddInterrupt(Reposition, Root, EGolemAIContextConditionKind::FriendlyDownedTarget);
				AddInterrupt(Reposition, Root, EGolemAIContextConditionKind::HoldPosition);
			}
		}
		FStateTreeCompilerLog CompilerLog;
		FStateTreeCompiler Compiler(CompilerLog);
		const bool bCompiled = Compiler.Compile(*Tree);
		CompilerLog.DumpToLog(Tree, LogGolemAIAssetAuthoring);
		if (!bCompiled || !ValidateDecisionTree(Tree, bCompanion) || !SaveAsset(Tree))
		{
			return nullptr;
		}
		return Tree;
	}

	FGolemAIActionEntry MakeAttack(const FName Key, const FGameplayTag ActionTag, const int32 Priority,
		const float Cooldown, const float StaminaCost)
	{
		FGolemAIActionEntry Entry;
		Entry.ActionKey = Key;
		Entry.ActionTag = ActionTag;
		Entry.MinRange = 0.0f;
		Entry.MaxRange = 210.0f;
		Entry.CooldownSeconds = Cooldown;
		Entry.Priority = Priority;
		Entry.RequiredStateTags.AddTag(GolemGameplayTags::State_Life_Alive);
		Entry.BlockedStateTags.AddTag(GolemGameplayTags::State_Combat_Attacking);
		Entry.BlockedStateTags.AddTag(GolemGameplayTags::State_Combat_Blocking);
		Entry.BlockedStateTags.AddTag(GolemGameplayTags::State_Combat_Dodging);
		Entry.BlockedStateTags.AddTag(GolemGameplayTags::State_Combat_Staggered);
		Entry.BlockedStateTags.AddTag(GolemGameplayTags::State_Life_Reviving);
		return Entry;
	}

	bool CreateActionSet(const TCHAR* DefinitionName, const bool bHeavy, const float AttackCooldown)
	{
		const FString PackageName = FString::Printf(TEXT("/Game/Golem/Definitions/AI/DA_Golem_ActionSet_%s"),
			DefinitionName);
		bool bCreated = false;
		UGolemAIActionSetDefinition* Definition = FindOrCreateAsset<UGolemAIActionSetDefinition>(*PackageName, bCreated);
		if (Definition == nullptr)
		{
			return false;
		}
		if (bCreated)
		{
			TArray<FGolemAIActionEntry> Actions;
			Actions.Add(MakeAttack(TEXT("HammerLight"), GolemGameplayTags::Ability_Combat_Primary,
				10, AttackCooldown, 8.0f));
			if (bHeavy)
			{
				Actions.Add(MakeAttack(TEXT("HammerHeavy"), GolemGameplayTags::Ability_Combat_Heavy,
					20, 3.0f, 15.0f));
			}
			if (!Definition->TrySetIdentity(FName(DefinitionName), TEXT("GolemAIActionSet"))
				|| !Definition->TrySetActions(Actions).IsSuccess())
			{
				UE_LOG(LogGolemAIAssetAuthoring, Error, TEXT("Could not author validated action set %s."), DefinitionName);
				return false;
			}
		}
		return Definition->GetPrimaryAssetId() == FPrimaryAssetId(TEXT("GolemAIActionSet"), DefinitionName)
			&& Definition->ValidateActionSet().IsSuccess() && SaveAsset(Definition);
	}

	bool CreateArchetype(const TCHAR* Name, const TCHAR* ActionSetName, UStateTree* Tree, UEnvQuery* Query,
		const float MaxHealth, const float AttackPower)
	{
		const FString PackageName = FString::Printf(TEXT("/Game/Golem/Definitions/AI/DA_Golem_Archetype_%s"), Name);
		bool bCreated = false;
		UGolemAIArchetypeDefinition* Definition = FindOrCreateAsset<UGolemAIArchetypeDefinition>(*PackageName, bCreated);
		if (Definition == nullptr)
		{
			return false;
		}
		if (bCreated)
		{
			FGolemAIArchetypeTuning Tuning;
			Tuning.Attributes.MaxHealth = MaxHealth;
			Tuning.Attributes.Power = AttackPower;
			Tuning.WalkSpeed = 360.0f;
			Tuning.SightAngle = 180.0f;
			if (!Definition->TrySetIdentity(FName(Name), TEXT("GolemAIArchetype"))
				|| !Definition->TrySetTuning(Tuning).IsSuccess()
				|| !Definition->TrySetIds(CombatProfileId,
					FPrimaryAssetId(TEXT("GolemAIActionSet"), ActionSetName)).IsSuccess()
				|| !Definition->TrySetDecisionAssets(Tree, Query).IsSuccess())
			{
				UE_LOG(LogGolemAIAssetAuthoring, Error, TEXT("Could not author validated AI archetype %s."), Name);
				return false;
			}
		}
		return Definition->GetPrimaryAssetId() == FPrimaryAssetId(TEXT("GolemAIArchetype"), Name)
			&& Definition->ValidateArchetype().IsSuccess() && SaveAsset(Definition);
	}

	bool MigrateLifecycleTestActionSet()
	{
		const TCHAR* PreviousPackageName = TEXT("/Game/Golem/AI/Tests/DA_Golem_AIActionSet_LifecyclePulse");
		if (!FPackageName::DoesPackageExist(PreviousPackageName))
		{
			return true;
		}
		UPackage* PreviousPackage = LoadPackage(nullptr, PreviousPackageName, LOAD_None);
		UObject* PreviousAsset = PreviousPackage != nullptr ? FindObject<UObject>(PreviousPackage,
			TEXT("DA_Golem_AIActionSet_LifecyclePulse")) : nullptr;
		if (const UObjectRedirector* Redirector = Cast<UObjectRedirector>(PreviousAsset))
		{
			const UGolemAIActionSetDefinition* Destination = Cast<UGolemAIActionSetDefinition>(Redirector->DestinationObject);
			if (Destination == nullptr || Destination->GetClass() != UGolemAIActionSetDefinition::StaticClass()
				|| Destination->GetPackage()->GetName() != LifecycleActionSetPackageName
				|| Destination->GetPrimaryAssetId() != LifecycleActionSetId || !Destination->ValidateActionSet().IsSuccess())
			{
				UE_LOG(LogGolemAIAssetAuthoring, Error,
					TEXT("Lifecycle fixture redirector does not target the expected validated catalog definition."));
				return false;
			}
			return true;
		}
		UGolemAIActionSetDefinition* Definition = Cast<UGolemAIActionSetDefinition>(PreviousAsset);
		if (Definition == nullptr || Definition->GetClass() != UGolemAIActionSetDefinition::StaticClass()
			|| Definition->GetPrimaryAssetId() != LifecycleActionSetId || !Definition->ValidateActionSet().IsSuccess()
			|| FPackageName::DoesPackageExist(LifecycleActionSetPackageName))
		{
			UE_LOG(LogGolemAIAssetAuthoring, Error,
				TEXT("Lifecycle fixture migration found unexpected source identity/type or an occupied destination; preserving both."));
			return false;
		}
		FAssetRegistryModule::GetRegistry().WaitForCompletion();
		const TArray<FAssetRenameData> Renames
		{
			FAssetRenameData(Definition, FPackageName::GetLongPackagePath(LifecycleActionSetPackageName),
				FPackageName::GetLongPackageAssetName(LifecycleActionSetPackageName))
		};
		FAssetToolsModule& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
		if (!AssetTools.Get().RenameAssets(Renames) || Definition->GetPackage()->GetName() != LifecycleActionSetPackageName
			|| Definition->GetPrimaryAssetId() != LifecycleActionSetId || !SaveAsset(Definition))
		{
			UE_LOG(LogGolemAIAssetAuthoring, Error, TEXT("Supported AssetTools migration did not persist the lifecycle definition."));
			return false;
		}
		UE_LOG(LogGolemAIAssetAuthoring, Display, TEXT("Migrated owned lifecycle fixture into the production definition catalog: %s."),
			*Definition->GetPathName());
		return true;
	}

	bool CreateLifecycleTestActionSet()
	{
		if (!MigrateLifecycleTestActionSet())
		{
			return false;
		}
		bool bCreated = false;
		UGolemAIActionSetDefinition* Definition = FindOrCreateAsset<UGolemAIActionSetDefinition>(
			LifecycleActionSetPackageName, bCreated);
		if (Definition == nullptr)
		{
			return false;
		}
		if (bCreated)
		{
			FGolemAIActionEntry Pulse;
			Pulse.ActionKey = TEXT("KeyPulse");
			Pulse.ActionTag = GolemGameplayTags::Ability_Test_Pulse;
			Pulse.MaxRange = 1000.0f;
			Pulse.CooldownSeconds = 1.0f;
			Pulse.Priority = 1;
			Pulse.RequiredStateTags.AddTag(GolemGameplayTags::State_Life_Alive);
			Pulse.BlockedStateTags.AddTag(GolemGameplayTags::State_Life_Downed);
			Pulse.BlockedStateTags.AddTag(GolemGameplayTags::State_Life_Dead);
			if (!Definition->TrySetIdentity(TEXT("LifecyclePulse"), TEXT("GolemAIActionSet"))
				|| !Definition->TrySetActions({ Pulse }).IsSuccess())
			{
				UE_LOG(LogGolemAIAssetAuthoring, Error, TEXT("Could not author the genuine instant-GAS lifecycle fixture."));
				return false;
			}
		}
		return Definition->GetPrimaryAssetId() == LifecycleActionSetId
			&& Definition->ValidateActionSet().IsSuccess() && SaveAsset(Definition);
	}

	bool CreateLifecycleTree(const TCHAR* PackageName, const bool bRevive, const bool bReposition)
	{
		bool bCreated = false;
		UStateTree* Tree = FindOrCreateAsset<UStateTree>(PackageName, bCreated);
		if (Tree == nullptr)
		{
			return false;
		}
		if (bCreated)
		{
			FStateTreeEditorModule& Module = FStateTreeEditorModule::GetModule();
			UStateTreeEditorData* Data = NewObject<UStateTreeEditorData>(Tree,
				Module.GetEditorDataClass(UStateTreeAIComponentSchema::StaticClass()), NAME_None, RF_Transactional);
			Tree->EditorData = Data;
			Data->Schema = NewObject<UStateTreeAIComponentSchema>(Data, NAME_None, RF_Transactional);
			Data->EditorSchema = NewObject<UStateTreeEditorSchema>(Data,
				Module.GetEditorSchemaClass(UStateTreeAIComponentSchema::StaticClass()), NAME_None, RF_Transactional);
			UStateTreeState& Root = Data->AddRootState();
			Root.Name = TEXT("SingleRealTaskLifecycle");
			Root.AddTransition(EStateTreeTransitionTrigger::OnStateSucceeded, EStateTreeTransitionType::Succeeded);
			Root.AddTransition(EStateTreeTransitionTrigger::OnStateFailed, EStateTreeTransitionType::Failed);
			if (bReposition)
			{
				Root.AddTask<FGolemAIRepositionTask>().GetNode().AcceptanceRadius = RepositionAcceptanceRadius;
			}
			else
			{
				TStateTreeEditorNode<FGolemAIExecuteActionTask>& Task = Root.AddTask<FGolemAIExecuteActionTask>();
				Task.GetNode().bRevive = bRevive;
				Task.GetNode().bUseAttackLease = !bRevive;
			}
		}
		const UStateTreeEditorData* Data = Cast<UStateTreeEditorData>(Tree->EditorData);
		if (Data == nullptr || !Data->Evaluators.IsEmpty() || Data->SubTrees.Num() != 1
			|| Data->SubTrees[0] == nullptr || Data->SubTrees[0]->Tasks.Num() != 1
			|| Data->SubTrees[0]->Transitions.Num() != 2)
		{
			UE_LOG(LogGolemAIAssetAuthoring, Error, TEXT("Lifecycle tree requires one genuine task and no evaluator/loop."));
			return false;
		}
		const TArray<FStateTreeTransition>& Transitions = Data->SubTrees[0]->Transitions;
		if (Transitions[0].Trigger != EStateTreeTransitionTrigger::OnStateSucceeded
			|| Transitions[0].State.LinkType != EStateTreeTransitionType::Succeeded
			|| Transitions[1].Trigger != EStateTreeTransitionTrigger::OnStateFailed
			|| Transitions[1].State.LinkType != EStateTreeTransitionType::Failed)
		{
			UE_LOG(LogGolemAIAssetAuthoring, Error, TEXT("Lifecycle tree lacks explicit terminal completion transitions."));
			return false;
		}
		const FStateTreeEditorNode& Task = Data->SubTrees[0]->Tasks[0];
		if (bReposition ? Task.Node.GetScriptStruct() != FGolemAIRepositionTask::StaticStruct()
			: (Task.Node.GetScriptStruct() != FGolemAIExecuteActionTask::StaticStruct()
				|| Task.Node.Get<FGolemAIExecuteActionTask>().bRevive != bRevive
				|| Task.Node.Get<FGolemAIExecuteActionTask>().bUseAttackLease == bRevive))
		{
			UE_LOG(LogGolemAIAssetAuthoring, Error, TEXT("Lifecycle tree has another task configuration; preserving it."));
			return false;
		}
		FStateTreeCompilerLog CompilerLog;
		FStateTreeCompiler Compiler(CompilerLog);
		const bool bCompiled = Compiler.Compile(*Tree);
		CompilerLog.DumpToLog(Tree, LogGolemAIAssetAuthoring);
		return bCompiled && Tree->IsReadyToRun() && SaveAsset(Tree);
	}

	bool CreateBossPhases()
	{
		bool bCreated = false;
		UGolemBossPhaseDefinition* Definition = FindOrCreateAsset<UGolemBossPhaseDefinition>(
			TEXT("/Game/Golem/Definitions/AI/DA_Golem_BossPhase_ArenaBoss"), bCreated);
		if (Definition == nullptr)
		{
			return false;
		}
		if (bCreated)
		{
			FGolemBossPhaseEntry First;
			First.PhaseKey = TEXT("MeasuredHammer");
			First.HealthFraction = 1.0f;
			First.ActionSetId = FPrimaryAssetId(TEXT("GolemAIActionSet"), TEXT("ArenaBossPhaseOne"));
			First.DecisionInterval = 0.15f;
			FGolemBossPhaseEntry Second;
			Second.PhaseKey = TEXT("HeavyPressure");
			Second.HealthFraction = 0.5f;
			Second.ActionSetId = FPrimaryAssetId(TEXT("GolemAIActionSet"), TEXT("ArenaBossPhaseTwo"));
			Second.DecisionInterval = 0.1f;
			if (!Definition->TrySetIdentity(TEXT("ArenaBoss"), TEXT("GolemBossPhase"))
				|| !Definition->TrySetPhases({ First, Second }).IsSuccess())
			{
				UE_LOG(LogGolemAIAssetAuthoring, Error, TEXT("Could not author validated two-phase boss definition."));
				return false;
			}
		}
		return Definition->GetPrimaryAssetId() == FPrimaryAssetId(TEXT("GolemBossPhase"), TEXT("ArenaBoss"))
			&& Definition->ValidateBossPhases().IsSuccess() && SaveAsset(Definition);
	}

	bool CreateAIAssets()
	{
		UEnvQuery* Query = CreateCombatPositionQuery();
		UStateTree* Companion = CreateDecisionTree(CompanionTreePackageName, true);
		UStateTree* Enemy = CreateDecisionTree(EnemyTreePackageName, false);
		UStateTree* Boss = CreateDecisionTree(BossTreePackageName, false);
		if (Query == nullptr || Companion == nullptr || Enemy == nullptr || Boss == nullptr)
		{
			return false;
		}
		return CreateActionSet(TEXT("ArenaCompanion"), true, 0.5f)
			&& CreateActionSet(TEXT("ArenaEnemy"), false, 1.2f)
			&& CreateActionSet(TEXT("ArenaBossPhaseOne"), false, 1.0f)
			&& CreateActionSet(TEXT("ArenaBossPhaseTwo"), true, 0.55f)
			&& CreateArchetype(TEXT("ArenaCompanion"), TEXT("ArenaCompanion"), Companion, Query, 100.0f, 25.0f)
			&& CreateArchetype(TEXT("ArenaEnemy"), TEXT("ArenaEnemy"), Enemy, Query, 90.0f, 8.0f)
			// The old arena boss hits lightly: nothing staggers it any more, so it attacks without a pause.
			&& CreateArchetype(TEXT("ArenaBoss"), TEXT("ArenaBossPhaseOne"), Boss, Query, 220.0f, 4.0f)
			&& CreateBossPhases()
			&& CreateLifecycleTestActionSet()
			&& CreateLifecycleTree(TEXT("/Game/Golem/AI/Tests/ST_Golem_LifecyclePulse"), false, false)
			&& CreateLifecycleTree(TEXT("/Game/Golem/AI/Tests/ST_Golem_LifecycleRevive"), true, false)
			&& CreateLifecycleTree(TEXT("/Game/Golem/AI/Tests/ST_Golem_LifecycleEQS"), false, true)
			&& ValidateAIAssets();
	}

	bool ValidateAIAssets()
	{
		UEnvQuery* Query = LoadObject<UEnvQuery>(nullptr,
			TEXT("/Game/Golem/AI/EQS_Golem_CombatPosition.EQS_Golem_CombatPosition"));
		UStateTree* Companion = LoadObject<UStateTree>(nullptr,
			TEXT("/Game/Golem/AI/ST_Golem_Companion.ST_Golem_Companion"));
		UStateTree* Enemy = LoadObject<UStateTree>(nullptr, TEXT("/Game/Golem/AI/ST_Golem_Enemy.ST_Golem_Enemy"));
		UStateTree* Boss = LoadObject<UStateTree>(nullptr, TEXT("/Game/Golem/AI/ST_Golem_Boss.ST_Golem_Boss"));
		if (!ValidateCombatPositionQuery(Query) || !ValidateDecisionTree(Companion, true)
			|| !ValidateDecisionTree(Enemy, false) || !ValidateDecisionTree(Boss, false))
		{
			return false;
		}
		const TArray<FString> ArchetypeNames = { TEXT("ArenaCompanion"), TEXT("ArenaEnemy"), TEXT("ArenaBoss") };
		for (const FString& Name : ArchetypeNames)
		{
			const FString Path = FString::Printf(
				TEXT("/Game/Golem/Definitions/AI/DA_Golem_Archetype_%s.DA_Golem_Archetype_%s"), *Name, *Name);
			UGolemAIArchetypeDefinition* Definition = LoadObject<UGolemAIArchetypeDefinition>(nullptr, *Path);
			if (Definition == nullptr || !Definition->ValidateArchetype().IsSuccess()
				|| Definition->GetCombatProfileId() != CombatProfileId || Definition->GetRepositionQuery() != Query)
			{
				UE_LOG(LogGolemAIAssetAuthoring, Error, TEXT("Arena archetype %s has invalid dependencies."), *Name);
				return false;
			}
		}
		const TArray<FString> ActionNames =
		{
			TEXT("ArenaCompanion"), TEXT("ArenaEnemy"), TEXT("ArenaBossPhaseOne"), TEXT("ArenaBossPhaseTwo")
		};
		for (const FString& Name : ActionNames)
		{
			const FString Path = FString::Printf(
				TEXT("/Game/Golem/Definitions/AI/DA_Golem_ActionSet_%s.DA_Golem_ActionSet_%s"), *Name, *Name);
			UGolemAIActionSetDefinition* Definition = LoadObject<UGolemAIActionSetDefinition>(nullptr, *Path);
			if (Definition == nullptr || !Definition->ValidateActionSet().IsSuccess())
			{
				UE_LOG(LogGolemAIAssetAuthoring, Error, TEXT("Arena action set %s is invalid."), *Name);
				return false;
			}
		}
		UGolemBossPhaseDefinition* Phases = LoadObject<UGolemBossPhaseDefinition>(nullptr,
			TEXT("/Game/Golem/Definitions/AI/DA_Golem_BossPhase_ArenaBoss.DA_Golem_BossPhase_ArenaBoss"));
		if (Phases == nullptr || !Phases->ValidateBossPhases().IsSuccess() || Phases->GetPhases().Num() < 2)
		{
			UE_LOG(LogGolemAIAssetAuthoring, Error, TEXT("Arena boss requires two valid stable action-set phases."));
			return false;
		}
		UGolemAssetManager* Manager = UGolemAssetManager::GetGolemAssetManager();
		UGolemDefinition* ResolvedLifecycle = nullptr;
		if (Manager == nullptr || !Manager->ValidateDefinitionCatalog().IsSuccess()
			|| !Manager->ResolveDefinition(LifecycleActionSetId, UGolemAIActionSetDefinition::StaticClass(), ResolvedLifecycle).IsSuccess()
			|| ResolvedLifecycle == nullptr || ResolvedLifecycle->GetPackage()->GetName() != LifecycleActionSetPackageName
			|| !CastChecked<UGolemAIActionSetDefinition>(ResolvedLifecycle)->ValidateActionSet().IsSuccess())
		{
			UE_LOG(LogGolemAIAssetAuthoring, Error, TEXT("Lifecycle pulse must resolve through the refreshed production typed catalog."));
			return false;
		}
		return true;
	}
}
