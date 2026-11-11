#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Character/GolemCharacter.h"
#include "Controllers/GolemCompanionController.h"
#include "Framework/GolemGameState.h"
#include "Framework/GolemPartyGameMode.h"
#include "Party/GolemPartyState.h"
#include "Player/GolemPlayerController.h"
#include "Player/GolemPlayerState.h"
#include "Settings/GolemSettings.h"

#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/NetConnection.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/PlatformTime.h"
#include "IPAddress.h"
#include "Misc/PackageName.h"
#include "PlayInEditorDataTypes.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "UnrealEngine.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectGlobals.h"

namespace GolemPartyNetworkTests
{
	const TCHAR* TrainingMap = TEXT("/Game/Knight/Maps/Playables/L_M01_TrainingYard");
	const TCHAR* DisconnectMap = TEXT("/Game/Knight/Maps/Playables/L_M00_Boot");
	constexpr double StartupTimeout = 45.0;
	constexpr double PhaseTimeout = 20.0;
	constexpr double MovementDuration = 1.5;
	constexpr double CompanionObservationDuration = 3.0;
	constexpr double CleanupTimeout = 15.0;
	constexpr uint16 TestServerPort = 17797;
	constexpr double RequiredMovementDistance = 100.0;
	constexpr double RequiredCompanionDistance = 20.0;
	constexpr double MovementConvergenceTolerance = 100.0;

	enum class EGolemNetworkPhase : uint8
	{
		Start,
		WaitForRoster,
		WaitForSwitchRejection,
		MoveOwningClient,
		WaitForMovement,
		WaitForTakeover,
		ObserveCompanion,
		WaitForReclaim,
		Cleanup,
		Finished
	};

	AGolemPartyState* GetParty(UWorld* World)
	{
		const AGolemGameState* GameState = World != nullptr ? World->GetGameState<AGolemGameState>() : nullptr;
		return GameState != nullptr ? GameState->GetPartyState() : nullptr;
	}

	int32 CountCanonicalActors(UWorld* World)
	{
		int32 Count = 0;
		if (World != nullptr)
		{
			for (TActorIterator<AGolemCharacter> Iterator(World); Iterator; ++Iterator)
			{
				++Count;
			}
		}
		return Count;
	}

	bool HasMatchingSnapshot(const AGolemPartyState* Authority, const AGolemPartyState* Replica)
	{
		if (Authority == nullptr || Replica == nullptr || !Authority->IsCanonicalRosterReady()
			|| !Replica->IsCanonicalRosterReady() || Authority->GetPartyId() != Replica->GetPartyId()
			|| Authority->GetRosterRevision() != Replica->GetRosterRevision())
		{
			return false;
		}
		for (int32 Slot = 0; Slot < UGolemSettings::CanonicalPartySize; ++Slot)
		{
			const FGolemRosterEntry* ServerEntry = Authority->FindSlot(Slot);
			const FGolemRosterEntry* ClientEntry = Replica->FindSlot(Slot);
			if (ServerEntry == nullptr || ClientEntry == nullptr || ServerEntry->Golem == nullptr
				|| ClientEntry->Golem == nullptr || ServerEntry->GolemId != ClientEntry->GolemId
				|| ServerEntry->HeroDefinitionId != ClientEntry->HeroDefinitionId
				|| ServerEntry->OwnerKind != ClientEntry->OwnerKind || ServerEntry->HumanId != ClientEntry->HumanId
				|| ServerEntry->HumanPlayerId != ClientEntry->HumanPlayerId
				|| ClientEntry->Golem->GetGolemEntityId() != ServerEntry->GolemId
				|| ClientEntry->Golem->GetHeroDefinitionId() != ServerEntry->HeroDefinitionId
				|| ClientEntry->Golem->GetHeroDefinition() == nullptr)
			{
				return false;
			}
			if (ServerEntry->OwnerKind == EGolemControllerOwner::Human
				&& (ClientEntry->HumanPlayerState == nullptr
					|| ClientEntry->HumanPlayerState->GetHumanId() != ServerEntry->HumanId
					|| ClientEntry->HumanPlayerState->GetAssignedSlot() != Slot))
			{
				return false;
			}
		}
		return true;
	}

