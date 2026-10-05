#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "AIController.h"
#include "Attributes/KnightAttributeSet.h"
#include "Character/KnightCharacter.h"
#include "Commands/KnightRPGCommand.h"
#include "Components/KnightAbilitySystemComponent.h"
#include "Components/KnightLifeComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/KnightGameState.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayAbilitySpec.h"
#include "GameplayEffectTypes.h"
#include "HAL/PlatformTime.h"
#include "IPAddress.h"
#include "Inventory/KnightInventoryComponent.h"
#include "Loot/KnightItemPickup.h"
#include "Misc/PackageName.h"
#include "Party/KnightPartyState.h"
#include "PartyArena/KnightArenaEnemy.h"
#include "PartyArena/KnightPartyArenaStartMarker.h"
#include "PartyArena/KnightPartyArenaState.h"
#include "PlayInEditorDataTypes.h"
#include "Player/KnightPlayerState.h"
#include "Progression/KnightRPGComponent.h"
#include "RPGLoop/KnightRPGCharacter.h"
#include "RPGLoop/KnightRPGGameState.h"
#include "RPGLoop/KnightRPGLoopGameMode.h"
#include "RPGLoop/KnightRPGPlayerController.h"
#include "RPGLoop/KnightRPGStateActor.h"
#include "RPGLoop/KnightRPGUpgradeStation.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Tags/KnightGameplayTags.h"
#include "UnrealEngine.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectGlobals.h"

namespace KnightRPGNetwork
{
	const TCHAR* RPGMap = TEXT("/Game/Knight/Maps/Playables/L_M05_RPGLoop");
	const TCHAR* DetachedMap = TEXT("/Game/Knight/Maps/Playables/L_M00_Boot");
	constexpr double StartupTimeout = 60.0;
	constexpr double PhaseTimeout = 25.0;
	constexpr uint16 ServerPort = 17905;

	enum class EKnightRPGNetworkPhase : uint8
	{
		Start, Ready, Reward, RacePickup, ReplayPickup, ChangedReplay, ForeignKnight, StaleRevision,
		OutOfRange, CollectRewards, UpgradeReady, RaceEquip, Unequip, Rune, InvalidSkill, CostFailure,
		Purchase, Respec, Disconnect, Rejoin, SoloSwitch, SoloReplay, SoloReturn, Cleanup, Complete
	};

	struct FKnightRPGReplyCapture
	{
		TMap<FGuid, FKnightRPGCommandResult> Replies;
		TMap<FGuid, TWeakObjectPtr<AKnightRPGPlayerController>> Recipients;
		TMap<FGuid, TWeakObjectPtr<AKnightRPGPlayerController>> ExpectedRecipients;
	};

	struct FKnightRPGAtomicCapture
	{
		FKnightInventoryView Inventory;
		FKnightRPGView RPG;
		TArray<FKnightSourceProjection> Projections;
		TArray<TArray<FGameplayAbilitySpecHandle>> Specs;
		TArray<TArray<FActiveGameplayEffectHandle>> Effects;
		TArray<int32> Grants;
		TArray<float> AttackPower;
		TArray<float> Defense;
		TArray<float> MaxHealth;
		TArray<float> MaxStamina;
		TArray<float> MaxRuneEnergy;
	};

	AKnightPartyState* FindParty(UWorld* World)
	{
		AKnightGameState* State = World != nullptr ? World->GetGameState<AKnightGameState>() : nullptr;
		return State != nullptr ? State->GetPartyState() : nullptr;
	}

	AKnightRPGStateActor* FindRPG(UWorld* World)
	{
		AKnightRPGGameState* State = World != nullptr ? World->GetGameState<AKnightRPGGameState>() : nullptr;
		return State != nullptr ? State->GetRPGState() : nullptr;
	}

	TArray<FGameplayAbilitySpecHandle> GetSpecs(const UKnightAbilitySystemComponent* ASC)
	{
		TArray<FGameplayAbilitySpecHandle> Result;
		for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
		{
			Result.Add(Spec.Handle);
		}
		return Result;
	}

	bool HasSameProjection(const FKnightSourceProjection& Left, const FKnightSourceProjection& Right)
	{
		return Left.EntityId == Right.EntityId && Left.Revision == Right.Revision && Left.Plan == Right.Plan;
	}

	const FKnightRPGKnightView* FindKnightView(const FKnightRPGView& View, const FKnightEntityId& Id)
	{
		return View.Knights.FindByPredicate([&Id](const FKnightRPGKnightView& Knight)
			{
				return Knight.KnightId == Id;
			});
	}

	/** Explicit encounter damage/positions isolate transaction faults; reward callbacks, RPCs and replication are real. */
	class FKnightRPGNetworkCommand final : public IAutomationLatentCommand
	{
	public:
		FKnightRPGNetworkCommand(FAutomationTestBase& InTest, bool bInNetwork)
			: Test(InTest), bNetwork(bInNetwork), Capture(MakeShared<FKnightRPGReplyCapture>())
		{
		}

		virtual ~FKnightRPGNetworkCommand() override
		{
			UnbindReplies();
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
				RestoreWindows();
			}
		}

		virtual bool Update() override
		{
			if (Phase < EKnightRPGNetworkPhase::Cleanup && Test.HasAnyErrors())
			{
				BeginCleanup();
			}
			if (Phase > EKnightRPGNetworkPhase::Ready && Phase < EKnightRPGNetworkPhase::Cleanup
				&& !HasCanonicalIdentities())
			{
				Fail(TEXT("An RPG transaction/ownership change replaced a canonical Knight, ID, hero or ASC owner/avatar."));
			}
			if (Phase > EKnightRPGNetworkPhase::Start && Phase < EKnightRPGNetworkPhase::Cleanup
				&& Elapsed() > (Phase == EKnightRPGNetworkPhase::Ready ? StartupTimeout : PhaseTimeout))
			{
				Fail(GetDiagnostics(TEXT("Actual RPG network phase timed out")));
			}
			switch (Phase)
			{
			case EKnightRPGNetworkPhase::Start: Start(); break;
			case EKnightRPGNetworkPhase::Ready: WaitReady(); break;
			case EKnightRPGNetworkPhase::Reward: WaitReward(); break;
			case EKnightRPGNetworkPhase::RacePickup: WaitPickupRace(); break;
			case EKnightRPGNetworkPhase::ReplayPickup: WaitPickupReplay(); break;
			case EKnightRPGNetworkPhase::ChangedReplay: WaitChangedReplay(); break;
			case EKnightRPGNetworkPhase::ForeignKnight: WaitForeignKnight(); break;
			case EKnightRPGNetworkPhase::StaleRevision: WaitStaleRevision(); break;
			case EKnightRPGNetworkPhase::OutOfRange: WaitOutOfRange(); break;
			case EKnightRPGNetworkPhase::CollectRewards: WaitCollectRewards(); break;
			case EKnightRPGNetworkPhase::UpgradeReady: WaitUpgradeReady(); break;
			case EKnightRPGNetworkPhase::RaceEquip: WaitEquipRace(); break;
			case EKnightRPGNetworkPhase::Unequip: WaitUnequip(); break;
			case EKnightRPGNetworkPhase::Rune: WaitRune(); break;
			case EKnightRPGNetworkPhase::InvalidSkill: WaitInvalidSkill(); break;
			case EKnightRPGNetworkPhase::CostFailure: WaitCostFailure(); break;
			case EKnightRPGNetworkPhase::Purchase: WaitPurchase(); break;
			case EKnightRPGNetworkPhase::Respec: WaitRespec(); break;
			case EKnightRPGNetworkPhase::Disconnect: WaitDisconnect(); break;
			case EKnightRPGNetworkPhase::Rejoin: WaitRejoin(); break;
			case EKnightRPGNetworkPhase::SoloSwitch: WaitSoloSwitch(); break;
			case EKnightRPGNetworkPhase::SoloReplay: WaitSoloReplay(); break;
			case EKnightRPGNetworkPhase::SoloReturn: WaitSoloReturn(); break;
			case EKnightRPGNetworkPhase::Cleanup: Cleanup(); break;
			default: break;
			}
			return Phase == EKnightRPGNetworkPhase::Complete;
		}

