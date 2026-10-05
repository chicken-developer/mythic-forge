#include "Combat/KnightCombatDirectorSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Components/KnightAbilitySystemComponent.h"
#include "Components/KnightLifeComponent.h"
#include "CoreGlobals.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Events/KnightEventSubsystem.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "HAL/PlatformTime.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Tags/KnightGameplayTags.h"
#include "Threat/KnightThreatComponent.h"
#include "UObject/StrongObjectPtr.h"
#include <limits>

namespace KnightAICoordinationTests
{
	class FKnightCoordinationWorld final : public FNoncopyable
	{
	public:
		FKnightCoordinationWorld()
			: GameInstance(NewObject<UGameInstance>(GEngine))
		{
			GameInstance->InitializeStandalone();
			World = GameInstance->GetWorld();
			World->SetShouldTick(false);
			World->GetWorldSettings()->DefaultGameMode = AGameModeBase::StaticClass();
			FURL URL;
			URL.Map = TEXT("KnightAICoordinationWorld");
			URL.AddOption(TEXT("game=/Script/Engine.GameModeBase"));
			World->SetGameMode(URL);
			World->InitializeActorsForPlay(URL);
			World->BeginPlay();
		}
		~FKnightCoordinationWorld()
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

	struct FKnightCoordinationActor
	{
		TWeakObjectPtr<ACharacter> Actor;
		TWeakObjectPtr<UKnightAbilitySystemComponent> ASC;
		TWeakObjectPtr<UKnightAttributeSet> Attributes;
		TWeakObjectPtr<UKnightLifeComponent> Life;
		FKnightResult Initialization = FKnightResult::Failure(EKnightError::InvalidState, TEXT("ActorFixture"), FText());
		bool IsReady() const
		{
			return Initialization.IsSuccess() && Actor.IsValid() && ASC.IsValid() && Attributes.IsValid() &&
				Life.IsValid() && ASC->GetEntityId().IsValid() && Life->IsAlive();
		}
	};

	FKnightCoordinationActor SpawnActor(UWorld* World, uint32 Identity, bool bParty)
	{
		FKnightCoordinationActor Fixture;
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
		Fixture.ASC = NewObject<UKnightAbilitySystemComponent>(Fixture.Actor.Get());
		Fixture.Actor->AddInstanceComponent(Fixture.ASC.Get());
		Fixture.ASC->RegisterComponent();
		Fixture.Attributes = NewObject<UKnightAttributeSet>(Fixture.Actor.Get());
		Fixture.Life = NewObject<UKnightLifeComponent>(Fixture.Actor.Get());
		Fixture.Actor->AddInstanceComponent(Fixture.Life.Get());
		Fixture.Life->RegisterComponent();
		Fixture.Initialization = Fixture.ASC->InitializeKnightActor(Fixture.Actor.Get(), Fixture.Attributes.Get(), Fixture.Life.Get(),
			FKnightEntityId(FGuid(0, 0, 0, Identity)), FKnightAttributeDefaults(), bParty);
		if (Fixture.Initialization.IsSuccess() && !bParty)
		{
			Fixture.ASC->SetLooseGameplayTagCount(KnightGameplayTags::Role_Enemy, 1,
				EGameplayTagReplicationState::TagAndCountToAll);
		}
		return Fixture;
	}

	/** TimerManager runs once per engine frame. Each latent update advances this owned world once. */
	class FKnightWaitForCoordinationTimeCommand final : public IAutomationLatentCommand
	{
	public:
		FKnightWaitForCoordinationTimeCommand(FAutomationTestBase* InTest,
			const TSharedRef<FKnightCoordinationWorld>& InContext, float Seconds, TFunction<void()> InCompletion)
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
		TSharedRef<FKnightCoordinationWorld> Context;
		TFunction<void()> Completion;
		double TargetWorldTime;
		double StartedAt;
	};

	void WaitForWorldTime(FAutomationTestBase* Test, const TSharedRef<FKnightCoordinationWorld>& Context,
		float Seconds, TFunction<void()> Completion)
	{
		FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FKnightWaitForCoordinationTimeCommand>(
			Test, Context, Seconds, MoveTemp(Completion)));
	}

