#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Character/KnightCharacter.h"
#include "Definitions/KnightHeroDefinition.h"
#include "Definitions/KnightStoryWorldDefinition.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "Input/KnightInputConfig.h"
#include "InputMappingContext.h"
#include "HAL/PlatformTime.h"
#include "Interaction/KnightStoryAnchor.h"
#include "Runtime/KnightStoryWorldComponent.h"
#include "Settings/KnightGameUserSettings.h"

namespace KnightCampaignTests
{
	class FWorld final
	{
	public:
		FWorld()
		{
			const UWorld::InitializationValues Values = UWorld::InitializationValues()
				.AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true,
				ERHIFeatureLevel::Num, &Values);
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			AActor* Owner = World->SpawnActor<AActor>();
			State = NewObject<UKnightStoryWorldComponent>(Owner);
			Owner->AddInstanceComponent(State);
			State->RegisterComponent();
			State->InitializeWorld(TEXT("MapTest"), FGuid::NewGuid());
			Definition = NewObject<UKnightStoryWorldDefinition>();
			Definition->TrySetIdentity(TEXT("MapTest"), TEXT("KnightStoryWorld"));
			FKnightMapRegion& Region = Definition->MapRegions.AddDefaulted_GetRef();
			Region.RegionId = TEXT("School");
			Region.Label = FText::FromString(TEXT("Test region"));
			Region.Map = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/Knight/Maps/Playables/L_M07_Campaign")));
			FKnightMapMarker& Marker = Definition->MapMarkers.AddDefaulted_GetRef();
			Marker.MarkerId = TEXT("Landmark");
			Marker.RegionId = Region.RegionId;
			Marker.Label = FText::FromString(TEXT("Landmark"));
			Marker.DiscoveryRadius = 150.0f;
			State->ConfigureMap(Definition, Region.RegionId);
		}
		~FWorld()
		{
			World->DestroyWorld(false);
			GEngine->DestroyWorldContext(World);
		}
		UWorld* World = nullptr;
		UKnightStoryWorldComponent* State = nullptr;
		UKnightStoryWorldDefinition* Definition = nullptr;
	};

	class FPingTimers final : public IAutomationLatentCommand
	{
	public:
		explicit FPingTimers(FAutomationTestBase& InTest) : Test(InTest), Started(FPlatformTime::Seconds())
		{
			Fixture.State->RevealNearbyMarkers(TArray<FVector>{FVector::ZeroVector});
			for (int32 Index = 0; Index < 3; ++Index)
			{
				AKnightCharacter* Knight = Fixture.World->SpawnActor<AKnightCharacter>();
				UKnightHeroDefinition* Hero = NewObject<UKnightHeroDefinition>();
				Hero->TrySetIdentity(FName(*FString::Printf(TEXT("PingHero%d"), Index)), TEXT("KnightHero"));
				Knight->InitializeKnight(FKnightEntityId::Create(), Hero);
				Fixture.World->SpawnActor<APlayerController>()->Possess(Knight);
				Knights.Add(Knight);
			}
		}
		virtual bool Update() override
		{
			if (FPlatformTime::Seconds() - Started > 15.0)
			{
				Test.AddError(TEXT("Ping timer world failed to advance through expiry"));
				return true;
			}
			if (Test.HasAnyErrors())
			{
				return true;
			}
			Fixture.World->Tick(LEVELTICK_All, 0.1f);
			const float Now = Fixture.World->GetTimeSeconds();
			if (Round < 3 && Now >= Round * 1.1f)
			{
				for (int32 Index = 0; Index < 3; ++Index)
				{
					const FKnightResult Result = Fixture.State->RequestMapPing(Knights[Index],
						Knights[Index]->GetKnightEntityId(), TEXT("Landmark"));
					Test.TestEqual(TEXT("Three owners fill the shared eight-ping capacity"), Result.IsSuccess(), Round != 2 || Index != 2);
				}
				++Round;
			}
			if (Round == 3 && !bCheckedOwner && Now >= 3.5f)
			{
				Test.TestEqual(TEXT("Global ping cap is eight"), Fixture.State->GetMapPings().Num(), 8);
				Test.TestFalse(TEXT("Owner cannot exceed three active pings"), Fixture.State->RequestMapPing(
					Knights[0], Knights[0]->GetKnightEntityId(), TEXT("Landmark")).IsSuccess());
				bCheckedOwner = true;
			}
			if (Now > 10.8f)
			{
				Test.TestEqual(TEXT("World timer expires every ping without another request or restore"), Fixture.State->GetMapPings().Num(), 0);
				Test.TestTrue(TEXT("Expired capacity and rate recover"), Fixture.State->RequestMapPing(
					Knights[0], Knights[0]->GetKnightEntityId(), TEXT("Landmark")).IsSuccess());
				return true;
			}
			return false;
		}
	private:
		FAutomationTestBase& Test;
		double Started;
		FWorld Fixture;
		TArray<AKnightCharacter*> Knights;
		int32 Round = 0;
		bool bCheckedOwner = false;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKnightCampaignPingTimerTest, "Knight.M7.PingTimerAndCapacity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FKnightCampaignPingTimerTest::RunTest(const FString& Parameters)
{
	FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<KnightCampaignTests::FPingTimers>(*this));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKnightCampaignProjectionTest, "Knight.M7.RegionProjectionAndValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FKnightCampaignProjectionTest::RunTest(const FString& Parameters)
{
	KnightCampaignTests::FWorld Fixture;
	FKnightMapRegion& Region = Fixture.Definition->MapRegions[0];
	Region.WorldCenter = FVector2D(24000.0, -9000.0);
	Region.HalfExtent = FVector2D(2300.0, 1700.0);
	Region.NorthYaw = 90.0f;
	const FVector Position(24600.0, -9700.0, 900.0);
	FVector2D UV;
	TestTrue(TEXT("Rotated landmark projects"), Region.Project(Position, UV));
	TestTrue(TEXT("Independent rotated landmark calibration"), UV.Equals(FVector2D(0.3478260869, 0.6764705882), 0.00001));
	FVector2D Restored;
	TestTrue(TEXT("Projection inverse is defined"), Region.Unproject(UV, Restored));
	TestTrue(TEXT("Calibrated landmark round trip"), Restored.Equals(FVector2D(Position.X, Position.Y), 0.001));
	const FIntVector StreamedOrigin(20000, -20000, 0);
	const FVector Relative = Position - FVector(StreamedOrigin);
	FVector2D StreamedUV;
	TestTrue(TEXT("Streaming/origin offset uses absolute region coordinates"), Region.Project(Relative + FVector(StreamedOrigin), StreamedUV));
	TestTrue(TEXT("Landmark stays aligned across origin shift"), StreamedUV.Equals(UV, 0.001));
	TestFalse(TEXT("Out-of-region positions are not clamped into a false location"), Region.Project(FVector::ZeroVector, UV));
	TestFalse(TEXT("Invalid map click rejected"), Region.Unproject(FVector2D(-0.1, 0.5), Restored));
	Fixture.Definition->MapMarkers[0].WorldPosition = Position;
	TestTrue(TEXT("Complete calibrated world validates"), Fixture.Definition->ValidateWorld().IsSuccess());
	const FKnightMapRegion Duplicate = Region;
	Fixture.Definition->MapRegions.Add(Duplicate);
	TestFalse(TEXT("Duplicate region rejected"), Fixture.Definition->ValidateWorld().IsSuccess());
	Fixture.Definition->MapRegions.Pop();
	Fixture.Definition->MapMarkers[0].TravelRegionId = TEXT("Missing");
	TestFalse(TEXT("Unregistered travel destination rejected"), Fixture.Definition->ValidateWorld().IsSuccess());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKnightCampaignDiscoveryTest, "Knight.M7.DiscoveryAndTransientPings",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FKnightCampaignDiscoveryTest::RunTest(const FString& Parameters)
{
	KnightCampaignTests::FWorld Fixture;
	TArray<FVector> Positions{FVector(1000.0, 0.0, 0.0)};
	Fixture.State->RevealNearbyMarkers(Positions);
	TestFalse(TEXT("Distant marker remains under fog"), Fixture.State->IsMarkerDiscovered(TEXT("Landmark")));
	Positions[0] = FVector::ZeroVector;
	Fixture.State->RevealNearbyMarkers(Positions);
	TestTrue(TEXT("Confirmed proximity discovers marker"), Fixture.State->IsMarkerDiscovered(TEXT("Landmark")));
	TestTrue(TEXT("Region discovery derives from persistent world facts"), Fixture.State->IsRegionDiscovered(TEXT("School")));
	const int64 Revision = Fixture.State->GetSnapshot().FactRevision;
	Fixture.State->RevealNearbyMarkers(Positions);
	TestEqual(TEXT("Discovery is idempotent"), Fixture.State->GetSnapshot().FactRevision, Revision);
	const FKnightStoryWorldSnapshot Saved = Fixture.State->GetSnapshot();
	TestTrue(TEXT("Durable world fragment validates"), Fixture.State->RestoreSnapshot(Saved).IsSuccess());
	TestTrue(TEXT("Fog survives reconstruction"), Fixture.State->IsMarkerDiscovered(TEXT("Landmark")));
	TestFalse(TEXT("Unknown region not unlocked"), Fixture.State->IsRegionDiscovered(TEXT("Missing")));

	AKnightCharacter* Knight = Fixture.World->SpawnActor<AKnightCharacter>();
	UKnightHeroDefinition* Hero = NewObject<UKnightHeroDefinition>();
	Hero->TrySetIdentity(TEXT("MapHero"), TEXT("KnightHero"));
	const FKnightEntityId KnightId = FKnightEntityId::Create();
	TestTrue(TEXT("Real character identity initializes"), Knight->InitializeKnight(KnightId, Hero).IsSuccess());
	APlayerController* Controller = Fixture.World->SpawnActor<APlayerController>();
	Controller->Possess(Knight);
	TestFalse(TEXT("Spoofed Knight cannot ping"), Fixture.State->RequestMapPing(Knight,
		FKnightEntityId::Create(), TEXT("Landmark")).IsSuccess());
	TestTrue(TEXT("Owned discovered marker ping accepted"), Fixture.State->RequestMapPing(Knight, KnightId, TEXT("Landmark")).IsSuccess());
	TestFalse(TEXT("Immediate ping flood rejected"), Fixture.State->RequestMapPing(Knight, KnightId, TEXT("Landmark")).IsSuccess());
	TestEqual(TEXT("Pings do not mutate durable fact revision"), Fixture.State->GetSnapshot().FactRevision, Revision);
	TestEqual(TEXT("Exactly one transient ping"), Fixture.State->GetMapPings().Num(), 1);
	TestTrue(TEXT("Checkpoint reconstruction clears transient pings"), Fixture.State->RestoreSnapshot(Saved).IsSuccess());
	TestEqual(TEXT("No ping persisted"), Fixture.State->GetMapPings().Num(), 0);

	AKnightStoryAnchor* Anchor = Fixture.World->SpawnActor<AKnightStoryAnchor>();
	Anchor->AnchorId = TEXT("Target");
	Fixture.State->BindAnchor(Anchor);
	FKnightStoryInteraction& Interaction = Fixture.Definition->Interactions.AddDefaulted_GetRef();
	Interaction.AnchorId = TEXT("Target");
	Interaction.SubjectId = TEXT("Target");
	Interaction.Label = FText::FromString(TEXT("Ping target"));
	Fixture.Definition->MapMarkers[0].AnchorId = TEXT("Target");
	TestTrue(TEXT("Actor ping uses a validated definition"), Fixture.Definition->ValidateWorld().IsSuccess());
	TestTrue(TEXT("Actor target ping accepted"), Fixture.State->RequestMapPing(Knight, KnightId, TEXT("Landmark")).IsSuccess());
	Anchor->Destroy();
	Fixture.State->RequestMapPing(nullptr, KnightId, NAME_None);
	TestEqual(TEXT("Destroyed ping target removed"), Fixture.State->GetMapPings().Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKnightCampaignInputTest, "Knight.M7.LocalPreferencesAndSemanticRemap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FKnightCampaignInputTest::RunTest(const FString& Parameters)
{
	UKnightGameUserSettings* Settings = NewObject<UKnightGameUserSettings>();
	Settings->EnsureBindings();
	TestTrue(TEXT("Native preferences validate"), Settings->ValidatePreferences().IsSuccess());
	TestEqual(TEXT("Default controls and all four specialization slots"), Settings->KeyBindings.Num(), 25);
	Settings->KeyBindings[1].Key = Settings->KeyBindings[0].Key;
	TestFalse(TEXT("Conflicting keys rejected before application"), Settings->ValidatePreferences().IsSuccess());
	Settings->KeyBindings.Reset();
	Settings->EnsureBindings();
	Settings->TextScale = 1.5f;
	Settings->SubtitleScale = 2.0f;
	TestTrue(TEXT("Largest supported readable settings validate"), Settings->ValidatePreferences().IsSuccess());
	Settings->Sensitivity = -1.0f;
	TestFalse(TEXT("Invalid sensitivity rejected"), Settings->ValidatePreferences().IsSuccess());
	UKnightInputConfig* Input = NewObject<UKnightInputConfig>();
	Input->InitializeDefaultMapping();
	const int32 Original = Input->GetMappingContext()->GetMappings().Num();
	TMap<FKey, FKey> Remapping;
	Remapping.Add(EKeys::W, EKeys::Up);
	TestTrue(TEXT("Remap uses existing semantic action"), Input->ApplyDesktopRemapping(Remapping).IsSuccess());
	TestTrue(TEXT("Forward axis modifiers retained"), Input->GetMappingContext()->GetMappings().ContainsByPredicate(
		[](const FEnhancedActionKeyMapping& Mapping)
		{
			return Mapping.Key == EKeys::Up && !Mapping.Modifiers.IsEmpty();
		}));
	TestTrue(TEXT("Repeated rebuild starts from original bindings"), Input->ApplyDesktopRemapping(Remapping).IsSuccess());
	TestEqual(TEXT("Remapping never duplicates contexts/actions"), Input->GetMappingContext()->GetMappings().Num(), Original);
	return true;
}

#endif
