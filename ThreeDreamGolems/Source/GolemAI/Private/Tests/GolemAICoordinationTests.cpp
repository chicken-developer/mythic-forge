#include "Combat/GolemCombatDirectorSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Components/GolemAbilitySystemComponent.h"
#include "Components/GolemLifeComponent.h"
#include "CoreGlobals.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Events/GolemEventSubsystem.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "HAL/PlatformTime.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Tags/GolemGameplayTags.h"
#include "Threat/GolemThreatComponent.h"
#include "UObject/StrongObjectPtr.h"
#include <limits>

namespace GolemAICoordinationTests
{
	class FGolemCoordinationWorld final : public FNoncopyable
	{
	public:
		FGolemCoordinationWorld()
			: GameInstance(NewObject<UGameInstance>(GEngine))
		{
			GameInstance->InitializeStandalone();
			World = GameInstance->GetWorld();
			World->SetShouldTick(false);
			World->GetWorldSettings()->DefaultGameMode = AGameModeBase::StaticClass();
			FURL URL;
			URL.Map = TEXT("GolemAICoordinationWorld");
			URL.AddOption(TEXT("game=/Script/Engine.GameModeBase"));
			World->SetGameMode(URL);
			World->InitializeActorsForPlay(URL);
			World->BeginPlay();
		}
		~FGolemCoordinationWorld()
		{
			StopPlay();
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
		void StopPlay()
		{
			if (World.IsValid() && World->HasBegunPlay())
			{
				World->EndPlay(EEndPlayReason::Quit);
			}
		}
		TWeakObjectPtr<UWorld> World;

	private:
		TStrongObjectPtr<UGameInstance> GameInstance;
		uint64 LastTickedFrame = MAX_uint64;
	};

	struct FGolemCoordinationActor
	{
		TWeakObjectPtr<ACharacter> Actor;
		TWeakObjectPtr<UGolemAbilitySystemComponent> ASC;
		TWeakObjectPtr<UGolemAttributeSet> Attributes;
		TWeakObjectPtr<UGolemLifeComponent> Life;
		FGolemResult Initialization = FGolemResult::Failure(EGolemError::InvalidState, TEXT("ActorFixture"), FText());
		bool IsReady() const
		{
			return Initialization.IsSuccess() && Actor.IsValid() && ASC.IsValid() && Attributes.IsValid() &&
				Life.IsValid() && ASC->GetEntityId().IsValid() && Life->IsAlive();
		}
	};

	FGolemCoordinationActor SpawnActor(UWorld* World, uint32 Identity, bool bParty)
	{
		FGolemCoordinationActor Fixture;
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Fixture.Actor = World->SpawnActor<ACharacter>(FVector(Identity * 100.0f, 0.0f, 0.0f),
			FRotator::ZeroRotator, SpawnParameters);
		if (!Fixture.Actor.IsValid())
		{
			return Fixture;
		}
		Fixture.Actor->GetCharacterMovement()->DisableMovement();
		Fixture.Actor->SetReplicates(true);
		Fixture.ASC = NewObject<UGolemAbilitySystemComponent>(Fixture.Actor.Get());
		Fixture.Actor->AddInstanceComponent(Fixture.ASC.Get());
		Fixture.ASC->RegisterComponent();
		Fixture.Attributes = NewObject<UGolemAttributeSet>(Fixture.Actor.Get());
		Fixture.Life = NewObject<UGolemLifeComponent>(Fixture.Actor.Get());
		Fixture.Actor->AddInstanceComponent(Fixture.Life.Get());
		Fixture.Life->RegisterComponent();
		Fixture.Initialization = Fixture.ASC->InitializeGolemActor(Fixture.Actor.Get(), Fixture.Attributes.Get(), Fixture.Life.Get(),
			FGolemEntityId(FGuid(0, 0, 0, Identity)), FGolemAttributeDefaults(), bParty);
		if (Fixture.Initialization.IsSuccess() && !bParty)
		{
			Fixture.ASC->SetLooseGameplayTagCount(GolemGameplayTags::Role_Enemy, 1,
				EGameplayTagReplicationState::TagAndCountToAll);
		}
		return Fixture;
	}

