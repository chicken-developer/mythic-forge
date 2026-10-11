#include "Context/KnightAIContextComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AIController.h"
#include "Abilities/GameplayAbility.h"
#include "Assets/KnightAssetManager.h"
#include "Attributes/KnightAttributeSet.h"
#include "Boss/KnightBossPhaseComponent.h"
#include "Components/KnightAbilitySystemComponent.h"
#include "Components/KnightLifeComponent.h"
#include "Components/StateTreeAIComponent.h"
#include "CoreGlobals.h"
#include "Definitions/KnightAIArchetypeDefinition.h"
#include "Definitions/KnightBossPhaseDefinition.h"
#include "Effects/KnightArenaResourceRecoveryEffect.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "EnvironmentQuery/EnvQuery.h"
#include "EnvironmentQuery/EnvQueryManager.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "HAL/PlatformTime.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Skills/KnightSkillRuntimeComponent.h"
#include "StateTree.h"
#include "StateTree/KnightAIStateTreeNodes.h"
#include "StateTreeInstanceData.h"
#include "StateTreeTypes.h"
#include "Tags/KnightGameplayTags.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"
#include <limits>

namespace KnightAIActionLifecycleTests
{
	const FPrimaryAssetId PulseSetId(TEXT("KnightAIActionSet"), TEXT("LifecyclePulse"));

	class FKnightActionWorld final : public FNoncopyable
	{
	public:
		FKnightActionWorld()
			: GameInstance(NewObject<UGameInstance>(GEngine))
		{
			GameInstance->InitializeStandalone();
			World = GameInstance->GetWorld();
			World->SetShouldTick(false);
			World->CreateAISystem();
			World->GetWorldSettings()->DefaultGameMode = AGameModeBase::StaticClass();
			FURL URL;
			URL.Map = TEXT("KnightAIActionLifecycleWorld");
			URL.AddOption(TEXT("game=/Script/Engine.GameModeBase"));
			World->SetGameMode(URL);
			World->InitializeActorsForPlay(URL);
			World->BeginPlay();
		}
		~FKnightActionWorld()
		{
			if (World.IsValid() && World->HasBegunPlay())
			{
				// Stock AAIController clears its pawn before brain cleanup; preserve schema context for task exit.
				for (TActorIterator<AAIController> Controller(World.Get()); Controller; ++Controller)
				{
					if (UStateTreeAIComponent* Tree = Cast<UStateTreeAIComponent>(Controller->BrainComponent))
					{
						Tree->StopLogic(TEXT("Owned action fixture world teardown"));
					}
				}
				World->EndPlay(EEndPlayReason::Quit);
			}
			UWorld* OwnedWorld = World.Get();
			GameInstance->Shutdown();
			if (OwnedWorld != nullptr)
			{
				OwnedWorld->DestroyWorld(false);
				GEngine->DestroyWorldContext(OwnedWorld);
			}
		}
		bool TickOnce()
		{
			if (LastTickedFrame == GFrameCounter)
			{
				return false;
			}
			LastTickedFrame = GFrameCounter;
			World->Tick(LEVELTICK_All, 0.05f);
			return true;
		}
		TWeakObjectPtr<UWorld> World;

	private:
		TStrongObjectPtr<UGameInstance> GameInstance;
		uint64 LastTickedFrame = MAX_uint64;
	};

	struct FKnightActionActor
	{
		TWeakObjectPtr<ACharacter> Actor;
		TWeakObjectPtr<UKnightAbilitySystemComponent> ASC;
		TWeakObjectPtr<UKnightAttributeSet> Attributes;
		TWeakObjectPtr<UKnightLifeComponent> Life;
		FKnightAbilitySetHandle FoundationGrant;
		FKnightResult Initialization = FKnightResult::Failure(EKnightError::InvalidState, TEXT("ActionActorFixture"), FText());
		bool IsReady() const
		{
			return Initialization.IsSuccess() && Actor.IsValid() && ASC.IsValid() && Attributes.IsValid() &&
				Life.IsValid() && ASC->GetEntityId().IsValid() && Life->IsAlive();
		}
	};

	template <typename TDefinition>
	TDefinition* ResolveDefinition(const FPrimaryAssetId& Id)
	{
		UKnightDefinition* Definition = nullptr;
		UKnightAssetManager* Manager = UKnightAssetManager::GetKnightAssetManager();
		return Manager != nullptr && Manager->ResolveDefinition(Id, TDefinition::StaticClass(), Definition).IsSuccess()
			? Cast<TDefinition>(Definition) : nullptr;
	}

	FKnightActionActor SpawnActor(UWorld* World, uint32 Identity, bool bParty, const FVector& Location)
	{
		FKnightActionActor Fixture;
		FActorSpawnParameters Parameters;
		Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Fixture.Actor = World->SpawnActor<ACharacter>(Location, FRotator::ZeroRotator, Parameters);
		if (!Fixture.Actor.IsValid())
		{
			return Fixture;
		}
		Fixture.Actor->GetCharacterMovement()->DisableMovement();
		Fixture.ASC = NewObject<UKnightAbilitySystemComponent>(Fixture.Actor.Get());
		Fixture.Actor->AddInstanceComponent(Fixture.ASC.Get());
		Fixture.ASC->RegisterComponent();
		Fixture.Attributes = NewObject<UKnightAttributeSet>(Fixture.Actor.Get());
		Fixture.Life = NewObject<UKnightLifeComponent>(Fixture.Actor.Get());
		Fixture.Actor->AddInstanceComponent(Fixture.Life.Get());
		Fixture.Life->RegisterComponent();
		Fixture.Initialization = Fixture.ASC->InitializeKnightActor(Fixture.Actor.Get(), Fixture.Attributes.Get(),
			Fixture.Life.Get(), FKnightEntityId(FGuid(0, 0, 0, Identity)), FKnightAttributeDefaults(), bParty);
		if (Fixture.Initialization.IsSuccess() && !bParty)
		{
			Fixture.ASC->SetLooseGameplayTagCount(KnightGameplayTags::Role_Enemy, 1,
				EGameplayTagReplicationState::TagAndCountToAll);
		}
		return Fixture;
	}

	bool GrantFoundation(FKnightActionActor& Actor, FName SetName = TEXT("Foundation"))
	{
		const UKnightAbilitySetDefinition* Definition = ResolveDefinition<UKnightAbilitySetDefinition>(
			FPrimaryAssetId(TEXT("KnightAbilitySet"), SetName));
		return Definition != nullptr && Actor.ASC.IsValid() &&
			Actor.ASC->GrantAbilitySet(Definition, TEXT("ActionLifecycleFixture"), Actor.FoundationGrant).IsSuccess();
	}

	TArray<FGameplayAbilitySpecHandle> CaptureSpecs(const UKnightAbilitySystemComponent* ASC)
	{
		TArray<FGameplayAbilitySpecHandle> Handles;
		for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
		{
			Handles.Add(Spec.Handle);
		}
		return Handles;
	}

	bool HasPressedInput(const UKnightAbilitySystemComponent* ASC)
	{
		for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
		{
			if (Spec.InputPressed)
			{
				return true;
			}
		}
		return false;
	}

	struct FKnightActionController
	{
		TWeakObjectPtr<AAIController> Controller;
		TWeakObjectPtr<UKnightAIContextComponent> Context;
		TWeakObjectPtr<UStateTreeAIComponent> Tree;
		FKnightResult Initialization = FKnightResult::Failure(EKnightError::InvalidState, TEXT("ActionControllerFixture"), FText());
		bool IsReady() const
		{
			return Initialization.IsSuccess() && Controller.IsValid() && Context.IsValid();
		}
	};

	FKnightActionController CreateController(const FKnightActionActor& Pawn, UStateTree* TreeAsset = nullptr)
	{
		FKnightActionController Fixture;
		Fixture.Controller = Pawn.Actor->GetWorld()->SpawnActor<AAIController>();
		Fixture.Context = NewObject<UKnightAIContextComponent>(Fixture.Controller.Get());
		Fixture.Controller->AddInstanceComponent(Fixture.Context.Get());
		Fixture.Context->RegisterComponent();
		Fixture.Controller->Possess(Pawn.Actor.Get());
		Fixture.Initialization = Fixture.Context->InitializeControlledPawn(Pawn.Actor.Get());
		if (TreeAsset != nullptr)
		{
			Fixture.Tree = NewObject<UStateTreeAIComponent>(Fixture.Controller.Get());
			Fixture.Tree->SetStartLogicAutomatically(false);
			Fixture.Tree->SetStateTree(TreeAsset);
			Fixture.Controller->AddInstanceComponent(Fixture.Tree.Get());
			Fixture.Controller->BrainComponent = Fixture.Tree.Get();
			Fixture.Tree->RegisterComponent();
		}
		return Fixture;
	}