	/** One bounded scenario owns its PIE session and transient settings for its entire lifetime. */
	class FGolemNetworkScenario final : public IAutomationLatentCommand
	{
	public:
		explicit FGolemNetworkScenario(FAutomationTestBase& InTest)
			: Test(InTest)
		{
		}

		virtual ~FGolemNetworkScenario() override
		{
			if (bOwnsSession && GEditor != nullptr)
			{
				if (GEditor->IsPlayingSessionInEditor())
				{
					GEditor->EndPlayMap();
				}
				else if (GEditor->IsPlaySessionRequestQueued())
				{
					GEditor->CancelRequestPlaySession();
				}
				if (!GEditor->IsPlaySessionInProgress())
				{
					RestorePlayWindowSettings();
				}
			}
		}

		virtual bool Update() override
		{
			if (Phase != EGolemNetworkPhase::Cleanup && Phase != EGolemNetworkPhase::Finished && Test.HasAnyErrors())
			{
				BeginCleanup();
			}
			switch (Phase)
			{
			case EGolemNetworkPhase::Start:
				StartSession();
				break;
			case EGolemNetworkPhase::WaitForRoster:
				WaitForRoster();
				break;
			case EGolemNetworkPhase::WaitForSwitchRejection:
				WaitForSwitchRejection();
				break;
			case EGolemNetworkPhase::MoveOwningClient:
				MoveOwningClient();
				break;
			case EGolemNetworkPhase::WaitForMovement:
				WaitForMovement();
				break;
			case EGolemNetworkPhase::WaitForTakeover:
				WaitForTakeover();
				break;
			case EGolemNetworkPhase::ObserveCompanion:
				ObserveCompanion();
				break;
			case EGolemNetworkPhase::WaitForReclaim:
				WaitForReclaim();
				break;
			case EGolemNetworkPhase::Cleanup:
				Cleanup();
				break;
			case EGolemNetworkPhase::Finished:
				return true;
			}
			return Phase == EGolemNetworkPhase::Finished;
		}

	private:
		void EnterPhase(const EGolemNetworkPhase NewPhase)
		{
			Phase = NewPhase;
			PhaseStarted = FPlatformTime::Seconds();
		}

		bool HasTimedOut(const double Timeout) const
		{
			return FPlatformTime::Seconds() - PhaseStarted >= Timeout;
		}

		void Fail(const FString& Message)
		{
			Test.AddError(Message);
			BeginCleanup();
		}

		void StartSession()
		{
			if (GEditor == nullptr || GEditor->IsPlaySessionInProgress())
			{
				Fail(TEXT("Network scenario requires an idle editor and will not end an existing user PIE session."));
				return;
			}
			if (!FPackageName::DoesPackageExist(TrainingMap) || !FPackageName::DoesPackageExist(DisconnectMap))
			{
				Fail(TEXT("Generate both real M0/M1 maps with GolemBootstrap before this network scenario."));
				return;
			}
			const ULevelEditorPlaySettings* DefaultSettings = GetDefault<ULevelEditorPlaySettings>();
			DefaultSettings->GetPlayNetMode(OriginalNetMode);
			DefaultSettings->GetRunUnderOneProcess(bOriginalOneProcess);
			DefaultSettings->GetPlayNumberOfClients(OriginalClientCount);
			OriginalWindowPositions = DefaultSettings->MultipleInstancePositions;
			OriginalLastSize = DefaultSettings->LastSize;
			OriginalWindowPosition = DefaultSettings->NewWindowPosition;
			OriginalWindowWidth = DefaultSettings->NewWindowWidth;
			OriginalWindowHeight = DefaultSettings->NewWindowHeight;
			Settings.Reset(DuplicateObject<ULevelEditorPlaySettings>(DefaultSettings, GetTransientPackage()));
			Settings->SetPlayNetMode(PIE_ListenServer);
			Settings->SetRunUnderOneProcess(true);
			Settings->SetPlayNumberOfClients(3);
			Settings->SetServerPort(TestServerPort);
			Settings->bLaunchSeparateServer = false;
			Settings->EnableGameSound = false;
			Settings->GameGetsMouseControl = false;
			Settings->NetworkEmulationSettings.bIsNetworkEmulationEnabled = false;
			Settings->SetClientWindowSize(FIntPoint(640, 360));

			FRequestPlaySessionParams Request;
			Request.EditorPlaySettings = Settings.Get();
			Request.GlobalMapOverride = TrainingMap;
			Request.bAllowOnlineSubsystem = false;
			GEditor->RequestPlaySession(Request);
			bOwnsSession = true;
			Test.AddInfo(TEXT("Started real PIE listen server + two client instances using transient play settings."));
			EnterPhase(EGolemNetworkPhase::WaitForRoster);
		}