	private:
		void Enter(EKnightRPGNetworkPhase InPhase)
		{
			Phase = InPhase;
			PhaseStarted = FPlatformTime::Seconds();
		}

		double Elapsed() const
		{
			return FPlatformTime::Seconds() - PhaseStarted;
		}

		void Fail(const FString& Message)
		{
			Test.AddError(Message);
			BeginCleanup();
		}

		UWorld* ContextWorld(FName Handle) const
		{
			const FWorldContext* Context = GEngine != nullptr ? GEngine->GetWorldContextFromHandle(Handle) : nullptr;
			return Context != nullptr ? Context->World() : nullptr;
		}

		AKnightRPGPlayerController* FindOwner(UWorld* World) const
		{
			AKnightPartyState* Party = FindParty(World);
			if (Party == nullptr)
			{
				return nullptr;
			}
			for (FConstPlayerControllerIterator Iterator = World->GetPlayerControllerIterator(); Iterator; ++Iterator)
			{
				AKnightRPGPlayerController* Player = Cast<AKnightRPGPlayerController>(Iterator->Get());
				AKnightPlayerState* Human = Player != nullptr ? Player->GetPlayerState<AKnightPlayerState>() : nullptr;
				AKnightCharacter* Pawn = Player != nullptr ? Player->GetControlledKnight() : nullptr;
				const FKnightRosterEntry* Entry = Human != nullptr ? Party->FindSlot(Human->GetAssignedSlot()) : nullptr;
				if (Player != nullptr && Player->IsLocalController() && Pawn != nullptr && Pawn->IsLocallyControlled()
					&& Entry != nullptr && Entry->Knight == Pawn && Entry->HumanPlayerState == Human
					&& Entry->KnightId == Pawn->GetKnightEntityId() && Pawn->GetKnightAbilitySystem()->GetAvatarActor() == Pawn
					&& (!bNetwork || World->GetNetMode() != NM_Client || Pawn->GetLocalRole() == ROLE_AutonomousProxy))
				{
					return Player;
				}
			}
			return nullptr;
		}

		void Start()
		{
			if (GEditor == nullptr || GEngine == nullptr || GEditor->IsPlaySessionInProgress()
				|| !FPackageName::DoesPackageExist(RPGMap) || !FPackageName::DoesPackageExist(DetachedMap))
			{
				Fail(TEXT("RPG gate requires an idle Editor and genuine M0/M5 bootstrap maps."));
				return;
			}
			const ULevelEditorPlaySettings* Defaults = GetDefault<ULevelEditorPlaySettings>();
			OriginalPositions = Defaults->MultipleInstancePositions;
			OriginalLastSize = Defaults->LastSize;
			OriginalWindowPosition = Defaults->NewWindowPosition;
			OriginalWidth = Defaults->NewWindowWidth;
			OriginalHeight = Defaults->NewWindowHeight;
			Settings.Reset(DuplicateObject<ULevelEditorPlaySettings>(Defaults, GetTransientPackage()));
			Settings->SetPlayNetMode(bNetwork ? PIE_ListenServer : PIE_Standalone);
			Settings->SetPlayNumberOfClients(bNetwork ? 3 : 1);
			Settings->SetRunUnderOneProcess(true);
			Settings->SetServerPort(ServerPort);
			Settings->bLaunchSeparateServer = false;
			Settings->EnableGameSound = false;
			Settings->GameGetsMouseControl = false;
			Settings->SetClientWindowSize(FIntPoint(640, 360));
			Settings->NetworkEmulationSettings.bIsNetworkEmulationEnabled = bNetwork;
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
			Request.GlobalMapOverride = RPGMap;
			Request.bAllowOnlineSubsystem = false;
			GEditor->RequestPlaySession(Request);
			bOwnsSession = true;
			Test.AddInfo(bNetwork ? TEXT("Actual M5 listen server/two clients requested with 50-100ms packet latency.")
				: TEXT("Actual M5 standalone requested for persistent Knight switch/receipt correlation."));
			Enter(EKnightRPGNetworkPhase::Ready);
		}

		void BindReply(AKnightRPGPlayerController* Player)
		{
			const TWeakPtr<FKnightRPGReplyCapture> WeakCapture(Capture);
			const TWeakObjectPtr<AKnightRPGPlayerController> WeakPlayer(Player);
			const FDelegateHandle Handle = Player->OnRPGCommandResult.AddLambda(
				[WeakCapture, WeakPlayer](const FKnightRPGCommandResult& Outcome)
				{
					if (TSharedPtr<FKnightRPGReplyCapture> Current = WeakCapture.Pin())
					{
						Current->Replies.Add(Outcome.RequestId, Outcome);
						Current->Recipients.Add(Outcome.RequestId, WeakPlayer);
					}
				});
			ObservedPlayers.Add(Player);
			ReplyHandles.Add(Handle);
		}

		void WaitReady()
		{
			ClientContexts.Reset();
			for (const FWorldContext& Context : GEngine->GetWorldContexts())
			{
				UWorld* World = Context.World();
				if (Context.WorldType == EWorldType::PIE && World != nullptr)
				{
					if (World->GetNetMode() == (bNetwork ? NM_ListenServer : NM_Standalone))
					{
						Server = World;
					}
					else if (bNetwork && World->GetNetMode() == NM_Client)
					{
						ClientContexts.Add(Context.ContextHandle);
					}
				}
			}
			AKnightPartyState* Party = FindParty(Server.Get());
			AKnightRPGLoopGameMode* Mode = Server.IsValid() ? Server->GetAuthGameMode<AKnightRPGLoopGameMode>() : nullptr;
			if (Party == nullptr || !Party->IsCanonicalRosterReady() || Party->GetHumanCount() != (bNetwork ? 3 : 1)
				|| Mode == nullptr || !Mode->GetRPGInitializationResult().IsSuccess() || Mode->GetRPGState() == nullptr
				|| !Mode->GetRPGState()->GetRPG()->IsInitialized() || ClientContexts.Num() != (bNetwork ? 2 : 0))
			{
				return;
			}
			Host = FindOwner(Server.Get());
			if (!Host.IsValid())
			{
				return;
			}
			Players.Reset();
			if (bNetwork)
			{
				for (FName Handle : ClientContexts)
				{
					AKnightRPGPlayerController* Player = FindOwner(ContextWorld(Handle));
					if (Player == nullptr)
					{
						return;
					}
					Players.Add(Player);
				}
#if DO_ENABLE_NET_TEST
				if (Server->GetNetDriver() == nullptr || Server->GetNetDriver()->PacketSimulationSettings.PktLagMin != 50
					|| Server->GetNetDriver()->PacketSimulationSettings.PktLagMax != 100)
				{
					Fail(TEXT("Actual RPG NetDriver did not apply 50-100ms outgoing packet latency."));
					return;
				}
#else
				Fail(TEXT("This build cannot verify actual network packet latency."));
				return;
#endif
			}
			else
			{
				Players.Add(Host);
			}
			RPGState = Mode->GetRPGState();
			for (int32 Slot = 0; Slot < 3; ++Slot)
			{
				AKnightCharacter* Knight = Party->FindSlot(Slot)->Knight;
				Knights.Add(Knight);
				Ids.Add(Knight->GetKnightEntityId());
				Heroes.Add(Knight->GetHeroDefinitionId());
			}
			if (!HasClientSnapshots())
			{
				Knights.Reset();
				Ids.Reset();
				Heroes.Reset();
				return;
			}
			for (const TWeakObjectPtr<AKnightRPGPlayerController>& Player : Players)
			{
				BindReply(Player.Get());
			}
			AKnightPartyArenaStartMarker* Marker = nullptr;
			for (TActorIterator<AKnightPartyArenaStartMarker> Iterator(Server.Get()); Iterator; ++Iterator)
			{
				Marker = *Iterator;
				break;
			}
			if (Marker == nullptr)
			{
				Fail(TEXT("M5 actual arena start marker is missing."));
				return;
			}
			Place(Host->GetControlledKnight(), Marker->GetActorLocation() + FVector(0.0, -150.0, 100.0));
			FScopedConditionalWorldSwitcher Switch(Server.Get());
			Host->RequestInteract();
			Test.AddInfo(TEXT("Transaction fixture uses explicit server GAS damage against actual encounter pawns; real death/Victory reward callbacks remain required. This is not normal combat playthrough evidence."));
			Enter(EKnightRPGNetworkPhase::Reward);
		}

