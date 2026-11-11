#include "Assets/GolemAssetManager.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Assets/GolemDefinition.h"
#include "Events/GolemAsyncRequestScope.h"
#include "Events/GolemEventSubsystem.h"
#include "Settings/GolemSettings.h"
#include "Tags/GolemGameplayTags.h"
#include "Types/GolemStableIds.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "UObject/UObjectGlobals.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGolemIdentityTest, "Golem.Core.IdentityAndErrors",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGolemIdentityTest::RunTest(const FString& Parameters)
{
	UGolemDefinition* Definition = NewObject<UGolemDefinition>();
	TestFalse(TEXT("Unassigned assets never fall back to package names"), Definition->GetPrimaryAssetId().IsValid());
	TestEqual(TEXT("Missing identity reports a typed failure"), Definition->ValidateIdentity().Error,
		EGolemError::InvalidId);
	TestTrue(TEXT("Stable identity authoring succeeds"), Definition->TrySetIdentity(TEXT("StableFixture")));
	const FPrimaryAssetId Expected(TEXT("GolemDefinition"), TEXT("StableFixture"));
	TestEqual(TEXT("Authored primary ID"), Definition->GetPrimaryAssetId(), Expected);
	Definition->Rename(TEXT("RenamedFixtureObject"));
	TestEqual(TEXT("Asset rename preserves persisted identity"), Definition->GetPrimaryAssetId(), Expected);

	FGolemResult Rejection;
	TestFalse(TEXT("Invalid characters are rejected"),
		Definition->TrySetIdentity(TEXT("Invalid Fixture"), TEXT("GolemDefinition"), &Rejection));
	TestEqual(TEXT("Invalid identity exposes a typed reason"), Rejection.Error, EGolemError::InvalidId);
	TestEqual(TEXT("Rejected change leaves identity intact"), Definition->GetPrimaryAssetId(), Expected);
	TestFalse(TEXT("Empty stable ID"), GolemIdentity::IsValidIdentifier(NAME_None));
	TestFalse(TEXT("Serialized delimiter cannot enter an ID"), GolemIdentity::IsValidIdentifier(TEXT("Bad:Id")));
	TestTrue(TEXT("Typed definition wrapper validates"), FGolemDefinitionId(Expected).IsValid());
	TestTrue(TEXT("Entity IDs are persistent valid GUIDs"), FGolemEntityId::Create().IsValid());
	TestFalse(TEXT("Unassigned entity ID is invalid"), FGolemEntityId().IsValid());
	TestTrue(TEXT("Canonical party cardinality remains three"), UGolemSettings::CanonicalPartySize == 3);
	TestTrue(TEXT("Success result is explicit"), FGolemResult::Success().IsSuccess());
	TestFalse(TEXT("Failure cannot accidentally use success code"),
		FGolemResult::Failure(EGolemError::None, TEXT("Test"), FText::GetEmpty()).IsSuccess());

	UGolemDefinition* Duplicate = NewObject<UGolemDefinition>();
	Duplicate->TrySetIdentity(TEXT("StableFixture"));
	UGolemDefinition* Missing = NewObject<UGolemDefinition>();
	TArray<UGolemDefinition*> Batch { Definition, Duplicate, Missing };
	const TArray<FGolemResult> Errors = UGolemAssetManager::ValidateDefinitionIdentities(Batch);
	TestEqual(TEXT("Batch reports duplicate plus missing ID"), Errors.Num(), 2);
	if (Errors.Num() == 2)
	{
		TestEqual(TEXT("Duplicate definitions cannot resolve arbitrarily"), Errors[0].Error, EGolemError::DuplicateId);
		TestEqual(TEXT("Missing definitions remain invalid"), Errors[1].Error, EGolemError::InvalidId);
	}
	TestTrue(TEXT("Native semantic action tag is registered"), GolemGameplayTags::Input_Combat_Primary.GetTag().IsValid());
	TestTrue(TEXT("State tag hierarchy is registered"), GolemGameplayTags::State_Life_Downed.GetTag().IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGolemAsyncLifetimeTest, "Golem.Core.AsyncLifetime",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGolemAsyncLifetimeTest::RunTest(const FString& Parameters)
{
	UObject* Owner = NewObject<UGolemDefinition>();
	FGolemAsyncRequestScope Requests;
	const FGolemCancellationToken First = Requests.BeginRequest(Owner);
	TestTrue(TEXT("First request may apply a result"), First.IsCurrent());
	const FGolemCancellationToken Second = Requests.BeginRequest(Owner);
	TestFalse(TEXT("New owner request rejects stale callbacks"), First.IsCurrent());
	TestTrue(TEXT("Current callback still applies"), Second.IsCurrent());
	Requests.Cancel();
	TestFalse(TEXT("Travel/teardown cancellation rejects current callback"), Second.IsCurrent());
	const FGolemCancellationToken OwnerLifetime = Requests.BeginRequest(Owner);
	Owner->MarkAsGarbage();
	TestFalse(TEXT("Garbage owner cannot receive async result"), OwnerLifetime.IsCurrent());
	TestFalse(TEXT("Null owner cannot receive async result"), Requests.BeginRequest(nullptr).IsCurrent());

	FGolemCancellationToken Detached;
	UObject* OtherOwner = NewObject<UGolemDefinition>();
	{
		FGolemAsyncRequestScope Temporary;
		Detached = Temporary.BeginRequest(OtherOwner);
		TestTrue(TEXT("Live scope accepts current callback"), Detached.IsCurrent());
	}
	TestFalse(TEXT("Scope destruction cancels outstanding callback"), Detached.IsCurrent());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGolemEventLifecycleTest, "Golem.Core.ConfirmedEventsAndLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGolemEventLifecycleTest::RunTest(const FString& Parameters)
{
	for (int32 Iteration = 0; Iteration < 2; ++Iteration)
	{
		const UWorld::InitializationValues Values = UWorld::InitializationValues()
			.AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true,
			ERHIFeatureLevel::Num, &Values);
		UGolemEventSubsystem* Bus = World->GetSubsystem<UGolemEventSubsystem>();
		if (!TestNotNull(TEXT("World lifecycle creates the confirmed event subsystem"), Bus))
		{
			World->DestroyWorld(false);
			return false;
		}
		TestTrue(TEXT("Bus is initialized before domain subscription"), Bus->IsReady());
		UObject* Owner = NewObject<UGolemDefinition>(World);
		UObject* IndependentOwner = NewObject<UGolemDefinition>(World);
		int32 Deliveries = 0;
		int32 IndependentDeliveries = 0;
		const FGolemEventSubscription Subscription = Bus->Subscribe<FGolemFoundationEvent>(Owner,
			[&Deliveries](const FGolemFoundationEvent& Event)
			{
				++Deliveries;
			});
		const FGolemEventSubscription Independent = Bus->Subscribe<FGolemFoundationEvent>(IndependentOwner,
			[&IndependentDeliveries](const FGolemFoundationEvent& Event)
			{
				++IndependentDeliveries;
			});
		TestTrue(TEXT("Registration returns a live handle"), Subscription.IsValid());
		FGolemFoundationEvent Event;
		Event.ConfirmationId = FGuid::NewGuid();
		Event.Revision = 1;
		Event.Stage = GolemGameplayTags::Event_Foundation_Started;
		TestTrue(TEXT("Committed authoritative event publishes"), Bus->PublishConfirmed(Event).IsSuccess());
		TestEqual(TEXT("Subscriber observes the real committed fact"), Deliveries, 1);
		TestEqual(TEXT("Repeated confirmation is rejected"), Bus->PublishConfirmed(Event).Error,
			EGolemError::DuplicateConfirmation);
		TestEqual(TEXT("Duplicate transaction does not notify twice"), Deliveries, 1);
		Event.ConfirmationId = FGuid();
		TestEqual(TEXT("Unconfirmed events cannot publish"), Bus->PublishConfirmed(Event).Error, EGolemError::InvalidId);
		Event.ConfirmationId = FGuid::NewGuid();
		Event.Revision = 0;
		TestEqual(TEXT("Uncommitted revision cannot publish"), Bus->PublishConfirmed(Event).Error,
			EGolemError::InvalidRevision);
		Bus->Unsubscribe(Subscription);
		TestFalse(TEXT("Removed handle cannot appear live because another listener remains"), Subscription.IsValid());
		Event.Revision = 2;
		TestTrue(TEXT("Remaining listener still receives committed facts"), Bus->PublishConfirmed(Event).IsSuccess());
		TestEqual(TEXT("Only the requesting owner's handle was removed"), IndependentDeliveries, 2);
		TestEqual(TEXT("Removed listener receives no later events"), Deliveries, 1);
		IndependentOwner->MarkAsGarbage();
		TestFalse(TEXT("Destroyed owner invalidates subscription"), Independent.IsValid());
		TestEqual(TEXT("Expired weak owners never count as live subscriptions"), Bus->GetSubscriptionCount(), 0);
		{
			FGolemScopedEventSubscription Scoped(Bus->Subscribe<FGolemFoundationEvent>(Owner,
				[](const FGolemFoundationEvent& Confirmed)
				{
				}));
			TestEqual(TEXT("Scoped subscription is active"), Bus->GetSubscriptionCount(), 1);
		}
		TestEqual(TEXT("Scope destruction removes only its registration"), Bus->GetSubscriptionCount(), 0);
		World->DestroyWorld(false);
		TestFalse(TEXT("World shutdown clears the bus"), Bus->IsReady());
		TestEqual(TEXT("Shutdown rejects late callbacks"), Bus->PublishConfirmed(Event).Error, EGolemError::ShuttingDown);
	}
	return true;
}

#if WITH_EDITOR
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGolemEventAuthorityTest, "Golem.Core.ClientEventAuthority",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGolemEventAuthorityTest::RunTest(const FString& Parameters)
{
	const UWorld::InitializationValues Values = UWorld::InitializationValues()
		.AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::PIE, false, NAME_None, nullptr, true,
		ERHIFeatureLevel::Num, &Values);
	World->SetPlayInEditorInitialNetMode(NM_Client);
	UGolemEventSubsystem* Bus = World->GetSubsystem<UGolemEventSubsystem>();
	TestEqual(TEXT("Fixture is a real engine client-mode world"), World->GetNetMode(), NM_Client);
	FGolemFoundationEvent Event;
	Event.ConfirmationId = FGuid::NewGuid();
	Event.Revision = 1;
	TestEqual(TEXT("Client world cannot publish authoritative confirmations"),
		Bus->PublishConfirmed(Event).Error, EGolemError::NotAuthority);
	World->DestroyWorld(false);
	return true;
}
#endif

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGolemAssetIntegrationTest, "Golem.Core.PrimaryAssetIntegration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGolemAssetIntegrationTest::RunTest(const FString& Parameters)
{
	UGolemAssetManager* Manager = UGolemAssetManager::GetGolemAssetManager();
	if (!TestNotNull(TEXT("Configured Golem Asset Manager is active"), Manager))
	{
		return false;
	}
	TestTrue(TEXT("Real authored catalog passes ID validation"), Manager->ValidateDefinitionCatalog().IsSuccess());
	const FPrimaryAssetId FixtureId = GetDefault<UGolemSettings>()->FoundationDefinition;
	TestTrue(TEXT("Fixture is registered in Unreal Primary Asset directory"),
		Manager->GetPrimaryAssetPath(FixtureId).IsValid());
	UGolemDefinition* Fixture = nullptr;
	const FGolemResult Loaded = Manager->ResolveDefinition(FixtureId, Fixture);
	TestTrue(TEXT("Real fixture resolves by stable Primary Asset ID"), Loaded.IsSuccess());
	if (TestNotNull(TEXT("Resolve returns the real loaded definition"), Fixture))
	{
		TestEqual(TEXT("Loaded definition matches saved identity"), Fixture->GetPrimaryAssetId(), FixtureId);
	}
	const FGolemResult Missing = Manager->ResolveDefinition(
		FPrimaryAssetId(TEXT("GolemDefinition"), TEXT("Missing")), Fixture);
	TestEqual(TEXT("Nonexistent ID reports NotFound"), Missing.Error, EGolemError::NotFound);
	TestNull(TEXT("Failure never leaks a prior loaded pointer"), Fixture);
	TestEqual(TEXT("Invalid ID reports InvalidId"), Manager->ResolveDefinition(FPrimaryAssetId(), Fixture).Error,
		EGolemError::InvalidId);
	TestEqual(TEXT("Invalid expected class reports TypeMismatch"),
		Manager->ResolveDefinition(FixtureId, UObject::StaticClass(), Fixture).Error, EGolemError::TypeMismatch);
	return true;
}

#endif