		void FindNetworkWorlds()
		{
			ClientContexts.Reset();
			for (const FWorldContext& Context : GEngine->GetWorldContexts())
			{
				UWorld* World = Context.World();
				if (Context.WorldType != EWorldType::PIE || World == nullptr)
				{
					continue;
				}
				if (World->GetNetMode() == NM_ListenServer)
				{
					ServerWorld = World;
				}
				else if (World->GetNetMode() == NM_Client)
				{
					ClientContexts.Add(Context.ContextHandle);
				}
			}
		}

		UWorld* GetContextWorld(const FName Handle) const
		{
			const FWorldContext* Context = GEngine->GetWorldContextFromHandle(Handle);
			return Context != nullptr ? Context->World() : nullptr;
		}

		bool HaveConvergedClients(const int32 ExpectedHumanCount, const FName SkippedContext = NAME_None) const
		{
			const AGolemPartyState* Authority = GetParty(ServerWorld.Get());
			if (Authority == nullptr || Authority->GetHumanCount() != ExpectedHumanCount
				|| CountCanonicalActors(ServerWorld.Get()) != UGolemSettings::CanonicalPartySize)
			{
				return false;
			}
			for (const FName Handle : ClientContexts)
			{
				if (Handle == SkippedContext)
				{
					continue;
				}
				UWorld* ClientWorld = GetContextWorld(Handle);
				if (ClientWorld == nullptr || CountCanonicalActors(ClientWorld) != UGolemSettings::CanonicalPartySize
					|| !HasMatchingSnapshot(Authority, GetParty(ClientWorld)))
				{
					return false;
				}
			}
			return true;
		}

		bool HasPreservedCanonicalActors() const
		{
			const AGolemPartyState* Authority = GetParty(ServerWorld.Get());
			if (Authority == nullptr || CanonicalActors.Num() != UGolemSettings::CanonicalPartySize
				|| CountCanonicalActors(ServerWorld.Get()) != UGolemSettings::CanonicalPartySize)
			{
				return false;
			}
			for (int32 Slot = 0; Slot < CanonicalActors.Num(); ++Slot)
			{
				const FGolemRosterEntry* Entry = Authority->FindSlot(Slot);
				if (Entry == nullptr || !CanonicalActors[Slot].IsValid() || Entry->Golem != CanonicalActors[Slot].Get()
					|| Entry->GolemId != CanonicalIds[Slot])
				{
					return false;
				}
			}
			return true;
		}