	struct FKnightThreatTestState
	{
		FKnightThreatTestState()
			: Context(MakeShared<FKnightCoordinationWorld>()), Enemy(SpawnActor(Context->World.Get(), 101, false)),
			First(SpawnActor(Context->World.Get(), 1, true)), Second(SpawnActor(Context->World.Get(), 2, true)),
			Third(SpawnActor(Context->World.Get(), 3, true))
		{
		}
		~FKnightThreatTestState()
		{
			if (Threat.IsValid())
			{
				Threat->OnThreatChanged.Remove(ChangeHandle);
			}
		}
		TSharedRef<FKnightCoordinationWorld> Context;
		FKnightCoordinationActor Enemy;
		FKnightCoordinationActor First;
		FKnightCoordinationActor Second;
		FKnightCoordinationActor Third;
		TWeakObjectPtr<UKnightThreatComponent> Threat;
		FDelegateHandle ChangeHandle;
		int32 InitialSubscriptions = 0;
		int32 Changes = 0;
	};

	struct FKnightLeaseTestState
	{
		FKnightLeaseTestState()
			: Context(MakeShared<FKnightCoordinationWorld>()), First(SpawnActor(Context->World.Get(), 101, false)),
			Second(SpawnActor(Context->World.Get(), 102, false)), Third(SpawnActor(Context->World.Get(), 103, false)),
			TargetOne(SpawnActor(Context->World.Get(), 1, true)), TargetTwo(SpawnActor(Context->World.Get(), 2, true))
		{
			Director = Context->World->GetSubsystem<UKnightCombatDirectorSubsystem>();
		}
		~FKnightLeaseTestState()
		{
			if (Director.IsValid())
			{
				Director->OnAttackLeaseReleased.Remove(ReleaseHandle);
			}
		}
		TSharedRef<FKnightCoordinationWorld> Context;
		FKnightCoordinationActor First;
		FKnightCoordinationActor Second;
		FKnightCoordinationActor Third;
		FKnightCoordinationActor TargetOne;
		FKnightCoordinationActor TargetTwo;
		TWeakObjectPtr<UKnightCombatDirectorSubsystem> Director;
		FKnightAttackLease ExpiringLease;
		FDelegateHandle ReleaseHandle;
		int32 ExpiryNotifications = 0;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKnightThreatConfirmationTest, "Knight.AI.Coordination.ConfirmedThreat",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FKnightThreatConfirmationTest::RunTest(const FString& Parameters)
{
	using namespace KnightAICoordinationTests;
	const TSharedRef<FKnightThreatTestState> State = MakeShared<FKnightThreatTestState>();
	const FKnightCoordinationActor& Enemy = State->Enemy;
	const FKnightCoordinationActor& First = State->First;
	const FKnightCoordinationActor& Second = State->Second;
	const FKnightCoordinationActor& Third = State->Third;
	if (!TestTrue(TEXT("All threat fixtures completed real foundation initialization"),
		Enemy.IsReady() && First.IsReady() && Second.IsReady() && Third.IsReady()))
	{
		return false;
	}
	UKnightEventSubsystem* Bus = State->Context->World->GetSubsystem<UKnightEventSubsystem>();
	State->InitialSubscriptions = Bus->GetSubscriptionCount();
	UKnightThreatComponent* Threat = NewObject<UKnightThreatComponent>(Enemy.Actor.Get());
	State->Threat = Threat;
	Enemy.Actor->AddInstanceComponent(Threat);
	Threat->RegisterComponent();
	TestTrue(TEXT("Deferred foundation composition initializes threat"), Threat->InitializeThreat().IsSuccess());
	TestTrue(TEXT("Threat initialization is idempotent"), Threat->InitializeThreat().IsSuccess());
	TestEqual(TEXT("Exactly one owned confirmed-damage subscription"), Bus->GetSubscriptionCount(), State->InitialSubscriptions + 1);
	TestTrue(TEXT("Finite native threat tuning is accepted"), Threat->TrySetThreatTuning(10.0f, 2.0f, 100.0f).IsSuccess());
	const TWeakPtr<FKnightThreatTestState> WeakState(State);
	State->ChangeHandle = Threat->OnThreatChanged.AddLambda([WeakState]()
		{
			if (const TSharedPtr<FKnightThreatTestState> Pinned = WeakState.Pin())
			{
				++Pinned->Changes;
			}
		});
	TestTrue(TEXT("Threat trigger is actual GAS damage execution"),
		UKnightAbilitySystemComponent::ApplyDamage(First.Actor.Get(), Enemy.Actor.Get(), 20.0f).IsSuccess());
	TestEqual(TEXT("Real GE reduced owner health"), Enemy.Attributes->GetHealth(), 80.0f);
	TestEqual(TEXT("Confirmed damage creates the scaled threat on stable source identity"),
		Threat->GetThreat(First.ASC->GetEntityId()), 40.0f);
	TestTrue(TEXT("Perception can explicitly add awareness"), Threat->ObserveTarget(Second.Actor.Get(), 40.0f).IsSuccess());
	TestTrue(TEXT("Equal threat uses deterministic stable-ID order"), Threat->GetHighestThreatTarget() == First.Actor.Get());
	Second.ASC->SetLooseGameplayTagCount(KnightGameplayTags::Role_Party_Knight, 0,
		EGameplayTagReplicationState::TagAndCountToAll);
	Second.ASC->SetLooseGameplayTagCount(KnightGameplayTags::Role_Enemy, 1,
		EGameplayTagReplicationState::TagAndCountToAll);
	TestEqual(TEXT("A newly friendly target is excluded without a mutating query"), Threat->GetThreatCount(), 1);
	TestEqual(TEXT("Friendly query preserves its stored entry until lifecycle maintenance"), Threat->GetTrackedEntryCount(), 2);
	Second.ASC->SetLooseGameplayTagCount(KnightGameplayTags::Role_Enemy, 0,
		EGameplayTagReplicationState::TagAndCountToAll);
	Second.ASC->SetLooseGameplayTagCount(KnightGameplayTags::Role_Party_Knight, 1,
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
		Threat->AddThreat(First.Actor.Get(), std::numeric_limits<float>::quiet_NaN()).Error, EKnightError::InvalidData);
	TestEqual(TEXT("Rejected threat is atomic"), Threat->GetThreat(First.ASC->GetEntityId()), 40.0f);
	TestEqual(TEXT("Rejected threat emits no change"), State->Changes, BeforeQueries);
	TestTrue(TEXT("Entries start bounded maintenance"), Threat->HasMaintenanceTimer());
	FAutomationTestBase* Test = this;
	WaitForWorldTime(Test, State->Context, 1.0f, [State, Test]()
		{
			UKnightThreatComponent* CurrentThreat = State->Threat.Get();
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
			Test->TestTrue(TEXT("Actual incoming GE downs a party target"), UKnightAbilitySystemComponent::ApplyDamage(
				State->Enemy.Actor.Get(), State->First.Actor.Get(), 200.0f).IsSuccess());
			Test->TestTrue(TEXT("Party target entered actual Downed life state"), State->First.Life->IsDowned());
			Test->TestEqual(TEXT("Target life delegate purges stored threat immediately"), CurrentThreat->GetTrackedEntryCount(), 0);
			Test->TestFalse(TEXT("No target means no maintenance timer"), CurrentThreat->HasMaintenanceTimer());
			Test->TestTrue(TEXT("New awareness can be reacquired"), CurrentThreat->ObserveTarget(State->Third.Actor.Get()).IsSuccess());
			Test->TestTrue(TEXT("Actual damage kills the enemy owner"), UKnightAbilitySystemComponent::ApplyDamage(
				State->Third.Actor.Get(), State->Enemy.Actor.Get(), 200.0f).IsSuccess());
			Test->TestTrue(TEXT("Threat owner is actually Dead"), State->Enemy.Life->IsDead());
			Test->TestEqual(TEXT("Owner death clears all stored threat"), CurrentThreat->GetTrackedEntryCount(), 0);
			Test->TestFalse(TEXT("Owner death stops maintenance"), CurrentThreat->HasMaintenanceTimer());
			Test->TestTrue(TEXT("Real foundation checkpoint restores the owner"), State->Enemy.Life->ResetForCheckpoint().IsSuccess());
			Test->TestTrue(TEXT("Explicit awareness still works after foundation restore"),
				CurrentThreat->ObserveTarget(State->Third.Actor.Get(), 1.0f).IsSuccess());
			WaitForWorldTime(Test, State->Context, 0.4f, [State, Test]()
				{
					UKnightThreatComponent* ExpiredThreat = State->Threat.Get();
					if (!Test->TestNotNull(TEXT("Decay fixture retains its native component"), ExpiredThreat))
					{
						return;
					}
					Test->TestEqual(TEXT("Decay to zero purges the stored entry"), ExpiredThreat->GetTrackedEntryCount(), 0);
					Test->TestFalse(TEXT("Expired awareness leaves no timer"), ExpiredThreat->HasMaintenanceTimer());
					ExpiredThreat->OnThreatChanged.Remove(State->ChangeHandle);
					ExpiredThreat->DestroyComponent();
					const UKnightEventSubsystem* CurrentBus = State->Context->World->GetSubsystem<UKnightEventSubsystem>();
					Test->TestEqual(TEXT("Component teardown removes only its own bus subscription"),
						CurrentBus->GetSubscriptionCount(), State->InitialSubscriptions);
				});
		});
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKnightAttackLeaseBudgetTest, "Knight.AI.Coordination.AttackBudgets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FKnightAttackLeaseBudgetTest::RunTest(const FString& Parameters)
{
	using namespace KnightAICoordinationTests;
	FKnightCoordinationWorld Context;
	const FKnightCoordinationActor First = SpawnActor(Context.World.Get(), 101, false);
	const FKnightCoordinationActor Second = SpawnActor(Context.World.Get(), 102, false);
	const FKnightCoordinationActor Third = SpawnActor(Context.World.Get(), 103, false);
	const FKnightCoordinationActor TargetOne = SpawnActor(Context.World.Get(), 1, true);
	const FKnightCoordinationActor TargetTwo = SpawnActor(Context.World.Get(), 2, true);
	const FKnightCoordinationActor TargetThree = SpawnActor(Context.World.Get(), 3, true);
	if (!TestTrue(TEXT("All budget fixtures completed real foundation initialization"), First.IsReady() &&
		Second.IsReady() && Third.IsReady() && TargetOne.IsReady() && TargetTwo.IsReady() && TargetThree.IsReady()))
	{
		return false;
	}
	UKnightCombatDirectorSubsystem* Director = Context.World->GetSubsystem<UKnightCombatDirectorSubsystem>();
	if (!TestNotNull(TEXT("Real server world owns the attack director"), Director))
	{
		return false;
	}
	TestEqual(TEXT("Default global budget"), Director->GetGlobalBudget(), 2);
	TestEqual(TEXT("Default target budget"), Director->GetPerTargetBudget(), 1);
	TestFalse(TEXT("Idle director has no timer"), Director->HasMaintenanceTimer());
	TestEqual(TEXT("Invalid budgets are rejected"), Director->TrySetBudgets(0, 1).Error, EKnightError::InvalidData);
	FKnightAttackLease One;
	FKnightAttackLease Two;
	FKnightAttackLease Rejected;
	TestTrue(TEXT("First contender acquires an actual lease"),
		Director->TryAcquireAttackToken(First.Actor.Get(), TargetOne.Actor.Get(), 5.0f, One).IsSuccess());
	TestEqual(TEXT("A second contender cannot attack an occupied target"),
		Director->TryAcquireAttackToken(Second.Actor.Get(), TargetOne.Actor.Get(), 5.0f, Rejected).Error,
		EKnightError::CapacityExceeded);
	TestFalse(TEXT("Failed acquire returns no usable capability"), Rejected.IsValid());
	TestEqual(TEXT("A source cannot hold two leases"),
		Director->TryAcquireAttackToken(First.Actor.Get(), TargetTwo.Actor.Get(), 5.0f, Rejected).Error, EKnightError::Conflict);
	TestTrue(TEXT("Different target fits the remaining global budget"),
		Director->TryAcquireAttackToken(Second.Actor.Get(), TargetTwo.Actor.Get(), 5.0f, Two).IsSuccess());
	TestEqual(TEXT("Third contender cannot exceed global budget"),
		Director->TryAcquireAttackToken(Third.Actor.Get(), TargetThree.Actor.Get(), 5.0f, Rejected).Error,
		EKnightError::CapacityExceeded);
	TestEqual(TEXT("Budget tuning cannot displace active leases"), Director->TrySetBudgets(3, 2).Error, EKnightError::Conflict);
	TestEqual(TEXT("Failed tuning keeps the budget"), Director->GetGlobalBudget(), 2);
	TestEqual(TEXT("Both accepted leases remain active"), Director->GetActiveLeaseCount(), 2);
	TestEqual(TEXT("One accepted lease belongs to target one"), Director->GetActiveLeaseCountForTarget(TargetOne.ASC->GetEntityId()), 1);
	TestTrue(TEXT("Active leases start maintenance"), Director->HasMaintenanceTimer());
	FKnightAttackLease Forged = One;
	Forged.TargetId = TargetTwo.ASC->GetEntityId();
	TestEqual(TEXT("Forged capability fields cannot release a valid GUID"),
		Director->ReleaseAttackToken(First.Actor.Get(), Forged).Error, EKnightError::NotOwned);
	TestEqual(TEXT("Foreign source cannot release another source's lease"),
		Director->ReleaseAttackToken(Second.Actor.Get(), One).Error, EKnightError::NotOwned);
	int32 Releases = 0;
	const FDelegateHandle ReleaseHandle = Director->OnAttackLeaseReleased.AddLambda(
		[&Releases](const FKnightAttackLease& Lease, EKnightAttackLeaseReleaseReason Reason)
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
	FKnightAttackLease Replacement;
	TestTrue(TEXT("Released source can acquire a new capability"),
		Director->TryAcquireAttackToken(First.Actor.Get(), TargetThree.Actor.Get(), 5.0f, Replacement).IsSuccess());
	TestTrue(TEXT("New request owns a fresh GUID"), Replacement.LeaseId != One.LeaseId);
	TestEqual(TEXT("Old capability cannot release a replacement lease"),
		Director->ReleaseAttackToken(First.Actor.Get(), One).Error, EKnightError::Conflict);
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
	TestEqual(TEXT("Nonfinite expiry request is rejected"), Director->TryAcquireAttackToken(Third.Actor.Get(),
		TargetThree.Actor.Get(), std::numeric_limits<float>::quiet_NaN(), Rejected).Error, EKnightError::InvalidData);
	FKnightCoordinationWorld ForeignContext;
	const FKnightCoordinationActor Foreign = SpawnActor(ForeignContext.World.Get(), 999, true);
	if (!TestTrue(TEXT("Cross-world target completed real foundation initialization"), Foreign.IsReady()))
	{
		return false;
	}
	TestEqual(TEXT("A lease cannot cross real server worlds"),
		Director->TryAcquireAttackToken(Third.Actor.Get(), Foreign.Actor.Get(), 1.0f, Rejected).Error, EKnightError::InvalidData);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKnightAttackLeaseLifecycleTest, "Knight.AI.Coordination.AttackLeaseLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FKnightAttackLeaseLifecycleTest::RunTest(const FString& Parameters)
{
	using namespace KnightAICoordinationTests;
	const TSharedRef<FKnightLeaseTestState> State = MakeShared<FKnightLeaseTestState>();
	const FKnightCoordinationActor& First = State->First;
	const FKnightCoordinationActor& Second = State->Second;
	const FKnightCoordinationActor& Third = State->Third;
	const FKnightCoordinationActor& TargetOne = State->TargetOne;
	const FKnightCoordinationActor& TargetTwo = State->TargetTwo;
	if (!TestTrue(TEXT("All lifecycle fixtures completed real foundation initialization"), First.IsReady() &&
		Second.IsReady() && Third.IsReady() && TargetOne.IsReady() && TargetTwo.IsReady()))
	{
		return false;
	}
	UKnightCombatDirectorSubsystem* Director = State->Director.Get();
	if (!TestNotNull(TEXT("Lifecycle uses real world director"), Director))
	{
		return false;
	}
	TestTrue(TEXT("Empty director accepts bounded multiple-target budget"), Director->TrySetBudgets(3, 2).IsSuccess());
	FKnightAttackLease One;
	FKnightAttackLease Two;
	FKnightAttackLease Three;
	TestTrue(TEXT("First source reserves shared target"),
		Director->TryAcquireAttackToken(First.Actor.Get(), TargetOne.Actor.Get(), 5.0f, One).IsSuccess());
	TestTrue(TEXT("Second source reserves the same target within tuned budget"),
		Director->TryAcquireAttackToken(Second.Actor.Get(), TargetOne.Actor.Get(), 5.0f, Two).IsSuccess());
	TestTrue(TEXT("Third source reserves independent target"),
		Director->TryAcquireAttackToken(Third.Actor.Get(), TargetTwo.Actor.Get(), 5.0f, Three).IsSuccess());
	TestTrue(TEXT("Actual GE downs the shared target"),
		UKnightAbilitySystemComponent::ApplyDamage(Third.Actor.Get(), TargetOne.Actor.Get(), 200.0f).IsSuccess());
	TestTrue(TEXT("Shared target is actually downed"), TargetOne.Life->IsDowned());
	TestFalse(TEXT("First shared lease is reclaimed synchronously"), Director->IsLeaseActive(One));
	TestFalse(TEXT("Second shared lease is reclaimed synchronously"), Director->IsLeaseActive(Two));
	TestTrue(TEXT("Independent target lease survives shared-target loss"), Director->IsLeaseActive(Three));
	TestEqual(TEXT("Exactly one lease remains after target loss"), Director->GetActiveLeaseCount(), 1);
	TestTrue(TEXT("Actual GE kills the remaining enemy source"),
		UKnightAbilitySystemComponent::ApplyDamage(TargetTwo.Actor.Get(), Third.Actor.Get(), 200.0f).IsSuccess());
	TestTrue(TEXT("Source is actually dead"), Third.Life->IsDead());
	TestEqual(TEXT("Source death releases the remaining lease immediately"), Director->GetActiveLeaseCount(), 0);
	TestFalse(TEXT("Death leaves no maintenance timer"), Director->HasMaintenanceTimer());
	FKnightAttackLease Rejected;
	TestEqual(TEXT("Dead source cannot reacquire"), Director->TryAcquireAttackToken(Third.Actor.Get(),
		TargetTwo.Actor.Get(), 1.0f, Rejected).Error, EKnightError::InvalidState);
	TestTrue(TEXT("Another actual GE transitions downed target to Dead"),
		UKnightAbilitySystemComponent::ApplyDamage(First.Actor.Get(), TargetOne.Actor.Get(), 20.0f).IsSuccess());
	TestTrue(TEXT("Shared target is actually dead"), TargetOne.Life->IsDead());
	TestEqual(TEXT("Dead target cannot receive an attack lease"), Director->TryAcquireAttackToken(First.Actor.Get(),
		TargetOne.Actor.Get(), 1.0f, Rejected).Error, EKnightError::InvalidState);
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
	const TWeakPtr<FKnightLeaseTestState> WeakState(State);
	State->ReleaseHandle = Director->OnAttackLeaseReleased.AddLambda(
		[WeakState](const FKnightAttackLease& Lease, EKnightAttackLeaseReleaseReason Reason)
		{
			if (const TSharedPtr<FKnightLeaseTestState> Pinned = WeakState.Pin();
				Pinned.IsValid() && Reason == EKnightAttackLeaseReleaseReason::Expired && Lease == Pinned->ExpiringLease)
			{
				++Pinned->ExpiryNotifications;
			}
		});
	TestTrue(TEXT("Short bounded lease is acquired"),
		Director->TryAcquireAttackToken(Second.Actor.Get(), TargetTwo.Actor.Get(), 0.1f, State->ExpiringLease).IsSuccess());
	FAutomationTestBase* Test = this;
	WaitForWorldTime(Test, State->Context, 0.25f, [State, Test]()
		{
			UKnightCombatDirectorSubsystem* CurrentDirector = State->Director.Get();
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
			FKnightAttackLease TeardownLease;
			Test->TestTrue(TEXT("World teardown starts with a real outstanding lease"), CurrentDirector->TryAcquireAttackToken(
				State->Second.Actor.Get(), State->TargetTwo.Actor.Get(), 10.0f, TeardownLease).IsSuccess());
			State->Context->StopPlay();
			Test->TestEqual(TEXT("World EndPlay clears leases before deinitialization"), CurrentDirector->GetActiveLeaseCount(), 0);
			Test->TestFalse(TEXT("World EndPlay clears its timer"), CurrentDirector->HasMaintenanceTimer());
			FKnightAttackLease RejectedLease;
			Test->TestEqual(TEXT("Ended world rejects new commands"), CurrentDirector->TryAcquireAttackToken(
				State->Second.Actor.Get(), State->TargetTwo.Actor.Get(), 1.0f, RejectedLease).Error, EKnightError::ShuttingDown);
		});
	return true;
}

#endif
