#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Character/KnightCharacter.h"
#include "Checkpoint/KnightCheckpointSubsystem.h"
#include "Components/KnightAbilitySystemComponent.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/PlatformTime.h"
#include "HAL/FileManager.h"
#include "Interaction/KnightStoryAnchor.h"
#include "Inventory/KnightInventoryComponent.h"
#include "InputCoreTypes.h"
#include "InputKeyEventArgs.h"
#include "Tags/KnightGameplayTags.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/Parse.h"
#include "Misc/CommandLine.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "Party/KnightPartyState.h"
#include "PlayInEditorDataTypes.h"
#include "Progression/KnightRPGComponent.h"
#include "RPGLoop/KnightRPGStateActor.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Story/KnightStoryGameMode.h"
#include "Story/KnightStoryPlayerController.h"
#include "Story/KnightStoryState.h"
#include "UObject/StrongObjectPtr.h"
#include "UnrealEngine.h"
#include "UnrealClient.h"

namespace KnightStoryIntegration
{
	class FPlaythrough final : public IAutomationLatentCommand
	{
	public:
		FPlaythrough(FAutomationTestBase& InTest, bool bInNetwork)
			: Test(InTest), bNetwork(bInNetwork), Started(FPlatformTime::Seconds())
		{
		}
		~FPlaythrough()
		{
			if (Player.IsValid())
			{
				Key(EKeys::W, false);
				Key(EKeys::F, false);
			}
			if (GEditor->IsPlaySessionInProgress())
			{
				GEditor->EndPlayMap();
			}
		}
		virtual bool Update() override
		{
			if (FPlatformTime::Seconds() - Started > 180.0 || Test.HasAnyErrors())
			{
				if (!Test.HasAnyErrors())
				{
					Test.AddError(FString::Printf(TEXT("Story playthrough timeout phase%d step%d"), Phase, Step));
				}
				return true;
			}
			if (Phase == 0)
			{
				Settings.Reset(DuplicateObject<ULevelEditorPlaySettings>(GetDefault<ULevelEditorPlaySettings>(), GetTransientPackage()));
				Settings->SetPlayNetMode(bNetwork ? PIE_ListenServer : PIE_Standalone);
				Settings->SetPlayNumberOfClients(bNetwork ? 3 : 1);
				Settings->SetRunUnderOneProcess(true);
				Settings->SetServerPort(17966);
				Settings->EnableGameSound = false;
				Settings->GameGetsMouseControl = false;
				Settings->SetClientWindowSize(FIntPoint(1280, 720));
				FRequestPlaySessionParams Request;
				Request.EditorPlaySettings = Settings.Get();
				Request.GlobalMapOverride = TEXT("/Game/Knight/Maps/Playables/L_M06_StoryV1");
				Request.bAllowOnlineSubsystem = false;
				GEditor->RequestPlaySession(Request);
				Phase = 1;
				return false;
			}
			if (Phase == 1)
			{
				for (const FWorldContext& Context : GEngine->GetWorldContexts())
				{
					UWorld* Candidate = Context.World();
					if (Context.WorldType == EWorldType::PIE && Candidate != nullptr && Candidate->GetNetMode() != NM_Client)
					{
						World = Candidate;
						Mode = Candidate->GetAuthGameMode<AKnightStoryGameMode>();
						Player = Cast<AKnightStoryPlayerController>(Candidate->GetFirstPlayerController());
					}
				}
				if (!Mode.IsValid() || !Player.IsValid() || Player->GetControlledKnight() == nullptr
					|| !Mode->GetStoryInitializationResult().IsSuccess())
				{
					return false;
				}
				if (Mode->GetPartyState()->GetHumanCount() != (bNetwork ? 3 : 1))
				{
					return false;
				}
				Test.TestEqual(TEXT("Three canonical Knights"), Mode->GetPartyState()->GetRoster().Num(), 3);
				Test.TestEqual(TEXT("No CatalogLab production seed"), Mode->GetRPGState()->GetInventory()->GetView().Items.Num(), 0);
				InitialId = Player->GetControlledKnight()->GetKnightEntityId();
				Phase = 2;
			}
			if (Phase == 2)
			{
				const FName Steps[]{TEXT("School_Boro"), TEXT("School_Boro"), TEXT("School_Boro"),
					TEXT("Model_Sky"), TEXT("Model_Sea"), TEXT("Model_Earth"),
					TEXT("Forge_Head"), TEXT("Forge_Handle"), TEXT("Forge_Heat"), TEXT("Forge_Hammer"),
					TEXT("Forge_Cool"), TEXT("Forge_Finish"), TEXT("School_Claim"), TEXT("School_Checkpoint")};
				if (Step == UE_ARRAY_COUNT(Steps))
				{
					Phase = 3;
					return false;
				}
				AKnightStoryState* State = Mode->GetStoryState();
				if (bPulse)
				{
					if (World->GetTimeSeconds() - PulseAt < 0.15)
					{
						return false;
					}
					Key(EKeys::F, false);
					if (State->WorldState->GetSnapshot().Revision == BeforeWorld
						&& State->Dialogue->GetSnapshot().Revision == BeforeDialogue && Step != 13)
					{
						Test.AddError(FString::Printf(TEXT("Mapped F did not commit step%d at %s"), Step, *Steps[Step].ToString()));
						return false;
					}
					++Step;
					bPulse = false;
					return false;
				}
				AKnightStoryAnchor* Anchor = State->WorldState->FindAnchor(Steps[Step]);
				if (Anchor == nullptr)
				{
					Test.AddError(TEXT("Missing genuine authored anchor."));
					return false;
				}
				const FVector Goal = Anchor->GetActorLocation() + FVector(0.0, -175.0, 30.0);
				if (!Walk(Goal))
				{
					return false;
				}
				Key(EKeys::W, false);
				if (FParse::Param(FCommandLine::Get(), TEXT("KnightStoryCapture")) && (Step == 1 || Step == 12))
				{
					FScreenshotRequest::RequestScreenshot(FPaths::ProjectDir()
						/ FString::Printf(TEXT("Evidence/M6/Rendered/Step%d.png"), Step), true, false);
				}
				BeforeWorld = State->WorldState->GetSnapshot().Revision;
				if (bNetwork && Step == 1)
				{
					int32 GuestViews = 0;
					for (const FWorldContext& Context : GEngine->GetWorldContexts())
					{
						UWorld* Client = Context.World();
						if (Context.WorldType == EWorldType::PIE && Client != nullptr && Client->GetNetMode() == NM_Client)
						{
							AKnightStoryState* ClientState = Client->GetGameState<AKnightStoryState>();
							if (ClientState == nullptr || !ClientState->Dialogue->GetSnapshot().bActive
								|| ClientState->Dialogue->GetSnapshot().NodeId != State->Dialogue->GetSnapshot().NodeId)
							{
								return false;
							}
							++GuestViews;
						}
					}
					Test.TestEqual(TEXT("Both guest views converge on the actual Boro cursor"), GuestViews, 2);
				}
				if (Step == 12)
				{
					// The original V1_pending file was captured too late. Retain its immutable bytes as evidence.
					const FString PendingPath = FPaths::ProjectDir() / TEXT("Evidence/M6/Fixtures/V1_reward_pending.knight");
					FKnightCampaignCheckpoint Pending;
					if (!IFileManager::Get().FileExists(*PendingPath))
					{
						Test.TestTrue(TEXT("Capture genuine pending reward"), Mode->CaptureCheckpoint(Pending).IsSuccess());
						Test.TestTrue(TEXT("Immutable pending-reward fixture"),
							UKnightCheckpointSubsystem::WriteCheckpointFile(PendingPath, Pending).IsSuccess());
					}
					Test.TestTrue(TEXT("Read immutable pending fixture before claim"),
						UKnightCheckpointSubsystem::ReadCheckpointFile(PendingPath, Pending).IsSuccess());
					const FKnightQuestProgress* PendingQuest = Pending.Quest.Quests.FindByPredicate(
						[](const FKnightQuestProgress& Progress)
						{
							return Progress.QuestId == TEXT("ActI_Q2");
						});
					Test.TestTrue(TEXT("Stored V1 Q2 really is RewardPending"),
						PendingQuest != nullptr && PendingQuest->Phase == EKnightQuestPhase::RewardPending);
					Test.TestEqual(TEXT("Pending input uses content V1"), Pending.Quest.ContentRevision, 1);
					Test.TestEqual(TEXT("Pending input has no claimed receipt"), Pending.RPG.View.ClaimedRewards.Num(), 0);
					Test.TestEqual(TEXT("Pending input has no minted hammer"), Pending.RPG.Inventory.Items.Num(), 0);
				}
				BeforeDialogue = State->Dialogue->GetSnapshot().Revision;
				PulseAt = World->GetTimeSeconds();
				Key(EKeys::F, true);
				bPulse = true;
				return false;
			}
			if (Phase == 3)
			{
				AKnightStoryState* State = Mode->GetStoryState();
				Test.TestEqual(TEXT("Q1 actually completed"), State->Quest->FindProgress(TEXT("ActI_Q1"))->Phase, EKnightQuestPhase::Complete);
				Test.TestEqual(TEXT("Q2 actually completed"), State->Quest->FindProgress(TEXT("ActI_Q2"))->Phase, EKnightQuestPhase::Complete);
				const FKnightInventoryView Inventory = Mode->GetRPGState()->GetInventory()->GetView();
				Test.TestEqual(TEXT("Exactly three common hammer outputs"), Inventory.Items.Num(), 3);
				Test.TestEqual(TEXT("One committed quest reward"), Mode->GetRPGState()->GetRPG()->GetView().ClaimedRewards.Num(), 1);
				FKnightCampaignCheckpoint Snapshot;
				Test.TestTrue(TEXT("Consistent real RPG/world/quest capture"), Mode->CaptureCheckpoint(Snapshot).IsSuccess());
				const FString FixturePath = FPaths::ProjectDir() / TEXT("Evidence/M6/Fixtures/V1_completed.knight");
				if (!IFileManager::Get().FileExists(*FixturePath))
				{
					Test.TestTrue(TEXT("Immutable V1 fixture first write"),
						UKnightCheckpointSubsystem::WriteCheckpointFile(FixturePath, Snapshot).IsSuccess());
				}
				FKnightCampaignCheckpoint Loaded;
				Test.TestTrue(TEXT("Durable real campaign read"), World->GetGameInstance()->GetSubsystem<UKnightCheckpointSubsystem>()
					->LoadCheckpoint(TEXT("Demo_M6"), TEXT("StoryDemo"), Loaded).IsSuccess());
				Test.TestEqual(TEXT("Cold bytes preserve run"), Loaded.Quest.RunId, Snapshot.Quest.RunId);
				Test.TestEqual(TEXT("Cold bytes preserve inventory GUID"), Loaded.RPG.Inventory.Items[0].InstanceId, Snapshot.RPG.Inventory.Items[0].InstanceId);
				Test.TestTrue(TEXT("Real movement used"), WalkDistance > 1500.0);
				Test.TestEqual(TEXT("Canonical identity retained"), Player->GetControlledKnight()->GetKnightEntityId(), InitialId);
				if (!bNetwork)
				{
					const TArray<FKnightRosterEntry> Roster = Mode->GetPartyState()->GetRoster();
					for (int32 Index = 0; Index < Roster.Num(); ++Index)
					{
						FKnightRPGCommand Equip;
						Equip.Operation = EKnightRPGOperation::Equip;
						Equip.RequestId = FGuid::NewGuid();
						Equip.ExpectedKnightId = Roster[Index].KnightId;
						Equip.InventoryId = Mode->GetRPGState()->GetInventory()->GetView().InventoryId;
						Equip.ExpectedInventoryRevision = Mode->GetRPGState()->GetInventory()->GetView().Revision;
						Equip.ExpectedKnightRevision = Mode->GetRPGState()->GetRPG()->GetKnightView(Roster[Index].KnightId).Revision;
						Equip.ItemInstanceId = Inventory.Items[Index].InstanceId;
						Equip.ExpectedItemRevision = Inventory.Items[Index].ItemRevision;
						Equip.SlotTag = KnightGameplayTags::Slot_Equipment_Weapon;
						Test.TestTrue(TEXT("Actual tutorial hammer equips on canonical Knight through existing coordinator"),
							Mode->ExecuteRPGCommand(Player.Get(), Equip).Result.IsSuccess());
					}
				}
				if (bNetwork)
				{
					for (const FWorldContext& Context : GEngine->GetWorldContexts())
					{
						UWorld* Client = Context.World();
						AKnightStoryState* ClientState = Client != nullptr ? Client->GetGameState<AKnightStoryState>() : nullptr;
						if (Client != nullptr && Client->GetNetMode() == NM_Client)
						{
							if (ClientState == nullptr || ClientState->Quest->FindProgress(TEXT("ActI_Q2")) == nullptr
								|| ClientState->Quest->FindProgress(TEXT("ActI_Q2"))->Phase != EKnightQuestPhase::Complete)
							{
								return false;
							}
						}
					}
				}
				Phase = 4;
				if (FParse::Param(FCommandLine::Get(), TEXT("KnightStoryCapture")))
				{
					Player->ToggleRPGPanel();
					FScreenshotRequest::RequestScreenshot(FPaths::ProjectDir()
						/ TEXT("Evidence/M6/Rendered/CompletedPanel.png"), true, false);
					CaptureUntil = FPlatformTime::Seconds() + 1.0;
					Phase = 5;
					return false;
				}
				GEditor->EndPlayMap();
				return false;
			}
			if (Phase == 5)
			{
				if (FPlatformTime::Seconds() < CaptureUntil)
				{
					return false;
				}
				GEditor->EndPlayMap();
				Phase = 4;
			}
			return !GEditor->IsPlaySessionInProgress();
		}