		bool HasCanonicalIdentities() const
		{
			AKnightPartyState* Party = FindParty(Server.Get());
			if (Party == nullptr || Knights.Num() != 3)
			{
				return false;
			}
			int32 Count = 0;
			for (TActorIterator<AKnightCharacter> Iterator(Server.Get()); Iterator; ++Iterator)
			{
				++Count;
			}
			if (Count != 3)
			{
				return false;
			}
			for (int32 Slot = 0; Slot < 3; ++Slot)
			{
				const FKnightRosterEntry* Entry = Party->FindSlot(Slot);
				if (!Knights[Slot].IsValid() || Entry == nullptr || Entry->Knight != Knights[Slot].Get()
					|| Entry->KnightId != Ids[Slot] || Entry->HeroDefinitionId != Heroes[Slot]
					|| Knights[Slot]->GetKnightAbilitySystem()->GetOwnerActor() != Knights[Slot].Get()
					|| Knights[Slot]->GetKnightAbilitySystem()->GetAvatarActor() != Knights[Slot].Get())
				{
					return false;
				}
			}
			return true;
		}

		bool HasKnightEquipmentVisuals(UWorld* World, const bool bAssert = false) const
		{
			AKnightPartyState* Party = FindParty(World);
			AKnightRPGStateActor* State = FindRPG(World);
			if (Party == nullptr || !Party->IsCanonicalRosterReady() || State == nullptr)
			{
				return false;
			}
			bool bMatches = true;
			for (int32 Slot = 0; Slot < 3; ++Slot)
			{
				const FKnightRosterEntry* Entry = Party->FindSlot(Slot);
				const AKnightRPGCharacter* Knight = Entry != nullptr
					? Cast<AKnightRPGCharacter>(Entry->Knight.Get()) : nullptr;
				const FKnightRPGKnightView View = Entry != nullptr
					? State->GetRPG()->GetKnightView(Entry->KnightId) : FKnightRPGKnightView();
				if (Knight == nullptr || !View.IsInitialized() || View.KnightId != Knight->GetKnightEntityId()
					|| Knight->GetWeaponUpgradeVisual() == nullptr || Knight->GetRuneVisual() == nullptr)
				{
					return false;
				}
				const bool bExpectedWeapon = View.FindAssignment(KnightGameplayTags::Slot_Equipment_Weapon) != nullptr;
				const bool bExpectedRune = View.FindAssignment(KnightGameplayTags::Slot_Equipment_Rune) != nullptr;
				const bool bWeaponVisible = Knight->GetWeaponUpgradeVisual()->IsVisible();
				const bool bRuneVisible = Knight->GetRuneVisual()->IsVisible();
				bMatches &= bWeaponVisible == bExpectedWeapon && bRuneVisible == bExpectedRune;
				if (bAssert)
				{
					Test.TestEqual(FString::Printf(TEXT("%s slot %d weapon visual matches committed Knight assignment"),
						*World->GetName(), Slot), bWeaponVisible, bExpectedWeapon);
					Test.TestEqual(FString::Printf(TEXT("%s slot %d rune visual matches committed Knight assignment"),
						*World->GetName(), Slot), bRuneVisible, bExpectedRune);
				}
			}
			return bMatches;
		}

		bool HasClientSnapshots() const
		{
			if (!RPGState.IsValid() || Knights.Num() != 3 || Ids.Num() != 3 || Heroes.Num() != 3
				|| !HasKnightEquipmentVisuals(Server.Get()))
			{
				return false;
			}
			for (FName Handle : ClientContexts)
			{
				UWorld* World = ContextWorld(Handle);
				AKnightPartyState* Party = FindParty(World);
				AKnightRPGStateActor* State = FindRPG(World);
				if (World == nullptr || World->GetNetMode() != NM_Client || Party == nullptr
					|| !Party->IsCanonicalRosterReady() || State == nullptr
					|| !(State->GetInventory()->GetView() == RPGState->GetInventory()->GetView())
					|| !(State->GetRPG()->GetView() == RPGState->GetRPG()->GetView())
					|| !HasKnightEquipmentVisuals(World))
				{
					return false;
				}
				int32 Count = 0;
				for (TActorIterator<AKnightCharacter> Iterator(World); Iterator; ++Iterator)
				{
					++Count;
				}
				if (Count != 3)
				{
					return false;
				}
				for (TActorIterator<AAIController> Iterator(World); Iterator; ++Iterator)
				{
					return false;
				}
				for (int32 Slot = 0; Slot < 3; ++Slot)
				{
					const FKnightRosterEntry* Entry = Party->FindSlot(Slot);
					UKnightAbilitySystemComponent* ASC = Entry != nullptr && Entry->Knight != nullptr
						? Entry->Knight->GetKnightAbilitySystem() : nullptr;
					UKnightAbilitySystemComponent* Authority = Knights[Slot]->GetKnightAbilitySystem();
					if (ASC == nullptr || Entry->KnightId != Ids[Slot] || Entry->HeroDefinitionId != Heroes[Slot]
						|| ASC->GetOwnerActor() != Entry->Knight || ASC->GetAvatarActor() != Entry->Knight
						|| ASC->GetOwnedGrantCount() != Authority->GetOwnedGrantCount()
						|| !HasSameProjection(ASC->GetSourceProjection(KnightRPG::ProjectionSource),
							Authority->GetSourceProjection(KnightRPG::ProjectionSource))
						|| !FMath::IsNearlyEqual(Entry->Knight->GetKnightAttributeSet()->GetAttackPower(),
							Knights[Slot]->GetKnightAttributeSet()->GetAttackPower()))
					{
						return false;
					}
				}
				for (const TWeakObjectPtr<AKnightItemPickup>& Authority : Pickups)
				{
					if (!Authority.IsValid())
					{
						return false;
					}
					AKnightItemPickup* Replica = nullptr;
					for (TActorIterator<AKnightItemPickup> Iterator(World); Iterator; ++Iterator)
					{
						if (Iterator->GetPickupId() == Authority->GetPickupId())
						{
							Replica = *Iterator;
							break;
						}
					}
					const UTextRenderComponent* Caption = Replica != nullptr
						? Replica->FindComponentByClass<UTextRenderComponent>() : nullptr;
					const UTextRenderComponent* SourceCaption = Authority->FindComponentByClass<UTextRenderComponent>();
					if (Replica == nullptr || !(Replica->GetView().Item == Authority->GetView().Item)
						|| Replica->IsClaimed() != Authority->IsClaimed()
						|| Replica->GetView().ClaimRequestId != Authority->GetView().ClaimRequestId
						|| Caption == nullptr || SourceCaption == nullptr || Caption->Text.IsEmpty()
						|| !Caption->Text.EqualTo(SourceCaption->Text) || Caption->IsVisible() != !Replica->IsClaimed())
					{
						return false;
					}
				}
			}
			return true;
		}