		void WaitForRoster()
		{
			FindNetworkWorlds();
			if (ServerWorld.IsValid() && ClientContexts.Num() == 2 && HaveConvergedClients(3))
			{
				AGolemPartyState* Authority = GetParty(ServerWorld.Get());
				for (int32 Slot = 0; Slot < UGolemSettings::CanonicalPartySize; ++Slot)
				{
					const FGolemRosterEntry* Entry = Authority->FindSlot(Slot);
					CanonicalActors.Add(Entry->Golem.Get());
					CanonicalIds.Add(Entry->GolemId);
				}
				Test.AddInfo(TEXT("All three real network worlds contain exactly three canonical actors; "
					"FastArray identity, definition, human identity, slot ownership and revision converge."));
				DisconnectedContext = ClientContexts[0];
				UWorld* ClientWorld = GetContextWorld(DisconnectedContext);
				AGolemPlayerController* Client = Cast<AGolemPlayerController>(ClientWorld->GetFirstPlayerController());
				const AGolemPlayerState* Human = Client != nullptr ? Client->GetPlayerState<AGolemPlayerState>() : nullptr;
				if (Client == nullptr || Human == nullptr || !Client->IsLocalController() || Client->HasAuthority()
					|| Client->GetControlledGolem() == nullptr || Human->GetAssignedSlot() == INDEX_NONE)
				{
					Fail(TEXT("Client lacks a locally owned canonical pawn after snapshot convergence."));
					return;
				}
				ClientController = Client;
				ReleasedSlot = Human->GetAssignedSlot();
				OriginalRevision = Authority->GetRosterRevision();
				UNetDriver* ServerDriver = ServerWorld->GetNetDriver();
				const TSharedPtr<const FInternetAddr> ServerAddress = ServerDriver != nullptr
					? ServerDriver->GetLocalAddr() : nullptr;
				if (!ServerAddress.IsValid() || ServerAddress->GetPort() <= 0 || ServerAddress->GetPort() > 65535)
				{
					Fail(TEXT("Listen server has no valid bound socket address for a real reconnect."));
					return;
				}
				ReconnectURL = FString::Printf(TEXT("127.0.0.1:%d?GolemSlot=%d"),
					ServerAddress->GetPort(), ReleasedSlot);
				Test.AddInfo(FString::Printf(TEXT("Reconnect uses actual listen socket port %d (configured %d)."),
					ServerAddress->GetPort(), TestServerPort));
				FScopedConditionalWorldSwitcher WorldSwitcher(ClientWorld);
				Client->RequestSwitchGolem((ReleasedSlot + 1) % UGolemSettings::CanonicalPartySize);
				EnterPhase(EGolemNetworkPhase::WaitForSwitchRejection);
			}
			else if (HasTimedOut(StartupTimeout))
			{
				Fail(TEXT("Timed out waiting for a listen server, two real client worlds and converged canonical roster."));
			}
		}

		void WaitForSwitchRejection()
		{
			AGolemPlayerController* Client = ClientController.Get();
			const AGolemPartyState* Authority = GetParty(ServerWorld.Get());
			if (Client == nullptr || Authority == nullptr)
			{
				Fail(TEXT("A network world/controller vanished during the owning-client switch RPC."));
				return;
			}
			if (Client->GetLastCommandResult().Context == FName(TEXT("SwitchGolem")))
			{
				if (Client->GetLastCommandResult().Error != EGolemError::InvalidState
					|| Authority->GetRosterRevision() != OriginalRevision
					|| Client->GetControlledGolem() == nullptr
					|| Client->GetControlledGolem()->GetGolemEntityId() != CanonicalIds[ReleasedSlot]
					|| !HasPreservedCanonicalActors())
				{
					Fail(TEXT("Co-op switch RPC did not reject without changing authoritative ownership/identity."));
					return;
				}
				MovementStart = CanonicalActors[ReleasedSlot]->GetActorLocation();
				Test.AddInfo(TEXT("Owning client sent a real ServerRequestSwitchGolem RPC; "
					"ClientReceiveCommandResult returned InvalidState, preserving the canonical roster."));
				EnterPhase(EGolemNetworkPhase::MoveOwningClient);
			}
			else if (HasTimedOut(PhaseTimeout))
			{
				Fail(TEXT("Timed out waiting for the server's co-op switch rejection to reach the owning client."));
			}
		}