	const FStateTreeInstanceData* ReadTreeInstances(const UStateTreeAIComponent* Tree)
	{
		const FStructProperty* Property = Tree != nullptr
			? FindFProperty<FStructProperty>(Tree->GetClass(), TEXT("InstanceData")) : nullptr;
		return Property != nullptr && Property->Struct == FStateTreeInstanceData::StaticStruct()
			? Property->ContainerPtrToValuePtr<FStateTreeInstanceData>(Tree) : nullptr;
	}

	bool HasClearedTreeExecution(const UStateTreeAIComponent* Tree)
	{
		const FStateTreeInstanceData* Instances = ReadTreeInstances(Tree);
		return Tree != nullptr && !Tree->IsRunning() && !Tree->IsComponentTickEnabled() &&
			Tree->GetStateTreeRunStatus() == EStateTreeRunStatus::Unset && Instances != nullptr &&
			Instances->Num() == 0 && Instances->GetExecutionState()->ActiveFrames.IsEmpty();
	}

	/** Uses the same tree-before-pawn ordering as production arena controllers. */
	bool UnpossessAfterStoppingTree(AAIController* Controller)
	{
		if (!IsValid(Controller))
		{
			return false;
		}
		UStateTreeAIComponent* Tree = Cast<UStateTreeAIComponent>(Controller->BrainComponent);
		if (Tree != nullptr)
		{
			Tree->StopLogic(TEXT("Owned action fixture controller transfer"));
			if (!HasClearedTreeExecution(Tree))
			{
				return false;
			}
		}
		Controller->UnPossess();
		return Controller->GetPawn() == nullptr;
	}

	/** Observes actual task lifetime without changing its state or exposing a production test hook. */
	TWeakPtr<FKnightAIActionCompletionState> CaptureActionCompletion(const UStateTreeAIComponent* Tree)
	{
		const FStateTreeInstanceData* Instances = ReadTreeInstances(Tree);
		if (Instances == nullptr)
		{
			return nullptr;
		}
		for (int32 Index = 0; Index < Instances->Num(); ++Index)
		{
			const FKnightAIExecuteActionInstanceData* Action =
				Instances->GetStruct(Index).GetPtr<FKnightAIExecuteActionInstanceData>();
			if (Action != nullptr && Action->ActionToken.IsValid())
			{
				return Action->CompletionState;
			}
		}
		return nullptr;
	}

	/** TimerManager and deferred task completion advance on distinct engine frames. */
	class FKnightWaitForActionTimeCommand final : public IAutomationLatentCommand
	{
	public:
		FKnightWaitForActionTimeCommand(FAutomationTestBase* InTest, const TSharedRef<FKnightActionWorld>& InWorld,
			float Seconds, TFunction<void()> InCompletion)
			: Test(InTest), Context(InWorld), Completion(MoveTemp(InCompletion)),
			TargetTime(InWorld->World->GetTimeSeconds() + Seconds), StartedAt(FPlatformTime::Seconds())
		{
		}
		virtual bool Update() override
		{
			if (!Context->World.IsValid() || !Context->World->HasBegunPlay() ||
				FPlatformTime::Seconds() - StartedAt > 20.0)
			{
				Test->AddError(TEXT("Action lifecycle fixture lost its world or exceeded its bounded latent wait."));
				return true;
			}
			if (!Context->TickOnce() || Context->World->GetTimeSeconds() < TargetTime)
			{
				return false;
			}
			Completion();
			Completion = nullptr;
			return true;
		}

	private:
		FAutomationTestBase* Test;
		TSharedRef<FKnightActionWorld> Context;
		TFunction<void()> Completion;
		double TargetTime;
		double StartedAt;
	};