		void Place(AKnightCharacter* Knight, const FVector& Location)
		{
			FScopedConditionalWorldSwitcher Switch(Server.Get());
			Knight->GetCharacterMovement()->StopMovementImmediately();
			Knight->SetActorLocationAndRotation(Location, FRotator::ZeroRotator, false, nullptr, ETeleportType::TeleportPhysics);
			Knight->ForceNetUpdate();
		}

		AKnightCharacter* ServerKnight(AKnightRPGPlayerController* Player) const
		{
			const FKnightEntityId Id = Player->GetControlledKnight()->GetKnightEntityId();
			for (const TWeakObjectPtr<AKnightCharacter>& Knight : Knights)
			{
				if (Knight->GetKnightEntityId() == Id)
				{
					return Knight.Get();
				}
			}
			return nullptr;
		}

		FKnightRPGCommand MakeCommand(AKnightRPGPlayerController* Player, EKnightRPGOperation Operation) const
		{
			FKnightRPGCommand Command;
			Command.RequestId = FGuid::NewGuid();
			Command.Operation = Operation;
			Command.ExpectedKnightId = Player->GetControlledKnight()->GetKnightEntityId();
			Command.InventoryId = RPGState->GetInventory()->GetView().InventoryId;
			Command.ExpectedInventoryRevision = RPGState->GetInventory()->GetView().Revision;
			Command.ExpectedKnightRevision = RPGState->GetRPG()->GetKnightView(Command.ExpectedKnightId).Revision;
			return Command;
		}

		void Send(AKnightRPGPlayerController* Player, const FKnightRPGCommand& Command)
		{
			Capture->Replies.Remove(Command.RequestId);
			Capture->Recipients.Remove(Command.RequestId);
			Capture->ExpectedRecipients.Add(Command.RequestId, Player);
			FScopedConditionalWorldSwitcher Switch(Player->GetWorld());
			Player->RequestRPGCommand(Command);
		}

		bool HasOwnedReply(const FKnightRPGCommand& Command) const
		{
			const TWeakObjectPtr<AKnightRPGPlayerController>* Actual = Capture->Recipients.Find(Command.RequestId);
			const TWeakObjectPtr<AKnightRPGPlayerController>* Expected = Capture->ExpectedRecipients.Find(Command.RequestId);
			const FKnightRPGCommandResult* Reply = Capture->Replies.Find(Command.RequestId);
			return Actual != nullptr && Expected != nullptr && Actual->IsValid() && *Actual == *Expected
				&& (*Actual)->IsLocalController() && (!bNetwork || (*Actual)->GetWorld()->GetNetMode() == NM_Client)
				&& Reply != nullptr && Reply->KnightId == Command.ExpectedKnightId
				&& Reply->InventoryId == Command.InventoryId && Reply->Operation == Command.Operation;
		}

		FKnightRPGAtomicCapture CaptureAtomic() const
		{
			FKnightRPGAtomicCapture Result;
			Result.Inventory = RPGState->GetInventory()->GetView();
			Result.RPG = RPGState->GetRPG()->GetView();
			for (const TWeakObjectPtr<AKnightCharacter>& Knight : Knights)
			{
				UKnightAbilitySystemComponent* ASC = Knight->GetKnightAbilitySystem();
				Result.Projections.Add(ASC->GetSourceProjection(KnightRPG::ProjectionSource));
				Result.Specs.Add(GetSpecs(ASC));
				Result.Effects.Add(ASC->GetActiveEffects(FGameplayEffectQuery()));
				Result.Grants.Add(ASC->GetOwnedGrantCount());
				Result.AttackPower.Add(Knight->GetKnightAttributeSet()->GetAttackPower());
				Result.Defense.Add(Knight->GetKnightAttributeSet()->GetDefense());
				Result.MaxHealth.Add(Knight->GetKnightAttributeSet()->GetMaxHealth());
				Result.MaxStamina.Add(Knight->GetKnightAttributeSet()->GetMaxStamina());
				Result.MaxRuneEnergy.Add(Knight->GetKnightAttributeSet()->GetMaxRuneEnergy());
			}
			return Result;
		}

		bool IsAtomicUnchanged(const FKnightRPGAtomicCapture& Before) const
		{
			const FKnightRPGAtomicCapture After = CaptureAtomic();
			if (!(Before.Inventory == After.Inventory) || !(Before.RPG == After.RPG)
				|| Before.Specs != After.Specs || Before.Effects != After.Effects || Before.Grants != After.Grants
				|| Before.AttackPower != After.AttackPower || Before.Defense != After.Defense
				|| Before.MaxHealth != After.MaxHealth || Before.MaxStamina != After.MaxStamina
				|| Before.MaxRuneEnergy != After.MaxRuneEnergy)
			{
				return false;
			}
			for (int32 Slot = 0; Slot < 3; ++Slot)
			{
				if (!HasSameProjection(Before.Projections[Slot], After.Projections[Slot]))
				{
					return false;
				}
			}
			return true;
		}

		bool WaitRejection(EKnightError Error)
		{
			const FKnightRPGCommandResult* Reply = Capture->Replies.Find(Pending.RequestId);
			if (Reply == nullptr)
			{
				return false;
			}
			if (!HasOwnedReply(Pending) || Reply->Result.Error != Error || !IsAtomicUnchanged(AtomicBefore))
			{
				Fail(FString::Printf(TEXT("Rejected RPG operation %d expected error %d, actual %d; atomic unchanged=%d. %s"),
					static_cast<int32>(Pending.Operation), static_cast<int32>(Error), static_cast<int32>(Reply->Result.Error),
					IsAtomicUnchanged(AtomicBefore), *Reply->Result.Message.ToString()));
				return false;
			}
			return HasClientSnapshots();
		}

		bool WaitSuccess()
		{
			const FKnightRPGCommandResult* Reply = Capture->Replies.Find(Pending.RequestId);
			if (Reply == nullptr)
			{
				return false;
			}
			if (!HasOwnedReply(Pending) || !Reply->Result.IsSuccess() || Reply->KnightId != Pending.ExpectedKnightId
				|| Reply->InventoryId != Pending.InventoryId || Reply->Operation != Pending.Operation)
			{
				Fail(FString::Printf(TEXT("Actual RPG RPC failed or miscorrelated op=%d error=%d source=%s receipt=%s: %s"),
					static_cast<int32>(Pending.Operation), static_cast<int32>(Reply->Result.Error),
					*Pending.ExpectedKnightId.Value.ToString(), *Reply->KnightId.Value.ToString(),
					*Reply->Result.Message.ToString()));
				return false;
			}
			return HasClientSnapshots();
		}