	/** TimerManager runs once per engine frame. Each latent update advances this owned world once. */
	class FGolemWaitForCoordinationTimeCommand final : public IAutomationLatentCommand
	{
	public:
		FGolemWaitForCoordinationTimeCommand(FAutomationTestBase* InTest,
			const TSharedRef<FGolemCoordinationWorld>& InContext, float Seconds, TFunction<void()> InCompletion)
			: Test(InTest), Context(InContext), Completion(MoveTemp(InCompletion)),
			TargetWorldTime(InContext->World->GetTimeSeconds() + Seconds), StartedAt(FPlatformTime::Seconds())
		{
		}
		virtual bool Update() override
		{
			if (!Context->World.IsValid() || !Context->World->HasBegunPlay() ||
				FPlatformTime::Seconds() - StartedAt > 20.0)
			{
				Test->AddError(TEXT("Coordination timer fixture lost its owned world or exceeded the bounded wait."));
				return true;
			}
			if (!Context->TickOnce() || Context->World->GetTimeSeconds() < TargetWorldTime)
			{
				return false;
			}
			Completion();
			Completion = nullptr;
			return true;
		}

	private:
		FAutomationTestBase* Test;
		TSharedRef<FGolemCoordinationWorld> Context;
		TFunction<void()> Completion;
		double TargetWorldTime;
		double StartedAt;
	};

	void WaitForWorldTime(FAutomationTestBase* Test, const TSharedRef<FGolemCoordinationWorld>& Context,
		float Seconds, TFunction<void()> Completion)
	{
		FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FGolemWaitForCoordinationTimeCommand>(
			Test, Context, Seconds, MoveTemp(Completion)));
	}

	struct FGolemThreatTestState
	{
		FGolemThreatTestState()
			: Context(MakeShared<FGolemCoordinationWorld>()), Enemy(SpawnActor(Context->World.Get(), 101, false)),
			First(SpawnActor(Context->World.Get(), 1, true)), Second(SpawnActor(Context->World.Get(), 2, true)),
			Third(SpawnActor(Context->World.Get(), 3, true))
		{
		}
		~FGolemThreatTestState()
		{
			if (Threat.IsValid())
			{
				Threat->OnThreatChanged.Remove(ChangeHandle);
			}
		}
		TSharedRef<FGolemCoordinationWorld> Context;
		FGolemCoordinationActor Enemy;
		FGolemCoordinationActor First;
		FGolemCoordinationActor Second;
		FGolemCoordinationActor Third;
		TWeakObjectPtr<UGolemThreatComponent> Threat;
		FDelegateHandle ChangeHandle;
		int32 InitialSubscriptions = 0;
		int32 Changes = 0;
	};

