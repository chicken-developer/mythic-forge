#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "AIController.h"
#include "Attributes/GolemAttributeSet.h"
#include "Character/GolemCharacter.h"
#include "Commands/GolemRPGCommand.h"
#include "Components/GolemAbilitySystemComponent.h"
#include "Components/GolemLifeComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/GolemGameState.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayAbilitySpec.h"
#include "GameplayEffectTypes.h"
#include "HAL/PlatformTime.h"
#include "IPAddress.h"
#include "Inventory/GolemInventoryComponent.h"
#include "Loot/GolemItemPickup.h"
#include "Misc/PackageName.h"
#include "Party/GolemPartyState.h"
#include "PartyArena/GolemArenaEnemy.h"
#include "PartyArena/GolemPartyArenaStartMarker.h"
#include "PartyArena/GolemPartyArenaState.h"
#include "PlayInEditorDataTypes.h"
#include "Player/GolemPlayerState.h"
#include "Progression/GolemRPGComponent.h"
#include "RPGLoop/GolemRPGCharacter.h"
#include "RPGLoop/GolemRPGGameState.h"
#include "RPGLoop/GolemRPGLoopGameMode.h"
#include "RPGLoop/GolemRPGPlayerController.h"
#include "RPGLoop/GolemRPGStateActor.h"
#include "RPGLoop/GolemRPGUpgradeStation.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Tags/GolemGameplayTags.h"
#include "UnrealEngine.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectGlobals.h"

namespace GolemRPGNetwork
{
	const TCHAR* RPGMap = TEXT("/Game/Golem/Maps/Playables/L_M05_RPGLoop");
	const TCHAR* DetachedMap = TEXT("/Game/Golem/Maps/Playables/L_M00_Boot");
	constexpr double StartupTimeout = 60.0;
	constexpr double PhaseTimeout = 25.0;
	constexpr uint16 ServerPort = 17905;

	enum class EGolemRPGNetworkPhase : uint8
	{
		Start, Ready, Reward, RacePickup, ReplayPickup, ChangedReplay, ForeignGolem, StaleRevision,
		OutOfRange, CollectRewards, UpgradeReady, RaceEquip, Unequip, Rune, InvalidSkill, CostFailure,
		Purchase, Respec, Disconnect, Rejoin, SoloSwitch, SoloReplay, SoloReturn, Cleanup, Complete
	};

	struct FGolemRPGReplyCapture
	{
		TMap<FGuid, FGolemRPGCommandResult> Replies;
		TMap<FGuid, TWeakObjectPtr<AGolemRPGPlayerController>> Recipients;
		TMap<FGuid, TWeakObjectPtr<AGolemRPGPlayerController>> ExpectedRecipients;
	};

	struct FGolemRPGAtomicCapture
	{
		FGolemInventoryView Inventory;
		FGolemRPGView RPG;
		TArray<FGolemSourceProjection> Projections;
		TArray<TArray<FGameplayAbilitySpecHandle>> Specs;
		TArray<TArray<FActiveGameplayEffectHandle>> Effects;
		TArray<int32> Grants;
		TArray<float> AttackPower;
		TArray<float> Defense;
		TArray<float> MaxHealth;
		TArray<float> MaxStamina;
		TArray<float> MaxRuneEnergy;
	};

	AGolemPartyState* FindParty(UWorld* World)
	{
		AGolemGameState* State = World != nullptr ? World->GetGameState<AGolemGameState>() : nullptr;
		return State != nullptr ? State->GetPartyState() : nullptr;
	}

	AGolemRPGStateActor* FindRPG(UWorld* World)
	{
		AGolemRPGGameState* State = World != nullptr ? World->GetGameState<AGolemRPGGameState>() : nullptr;
		return State != nullptr ? State->GetRPGState() : nullptr;
	}

	TArray<FGameplayAbilitySpecHandle> GetSpecs(const UGolemAbilitySystemComponent* ASC)
	{
		TArray<FGameplayAbilitySpecHandle> Result;
		for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
		{
			Result.Add(Spec.Handle);
		}
		return Result;
	}

	bool HasSameProjection(const FGolemSourceProjection& Left, const FGolemSourceProjection& Right)
	{
		return Left.EntityId == Right.EntityId && Left.Revision == Right.Revision && Left.Plan == Right.Plan;
	}

	const FGolemRPGGolemView* FindGolemView(const FGolemRPGView& View, const FGolemEntityId& Id)
	{
		return View.Golems.FindByPredicate([&Id](const FGolemRPGGolemView& Golem)
			{
				return Golem.GolemId == Id;
			});
	}

	/** Explicit encounter damage/positions isolate transaction faults; reward callbacks, RPCs and replication are real. */
	class FGolemRPGNetworkCommand final : public IAutomationLatentCommand
	{
	public:
		FGolemRPGNetworkCommand(FAutomationTestBase& InTest, bool bInNetwork)
			: Test(InTest), bNetwork(bInNetwork), Capture(MakeShared<FGolemRPGReplyCapture>())
		{
		}