		void WaitReward()
		{
			AKnightRPGLoopGameMode* Mode = Server->GetAuthGameMode<AKnightRPGLoopGameMode>();
			if (Mode->GetArenaState()->GetSnapshot().Stage == EKnightArenaStage::Defeat)
			{
				Fail(TEXT("Encounter fault setup unexpectedly caused Defeat before genuine loot reward."));
				return;
			}
			FScopedConditionalWorldSwitcher Switch(Server.Get());
			for (AKnightArenaEnemy* Enemy : Mode->GetEncounterEnemies())
			{
				if (Enemy->GetLifeComponent()->IsAlive())
				{
					const FKnightResult Result = UKnightAbilitySystemComponent::ApplyDamage(Host->GetControlledKnight(), Enemy,
						Enemy->GetKnightAttributeSet()->GetMaxHealth() + Enemy->GetKnightAttributeSet()->GetDefense() + 1000.0f);
					if (!Result.IsSuccess())
					{
						Fail(TEXT("Validated real GAS encounter damage setup failed."));
						return;
					}
				}
			}
			if (RPGState->GetLoopSnapshot().Stage != EKnightRPGLoopStage::RewardAvailable
				|| !RPGState->GetLoopSnapshot().bRewardIssued || !HasClientSnapshots())
			{
				return;
			}
			Pickups.Reset();
			for (TActorIterator<AKnightItemPickup> Iterator(Server.Get()); Iterator; ++Iterator)
			{
				Pickups.Add(*Iterator);
				if (Iterator->GetView().Item.DefinitionId == FPrimaryAssetId(TEXT("KnightItem"), TEXT("HammerUpgrade")))
				{
					RacedPickup = *Iterator;
				}
			}
			if (!RacedPickup.IsValid() || Pickups.Num() < 3
				|| Pickups.Num() != RPGState->GetLoopSnapshot().RewardPickupCount)
			{
				Fail(TEXT("Actual deterministic reward must author equipment, rune and material pickups exactly once."));
				return;
			}
			for (int32 Index = 0; Index < Players.Num(); ++Index)
			{
				Place(ServerKnight(Players[Index].Get()), RacedPickup->GetActorLocation() + FVector(0.0, Index == 0 ? -90.0 : 90.0, 100.0));
			}
			RaceCommands.Reset();
			AtomicBefore = CaptureAtomic();
			for (const TWeakObjectPtr<AKnightRPGPlayerController>& Player : Players)
			{
				FKnightRPGCommand Command = MakeCommand(Player.Get(), EKnightRPGOperation::Pickup);
				Command.PickupId = RacedPickup->GetPickupId();
				RaceCommands.Add(Command);
				Send(Player.Get(), Command);
			}
			Enter(EKnightRPGNetworkPhase::RacePickup);
		}

		void WaitPickupRace()
		{
			int32 Successes = 0;
			for (int32 Index = 0; Index < RaceCommands.Num(); ++Index)
			{
				const FKnightRPGCommandResult* Reply = Capture->Replies.Find(RaceCommands[Index].RequestId);
				if (Reply == nullptr)
				{
					return;
				}
				if (!HasOwnedReply(RaceCommands[Index]))
				{
					Fail(TEXT("Pickup race Client RPC was not delivered to its actual requesting owning controller."));
					return;
				}
				if (Reply->Result.IsSuccess())
				{
					++Successes;
					WinnerIndex = Index;
					WinningPickupCommand = RaceCommands[Index];
					WinningPickupReceipt = *Reply;
				}
				else if (Reply->Result.Error != EKnightError::InvalidRevision && Reply->Result.Error != EKnightError::Conflict)
				{
					Fail(TEXT("Losing pickup race did not report the actual stale revision/claimed-item conflict."));
					return;
				}
			}
			const FKnightInventoryView Inventory = RPGState->GetInventory()->GetView();
			if (Successes != 1 || Inventory.Revision != AtomicBefore.Inventory.Revision + 1
				|| Inventory.Items.Num() != AtomicBefore.Inventory.Items.Num() + 1 || !RacedPickup->IsClaimed()
				|| RacedPickup->GetView().ClaimRequestId != WinningPickupCommand.RequestId)
			{
				Fail(TEXT("Same-pickup/same-revision RPC race did not commit exactly one genuine instance and claim."));
				return;
			}
			if (!HasClientSnapshots())
			{
				return;
			}
			Winner = Players[WinnerIndex];
			OldSlot = Winner->GetPlayerState<AKnightPlayerState>()->GetAssignedSlot();
			EquipmentId = WinningPickupReceipt.AffectedInstanceId;
			Test.AddInfo(FString::Printf(TEXT("Real pickup race committed one request=%s instance=%s invrev=%lld sourceKnight=%s."),
				*WinningPickupCommand.RequestId.ToString(), *EquipmentId.ToString(), Inventory.Revision,
				*WinningPickupReceipt.KnightId.Value.ToString()));
			AtomicBefore = CaptureAtomic();
			Pending = WinningPickupCommand;
			Send(Winner.Get(), Pending);
			Enter(EKnightRPGNetworkPhase::ReplayPickup);
		}

		void WaitPickupReplay()
		{
			if (!WaitSuccess())
			{
				return;
			}
			const FKnightRPGCommandResult& Reply = Capture->Replies.FindChecked(Pending.RequestId);
			if (!Reply.bWasReplay || Reply.CommittedInventoryRevision != WinningPickupReceipt.CommittedInventoryRevision
				|| Reply.AffectedInstanceId != EquipmentId || !IsAtomicUnchanged(AtomicBefore))
			{
				Fail(TEXT("Exact winning pickup replay altered state or replaced committed receipt identity/revision."));
				return;
			}
			Pending = WinningPickupCommand;
			++Pending.ExpectedInventoryRevision;
			AtomicBefore = CaptureAtomic();
			Send(Winner.Get(), Pending);
			Enter(EKnightRPGNetworkPhase::ChangedReplay);
		}

		void WaitChangedReplay()
		{
			if (!WaitRejection(EKnightError::Conflict))
			{
				return;
			}
			AKnightRPGUpgradeStation* Station = Server->GetAuthGameMode<AKnightRPGLoopGameMode>()->GetUpgradeStation();
			if (Station == nullptr)
			{
				Fail(TEXT("Actual upgrade station is missing for the ownership/revision fixture."));
				return;
			}
			Place(ServerKnight(Winner.Get()), Station->GetActorLocation() + FVector(0.0, -150.0, 100.0));
			Pending = MakeCommand(Winner.Get(), EKnightRPGOperation::Equip);
			Pending.ExpectedKnightId = Ids[(OldSlot + 1) % 3];
			Pending.ExpectedKnightRevision = RPGState->GetRPG()->GetKnightView(Pending.ExpectedKnightId).Revision;
			Pending.ItemInstanceId = EquipmentId;
			Pending.SlotTag = KnightGameplayTags::Slot_Equipment_Weapon;
			AtomicBefore = CaptureAtomic();
			Send(Winner.Get(), Pending);
			Enter(EKnightRPGNetworkPhase::ForeignKnight);
		}

		void WaitForeignKnight()
		{
			if (!WaitRejection(EKnightError::NotOwned))
			{
				return;
			}
			Pending = MakeCommand(Winner.Get(), EKnightRPGOperation::Equip);
			--Pending.ExpectedInventoryRevision;
			Pending.ItemInstanceId = EquipmentId;
			Pending.SlotTag = KnightGameplayTags::Slot_Equipment_Weapon;
			AtomicBefore = CaptureAtomic();
			Send(Winner.Get(), Pending);
			Enter(EKnightRPGNetworkPhase::StaleRevision);
		}

		void WaitStaleRevision()
		{
			if (!WaitRejection(EKnightError::InvalidRevision))
			{
				return;
			}
			for (const TWeakObjectPtr<AKnightItemPickup>& Pickup : Pickups)
			{
				if (!Pickup->IsClaimed())
				{
					RangePickup = Pickup;
					break;
				}
			}
			if (!RangePickup.IsValid())
			{
				Fail(TEXT("Actual unclaimed pickup required for authoritative range rejection."));
				return;
			}
			Place(ServerKnight(Winner.Get()), RangePickup->GetActorLocation() + FVector(900.0, 0.0, 100.0));
			Pending = MakeCommand(Winner.Get(), EKnightRPGOperation::Pickup);
			Pending.PickupId = RangePickup->GetPickupId();
			AtomicBefore = CaptureAtomic();
			Send(Winner.Get(), Pending);
			Enter(EKnightRPGNetworkPhase::OutOfRange);
		}

		void WaitOutOfRange()
		{
			if (!WaitRejection(EKnightError::OutOfRange))
			{
				return;
			}
			if (RangePickup->IsClaimed())
			{
				Fail(TEXT("Rejected out-of-range RPC still claimed the world pickup."));
				return;
			}
			Pending = FKnightRPGCommand();
			Enter(EKnightRPGNetworkPhase::CollectRewards);
		}