	struct FGolemLeaseTestState
	{
		FGolemLeaseTestState()
			: Context(MakeShared<FGolemCoordinationWorld>()), First(SpawnActor(Context->World.Get(), 101, false)),
			Second(SpawnActor(Context->World.Get(), 102, false)), Third(SpawnActor(Context->World.Get(), 103, false)),
			TargetOne(SpawnActor(Context->World.Get(), 1, true)), TargetTwo(SpawnActor(Context->World.Get(), 2, true))
		{
			Director = Context->World->GetSubsystem<UGolemCombatDirectorSubsystem>();
		}
		~FGolemLeaseTestState()
		{
			if (Director.IsValid())
			{
				Director->OnAttackLeaseReleased.Remove(ReleaseHandle);
			}
		}
		TSharedRef<FGolemCoordinationWorld> Context;
		FGolemCoordinationActor First;
		FGolemCoordinationActor Second;
		FGolemCoordinationActor Third;
		FGolemCoordinationActor TargetOne;
		FGolemCoordinationActor TargetTwo;
		TWeakObjectPtr<UGolemCombatDirectorSubsystem> Director;
		FGolemAttackLease ExpiringLease;
		FDelegateHandle ReleaseHandle;
		int32 ExpiryNotifications = 0;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGolemThreatConfirmationTest, "Golem.AI.Coordination.ConfirmedThreat",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGolemThreatConfirmationTest::RunTest(const FString& Parameters)
{
	using namespace GolemAICoordinationTests;
	const TSharedRef<FGolemThreatTestState> State = MakeShared<FGolemThreatTestState>();
	const FGolemCoordinationActor& Enemy = State->Enemy;
	const FGolemCoordinationActor& First = State->First;
	const FGolemCoordinationActor& Second = State->Second;
	const FGolemCoordinationActor& Third = State->Third;
	if (!TestTrue(TEXT("All threat fixtures completed real foundation initialization"),
		Enemy.IsReady() && First.IsReady() && Second.IsReady() && Third.IsReady()))
	{
		return false;
	}
	UGolemEventSubsystem* Bus = State->Context->World->GetSubsystem<UGolemEventSubsystem>();
	State->InitialSubscriptions = Bus->GetSubscriptionCount();
	UGolemThreatComponent* Threat = NewObject<UGolemThreatComponent>(Enemy.Actor.Get());
	State->Threat = Threat;
	Enemy.Actor->AddInstanceComponent(Threat);
	Threat->RegisterComponent();
	TestTrue(TEXT("Deferred foundation composition initializes threat"), Threat->InitializeThreat().IsSuccess());
	TestTrue(TEXT("Threat initialization is idempotent"), Threat->InitializeThreat().IsSuccess());
	TestEqual(TEXT("Exactly one owned confirmed-damage subscription"), Bus->GetSubscriptionCount(), State->InitialSubscriptions + 1);
	TestTrue(TEXT("Finite native threat tuning is accepted"), Threat->TrySetThreatTuning(10.0f, 2.0f, 100.0f).IsSuccess());
	const TWeakPtr<FGolemThreatTestState> WeakState(State);
	State->ChangeHandle = Threat->OnThreatChanged.AddLambda([WeakState]()
		{
			if (const TSharedPtr<FGolemThreatTestState> Pinned = WeakState.Pin())
			{
				++Pinned->Changes;
			}
		});
	TestTrue(TEXT("Threat trigger is actual GAS damage execution"),
		UGolemAbilitySystemComponent::ApplyDamage(First.Actor.Get(), Enemy.Actor.Get(), 20.0f).IsSuccess());
	TestEqual(TEXT("Real GE reduced owner health"), Enemy.Attributes->GetHealth(), 80.0f);
	TestEqual(TEXT("Confirmed damage creates the scaled threat on stable source identity"),
		Threat->GetThreat(First.ASC->GetEntityId()), 40.0f);
	TestTrue(TEXT("Perception can explicitly add awareness"), Threat->ObserveTarget(Second.Actor.Get(), 40.0f).IsSuccess());
	TestTrue(TEXT("Equal threat uses deterministic stable-ID order"), Threat->GetHighestThreatTarget() == First.Actor.Get());
	Second.ASC->SetLooseGameplayTagCount(GolemGameplayTags::Role_Party_Golem, 0,
		EGameplayTagReplicationState::TagAndCountToAll);
	Second.ASC->SetLooseGameplayTagCount(GolemGameplayTags::Role_Enemy, 1,
		EGameplayTagReplicationState::TagAndCountToAll);
	TestEqual(TEXT("A newly friendly target is excluded without a mutating query"), Threat->GetThreatCount(), 1);
	TestEqual(TEXT("Friendly query preserves its stored entry until lifecycle maintenance"), Threat->GetTrackedEntryCount(), 2);
	Second.ASC->SetLooseGameplayTagCount(GolemGameplayTags::Role_Enemy, 0,
		EGameplayTagReplicationState::TagAndCountToAll);
	Second.ASC->SetLooseGameplayTagCount(GolemGameplayTags::Role_Party_Golem, 1,
		EGameplayTagReplicationState::TagAndCountToAll);
	const int32 BeforeQueries = State->Changes;
	for (int32 Index = 0; Index < 10; ++Index)
	{
		Threat->GetHighestThreatTarget();
		Threat->GetThreatSnapshot();
		Threat->GetThreatCount();
	}
	TestEqual(TEXT("Read queries emit no mutation notifications"), State->Changes, BeforeQueries);
	TestEqual(TEXT("Read queries preserve stored entries"), Threat->GetTrackedEntryCount(), 2);
	TestEqual(TEXT("Nonfinite threat is rejected"),
		Threat->AddThreat(First.Actor.Get(), std::numeric_limits<float>::quiet_NaN()).Error, EGolemError::InvalidData);
	TestEqual(TEXT("Rejected threat is atomic"), Threat->GetThreat(First.ASC->GetEntityId()), 40.0f);
	TestEqual(TEXT("Rejected threat emits no change"), State->Changes, BeforeQueries);
	TestTrue(TEXT("Entries start bounded maintenance"), Threat->HasMaintenanceTimer());
	FAutomationTestBase* Test = this;
	WaitForWorldTime(Test, State->Context, 1.0f, [State, Test]()
		{
			UGolemThreatComponent* CurrentThreat = State->Threat.Get();
			if (!Test->TestNotNull(TEXT("Latent threat fixture retains its native component"), CurrentThreat))
			{
				return;
			}
			Test->TestTrue(TEXT("Threat decays with actual world time"),
				FMath::IsNearlyEqual(CurrentThreat->GetThreat(State->First.ASC->GetEntityId()), 30.0f, 0.6f));
			Test->TestTrue(TEXT("Bounded positive addition is accepted"),
				CurrentThreat->AddThreat(State->First.Actor.Get(), 1000000.0f).IsSuccess());
			Test->TestEqual(TEXT("Accumulation clamps to configured maximum"),
				CurrentThreat->GetThreat(State->First.ASC->GetEntityId()), 100.0f);
			State->Second.Actor->Destroy();
			Test->TestEqual(TEXT("Target EndPlay removes its stored awareness immediately"), CurrentThreat->GetTrackedEntryCount(), 1);
			Test->TestTrue(TEXT("Actual incoming GE downs a party target"), UGolemAbilitySystemComponent::ApplyDamage(
				State->Enemy.Actor.Get(), State->First.Actor.Get(), 200.0f).IsSuccess());
			Test->TestTrue(TEXT("Party target entered actual Downed life state"), State->First.Life->IsDowned());
			Test->TestEqual(TEXT("Target life delegate purges stored threat immediately"), CurrentThreat->GetTrackedEntryCount(), 0);
			Test->TestFalse(TEXT("No target means no maintenance timer"), CurrentThreat->HasMaintenanceTimer());
			Test->TestTrue(TEXT("New awareness can be reacquired"), CurrentThreat->ObserveTarget(State->Third.Actor.Get()).IsSuccess());
			Test->TestTrue(TEXT("Actual damage kills the enemy owner"), UGolemAbilitySystemComponent::ApplyDamage(
				State->Third.Actor.Get(), State->Enemy.Actor.Get(), 200.0f).IsSuccess());
			Test->TestTrue(TEXT("Threat owner is actually Dead"), State->Enemy.Life->IsDead());
			Test->TestEqual(TEXT("Owner death clears all stored threat"), CurrentThreat->GetTrackedEntryCount(), 0);
			Test->TestFalse(TEXT("Owner death stops maintenance"), CurrentThreat->HasMaintenanceTimer());
			Test->TestTrue(TEXT("Real foundation checkpoint restores the owner"), State->Enemy.Life->ResetForCheckpoint().IsSuccess());
			Test->TestTrue(TEXT("Explicit awareness still works after foundation restore"),
				CurrentThreat->ObserveTarget(State->Third.Actor.Get(), 1.0f).IsSuccess());
			WaitForWorldTime(Test, State->Context, 0.4f, [State, Test]()
				{
					UGolemThreatComponent* ExpiredThreat = State->Threat.Get();
					if (!Test->TestNotNull(TEXT("Decay fixture retains its native component"), ExpiredThreat))
					{
						return;
					}
					Test->TestEqual(TEXT("Decay to zero purges the stored entry"), ExpiredThreat->GetTrackedEntryCount(), 0);
					Test->TestFalse(TEXT("Expired awareness leaves no timer"), ExpiredThreat->HasMaintenanceTimer());
					ExpiredThreat->OnThreatChanged.Remove(State->ChangeHandle);
					ExpiredThreat->DestroyComponent();
					const UGolemEventSubsystem* CurrentBus = State->Context->World->GetSubsystem<UGolemEventSubsystem>();
					Test->TestEqual(TEXT("Component teardown removes only its own bus subscription"),
						CurrentBus->GetSubscriptionCount(), State->InitialSubscriptions);
				});
		});
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGolemAttackLeaseBudgetTest, "Golem.AI.Coordination.AttackBudgets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGolemAttackLeaseBudgetTest::RunTest(const FString& Parameters)
{
	using namespace GolemAICoordinationTests;
	FGolemCoordinationWorld Context;
	const FGolemCoordinationActor First = SpawnActor(Context.World.Get(), 101, false);
	const FGolemCoordinationActor Second = SpawnActor(Context.World.Get(), 102, false);
	const FGolemCoordinationActor Third = SpawnActor(Context.World.Get(), 103, false);
	const FGolemCoordinationActor TargetOne = SpawnActor(Context.World.Get(), 1, true);
	const FGolemCoordinationActor TargetTwo = SpawnActor(Context.World.Get(), 2, true);
	const FGolemCoordinationActor TargetThree = SpawnActor(Context.World.Get(), 3, true);
	if (!TestTrue(TEXT("All budget fixtures completed real foundation initialization"), First.IsReady() &&
		Second.IsReady() && Third.IsReady() && TargetOne.IsReady() && TargetTwo.IsReady() && TargetThree.IsReady()))
	{
		return false;
	}
	UGolemCombatDirectorSubsystem* Director = Context.World->GetSubsystem<UGolemCombatDirectorSubsystem>();
	if (!TestNotNull(TEXT("Real server world owns the attack director"), Director))
	{
		return false;
	}
	TestEqual(TEXT("Default global budget"), Director->GetGlobalBudget(), 2);
	TestEqual(TEXT("Default target budget"), Director->GetPerTargetBudget(), 1);
	TestFalse(TEXT("Idle director has no timer"), Director->HasMaintenanceTimer());
	TestEqual(TEXT("Invalid budgets are rejected"), Director->TrySetBudgets(0, 1).Error, EGolemError::InvalidData);
	FGolemAttackLease One;
	FGolemAttackLease Two;
	FGolemAttackLease Rejected;
	TestTrue(TEXT("First contender acquires an actual lease"),
		Director->TryAcquireAttackToken(First.Actor.Get(), TargetOne.Actor.Get(), 5.0f, One).IsSuccess());
	TestEqual(TEXT("A second contender cannot attack an occupied target"),
		Director->TryAcquireAttackToken(Second.Actor.Get(), TargetOne.Actor.Get(), 5.0f, Rejected).Error,
		EGolemError::CapacityExceeded);
	TestFalse(TEXT("Failed acquire returns no usable capability"), Rejected.IsValid());
	TestEqual(TEXT("A source cannot hold two leases"),
		Director->TryAcquireAttackToken(First.Actor.Get(), TargetTwo.Actor.Get(), 5.0f, Rejected).Error, EGolemError::Conflict);
	TestTrue(TEXT("Different target fits the remaining global budget"),
		Director->TryAcquireAttackToken(Second.Actor.Get(), TargetTwo.Actor.Get(), 5.0f, Two).IsSuccess());
	TestEqual(TEXT("Third contender cannot exceed global budget"),
		Director->TryAcquireAttackToken(Third.Actor.Get(), TargetThree.Actor.Get(), 5.0f, Rejected).Error,
		EGolemError::CapacityExceeded);
	TestEqual(TEXT("Budget tuning cannot displace active leases"), Director->TrySetBudgets(3, 2).Error, EGolemError::Conflict);
	TestEqual(TEXT("Failed tuning keeps the budget"), Director->GetGlobalBudget(), 2);
	TestEqual(TEXT("Both accepted leases remain active"), Director->GetActiveLeaseCount(), 2);
	TestEqual(TEXT("One accepted lease belongs to target one"), Director->GetActiveLeaseCountForTarget(TargetOne.ASC->GetEntityId()), 1);
	TestTrue(TEXT("Active leases start maintenance"), Director->HasMaintenanceTimer());
	FGolemAttackLease Forged = One;
	Forged.TargetId = TargetTwo.ASC->GetEntityId();
	TestEqual(TEXT("Forged capability fields cannot release a valid GUID"),
		Director->ReleaseAttackToken(First.Actor.Get(), Forged).Error, EGolemError::NotOwned);
	TestEqual(TEXT("Foreign source cannot release another source's lease"),
		Director->ReleaseAttackToken(Second.Actor.Get(), One).Error, EGolemError::NotOwned);
	int32 Releases = 0;
	const FDelegateHandle ReleaseHandle = Director->OnAttackLeaseReleased.AddLambda(
		[&Releases](const FGolemAttackLease& Lease, EGolemAttackLeaseReleaseReason Reason)
		{
			++Releases;
		});
	ON_SCOPE_EXIT
	{
		Director->OnAttackLeaseReleased.Remove(ReleaseHandle);
	};
	TestTrue(TEXT("Owner can release its exact lease"), Director->ReleaseAttackToken(First.Actor.Get(), One).IsSuccess());
	TestTrue(TEXT("Exact repeated own release is idempotent"), Director->ReleaseAttackToken(First.Actor.Get(), One).IsSuccess());
	TestEqual(TEXT("Repeated release emits no duplicate callback"), Releases, 1);
	FGolemAttackLease Replacement;
	TestTrue(TEXT("Released source can acquire a new capability"),
		Director->TryAcquireAttackToken(First.Actor.Get(), TargetThree.Actor.Get(), 5.0f, Replacement).IsSuccess());
	TestTrue(TEXT("New request owns a fresh GUID"), Replacement.LeaseId != One.LeaseId);
	TestEqual(TEXT("Old capability cannot release a replacement lease"),
		Director->ReleaseAttackToken(First.Actor.Get(), One).Error, EGolemError::Conflict);
	TestTrue(TEXT("Replacement remains active after stale release"), Director->IsLeaseActive(Replacement));
	const int32 BeforeQueries = Releases;
	for (int32 Index = 0; Index < 10; ++Index)
	{
		Director->IsLeaseActive(Replacement);
		Director->GetActiveLeaseCount();
		Director->GetActiveLeaseCountForTarget(TargetThree.ASC->GetEntityId());
	}
	TestEqual(TEXT("Read-only lease queries emit no release callbacks"), Releases, BeforeQueries);
	TestTrue(TEXT("Replacement exact release succeeds"), Director->ReleaseAttackToken(First.Actor.Get(), Replacement).IsSuccess());
	TestTrue(TEXT("Second contender exact release succeeds"), Director->ReleaseAttackToken(Second.Actor.Get(), Two).IsSuccess());
	TestEqual(TEXT("All explicit releases restore budget"), Director->GetActiveLeaseCount(), 0);
	TestFalse(TEXT("Empty director stops maintenance"), Director->HasMaintenanceTimer());
	TestTrue(TEXT("Story can limit hostile coordination without starving companion slots"),
		Director->TrySetTeamBudgets(2, 1, 1).IsSuccess());
	TestTrue(TEXT("One hostile fits Story coordination"),
		Director->TryAcquireAttackToken(First.Actor.Get(), TargetOne.Actor.Get(), 5.0f, One).IsSuccess());
	TestEqual(TEXT("A different Golem is protected by the hostile side cap"),
		Director->TryAcquireAttackToken(Second.Actor.Get(), TargetTwo.Actor.Get(), 5.0f, Rejected).Error, EGolemError::CapacityExceeded);
	TestTrue(TEXT("Companion retains a separate opportunity"),
		Director->TryAcquireAttackToken(TargetTwo.Actor.Get(), Second.Actor.Get(), 5.0f, Two).IsSuccess());
	FGolemAttackLease PartySecond;
	TestTrue(TEXT("Second companion retains coordination slot"),
		Director->TryAcquireAttackToken(TargetThree.Actor.Get(), Third.Actor.Get(), 5.0f, PartySecond).IsSuccess());
	Director->ReleaseAttackToken(First.Actor.Get(), One);
	TestEqual(TEXT("Party cap still applies when the hostile slot is empty"),
		Director->TryAcquireAttackToken(TargetOne.Actor.Get(), First.Actor.Get(), 5.0f, Rejected).Error, EGolemError::CapacityExceeded);
	TestEqual(TEXT("Active leases prevent difficulty retuning"), Director->TrySetTeamBudgets(2, 2, 1).Error, EGolemError::Conflict);
	Director->ReleaseAttackToken(TargetTwo.Actor.Get(), Two);
	Director->ReleaseAttackToken(TargetThree.Actor.Get(), PartySecond);
	TestTrue(TEXT("Legacy tuning remains available after team release"), Director->TrySetBudgets(2, 1).IsSuccess());
	TestEqual(TEXT("Nonfinite expiry request is rejected"), Director->TryAcquireAttackToken(Third.Actor.Get(),
		TargetThree.Actor.Get(), std::numeric_limits<float>::quiet_NaN(), Rejected).Error, EGolemError::InvalidData);
	FGolemCoordinationWorld ForeignContext;
	const FGolemCoordinationActor Foreign = SpawnActor(ForeignContext.World.Get(), 999, true);
	if (!TestTrue(TEXT("Cross-world target completed real foundation initialization"), Foreign.IsReady()))
	{
		return false;
	}
	TestEqual(TEXT("A lease cannot cross real server worlds"),
		Director->TryAcquireAttackToken(Third.Actor.Get(), Foreign.Actor.Get(), 1.0f, Rejected).Error, EGolemError::InvalidData);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGolemAttackLeaseLifecycleTest, "Golem.AI.Coordination.AttackLeaseLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGolemAttackLeaseLifecycleTest::RunTest(const FString& Parameters)
{
	using namespace GolemAICoordinationTests;
	const TSharedRef<FGolemLeaseTestState> State = MakeShared<FGolemLeaseTestState>();
	const FGolemCoordinationActor& First = State->First;
	const FGolemCoordinationActor& Second = State->Second;
	const FGolemCoordinationActor& Third = State->Third;
	const FGolemCoordinationActor& TargetOne = State->TargetOne;
	const FGolemCoordinationActor& TargetTwo = State->TargetTwo;
	if (!TestTrue(TEXT("All lifecycle fixtures completed real foundation initialization"), First.IsReady() &&
		Second.IsReady() && Third.IsReady() && TargetOne.IsReady() && TargetTwo.IsReady()))
	{
		return false;
	}
	UGolemCombatDirectorSubsystem* Director = State->Director.Get();
	if (!TestNotNull(TEXT("Lifecycle uses real world director"), Director))
	{
		return false;
	}
	TestTrue(TEXT("Empty director accepts bounded multiple-target budget"), Director->TrySetBudgets(3, 2).IsSuccess());
	FGolemAttackLease One;
	FGolemAttackLease Two;
	FGolemAttackLease Three;
	TestTrue(TEXT("First source reserves shared target"),
		Director->TryAcquireAttackToken(First.Actor.Get(), TargetOne.Actor.Get(), 5.0f, One).IsSuccess());
	TestTrue(TEXT("Second source reserves the same target within tuned budget"),
		Director->TryAcquireAttackToken(Second.Actor.Get(), TargetOne.Actor.Get(), 5.0f, Two).IsSuccess());
	TestTrue(TEXT("Third source reserves independent target"),
		Director->TryAcquireAttackToken(Third.Actor.Get(), TargetTwo.Actor.Get(), 5.0f, Three).IsSuccess());
	TestTrue(TEXT("Actual GE downs the shared target"),
		UGolemAbilitySystemComponent::ApplyDamage(Third.Actor.Get(), TargetOne.Actor.Get(), 200.0f).IsSuccess());
	TestTrue(TEXT("Shared target is actually downed"), TargetOne.Life->IsDowned());
	TestFalse(TEXT("First shared lease is reclaimed synchronously"), Director->IsLeaseActive(One));
	TestFalse(TEXT("Second shared lease is reclaimed synchronously"), Director->IsLeaseActive(Two));
	TestTrue(TEXT("Independent target lease survives shared-target loss"), Director->IsLeaseActive(Three));
	TestEqual(TEXT("Exactly one lease remains after target loss"), Director->GetActiveLeaseCount(), 1);
	TestTrue(TEXT("Actual GE kills the remaining enemy source"),
		UGolemAbilitySystemComponent::ApplyDamage(TargetTwo.Actor.Get(), Third.Actor.Get(), 200.0f).IsSuccess());
	TestTrue(TEXT("Source is actually dead"), Third.Life->IsDead());
	TestEqual(TEXT("Source death releases the remaining lease immediately"), Director->GetActiveLeaseCount(), 0);
	TestFalse(TEXT("Death leaves no maintenance timer"), Director->HasMaintenanceTimer());
	FGolemAttackLease Rejected;
	TestEqual(TEXT("Dead source cannot reacquire"), Director->TryAcquireAttackToken(Third.Actor.Get(),
		TargetTwo.Actor.Get(), 1.0f, Rejected).Error, EGolemError::InvalidState);
	TestTrue(TEXT("Another actual GE transitions downed target to Dead"),
		UGolemAbilitySystemComponent::ApplyDamage(First.Actor.Get(), TargetOne.Actor.Get(), 20.0f).IsSuccess());
	TestTrue(TEXT("Shared target is actually dead"), TargetOne.Life->IsDead());
	TestEqual(TEXT("Dead target cannot receive an attack lease"), Director->TryAcquireAttackToken(First.Actor.Get(),
		TargetOne.Actor.Get(), 1.0f, Rejected).Error, EGolemError::InvalidState);
	TestTrue(TEXT("Actual checkpoint restores test target"), TargetOne.Life->ResetForCheckpoint().IsSuccess());
	TestTrue(TEXT("A live source can reacquire after shared-target restore"),
		Director->TryAcquireAttackToken(First.Actor.Get(), TargetOne.Actor.Get(), 5.0f, One).IsSuccess());
	First.Actor->Destroy();
	TestEqual(TEXT("Source actor EndPlay returns budget immediately"), Director->GetActiveLeaseCount(), 0);
	TestTrue(TEXT("Another source can reserve the restored target"),
		Director->TryAcquireAttackToken(Second.Actor.Get(), TargetOne.Actor.Get(), 5.0f, Two).IsSuccess());
	TargetOne.Actor->Destroy();
	TestEqual(TEXT("Target actor EndPlay returns budget immediately"), Director->GetActiveLeaseCount(), 0);
	APlayerController* Controller = State->Context->World->SpawnActor<APlayerController>();
	Controller->Possess(Second.Actor.Get());
	TestTrue(TEXT("Lease snapshots actual controller ownership"),
		Director->TryAcquireAttackToken(Second.Actor.Get(), TargetTwo.Actor.Get(), 5.0f, Two).IsSuccess());
	Controller->UnPossess();
	TestEqual(TEXT("Actual controller change reclaims the lease immediately"), Director->GetActiveLeaseCount(), 0);
	TestFalse(TEXT("Controller change leaves no timer"), Director->HasMaintenanceTimer());
	const TWeakPtr<FGolemLeaseTestState> WeakState(State);
	State->ReleaseHandle = Director->OnAttackLeaseReleased.AddLambda(
		[WeakState](const FGolemAttackLease& Lease, EGolemAttackLeaseReleaseReason Reason)
		{
			if (const TSharedPtr<FGolemLeaseTestState> Pinned = WeakState.Pin();
				Pinned.IsValid() && Reason == EGolemAttackLeaseReleaseReason::Expired && Lease == Pinned->ExpiringLease)
			{
				++Pinned->ExpiryNotifications;
			}
		});
	TestTrue(TEXT("Short bounded lease is acquired"),
		Director->TryAcquireAttackToken(Second.Actor.Get(), TargetTwo.Actor.Get(), 0.1f, State->ExpiringLease).IsSuccess());
	FAutomationTestBase* Test = this;
	WaitForWorldTime(Test, State->Context, 0.25f, [State, Test]()
		{
			UGolemCombatDirectorSubsystem* CurrentDirector = State->Director.Get();
			if (!Test->TestNotNull(TEXT("Latent expiry fixture retains its native director"), CurrentDirector))
			{
				return;
			}
			Test->TestFalse(TEXT("Actual world timer expires the capability"), CurrentDirector->IsLeaseActive(State->ExpiringLease));
			Test->TestEqual(TEXT("Expiry restores the entire budget"), CurrentDirector->GetActiveLeaseCount(), 0);
			Test->TestFalse(TEXT("Expiry stops the maintenance timer"), CurrentDirector->HasMaintenanceTimer());
			Test->TestEqual(TEXT("Actual timer broadcasts one confirmed expiry release"), State->ExpiryNotifications, 1);
			Test->TestTrue(TEXT("Expired exact capability release remains idempotent"),
				CurrentDirector->ReleaseAttackToken(State->Second.Actor.Get(), State->ExpiringLease).IsSuccess());
			FGolemAttackLease TeardownLease;
			Test->TestTrue(TEXT("World teardown starts with a real outstanding lease"), CurrentDirector->TryAcquireAttackToken(
				State->Second.Actor.Get(), State->TargetTwo.Actor.Get(), 10.0f, TeardownLease).IsSuccess());
			State->Context->StopPlay();
			Test->TestEqual(TEXT("World EndPlay clears leases before deinitialization"), CurrentDirector->GetActiveLeaseCount(), 0);
			Test->TestFalse(TEXT("World EndPlay clears its timer"), CurrentDirector->HasMaintenanceTimer());
			FGolemAttackLease RejectedLease;
			Test->TestEqual(TEXT("Ended world rejects new commands"), CurrentDirector->TryAcquireAttackToken(
				State->Second.Actor.Get(), State->TargetTwo.Actor.Get(), 1.0f, RejectedLease).Error, EGolemError::ShuttingDown);
		});
	return true;
}

#endif