	private:
		void Key(const FKey& InKey, const bool bPressed)
		{
			Player->InputKey(FInputKeyEventArgs::CreateSimulated(InKey,
				bPressed ? IE_Pressed : IE_Released, bPressed ? 1.0f : 0.0f));
		}
		bool Walk(const FVector& Goal)
		{
			AKnightCharacter* Knight = Player->GetControlledKnight();
			if (!PreviousPosition.IsZero())
			{
				WalkDistance += FVector::Dist2D(PreviousPosition, Knight->GetActorLocation());
			}
			PreviousPosition = Knight->GetActorLocation();
			if (FVector::DistSquared2D(Knight->GetActorLocation(), Goal) < FMath::Square(45.0f))
			{
				return true;
			}
			if (World->GetTimeSeconds() >= NextPathAt || Path.IsEmpty())
			{
				FScopedConditionalWorldSwitcher Switch(World.Get());
				UNavigationPath* Navigation = UNavigationSystemV1::FindPathToLocationSynchronously(World.Get(),
					Knight->GetActorLocation(), Goal, Knight);
				Path = Navigation != nullptr && Navigation->IsValid() && !Navigation->IsPartial()
					? Navigation->PathPoints : TArray<FVector>();
				Waypoint = Path.Num() > 1 ? 1 : 0;
				NextPathAt = World->GetTimeSeconds() + 0.4;
			}
			if (Path.IsEmpty())
			{
				return false;
			}
			while (Waypoint < Path.Num() - 1
				&& FVector::DistSquared2D(Knight->GetActorLocation(), Path[Waypoint]) < FMath::Square(60.0f))
			{
				++Waypoint;
			}
			Player->SetControlRotation((Path[Waypoint] - Knight->GetActorLocation()).GetSafeNormal2D().Rotation());
			Key(EKeys::W, true);
			return false;
		}
		FAutomationTestBase& Test;
		bool bNetwork = false;
		double Started = 0.0;
		int32 Phase = 0;
		int32 Step = 0;
		TStrongObjectPtr<ULevelEditorPlaySettings> Settings;
		TWeakObjectPtr<UWorld> World;
		TWeakObjectPtr<AKnightStoryGameMode> Mode;
		TWeakObjectPtr<AKnightStoryPlayerController> Player;
		FKnightEntityId InitialId;
		bool bPulse = false;
		double PulseAt = 0.0;
		double NextPathAt = 0.0;
		int64 BeforeWorld = 0;
		int64 BeforeDialogue = 0;
		TArray<FVector> Path;
		int32 Waypoint = 0;
		FVector PreviousPosition = FVector::ZeroVector;
		double WalkDistance = 0.0;
		double CaptureUntil = 0.0;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKnightStoryPlaythroughTest, "Knight.M6.MappedSoloPlaythrough",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FKnightStoryPlaythroughTest::RunTest(const FString& Parameters)
{
	ADD_LATENT_AUTOMATION_COMMAND(KnightStoryIntegration::FPlaythrough(*this, false));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKnightStoryNetworkTest, "Knight.M6.MappedHostAndTwoClients",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FKnightStoryNetworkTest::RunTest(const FString& Parameters)
{
	ADD_LATENT_AUTOMATION_COMMAND(KnightStoryIntegration::FPlaythrough(*this, true));
	return true;
}

#endif