		void WaitCollectRewards()
		{
			if (Pending.RequestId.IsValid())
			{
				if (!WaitSuccess())
				{
					return;
				}
				Pending = FKnightRPGCommand();
			}
			for (const TWeakObjectPtr<AKnightItemPickup>& Pickup : Pickups)
			{
				if (!Pickup->IsClaimed())
				{
					Place(ServerKnight(Winner.Get()), Pickup->GetActorLocation() + FVector(0.0, -100.0, 100.0));
					Pending = MakeCommand(Winner.Get(), EKnightRPGOperation::Pickup);
					Pending.PickupId = Pickup->GetPickupId();
					Send(Winner.Get(), Pending);
					return;
				}
			}
			const FKnightInventoryView Inventory = RPGState->GetInventory()->GetView();
			for (const FKnightItemInstance& Item : Inventory.Items)
			{
				if (Item.DefinitionId == FPrimaryAssetId(TEXT("KnightItem"), TEXT("RuneUpgrade")))
				{
					RuneId = Item.InstanceId;
				}
			}
			AKnightRPGLoopGameMode* Mode = Server->GetAuthGameMode<AKnightRPGLoopGameMode>();
			AKnightRPGUpgradeStation* Station = Mode->GetUpgradeStation();
			if (!RuneId.IsValid() || Station == nullptr)
			{
				Fail(TEXT("Actual reward rune or upgrade station is missing."));
				return;
			}
			for (int32 Index = 0; Index < Players.Num(); ++Index)
			{
				Place(ServerKnight(Players[Index].Get()), Station->GetActorLocation() + FVector(Index * 150.0, -100.0, 100.0));
			}
			Enter(EKnightRPGNetworkPhase::UpgradeReady);
		}

		void WaitUpgradeReady()
		{
			AKnightRPGLoopGameMode* Mode = Server->GetAuthGameMode<AKnightRPGLoopGameMode>();
			if (!HasClientSnapshots())
			{
				return;
			}
			for (const TWeakObjectPtr<AKnightRPGPlayerController>& Player : Players)
			{
				if (!Mode->IsInUpgradeSafeZone(ServerKnight(Player.Get())))
				{
					Fail(TEXT("Explicit transaction fixture is outside the actual upgrade safe zone."));
					return;
				}
			}
			RaceCommands.Reset();
			AtomicBefore = CaptureAtomic();
			for (const TWeakObjectPtr<AKnightRPGPlayerController>& Player : Players)
			{
				FKnightRPGCommand Command = MakeCommand(Player.Get(), EKnightRPGOperation::Equip);
				Command.ItemInstanceId = EquipmentId;
				Command.SlotTag = KnightGameplayTags::Slot_Equipment_Weapon;
				RaceCommands.Add(Command);
				Send(Player.Get(), Command);
			}
			Enter(EKnightRPGNetworkPhase::RaceEquip);
		}

		void WaitEquipRace()
		{
			int32 Successes = 0;
			for (int32 Index = 0; Index < RaceCommands.Num(); ++Index)
			{
				const FKnightRPGCommandResult* Reply = Capture->Replies.Find(RaceCommands[Index].RequestId);
				if (Reply == nullptr)
				{
					return;
				}
				if (!HasOwnedReply(RaceCommands[Index]))
				{
					Fail(TEXT("Equipment race Client RPC was not delivered to its actual requesting owning controller."));
					return;
				}
				if (Reply->Result.IsSuccess())
				{
					++Successes;
					Winner = Players[Index];
					WinnerIndex = Index;
					OldSlot = Winner->GetPlayerState<AKnightPlayerState>()->GetAssignedSlot();
				}
				else if (Reply->Result.Error != EKnightError::InvalidRevision && Reply->Result.Error != EKnightError::Conflict)
				{
					Fail(TEXT("Losing equipment race did not report actual stale revision/exclusive-item conflict."));
					return;
				}
			}
			const FKnightRPGView View = RPGState->GetRPG()->GetView();
			int32 Assignments = 0;
			for (const FKnightRPGKnightView& Knight : View.Knights)
			{
				const FKnightEquipmentAssignment* Assignment = Knight.FindAssignment(KnightGameplayTags::Slot_Equipment_Weapon);
				if (Assignment != nullptr && Assignment->ItemInstanceId == EquipmentId)
				{
					++Assignments;
				}
			}
			if (Successes != 1 || Assignments != 1
				|| ServerKnight(Winner.Get())->GetKnightAttributeSet()->GetAttackPower() <= AtomicBefore.AttackPower[OldSlot])
			{
				Fail(TEXT("Same-instance equipment race failed exclusive per-Knight assignment or real GAS attack-power bonus."));
				return;
			}
			for (int32 Slot = 0; Slot < 3; ++Slot)
			{
				const FKnightRPGKnightView* Previous = FindKnightView(AtomicBefore.RPG, Ids[Slot]);
				const FKnightRPGKnightView* Current = FindKnightView(View, Ids[Slot]);
				if (Slot != OldSlot && (!HasSameProjection(AtomicBefore.Projections[Slot],
					Knights[Slot]->GetKnightAbilitySystem()->GetSourceProjection(KnightRPG::ProjectionSource))
					|| Previous == nullptr || Current == nullptr || !(*Previous == *Current)))
				{
					Fail(TEXT("An equipment race leaked projection or RPG mutation into another Knight."));
					return;
				}
			}
			if (!HasClientSnapshots())
			{
				return;
			}
			Test.AddInfo(TEXT("Both-client same-instance equip race committed one exclusive assignment with real GAS stat change; other Knight projections remained exact."));
			Pending = MakeCommand(Winner.Get(), EKnightRPGOperation::Unequip);
			Pending.SlotTag = KnightGameplayTags::Slot_Equipment_Weapon;
			Send(Winner.Get(), Pending);
			Enter(EKnightRPGNetworkPhase::Unequip);
		}

		void WaitUnequip()
		{
			if (!WaitSuccess())
			{
				return;
			}
			if (RPGState->GetRPG()->GetKnightView(Ids[OldSlot]).FindAssignment(KnightGameplayTags::Slot_Equipment_Weapon) != nullptr
				|| !FMath::IsNearlyEqual(Knights[OldSlot]->GetKnightAttributeSet()->GetAttackPower(), AtomicBefore.AttackPower[OldSlot]))
			{
				Fail(TEXT("Real unequip did not revoke the exact equipment projection and restore attack power."));
				return;
			}
			Pending = MakeCommand(Winner.Get(), EKnightRPGOperation::Rune);
			Pending.SlotTag = KnightGameplayTags::Slot_Equipment_Rune;
			Pending.ItemInstanceId = RuneId;
			Send(Winner.Get(), Pending);
			Enter(EKnightRPGNetworkPhase::Rune);
		}

		void WaitRune()
		{
			if (!WaitSuccess())
			{
				return;
			}
			const FKnightRPGKnightView View = RPGState->GetRPG()->GetKnightView(Ids[OldSlot]);
			const FKnightEquipmentAssignment* Assignment = View.FindAssignment(KnightGameplayTags::Slot_Equipment_Rune);
			const FKnightActionModifier Modifier = Knights[OldSlot]->GetKnightAbilitySystem()->GetActionModifier(KnightGameplayTags::Ability_Combat_Rune);
			if (Assignment == nullptr || Assignment->ItemInstanceId != RuneId
				|| !FMath::IsNearlyEqual(Modifier.DamageScale, View.RuneTuning.DamageScale)
				|| !FMath::IsNearlyEqual(Modifier.CostScale, View.RuneTuning.CostScale)
				|| !FMath::IsNearlyEqual(Modifier.DurationScale, View.RuneTuning.DurationScale)
				|| View.RuneTuning == FKnightRuneTuning())
			{
				Fail(TEXT("Rune transaction did not tune the real shared-GAS action modifier for the correct Knight."));
				return;
			}
			Pending = MakeCommand(Winner.Get(), EKnightRPGOperation::PurchaseSkill);
			Pending.SkillDefinitionId = FPrimaryAssetId(TEXT("KnightSkill"), TEXT("MissingNetworkFixture"));
			AtomicBefore = CaptureAtomic();
			Send(Winner.Get(), Pending);
			Enter(EKnightRPGNetworkPhase::InvalidSkill);
		}

