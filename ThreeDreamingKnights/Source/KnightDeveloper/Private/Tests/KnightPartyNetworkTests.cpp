#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Character/KnightCharacter.h"
#include "Controllers/KnightCompanionController.h"
#include "Framework/KnightGameState.h"
#include "Framework/KnightPartyGameMode.h"
#include "Party/KnightPartyState.h"
#include "Player/KnightPlayerController.h"
#include "Player/KnightPlayerState.h"
#include "Settings/KnightSettings.h"

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

namespace KnightPartyNetworkTests
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

	enum class EKnightNetworkPhase : uint8
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

	AKnightPartyState* GetParty(UWorld* World)
	{
		const AKnightGameState* GameState = World != nullptr ? World->GetGameState<AKnightGameState>() : nullptr;
		return GameState != nullptr ? GameState->GetPartyState() : nullptr;
	}

	int32 CountCanonicalActors(UWorld* World)
	{
		int32 Count = 0;
		if (World != nullptr)
		{
			for (TActorIterator<AKnightCharacter> Iterator(World); Iterator; ++Iterator)
			{
				++Count;
			}
		}
		return Count;
	}

	bool HasMatchingSnapshot(const AKnightPartyState* Authority, const AKnightPartyState* Replica)
	{
		if (Authority == nullptr || Replica == nullptr || !Authority->IsCanonicalRosterReady()
			|| !Replica->IsCanonicalRosterReady() || Authority->GetPartyId() != Replica->GetPartyId()
			|| Authority->GetRosterRevision() != Replica->GetRosterRevision())
		{
			return false;
		}
		for (int32 Slot = 0; Slot < UKnightSettings::CanonicalPartySize; ++Slot)
		{
			const FKnightRosterEntry* ServerEntry = Authority->FindSlot(Slot);
			const FKnightRosterEntry* ClientEntry = Replica->FindSlot(Slot);
			if (ServerEntry == nullptr || ClientEntry == nullptr || ServerEntry->Knight == nullptr
				|| ClientEntry->Knight == nullptr || ServerEntry->KnightId != ClientEntry->KnightId
				|| ServerEntry->HeroDefinitionId != ClientEntry->HeroDefinitionId
				|| ServerEntry->OwnerKind != ClientEntry->OwnerKind || ServerEntry->HumanId != ClientEntry->HumanId
				|| ServerEntry->HumanPlayerId != ClientEntry->HumanPlayerId
				|| ClientEntry->Knight->GetKnightEntityId() != ServerEntry->KnightId
				|| ClientEntry->Knight->GetHeroDefinitionId() != ServerEntry->HeroDefinitionId
				|| ClientEntry->Knight->GetHeroDefinition() == nullptr)
			{
				return false;
			}
			if (ServerEntry->OwnerKind == EKnightControllerOwner::Human
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
	class FKnightNetworkScenario final : public IAutomationLatentCommand
	{
	public:
		explicit FKnightNetworkScenario(FAutomationTestBase& InTest)
			: Test(InTest)
		{
		}

		virtual ~FKnightNetworkScenario() override
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
			if (Phase != EKnightNetworkPhase::Cleanup && Phase != EKnightNetworkPhase::Finished && Test.HasAnyErrors())
			{
				BeginCleanup();
			}
			switch (Phase)
			{
			case EKnightNetworkPhase::Start:
				StartSession();
				break;
			case EKnightNetworkPhase::WaitForRoster:
				WaitForRoster();
				break;
			case EKnightNetworkPhase::WaitForSwitchRejection:
				WaitForSwitchRejection();
				break;
			case EKnightNetworkPhase::MoveOwningClient:
				MoveOwningClient();
				break;
			case EKnightNetworkPhase::WaitForMovement:
				WaitForMovement();
				break;
			case EKnightNetworkPhase::WaitForTakeover:
				WaitForTakeover();
				break;
			case EKnightNetworkPhase::ObserveCompanion:
				ObserveCompanion();
				break;
			case EKnightNetworkPhase::WaitForReclaim:
				WaitForReclaim();
				break;
			case EKnightNetworkPhase::Cleanup:
				Cleanup();
				break;
			case EKnightNetworkPhase::Finished:
				return true;
			}
			return Phase == EKnightNetworkPhase::Finished;
		}

	private:
		void EnterPhase(const EKnightNetworkPhase NewPhase)
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
				Fail(TEXT("Generate both real M0/M1 maps with KnightBootstrap before this network scenario."));
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
			EnterPhase(EKnightNetworkPhase::WaitForRoster);
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
			const AKnightPartyState* Authority = GetParty(ServerWorld.Get());
			if (Authority == nullptr || Authority->GetHumanCount() != ExpectedHumanCount
				|| CountCanonicalActors(ServerWorld.Get()) != UKnightSettings::CanonicalPartySize)
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
				if (ClientWorld == nullptr || CountCanonicalActors(ClientWorld) != UKnightSettings::CanonicalPartySize
					|| !HasMatchingSnapshot(Authority, GetParty(ClientWorld)))
				{
					return false;
				}
			}
			return true;
		}

		bool HasPreservedCanonicalActors() const
		{
			const AKnightPartyState* Authority = GetParty(ServerWorld.Get());
			if (Authority == nullptr || CanonicalActors.Num() != UKnightSettings::CanonicalPartySize
				|| CountCanonicalActors(ServerWorld.Get()) != UKnightSettings::CanonicalPartySize)
			{
				return false;
			}
			for (int32 Slot = 0; Slot < CanonicalActors.Num(); ++Slot)
			{
				const FKnightRosterEntry* Entry = Authority->FindSlot(Slot);
				if (Entry == nullptr || !CanonicalActors[Slot].IsValid() || Entry->Knight != CanonicalActors[Slot].Get()
					|| Entry->KnightId != CanonicalIds[Slot])
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
				AKnightPartyState* Authority = GetParty(ServerWorld.Get());
				for (int32 Slot = 0; Slot < UKnightSettings::CanonicalPartySize; ++Slot)
				{
					const FKnightRosterEntry* Entry = Authority->FindSlot(Slot);
					CanonicalActors.Add(Entry->Knight.Get());
					CanonicalIds.Add(Entry->KnightId);
				}
				Test.AddInfo(TEXT("All three real network worlds contain exactly three canonical actors; "
					"FastArray identity, definition, human identity, slot ownership and revision converge."));
				DisconnectedContext = ClientContexts[0];
				UWorld* ClientWorld = GetContextWorld(DisconnectedContext);
				AKnightPlayerController* Client = Cast<AKnightPlayerController>(ClientWorld->GetFirstPlayerController());
				const AKnightPlayerState* Human = Client != nullptr ? Client->GetPlayerState<AKnightPlayerState>() : nullptr;
				if (Client == nullptr || Human == nullptr || !Client->IsLocalController() || Client->HasAuthority()
					|| Client->GetControlledKnight() == nullptr || Human->GetAssignedSlot() == INDEX_NONE)
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
				ReconnectURL = FString::Printf(TEXT("127.0.0.1:%d?KnightSlot=%d"),
					ServerAddress->GetPort(), ReleasedSlot);
				Test.AddInfo(FString::Printf(TEXT("Reconnect uses actual listen socket port %d (configured %d)."),
					ServerAddress->GetPort(), TestServerPort));
				FScopedConditionalWorldSwitcher WorldSwitcher(ClientWorld);
				Client->RequestSwitchKnight((ReleasedSlot + 1) % UKnightSettings::CanonicalPartySize);
				EnterPhase(EKnightNetworkPhase::WaitForSwitchRejection);
			}
			else if (HasTimedOut(StartupTimeout))
			{
				Fail(TEXT("Timed out waiting for a listen server, two real client worlds and converged canonical roster."));
			}
		}

		void WaitForSwitchRejection()
		{
			AKnightPlayerController* Client = ClientController.Get();
			const AKnightPartyState* Authority = GetParty(ServerWorld.Get());
			if (Client == nullptr || Authority == nullptr)
			{
				Fail(TEXT("A network world/controller vanished during the owning-client switch RPC."));
				return;
			}
			if (Client->GetLastCommandResult().Context == FName(TEXT("SwitchKnight")))
			{
				if (Client->GetLastCommandResult().Error != EKnightError::InvalidState
					|| Authority->GetRosterRevision() != OriginalRevision
					|| Client->GetControlledKnight() == nullptr
					|| Client->GetControlledKnight()->GetKnightEntityId() != CanonicalIds[ReleasedSlot]
					|| !HasPreservedCanonicalActors())
				{
					Fail(TEXT("Co-op switch RPC did not reject without changing authoritative ownership/identity."));
					return;
				}
				MovementStart = CanonicalActors[ReleasedSlot]->GetActorLocation();
				Test.AddInfo(TEXT("Owning client sent a real ServerRequestSwitchKnight RPC; "
					"ClientReceiveCommandResult returned InvalidState, preserving the canonical roster."));
				EnterPhase(EKnightNetworkPhase::MoveOwningClient);
			}
			else if (HasTimedOut(PhaseTimeout))
			{
				Fail(TEXT("Timed out waiting for the server's co-op switch rejection to reach the owning client."));
			}
		}

		void MoveOwningClient()
		{
			AKnightPlayerController* Client = ClientController.Get();
			AKnightCharacter* Knight = Client != nullptr ? Client->GetControlledKnight() : nullptr;
			if (Knight == nullptr || Knight->HasAuthority() || Knight->GetLocalRole() != ROLE_AutonomousProxy
				|| !Knight->IsLocallyControlled())
			{
				Fail(TEXT("Movement test pawn is not the owning client's real autonomous proxy."));
				return;
			}
			FScopedConditionalWorldSwitcher WorldSwitcher(Knight->GetWorld());
			Knight->AddMovementInput(FVector(0.0, 1.0, 0.0), 1.0f);
			if (HasTimedOut(MovementDuration))
			{
				Client->ResetControlInput();
				EnterPhase(EKnightNetworkPhase::WaitForMovement);
			}
		}

		void WaitForMovement()
		{
			const AKnightCharacter* ServerKnight = CanonicalActors.IsValidIndex(ReleasedSlot)
				? CanonicalActors[ReleasedSlot].Get() : nullptr;
			const AKnightPlayerController* Client = ClientController.Get();
			const AKnightCharacter* ClientKnight = Client != nullptr ? Client->GetControlledKnight() : nullptr;
			if (ServerKnight == nullptr || ClientKnight == nullptr)
			{
				Fail(TEXT("Canonical pawn vanished during client CharacterMovement replication."));
				return;
			}
			const double Travelled = FVector::Dist2D(MovementStart, ServerKnight->GetActorLocation());
			const double Error = FVector::Dist2D(ServerKnight->GetActorLocation(), ClientKnight->GetActorLocation());
			if (Travelled >= RequiredMovementDistance && Error <= MovementConvergenceTolerance
				&& HaveConvergedClients(3) && HasPreservedCanonicalActors())
			{
				Test.AddInfo(FString::Printf(TEXT("Owning-client CharacterMovement reached server: %.1f cm; "
					"server/client position error %.1f cm. Actor/definition/ownership stable."), Travelled, Error));
				UNetDriver* ClientDriver = ClientKnight->GetWorld()->GetNetDriver();
				if (ClientDriver == nullptr || ClientDriver->ServerConnection == nullptr)
				{
					Fail(TEXT("Disconnect scenario has no actual client server connection to close."));
					return;
				}
				FScopedConditionalWorldSwitcher WorldSwitcher(ClientKnight->GetWorld());
				ClientController->ClientTravel(DisconnectMap, TRAVEL_Absolute);
				Test.AddInfo(TEXT("Owning client requested ClientTravel to M0 boot; "
					"engine travel tears down its real connection without invoking GameMode Logout from the test."));
				EnterPhase(EKnightNetworkPhase::WaitForTakeover);
			}
			else if (HasTimedOut(PhaseTimeout))
			{
				Fail(FString::Printf(TEXT("Client movement did not converge: authority distance %.1f cm, error %.1f cm."),
					Travelled, Error));
			}
		}

		void WaitForTakeover()
		{
			AKnightPartyState* Authority = GetParty(ServerWorld.Get());
			const FKnightRosterEntry* Entry = Authority != nullptr ? Authority->FindSlot(ReleasedSlot) : nullptr;
			UWorld* DetachedWorld = GetContextWorld(DisconnectedContext);
			if (Entry != nullptr && IsValid(Entry->Knight) && HaveConvergedClients(2, DisconnectedContext)
				&& Entry->OwnerKind == EKnightControllerOwner::Companion && DetachedWorld != nullptr
				&& DetachedWorld->GetNetMode() == NM_Standalone && DetachedWorld->GetNetDriver() == nullptr)
			{
				AKnightCompanionController* Companion = Cast<AKnightCompanionController>(Entry->Knight->GetController());
				if (!HasPreservedCanonicalActors() || Companion == nullptr || !Companion->IsFollowing()
					|| Entry->HumanId.IsValid())
				{
					Fail(TEXT("Actual network disconnect failed AI takeover, cleared ownership, or actor/identity preservation."));
					return;
				}
				CompanionStart = Entry->Knight->GetActorLocation();
				Test.AddInfo(FString::Printf(TEXT("Real disconnect reclaimed slot %d with a running native StateTree; "
					"all original server actors/IDs preserved and no fourth Knight."), ReleasedSlot));
				EnterPhase(EKnightNetworkPhase::ObserveCompanion);
			}
			else if (HasTimedOut(PhaseTimeout))
			{
				Fail(TEXT("Timed out waiting for actual client disconnect, standalone client world, and server AI takeover."));
			}
		}

		void ObserveCompanion()
		{
			const AKnightPartyState* Authority = GetParty(ServerWorld.Get());
			const FKnightRosterEntry* Entry = Authority != nullptr ? Authority->FindSlot(ReleasedSlot) : nullptr;
			if (Entry == nullptr || !HasPreservedCanonicalActors())
			{
				Fail(TEXT("AI cohesion changed or lost a canonical actor after disconnect."));
				return;
			}
			if (!HasTimedOut(CompanionObservationDuration))
			{
				return;
			}
			const double Travelled = FVector::Dist2D(CompanionStart, Entry->Knight->GetActorLocation());
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
			EnterPhase(EKnightNetworkPhase::WaitForReclaim);
		}

		void WaitForReclaim()
		{
			UWorld* ReconnectedWorld = GetContextWorld(DisconnectedContext);
			AKnightPlayerController* Client = ReconnectedWorld != nullptr
				? Cast<AKnightPlayerController>(ReconnectedWorld->GetFirstPlayerController()) : nullptr;
			const AKnightPlayerState* Human = Client != nullptr ? Client->GetPlayerState<AKnightPlayerState>() : nullptr;
			if (ReconnectedWorld != nullptr && ReconnectedWorld->GetNetMode() == NM_Client
				&& HaveConvergedClients(3) && Human != nullptr && Human->GetAssignedSlot() == ReleasedSlot
				&& Client->GetControlledKnight() != nullptr
				&& Client->GetControlledKnight()->GetKnightEntityId() == CanonicalIds[ReleasedSlot])
			{
				if (!HasPreservedCanonicalActors())
				{
					Fail(TEXT("Same-slot network reconnect replaced or duplicated a canonical server Knight."));
					return;
				}
				Test.AddInfo(TEXT("Real reconnect reclaimed the released slot; all three network worlds "
					"converge again with exactly three original canonical Knight identities."));
				BeginCleanup();
			}
			else if (HasTimedOut(PhaseTimeout))
			{
				Fail(TEXT("Timed out waiting for actual PIE ClientTravel reconnect and same-slot human reclaim."));
			}
		}

		void BeginCleanup()
		{
			if (Phase == EKnightNetworkPhase::Cleanup || Phase == EKnightNetworkPhase::Finished)
			{
				return;
			}
			EnterPhase(EKnightNetworkPhase::Cleanup);
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
			EnterPhase(EKnightNetworkPhase::Finished);
		}

		FAutomationTestBase& Test;
		EKnightNetworkPhase Phase = EKnightNetworkPhase::Start;
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
		TWeakObjectPtr<AKnightPlayerController> ClientController;
		TArray<TWeakObjectPtr<AKnightCharacter>> CanonicalActors;
		TArray<FKnightEntityId> CanonicalIds;
		int32 ReleasedSlot = INDEX_NONE;
		int32 OriginalRevision = 0;
		FVector MovementStart = FVector::ZeroVector;
		FVector CompanionStart = FVector::ZeroVector;
		FString ReconnectURL;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKnightPartyNetworkTest, "Knight.Party.NetworkOwnershipTakeoverReclaim",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FKnightPartyNetworkTest::RunTest(const FString& Parameters)
{
	FAutomationTestFramework::Get().EnqueueLatentCommand(
		MakeShared<KnightPartyNetworkTests::FKnightNetworkScenario>(*this));
	return true;
}

#endif