		void MoveOwningClient()
		{
			AGolemPlayerController* Client = ClientController.Get();
			AGolemCharacter* Golem = Client != nullptr ? Client->GetControlledGolem() : nullptr;
			if (Golem == nullptr || Golem->HasAuthority() || Golem->GetLocalRole() != ROLE_AutonomousProxy
				|| !Golem->IsLocallyControlled())
			{
				Fail(TEXT("Movement test pawn is not the owning client's real autonomous proxy."));
				return;
			}
			FScopedConditionalWorldSwitcher WorldSwitcher(Golem->GetWorld());
			Golem->AddMovementInput(FVector(0.0, 1.0, 0.0), 1.0f);
			if (HasTimedOut(MovementDuration))
			{
				Client->ResetControlInput();
				EnterPhase(EGolemNetworkPhase::WaitForMovement);
			}
		}

		void WaitForMovement()
		{
			const AGolemCharacter* ServerGolem = CanonicalActors.IsValidIndex(ReleasedSlot)
				? CanonicalActors[ReleasedSlot].Get() : nullptr;
			const AGolemPlayerController* Client = ClientController.Get();
			const AGolemCharacter* ClientGolem = Client != nullptr ? Client->GetControlledGolem() : nullptr;
			if (ServerGolem == nullptr || ClientGolem == nullptr)
			{
				Fail(TEXT("Canonical pawn vanished during client CharacterMovement replication."));
				return;
			}
			const double Travelled = FVector::Dist2D(MovementStart, ServerGolem->GetActorLocation());
			const double Error = FVector::Dist2D(ServerGolem->GetActorLocation(), ClientGolem->GetActorLocation());
			if (Travelled >= RequiredMovementDistance && Error <= MovementConvergenceTolerance
				&& HaveConvergedClients(3) && HasPreservedCanonicalActors())
			{
				Test.AddInfo(FString::Printf(TEXT("Owning-client CharacterMovement reached server: %.1f cm; "
					"server/client position error %.1f cm. Actor/definition/ownership stable."), Travelled, Error));
				UNetDriver* ClientDriver = ClientGolem->GetWorld()->GetNetDriver();
				if (ClientDriver == nullptr || ClientDriver->ServerConnection == nullptr)
				{
					Fail(TEXT("Disconnect scenario has no actual client server connection to close."));
					return;
				}
				FScopedConditionalWorldSwitcher WorldSwitcher(ClientGolem->GetWorld());
				ClientController->ClientTravel(DisconnectMap, TRAVEL_Absolute);
				Test.AddInfo(TEXT("Owning client requested ClientTravel to M0 boot; "
					"engine travel tears down its real connection without invoking GameMode Logout from the test."));
				EnterPhase(EGolemNetworkPhase::WaitForTakeover);
			}
			else if (HasTimedOut(PhaseTimeout))
			{
				Fail(FString::Printf(TEXT("Client movement did not converge: authority distance %.1f cm, error %.1f cm."),
					Travelled, Error));
			}
		}

		void WaitForTakeover()
		{
			AGolemPartyState* Authority = GetParty(ServerWorld.Get());
			const FGolemRosterEntry* Entry = Authority != nullptr ? Authority->FindSlot(ReleasedSlot) : nullptr;
			UWorld* DetachedWorld = GetContextWorld(DisconnectedContext);
			if (Entry != nullptr && IsValid(Entry->Golem) && HaveConvergedClients(2, DisconnectedContext)
				&& Entry->OwnerKind == EGolemControllerOwner::Companion && DetachedWorld != nullptr
				&& DetachedWorld->GetNetMode() == NM_Standalone && DetachedWorld->GetNetDriver() == nullptr)
			{
				AGolemCompanionController* Companion = Cast<AGolemCompanionController>(Entry->Golem->GetController());
				if (!HasPreservedCanonicalActors() || Companion == nullptr || !Companion->IsFollowing()
					|| Entry->HumanId.IsValid())
				{
					Fail(TEXT("Actual network disconnect failed AI takeover, cleared ownership, or actor/identity preservation."));
					return;
				}
				CompanionStart = Entry->Golem->GetActorLocation();
				Test.AddInfo(FString::Printf(TEXT("Real disconnect reclaimed slot %d with a running native StateTree; "
					"all original server actors/IDs preserved and no fourth Golem."), ReleasedSlot));
				EnterPhase(EGolemNetworkPhase::ObserveCompanion);
			}
			else if (HasTimedOut(PhaseTimeout))
			{
				Fail(TEXT("Timed out waiting for actual client disconnect, standalone client world, and server AI takeover."));
			}
		}

