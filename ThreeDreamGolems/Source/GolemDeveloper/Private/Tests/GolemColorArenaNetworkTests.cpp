#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Attributes/GolemAttributeSet.h"
#include "Character/GolemCharacter.h"
#include "ColorArena/GolemColorArenaGameMode.h"
#include "Components/GolemAbilitySystemComponent.h"
#include "Components/GolemCombatComponent.h"
#include "Framework/GolemGameState.h"
#include "Party/GolemPartyState.h"
#include "PartyArena/GolemArenaPlayerController.h"
#include "Projectiles/GolemColorProjectile.h"
#include "Types/GolemElement.h"
#include "Voids/GolemColorVoidComponent.h"
#include "Voids/GolemVoidHostComponent.h"
#include "Xenon/GolemXenon.h"
#include "Xenon/GolemXenonBrainComponent.h"

#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/PlatformTime.h"
#include "InputCoreTypes.h"
#include "InputKeyEventArgs.h"
#include "Misc/PackageName.h"
#include "PlayInEditorDataTypes.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "UObject/StrongObjectPtr.h"

DEFINE_LOG_CATEGORY_STATIC(LogGolemColorNetwork, Log, All);

namespace GolemColorArenaNetwork
{
	const TCHAR* MapPath = TEXT("/Game/Golem/Maps/Playables/L_M0_ColorArena");
	constexpr uint16 ServerPort = 17905;
	constexpr uint8 Earth = GolemElements::Mask(EGolemElement::Earth);
	constexpr uint8 Sky = GolemElements::Mask(EGolemElement::Sky);
	constexpr uint8 Sea = GolemElements::Mask(EGolemElement::Sea);

	enum class EPhase : uint8
	{
		Start,
		Ready,
		Settle,
		JupiterShot,
		MarsSwing,
		ReplicatedFill,
		NextVoid,
		SettleBehind,
		Signal,
		SignalSeen,
		NeptuneShot,
		ReplicatedSecondFill,
		Cleanup,
		Complete
	};

	/** One human player: the Golem on the host, and the controller in the world where that human sits. */
	struct FSeat
	{
		TWeakObjectPtr<AGolemCharacter> ServerGolem;
		TWeakObjectPtr<AGolemArenaPlayerController> LocalController;
		TWeakObjectPtr<UWorld> OwnerWorld;
		bool bKeyDown = false;
		double KeyPressedAt = 0.0;
	};

	/**
	 * Three humans on one listen server with 50 to 100 ms of latency: the host and two clients each
	 * drive one Golem with real input. Checks that a shot and a swing from different machines mix on
	 * the host's void, that every machine sees the same void state and Xenon progress, and that a
	 * signal from one player shows for the others. The test picks void colours and where the Golems
	 * stand; aiming, input, RPCs and replication are the game's own.
	 */
	class FScenario final : public IAutomationLatentCommand
	{
	public:
		explicit FScenario(FAutomationTestBase& InTest)
			: Test(InTest), RunStarted(FPlatformTime::Seconds()), PhaseStarted(RunStarted)
		{
		}

