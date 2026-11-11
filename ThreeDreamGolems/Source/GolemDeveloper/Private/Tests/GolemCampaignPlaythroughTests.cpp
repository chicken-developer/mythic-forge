#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Blueprint/WidgetTree.h"
#include "Campaign/GolemCampaignPlayerController.h"
#include "Campaign/GolemCampaignGameMode.h"
#include "Character/GolemCharacter.h"
#include "Checkpoint/GolemCheckpointSubsystem.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "InputKeyEventArgs.h"
#include "Interaction/GolemStoryAnchor.h"
#include "Inventory/GolemInventoryComponent.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "Party/GolemPartyState.h"
#include "PlayInEditorDataTypes.h"
#include "RPGLoop/GolemRPGStateActor.h"
#include "Settings/GolemGameUserSettings.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Story/GolemStoryState.h"
#include "UObject/StrongObjectPtr.h"
#include "UnrealClient.h"
#include "UnrealEngine.h"
#include "Widgets/GolemRPGActionButton.h"
#include "Widgets/GolemRPGPanel.h"

namespace GolemCampaignPlaythrough
{
	/** Restore only the exact local files touched by the normal UI playthrough. */
	class FLocalFileGuard final
	{
	public:
		FLocalFileGuard()
		{
			if (UGolemGameUserSettings* Local = UGolemGameUserSettings::Get())
			{
				OriginalPreferences.Reset(DuplicateObject<UGolemGameUserSettings>(Local, GetTransientPackage()));
			}
			const FString Slot = UGolemCheckpointSubsystem::GetCheckpointPath(TEXT("Demo_M6"), TEXT("StoryDemo"));
			for (const FString& Path : {UGolemGameUserSettings::GetPreferenceFilePath(), Slot, Slot + TEXT(".bak"), Slot + TEXT(".tmp")})
			{
				FFile& Entry = Files.AddDefaulted_GetRef();
				Entry.Path = FPaths::ConvertRelativePathToFull(Path);
				Entry.bExisted = FFileHelper::LoadFileToArray(Entry.Bytes, *Entry.Path);
			}
		}
		~FLocalFileGuard()
		{
			GConfig->Flush(false, GGameUserSettingsIni);
			for (const FFile& Entry : Files)
			{
				if (Entry.bExisted)
				{
					FFileHelper::SaveArrayToFile(Entry.Bytes, *Entry.Path);
				}
				else if (Entry.Path.StartsWith(FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir())))
				{
					IFileManager::Get().Delete(*Entry.Path, false, true);
				}
			}
			if (UGolemGameUserSettings* Local = UGolemGameUserSettings::Get())
			{
				Local->LoadSettings(true);
				if (OriginalPreferences.IsValid())
				{
					UEngine::CopyPropertiesForUnrelatedObjects(OriginalPreferences.Get(), Local);
				}
			}
		}
	private:
		struct FFile
		{
			FString Path;
			TArray<uint8> Bytes;
			bool bExisted = false;
		};
		TArray<FFile> Files;
		TStrongObjectPtr<UGolemGameUserSettings> OriginalPreferences;
	};

	class FRun final : public IAutomationLatentCommand
	{
	public:
		FRun(FAutomationTestBase& InTest, const bool bInNetwork)
			: Test(InTest), bNetwork(bInNetwork), Started(FPlatformTime::Seconds())
		{
		}
		~FRun()
		{
			if (Player.IsValid())
			{
				Key(EKeys::W, false);
				Key(EKeys::F, false);
				Player->SetPause(false);
			}
			if (GEditor->IsPlaySessionInProgress())
			{
				GEditor->EndPlayMap();
			}
		}
		virtual bool Update() override
		{
			if (Test.HasAnyErrors() || FPlatformTime::Seconds() - Started > 240.0)
			{
				if (!Test.HasAnyErrors())
				{
					Test.AddError(FString::Printf(TEXT("M7 normal UI timeout phase%d step%d"), Phase, Step));
					if (Mode.IsValid() && Player.IsValid())
					{
						Test.AddInfo(FString::Printf(TEXT("Runtime boundary region=%s selected=%s humans=%d pending=%d result=%s origin=%s"),
							*Mode->GetStoryState()->WorldState->GetActiveRegionId().ToString(),
							*Player->GetCampaignPresentation().RegionId.ToString(), Mode->GetPartyState()->GetHumanCount(),
							Player->GetCampaignPresentation().bPending, *Player->GetLastCampaignResult().Context.ToString(),
							*Mode->GetWorld()->OriginLocation.ToString()));
					}
				}
				return true;
			}
			if (Phase == 0)
			{
				if (UGolemGameUserSettings* Local = UGolemGameUserSettings::Get())
				{
					Local->KeyBindings.Reset();
					Local->EnsureBindings();
					Local->TextScale = 1.0f;
				}
				Settings.Reset(DuplicateObject<ULevelEditorPlaySettings>(GetDefault<ULevelEditorPlaySettings>(), GetTransientPackage()));
				Settings->SetPlayNetMode(bNetwork ? PIE_ListenServer : PIE_Standalone);
				Settings->SetPlayNumberOfClients(bNetwork ? 3 : 1);
				Settings->SetRunUnderOneProcess(true);
				Settings->SetServerPort(17977);
				Settings->EnableGameSound = false;
				Settings->GameGetsMouseControl = false;
				FRequestPlaySessionParams Request;
				Request.EditorPlaySettings = Settings.Get();
				Request.GlobalMapOverride = TEXT("/Game/Knight/Maps/Playables/L_M07_Campaign");
				Request.bAllowOnlineSubsystem = false;
				GEditor->RequestPlaySession(Request);
				Phase = 1;
				return false;
			}
			if (Phase == 8)
			{
				return !GEditor->IsPlaySessionInProgress();
			}
			FindHost();
			if (!Player.IsValid() || !Mode.IsValid() || !Player->GetCampaignPresentation().bEnabled)
			{
				return false;
			}
			if (Phase == 1)
			{
				if (bNetwork && Mode->GetPartyState()->GetHumanCount() != 3)
				{
					return false;
				}
				if (Player->GetRPGPanel() == nullptr || !Player->GetRPGPanel()->IsActivated())
				{
					return false;
				}
				Test.TestTrue(TEXT("Actual boot shows frontend"), Player->GetCampaignPresentation().bFrontEnd);
				Test.TestEqual(TEXT("Solo pause reflects actual simulation"), World->IsPaused(), !bNetwork);
				Click(TEXT("New campaign"));
				Phase = 2;
				return false;
			}
			if (Phase == 2)
			{
				Test.TestEqual(TEXT("New requires explicit overwrite confirmation"),
					Player->GetCampaignPresentation().Confirmation, FName(TEXT("New")));
				Click(TEXT("Confirm"));
				PreviousWorld = World;
				Phase = 3;
				return false;
			}
			if (Phase == 3)
			{
				if (World == PreviousWorld || Player->GetCampaignPresentation().bFrontEnd
					|| Mode->GetPartyState()->GetHumanCount() != (bNetwork ? 3 : 1))
				{
					return false;
				}
				Test.TestEqual(TEXT("New retains exactly three canonical Golems"), Mode->GetPartyState()->GetRoster().Num(), 3);
				Test.TestEqual(TEXT("Campaign does not import fixture inventory"), Mode->GetRPGState()->GetInventory()->GetView().Items.Num(), 0);
				Phase = 4;
			}
			if (Phase == 4)
			{
				const FName Steps[]{TEXT("School_Boro"), TEXT("School_Boro"), TEXT("School_Boro"),
					TEXT("Model_Sky"), TEXT("Model_Sea"), TEXT("Model_Earth"), TEXT("Forge_Head"),
					TEXT("Forge_Handle"), TEXT("Forge_Heat"), TEXT("Forge_Hammer"), TEXT("Forge_Cool"),
					TEXT("Forge_Finish"), TEXT("School_Claim"), TEXT("School_Checkpoint")};
				if (Step == UE_ARRAY_COUNT(Steps))
				{
					Phase = 5;
					return false;
				}
				if (bPulse)
				{
					if (World->GetTimeSeconds() - PulseAt < 0.15)
					{
						return false;
					}
					Key(EKeys::F, false);
					bPulse = false;
					++Step;
					return false;
				}
				AGolemStoryAnchor* Anchor = Mode->GetStoryState()->WorldState->FindAnchor(Steps[Step]);
				if (Anchor == nullptr)
				{
					Test.AddError(TEXT("Missing authored campaign anchor"));
					return false;
				}
				if (!Walk(Anchor->GetActorLocation() + FVector(0.0, -175.0, 30.0)))
				{
					return false;
				}
				Key(EKeys::W, false);
				Key(EKeys::F, true);
				PulseAt = World->GetTimeSeconds();
				bPulse = true;
				return false;
			}
			if (Phase == 5)
			{
				Test.TestEqual(TEXT("Normal interactions complete Q2"), Mode->GetStoryState()->Quest->FindProgress(TEXT("ActI_Q2"))->Phase,
					EGolemQuestPhase::Complete);
				Test.TestEqual(TEXT("Exactly three tutorial outputs"), Mode->GetRPGState()->GetInventory()->GetView().Items.Num(), 3);
				Player->OpenCampaignPage(EGolemRPGPanelTab::Journal);
				Test.TestTrue(TEXT("Real journal has campaign quests"), Player->GetRPGPanel()->GetCampaignPresentation().Quests.Num() >= 3);
				Player->OpenCampaignPage(EGolemRPGPanelTab::Map);
				Test.TestTrue(TEXT("Map has a calibrated player location"), Player->GetCampaignPresentation().bPlayerVisible);
				if (!bNetwork && !bOriginVerified)
				{
					BeforeOrigin = Player->GetCampaignPresentation().PlayerUV;
					Player->RequestCampaignAction(TEXT("Resume"));
					OriginFrame = GFrameCounter;
					Phase = 17;
					return false;
				}
				for (const FGolemCampaignMarkerRow& Marker : Player->GetCampaignPresentation().Markers)
				{
					if (Marker.bDiscovered)
					{
						PingMarker = Marker.MarkerId;
						break;
					}
				}
				Test.TestFalse(TEXT("Normal movement reveals map markers"), PingMarker.IsNone());
				Player->GetRPGPanel()->OnCampaignActionRequested.Broadcast(TEXT("Ping"), PingMarker);
				Phase = 6;
				return false;
			}
			if (Phase == 6)
			{
				if (Player->GetCampaignPresentation().bPending)
				{
					return false;
				}
				Test.TestTrue(TEXT("Map ping confirms through actual RPC/domain"), Player->GetLastCampaignResult().IsSuccess());
				Test.TestEqual(TEXT("Actual transient map ping"), Mode->GetStoryState()->WorldState->GetMapPings().Num(), 1);
				Player->OpenCampaignPage(EGolemRPGPanelTab::Campaign);
				Click(TEXT("Save safe checkpoint"));
				Phase = 7;
				return false;
			}
			if (Phase == 7)
			{
				if (Player->GetCampaignPresentation().bPending)
				{
					return false;
				}
				Test.TestTrue(TEXT("Actual UI durable save confirmed"), Player->GetLastCampaignResult().IsSuccess());
				Test.TestTrue(TEXT("Save feedback uses real disk result"), Player->GetCampaignPresentation().SaveStatus.ToString().Contains(TEXT("Saved checkpoint")));
				FGolemCampaignCheckpoint Snapshot;
				Test.TestTrue(TEXT("Capture completed M7 route"), Mode->CaptureCheckpoint(Snapshot).IsSuccess());
				Completed = Snapshot;
				if (!bNetwork)
				{
					Test.TestTrue(TEXT("Persist real M7 route for cold process proof"), UGolemCheckpointSubsystem::WriteCheckpointFile(
						FPaths::ProjectDir() / TEXT("Saved/Evidence/M7/Fixtures/Main_completed.golem"), Snapshot).IsSuccess());
				}
				Player->RequestCampaignAction(TEXT("Resume"));
				Test.TestFalse(TEXT("Back releases owned pause"), World->IsPaused());
				Test.TestTrue(TEXT("Route used real CharacterMovement"), WalkDistance > 1500.0);
				Phase = 9;
				return false;
			}
			if (Phase == 9)
			{
				AGolemStoryAnchor* Travel = Mode->GetStoryState()->WorldState->FindAnchor(TEXT("School_Travel"));
				if (Travel == nullptr || !Walk(Travel->GetActorLocation() + FVector(0.0, -175.0, 30.0)))
				{
					return false;
				}
				Key(EKeys::W, false);
				Key(EKeys::F, true);
				Phase = 10;
				return false;
			}
			if (Phase == 10)
			{
				if (Mode->GetStoryState()->WorldState->GetActiveRegionId() != TEXT("Courtyard")
					|| Mode->GetPartyState()->GetHumanCount() != (bNetwork ? 3 : 1))
				{
					return false;
				}
				if (bNetwork)
				{
					int32 Converged = 0;
					for (const FWorldContext& Context : GEngine->GetWorldContexts())
					{
						UWorld* ClientWorld = Context.World();
						AGolemStoryState* ClientState = ClientWorld != nullptr ? ClientWorld->GetGameState<AGolemStoryState>() : nullptr;
						if (Context.WorldType == EWorldType::PIE && ClientWorld != nullptr && ClientWorld->GetNetMode() == NM_Client
							&& ClientState != nullptr && ClientState->WorldState->GetActiveRegionId() == TEXT("Courtyard")
							&& ClientState->WorldState->GetSnapshot().FactRevision == Mode->GetStoryState()->WorldState->GetSnapshot().FactRevision)
						{
							++Converged;
							Guest = Cast<AGolemCampaignPlayerController>(ClientWorld->GetFirstPlayerController());
						}
					}
					if (Converged != 2 || !Guest.IsValid() || !Guest->GetCampaignPresentation().bEnabled)
					{
						return false;
					}
					Guest->RequestCampaignAction(TEXT("Save"));
				}
				Test.TestEqual(TEXT("Travel clears transient pings"), Mode->GetStoryState()->WorldState->GetMapPings().Num(), 0);
				Phase = 11;
				return false;
			}
			if (Phase == 11)
			{
				if (bNetwork)
				{
					if (Guest->GetCampaignPresentation().bPending)
					{
						return false;
					}
					Test.TestEqual(TEXT("Actual guest save RPC rejected by host policy"), Guest->GetLastCampaignResult().Context, FName(TEXT("HostDecision")));
				}
				AGolemStoryAnchor* ReturnPoint = Mode->GetStoryState()->WorldState->FindAnchor(TEXT("School_Travel"));
				if (ReturnPoint == nullptr || !Walk(ReturnPoint->GetActorLocation() + FVector(0.0, -175.0, 30.0)))
				{
					return false;
				}
				Key(EKeys::W, false);
				Player->OpenCampaignPage(EGolemRPGPanelTab::Map);
				for (const FGolemCampaignMarkerRow& Marker : Player->GetCampaignPresentation().Markers)
				{
					if (Marker.bCanTravel)
					{
						Player->RequestCampaignAction(TEXT("FastTravel"), Marker.MarkerId);
						TravelMarker = Marker.MarkerId;
						Phase = 12;
						return false;
					}
				}
				return false;
			}
			if (Phase == 12)
			{
				if (!Player->GetCampaignPresentation().bPending && !Player->GetLastCampaignResult().IsSuccess())
				{
					if (Player->GetLastCampaignResult().Context == TEXT("MapRevision") && TravelRetries++ < 2)
					{
						Test.AddInfo(TEXT("Discovery changed during paused-map dispatch; visible rejection followed by an explicit UI retry."));
						Player->RequestCampaignAction(TEXT("FastTravel"), TravelMarker);
						return false;
					}
					Test.AddError(TEXT("Fast travel rejected: ") + Player->GetLastCampaignResult().Context.ToString());
					return true;
				}
				if (Mode->GetStoryState()->WorldState->GetActiveRegionId() != TEXT("School")
					|| Mode->GetPartyState()->GetHumanCount() != (bNetwork ? 3 : 1))
				{
					return false;
				}
				Test.TestTrue(TEXT("Fog for both visited regions survives authoritative round trip"),
					Mode->GetStoryState()->WorldState->IsRegionDiscovered(TEXT("School"))
					&& Mode->GetStoryState()->WorldState->IsRegionDiscovered(TEXT("Courtyard")));
				FGolemCampaignCheckpoint After;
				Test.TestTrue(TEXT("Safe checkpoint remains valid after normal travel and map fast travel"), Mode->CaptureCheckpoint(After).IsSuccess());
				Test.TestTrue(TEXT("Exact item GUIDs and reward receipts preserved across both maps"),
					After.RPG.Inventory.Items == Completed.RPG.Inventory.Items && After.RPG.View.ClaimedRewards == Completed.RPG.View.ClaimedRewards);
				for (int32 Index = 0; Index < 3; ++Index)
				{
					Test.TestTrue(TEXT("Canonical Golem stable identity survives travel"), After.RPG.View.Golems[Index].GolemId == Completed.RPG.View.Golems[Index].GolemId);
				}
				Test.TestFalse(TEXT("Travel releases movement input"), Player->IsMoveInputIgnored());
				Test.TestFalse(TEXT("Travel releases look input"), Player->IsLookInputIgnored());
				if (!bNetwork)
				{
					UGolemCheckpointSubsystem::WriteCheckpointFile(FPaths::ProjectDir() / TEXT("Saved/Evidence/M7/Fixtures/Main_completed.golem"), After);
					Test.TestTrue(TEXT("Quest-only unsaved change uses the existing owner"),
						Mode->GetStoryState()->Quest->ActivateQuest(TEXT("Demo_Append")).IsSuccess());
					Player->OpenCampaignPage(EGolemRPGPanelTab::Campaign);
					Click(TEXT("Load / inspect checkpoint"));
					Test.TestTrue(TEXT("Inspecting disk does not clear quest-only dirty state"),
						Player->GetCampaignPresentation().SaveStatus.ToString().Contains(TEXT("Unsaved")));
					Click(TEXT("Continue last checkpoint"));
					Test.TestEqual(TEXT("Destructive Continue requires confirmation"), Player->GetCampaignPresentation().Confirmation, FName(TEXT("Continue")));
					PreviousWorld = World;
					Click(TEXT("Confirm"));
					Phase = 15;
					return false;
				}
				else
				{
					for (const FWorldContext& Context : GEngine->GetWorldContexts())
					{
						if (Context.WorldType == EWorldType::PIE && Context.World() != nullptr && Context.World()->GetNetMode() == NM_Client)
						{
							Guest = Cast<AGolemCampaignPlayerController>(Context.World()->GetFirstPlayerController());
							if (Guest.IsValid())
							{
								Guest->ClientTravel(TEXT("/Game/Knight/Maps/Playables/L_M00_Boot"), TRAVEL_Absolute);
								Phase = 13;
								return false;
							}
						}
					}
				}
				GEditor->EndPlayMap();
				Phase = 8;
				return false;
			}
			if (Phase == 15)
			{
				if (World == PreviousWorld || Player->GetCampaignPresentation().bPending)
				{
					return false;
				}
				FGolemCampaignCheckpoint Reloaded;
				Test.TestTrue(TEXT("Normal Continue restores a valid checkpoint"), Mode->CaptureCheckpoint(Reloaded).IsSuccess());
				Test.TestTrue(TEXT("Continue preserves exact inventory and reward receipts"),
					Reloaded.RPG.Inventory.Items == Completed.RPG.Inventory.Items
					&& Reloaded.RPG.View.ClaimedRewards == Completed.RPG.View.ClaimedRewards);
				const FGolemQuestProgress* Appended = Mode->GetStoryState()->Quest->FindProgress(TEXT("Demo_Append"));
				Test.TestTrue(TEXT("Continue discards the unsaved quest activation"), Appended != nullptr && Appended->Phase == EGolemQuestPhase::Available);
				Player->OpenCampaignPage(EGolemRPGPanelTab::Campaign);
				Click(TEXT("Quit game"));
				Click(TEXT("Cancel / keep current progress"));
				Test.TestTrue(TEXT("Quit cancellation leaves the active campaign intact"), GEditor->IsPlaySessionInProgress());
				Click(TEXT("Quit game"));
				Click(TEXT("Confirm"));
				Phase = 8;
				return false;
			}
			if (Phase == 13)
			{
				if (Mode->GetPartyState()->GetHumanCount() != 2)
				{
					return false;
				}
				Test.TestEqual(TEXT("Disconnect hands control to AI without adding a Golem"), Mode->GetPartyState()->GetRoster().Num(), 3);
				GEditor->RequestLateJoin();
				Phase = 14;
				return false;
			}
			if (Phase == 14)
			{
				if (Mode->GetPartyState()->GetHumanCount() != 3)
				{
					return false;
				}
				int32 Converged = 0;
				for (const FWorldContext& Context : GEngine->GetWorldContexts())
				{
					UWorld* ClientWorld = Context.World();
					AGolemStoryState* State = ClientWorld != nullptr ? ClientWorld->GetGameState<AGolemStoryState>() : nullptr;
					if (Context.WorldType == EWorldType::PIE && ClientWorld != nullptr && ClientWorld->GetNetMode() == NM_Client
						&& State != nullptr && State->GetRPGState() != nullptr
						&& State->WorldState->GetSnapshot().FactRevision == Mode->GetStoryState()->WorldState->GetSnapshot().FactRevision
						&& State->WorldState->GetActiveRegionId() == TEXT("School")
						&& State->WorldState->IsRegionDiscovered(TEXT("Courtyard"))
						&& State->GetRPGState()->GetInventory()->GetView().Items == Completed.RPG.Inventory.Items)
					{
						++Converged;
					}
				}
				if (Converged != 2)
				{
					return false;
				}
				Test.TestEqual(TEXT("Actual late join reconstructs fog and inventory without a fourth Golem"), Mode->GetPartyState()->GetRoster().Num(), 3);
				GEditor->EndPlayMap();
				Phase = 8;
				return false;
			}
			if (GFrameCounter - OriginFrame >= 5 && Phase == 17)
			{
				Test.TestTrue(TEXT("Actual world origin shift succeeds"), World->SetNewWorldOrigin(FIntVector(20000, -10000, 0)));
				OriginFrame = GFrameCounter;
				Phase = 18;
				return false;
			}
			if (GFrameCounter - OriginFrame >= 5 && Phase == 18)
			{
				Player->OpenCampaignPage(EGolemRPGPanelTab::Map);
				Test.TestTrue(TEXT("Actual rebased world keeps the player on the calibrated map location"),
					Player->GetCampaignPresentation().PlayerUV.Equals(BeforeOrigin, 0.001));
				Player->RequestCampaignAction(TEXT("Resume"));
				OriginFrame = GFrameCounter;
				Phase = 19;
				return false;
			}
			if (GFrameCounter - OriginFrame >= 5 && Phase == 19)
			{
				Test.TestTrue(TEXT("Restore world origin after calibration fixture"), World->SetNewWorldOrigin(FIntVector::ZeroValue));
				bOriginVerified = true;
				Phase = 5;
				return false;
			}
			return Phase == 8 && !GEditor->IsPlaySessionInProgress();
		}

	private:
		void FindHost()
		{
			for (const FWorldContext& Context : GEngine->GetWorldContexts())
			{
				UWorld* Candidate = Context.World();
				if (Context.WorldType == EWorldType::PIE && Candidate != nullptr
					&& Candidate->GetAuthGameMode<AGolemCampaignGameMode>() != nullptr)
				{
					World = Candidate;
					Mode = Candidate->GetAuthGameMode<AGolemCampaignGameMode>();
					Player = Cast<AGolemCampaignPlayerController>(Candidate->GetFirstPlayerController());
				}
			}
		}
		void Click(const FString& Caption)
		{
			UGolemRPGActionButton* Found = nullptr;
			Player->GetRPGPanel()->WidgetTree->ForEachWidget([&Found, &Caption](UWidget* Widget)
			{
				if (UGolemRPGActionButton* Button = Cast<UGolemRPGActionButton>(Widget);
					Button != nullptr && Button->GetIsEnabled() && Button->GetCaption().ToString() == Caption)
				{
					Found = Button;
				}
			});
			if (Found == nullptr)
			{
				Test.AddError(TEXT("Missing/enabled UI action: ") + Caption);
				return;
			}
			Found->OnClicked().Broadcast();
		}
		void Key(const FKey& InKey, const bool bPressed)
		{
			Player->InputKey(FInputKeyEventArgs::CreateSimulated(InKey,
				bPressed ? IE_Pressed : IE_Released, bPressed ? 1.0f : 0.0f));
		}
		bool Walk(const FVector& Goal)
		{
			AGolemCharacter* Golem = Player->GetControlledGolem();
			if (!PreviousPosition.IsZero())
			{
				WalkDistance += FVector::Dist2D(PreviousPosition, Golem->GetActorLocation());
			}
			PreviousPosition = Golem->GetActorLocation();
			if (FVector::DistSquared2D(Golem->GetActorLocation(), Goal) < FMath::Square(45.0f))
			{
				return true;
			}
			if (World->GetTimeSeconds() >= NextPathAt || Path.IsEmpty())
			{
				FScopedConditionalWorldSwitcher Switch(World.Get());
				UNavigationPath* Navigation = UNavigationSystemV1::FindPathToLocationSynchronously(World.Get(),
					Golem->GetActorLocation(), Goal, Golem);
				Path = Navigation != nullptr && Navigation->IsValid() && !Navigation->IsPartial()
					? Navigation->PathPoints : TArray<FVector>();
				Waypoint = Path.Num() > 1 ? 1 : 0;
				NextPathAt = World->GetTimeSeconds() + 0.4;
			}
			if (Path.IsEmpty())
			{
				return false;
			}
			while (Waypoint < Path.Num() - 1 && FVector::DistSquared2D(Golem->GetActorLocation(), Path[Waypoint]) < FMath::Square(60.0f))
			{
				++Waypoint;
			}
			Player->SetControlRotation((Path[Waypoint] - Golem->GetActorLocation()).GetSafeNormal2D().Rotation());
			Key(EKeys::W, true);
			return false;
		}
		FAutomationTestBase& Test;
		bool bNetwork = false;
		double Started = 0.0;
		FLocalFileGuard Files;
		TStrongObjectPtr<ULevelEditorPlaySettings> Settings;
		TWeakObjectPtr<UWorld> World;
		TWeakObjectPtr<UWorld> PreviousWorld;
		TWeakObjectPtr<AGolemCampaignGameMode> Mode;
		TWeakObjectPtr<AGolemCampaignPlayerController> Player;
		TWeakObjectPtr<AGolemCampaignPlayerController> Guest;
		FGolemCampaignCheckpoint Completed;
		TArray<FVector> Path;
		FName PingMarker;
		FName TravelMarker;
		int32 TravelRetries = 0;
		FVector PreviousPosition = FVector::ZeroVector;
		double WalkDistance = 0.0;
		double NextPathAt = 0.0;
		double PulseAt = 0.0;
		int32 Waypoint = 0;
		int32 Phase = 0;
		int32 Step = 0;
		bool bPulse = false;
		bool bOriginVerified = false;
		uint64 OriginFrame = 0;
		FVector2D BeforeOrigin = FVector2D::ZeroVector;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGolemCampaignNormalUITest, "Golem.M7.NormalUIPlaythrough",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGolemCampaignNormalUITest::RunTest(const FString& Parameters)
{
	ADD_LATENT_AUTOMATION_COMMAND(GolemCampaignPlaythrough::FRun(*this, false));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGolemCampaignCoopUITest, "Golem.M7.HostAndTwoClientsUI",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGolemCampaignCoopUITest::RunTest(const FString& Parameters)
{
	ADD_LATENT_AUTOMATION_COMMAND(GolemCampaignPlaythrough::FRun(*this, true));
	return true;
}

#endif