		void WaitInvalidSkill()
		{
			if (!WaitRejection(EKnightError::NotFound))
			{
				return;
			}
			Pending = MakeCommand(Winner.Get(), EKnightRPGOperation::PurchaseSkill);
			Pending.SkillDefinitionId = FPrimaryAssetId(TEXT("KnightSkill"), TEXT("ImpossibleCost"));
			AtomicBefore = CaptureAtomic();
			Send(Winner.Get(), Pending);
			Enter(EKnightRPGNetworkPhase::CostFailure);
		}

		void WaitCostFailure()
		{
			if (!WaitRejection(EKnightError::InsufficientResources))
			{
				return;
			}
			Pending = MakeCommand(Winner.Get(), EKnightRPGOperation::PurchaseSkill);
			Pending.SkillDefinitionId = FPrimaryAssetId(TEXT("KnightSkill"), TEXT("HammerForce"));
			BeforePurchase = CaptureAtomic();
			Send(Winner.Get(), Pending);
			Enter(EKnightRPGNetworkPhase::Purchase);
		}

		void WaitPurchase()
		{
			if (!WaitSuccess())
			{
				return;
			}
			const FKnightRPGKnightView View = RPGState->GetRPG()->GetKnightView(Ids[OldSlot]);
			const FKnightRPGKnightView* Previous = FindKnightView(BeforePurchase.RPG, Ids[OldSlot]);
			if (Previous == nullptr || !View.HasSkill(Pending.SkillDefinitionId) || View.SkillPoints >= Previous->SkillPoints
				|| RPGState->GetInventory()->GetView() == BeforePurchase.Inventory
				|| Knights[OldSlot]->GetKnightAttributeSet()->GetAttackPower() <= BeforePurchase.AttackPower[OldSlot])
			{
				Fail(TEXT("Actual skill purchase did not atomically spend points/materials and apply its real GAS bonus."));
				return;
			}
			Pending = MakeCommand(Winner.Get(), EKnightRPGOperation::Respec);
			Send(Winner.Get(), Pending);
			Enter(EKnightRPGNetworkPhase::Respec);
		}

		void WaitRespec()
		{
			if (!WaitSuccess())
			{
				return;
			}
			const FKnightRPGKnightView View = RPGState->GetRPG()->GetKnightView(Ids[OldSlot]);
			const FKnightRPGKnightView* Previous = FindKnightView(BeforePurchase.RPG, Ids[OldSlot]);
			if (Previous == nullptr || !View.PurchasedSkills.IsEmpty() || View.SkillPoints != Previous->SkillPoints
				|| RPGState->GetInventory()->GetView().Items != BeforePurchase.Inventory.Items
				|| !FMath::IsNearlyEqual(Knights[OldSlot]->GetKnightAttributeSet()->GetAttackPower(), BeforePurchase.AttackPower[OldSlot])
				|| GetSpecs(Knights[OldSlot]->GetKnightAbilitySystem()) != BeforePurchase.Specs[OldSlot]
				|| Knights[OldSlot]->GetKnightAbilitySystem()->GetOwnedGrantCount() != BeforePurchase.Grants[OldSlot])
			{
				Fail(TEXT("Atomic respec did not refund exact paid identities/points and revoke the skill-owned GAS projection."));
				return;
			}
			Test.AddInfo(TEXT("Invalid definition/cost rejected without partial changes; real purchase/respec restored material instance IDs, points, specs and grants."));
			AtomicBefore = CaptureAtomic();
			if (bNetwork)
			{
				const TSharedPtr<const FInternetAddr> Address = Server->GetNetDriver()->GetLocalAddr();
				if (!Address.IsValid() || Address->GetPort() <= 0)
				{
					Fail(TEXT("Actual listening socket unavailable for RPG JIP."));
					return;
				}
				ReconnectURL = FString::Printf(TEXT("127.0.0.1:%d?KnightSlot=%d"), Address->GetPort(), OldSlot);
				DepartingContext = ClientContexts[WinnerIndex];
				FScopedConditionalWorldSwitcher Switch(Winner->GetWorld());
				Winner->ClientTravel(DetachedMap, TRAVEL_Absolute);
				Enter(EKnightRPGNetworkPhase::Disconnect);
			}
			else
			{
				NewSlot = (OldSlot + 1) % 3;
				FScopedConditionalWorldSwitcher Switch(Server.Get());
				Winner->RequestSwitchKnight(NewSlot);
				Enter(EKnightRPGNetworkPhase::SoloSwitch);
			}
		}

		void WaitDisconnect()
		{
			UWorld* Detached = ContextWorld(DepartingContext);
			AKnightPartyState* Party = FindParty(Server.Get());
			const FKnightRosterEntry* Entry = Party->FindSlot(OldSlot);
			if (Detached == nullptr || Detached->GetNetMode() != NM_Standalone || Detached->GetNetDriver() != nullptr
				|| Detached->GetFirstPlayerController() == nullptr || Entry->OwnerKind != EKnightControllerOwner::Companion
				|| Cast<AAIController>(Knights[OldSlot]->GetController()) == nullptr || Party->GetHumanCount() != 2)
			{
				return;
			}
			if (!IsAtomicUnchanged(AtomicBefore))
			{
				Fail(TEXT("Disconnect/AI takeover changed persistent RPG state, IDs, specs, grants or stat projection."));
				return;
			}
			FScopedConditionalWorldSwitcher Switch(Detached);
			Detached->GetFirstPlayerController()->ClientTravel(ReconnectURL, TRAVEL_Absolute);
			Enter(EKnightRPGNetworkPhase::Rejoin);
		}

		void WaitRejoin()
		{
			AKnightRPGPlayerController* Reclaimed = FindOwner(ContextWorld(DepartingContext));
			if (Reclaimed == nullptr || Reclaimed->GetWorld()->GetNetMode() != NM_Client
				|| Reclaimed->GetPlayerState<AKnightPlayerState>()->GetAssignedSlot() != OldSlot
				|| FindParty(Server.Get())->GetHumanCount() != 3 || !HasClientSnapshots())
			{
				return;
			}
			if (!IsAtomicUnchanged(AtomicBefore)
				|| GetSpecs(Reclaimed->GetControlledKnight()->GetKnightAbilitySystem()) != AtomicBefore.Specs[OldSlot])
			{
				Fail(TEXT("Real JIP/reclaim changed persistent three-Knight views/projections/grants or owning-client specs."));
				return;
			}
			for (FName Handle : ClientContexts)
			{
				if (!HasKnightEquipmentVisuals(ContextWorld(Handle), true))
				{
					Fail(TEXT("JIP per-Knight equipment visuals did not match committed assignments on both clients."));
					return;
				}
			}
			Test.AddInfo(TEXT("Real disconnect -> companion takeover -> same-slot JIP converged all three Knight RPG views, inventory, source projections, attributes, native weapon/rune visuals and owning specs on both clients; no client AI controllers."));
			BeginCleanup();
		}