		virtual ~FGolemRPGNetworkCommand() override
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
			if (Phase < EGolemRPGNetworkPhase::Cleanup && Test.HasAnyErrors())
			{
				BeginCleanup();
			}
			if (Phase > EGolemRPGNetworkPhase::Ready && Phase < EGolemRPGNetworkPhase::Cleanup
				&& !HasCanonicalIdentities())
			{
				Fail(TEXT("An RPG transaction/ownership change replaced a canonical Golem, ID, hero or ASC owner/avatar."));
			}
			if (Phase > EGolemRPGNetworkPhase::Start && Phase < EGolemRPGNetworkPhase::Cleanup
				&& Elapsed() > (Phase == EGolemRPGNetworkPhase::Ready ? StartupTimeout : PhaseTimeout))
			{
				Fail(GetDiagnostics(TEXT("Actual RPG network phase timed out")));
			}
			switch (Phase)
			{
			case EGolemRPGNetworkPhase::Start: Start(); break;
			case EGolemRPGNetworkPhase::Ready: WaitReady(); break;
			case EGolemRPGNetworkPhase::Reward: WaitReward(); break;
			case EGolemRPGNetworkPhase::RacePickup: WaitPickupRace(); break;
			case EGolemRPGNetworkPhase::ReplayPickup: WaitPickupReplay(); break;
			case EGolemRPGNetworkPhase::ChangedReplay: WaitChangedReplay(); break;
			case EGolemRPGNetworkPhase::ForeignGolem: WaitForeignGolem(); break;
			case EGolemRPGNetworkPhase::StaleRevision: WaitStaleRevision(); break;
			case EGolemRPGNetworkPhase::OutOfRange: WaitOutOfRange(); break;
			case EGolemRPGNetworkPhase::CollectRewards: WaitCollectRewards(); break;
			case EGolemRPGNetworkPhase::UpgradeReady: WaitUpgradeReady(); break;
			case EGolemRPGNetworkPhase::RaceEquip: WaitEquipRace(); break;
			case EGolemRPGNetworkPhase::Unequip: WaitUnequip(); break;
			case EGolemRPGNetworkPhase::Rune: WaitRune(); break;
			case EGolemRPGNetworkPhase::InvalidSkill: WaitInvalidSkill(); break;
			case EGolemRPGNetworkPhase::CostFailure: WaitCostFailure(); break;
			case EGolemRPGNetworkPhase::Purchase: WaitPurchase(); break;
			case EGolemRPGNetworkPhase::Respec: WaitRespec(); break;
			case EGolemRPGNetworkPhase::Disconnect: WaitDisconnect(); break;
			case EGolemRPGNetworkPhase::Rejoin: WaitRejoin(); break;
			case EGolemRPGNetworkPhase::SoloSwitch: WaitSoloSwitch(); break;
			case EGolemRPGNetworkPhase::SoloReplay: WaitSoloReplay(); break;
			case EGolemRPGNetworkPhase::SoloReturn: WaitSoloReturn(); break;
			case EGolemRPGNetworkPhase::Cleanup: Cleanup(); break;
			default: break;
			}
			return Phase == EGolemRPGNetworkPhase::Complete;
		}

	private:
		void Enter(EGolemRPGNetworkPhase InPhase)
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