		void ObserveCompanion()
		{
			const AGolemPartyState* Authority = GetParty(ServerWorld.Get());
			const FGolemRosterEntry* Entry = Authority != nullptr ? Authority->FindSlot(ReleasedSlot) : nullptr;
			if (Entry == nullptr || !HasPreservedCanonicalActors())
			{
				Fail(TEXT("AI cohesion changed or lost a canonical actor after disconnect."));
				return;
			}
			if (!HasTimedOut(CompanionObservationDuration))
			{
				return;
			}
			const double Travelled = FVector::Dist2D(CompanionStart, Entry->Golem->GetActorLocation());
			if (Travelled < RequiredCompanionDistance)
			{
				Fail(FString::Printf(TEXT("StateTree takeover did not visibly regroup through navigation: %.1f cm."), Travelled));
				return;
			}
			UWorld* DetachedWorld = GetContextWorld(DisconnectedContext);
			APlayerController* DetachedPlayer = DetachedWorld != nullptr ? DetachedWorld->GetFirstPlayerController() : nullptr;
			if (DetachedPlayer == nullptr)
			{
				Fail(TEXT("PIE client context did not survive local travel to permit a real same-slot reconnect."));
				return;
			}
			FScopedConditionalWorldSwitcher WorldSwitcher(DetachedWorld);
			DetachedPlayer->ClientTravel(ReconnectURL, TRAVEL_Absolute);
			Test.AddInfo(FString::Printf(TEXT("Native StateTree/nav regroup moved %.1f cm; "
				"reconnecting the detached PIE client through %s."), Travelled, *ReconnectURL));
			EnterPhase(EGolemNetworkPhase::WaitForReclaim);
		}

		void WaitForReclaim()
		{
			UWorld* ReconnectedWorld = GetContextWorld(DisconnectedContext);
			AGolemPlayerController* Client = ReconnectedWorld != nullptr
				? Cast<AGolemPlayerController>(ReconnectedWorld->GetFirstPlayerController()) : nullptr;
			const AGolemPlayerState* Human = Client != nullptr ? Client->GetPlayerState<AGolemPlayerState>() : nullptr;
			if (ReconnectedWorld != nullptr && ReconnectedWorld->GetNetMode() == NM_Client
				&& HaveConvergedClients(3) && Human != nullptr && Human->GetAssignedSlot() == ReleasedSlot
				&& Client->GetControlledGolem() != nullptr
				&& Client->GetControlledGolem()->GetGolemEntityId() == CanonicalIds[ReleasedSlot])
			{
				if (!HasPreservedCanonicalActors())
				{
					Fail(TEXT("Same-slot network reconnect replaced or duplicated a canonical server Golem."));
					return;
				}
				Test.AddInfo(TEXT("Real reconnect reclaimed the released slot; all three network worlds "
					"converge again with exactly three original canonical Golem identities."));
				BeginCleanup();
			}
			else if (HasTimedOut(PhaseTimeout))
			{
				Fail(TEXT("Timed out waiting for actual PIE ClientTravel reconnect and same-slot human reclaim."));
			}
		}

		void BeginCleanup()
		{
			if (Phase == EGolemNetworkPhase::Cleanup || Phase == EGolemNetworkPhase::Finished)
			{
				return;
			}
			EnterPhase(EGolemNetworkPhase::Cleanup);
			if (bOwnsSession && GEditor != nullptr)
			{
				if (GEditor->IsPlayingSessionInEditor())
				{
					// CancelRequestPlaySession clears live session metadata needed by EndPlayMap.
					GEditor->RequestEndPlayMap();
				}
				else if (GEditor->IsPlaySessionRequestQueued())
				{
					GEditor->CancelRequestPlaySession();
				}
			}
		}