		void WaitSoloSwitch()
		{
			if (Winner->GetControlledKnight() != Knights[NewSlot].Get() || !HasClientSnapshots())
			{
				return;
			}
			if (!IsAtomicUnchanged(AtomicBefore) || Winner->GetLastRPGResult().KnightId != Ids[OldSlot])
			{
				Fail(TEXT("Actual solo switch changed persistent per-Knight RPG state or relabeled the previous receipt."));
				return;
			}
			if (!HasKnightEquipmentVisuals(Server.Get(), true))
			{
				Fail(TEXT("Actual solo switch moved equipment presentation away from its committed canonical Knight."));
				return;
			}
			// This is an authenticated read-only old receipt replay after a real switch, not an in-flight network claim.
			Pending = WinningPickupCommand;
			Send(Winner.Get(), Pending);
			Enter(EKnightRPGNetworkPhase::SoloReplay);
		}

		void WaitSoloReplay()
		{
			if (!WaitSuccess())
			{
				return;
			}
			const FKnightRPGCommandResult& Reply = Capture->Replies.FindChecked(Pending.RequestId);
			if (!Reply.bWasReplay || Reply.KnightId != Ids[OldSlot] || Winner->GetControlledKnight() != Knights[NewSlot].Get()
				|| !IsAtomicUnchanged(AtomicBefore))
			{
				Fail(TEXT("Authenticated old receipt replay mutated the newly controlled Knight or lost original ID correlation."));
				return;
			}
			FScopedConditionalWorldSwitcher Switch(Server.Get());
			Winner->RequestSwitchKnight(OldSlot);
			Enter(EKnightRPGNetworkPhase::SoloReturn);
		}

		void WaitSoloReturn()
		{
			if (Winner->GetControlledKnight() != Knights[OldSlot].Get() || !HasClientSnapshots())
			{
				return;
			}
			if (!IsAtomicUnchanged(AtomicBefore))
			{
				Fail(TEXT("Returning to the original Knight changed its retained rune/equipment/skill state or grants."));
				return;
			}
			if (!HasKnightEquipmentVisuals(Server.Get(), true))
			{
				Fail(TEXT("Returning to the original Knight did not preserve native per-Knight equipment presentation."));
				return;
			}
			Test.AddInfo(TEXT("Actual standalone switch away/back preserved the same three actors/views/GAS projections/specs and each Knight's native weapon/rune visuals; old authenticated receipt remained correlated to its original Knight and replayed without mutation."));
			BeginCleanup();
		}

		FString GetDiagnostics(const TCHAR* Prefix) const
		{
			const int64 InventoryRevision = RPGState.IsValid() ? RPGState->GetInventory()->GetView().Revision : -1;
			const int64 RPGRevision = RPGState.IsValid() ? RPGState->GetRPG()->GetView().Revision : -1;
			const FKnightRPGCommandResult* Reply = Capture->Replies.Find(Pending.RequestId);
			return FString::Printf(TEXT("%s phase=%d elapsed=%.2f network=%d invrev=%lld rpgrev=%lld replies=%d pending=%s error=%d message=%s clientsConverged=%d"),
				Prefix, static_cast<int32>(Phase), Elapsed(), bNetwork, InventoryRevision, RPGRevision,
				Capture->Replies.Num(), *Pending.RequestId.ToString(), Reply != nullptr ? static_cast<int32>(Reply->Result.Error) : -1,
				Reply != nullptr ? *Reply->Result.Message.ToString() : TEXT("waiting"), HasClientSnapshots());
		}

		void UnbindReplies()
		{
			for (int32 Index = 0; Index < ObservedPlayers.Num(); ++Index)
			{
				if (ObservedPlayers[Index].IsValid())
				{
					ObservedPlayers[Index]->OnRPGCommandResult.Remove(ReplyHandles[Index]);
				}
			}
			ObservedPlayers.Reset();
			ReplyHandles.Reset();
		}

		void BeginCleanup()
		{
			if (Phase >= EKnightRPGNetworkPhase::Cleanup)
			{
				return;
			}
			UnbindReplies();
			Enter(EKnightRPGNetworkPhase::Cleanup);
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

		void RestoreWindows()
		{
			if (Settings.IsValid())
			{
				ULevelEditorPlaySettings* Defaults = GetMutableDefault<ULevelEditorPlaySettings>();
				Defaults->MultipleInstancePositions = OriginalPositions;
				Defaults->LastSize = OriginalLastSize;
				Defaults->NewWindowPosition = OriginalWindowPosition;
				Defaults->NewWindowWidth = OriginalWidth;
				Defaults->NewWindowHeight = OriginalHeight;
				Defaults->SaveConfig();
			}
		}

		void Cleanup()
		{
			if (bOwnsSession && GEditor != nullptr && GEditor->IsPlaySessionInProgress())
			{
				if (Elapsed() < 15.0)
				{
					return;
				}
				Test.AddError(TEXT("RPG PIE cleanup timed out; forcing synchronous EndPlay."));
				if (GEditor->IsPlayingSessionInEditor())
				{
					GEditor->EndPlayMap();
				}
				else if (GEditor->IsPlaySessionRequestQueued())
				{
					GEditor->CancelRequestPlaySession();
				}
			}
			RestoreWindows();
			Settings.Reset();
			bOwnsSession = false;
			Enter(EKnightRPGNetworkPhase::Complete);
		}

		FAutomationTestBase& Test;
		bool bNetwork = false;
		TSharedPtr<FKnightRPGReplyCapture> Capture;
		EKnightRPGNetworkPhase Phase = EKnightRPGNetworkPhase::Start;
		double PhaseStarted = 0.0;
		bool bOwnsSession = false;
		TStrongObjectPtr<ULevelEditorPlaySettings> Settings;
		TArray<FIntPoint> OriginalPositions;
		FIntPoint OriginalLastSize;
		FIntPoint OriginalWindowPosition;
		int32 OriginalWidth = 0;
		int32 OriginalHeight = 0;
		TWeakObjectPtr<UWorld> Server;
		TWeakObjectPtr<AKnightRPGStateActor> RPGState;
		TWeakObjectPtr<AKnightRPGPlayerController> Host;
		TWeakObjectPtr<AKnightRPGPlayerController> Winner;
		TArray<FName> ClientContexts;
		TArray<TWeakObjectPtr<AKnightRPGPlayerController>> Players;
		TArray<TWeakObjectPtr<AKnightRPGPlayerController>> ObservedPlayers;
		TArray<FDelegateHandle> ReplyHandles;
		TArray<TWeakObjectPtr<AKnightCharacter>> Knights;
		TArray<FKnightEntityId> Ids;
		TArray<FPrimaryAssetId> Heroes;
		TArray<TWeakObjectPtr<AKnightItemPickup>> Pickups;
		TWeakObjectPtr<AKnightItemPickup> RacedPickup;
		TWeakObjectPtr<AKnightItemPickup> RangePickup;
		TArray<FKnightRPGCommand> RaceCommands;
		FKnightRPGCommand Pending;
		FKnightRPGCommand WinningPickupCommand;
		FKnightRPGCommandResult WinningPickupReceipt;
		FKnightRPGAtomicCapture AtomicBefore;
		FKnightRPGAtomicCapture BeforePurchase;
		FGuid EquipmentId;
		FGuid RuneId;
		int32 WinnerIndex = INDEX_NONE;
		int32 OldSlot = INDEX_NONE;
		int32 NewSlot = INDEX_NONE;
		FName DepartingContext;
		FString ReconnectURL;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKnightRPGNetworkTransactionTest, "Knight.Items.RPG.NetworkTransactionsJIP",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FKnightRPGNetworkTransactionTest::RunTest(const FString& Parameters)
{
	FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<KnightRPGNetwork::FKnightRPGNetworkCommand>(*this, true));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKnightRPGSoloSwitchReceiptTest, "Knight.Items.RPG.SoloSwitchReceipt",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FKnightRPGSoloSwitchReceiptTest::RunTest(const FString& Parameters)
{
	FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<KnightRPGNetwork::FKnightRPGNetworkCommand>(*this, false));
	return true;
}

#endif
