#include "Assets/KnightAssetManager.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Assets/KnightDefinition.h"
#include "Events/KnightAsyncRequestScope.h"
#include "Events/KnightEventSubsystem.h"
#include "Settings/KnightSettings.h"
#include "Tags/KnightGameplayTags.h"
#include "Types/KnightStableIds.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "UObject/UObjectGlobals.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKnightIdentityTest, "Knight.Core.IdentityAndErrors",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FKnightIdentityTest::RunTest(const FString& Parameters)
{
	UKnightDefinition* Definition = NewObject<UKnightDefinition>();
	TestFalse(TEXT("Unassigned assets never fall back to package names"), Definition->GetPrimaryAssetId().IsValid());
	TestEqual(TEXT("Missing identity reports a typed failure"), Definition->ValidateIdentity().Error,
		EKnightError::InvalidId);
	TestTrue(TEXT("Stable identity authoring succeeds"), Definition->TrySetIdentity(TEXT("StableFixture")));
	const FPrimaryAssetId Expected(TEXT("KnightDefinition"), TEXT("StableFixture"));
	TestEqual(TEXT("Authored primary ID"), Definition->GetPrimaryAssetId(), Expected);
	Definition->Rename(TEXT("RenamedFixtureObject"));
	TestEqual(TEXT("Asset rename preserves persisted identity"), Definition->GetPrimaryAssetId(), Expected);

	FKnightResult Rejection;
	TestFalse(TEXT("Invalid characters are rejected"),
		Definition->TrySetIdentity(TEXT("Invalid Fixture"), TEXT("KnightDefinition"), &Rejection));
	TestEqual(TEXT("Invalid identity exposes a typed reason"), Rejection.Error, EKnightError::InvalidId);
	TestEqual(TEXT("Rejected change leaves identity intact"), Definition->GetPrimaryAssetId(), Expected);
	TestFalse(TEXT("Empty stable ID"), KnightIdentity::IsValidIdentifier(NAME_None));
	TestFalse(TEXT("Serialized delimiter cannot enter an ID"), KnightIdentity::IsValidIdentifier(TEXT("Bad:Id")));
	TestTrue(TEXT("Typed definition wrapper validates"), FKnightDefinitionId(Expected).IsValid());
	TestTrue(TEXT("Entity IDs are persistent valid GUIDs"), FKnightEntityId::Create().IsValid());
	TestFalse(TEXT("Unassigned entity ID is invalid"), FKnightEntityId().IsValid());
	TestTrue(TEXT("Canonical party cardinality remains three"), UKnightSettings::CanonicalPartySize == 3);
	TestTrue(TEXT("Success result is explicit"), FKnightResult::Success().IsSuccess());
	TestFalse(TEXT("Failure cannot accidentally use success code"),
		FKnightResult::Failure(EKnightError::None, TEXT("Test"), FText::GetEmpty()).IsSuccess());

	UKnightDefinition* Duplicate = NewObject<UKnightDefinition>();
	Duplicate->TrySetIdentity(TEXT("StableFixture"));
	UKnightDefinition* Missing = NewObject<UKnightDefinition>();
	TArray<UKnightDefinition*> Batch { Definition, Duplicate, Missing };
	const TArray<FKnightResult> Errors = UKnightAssetManager::ValidateDefinitionIdentities(Batch);
	TestEqual(TEXT("Batch reports duplicate plus missing ID"), Errors.Num(), 2);
	if (Errors.Num() == 2)
	{
		TestEqual(TEXT("Duplicate definitions cannot resolve arbitrarily"), Errors[0].Error, EKnightError::DuplicateId);
		TestEqual(TEXT("Missing definitions remain invalid"), Errors[1].Error, EKnightError::InvalidId);
	}
	TestTrue(TEXT("Native semantic action tag is registered"), KnightGameplayTags::Input_Combat_Primary.GetTag().IsValid());
	TestTrue(TEXT("State tag hierarchy is registered"), KnightGameplayTags::State_Life_Downed.GetTag().IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKnightAsyncLifetimeTest, "Knight.Core.AsyncLifetime",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FKnightAsyncLifetimeTest::RunTest(const FString& Parameters)
{
	UObject* Owner = NewObject<UKnightDefinition>();
	FKnightAsyncRequestScope Requests;
	const FKnightCancellationToken First = Requests.BeginRequest(Owner);
	TestTrue(TEXT("First request may apply a result"), First.IsCurrent());
	const FKnightCancellationToken Second = Requests.BeginRequest(Owner);
	TestFalse(TEXT("New owner request rejects stale callbacks"), First.IsCurrent());
	TestTrue(TEXT("Current callback still applies"), Second.IsCurrent());
	Requests.Cancel();
	TestFalse(TEXT("Travel/teardown cancellation rejects current callback"), Second.IsCurrent());
	const FKnightCancellationToken OwnerLifetime = Requests.BeginRequest(Owner);
	Owner->MarkAsGarbage();
	TestFalse(TEXT("Garbage owner cannot receive async result"), OwnerLifetime.IsCurrent());
	TestFalse(TEXT("Null owner cannot receive async result"), Requests.BeginRequest(nullptr).IsCurrent());

	FKnightCancellationToken Detached;
	UObject* OtherOwner = NewObject<UKnightDefinition>();
	{
		FKnightAsyncRequestScope Temporary;
		Detached = Temporary.BeginRequest(OtherOwner);
		TestTrue(TEXT("Live scope accepts current callback"), Detached.IsCurrent());
	}
	TestFalse(TEXT("Scope destruction cancels outstanding callback"), Detached.IsCurrent());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKnightEventLifecycleTest, "Knight.Core.ConfirmedEventsAndLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FKnightEventLifecycleTest::RunTest(const FString& Parameters)
{
	for (int32 Iteration = 0; Iteration < 2; ++Iteration)
	{
		const UWorld::InitializationValues Values = UWorld::InitializationValues()
			.AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true,
			ERHIFeatureLevel::Num, &Values);
		UKnightEventSubsystem* Bus = World->GetSubsystem<UKnightEventSubsystem>();
		if (!TestNotNull(TEXT("World lifecycle creates the confirmed event subsystem"), Bus))
		{
			World->DestroyWorld(false);
			return false;
		}
		TestTrue(TEXT("Bus is initialized before domain subscription"), Bus->IsReady());
		UObject* Owner = NewObject<UKnightDefinition>(World);
		UObject* IndependentOwner = NewObject<UKnightDefinition>(World);
		int32 Deliveries = 0;
		int32 IndependentDeliveries = 0;
		const FKnightEventSubscription Subscription = Bus->Subscribe<FKnightFoundationEvent>(Owner,
			[&Deliveries](const FKnightFoundationEvent& Event)
			{
				++Deliveries;
			});
		const FKnightEventSubscription Independent = Bus->Subscribe<FKnightFoundationEvent>(IndependentOwner,
			[&IndependentDeliveries](const FKnightFoundationEvent& Event)
			{
				++IndependentDeliveries;
			});
		TestTrue(TEXT("Registration returns a live handle"), Subscription.IsValid());
		FKnightFoundationEvent Event;
		Event.ConfirmationId = FGuid::NewGuid();
		Event.Revision = 1;
		Event.Stage = KnightGameplayTags::Event_Foundation_Started;
		TestTrue(TEXT("Committed authoritative event publishes"), Bus->PublishConfirmed(Event).IsSuccess());
		TestEqual(TEXT("Subscriber observes the real committed fact"), Deliveries, 1);
		TestEqual(TEXT("Repeated confirmation is rejected"), Bus->PublishConfirmed(Event).Error,
			EKnightError::DuplicateConfirmation);
		TestEqual(TEXT("Duplicate transaction does not notify twice"), Deliveries, 1);
		Event.ConfirmationId = FGuid();
		TestEqual(TEXT("Unconfirmed events cannot publish"), Bus->PublishConfirmed(Event).Error, EKnightError::InvalidId);
		Event.ConfirmationId = FGuid::NewGuid();
		Event.Revision = 0;
		TestEqual(TEXT("Uncommitted revision cannot publish"), Bus->PublishConfirmed(Event).Error,
			EKnightError::InvalidRevision);
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
			FKnightScopedEventSubscription Scoped(Bus->Subscribe<FKnightFoundationEvent>(Owner,
				[](const FKnightFoundationEvent& Confirmed)
				{
				}));
			TestEqual(TEXT("Scoped subscription is active"), Bus->GetSubscriptionCount(), 1);
		}
		TestEqual(TEXT("Scope destruction removes only its registration"), Bus->GetSubscriptionCount(), 0);
		World->DestroyWorld(false);
		TestFalse(TEXT("World shutdown clears the bus"), Bus->IsReady());
		TestEqual(TEXT("Shutdown rejects late callbacks"), Bus->PublishConfirmed(Event).Error, EKnightError::ShuttingDown);
	}
	return true;
}

#if WITH_EDITOR
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKnightEventAuthorityTest, "Knight.Core.ClientEventAuthority",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FKnightEventAuthorityTest::RunTest(const FString& Parameters)
{
	const UWorld::InitializationValues Values = UWorld::InitializationValues()
		.AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::PIE, false, NAME_None, nullptr, true,
		ERHIFeatureLevel::Num, &Values);
	World->SetPlayInEditorInitialNetMode(NM_Client);
	UKnightEventSubsystem* Bus = World->GetSubsystem<UKnightEventSubsystem>();
	TestEqual(TEXT("Fixture is a real engine client-mode world"), World->GetNetMode(), NM_Client);
	FKnightFoundationEvent Event;
	Event.ConfirmationId = FGuid::NewGuid();
	Event.Revision = 1;
	TestEqual(TEXT("Client world cannot publish authoritative confirmations"),
		Bus->PublishConfirmed(Event).Error, EKnightError::NotAuthority);
	World->DestroyWorld(false);
	return true;
}
#endif

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKnightAssetIntegrationTest, "Knight.Core.PrimaryAssetIntegration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FKnightAssetIntegrationTest::RunTest(const FString& Parameters)
{
	UKnightAssetManager* Manager = UKnightAssetManager::GetKnightAssetManager();
	if (!TestNotNull(TEXT("Configured Knight Asset Manager is active"), Manager))
	{
		return false;
	}
	TestTrue(TEXT("Real authored catalog passes ID validation"), Manager->ValidateDefinitionCatalog().IsSuccess());
	const FPrimaryAssetId FixtureId = GetDefault<UKnightSettings>()->FoundationDefinition;
	TestTrue(TEXT("Fixture is registered in Unreal Primary Asset directory"),
		Manager->GetPrimaryAssetPath(FixtureId).IsValid());
	UKnightDefinition* Fixture = nullptr;
	const FKnightResult Loaded = Manager->ResolveDefinition(FixtureId, Fixture);
	TestTrue(TEXT("Real fixture resolves by stable Primary Asset ID"), Loaded.IsSuccess());
	if (TestNotNull(TEXT("Resolve returns the real loaded definition"), Fixture))
	{
		TestEqual(TEXT("Loaded definition matches saved identity"), Fixture->GetPrimaryAssetId(), FixtureId);
	}
	const FKnightResult Missing = Manager->ResolveDefinition(
		FPrimaryAssetId(TEXT("KnightDefinition"), TEXT("Missing")), Fixture);
	TestEqual(TEXT("Nonexistent ID reports NotFound"), Missing.Error, EKnightError::NotFound);
	TestNull(TEXT("Failure never leaks a prior loaded pointer"), Fixture);
	TestEqual(TEXT("Invalid ID reports InvalidId"), Manager->ResolveDefinition(FPrimaryAssetId(), Fixture).Error,
		EKnightError::InvalidId);
	TestEqual(TEXT("Invalid expected class reports TypeMismatch"),
		Manager->ResolveDefinition(FixtureId, UObject::StaticClass(), Fixture).Error, EKnightError::TypeMismatch);
	return true;
}

#endif