	void WaitForWorldTime(FAutomationTestBase* Test, const TSharedRef<FKnightActionWorld>& Context,
		float Seconds, TFunction<void()> Completion)
	{
		FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FKnightWaitForActionTimeCommand>(
			Test, Context, Seconds, MoveTemp(Completion)));
	}

	struct FKnightReviveState
	{
		FKnightReviveState()
			: Context(MakeShared<FKnightActionWorld>()),
			Source(SpawnActor(Context->World.Get(), 1, true, FVector::ZeroVector)),
			Target(SpawnActor(Context->World.Get(), 2, true, FVector(200.0, 0.0, 0.0))),
			Enemy(SpawnActor(Context->World.Get(), 101, false, FVector(600.0, 600.0, 0.0)))
		{
		}
		TSharedRef<FKnightActionWorld> Context;
		FKnightActionActor Source;
		FKnightActionActor Target;
		FKnightActionActor Enemy;
		FKnightActionController AI;
		TStrongObjectPtr<UStateTree> TreeAsset;
		TArray<FGameplayAbilitySpecHandle> OriginalSpecs;
		FKnightEntityId OriginalIdentity;
		FGuid CompletedAction;
	};

	struct FKnightRecoveryState
	{
		FKnightRecoveryState()
			: Context(MakeShared<FKnightActionWorld>()),
			Source(SpawnActor(Context->World.Get(), 1, true, FVector::ZeroVector)),
			Enemy(SpawnActor(Context->World.Get(), 101, false, FVector(500.0, 0.0, 0.0)))
		{
		}
		TSharedRef<FKnightActionWorld> Context;
		FKnightActionActor Source;
		FKnightActionActor Enemy;
		FActiveGameplayEffectHandle RecoveryHandle;
		TArray<FGameplayAbilitySpecHandle> OriginalSpecs;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKnightAIArchetypeBossContractsTest, "Knight.AI.Definitions.ArchetypeAndBossContracts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FKnightAIArchetypeBossContractsTest::RunTest(const FString& Parameters)
{
	using namespace KnightAIActionLifecycleTests;
	TStrongObjectPtr<UKnightAIArchetypeDefinition> Archetype(NewObject<UKnightAIArchetypeDefinition>());
	TestTrue(TEXT("Archetype authoring retains a stable native definition type"),
		Archetype->TrySetIdentity(TEXT("ContractFixture"), TEXT("KnightAIArchetype")));
	TestTrue(TEXT("Typed combat and action definitions are accepted"), Archetype->TrySetIds(
		FPrimaryAssetId(TEXT("KnightCombatProfile"), TEXT("FoundationCombat")),
		FPrimaryAssetId(TEXT("KnightAIActionSet"), TEXT("LifecyclePulse"))).IsSuccess());
	const FPrimaryAssetId PreviousProfile = Archetype->GetCombatProfileId();
	TestEqual(TEXT("Package-like unstable profile IDs are rejected"), Archetype->TrySetIds(
		FPrimaryAssetId(TEXT("KnightCombatProfile"), TEXT("Bad/Profile")),
		PulseSetId).Error, EKnightError::InvalidId);
	TestTrue(TEXT("Rejected IDs preserve previous authoring"), Archetype->GetCombatProfileId() == PreviousProfile);
	FKnightAIArchetypeTuning Tuning = Archetype->GetTuning();
	Tuning.WalkSpeed = 420.0f;
	TestTrue(TEXT("Finite tuning is accepted"), Archetype->TrySetTuning(Tuning).IsSuccess());
	Tuning.SightRadius = std::numeric_limits<float>::quiet_NaN();
	TestEqual(TEXT("Nonfinite sight is rejected"), Archetype->TrySetTuning(Tuning).Error, EKnightError::InvalidData);
	TestEqual(TEXT("Rejected tuning preserves all prior fields"), Archetype->GetTuning().WalkSpeed, 420.0f);
	Tuning = Archetype->GetTuning();
	Tuning.LoseSightRadius = Tuning.SightRadius - 1.0f;
	TestEqual(TEXT("Lose-sight cannot invalidate acquisition radius"),
		Archetype->TrySetTuning(Tuning).Error, EKnightError::InvalidData);
	TStrongObjectPtr<UStateTree> Tree(LoadObject<UStateTree>(nullptr,
		TEXT("/Game/Knight/AI/Tests/ST_Knight_LifecyclePulse.ST_Knight_LifecyclePulse")));
	TStrongObjectPtr<UEnvQuery> Query(LoadObject<UEnvQuery>(nullptr,
		TEXT("/Game/Knight/AI/EQS_Knight_CombatPosition.EQS_Knight_CombatPosition")));
	if (!TestTrue(TEXT("Actual compiled lifecycle tree and EQS fixture exist"), Tree.IsValid() && Query.IsValid()))
	{
		return false;
	}
	TestTrue(TEXT("Compiled stock-AI decision assets are accepted"), Archetype->TrySetDecisionAssets(Tree.Get(), Query.Get()).IsSuccess());
	TestTrue(TEXT("Complete archetype validates actual decision assets"), Archetype->ValidateArchetype().IsSuccess());
	TStrongObjectPtr<UStateTree> Uncompiled(NewObject<UStateTree>());
	TestEqual(TEXT("Uncompiled decision assets are rejected"),
		Archetype->TrySetDecisionAssets(Uncompiled.Get(), Query.Get()).Error, EKnightError::InvalidData);
	TestTrue(TEXT("Rejected decision assets retain the compiled tree"), Archetype->GetStateTree() == Tree.Get());

	TStrongObjectPtr<UKnightBossPhaseDefinition> Boss(NewObject<UKnightBossPhaseDefinition>());
	TestTrue(TEXT("Boss phase definition uses stable native identity"),
		Boss->TrySetIdentity(TEXT("BossContractFixture"), TEXT("KnightBossPhase")));
	TArray<FKnightBossPhaseEntry> Phases;
	FKnightBossPhaseEntry& First = Phases.AddDefaulted_GetRef();
	First.PhaseKey = TEXT("PhaseOne");
	First.HealthFraction = 1.0f;
	First.ActionSetId = FPrimaryAssetId(TEXT("KnightAIActionSet"), TEXT("ArenaBossPhaseOne"));
	FKnightBossPhaseEntry& Second = Phases.AddDefaulted_GetRef();
	Second.PhaseKey = TEXT("PhaseTwo");
	Second.HealthFraction = 0.5f;
	Second.ActionSetId = FPrimaryAssetId(TEXT("KnightAIActionSet"), TEXT("ArenaBossPhaseTwo"));
	Second.DecisionInterval = 0.1f;
	TestTrue(TEXT("Ordered two-phase configuration is accepted"), Boss->TrySetPhases(Phases).IsSuccess());
	TestTrue(TEXT("Complete boss phase definition validates"), Boss->ValidateBossPhases().IsSuccess());
	TArray<FKnightBossPhaseEntry> Invalid = Phases;
	Invalid[1].PhaseKey = TEXT("Invalid Phase");
	TestEqual(TEXT("Phase keys follow the shared stable identifier contract"),
		Boss->TrySetPhases(Invalid).Error, EKnightError::InvalidData);
	Invalid = Phases;
	Invalid[1].PhaseKey = Invalid[0].PhaseKey;
	TestEqual(TEXT("Duplicate phase keys are rejected"), Boss->TrySetPhases(Invalid).Error, EKnightError::InvalidData);
	Invalid = Phases;
	Invalid[1].HealthFraction = 1.0f;
	TestEqual(TEXT("Phase thresholds must descend"), Boss->TrySetPhases(Invalid).Error, EKnightError::InvalidData);
	Invalid = Phases;
	Invalid[1].DecisionInterval = std::numeric_limits<float>::quiet_NaN();
	TestEqual(TEXT("Nonfinite phase pacing is rejected"), Boss->TrySetPhases(Invalid).Error, EKnightError::InvalidData);
	TestEqual(TEXT("Every rejected phase edit remains atomic"), Boss->GetPhases()[1].HealthFraction, 0.5f);
	TestTrue(TEXT("Rejected phase edits preserve the stable action-set choice"), Boss->GetPhases()[1].ActionSetId == Second.ActionSetId);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKnightAIExactSharedGASTest, "Knight.AI.Actions.ExactSharedGAS",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FKnightAIExactSharedGASTest::RunTest(const FString& Parameters)
{
	using namespace KnightAIActionLifecycleTests;
	const TSharedRef<FKnightActionWorld> OwnedWorld = MakeShared<FKnightActionWorld>();
	FKnightActionWorld& World = OwnedWorld.Get();
	FKnightActionActor Source = SpawnActor(World.World.Get(), 1, true, FVector::ZeroVector);
	const FKnightActionActor Enemy = SpawnActor(World.World.Get(), 101, false, FVector(250.0, 0.0, 0.0));
	if (!TestTrue(TEXT("Real source and enemy ASC/life initialize"), Source.IsReady() && Enemy.IsReady()) ||
		!TestTrue(TEXT("Source owns a real Foundation grant"), GrantFoundation(Source)))
	{
		return false;
	}
	const FKnightActionController AI = CreateController(Source);
	if (!TestTrue(TEXT("Actual AI possession initializes its controller context"), AI.IsReady()))
	{
		return false;
	}
	TestTrue(TEXT("Stable action set validates the exact granted spec"), AI.Context->SetActionSet(PulseSetId).IsSuccess());
	TestTrue(TEXT("Authority resolves a hostile stable target"), AI.Context->SetValidatedTarget(Enemy.Actor.Get()).IsSuccess());
	TestTrue(TEXT("Archetype base decision pacing is configurable"), AI.Context->SetDecisionInterval(0.25f).IsSuccess());
	TestTrue(TEXT("Near LOD is a separate scheduled interval"), AI.Context->SetScheduledDecisionInterval(0.1f).IsSuccess());
	TestEqual(TEXT("Near scheduling preserves the phase/base pace"), AI.Context->GetDecisionSnapshot().DecisionInterval, 0.25f);
	TestTrue(TEXT("Far LOD delays the effective decision interval"), AI.Context->SetScheduledDecisionInterval(0.35f).IsSuccess());
	TestEqual(TEXT("Far scheduling retains the base for returning near"), AI.Context->GetBaseDecisionInterval(), 0.25f);
	FKnightAIActionEntry Action;
	TestTrue(TEXT("Pure selection finds the shared native pulse"), AI.Context->TrySelectAction(Action).IsSuccess());
	const TArray<FGameplayAbilitySpecHandle> OriginalSpecs = CaptureSpecs(Source.ASC.Get());
	int32 Ended = 0;
	EKnightAIActionStatus EndStatus = EKnightAIActionStatus::Unknown;
	const FDelegateHandle EndHandle = AI.Context->OnActionFinished.AddLambda(
		[&Ended, &EndStatus](FGuid Token, EKnightAIActionStatus Status)
		{
			++Ended;
			EndStatus = Status;
		});
	ON_SCOPE_EXIT
	{
		AI.Context->OnActionFinished.Remove(EndHandle);
	};
	FGuid Token;
	TestTrue(TEXT("Exact shared GAS action really activates"), AI.Context->TryBeginAction(Action, true, Token).IsSuccess());
	TestTrue(TEXT("Activated action returns an owned generation token"), Token.IsValid());
	TestEqual(TEXT("Actual pulse GE reduces target health once"), Enemy.Attributes->GetHealth(), 75.0f);
	TestEqual(TEXT("Synchronous ability end is captured once"), Ended, 1);
	TestEqual(TEXT("Actual successful ability end is retained"), EndStatus, EKnightAIActionStatus::Succeeded);
	TestEqual(TEXT("Instant action status remains queryable"), AI.Context->GetActionStatus(Token), EKnightAIActionStatus::Succeeded);
	TestFalse(TEXT("Completion clears only the owned spec input"), HasPressedInput(Source.ASC.Get()));
	TestTrue(TEXT("Action execution preserves granted spec handles"), CaptureSpecs(Source.ASC.Get()) == OriginalSpecs);
	TestEqual(TEXT("Instant completion returns the actual director budget"),
		World.World->GetSubsystem<UKnightCombatDirectorSubsystem>()->GetActiveLeaseCount(), 0);
	TestEqual(TEXT("Confirmed activation starts the selection cooldown"), AI.Context->TrySelectAction(Action).Error, EKnightError::NotFound);
	TestEqual(TEXT("Cooldown queries never repeat damage"), Enemy.Attributes->GetHealth(), 75.0f);
	TestEqual(TEXT("Foreign cancellation cannot claim another action token"), AI.Context->CancelAction(FGuid::NewGuid()).Error, EKnightError::NotOwned);
	FKnightAbilitySetHandle Duplicate;
	const UKnightAbilitySetDefinition* Foundation = ResolveDefinition<UKnightAbilitySetDefinition>(
		FPrimaryAssetId(TEXT("KnightAbilitySet"), TEXT("Foundation")));
	TestTrue(TEXT("A distinct source can own a separate real GAS grant"), Source.ASC->GrantAbilitySet(
		Foundation, TEXT("DuplicateActionFixture"), Duplicate).IsSuccess());
	TestEqual(TEXT("Ambiguous shared action/input binding is rejected before activation"),
		AI.Context->ValidateActionSet(PulseSetId).Error, EKnightError::Conflict);
	TestEqual(TEXT("Ambiguous binding never changes target health"), Enemy.Attributes->GetHealth(), 75.0f);
	TestTrue(TEXT("Only duplicate fixture grant is revoked"), Source.ASC->RevokeAbilitySet(Duplicate).IsSuccess());
	TestTrue(TEXT("Original spec handles survive rejected ambiguous action"), CaptureSpecs(Source.ASC.Get()) == OriginalSpecs);

	TStrongObjectPtr<UStateTree> Tree(LoadObject<UStateTree>(nullptr,
		TEXT("/Game/Knight/AI/Tests/ST_Knight_LifecyclePulse.ST_Knight_LifecyclePulse")));
	if (!TestTrue(TEXT("The actual single-action fixture is compiled"), Tree.IsValid() && Tree->IsReadyToRun()))
	{
		return false;
	}
	FKnightActionActor TreeSource = SpawnActor(World.World.Get(), 2, true, FVector(0.0, 600.0, 0.0));
	const FKnightActionActor TreeEnemy = SpawnActor(World.World.Get(), 102, false, FVector(250.0, 600.0, 0.0));
	if (!TestTrue(TEXT("Tree fixture foundation is real"), TreeSource.IsReady() && TreeEnemy.IsReady() && GrantFoundation(TreeSource)))
	{
		return false;
	}
	const FKnightActionController TreeAI = CreateController(TreeSource, Tree.Get());
	if (!TestTrue(TEXT("Native task context is initialized"), TreeAI.IsReady() && TreeAI.Tree.IsValid()))
	{
		return false;
	}
	TestTrue(TEXT("Tree task uses the actual validated action set"), TreeAI.Context->SetActionSet(PulseSetId).IsSuccess());
	TestTrue(TEXT("Tree task target is server validated"), TreeAI.Context->SetValidatedTarget(TreeEnemy.Actor.Get()).IsSuccess());
	const TSharedRef<int32> TreeEnded = MakeShared<int32>(0);
	const FDelegateHandle TreeEndHandle = TreeAI.Context->OnActionFinished.AddLambda(
		[TreeEnded](FGuid Token, EKnightAIActionStatus Status)
		{
			++(*TreeEnded);
		});
	TreeAI.Tree->StartLogic();
	TestEqual(TEXT("The StateTree task applied one actual pulse GE"), TreeEnemy.Attributes->GetHealth(), 75.0f);
	const TWeakPtr<FKnightAIActionCompletionState> TreeCompletion = CaptureActionCompletion(TreeAI.Tree.Get());
	FAutomationTestBase* Test = this;
	// Completed EnterState schedules its terminal transition for the next real StateTree tick.
	WaitForWorldTime(Test, OwnedWorld, 0.2f, [OwnedWorld, TreeAI, TreeEnemy, TreeEnded, TreeEndHandle, TreeCompletion, Test]()
		{
			ON_SCOPE_EXIT
			{
				if (TreeAI.Context.IsValid())
				{
					TreeAI.Context->OnActionFinished.Remove(TreeEndHandle);
				}
			};
			if (!Test->TestTrue(TEXT("Terminal fixture retains its actual tree and target"),
				TreeAI.Tree.IsValid() && TreeAI.Context.IsValid() && TreeEnemy.Attributes.IsValid()))
			{
				return;
			}
			const EStateTreeRunStatus ActualStatus = TreeAI.Tree->GetStateTreeRunStatus();
			Test->TestEqual(TEXT("An instant GAS task reaches a real terminal state once"),
				ActualStatus, EStateTreeRunStatus::Succeeded);
			Test->TestFalse(TEXT("Terminal StateTree schedules no further execution ticks"),
				TreeAI.Tree->IsComponentTickEnabled());
			Test->TestEqual(TEXT("Terminal transition never repeats the shared GAS action"), *TreeEnded, 1);
			Test->TestEqual(TEXT("Terminal transition never applies another pulse GE"), TreeEnemy.Attributes->GetHealth(), 75.0f);
			Test->TestFalse(TEXT("Terminal task releases completion timer and delegate ownership"), TreeCompletion.IsValid());
			Test->TestFalse(TEXT("Terminal task clears its active action capability"), TreeAI.Context->GetDecisionSnapshot().bActionRunning);
			Test->TestEqual(TEXT("Terminal task releases its director lease"),
				OwnedWorld->World->GetSubsystem<UKnightCombatDirectorSubsystem>()->GetActiveLeaseCount(), 0);
			// UE's brain IsRunning flag is cleared by owner shutdown after the actual terminal transition.
			if (ActualStatus == EStateTreeRunStatus::Succeeded)
			{
				TreeAI.Tree->StopLogic(TEXT("Confirmed terminal fixture completion"));
			}
			Test->TestFalse(TEXT("Confirmed terminal fixture no longer runs after owned shutdown"), TreeAI.Tree->IsRunning());
		});
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKnightAIHeldReviveOwnershipTest, "Knight.AI.Actions.HeldReviveOwnership",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FKnightAIHeldReviveOwnershipTest::RunTest(const FString& Parameters)
{
	using namespace KnightAIActionLifecycleTests;
	const TSharedRef<FKnightReviveState> State = MakeShared<FKnightReviveState>();
	if (!TestTrue(TEXT("Revive fixtures have actual actor/ASC/life state"),
		State->Source.IsReady() && State->Target.IsReady() && State->Enemy.IsReady()) ||
		!TestTrue(TEXT("AI shares the real human Foundation abilities"), GrantFoundation(State->Source)))
	{
		return false;
	}
	State->TreeAsset.Reset(LoadObject<UStateTree>(nullptr,
		TEXT("/Game/Knight/AI/Tests/ST_Knight_LifecycleRevive.ST_Knight_LifecycleRevive")));
	if (!TestTrue(TEXT("Compiled held-revive tree exists"), State->TreeAsset.IsValid() && State->TreeAsset->IsReadyToRun()))
	{
		return false;
	}
	State->AI = CreateController(State->Source, State->TreeAsset.Get());
	if (!TestTrue(TEXT("Revive tree owns a real AI context"), State->AI.IsReady() && State->AI.Tree.IsValid()))
	{
		return false;
	}
	TestTrue(TEXT("Target channel timing is finite and bounded"),
		State->Target.Life->TrySetLifeTuning(10.0f, 0.3f, 300.0f, 0.35f).IsSuccess());
	TestTrue(TEXT("Source health change comes through real GAS damage"), UKnightAbilitySystemComponent::ApplyDamage(
		State->Enemy.Actor.Get(), State->Source.Actor.Get(), 14.0f).IsSuccess());
	TestTrue(TEXT("Target is downed through real GAS damage"), UKnightAbilitySystemComponent::ApplyDamage(
		State->Enemy.Actor.Get(), State->Target.Actor.Get(), 200.0f).IsSuccess());
	TestTrue(TEXT("Fixture target is actually downed"), State->Target.Life->IsDowned());
	State->OriginalSpecs = CaptureSpecs(State->Source.ASC.Get());
	State->OriginalIdentity = State->Source.ASC->GetEntityId();
	TestTrue(TEXT("Friendly downed target is server validated"), State->AI.Context->SetValidatedTarget(State->Target.Actor.Get(), true).IsSuccess());
	State->AI.Tree->StartLogic();
	TestTrue(TEXT("StateTree starts the actual held GAS channel"), State->AI.Tree->IsRunning());
	TestTrue(TEXT("Actual channel reserves target for the AI pawn"), State->Target.Life->GetActiveReviver() == State->Source.Actor.Get());
	TestTrue(TEXT("Actual channel owns the GAS reviving tag"), State->Source.ASC->HasMatchingGameplayTag(KnightGameplayTags::State_Life_Reviving));
	const FGuid StoppedToken = State->AI.Context->GetDecisionSnapshot().ActiveActionToken;
	TestTrue(TEXT("Held action has an owned capability"), StoppedToken.IsValid());
	State->AI.Tree->StopLogic(TEXT("Lifecycle cancellation regression"));
	TestFalse(TEXT("Explicit tree stop ends actual component execution"), State->AI.Tree->IsRunning());
	TestTrue(TEXT("Explicit tree stop destroys actual task storage and resets execution"),
		HasClearedTreeExecution(State->AI.Tree.Get()));
	TestFalse(TEXT("Tree stop cancels exact held action"), State->AI.Context->GetDecisionSnapshot().bActionRunning);
	TestFalse(TEXT("Tree stop releases the actual target reservation"), State->Target.Life->IsReviveInProgress());
	TestFalse(TEXT("Tree stop removes actual channel tag"), State->Source.ASC->HasMatchingGameplayTag(KnightGameplayTags::State_Life_Reviving));
	TestFalse(TEXT("Tree stop removes actual persistent revive cue"), State->Source.ASC->IsGameplayCueActive(KnightGameplayTags::GameplayCue_Revive));
	TestFalse(TEXT("Tree stop releases exact spec input"), HasPressedInput(State->Source.ASC.Get()));
	TestEqual(TEXT("Stopped channel never heals the target"), State->Target.Attributes->GetHealth(), 0.0f);

	State->AI.Tree->StartLogic();
	const FGuid PendingCompletionToken = State->AI.Context->GetDecisionSnapshot().ActiveActionToken;
	const TWeakPtr<FKnightAIActionCompletionState> PendingCompletion = CaptureActionCompletion(State->AI.Tree.Get());
	TestTrue(TEXT("Actual running task owns its completion lifecycle"), PendingCompletion.IsValid());
	TestTrue(TEXT("Actual GAS cancellation queues task completion outside tree execution"),
		State->AI.Context->CancelAction(PendingCompletionToken).IsSuccess());
	TestTrue(TEXT("Queued completion retains the task callback until actual tree exit"), PendingCompletion.IsValid());
	State->AI.Tree->StopLogic(TEXT("Pending completion timer cleanup regression"));
	TestTrue(TEXT("Task exit clears actual execution storage before pending timer retirement"),
		HasClearedTreeExecution(State->AI.Tree.Get()));
	TestFalse(TEXT("Stopped tree no longer owns a task completion delegate"),
		CaptureActionCompletion(State->AI.Tree.Get()).IsValid());
	TestFalse(TEXT("Pending completion cannot keep the old tree running"), State->AI.Tree->IsRunning());

	State->AI.Tree->StartLogic();
	const FGuid TransferToken = State->AI.Context->GetDecisionSnapshot().ActiveActionToken;
	const TWeakPtr<FKnightAIActionCompletionState> TransferCompletion = CaptureActionCompletion(State->AI.Tree.Get());
	TestTrue(TEXT("Restart owns a new held action generation"), TransferToken.IsValid() && TransferToken != StoppedToken);
	TestTrue(TEXT("Transfer starts with an actual owned task completion lifecycle"), TransferCompletion.IsValid());
	TestEqual(TEXT("A stale token cannot cancel the restarted channel"),
		State->AI.Context->CancelAction(StoppedToken).Error, EKnightError::NotOwned);
	TestTrue(TEXT("Restarted channel still owns target reservation"), State->Target.Life->IsReviveInProgress());
	APlayerController* Human = State->Context->World->SpawnActor<APlayerController>();
	const TWeakObjectPtr<AAIController> WeakAI = State->AI.Controller;
	const TWeakObjectPtr<UStateTreeAIComponent> WeakTree = State->AI.Tree;
	const TWeakObjectPtr<APlayerController> WeakHuman = Human;
	const TWeakObjectPtr<ACharacter> WeakSource = State->Source.Actor;
	const TSharedRef<int32> Takeovers = MakeShared<int32>(0);
	const FDelegateHandle TakeoverHandle = State->Source.ASC->OnAbilityEnded.AddLambda(
		[WeakAI, WeakTree, WeakHuman, WeakSource, Takeovers](const FAbilityEndedData& Data)
		{
			if (Data.bWasCancelled && Data.AbilityThatEnded != nullptr &&
				Data.AbilityThatEnded->GetAssetTags().HasTagExact(KnightGameplayTags::Ability_Party_Revive) &&
				WeakAI.IsValid() && WeakTree.IsValid() && WeakHuman.IsValid() && WeakSource.IsValid())
			{
				if (UnpossessAfterStoppingTree(WeakAI.Get()))
				{
					++(*Takeovers);
					WeakHuman->Possess(WeakSource.Get());
				}
			}
		});
	const FKnightResult CallbackCancellation = State->AI.Context->CancelAction(TransferToken);
	State->Source.ASC->OnAbilityEnded.Remove(TakeoverHandle);
	TestTrue(TEXT("Exact cancellation survives actual synchronous GAS callback takeover"), CallbackCancellation.IsSuccess());
	TestEqual(TEXT("Actual held-channel cancellation performs one synchronous takeover"), *Takeovers, 1);
	TestTrue(TEXT("Real human takeover retains the same canonical fixture actor"), Human->GetPawn() == State->Source.Actor.Get());
	TestFalse(TEXT("Actual old tree is stopped before synchronous human handover"), State->AI.Tree->IsRunning());
	TestTrue(TEXT("Actual old tree has empty task storage at handover"),
		HasClearedTreeExecution(State->AI.Tree.Get()));
	TestFalse(TEXT("Synchronous handover releases old task delegate and timer capability"), TransferCompletion.IsValid());
	TestFalse(TEXT("Controller transfer cancels old AI context action"), State->AI.Context->GetDecisionSnapshot().bActionRunning);
	TestFalse(TEXT("Controller transfer releases revive reservation"), State->Target.Life->IsReviveInProgress());
	TestFalse(TEXT("Controller transfer removes reviving tag"), State->Source.ASC->HasMatchingGameplayTag(KnightGameplayTags::State_Life_Reviving));
	TestFalse(TEXT("Controller transfer leaves no held input"), HasPressedInput(State->Source.ASC.Get()));
	TestEqual(TEXT("Controller transfer retains no owned pending EQS request"),
		State->AI.Context->GetActiveEQSRequestIds().Num(), 0);
	TestFalse(TEXT("Controller transfer retains no attack lease"), State->AI.Context->GetActiveAttackLease().IsValid());
	TestEqual(TEXT("Controller transfer preserves damaged source health"), State->Source.Attributes->GetHealth(), 86.0f);
	TestTrue(TEXT("Controller transfer preserves immutable actor ASC identity"), State->Source.ASC->GetEntityId() == State->OriginalIdentity);
	TestTrue(TEXT("Controller transfer preserves grant/spec handles"), CaptureSpecs(State->Source.ASC.Get()) == State->OriginalSpecs);
	TestEqual(TEXT("Controller transfer preserves exactly one source-owned grant"), State->Source.ASC->GetOwnedGrantCount(), 1);

	Human->UnPossess();
	State->AI.Controller->Possess(State->Source.Actor.Get());
	TestTrue(TEXT("AI reclaims the same existing actor"), State->AI.Context->InitializeControlledPawn(State->Source.Actor.Get()).IsSuccess());
	TestTrue(TEXT("Reclaimed AI validates target again"), State->AI.Context->SetValidatedTarget(State->Target.Actor.Get(), true).IsSuccess());
	State->AI.Tree->StartLogic();
	State->CompletedAction = State->AI.Context->GetDecisionSnapshot().ActiveActionToken;
	TestTrue(TEXT("Reclaimed AI starts a real bounded channel"), State->CompletedAction.IsValid() && State->Target.Life->IsReviveInProgress());
	FAutomationTestBase* Test = this;
	WaitForWorldTime(Test, State->Context, 0.8f, [State, PendingCompletion, Test]()
		{
			// Cleared active timers retain their delegate until TimerManager retires them on an actual tick.
			Test->TestFalse(TEXT("Actual timer ticks release canceled task callback ownership"), PendingCompletion.IsValid());
			Test->TestTrue(TEXT("Actual world timer completes shared GAS revive"), State->Target.Life->IsAlive());
			Test->TestEqual(TEXT("Actual revive GE restores configured health fraction"), State->Target.Attributes->GetHealth(), 35.0f);
			Test->TestFalse(TEXT("Completed channel releases actual reservation"), State->Target.Life->IsReviveInProgress());
			Test->TestFalse(TEXT("Completed channel removes actual revive cue"), State->Source.ASC->IsGameplayCueActive(KnightGameplayTags::GameplayCue_Revive));
			Test->TestFalse(TEXT("Completed channel clears spec input"), HasPressedInput(State->Source.ASC.Get()));
			Test->TestEqual(TEXT("Actual OnAbilityEnded advances terminal StateTree"),
				State->AI.Tree->GetStateTreeRunStatus(), EStateTreeRunStatus::Succeeded);
			Test->TestEqual(TEXT("Completed generation retains confirmed success"),
				State->AI.Context->GetActionStatus(State->CompletedAction), EKnightAIActionStatus::Succeeded);
			Test->TestEqual(TEXT("Revive never resets source health"), State->Source.Attributes->GetHealth(), 86.0f);
			Test->TestTrue(TEXT("Revive preserves persistent spec handles"), CaptureSpecs(State->Source.ASC.Get()) == State->OriginalSpecs);
		});
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKnightAIEQSAbortGenerationTest, "Knight.AI.Actions.EQSAbortGeneration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FKnightAIEQSAbortGenerationTest::RunTest(const FString& Parameters)
{
	using namespace KnightAIActionLifecycleTests;
	FKnightActionWorld World;
	const FKnightActionActor Source = SpawnActor(World.World.Get(), 1, true, FVector::ZeroVector);
	const FKnightActionActor Enemy = SpawnActor(World.World.Get(), 101, false, FVector(500.0, 0.0, 0.0));
	TStrongObjectPtr<UStateTree> Tree(LoadObject<UStateTree>(nullptr,
		TEXT("/Game/Knight/AI/Tests/ST_Knight_LifecycleEQS.ST_Knight_LifecycleEQS")));
	TStrongObjectPtr<UEnvQuery> Query(LoadObject<UEnvQuery>(nullptr,
		TEXT("/Game/Knight/AI/EQS_Knight_CombatPosition.EQS_Knight_CombatPosition")));
	if (!TestTrue(TEXT("EQS fixtures and compiled tree are real"), Source.IsReady() && Enemy.IsReady() &&
		Tree.IsValid() && Tree->IsReadyToRun() && Query.IsValid()))
	{
		return false;
	}
	const FKnightActionController AI = CreateController(Source, Tree.Get());
	if (!TestTrue(TEXT("EQS task has actual controlled-pawn context"), AI.IsReady() && AI.Tree.IsValid()))
	{
		return false;
	}
	UEnvQueryManager* Manager = UEnvQueryManager::GetCurrent(World.World.Get());
	if (!TestNotNull(TEXT("Owned server world has the real EQS manager"), Manager))
	{
		return false;
	}
	TestTrue(TEXT("Real query asset is configured"), AI.Context->SetRepositionQuery(Query.Get()).IsSuccess());
	TestTrue(TEXT("Query target is authority validated"), AI.Context->SetValidatedTarget(Enemy.Actor.Get()).IsSuccess());
	const FVector OriginalPosition = Source.Actor->GetActorLocation();
	AI.Tree->StartLogic();
	const TArray<int32> First = AI.Context->GetActiveEQSRequestIds();
	if (!TestEqual(TEXT("Real asynchronous query receives exactly one pending request ID"), First.Num(), 1))
	{
		return false;
	}
	TestTrue(TEXT("The real asynchronous task is running"), AI.Tree->IsRunning());
	AI.Tree->StopLogic(TEXT("Exact EQS abort regression"));
	TestEqual(TEXT("Task exit removes only its owned pending request"), AI.Context->GetActiveEQSRequestIds().Num(), 0);
	TestEqual(TEXT("Actual abort is recorded once"), AI.Context->GetAbortedEQSRequestCount(), 1);
	TestEqual(TEXT("Aborted query never records a successful completion"), AI.Context->GetCompletedEQSRequestCount(), 0);
	TestFalse(TEXT("Real manager confirms the old request is already aborted"), Manager->AbortQuery(First[0]));
	AI.Tree->StartLogic();
	const TArray<int32> Second = AI.Context->GetActiveEQSRequestIds();
	if (!TestEqual(TEXT("Restart has one fresh pending request"), Second.Num(), 1))
	{
		return false;
	}
	TestTrue(TEXT("Restarted query owns a different request generation"), Second[0] != First[0]);
	TestFalse(TEXT("Old request cannot abort its replacement"), Manager->AbortQuery(First[0]));
	TestTrue(TEXT("Replacement remains the sole owned query"), AI.Context->GetActiveEQSRequestIds() == Second);
	TestTrue(TEXT("Real EQS handover stops its old tree before controller loss"),
		UnpossessAfterStoppingTree(AI.Controller.Get()));
	TestFalse(TEXT("EQS handover leaves no running old tree"), AI.Tree->IsRunning());
	TestTrue(TEXT("EQS handover retains no task instance or execution frame"), HasClearedTreeExecution(AI.Tree.Get()));
	TestEqual(TEXT("Real controller loss aborts the owned replacement request"), AI.Context->GetActiveEQSRequestIds().Num(), 0);
	TestEqual(TEXT("Each owned generation is aborted exactly once"), AI.Context->GetAbortedEQSRequestCount(), 2);
	TestFalse(TEXT("Replacement is actually aborted in the manager"), Manager->AbortQuery(Second[0]));
	TestEqual(TEXT("Aborted tasks produced no query-completion success"), AI.Context->GetCompletedEQSRequestCount(), 0);
	TestTrue(TEXT("Pending query cancellation never teleports the pawn"), Source.Actor->GetActorLocation().Equals(OriginalPosition));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKnightAIBossHealthPhaseTest, "Knight.AI.Actions.BossHealthPhase",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FKnightAIBossHealthPhaseTest::RunTest(const FString& Parameters)
{
	using namespace KnightAIActionLifecycleTests;
	FKnightActionWorld World;
	FKnightActionActor BossActor = SpawnActor(World.World.Get(), 101, false, FVector::ZeroVector);
	const FKnightActionActor Human = SpawnActor(World.World.Get(), 1, true, FVector(300.0, 0.0, 0.0));
	const UKnightBossPhaseDefinition* Definition = ResolveDefinition<UKnightBossPhaseDefinition>(
		FPrimaryAssetId(TEXT("KnightBossPhase"), TEXT("ArenaBoss")));
	if (!TestTrue(TEXT("Boss uses actual ASC/life and authored phases"), BossActor.IsReady() && Human.IsReady() &&
		Definition != nullptr && Definition->GetPhases().Num() >= 2) ||
		!TestTrue(TEXT("Boss owns the existing shared combat grant"), GrantFoundation(BossActor, TEXT("FoundationCombat"))))
	{
		return false;
	}
	const FKnightActionController AI = CreateController(BossActor);
	if (!TestTrue(TEXT("Boss owns a real generic controller context"), AI.IsReady()))
	{
		return false;
	}
	UKnightBossPhaseComponent* Phases = NewObject<UKnightBossPhaseComponent>(BossActor.Actor.Get());
	BossActor.Actor->AddInstanceComponent(Phases);
	Phases->RegisterComponent();
	TestTrue(TEXT("Real phase component validates and binds current shared GAS specs"),
		Phases->InitializePhases(Definition->GetPrimaryAssetId(), AI.Context.Get()).IsSuccess());
	TestEqual(TEXT("Initial phase is authored phase zero"), Phases->GetPhaseIndex(), 0);
	const TArray<FGameplayAbilitySpecHandle> OriginalSpecs = CaptureSpecs(BossActor.ASC.Get());
	const FKnightEntityId OriginalId = BossActor.ASC->GetEntityId();
	const float Fraction = Definition->GetPhases()[1].HealthFraction;
	const float Damage = BossActor.Attributes->GetMaxHealth() * (1.0f - Fraction) + 1.0f;
	TestTrue(TEXT("Actual damage execution crosses authored threshold"),
		UKnightAbilitySystemComponent::ApplyDamage(Human.Actor.Get(), BossActor.Actor.Get(), Damage).IsSuccess());
	TestEqual(TEXT("Health delegate commits the actual second phase"), Phases->GetPhaseIndex(), 1);
	TestTrue(TEXT("Second phase selects its authored stable action set"),
		AI.Context->GetActionSetId() == Definition->GetPhases()[1].ActionSetId);
	TestEqual(TEXT("Phase pace remains the base decision interval"), AI.Context->GetBaseDecisionInterval(),
		Definition->GetPhases()[1].DecisionInterval);
	TestTrue(TEXT("Near scheduling cannot overwrite boss phase pace"), AI.Context->SetScheduledDecisionInterval(0.1f).IsSuccess());
	TestEqual(TEXT("Effective pace respects authored phase interval"), AI.Context->GetDecisionSnapshot().DecisionInterval,
		FMath::Max(0.1f, Definition->GetPhases()[1].DecisionInterval));
	TestTrue(TEXT("Boss phase transition preserves stable ASC identity"), BossActor.ASC->GetEntityId() == OriginalId);
	TestTrue(TEXT("Boss phase transition preserves every granted spec handle"), CaptureSpecs(BossActor.ASC.Get()) == OriginalSpecs);
	TestEqual(TEXT("Boss phase transition never duplicates grants"), BossActor.ASC->GetOwnedGrantCount(), 1);
	TestEqual(TEXT("Boss phase transition never resets health"), BossActor.Attributes->GetHealth(),
		BossActor.Attributes->GetMaxHealth() - Damage);
	TestTrue(TEXT("Explicit checkpoint resets only phase selection"), Phases->ResetForCheckpoint().IsSuccess());
	TestEqual(TEXT("Checkpoint phase reset returns to authored phase zero"), Phases->GetPhaseIndex(), 0);
	TestEqual(TEXT("Phase checkpoint does not revive or reset attributes"), BossActor.Attributes->GetHealth(),
		BossActor.Attributes->GetMaxHealth() - Damage);
	TestTrue(TEXT("Phase checkpoint preserves persistent grant handles"), CaptureSpecs(BossActor.ASC.Get()) == OriginalSpecs);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKnightAIResourceRecoveryTest, "Knight.AI.Actions.PeriodicResourceRecovery",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FKnightAIResourceRecoveryTest::RunTest(const FString& Parameters)
{
	using namespace KnightAIActionLifecycleTests;
	const TSharedRef<FKnightRecoveryState> State = MakeShared<FKnightRecoveryState>();
	if (!TestTrue(TEXT("Periodic recovery actors have real foundation state"), State->Source.IsReady() && State->Enemy.IsReady()) ||
		!TestTrue(TEXT("Recovery never replaces the existing grant"), GrantFoundation(State->Source)))
	{
		return false;
	}
	State->OriginalSpecs = CaptureSpecs(State->Source.ASC.Get());
	TestTrue(TEXT("Distinct source health is produced by actual damage GE"), UKnightAbilitySystemComponent::ApplyDamage(
		State->Enemy.Actor.Get(), State->Source.Actor.Get(), 14.0f).IsSuccess());
	State->Source.ASC->SetNumericAttributeBase(UKnightAttributeSet::GetStaminaAttribute(), 99.0f);
	State->Source.ASC->SetNumericAttributeBase(UKnightAttributeSet::GetRuneEnergyAttribute(), 99.0f);
	const FGameplayEffectSpecHandle Spec = State->Source.ASC->MakeOutgoingSpec(
		UKnightArenaResourceRecoveryEffect::StaticClass(), 1.0f, State->Source.ASC->MakeEffectContext());
	if (!TestTrue(TEXT("Native periodic recovery builds a real outgoing GE spec"), Spec.IsValid()))
	{
		return false;
	}
	State->RecoveryHandle = State->Source.ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	TestTrue(TEXT("Recovery owns an actual active effect handle"), State->RecoveryHandle.IsValid());
	TestEqual(TEXT("Recovery waits for its actual period"), State->Source.Attributes->GetStamina(), 99.0f);
	FAutomationTestBase* Test = this;
	WaitForWorldTime(Test, State->Context, 1.1f, [State, Test]()
		{
			Test->TestEqual(TEXT("Actual periodic stamina addition clamps to its maximum"), State->Source.Attributes->GetStamina(), 100.0f);
			Test->TestEqual(TEXT("Actual periodic rune addition clamps to its maximum"), State->Source.Attributes->GetRuneEnergy(), 100.0f);
			Test->TestEqual(TEXT("Periodic recovery never heals damage"), State->Source.Attributes->GetHealth(), 86.0f);
			Test->TestTrue(TEXT("Periodic recovery preserves every spec handle"), CaptureSpecs(State->Source.ASC.Get()) == State->OriginalSpecs);
			State->Source.ASC->SetNumericAttributeBase(UKnightAttributeSet::GetStaminaAttribute(), 10.0f);
			State->Source.ASC->SetNumericAttributeBase(UKnightAttributeSet::GetRuneEnergyAttribute(), 10.0f);
			Test->TestTrue(TEXT("Owner removes only its exact recovery handle"), State->Source.ASC->RemoveActiveGameplayEffect(State->RecoveryHandle));
			WaitForWorldTime(Test, State->Context, 1.1f, [State, Test]()
				{
					Test->TestEqual(TEXT("Removed recovery cannot increment stamina later"), State->Source.Attributes->GetStamina(), 10.0f);
					Test->TestEqual(TEXT("Removed recovery cannot increment rune later"), State->Source.Attributes->GetRuneEnergy(), 10.0f);
					Test->TestEqual(TEXT("Removed recovery preserves damaged health"), State->Source.Attributes->GetHealth(), 86.0f);
					Test->TestEqual(TEXT("Recovery lifecycle preserves one persistent grant"), State->Source.ASC->GetOwnedGrantCount(), 1);
				});
		});
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKnightAINineSpecializationTest, "Knight.AI.Actions.NineSpecializationSharedGAS",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace KnightAIActionLifecycleTests
{
	bool RunNineSpecializations(FAutomationTestBase* Test, bool bEquipment)
	{
		using namespace KnightAIActionLifecycleTests;
		const TSharedRef<FKnightActionWorld> OwnedWorld = MakeShared<FKnightActionWorld>();
		TStrongObjectPtr<UStateTree> Tree(LoadObject<UStateTree>(nullptr,
			TEXT("/Game/Knight/AI/Tests/ST_Knight_LifecyclePulse.ST_Knight_LifecyclePulse")));
		if (!Test->TestTrue(TEXT("The existing native action fixture StateTree is compiled"), Tree.IsValid() && Tree->IsReadyToRun()))
		{
			return false;
		}
		UKnightCombatDirectorSubsystem* Director = OwnedWorld->World->GetSubsystem<UKnightCombatDirectorSubsystem>();
		if (!Test->TestTrue(TEXT("Fixture reserves three separate DPS targets through real director budgets"),
			Director != nullptr && Director->TrySetBudgets(3, 1).IsSuccess()))
		{
			return false;
		}
		const TArray<FName> Specs = FKnightSkillCatalog::AllSpecs();
		TArray<FKnightActionActor> Sources;
		TArray<FKnightActionActor> Allies;
		TArray<FKnightActionActor> Enemies;
		TArray<FKnightActionController> Controllers;
		TArray<TWeakObjectPtr<UKnightSkillRuntimeComponent>> Runtimes;
		TArray<FDelegateHandle> EndHandles;
		const TSharedRef<TArray<int32>> Ended = MakeShared<TArray<int32>>();
		Ended->Init(0, Specs.Num());
		for (int32 Index = 0; Index < Specs.Num(); ++Index)
		{
			const FVector Origin(Index * 3000.0f, 0.0f, 0.0f);
			FKnightActionActor Source = SpawnActor(OwnedWorld->World.Get(), 1000 + Index * 3, true, Origin);
			const FKnightActionActor Ally = SpawnActor(OwnedWorld->World.Get(), 1001 + Index * 3, true,
				Origin + FVector(0.0f, 100.0f, 0.0f));
			const FKnightActionActor Enemy = SpawnActor(OwnedWorld->World.Get(), 1002 + Index * 3, false,
				Origin + FVector(250.0f, 0.0f, 0.0f));
			if (!Test->TestTrue(TEXT("Every source, injured ally and enemy uses initialized character-owned ASC"),
				Source.IsReady() && Ally.IsReady() && Enemy.IsReady() && GrantFoundation(Source)))
			{
				return false;
			}
			for (const FKnightActionActor& Actor : { Source, Ally, Enemy })
			{
				Actor.ASC->SetNumericAttributeBase(UKnightAttributeSet::GetMaxHealthAttribute(), 1000.0f);
				Actor.ASC->SetNumericAttributeBase(UKnightAttributeSet::GetHealthAttribute(), 1000.0f);
				Actor.ASC->SetNumericAttributeBase(UKnightAttributeSet::GetDefenseAttribute(), 0.0f);
			}
			Ally.ASC->SetNumericAttributeBase(UKnightAttributeSet::GetHealthAttribute(), 100.0f);
			Source.ASC->SetNumericAttributeBase(UKnightAttributeSet::GetAttackPowerAttribute(), 100.0f);
			Source.ASC->SetNumericAttributeBase(UKnightAttributeSet::GetCritChanceAttribute(), 0.0f);
			UKnightSkillRuntimeComponent* Runtime = NewObject<UKnightSkillRuntimeComponent>(Source.Actor.Get());
			Source.Actor->AddInstanceComponent(Runtime);
			Runtime->RegisterComponent();
			FKnightSkillBuild Build;
			Build.SpecId = Specs[Index];
			if (!Test->TestTrue(TEXT("Selected specialization projects four real GAS actions"), Runtime->ConfigureBuild(Build).IsSuccess()))
			{
				return false;
			}
			if (bEquipment)
			{
				const EKnightSkillRole Role = FKnightSkillCatalog::Role(Build.SpecId);
				FKnightGASProjectionPlan Plan;
				Plan.SourceKey = KnightProjection::RPGSourceKey;
				Plan.DefinitionId = FPrimaryAssetId(TEXT("KnightHero"), TEXT("Training_Red"));
				Plan.SkillBuild = Build;
				Plan.EquipmentCombatBuild.bEnabled = true;
				Plan.EquipmentCombatBuild.bHasHammer = true;
				Plan.MaxHealthBonus = Role == EKnightSkillRole::Tank ? 380.0f : 310.0f;
				Plan.AttackPowerBonus = Role == EKnightSkillRole::Tank ? 28.0f : Role == EKnightSkillRole::Healer ? 32.0f : 36.0f;
				Plan.DefenseBonus = Role == EKnightSkillRole::Tank ? 97.0f : 72.0f;
				Plan.CritChanceBonus = Role == EKnightSkillRole::Damage ? 0.09f : 0.02f;
				Plan.MasteryBonus = Role == EKnightSkillRole::Healer ? 0.09f : 0.04f;
				Plan.CooldownRecoveryBonus = Role == EKnightSkillRole::Tank ? 0.02f : 0.05f;
				Plan.VersatilityBonus = Role == EKnightSkillRole::Tank ? 0.12f : 0.07f;
				Plan.MaxPoiseBonus = 10.0f;
				FKnightMutationFence Fence;
				FKnightProjectionCapability Capability;
				if (!Test->TestTrue(TEXT("AI gear owns the same actual source projection fence as human gear"),
					Source.ASC->BeginMutationFence(FGuid::NewGuid(), Fence).IsSuccess()))
				{
					return false;
				}
				const bool bApplied = Source.ASC->PrepareSourceProjection(Fence, Plan, 0, Capability).IsSuccess()
					&& Source.ASC->StageSourceProjection(Fence, Capability).IsSuccess()
					&& Source.ASC->CommitSourceProjection(Fence, Capability).IsSuccess()
					&& Source.ASC->PublishSourceProjection(Fence, Capability).IsSuccess();
				if (!bApplied && Capability.IsValid())
				{
					Source.ASC->AbortSourceProjection(Fence, Capability);
				}
				Source.ASC->EndMutationFence(Fence);
				if (!Test->TestTrue(TEXT("Actual gear GAS stats and hammer descriptor commit before AI decisions"), bApplied))
				{
					return false;
				}
				Test->TestEqual(TEXT("AI consumes projected gear Power exactly once"), Source.Attributes->GetAttackPower(),
					100.0f + Plan.AttackPowerBonus);
				Test->TestFalse(TEXT("Equipped AI resolves the same hammer requirement as human"), Runtime->RequiresHammer());
			}
			FKnightSkillRuntimeSnapshot Snapshot = Runtime->CaptureSnapshot();
			// A retained major cooldown forces role selection to execute a combat action, rather than its opening buff.
			Snapshot.CooldownRemaining[3] = 60.0f;
			Runtime->RestoreSnapshot(Snapshot);
			const FKnightActionController AI = CreateController(Source, Tree.Get());
			if (!Test->TestTrue(TEXT("Every actual controller initializes its existing StateTree/GAS context"),
				AI.IsReady() && AI.Tree.IsValid() && AI.Context->SetActionSet(PulseSetId).IsSuccess()
				&& AI.Context->SetValidatedTarget(Enemy.Actor.Get()).IsSuccess()))
			{
				return false;
			}
			Sources.Add(Source);
			Allies.Add(Ally);
			Enemies.Add(Enemy);
			Controllers.Add(AI);
			Runtimes.Add(Runtime);
			EndHandles.Add(AI.Context->OnActionFinished.AddLambda([Ended, Index](FGuid Token, EKnightAIActionStatus Status)
			{
				++(*Ended)[Index];
			}));
		}
		for (const FKnightActionController& AI : Controllers)
		{
			AI.Tree->StartLogic();
		}
		WaitForWorldTime(Test, OwnedWorld, 1.8f,
			[Test, OwnedWorld, Specs, Sources, Allies, Enemies, Controllers, Runtimes, Ended, EndHandles]()
		{
			for (int32 Index = 0; Index < Specs.Num(); ++Index)
			{
				const FName Spec = Specs[Index];
				const FKnightActionController& AI = Controllers[Index];
				AI.Context->OnActionFinished.Remove(EndHandles[Index]);
				Test->TestEqual(*FString::Printf(TEXT("%s StateTree action completes once"), *Spec.ToString()), (*Ended)[Index], 1);
				Test->TestEqual(*FString::Printf(TEXT("%s reaches a real terminal StateTree state"), *Spec.ToString()),
					AI.Tree->GetStateTreeRunStatus(), EStateTreeRunStatus::Succeeded);
				Test->TestFalse(TEXT("Successful role action clears owned action token"), AI.Context->GetDecisionSnapshot().bActionRunning);
				Test->TestFalse(TEXT("Shared GAS specialization action has ended"), Runtimes[Index]->HasActiveSkill());
				const EKnightSkillRole Role = FKnightSkillCatalog::Role(Spec);
				if (Role == EKnightSkillRole::Tank)
				{
					const UKnightSkillRuntimeComponent* ShieldOwner = Spec == TEXT("Uranus")
						? Allies[Index].Actor->FindComponentByClass<UKnightSkillRuntimeComponent>() : Runtimes[Index].Get();
					Test->TestTrue(*FString::Printf(TEXT("%s AI applied a real protective shield"), *Spec.ToString()),
						ShieldOwner != nullptr && ShieldOwner->GetSkillState().Shield >= 200.0f);
				}
				else if (Role == EKnightSkillRole::Healer)
				{
					Test->TestTrue(*FString::Printf(TEXT("%s AI selects and really heals the missing-HP ally"), *Spec.ToString()),
						Allies[Index].Attributes->GetHealth() > 100.0f);
					Test->TestEqual(TEXT("Healer target selection keeps full-health self unchanged"), Sources[Index].Attributes->GetHealth(), 1000.0f);
				}
				else
				{
					Test->TestTrue(*FString::Printf(TEXT("%s AI applied real enemy damage"), *Spec.ToString()),
						Enemies[Index].Attributes->GetHealth() < 1000.0f);
					if (Spec == TEXT("Jupiter") || Spec == TEXT("Mars"))
					{
						Test->TestEqual(TEXT("DPS builder creates its one real specialization stack"), Runtimes[Index]->GetSkillState().Stacks, 1);
					}
					else
					{
						const UKnightSkillRuntimeComponent* Status = Enemies[Index].Actor->FindComponentByClass<UKnightSkillRuntimeComponent>();
						Test->TestTrue(TEXT("Pluto AI executes real Ice AoE and shared chilled state"),
							Status != nullptr && Status->GetSkillState().Chilled == 2);
					}
				}
			}
			Test->TestEqual(TEXT("All completed specialization actions release actual director leases"),
				OwnedWorld->World->GetSubsystem<UKnightCombatDirectorSubsystem>()->GetActiveLeaseCount(), 0);
		});
		return true;
	}
}

bool FKnightAINineSpecializationTest::RunTest(const FString& Parameters)
{
	return KnightAIActionLifecycleTests::RunNineSpecializations(this, false);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKnightAINineEquipmentTest, "Knight.Equipment.AI.NineSpecializationsSharedGAS",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FKnightAINineEquipmentTest::RunTest(const FString& Parameters)
{
	return KnightAIActionLifecycleTests::RunNineSpecializations(this, true);
}
#endif
