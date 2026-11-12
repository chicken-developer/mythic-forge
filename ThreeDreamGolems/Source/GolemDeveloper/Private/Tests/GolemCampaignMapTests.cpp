#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Character/GolemCharacter.h"
#include "Definitions/GolemHeroDefinition.h"
#include "Definitions/GolemStoryWorldDefinition.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "Input/GolemInputConfig.h"
#include "InputMappingContext.h"
#include "HAL/PlatformTime.h"
#include "Interaction/GolemStoryAnchor.h"
#include "Runtime/GolemStoryWorldComponent.h"
#include "Settings/GolemGameUserSettings.h"

namespace GolemCampaignTests
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
			State = NewObject<UGolemStoryWorldComponent>(Owner);
			Owner->AddInstanceComponent(State);
			State->RegisterComponent();
			State->InitializeWorld(TEXT("MapTest"), FGuid::NewGuid());
			Definition = NewObject<UGolemStoryWorldDefinition>();
			Definition->TrySetIdentity(TEXT("MapTest"), TEXT("GolemStoryWorld"));
			FGolemMapRegion& Region = Definition->MapRegions.AddDefaulted_GetRef();
			Region.RegionId = TEXT("School");
			Region.Label = FText::FromString(TEXT("Test region"));
			Region.Map = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/Golem/Maps/Playables/L_M04_PartyArena")));
			FGolemMapMarker& Marker = Definition->MapMarkers.AddDefaulted_GetRef();
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
		UGolemStoryWorldComponent* State = nullptr;
		UGolemStoryWorldDefinition* Definition = nullptr;
	};

	class FPingTimers final : public IAutomationLatentCommand
	{
	public:
		explicit FPingTimers(FAutomationTestBase& InTest) : Test(InTest), Started(FPlatformTime::Seconds())
		{
			Fixture.State->RevealNearbyMarkers(TArray<FVector>{FVector::ZeroVector});
			for (int32 Index = 0; Index < 3; ++Index)
			{
				AGolemCharacter* Golem = Fixture.World->SpawnActor<AGolemCharacter>();
				UGolemHeroDefinition* Hero = NewObject<UGolemHeroDefinition>();
				Hero->TrySetIdentity(FName(*FString::Printf(TEXT("PingHero%d"), Index)), TEXT("GolemHero"));
				Golem->InitializeGolem(FGolemEntityId::Create(), Hero);
				Fixture.World->SpawnActor<APlayerController>()->Possess(Golem);
				Golems.Add(Golem);
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
					const FGolemResult Result = Fixture.State->RequestMapPing(Golems[Index],
						Golems[Index]->GetGolemEntityId(), TEXT("Landmark"));
					Test.TestEqual(TEXT("Three owners fill the shared eight-ping capacity"), Result.IsSuccess(), Round != 2 || Index != 2);
				}
				++Round;
			}
			if (Round == 3 && !bCheckedOwner && Now >= 3.5f)
			{
				Test.TestEqual(TEXT("Global ping cap is eight"), Fixture.State->GetMapPings().Num(), 8);
				Test.TestFalse(TEXT("Owner cannot exceed three active pings"), Fixture.State->RequestMapPing(
					Golems[0], Golems[0]->GetGolemEntityId(), TEXT("Landmark")).IsSuccess());
				bCheckedOwner = true;
			}
			if (Now > 10.8f)
			{
				Test.TestEqual(TEXT("World timer expires every ping without another request or restore"), Fixture.State->GetMapPings().Num(), 0);
				Test.TestTrue(TEXT("Expired capacity and rate recover"), Fixture.State->RequestMapPing(
					Golems[0], Golems[0]->GetGolemEntityId(), TEXT("Landmark")).IsSuccess());
				return true;
			}
			return false;
		}
	private:
		FAutomationTestBase& Test;
		double Started;
		FWorld Fixture;
		TArray<AGolemCharacter*> Golems;
		int32 Round = 0;
		bool bCheckedOwner = false;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGolemCampaignPingTimerTest, "Golem.M7.PingTimerAndCapacity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGolemCampaignPingTimerTest::RunTest(const FString& Parameters)
{
	FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<GolemCampaignTests::FPingTimers>(*this));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGolemCampaignProjectionTest, "Golem.M7.RegionProjectionAndValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGolemCampaignProjectionTest::RunTest(const FString& Parameters)
{
	GolemCampaignTests::FWorld Fixture;
	FGolemMapRegion& Region = Fixture.Definition->MapRegions[0];
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
	const FGolemMapRegion Duplicate = Region;
	Fixture.Definition->MapRegions.Add(Duplicate);
	TestFalse(TEXT("Duplicate region rejected"), Fixture.Definition->ValidateWorld().IsSuccess());
	Fixture.Definition->MapRegions.Pop();
	Fixture.Definition->MapMarkers[0].TravelRegionId = TEXT("Missing");
	TestFalse(TEXT("Unregistered travel destination rejected"), Fixture.Definition->ValidateWorld().IsSuccess());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGolemCampaignDiscoveryTest, "Golem.M7.DiscoveryAndTransientPings",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGolemCampaignDiscoveryTest::RunTest(const FString& Parameters)
{
	GolemCampaignTests::FWorld Fixture;
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
	const FGolemStoryWorldSnapshot Saved = Fixture.State->GetSnapshot();
	TestTrue(TEXT("Durable world fragment validates"), Fixture.State->RestoreSnapshot(Saved).IsSuccess());
	TestTrue(TEXT("Fog survives reconstruction"), Fixture.State->IsMarkerDiscovered(TEXT("Landmark")));
	TestFalse(TEXT("Unknown region not unlocked"), Fixture.State->IsRegionDiscovered(TEXT("Missing")));

	AGolemCharacter* Golem = Fixture.World->SpawnActor<AGolemCharacter>();
	UGolemHeroDefinition* Hero = NewObject<UGolemHeroDefinition>();
	Hero->TrySetIdentity(TEXT("MapHero"), TEXT("GolemHero"));
	const FGolemEntityId GolemId = FGolemEntityId::Create();
	TestTrue(TEXT("Real character identity initializes"), Golem->InitializeGolem(GolemId, Hero).IsSuccess());
	APlayerController* Controller = Fixture.World->SpawnActor<APlayerController>();
	Controller->Possess(Golem);
	TestFalse(TEXT("Spoofed Golem cannot ping"), Fixture.State->RequestMapPing(Golem,
		FGolemEntityId::Create(), TEXT("Landmark")).IsSuccess());
	TestTrue(TEXT("Owned discovered marker ping accepted"), Fixture.State->RequestMapPing(Golem, GolemId, TEXT("Landmark")).IsSuccess());
	TestFalse(TEXT("Immediate ping flood rejected"), Fixture.State->RequestMapPing(Golem, GolemId, TEXT("Landmark")).IsSuccess());
	TestEqual(TEXT("Pings do not mutate durable fact revision"), Fixture.State->GetSnapshot().FactRevision, Revision);
	TestEqual(TEXT("Exactly one transient ping"), Fixture.State->GetMapPings().Num(), 1);
	TestTrue(TEXT("Checkpoint reconstruction clears transient pings"), Fixture.State->RestoreSnapshot(Saved).IsSuccess());
	TestEqual(TEXT("No ping persisted"), Fixture.State->GetMapPings().Num(), 0);

	AGolemStoryAnchor* Anchor = Fixture.World->SpawnActor<AGolemStoryAnchor>();
	Anchor->AnchorId = TEXT("Target");
	Fixture.State->BindAnchor(Anchor);
	FGolemStoryInteraction& Interaction = Fixture.Definition->Interactions.AddDefaulted_GetRef();
	Interaction.AnchorId = TEXT("Target");
	Interaction.SubjectId = TEXT("Target");
	Interaction.Label = FText::FromString(TEXT("Ping target"));
	Fixture.Definition->MapMarkers[0].AnchorId = TEXT("Target");
	TestTrue(TEXT("Actor ping uses a validated definition"), Fixture.Definition->ValidateWorld().IsSuccess());
	TestTrue(TEXT("Actor target ping accepted"), Fixture.State->RequestMapPing(Golem, GolemId, TEXT("Landmark")).IsSuccess());
	Anchor->Destroy();
	Fixture.State->RequestMapPing(nullptr, GolemId, NAME_None);
	TestEqual(TEXT("Destroyed ping target removed"), Fixture.State->GetMapPings().Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGolemCampaignInputTest, "Golem.M7.LocalPreferencesAndSemanticRemap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGolemCampaignInputTest::RunTest(const FString& Parameters)
{
	UGolemGameUserSettings* Settings = NewObject<UGolemGameUserSettings>();
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
	UGolemInputConfig* Input = NewObject<UGolemInputConfig>();
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