		void RestorePlayWindowSettings()
		{
			if (!Settings.IsValid())
			{
				return;
			}
			// EndPlayMap persists window layout to the CDO even when the request uses transient settings.
			ULevelEditorPlaySettings* DefaultSettings = GetMutableDefault<ULevelEditorPlaySettings>();
			DefaultSettings->MultipleInstancePositions = OriginalWindowPositions;
			DefaultSettings->LastSize = OriginalLastSize;
			DefaultSettings->NewWindowPosition = OriginalWindowPosition;
			DefaultSettings->NewWindowWidth = OriginalWindowWidth;
			DefaultSettings->NewWindowHeight = OriginalWindowHeight;
			DefaultSettings->SaveConfig();
		}

		void Cleanup()
		{
			if (bOwnsSession && GEditor != nullptr && GEditor->IsPlaySessionInProgress())
			{
				if (HasTimedOut(CleanupTimeout))
				{
					Test.AddError(TEXT("PIE network cleanup timed out; forcing synchronous editor teardown."));
					if (GEditor->IsPlayingSessionInEditor())
					{
						GEditor->EndPlayMap();
					}
					else if (GEditor->IsPlaySessionRequestQueued())
					{
						GEditor->CancelRequestPlaySession();
					}
				}
				else
				{
					return;
				}
			}
			if (Settings.IsValid())
			{
				RestorePlayWindowSettings();
				const ULevelEditorPlaySettings* DefaultSettings = GetDefault<ULevelEditorPlaySettings>();
				EPlayNetMode CurrentNetMode = PIE_Standalone;
				bool bCurrentOneProcess = false;
				int32 CurrentClientCount = 0;
				DefaultSettings->GetPlayNetMode(CurrentNetMode);
				DefaultSettings->GetRunUnderOneProcess(bCurrentOneProcess);
				DefaultSettings->GetPlayNumberOfClients(CurrentClientCount);
				Test.TestEqual(TEXT("User play net mode remains unchanged"), CurrentNetMode, OriginalNetMode);
				Test.TestEqual(TEXT("User single-process preference remains unchanged"),
					bCurrentOneProcess, bOriginalOneProcess);
				Test.TestEqual(TEXT("User client count remains unchanged"), CurrentClientCount, OriginalClientCount);
			}
			Settings.Reset();
			CanonicalActors.Reset();
			ServerWorld.Reset();
			ClientController.Reset();
			ClientContexts.Reset();
			bOwnsSession = false;
			EnterPhase(EGolemNetworkPhase::Finished);
		}

		FAutomationTestBase& Test;
		EGolemNetworkPhase Phase = EGolemNetworkPhase::Start;
		double PhaseStarted = 0.0;
		bool bOwnsSession = false;
		TStrongObjectPtr<ULevelEditorPlaySettings> Settings;
		EPlayNetMode OriginalNetMode = PIE_Standalone;
		bool bOriginalOneProcess = false;
		int32 OriginalClientCount = 0;
		TArray<FIntPoint> OriginalWindowPositions;
		FIntPoint OriginalLastSize = FIntPoint::ZeroValue;
		FIntPoint OriginalWindowPosition = FIntPoint::ZeroValue;
		int32 OriginalWindowWidth = 0;
		int32 OriginalWindowHeight = 0;
		TWeakObjectPtr<UWorld> ServerWorld;
		TArray<FName> ClientContexts;
		FName DisconnectedContext;
		TWeakObjectPtr<AGolemPlayerController> ClientController;
		TArray<TWeakObjectPtr<AGolemCharacter>> CanonicalActors;
		TArray<FGolemEntityId> CanonicalIds;
		int32 ReleasedSlot = INDEX_NONE;
		int32 OriginalRevision = 0;
		FVector MovementStart = FVector::ZeroVector;
		FVector CompanionStart = FVector::ZeroVector;
		FString ReconnectURL;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGolemPartyNetworkTest, "Golem.Party.NetworkOwnershipTakeoverReclaim",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGolemPartyNetworkTest::RunTest(const FString& Parameters)
{
	FAutomationTestFramework::Get().EnqueueLatentCommand(
		MakeShared<GolemPartyNetworkTests::FGolemNetworkScenario>(*this));
	return true;
}

#endif