		AGolemRPGPlayerController* FindOwner(UWorld* World) const
		{
			AGolemPartyState* Party = FindParty(World);
			if (Party == nullptr)
			{
				return nullptr;
			}
			for (FConstPlayerControllerIterator Iterator = World->GetPlayerControllerIterator(); Iterator; ++Iterator)
			{
				AGolemRPGPlayerController* Player = Cast<AGolemRPGPlayerController>(Iterator->Get());
				AGolemPlayerState* Human = Player != nullptr ? Player->GetPlayerState<AGolemPlayerState>() : nullptr;
				AGolemCharacter* Pawn = Player != nullptr ? Player->GetControlledGolem() : nullptr;
				const FGolemRosterEntry* Entry = Human != nullptr ? Party->FindSlot(Human->GetAssignedSlot()) : nullptr;
				if (Player != nullptr && Player->IsLocalController() && Pawn != nullptr && Pawn->IsLocallyControlled()
					&& Entry != nullptr && Entry->Golem == Pawn && Entry->HumanPlayerState == Human
					&& Entry->GolemId == Pawn->GetGolemEntityId() && Pawn->GetGolemAbilitySystem()->GetAvatarActor() == Pawn
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
				: TEXT("Actual M5 standalone requested for persistent Golem switch/receipt correlation."));
			Enter(EGolemRPGNetworkPhase::Ready);
		}

		void BindReply(AGolemRPGPlayerController* Player)
		{
			const TWeakPtr<FGolemRPGReplyCapture> WeakCapture(Capture);
			const TWeakObjectPtr<AGolemRPGPlayerController> WeakPlayer(Player);
			const FDelegateHandle Handle = Player->OnRPGCommandResult.AddLambda(
				[WeakCapture, WeakPlayer](const FGolemRPGCommandResult& Outcome)
				{
					if (TSharedPtr<FGolemRPGReplyCapture> Current = WeakCapture.Pin())
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
			AGolemPartyState* Party = FindParty(Server.Get());
			AGolemRPGLoopGameMode* Mode = Server.IsValid() ? Server->GetAuthGameMode<AGolemRPGLoopGameMode>() : nullptr;
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
					AGolemRPGPlayerController* Player = FindOwner(ContextWorld(Handle));
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
				AGolemCharacter* Golem = Party->FindSlot(Slot)->Golem;
				Golems.Add(Golem);
				Ids.Add(Golem->GetGolemEntityId());
				Heroes.Add(Golem->GetHeroDefinitionId());
			}
			if (!HasClientSnapshots())
			{
				Golems.Reset();
				Ids.Reset();
				Heroes.Reset();
				return;
			}
			for (const TWeakObjectPtr<AGolemRPGPlayerController>& Player : Players)
			{
				BindReply(Player.Get());
			}
			AGolemPartyArenaStartMarker* Marker = nullptr;
			for (TActorIterator<AGolemPartyArenaStartMarker> Iterator(Server.Get()); Iterator; ++Iterator)
			{
				Marker = *Iterator;
				break;
			}
			if (Marker == nullptr)
			{
				Fail(TEXT("M5 actual arena start marker is missing."));
				return;
			}
			Place(Host->GetControlledGolem(), Marker->GetActorLocation() + FVector(0.0, -150.0, 100.0));
			FScopedConditionalWorldSwitcher Switch(Server.Get());
			Host->RequestInteract();
			Test.AddInfo(TEXT("Transaction fixture uses explicit server GAS damage against actual encounter pawns; real death/Victory reward callbacks remain required. This is not normal combat playthrough evidence."));
			Enter(EGolemRPGNetworkPhase::Reward);
		}

		bool HasCanonicalIdentities() const
		{
			AGolemPartyState* Party = FindParty(Server.Get());
			if (Party == nullptr || Golems.Num() != 3)
			{
				return false;
			}
			int32 Count = 0;
			for (TActorIterator<AGolemCharacter> Iterator(Server.Get()); Iterator; ++Iterator)
			{
				++Count;
			}
			if (Count != 3)
			{
				return false;
			}
			for (int32 Slot = 0; Slot < 3; ++Slot)
			{
				const FGolemRosterEntry* Entry = Party->FindSlot(Slot);
				if (!Golems[Slot].IsValid() || Entry == nullptr || Entry->Golem != Golems[Slot].Get()
					|| Entry->GolemId != Ids[Slot] || Entry->HeroDefinitionId != Heroes[Slot]
					|| Golems[Slot]->GetGolemAbilitySystem()->GetOwnerActor() != Golems[Slot].Get()
					|| Golems[Slot]->GetGolemAbilitySystem()->GetAvatarActor() != Golems[Slot].Get())
				{
					return false;
				}
			}
			return true;
		}

		bool HasGolemEquipmentVisuals(UWorld* World, const bool bAssert = false) const
		{
			AGolemPartyState* Party = FindParty(World);
			AGolemRPGStateActor* State = FindRPG(World);
			if (Party == nullptr || !Party->IsCanonicalRosterReady() || State == nullptr)
			{
				return false;
			}
			bool bMatches = true;
			for (int32 Slot = 0; Slot < 3; ++Slot)
			{
				const FGolemRosterEntry* Entry = Party->FindSlot(Slot);
				const AGolemRPGCharacter* Golem = Entry != nullptr
					? Cast<AGolemRPGCharacter>(Entry->Golem.Get()) : nullptr;
				const FGolemRPGGolemView View = Entry != nullptr
					? State->GetRPG()->GetGolemView(Entry->GolemId) : FGolemRPGGolemView();
				if (Golem == nullptr || !View.IsInitialized() || View.GolemId != Golem->GetGolemEntityId()
					|| Golem->GetWeaponUpgradeVisual() == nullptr || Golem->GetRuneVisual() == nullptr)
				{
					return false;
				}
				const bool bExpectedWeapon = View.FindAssignment(GolemGameplayTags::Slot_Equipment_Weapon) != nullptr;
				const bool bExpectedRune = View.FindAssignment(GolemGameplayTags::Slot_Equipment_Rune) != nullptr;
				const bool bWeaponVisible = Golem->GetWeaponUpgradeVisual()->IsVisible();
				const bool bRuneVisible = Golem->GetRuneVisual()->IsVisible();
				bMatches &= bWeaponVisible == bExpectedWeapon && bRuneVisible == bExpectedRune;
				if (bAssert)
				{
					Test.TestEqual(FString::Printf(TEXT("%s slot %d weapon visual matches committed Golem assignment"),
						*World->GetName(), Slot), bWeaponVisible, bExpectedWeapon);
					Test.TestEqual(FString::Printf(TEXT("%s slot %d rune visual matches committed Golem assignment"),
						*World->GetName(), Slot), bRuneVisible, bExpectedRune);
				}
			}
			return bMatches;
		}

		bool HasClientSnapshots() const
		{
			if (!RPGState.IsValid() || Golems.Num() != 3 || Ids.Num() != 3 || Heroes.Num() != 3
				|| !HasGolemEquipmentVisuals(Server.Get()))
			{
				return false;
			}
			for (FName Handle : ClientContexts)
			{
				UWorld* World = ContextWorld(Handle);
				AGolemPartyState* Party = FindParty(World);
				AGolemRPGStateActor* State = FindRPG(World);
				if (World == nullptr || World->GetNetMode() != NM_Client || Party == nullptr
					|| !Party->IsCanonicalRosterReady() || State == nullptr
					|| !(State->GetInventory()->GetView() == RPGState->GetInventory()->GetView())
					|| !(State->GetRPG()->GetView() == RPGState->GetRPG()->GetView())
					|| !HasGolemEquipmentVisuals(World))
				{
					return false;
				}
				int32 Count = 0;
				for (TActorIterator<AGolemCharacter> Iterator(World); Iterator; ++Iterator)
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
					const FGolemRosterEntry* Entry = Party->FindSlot(Slot);
					UGolemAbilitySystemComponent* ASC = Entry != nullptr && Entry->Golem != nullptr
						? Entry->Golem->GetGolemAbilitySystem() : nullptr;
					UGolemAbilitySystemComponent* Authority = Golems[Slot]->GetGolemAbilitySystem();
					if (ASC == nullptr || Entry->GolemId != Ids[Slot] || Entry->HeroDefinitionId != Heroes[Slot]
						|| ASC->GetOwnerActor() != Entry->Golem || ASC->GetAvatarActor() != Entry->Golem
						|| ASC->GetOwnedGrantCount() != Authority->GetOwnedGrantCount()
						|| !HasSameProjection(ASC->GetSourceProjection(GolemRPG::ProjectionSource),
							Authority->GetSourceProjection(GolemRPG::ProjectionSource))
						|| !FMath::IsNearlyEqual(Entry->Golem->GetGolemAttributeSet()->GetAttackPower(),
							Golems[Slot]->GetGolemAttributeSet()->GetAttackPower()))
					{
						return false;
					}
				}
				for (const TWeakObjectPtr<AGolemItemPickup>& Authority : Pickups)
				{
					if (!Authority.IsValid())
					{
						return false;
					}
					AGolemItemPickup* Replica = nullptr;
					for (TActorIterator<AGolemItemPickup> Iterator(World); Iterator; ++Iterator)
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

		void Place(AGolemCharacter* Golem, const FVector& Location)
		{
			FScopedConditionalWorldSwitcher Switch(Server.Get());
			Golem->GetCharacterMovement()->StopMovementImmediately();
			Golem->SetActorLocationAndRotation(Location, FRotator::ZeroRotator, false, nullptr, ETeleportType::TeleportPhysics);
			Golem->ForceNetUpdate();
		}

		AGolemCharacter* ServerGolem(AGolemRPGPlayerController* Player) const
		{
			const FGolemEntityId Id = Player->GetControlledGolem()->GetGolemEntityId();
			for (const TWeakObjectPtr<AGolemCharacter>& Golem : Golems)
			{
				if (Golem->GetGolemEntityId() == Id)
				{
					return Golem.Get();
				}
			}
			return nullptr;
		}

		FGolemRPGCommand MakeCommand(AGolemRPGPlayerController* Player, EGolemRPGOperation Operation) const
		{
			FGolemRPGCommand Command;
			Command.RequestId = FGuid::NewGuid();
			Command.Operation = Operation;
			Command.ExpectedGolemId = Player->GetControlledGolem()->GetGolemEntityId();
			Command.InventoryId = RPGState->GetInventory()->GetView().InventoryId;
			Command.ExpectedInventoryRevision = RPGState->GetInventory()->GetView().Revision;
			Command.ExpectedGolemRevision = RPGState->GetRPG()->GetGolemView(Command.ExpectedGolemId).Revision;
			return Command;
		}

		void Send(AGolemRPGPlayerController* Player, const FGolemRPGCommand& Command)
		{
			Capture->Replies.Remove(Command.RequestId);
			Capture->Recipients.Remove(Command.RequestId);
			Capture->ExpectedRecipients.Add(Command.RequestId, Player);
			FScopedConditionalWorldSwitcher Switch(Player->GetWorld());
			Player->RequestRPGCommand(Command);
		}

		bool HasOwnedReply(const FGolemRPGCommand& Command) const
		{
			const TWeakObjectPtr<AGolemRPGPlayerController>* Actual = Capture->Recipients.Find(Command.RequestId);
			const TWeakObjectPtr<AGolemRPGPlayerController>* Expected = Capture->ExpectedRecipients.Find(Command.RequestId);
			const FGolemRPGCommandResult* Reply = Capture->Replies.Find(Command.RequestId);
			return Actual != nullptr && Expected != nullptr && Actual->IsValid() && *Actual == *Expected
				&& (*Actual)->IsLocalController() && (!bNetwork || (*Actual)->GetWorld()->GetNetMode() == NM_Client)
				&& Reply != nullptr && Reply->GolemId == Command.ExpectedGolemId
				&& Reply->InventoryId == Command.InventoryId && Reply->Operation == Command.Operation;
		}

		FGolemRPGAtomicCapture CaptureAtomic() const
		{
			FGolemRPGAtomicCapture Result;
			Result.Inventory = RPGState->GetInventory()->GetView();
			Result.RPG = RPGState->GetRPG()->GetView();
			for (const TWeakObjectPtr<AGolemCharacter>& Golem : Golems)
			{
				UGolemAbilitySystemComponent* ASC = Golem->GetGolemAbilitySystem();
				Result.Projections.Add(ASC->GetSourceProjection(GolemRPG::ProjectionSource));
				Result.Specs.Add(GetSpecs(ASC));
				Result.Effects.Add(ASC->GetActiveEffects(FGameplayEffectQuery()));
				Result.Grants.Add(ASC->GetOwnedGrantCount());
				Result.AttackPower.Add(Golem->GetGolemAttributeSet()->GetAttackPower());
				Result.Defense.Add(Golem->GetGolemAttributeSet()->GetDefense());
				Result.MaxHealth.Add(Golem->GetGolemAttributeSet()->GetMaxHealth());
				Result.MaxStamina.Add(Golem->GetGolemAttributeSet()->GetMaxStamina());
				Result.MaxRuneEnergy.Add(Golem->GetGolemAttributeSet()->GetMaxRuneEnergy());
			}
			return Result;
		}

		bool IsAtomicUnchanged(const FGolemRPGAtomicCapture& Before) const
		{
			const FGolemRPGAtomicCapture After = CaptureAtomic();
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

		bool WaitRejection(EGolemError Error)
		{
			const FGolemRPGCommandResult* Reply = Capture->Replies.Find(Pending.RequestId);
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
			const FGolemRPGCommandResult* Reply = Capture->Replies.Find(Pending.RequestId);
			if (Reply == nullptr)
			{
				return false;
			}
			if (!HasOwnedReply(Pending) || !Reply->Result.IsSuccess() || Reply->GolemId != Pending.ExpectedGolemId
				|| Reply->InventoryId != Pending.InventoryId || Reply->Operation != Pending.Operation)
			{
				Fail(FString::Printf(TEXT("Actual RPG RPC failed or miscorrelated op=%d error=%d source=%s receipt=%s: %s"),
					static_cast<int32>(Pending.Operation), static_cast<int32>(Reply->Result.Error),
					*Pending.ExpectedGolemId.Value.ToString(), *Reply->GolemId.Value.ToString(),
					*Reply->Result.Message.ToString()));
				return false;
			}
			return HasClientSnapshots();
		}

		void WaitReward()
		{
			AGolemRPGLoopGameMode* Mode = Server->GetAuthGameMode<AGolemRPGLoopGameMode>();
			if (Mode->GetArenaState()->GetSnapshot().Stage == EGolemArenaStage::Defeat)
			{
				Fail(TEXT("Encounter fault setup unexpectedly caused Defeat before genuine loot reward."));
				return;
			}
			FScopedConditionalWorldSwitcher Switch(Server.Get());
			for (AGolemArenaEnemy* Enemy : Mode->GetEncounterEnemies())
			{
				if (Enemy->GetLifeComponent()->IsAlive())
				{
					const FGolemResult Result = UGolemAbilitySystemComponent::ApplyDamage(Host->GetControlledGolem(), Enemy,
						Enemy->GetGolemAttributeSet()->GetMaxHealth() + Enemy->GetGolemAttributeSet()->GetDefense() + 1000.0f);
					if (!Result.IsSuccess())
					{
						Fail(TEXT("Validated real GAS encounter damage setup failed."));
						return;
					}
				}
			}
			if (RPGState->GetLoopSnapshot().Stage != EGolemRPGLoopStage::RewardAvailable
				|| !RPGState->GetLoopSnapshot().bRewardIssued || !HasClientSnapshots())
			{
				return;
			}
			Pickups.Reset();
			for (TActorIterator<AGolemItemPickup> Iterator(Server.Get()); Iterator; ++Iterator)
			{
				Pickups.Add(*Iterator);
				if (Iterator->GetView().Item.DefinitionId == FPrimaryAssetId(TEXT("GolemItem"), TEXT("HammerUpgrade")))
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
				Place(ServerGolem(Players[Index].Get()), RacedPickup->GetActorLocation() + FVector(0.0, Index == 0 ? -90.0 : 90.0, 100.0));
			}
			RaceCommands.Reset();
			AtomicBefore = CaptureAtomic();
			for (const TWeakObjectPtr<AGolemRPGPlayerController>& Player : Players)
			{
				FGolemRPGCommand Command = MakeCommand(Player.Get(), EGolemRPGOperation::Pickup);
				Command.PickupId = RacedPickup->GetPickupId();
				RaceCommands.Add(Command);
				Send(Player.Get(), Command);
			}
			Enter(EGolemRPGNetworkPhase::RacePickup);
		}

		void WaitPickupRace()
		{
			int32 Successes = 0;
			for (int32 Index = 0; Index < RaceCommands.Num(); ++Index)
			{
				const FGolemRPGCommandResult* Reply = Capture->Replies.Find(RaceCommands[Index].RequestId);
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
				else if (Reply->Result.Error != EGolemError::InvalidRevision && Reply->Result.Error != EGolemError::Conflict)
				{
					Fail(TEXT("Losing pickup race did not report the actual stale revision/claimed-item conflict."));
					return;
				}
			}
			const FGolemInventoryView Inventory = RPGState->GetInventory()->GetView();
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
			OldSlot = Winner->GetPlayerState<AGolemPlayerState>()->GetAssignedSlot();
			EquipmentId = WinningPickupReceipt.AffectedInstanceId;
			Test.AddInfo(FString::Printf(TEXT("Real pickup race committed one request=%s instance=%s invrev=%lld sourceGolem=%s."),
				*WinningPickupCommand.RequestId.ToString(), *EquipmentId.ToString(), Inventory.Revision,
				*WinningPickupReceipt.GolemId.Value.ToString()));
			AtomicBefore = CaptureAtomic();
			Pending = WinningPickupCommand;
			Send(Winner.Get(), Pending);
			Enter(EGolemRPGNetworkPhase::ReplayPickup);
		}

		void WaitPickupReplay()
		{
			if (!WaitSuccess())
			{
				return;
			}
			const FGolemRPGCommandResult& Reply = Capture->Replies.FindChecked(Pending.RequestId);
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
			Enter(EGolemRPGNetworkPhase::ChangedReplay);
		}

		void WaitChangedReplay()
		{
			if (!WaitRejection(EGolemError::Conflict))
			{
				return;
			}
			AGolemRPGUpgradeStation* Station = Server->GetAuthGameMode<AGolemRPGLoopGameMode>()->GetUpgradeStation();
			if (Station == nullptr)
			{
				Fail(TEXT("Actual upgrade station is missing for the ownership/revision fixture."));
				return;
			}
			Place(ServerGolem(Winner.Get()), Station->GetActorLocation() + FVector(0.0, -150.0, 100.0));
			Pending = MakeCommand(Winner.Get(), EGolemRPGOperation::Equip);
			Pending.ExpectedGolemId = Ids[(OldSlot + 1) % 3];
			Pending.ExpectedGolemRevision = RPGState->GetRPG()->GetGolemView(Pending.ExpectedGolemId).Revision;
			Pending.ItemInstanceId = EquipmentId;
			Pending.SlotTag = GolemGameplayTags::Slot_Equipment_Weapon;
			AtomicBefore = CaptureAtomic();
			Send(Winner.Get(), Pending);
			Enter(EGolemRPGNetworkPhase::ForeignGolem);
		}

		void WaitForeignGolem()
		{
			if (!WaitRejection(EGolemError::NotOwned))
			{
				return;
			}
			Pending = MakeCommand(Winner.Get(), EGolemRPGOperation::Equip);
			--Pending.ExpectedInventoryRevision;
			Pending.ItemInstanceId = EquipmentId;
			Pending.SlotTag = GolemGameplayTags::Slot_Equipment_Weapon;
			AtomicBefore = CaptureAtomic();
			Send(Winner.Get(), Pending);
			Enter(EGolemRPGNetworkPhase::StaleRevision);
		}

		void WaitStaleRevision()
		{
			if (!WaitRejection(EGolemError::InvalidRevision))
			{
				return;
			}
			for (const TWeakObjectPtr<AGolemItemPickup>& Pickup : Pickups)
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
			Place(ServerGolem(Winner.Get()), RangePickup->GetActorLocation() + FVector(900.0, 0.0, 100.0));
			Pending = MakeCommand(Winner.Get(), EGolemRPGOperation::Pickup);
			Pending.PickupId = RangePickup->GetPickupId();
			AtomicBefore = CaptureAtomic();
			Send(Winner.Get(), Pending);
			Enter(EGolemRPGNetworkPhase::OutOfRange);
		}

		void WaitOutOfRange()
		{
			if (!WaitRejection(EGolemError::OutOfRange))
			{
				return;
			}
			if (RangePickup->IsClaimed())
			{
				Fail(TEXT("Rejected out-of-range RPC still claimed the world pickup."));
				return;
			}
			Pending = FGolemRPGCommand();
			Enter(EGolemRPGNetworkPhase::CollectRewards);
		}

		void WaitCollectRewards()
		{
			if (Pending.RequestId.IsValid())
			{
				if (!WaitSuccess())
				{
					return;
				}
				Pending = FGolemRPGCommand();
			}
			for (const TWeakObjectPtr<AGolemItemPickup>& Pickup : Pickups)
			{
				if (!Pickup->IsClaimed())
				{
					Place(ServerGolem(Winner.Get()), Pickup->GetActorLocation() + FVector(0.0, -100.0, 100.0));
					Pending = MakeCommand(Winner.Get(), EGolemRPGOperation::Pickup);
					Pending.PickupId = Pickup->GetPickupId();
					Send(Winner.Get(), Pending);
					return;
				}
			}
			const FGolemInventoryView Inventory = RPGState->GetInventory()->GetView();
			for (const FGolemItemInstance& Item : Inventory.Items)
			{
				if (Item.DefinitionId == FPrimaryAssetId(TEXT("GolemItem"), TEXT("RuneUpgrade")))
				{
					RuneId = Item.InstanceId;
				}
			}
			AGolemRPGLoopGameMode* Mode = Server->GetAuthGameMode<AGolemRPGLoopGameMode>();
			AGolemRPGUpgradeStation* Station = Mode->GetUpgradeStation();
			if (!RuneId.IsValid() || Station == nullptr)
			{
				Fail(TEXT("Actual reward rune or upgrade station is missing."));
				return;
			}
			for (int32 Index = 0; Index < Players.Num(); ++Index)
			{
				Place(ServerGolem(Players[Index].Get()), Station->GetActorLocation() + FVector(Index * 150.0, -100.0, 100.0));
			}
			Enter(EGolemRPGNetworkPhase::UpgradeReady);
		}

		void WaitUpgradeReady()
		{
			AGolemRPGLoopGameMode* Mode = Server->GetAuthGameMode<AGolemRPGLoopGameMode>();
			if (!HasClientSnapshots())
			{
				return;
			}
			for (const TWeakObjectPtr<AGolemRPGPlayerController>& Player : Players)
			{
				if (!Mode->IsInUpgradeSafeZone(ServerGolem(Player.Get())))
				{
					Fail(TEXT("Explicit transaction fixture is outside the actual upgrade safe zone."));
					return;
				}
			}
			RaceCommands.Reset();
			AtomicBefore = CaptureAtomic();
			for (const TWeakObjectPtr<AGolemRPGPlayerController>& Player : Players)
			{
				FGolemRPGCommand Command = MakeCommand(Player.Get(), EGolemRPGOperation::Equip);
				Command.ItemInstanceId = EquipmentId;
				Command.SlotTag = GolemGameplayTags::Slot_Equipment_Weapon;
				RaceCommands.Add(Command);
				Send(Player.Get(), Command);
			}
			Enter(EGolemRPGNetworkPhase::RaceEquip);
		}

		void WaitEquipRace()
		{
			int32 Successes = 0;
			for (int32 Index = 0; Index < RaceCommands.Num(); ++Index)
			{
				const FGolemRPGCommandResult* Reply = Capture->Replies.Find(RaceCommands[Index].RequestId);
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
					OldSlot = Winner->GetPlayerState<AGolemPlayerState>()->GetAssignedSlot();
				}
				else if (Reply->Result.Error != EGolemError::InvalidRevision && Reply->Result.Error != EGolemError::Conflict)
				{
					Fail(TEXT("Losing equipment race did not report actual stale revision/exclusive-item conflict."));
					return;
				}
			}
			const FGolemRPGView View = RPGState->GetRPG()->GetView();
			int32 Assignments = 0;
			for (const FGolemRPGGolemView& Golem : View.Golems)
			{
				const FGolemEquipmentAssignment* Assignment = Golem.FindAssignment(GolemGameplayTags::Slot_Equipment_Weapon);
				if (Assignment != nullptr && Assignment->ItemInstanceId == EquipmentId)
				{
					++Assignments;
				}
			}
			if (Successes != 1 || Assignments != 1
				|| ServerGolem(Winner.Get())->GetGolemAttributeSet()->GetAttackPower() <= AtomicBefore.AttackPower[OldSlot])
			{
				Fail(TEXT("Same-instance equipment race failed exclusive per-Golem assignment or real GAS attack-power bonus."));
				return;
			}
			for (int32 Slot = 0; Slot < 3; ++Slot)
			{
				const FGolemRPGGolemView* Previous = FindGolemView(AtomicBefore.RPG, Ids[Slot]);
				const FGolemRPGGolemView* Current = FindGolemView(View, Ids[Slot]);
				if (Slot != OldSlot && (!HasSameProjection(AtomicBefore.Projections[Slot],
					Golems[Slot]->GetGolemAbilitySystem()->GetSourceProjection(GolemRPG::ProjectionSource))
					|| Previous == nullptr || Current == nullptr || !(*Previous == *Current)))
				{
					Fail(TEXT("An equipment race leaked projection or RPG mutation into another Golem."));
					return;
				}
			}
			if (!HasClientSnapshots())
			{
				return;
			}
			Test.AddInfo(TEXT("Both-client same-instance equip race committed one exclusive assignment with real GAS stat change; other Golem projections remained exact."));
			Pending = MakeCommand(Winner.Get(), EGolemRPGOperation::Unequip);
			Pending.ItemInstanceId = EquipmentId;
			Pending.SlotTag = GolemGameplayTags::Slot_Equipment_Weapon;
			Send(Winner.Get(), Pending);
			Enter(EGolemRPGNetworkPhase::Unequip);
		}

		void WaitUnequip()
		{
			if (!WaitSuccess())
			{
				return;
			}
			if (RPGState->GetRPG()->GetGolemView(Ids[OldSlot]).FindAssignment(GolemGameplayTags::Slot_Equipment_Weapon) != nullptr
				|| !FMath::IsNearlyEqual(Golems[OldSlot]->GetGolemAttributeSet()->GetAttackPower(), AtomicBefore.AttackPower[OldSlot]))
			{
				Fail(TEXT("Real unequip did not revoke the exact equipment projection and restore attack power."));
				return;
			}
			Pending = MakeCommand(Winner.Get(), EGolemRPGOperation::Rune);
			Pending.SlotTag = GolemGameplayTags::Slot_Equipment_Rune;
			Pending.ItemInstanceId = RuneId;
			Send(Winner.Get(), Pending);
			Enter(EGolemRPGNetworkPhase::Rune);
		}

		void WaitRune()
		{
			if (!WaitSuccess())
			{
				return;
			}
			const FGolemRPGGolemView View = RPGState->GetRPG()->GetGolemView(Ids[OldSlot]);
			const FGolemEquipmentAssignment* Assignment = View.FindAssignment(GolemGameplayTags::Slot_Equipment_Rune);
			const FGolemActionModifier Modifier = Golems[OldSlot]->GetGolemAbilitySystem()->GetActionModifier(GolemGameplayTags::Ability_Combat_Rune);
			if (Assignment == nullptr || Assignment->ItemInstanceId != RuneId
				|| !FMath::IsNearlyEqual(Modifier.DamageScale, View.RuneTuning.DamageScale)
				|| !FMath::IsNearlyEqual(Modifier.CostScale, View.RuneTuning.CostScale)
				|| !FMath::IsNearlyEqual(Modifier.DurationScale, View.RuneTuning.DurationScale)
				|| View.RuneTuning == FGolemRuneTuning())
			{
				Fail(TEXT("Rune transaction did not tune the real shared-GAS action modifier for the correct Golem."));
				return;
			}
			Pending = MakeCommand(Winner.Get(), EGolemRPGOperation::PurchaseSkill);
			Pending.SkillDefinitionId = FPrimaryAssetId(TEXT("GolemSkill"), TEXT("MissingNetworkFixture"));
			AtomicBefore = CaptureAtomic();
			Send(Winner.Get(), Pending);
			Enter(EGolemRPGNetworkPhase::InvalidSkill);
		}

		void WaitInvalidSkill()
		{
			if (!WaitRejection(EGolemError::NotFound))
			{
				return;
			}
			Pending = MakeCommand(Winner.Get(), EGolemRPGOperation::PurchaseSkill);
			Pending.SkillDefinitionId = FPrimaryAssetId(TEXT("GolemSkill"), TEXT("ImpossibleCost"));
			AtomicBefore = CaptureAtomic();
			Send(Winner.Get(), Pending);
			Enter(EGolemRPGNetworkPhase::CostFailure);
		}

		void WaitCostFailure()
		{
			if (!WaitRejection(EGolemError::InsufficientResources))
			{
				return;
			}
			Pending = MakeCommand(Winner.Get(), EGolemRPGOperation::PurchaseSkill);
			Pending.SkillDefinitionId = FPrimaryAssetId(TEXT("GolemSkill"), TEXT("HammerForce"));
			BeforePurchase = CaptureAtomic();
			Send(Winner.Get(), Pending);
			Enter(EGolemRPGNetworkPhase::Purchase);
		}

		void WaitPurchase()
		{
			if (!WaitSuccess())
			{
				return;
			}
			const FGolemRPGGolemView View = RPGState->GetRPG()->GetGolemView(Ids[OldSlot]);
			const FGolemRPGGolemView* Previous = FindGolemView(BeforePurchase.RPG, Ids[OldSlot]);
			if (Previous == nullptr || !View.HasSkill(Pending.SkillDefinitionId) || View.SkillPoints >= Previous->SkillPoints
				|| RPGState->GetInventory()->GetView() == BeforePurchase.Inventory
				|| Golems[OldSlot]->GetGolemAttributeSet()->GetAttackPower() <= BeforePurchase.AttackPower[OldSlot])
			{
				Fail(TEXT("Actual skill purchase did not atomically spend points/materials and apply its real GAS bonus."));
				return;
			}
			Pending = MakeCommand(Winner.Get(), EGolemRPGOperation::Respec);
			Send(Winner.Get(), Pending);
			Enter(EGolemRPGNetworkPhase::Respec);
		}

		void WaitRespec()
		{
			if (!WaitSuccess())
			{
				return;
			}
			const FGolemRPGGolemView View = RPGState->GetRPG()->GetGolemView(Ids[OldSlot]);
			const FGolemRPGGolemView* Previous = FindGolemView(BeforePurchase.RPG, Ids[OldSlot]);
			if (Previous == nullptr || !View.PurchasedSkills.IsEmpty() || View.SkillPoints != Previous->SkillPoints
				|| RPGState->GetInventory()->GetView().Items != BeforePurchase.Inventory.Items
				|| !FMath::IsNearlyEqual(Golems[OldSlot]->GetGolemAttributeSet()->GetAttackPower(), BeforePurchase.AttackPower[OldSlot])
				|| GetSpecs(Golems[OldSlot]->GetGolemAbilitySystem()) != BeforePurchase.Specs[OldSlot]
				|| Golems[OldSlot]->GetGolemAbilitySystem()->GetOwnedGrantCount() != BeforePurchase.Grants[OldSlot])
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
				ReconnectURL = FString::Printf(TEXT("127.0.0.1:%d?GolemSlot=%d"), Address->GetPort(), OldSlot);
				DepartingContext = ClientContexts[WinnerIndex];
				FScopedConditionalWorldSwitcher Switch(Winner->GetWorld());
				Winner->ClientTravel(DetachedMap, TRAVEL_Absolute);
				Enter(EGolemRPGNetworkPhase::Disconnect);
			}
			else
			{
				NewSlot = (OldSlot + 1) % 3;
				FScopedConditionalWorldSwitcher Switch(Server.Get());
				Winner->RequestSwitchGolem(NewSlot);
				Enter(EGolemRPGNetworkPhase::SoloSwitch);
			}
		}

		void WaitDisconnect()
		{
			UWorld* Detached = ContextWorld(DepartingContext);
			AGolemPartyState* Party = FindParty(Server.Get());
			const FGolemRosterEntry* Entry = Party->FindSlot(OldSlot);
			if (Detached == nullptr || Detached->GetNetMode() != NM_Standalone || Detached->GetNetDriver() != nullptr
				|| Detached->GetFirstPlayerController() == nullptr || Entry->OwnerKind != EGolemControllerOwner::Companion
				|| Cast<AAIController>(Golems[OldSlot]->GetController()) == nullptr || Party->GetHumanCount() != 2)
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
			Enter(EGolemRPGNetworkPhase::Rejoin);
		}

		void WaitRejoin()
		{
			AGolemRPGPlayerController* Reclaimed = FindOwner(ContextWorld(DepartingContext));
			if (Reclaimed == nullptr || Reclaimed->GetWorld()->GetNetMode() != NM_Client
				|| Reclaimed->GetPlayerState<AGolemPlayerState>()->GetAssignedSlot() != OldSlot
				|| FindParty(Server.Get())->GetHumanCount() != 3 || !HasClientSnapshots())
			{
				return;
			}
			if (!IsAtomicUnchanged(AtomicBefore)
				|| GetSpecs(Reclaimed->GetControlledGolem()->GetGolemAbilitySystem()) != AtomicBefore.Specs[OldSlot])
			{
				Fail(TEXT("Real JIP/reclaim changed persistent three-Golem views/projections/grants or owning-client specs."));
				return;
			}
			for (FName Handle : ClientContexts)
			{
				if (!HasGolemEquipmentVisuals(ContextWorld(Handle), true))
				{
					Fail(TEXT("JIP per-Golem equipment visuals did not match committed assignments on both clients."));
					return;
				}
			}
			Test.AddInfo(TEXT("Real disconnect -> companion takeover -> same-slot JIP converged all three Golem RPG views, inventory, source projections, attributes, native weapon/rune visuals and owning specs on both clients; no client AI controllers."));
			BeginCleanup();
		}

		void WaitSoloSwitch()
		{
			if (Winner->GetControlledGolem() != Golems[NewSlot].Get() || !HasClientSnapshots())
			{
				return;
			}
			if (!IsAtomicUnchanged(AtomicBefore) || Winner->GetLastRPGResult().GolemId != Ids[OldSlot])
			{
				Fail(TEXT("Actual solo switch changed persistent per-Golem RPG state or relabeled the previous receipt."));
				return;
			}
			if (!HasGolemEquipmentVisuals(Server.Get(), true))
			{
				Fail(TEXT("Actual solo switch moved equipment presentation away from its committed canonical Golem."));
				return;
			}
			// This is an authenticated read-only old receipt replay after a real switch, not an in-flight network claim.
			Pending = WinningPickupCommand;
			Send(Winner.Get(), Pending);
			Enter(EGolemRPGNetworkPhase::SoloReplay);
		}

		void WaitSoloReplay()
		{
			if (!WaitSuccess())
			{
				return;
			}
			const FGolemRPGCommandResult& Reply = Capture->Replies.FindChecked(Pending.RequestId);
			if (!Reply.bWasReplay || Reply.GolemId != Ids[OldSlot] || Winner->GetControlledGolem() != Golems[NewSlot].Get()
				|| !IsAtomicUnchanged(AtomicBefore))
			{
				Fail(TEXT("Authenticated old receipt replay mutated the newly controlled Golem or lost original ID correlation."));
				return;
			}
			FScopedConditionalWorldSwitcher Switch(Server.Get());
			Winner->RequestSwitchGolem(OldSlot);
			Enter(EGolemRPGNetworkPhase::SoloReturn);
		}

		void WaitSoloReturn()
		{
			if (Winner->GetControlledGolem() != Golems[OldSlot].Get() || !HasClientSnapshots())
			{
				return;
			}
			if (!IsAtomicUnchanged(AtomicBefore))
			{
				Fail(TEXT("Returning to the original Golem changed its retained rune/equipment/skill state or grants."));
				return;
			}
			if (!HasGolemEquipmentVisuals(Server.Get(), true))
			{
				Fail(TEXT("Returning to the original Golem did not preserve native per-Golem equipment presentation."));
				return;
			}
			Test.AddInfo(TEXT("Actual standalone switch away/back preserved the same three actors/views/GAS projections/specs and each Golem's native weapon/rune visuals; old authenticated receipt remained correlated to its original Golem and replayed without mutation."));
			BeginCleanup();
		}

		FString GetDiagnostics(const TCHAR* Prefix) const
		{
			const int64 InventoryRevision = RPGState.IsValid() ? RPGState->GetInventory()->GetView().Revision : -1;
			const int64 RPGRevision = RPGState.IsValid() ? RPGState->GetRPG()->GetView().Revision : -1;
			const FGolemRPGCommandResult* Reply = Capture->Replies.Find(Pending.RequestId);
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
			if (Phase >= EGolemRPGNetworkPhase::Cleanup)
			{
				return;
			}
			UnbindReplies();
			Enter(EGolemRPGNetworkPhase::Cleanup);
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
			Enter(EGolemRPGNetworkPhase::Complete);
		}

		FAutomationTestBase& Test;
		bool bNetwork = false;
		TSharedPtr<FGolemRPGReplyCapture> Capture;
		EGolemRPGNetworkPhase Phase = EGolemRPGNetworkPhase::Start;
		double PhaseStarted = 0.0;
		bool bOwnsSession = false;
		TStrongObjectPtr<ULevelEditorPlaySettings> Settings;
		TArray<FIntPoint> OriginalPositions;
		FIntPoint OriginalLastSize;
		FIntPoint OriginalWindowPosition;
		int32 OriginalWidth = 0;
		int32 OriginalHeight = 0;
		TWeakObjectPtr<UWorld> Server;
		TWeakObjectPtr<AGolemRPGStateActor> RPGState;
		TWeakObjectPtr<AGolemRPGPlayerController> Host;
		TWeakObjectPtr<AGolemRPGPlayerController> Winner;
		TArray<FName> ClientContexts;
		TArray<TWeakObjectPtr<AGolemRPGPlayerController>> Players;
		TArray<TWeakObjectPtr<AGolemRPGPlayerController>> ObservedPlayers;
		TArray<FDelegateHandle> ReplyHandles;
		TArray<TWeakObjectPtr<AGolemCharacter>> Golems;
		TArray<FGolemEntityId> Ids;
		TArray<FPrimaryAssetId> Heroes;
		TArray<TWeakObjectPtr<AGolemItemPickup>> Pickups;
		TWeakObjectPtr<AGolemItemPickup> RacedPickup;
		TWeakObjectPtr<AGolemItemPickup> RangePickup;
		TArray<FGolemRPGCommand> RaceCommands;
		FGolemRPGCommand Pending;
		FGolemRPGCommand WinningPickupCommand;
		FGolemRPGCommandResult WinningPickupReceipt;
		FGolemRPGAtomicCapture AtomicBefore;
		FGolemRPGAtomicCapture BeforePurchase;
		FGuid EquipmentId;
		FGuid RuneId;
		int32 WinnerIndex = INDEX_NONE;
		int32 OldSlot = INDEX_NONE;
		int32 NewSlot = INDEX_NONE;
		FName DepartingContext;
		FString ReconnectURL;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGolemRPGNetworkTransactionTest, "Golem.Items.RPG.NetworkTransactionsJIP",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGolemRPGNetworkTransactionTest::RunTest(const FString& Parameters)
{
	FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<GolemRPGNetwork::FGolemRPGNetworkCommand>(*this, true));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGolemRPGSoloSwitchReceiptTest, "Golem.Items.RPG.SoloSwitchReceipt",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGolemRPGSoloSwitchReceiptTest::RunTest(const FString& Parameters)
{
	FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<GolemRPGNetwork::FGolemRPGNetworkCommand>(*this, false));
	return true;
}

#endif