		virtual ~FScenario() override
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
				RestoreSettings();
			}
		}

		virtual bool Update() override
		{
			if (Phase == EPhase::Complete)
			{
				return true;
			}
			if (Phase != EPhase::Cleanup && (FPlatformTime::Seconds() - RunStarted > 240.0
				|| FPlatformTime::Seconds() - PhaseStarted > PhaseTimeout))
			{
				Fail(FString::Printf(TEXT("Colour network phase %d timed out."), static_cast<int32>(Phase)));
			}
			if (Phase != EPhase::Cleanup && Test.HasAnyErrors())
			{
				BeginCleanup();
			}
			if (Phase > EPhase::Ready && Phase < EPhase::Cleanup)
			{
				for (FSeat* Seat : { &Mars, &Jupiter, &Neptune })
				{
					if (Seat->bKeyDown && Seat->OwnerWorld.IsValid() && Seat->OwnerWorld->GetTimeSeconds() - Seat->KeyPressedAt > 0.08)
					{
						Release(*Seat);
					}
				}
			}
			switch (Phase)
			{
			case EPhase::Start:
				Start();
				break;
			case EPhase::Ready:
				Ready();
				break;
			case EPhase::Settle:
				Settle();
				break;
			case EPhase::JupiterShot:
				JupiterShot();
				break;
			case EPhase::MarsSwing:
				MarsSwing();
				break;
			case EPhase::ReplicatedFill:
				ReplicatedFill();
				break;
			case EPhase::NextVoid:
				NextVoid();
				break;
			case EPhase::SettleBehind:
				SettleBehind();
				break;
			case EPhase::Signal:
				Signal();
				break;
			case EPhase::SignalSeen:
				SignalSeen();
				break;
			case EPhase::NeptuneShot:
				NeptuneShot();
				break;
			case EPhase::ReplicatedSecondFill:
				ReplicatedSecondFill();
				break;
			case EPhase::Cleanup:
				Cleanup();
				break;
			default:
				break;
			}
			return Phase == EPhase::Complete;
		}

	private:
		void Enter(const EPhase Next, const double Timeout = 15.0)
		{
			Phase = Next;
			PhaseStarted = FPlatformTime::Seconds();
			PhaseServerStarted = Server.IsValid() ? Server->GetTimeSeconds() : 0.0;
			PhaseTimeout = Timeout;
			UE_LOG(LogGolemColorNetwork, Display, TEXT("Colour network phase %d started."), static_cast<int32>(Phase));
		}

		double ServerElapsed() const
		{
			return Server.IsValid() ? Server->GetTimeSeconds() - PhaseServerStarted : 0.0;
		}

		void Press(FSeat& Seat, const FKey& Key)
		{
			Release(Seat);
			Seat.bKeyDown = true;
			Seat.KeyPressedAt = Seat.OwnerWorld->GetTimeSeconds();
			HeldKey = Key;
			Seat.LocalController->InputKey(FInputKeyEventArgs::CreateSimulated(Key, IE_Pressed, 1.0f));
		}

		void Release(FSeat& Seat)
		{
			if (Seat.bKeyDown && Seat.LocalController.IsValid())
			{
				Seat.LocalController->InputKey(FInputKeyEventArgs::CreateSimulated(HeldKey, IE_Released, 0.0f));
			}
			Seat.bKeyDown = false;
		}

		/** Turns the seat's own camera until the centre of its screen is on Target. */
		void AimAt(FSeat& Seat, const FVector& Target)
		{
			if (!Seat.LocalController.IsValid())
			{
				return;
			}
			FVector ViewLocation;
			FRotator ViewRotation;
			Seat.LocalController->GetPlayerViewPoint(ViewLocation, ViewRotation);
			Seat.LocalController->SetControlRotation((Target - ViewLocation).Rotation());
		}

		void Place(const FSeat& Seat, const FVector& Location, const float Yaw)
		{
			AGolemCharacter* Golem = Seat.ServerGolem.Get();
			Golem->GetCharacterMovement()->StopMovementImmediately();
			Golem->SetActorLocationAndRotation(Location, FRotator(0.0, Yaw, 0.0), false, nullptr, ETeleportType::TeleportPhysics);
			Golem->ForceNetUpdate();
			if (Seat.LocalController.IsValid())
			{
				Seat.LocalController->SetControlRotation(FRotator(0.0, Yaw, 0.0));
			}
		}

		void Start()
		{
			if (GEditor == nullptr || GEngine == nullptr || GEditor->IsPlaySessionInProgress()
				|| !FPackageName::DoesPackageExist(MapPath))
			{
				Fail(TEXT("The colour network test requires an idle Editor and the M0 map (Bootstrap -Milestone 10)."));
				return;
			}
			const ULevelEditorPlaySettings* Defaults = GetDefault<ULevelEditorPlaySettings>();
			OriginalPositions = Defaults->MultipleInstancePositions;
			OriginalLastSize = Defaults->LastSize;
			OriginalPosition = Defaults->NewWindowPosition;
			OriginalWidth = Defaults->NewWindowWidth;
			OriginalHeight = Defaults->NewWindowHeight;
			Settings.Reset(DuplicateObject<ULevelEditorPlaySettings>(Defaults, GetTransientPackage()));
			Settings->SetPlayNetMode(PIE_ListenServer);
			Settings->SetPlayNumberOfClients(3);
			Settings->SetRunUnderOneProcess(true);
			Settings->SetServerPort(ServerPort);
			Settings->bLaunchSeparateServer = false;
			Settings->EnableGameSound = false;
			Settings->GameGetsMouseControl = false;
			Settings->SetClientWindowSize(FIntPoint(640, 360));
			Settings->NetworkEmulationSettings.bIsNetworkEmulationEnabled = true;
			Settings->NetworkEmulationSettings.EmulationTarget = NetworkEmulationTarget::Any;
			Settings->NetworkEmulationSettings.CurrentProfile = TEXT("Custom");
			Settings->NetworkEmulationSettings.OutPackets.MinLatency = 50;
			Settings->NetworkEmulationSettings.OutPackets.MaxLatency = 100;
			Settings->NetworkEmulationSettings.OutPackets.PacketLossPercentage = 0;
			Settings->NetworkEmulationSettings.InPackets.MinLatency = 0;
			Settings->NetworkEmulationSettings.InPackets.MaxLatency = 0;
			Settings->NetworkEmulationSettings.InPackets.PacketLossPercentage = 0;
			FRequestPlaySessionParams Request;
			Request.EditorPlaySettings = Settings.Get();
			Request.GlobalMapOverride = MapPath;
			Request.bAllowOnlineSubsystem = false;
			GEditor->RequestPlaySession(Request);
			bOwnsSession = true;
			Test.AddInfo(TEXT("Listen server with two clients requested, 50 to 100 ms outgoing latency."));
			Enter(EPhase::Ready, 60.0);
		}

		static AGolemPartyState* FindParty(const UWorld* World)
		{
			const AGolemGameState* State = World != nullptr ? World->GetGameState<AGolemGameState>() : nullptr;
			return State != nullptr ? State->GetPartyState() : nullptr;
		}

		/** The human who drives ServerGolem: the host's own controller, or the local controller of one client. */
		bool Seat(FSeat& OutSeat, AGolemCharacter* ServerGolem) const
		{
			OutSeat.ServerGolem = ServerGolem;
			if (ServerGolem->IsLocallyControlled())
			{
				OutSeat.OwnerWorld = Server;
				OutSeat.LocalController = Cast<AGolemArenaPlayerController>(ServerGolem->GetController());
				return OutSeat.LocalController.IsValid();
			}
			for (const TWeakObjectPtr<UWorld>& Client : Clients)
			{
				AGolemArenaPlayerController* Local = Client.IsValid()
					? Cast<AGolemArenaPlayerController>(Client->GetFirstPlayerController()) : nullptr;
				const AGolemCharacter* Pawn = Local != nullptr ? Local->GetControlledGolem() : nullptr;
				if (Pawn != nullptr && Pawn->GetGolemEntityId() == ServerGolem->GetGolemEntityId()
					&& Pawn->GetLocalRole() == ROLE_AutonomousProxy && Pawn->GetCombatComponent()->GetProfile() != nullptr)
				{
					OutSeat.OwnerWorld = Client;
					OutSeat.LocalController = Local;
					return true;
				}
			}
			return false;
		}

		static AGolemXenon* FindGrunt(UWorld* World)
		{
			for (TActorIterator<AGolemXenon> It(World); It; ++It)
			{
				if (!It->IsActorBeingDestroyed() && It->GetDefinitionId().PrimaryAssetName == FName(TEXT("Grunt")))
				{
					return *It;
				}
			}
			return nullptr;
		}

		/** The copy of the host's void on one client: same Xenon, same component name. */
		static UGolemColorVoidComponent* Replica(UWorld* ClientWorld, const UGolemColorVoidComponent* ServerVoid)
		{
			AGolemXenon* Grunt = FindGrunt(ClientWorld);
			if (Grunt == nullptr || ServerVoid == nullptr)
			{
				return nullptr;
			}
			TInlineComponentArray<UGolemColorVoidComponent*> Voids(Grunt);
			for (UGolemColorVoidComponent* Void : Voids)
			{
				if (Void->GetFName() == ServerVoid->GetFName())
				{
					return Void;
				}
			}
			return nullptr;
		}

		UGolemColorVoidComponent* OpenVoid() const
		{
			if (!Xenon.IsValid())
			{
				return nullptr;
			}
			const TArray<UGolemColorVoidComponent*> Open = Xenon->GetVoidHost()->GetOpenVoids();
			return Open.Num() == 1 ? Open[0] : nullptr;
		}

		void Ready()
		{
			Clients.Reset();
			for (const FWorldContext& Context : GEngine->GetWorldContexts())
			{
				if (Context.WorldType == EWorldType::PIE && Context.World() != nullptr)
				{
					if (Context.World()->GetNetMode() == NM_ListenServer)
					{
						Server = Context.World();
					}
					else if (Context.World()->GetNetMode() == NM_Client)
					{
						Clients.Add(Context.World());
					}
				}
			}
			AGolemPartyState* Party = FindParty(Server.Get());
			AGolemColorArenaGameMode* Mode = Server.IsValid() ? Server->GetAuthGameMode<AGolemColorArenaGameMode>() : nullptr;
			if (Party == nullptr || Mode == nullptr || Clients.Num() != 2 || !Party->IsCanonicalRosterReady()
				|| Party->GetHumanCount() != 3 || Mode->GetXenons().Num() != 2)
			{
				return;
			}
			if (!bXenonsReset)
			{
				// Xenons start hunting as soon as the map opens and have wandered while the clients connected.
				// This test is about colour over the network: fresh Xenons at their spawn points, standing still.
				if (!Mode->RespawnXenons().IsSuccess())
				{
					Fail(TEXT("The arena could not place fresh Xenons."));
					return;
				}
				for (AGolemXenon* Candidate : Mode->GetXenons())
				{
					if (Candidate != nullptr)
					{
						Candidate->GetBrain()->SetBrainEnabled(false);
					}
				}
				bXenonsReset = true;
			}
			Xenon = FindGrunt(Server.Get());
			for (const TWeakObjectPtr<UWorld>& Client : Clients)
			{
				// Wait until both clients have the fresh Grunt, not the one that was just removed.
				const AGolemXenon* Copy = FindGrunt(Client.Get());
				if (!Xenon.IsValid() || Copy == nullptr || !(Copy->GetXenonId() == Xenon->GetXenonId()))
				{
					return;
				}
			}
			AGolemCharacter* Found[3] = { nullptr, nullptr, nullptr };
			const FName Names[3] = { TEXT("Mars"), TEXT("Jupiter"), TEXT("Neptune") };
			for (const FGolemRosterEntry& Entry : Party->GetRoster())
			{
				for (int32 Index = 0; Index < 3; ++Index)
				{
					if (Entry.Golem != nullptr && Entry.HeroDefinitionId.PrimaryAssetName == Names[Index])
					{
						Found[Index] = Entry.Golem.Get();
					}
				}
			}
			if (!Xenon.IsValid() || Found[0] == nullptr || Found[1] == nullptr || Found[2] == nullptr
				|| !Seat(Mars, Found[0]) || !Seat(Jupiter, Found[1]) || !Seat(Neptune, Found[2])
				|| FindGrunt(Clients[0].Get()) == nullptr || FindGrunt(Clients[1].Get()) == nullptr)
			{
				return;
			}
#if DO_ENABLE_NET_TEST
			if (Server->GetNetDriver() == nullptr || Server->GetNetDriver()->PacketSimulationSettings.PktLagMin != 50
				|| Server->GetNetDriver()->PacketSimulationSettings.PktLagMax != 100)
			{
				Fail(TEXT("The listen server did not apply the requested 50 to 100 ms latency."));
				return;
			}
#else
			Test.AddWarning(TEXT("This build cannot emulate packet latency; the run used a local connection."));
#endif
			int32 RemoteSeats = 0;
			for (const FSeat* Each : { &Mars, &Jupiter, &Neptune })
			{
				RemoteSeats += Each->OwnerWorld != Server ? 1 : 0;
			}
			Test.TestEqual(TEXT("Two of the three Golems are driven from client machines"), RemoteSeats, 2);
			UGolemColorVoidComponent* Void = OpenVoid();
			if (Void == nullptr)
			{
				Fail(TEXT("The Grunt did not start with exactly one open void."));
				return;
			}
			// An orange void: Jupiter's player shoots first, Mars's player has to finish it with a swing.
			Void->ConfigureVoid(Earth | Sky, Xenon->GetVoidHost()->GetTuning().Void, false);
			ActiveVoid = Void;
			const FVector VoidLocation = Void->GetComponentLocation();
			const FVector Outward = (VoidLocation - Xenon->GetActorLocation()).GetSafeNormal2D();
			const FVector Stand = VoidLocation + Outward * (Void->GetScaledSphereRadius() + 60.0f);
			const float FaceYaw = static_cast<float>((-Outward).Rotation().Yaw);
			const FVector Right = FVector::CrossProduct(FVector::UpVector, Outward);
			Place(Mars, FVector(Stand.X, Stand.Y, 100.0), FaceYaw);
			Place(Jupiter, FVector(Stand.X, Stand.Y, 100.0) + Right * 260.0 + Outward * 500.0, FaceYaw);
			Place(Neptune, FVector(Stand.X, Stand.Y, 100.0) - Right * 260.0 + Outward * 500.0, FaceYaw);
			Enter(EPhase::Settle, 10.0);
		}

		void Settle()
		{
			AimAt(Jupiter, ActiveVoid->GetComponentLocation());
			// Clients need the teleport correction and the new void colour before anyone acts.
			bool bReplicated = true;
			for (const TWeakObjectPtr<UWorld>& Client : Clients)
			{
				const UGolemColorVoidComponent* Copy = Replica(Client.Get(), ActiveVoid.Get());
				bReplicated &= Copy != nullptr && Copy->GetVoidState() == EGolemVoidState::Open
					&& Copy->GetRequiredMask() == (Earth | Sky);
			}
			if (ServerElapsed() < 1.5 || !bReplicated)
			{
				return;
			}
			Test.TestTrue(TEXT("Both clients see the void and the colours it needs"), bReplicated);
			Press(Jupiter, EKeys::RightMouseButton);
			Enter(EPhase::JupiterShot, 8.0);
		}

		/** Records what the host sees of Seat's heavy attack while it is in progress. */
		void ObserveShot(const FSeat& Seat)
		{
			bSawSwing |= Seat.ServerGolem->GetCombatComponent()->GetSnapshot().AttackPhase == 2;
			for (TActorIterator<AGolemColorProjectile> It(Server.Get()); It; ++It)
			{
				if (!bSawProjectile)
				{
					bSawProjectile = true;
					ProjectileFrom = It->GetActorLocation();
					ProjectileDirection = It->GetActorForwardVector();
				}
			}
			FVector ViewLocation;
			FRotator ViewRotation;
			if (const APlayerController* HostSide = Cast<APlayerController>(Seat.ServerGolem->GetController()))
			{
				// How far the host's idea of this player's camera is from the void, in degrees.
				HostSide->GetPlayerViewPoint(ViewLocation, ViewRotation);
				const FVector ToVoid = (ActiveVoid->GetComponentLocation() - ViewLocation).GetSafeNormal();
				HostAimErrorDegrees = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(
					static_cast<float>(FVector::DotProduct(ToVoid, ViewRotation.Vector())), -1.0f, 1.0f)));
			}
		}

		FString DescribeShot(const FSeat& Seat) const
		{
			const AGolemCharacter* OwnerSide = Seat.LocalController.IsValid() ? Seat.LocalController->GetControlledGolem() : nullptr;
			return FString::Printf(TEXT("host-owned %d, Golem on host at %s and on its owner's machine at %s, void at %s, swing seen %d, projectile seen %d from %s towards %s, host aim error %.1f deg"),
				Seat.OwnerWorld == Server, *Seat.ServerGolem->GetActorLocation().ToCompactString(),
				OwnerSide != nullptr ? *OwnerSide->GetActorLocation().ToCompactString() : TEXT("-"),
				*ActiveVoid->GetComponentLocation().ToCompactString(), bSawSwing, bSawProjectile,
				*ProjectileFrom.ToCompactString(), *ProjectileDirection.ToCompactString(), HostAimErrorDegrees);
		}

		void JupiterShot()
		{
			AimAt(Jupiter, ActiveVoid->GetComponentLocation());
			ObserveShot(Jupiter);
			if (ActiveVoid->GetVoidState() != EGolemVoidState::Primed)
			{
				if (ServerElapsed() > 5.0)
				{
					Fail(FString::Printf(TEXT("Jupiter's shot did not reach the void (state %d, Xenon health %.1f; %s)."),
						static_cast<int32>(ActiveVoid->GetVoidState()), Xenon->GetGolemAttributeSet()->GetHealth(),
						*DescribeShot(Jupiter)));
				}
				return;
			}
			Test.TestEqual(TEXT("Jupiter's player placed Sky with a real shot"), ActiveVoid->GetAppliedMask(), Sky);
			Test.AddInfo(FString::Printf(TEXT("Jupiter's shot: %s."), *DescribeShot(Jupiter)));
			bSawSwing = false;
			bSawProjectile = false;
			PrimedAt = Server->GetTimeSeconds();
			Press(Mars, EKeys::RightMouseButton);
			Enter(EPhase::MarsSwing, 8.0);
		}

		void MarsSwing()
		{
			// While the window runs, every client has to show the same Primed void and the same deadline.
			if (ActiveVoid->GetVoidState() == EGolemVoidState::Primed)
			{
				for (int32 Index = 0; Index < Clients.Num(); ++Index)
				{
					const UGolemColorVoidComponent* Copy = Replica(Clients[Index].Get(), ActiveVoid.Get());
					if (Copy != nullptr && Copy->GetVoidState() == EGolemVoidState::Primed && Copy->GetAppliedMask() == Sky
						&& FMath::IsNearlyEqual(Copy->GetTimerEndServerTime(), ActiveVoid->GetTimerEndServerTime(), 0.001))
					{
						ClientsSawPrimed |= 1 << Index;
					}
				}
			}
			if (Xenon->GetVoidHost()->GetFilledCount() == 1)
			{
				Test.TestTrue(TEXT("Mars's player finished the mix inside the 4 s window"), Server->GetTimeSeconds() - PrimedAt < 4.0);
				Test.TestEqual(TEXT("Both clients saw the Primed void with the host's deadline"), ClientsSawPrimed, 3);
				Enter(EPhase::ReplicatedFill, 6.0);
			}
			else if (Server->GetTimeSeconds() - PrimedAt > 4.3)
			{
				Fail(FString::Printf(TEXT("Mars's swing did not finish the void in time (state %d)."),
					static_cast<int32>(ActiveVoid->GetVoidState())));
			}
		}

		bool ClientsShowProgress(const int32 Filled, const float Health) const
		{
			for (const TWeakObjectPtr<UWorld>& Client : Clients)
			{
				const AGolemXenon* Copy = FindGrunt(Client.Get());
				if (Copy == nullptr || Copy->GetVoidHost()->GetFilledCount() != Filled
					|| !FMath::IsNearlyEqual(Copy->GetGolemAttributeSet()->GetHealth(), Health, 0.01f))
				{
					return false;
				}
			}
			return true;
		}

		void ReplicatedFill()
		{
			if (!ClientsShowProgress(1, 200.0f))
			{
				return;
			}
			Test.TestEqual(TEXT("One fill of three removes a third of the Grunt's health on the host"),
				Xenon->GetGolemAttributeSet()->GetHealth(), 200.0f);
			Test.AddInfo(TEXT("Both clients show 1 of 3 voids filled and 200 health."));
			Enter(EPhase::NextVoid, 10.0);
		}

		void NextVoid()
		{
			UGolemColorVoidComponent* Void = OpenVoid();
			if (Void == nullptr || Xenon->GetVoidHost()->IsStaggered())
			{
				return;
			}
			// A blue void on the other side: Neptune's player signals it, then fills it.
			Void->ConfigureVoid(Sea, Xenon->GetVoidHost()->GetTuning().Void, false);
			ActiveVoid = Void;
			const FVector VoidLocation = Void->GetComponentLocation();
			const FVector Outward = (VoidLocation - Xenon->GetActorLocation()).GetSafeNormal2D();
			const FVector Spot = VoidLocation + Outward * (Void->GetScaledSphereRadius() + 550.0f);
			Place(Neptune, FVector(Spot.X, Spot.Y, 100.0), static_cast<float>((-Outward).Rotation().Yaw));
			Enter(EPhase::SettleBehind, 10.0);
		}

		void SettleBehind()
		{
			AimAt(Neptune, ActiveVoid->GetComponentLocation());
			bool bReplicated = true;
			for (const TWeakObjectPtr<UWorld>& Client : Clients)
			{
				const UGolemColorVoidComponent* Copy = Replica(Client.Get(), ActiveVoid.Get());
				bReplicated &= Copy != nullptr && Copy->GetVoidState() == EGolemVoidState::Open && Copy->GetRequiredMask() == Sea;
			}
			if (ServerElapsed() >= 1.5 && bReplicated)
			{
				Enter(EPhase::Signal, 8.0);
			}
		}

		void Signal()
		{
			AimAt(Neptune, ActiveVoid->GetComponentLocation());
			if (ActiveVoid->IsSignaled())
			{
				Test.AddInfo(TEXT("Neptune's player signalled the void through the signal button's own aim and RPC."));
				Enter(EPhase::SignalSeen, 6.0);
				return;
			}
			// The button is pressed again until the aim has settled on the void; a miss is a normal outcome.
			if (Server->GetTimeSeconds() - LastSignalAt > 0.5)
			{
				LastSignalAt = Server->GetTimeSeconds();
				Neptune.LocalController->RequestSignal();
			}
		}

		void SignalSeen()
		{
			AimAt(Neptune, ActiveVoid->GetComponentLocation());
			// Everyone has to see the marker, on the host and on both clients, with the same end time.
			for (const TWeakObjectPtr<UWorld>& Client : Clients)
			{
				const UGolemColorVoidComponent* Copy = Replica(Client.Get(), ActiveVoid.Get());
				if (Copy == nullptr || !Copy->IsSignaled()
					|| !FMath::IsNearlyEqual(Copy->GetSignalEndServerTime(), ActiveVoid->GetSignalEndServerTime(), 0.001)
					|| Copy->GetMissingMask() != Sea)
				{
					return;
				}
			}
			Test.TestTrue(TEXT("The marker lasts six seconds from the host's clock"), FMath::IsNearlyEqual(
				ActiveVoid->GetSignalEndServerTime() - ActiveVoid->GetSignalStartServerTime(), 6.0, 0.001));
			Press(Neptune, EKeys::RightMouseButton);
			ShotAt = Server->GetTimeSeconds();
			Enter(EPhase::NeptuneShot, 8.0);
		}

		void NeptuneShot()
		{
			AimAt(Neptune, ActiveVoid->GetComponentLocation());
			ObserveShot(Neptune);
			if (Xenon->GetVoidHost()->GetFilledCount() == 2)
			{
				Test.AddInfo(FString::Printf(TEXT("Neptune's shot: %s."), *DescribeShot(Neptune)));
				Enter(EPhase::ReplicatedSecondFill, 6.0);
			}
			else if (Server->GetTimeSeconds() - ShotAt > 5.0)
			{
				Fail(FString::Printf(TEXT("Neptune's shot did not fill the signalled void (state %d, Xenon health %.1f; %s)."),
					static_cast<int32>(ActiveVoid->GetVoidState()), Xenon->GetGolemAttributeSet()->GetHealth(),
					*DescribeShot(Neptune)));
			}
		}

		void ReplicatedSecondFill()
		{
			if (!ClientsShowProgress(2, 100.0f))
			{
				return;
			}
			Test.AddInfo(TEXT("Both clients show 2 of 3 voids filled and 100 health."));
			BeginCleanup();
		}

		void Fail(const FString& Message)
		{
			if (Phase == EPhase::Cleanup || Phase == EPhase::Complete)
			{
				return;
			}
			for (const FSeat* Each : { &Mars, &Jupiter, &Neptune })
			{
				if (Each->ServerGolem.IsValid())
				{
					Test.AddInfo(FString::Printf(TEXT("%s at %s, host-owned %d, attacking %d"), *Each->ServerGolem->GetName(),
						*Each->ServerGolem->GetActorLocation().ToCompactString(), Each->OwnerWorld == Server,
						Each->ServerGolem->GetCombatComponent()->GetSnapshot().bAttacking));
				}
			}
			if (ActiveVoid.IsValid())
			{
				Test.AddInfo(FString::Printf(TEXT("Void at %s state %d applied %d required %d signalled %d"),
					*ActiveVoid->GetComponentLocation().ToCompactString(), static_cast<int32>(ActiveVoid->GetVoidState()),
					ActiveVoid->GetAppliedMask(), ActiveVoid->GetRequiredMask(), ActiveVoid->IsSignaled()));
			}
			Test.AddError(Message);
			BeginCleanup();
		}

		void BeginCleanup()
		{
			if (Phase == EPhase::Cleanup || Phase == EPhase::Complete)
			{
				return;
			}
			for (FSeat* Each : { &Mars, &Jupiter, &Neptune })
			{
				Release(*Each);
			}
			Enter(EPhase::Cleanup, 20.0);
			if (bOwnsSession && GEditor != nullptr)
			{
				if (GEditor->IsPlayingSessionInEditor())
				{
					GEditor->RequestEndPlayMap();
				}
				else if (GEditor->IsPlaySessionRequestQueued())
				{
					GEditor->CancelRequestPlaySession();
				}
			}
		}

		void RestoreSettings()
		{
			if (Settings.IsValid())
			{
				ULevelEditorPlaySettings* Defaults = GetMutableDefault<ULevelEditorPlaySettings>();
				Defaults->MultipleInstancePositions = OriginalPositions;
				Defaults->LastSize = OriginalLastSize;
				Defaults->NewWindowPosition = OriginalPosition;
				Defaults->NewWindowWidth = OriginalWidth;
				Defaults->NewWindowHeight = OriginalHeight;
				Defaults->SaveConfig();
			}
		}

		void Cleanup()
		{
			if (bOwnsSession && GEditor != nullptr && GEditor->IsPlaySessionInProgress())
			{
				if (FPlatformTime::Seconds() - PhaseStarted < 15.0)
				{
					return;
				}
				Test.AddError(TEXT("Colour network PIE cleanup exceeded its bound."));
				if (GEditor->IsPlayingSessionInEditor())
				{
					GEditor->EndPlayMap();
				}
			}
			RestoreSettings();
			Settings.Reset();
			bOwnsSession = false;
			Enter(EPhase::Complete);
		}

		FAutomationTestBase& Test;
		EPhase Phase = EPhase::Start;
		double RunStarted = 0.0;
		double PhaseStarted = 0.0;
		double PhaseServerStarted = 0.0;
		double PhaseTimeout = 30.0;
		double PrimedAt = 0.0;
		double ShotAt = 0.0;
		double LastSignalAt = -10.0;
		int32 ClientsSawPrimed = 0;
		bool bXenonsReset = false;
		bool bSawSwing = false;
		bool bSawProjectile = false;
		float HostAimErrorDegrees = -1.0f;
		FVector ProjectileFrom = FVector::ZeroVector;
		FVector ProjectileDirection = FVector::ZeroVector;
		bool bOwnsSession = false;
		FKey HeldKey;
		FSeat Mars;
		FSeat Jupiter;
		FSeat Neptune;
		TWeakObjectPtr<UWorld> Server;
		TArray<TWeakObjectPtr<UWorld>> Clients;
		TWeakObjectPtr<AGolemXenon> Xenon;
		TWeakObjectPtr<UGolemColorVoidComponent> ActiveVoid;
		TStrongObjectPtr<ULevelEditorPlaySettings> Settings;
		TArray<FIntPoint> OriginalPositions;
		FIntPoint OriginalLastSize = FIntPoint::ZeroValue;
		FIntPoint OriginalPosition = FIntPoint::ZeroValue;
		int32 OriginalWidth = 0;
		int32 OriginalHeight = 0;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGolemColorArenaNetworkTest, "Golem.Color.Arena.ListenServerThreePlayers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGolemColorArenaNetworkTest::RunTest(const FString& Parameters)
{
	AddCommand(new GolemColorArenaNetwork::FScenario(*this));
	return true;
}

#endif
