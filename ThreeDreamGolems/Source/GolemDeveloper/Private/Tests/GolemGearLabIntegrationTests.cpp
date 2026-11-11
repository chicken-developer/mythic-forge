#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "AIController.h"
#include "Assets/GolemAssetManager.h"
#include "Attributes/GolemAttributeSet.h"
#include "Catalog/GolemGearCatalog.h"
#include "Character/GolemCharacter.h"
#include "Components/GolemAbilitySystemComponent.h"
#include "Components/GolemLifeComponent.h"
#include "Definitions/GolemItemDefinition.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/GolemGameState.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GearLab/GolemGearAppearanceComponent.h"
#include "GearLab/GolemGearLabGameMode.h"
#include "GearLab/GolemGearLabPlayerController.h"
#include "GearLab/GolemGearLabState.h"
#include "HAL/PlatformTime.h"
#include "IPAddress.h"
#include "Inventory/GolemInventoryComponent.h"
#include "InputCoreTypes.h"
#include "InputKeyEventArgs.h"
#include "Misc/PackageName.h"
#include "Party/GolemPartyState.h"
#include "PlayInEditorDataTypes.h"
#include "Progression/GolemRPGComponent.h"
#include "RPGLoop/GolemRPGGameState.h"
#include "RPGLoop/GolemRPGStateActor.h"
#include "RPGLoop/GolemRPGUpgradeStation.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Skills/GolemSkillRuntimeComponent.h"
#include "Tags/GolemGameplayTags.h"
#include "UnrealEngine.h"
#include "UObject/StrongObjectPtr.h"

namespace GolemGearLabIntegration
{
	const TCHAR* CatalogMap = TEXT("/Game/Golem/Maps/Playables/L_M05_6_CatalogLab");
	const TCHAR* GearMap = TEXT("/Game/Golem/Maps/Playables/L_M05_6_GearLab");
	const TCHAR* DetachedMap = TEXT("/Game/Golem/Maps/Playables/L_M00_Boot");

	AGolemPartyState* Party(UWorld* World)
	{
		const AGolemGameState* State = World != nullptr ? World->GetGameState<AGolemGameState>() : nullptr;
		return State != nullptr ? State->GetPartyState() : nullptr;
	}

	AGolemRPGStateActor* RPG(UWorld* World)
	{
		const AGolemRPGGameState* State = World != nullptr ? World->GetGameState<AGolemRPGGameState>() : nullptr;
		return State != nullptr ? State->GetRPGState() : nullptr;
	}

	const FGolemRosterEntry* EntryForId(UWorld* World, const FGolemEntityId& Id)
	{
		AGolemPartyState* State = Party(World);
		return State != nullptr ? State->GetRoster().FindByPredicate([&Id](const FGolemRosterEntry& Entry)
		{
			return Entry.GolemId == Id;
		}) : nullptr;
	}

	AGolemGearLabPlayerController* LocalPlayer(UWorld* World)
	{
		if (World != nullptr)
		{
			for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
			{
				AGolemGearLabPlayerController* Player = Cast<AGolemGearLabPlayerController>(It->Get());
				if (Player != nullptr && Player->IsLocalController() && Player->GetControlledGolem() != nullptr)
				{
					return Player;
				}
			}
		}
		return nullptr;
	}

	void Place(AGolemCharacter* Golem, const FVector& Point)
	{
		Golem->SetActorLocation(Point, false, nullptr, ETeleportType::TeleportPhysics);
		Golem->GetCharacterMovement()->StopMovementImmediately();
	}

	struct FReplies
	{
		TMap<FGuid, FGolemRPGCommandResult> Values;
	};

	enum class EPhase : uint8
	{
		Start, Ready, Ownership, Preset, Race, Replay, Rune, Upgrade, RewardRace, Consumable,
		Disconnect, Rejoin, SoloSwitch, Cleanup, Complete
	};

	/** Real PIE/RPC/readmodel/ownership test. Sanctuary placement isolates gear transactions from combat pacing. */
	class FGearNetworkCommand final : public IAutomationLatentCommand
	{
	public:
		FGearNetworkCommand(FAutomationTestBase& InTest, bool bInNetwork)
			: Test(InTest), bNetwork(bInNetwork), Started(FPlatformTime::Seconds()), Replies(MakeShared<FReplies>())
		{
			PhaseStarted = Started;
		}

		virtual ~FGearNetworkCommand() override
		{
			ReleaseQuickInput();
			Unbind();
			if (bOwnSession && GEditor != nullptr && GEditor->IsPlayingSessionInEditor())
			{
				GEditor->EndPlayMap();
			}
			RestoreSettings();
		}

		virtual bool Update() override
		{
			if (Phase < EPhase::Cleanup && (Test.HasAnyErrors() || FPlatformTime::Seconds() - Started > 180.0
				|| FPlatformTime::Seconds() - PhaseStarted > (Phase == EPhase::Ready || Phase == EPhase::Rejoin ? 60.0 : 25.0)))
			{
				if (!Test.HasAnyErrors())
				{
					Test.AddError(FString::Printf(TEXT("GearLab runtime timeout phase%d replies%d"),
						static_cast<int32>(Phase), Replies->Values.Num()));
				}
				LogConvergenceState();
				BeginCleanup();
			}
			switch (Phase)
			{
			case EPhase::Start: Start(); break;
			case EPhase::Ready: Ready(); break;
			case EPhase::Ownership: WaitOwnership(); break;
			case EPhase::Preset: WaitPreset(); break;
			case EPhase::Race: WaitRace(); break;
			case EPhase::Replay: WaitReplay(); break;
			case EPhase::Rune: WaitRune(); break;
			case EPhase::Upgrade: WaitUpgrade(); break;
			case EPhase::RewardRace: WaitRewardRace(); break;
			case EPhase::Consumable: WaitConsumable(); break;
			case EPhase::Disconnect: WaitDisconnect(); break;
			case EPhase::Rejoin: WaitRejoin(); break;
			case EPhase::SoloSwitch: WaitSoloSwitch(); break;
			case EPhase::Cleanup: Cleanup(); break;
			case EPhase::Complete: return true;
			}
			return false;
		}

	private:
		void Enter(EPhase Next)
		{
			Phase = Next;
			PhaseStarted = FPlatformTime::Seconds();
		}

		void Start()
		{
			if (GEditor == nullptr || GEngine == nullptr || GEditor->IsPlaySessionInProgress()
				|| !FPackageName::DoesPackageExist(CatalogMap))
			{
				Test.AddError(TEXT("GearLab requires idle Editor and genuine authored CatalogLab map."));
				return;
			}
			const ULevelEditorPlaySettings* Defaults = GetDefault<ULevelEditorPlaySettings>();
			OriginalPositions = Defaults->MultipleInstancePositions;
			OriginalSize = Defaults->LastSize;
			OriginalPosition = Defaults->NewWindowPosition;
			OriginalWidth = Defaults->NewWindowWidth;
			OriginalHeight = Defaults->NewWindowHeight;
			Settings.Reset(DuplicateObject<ULevelEditorPlaySettings>(Defaults, GetTransientPackage()));
			Settings->SetPlayNetMode(bNetwork ? PIE_ListenServer : PIE_Standalone);
			Settings->SetPlayNumberOfClients(bNetwork ? 3 : 1);
			Settings->SetRunUnderOneProcess(true);
			Settings->SetServerPort(17956);
			Settings->bLaunchSeparateServer = false;
			Settings->EnableGameSound = false;
			Settings->GameGetsMouseControl = false;
			Settings->SetClientWindowSize(FIntPoint(1280, 720));
			Settings->NetworkEmulationSettings.bIsNetworkEmulationEnabled = bNetwork;
			Settings->NetworkEmulationSettings.EmulationTarget = NetworkEmulationTarget::Any;
			Settings->NetworkEmulationSettings.CurrentProfile = TEXT("Custom");
			Settings->NetworkEmulationSettings.OutPackets.MinLatency = 50;
			Settings->NetworkEmulationSettings.OutPackets.MaxLatency = 100;
			Settings->NetworkEmulationSettings.OutPackets.PacketLossPercentage = 0;
			FRequestPlaySessionParams PlayRequest;
			PlayRequest.EditorPlaySettings = Settings.Get();
			PlayRequest.GlobalMapOverride = CatalogMap;
			PlayRequest.bAllowOnlineSubsystem = false;
			GEditor->RequestPlaySession(PlayRequest);
			bOwnSession = true;
			Enter(EPhase::Ready);
		}

		void Bind(AGolemGearLabPlayerController* Player)
		{
			const TWeakPtr<FReplies> Weak(Replies);
			Handles.Add(Player->OnRPGCommandResult.AddLambda([Weak](const FGolemRPGCommandResult& Result)
			{
				if (const TSharedPtr<FReplies> Capture = Weak.Pin())
				{
					Capture->Values.Add(Result.RequestId, Result);
				}
			}));
			ObservedPlayers.Add(Player);
		}

		UWorld* ContextWorld(FName Handle) const
		{
			const FWorldContext* Context = GEngine->GetWorldContextFromHandle(Handle);
			return Context != nullptr ? Context->World() : nullptr;
		}

		bool Converged() const
		{
			if (!Server.IsValid() || RPG(Server.Get()) == nullptr)
			{
				return false;
			}
			for (const FName Context : ClientContexts)
			{
				UWorld* World = ContextWorld(Context);
				if (World == nullptr || World->GetNetMode() != NM_Client || RPG(World) == nullptr
					|| !(RPG(World)->GetInventory()->GetView() == RPG(Server.Get())->GetInventory()->GetView())
					|| !(RPG(World)->GetRPG()->GetView() == RPG(Server.Get())->GetRPG()->GetView()))
				{
					return false;
				}
			}
			return true;
		}

		void LogConvergenceState()
		{
			if (!Server.IsValid() || RPG(Server.Get()) == nullptr)
			{
				Test.AddInfo(TEXT("Gear convergence: authoritative RPG state is unavailable."));
				return;
			}
			const FGolemInventoryView ServerInventory = RPG(Server.Get())->GetInventory()->GetView();
			const FGolemRPGView ServerView = RPG(Server.Get())->GetRPG()->GetView();
			Test.AddInfo(FString::Printf(TEXT("Gear server: humans=%d clientContexts=%d inv=%lld items=%d bag=%d capacity=%d RPG=%lld invref=%lld Golems=%d currency=%lld demo=%d."),
				Party(Server.Get()) != nullptr ? Party(Server.Get())->GetHumanCount() : -1, ClientContexts.Num(),
				ServerInventory.Revision, ServerInventory.Items.Num(), ServerInventory.GetBagCount(), ServerInventory.Capacity,
				ServerView.Revision, ServerView.InventoryRevision, ServerView.Golems.Num(), ServerView.Currency, ServerView.bGearDemo));
			for (const FName Context : ClientContexts)
			{
				UWorld* ClientWorld = ContextWorld(Context);
				AGolemRPGStateActor* State = RPG(ClientWorld);
				if (State == nullptr)
				{
					Test.AddInfo(FString::Printf(TEXT("Gear client %s: RPG state is unavailable."), *Context.ToString()));
					continue;
				}
				const FGolemInventoryView ClientInventory = State->GetInventory()->GetView();
				const FGolemRPGView ClientView = State->GetRPG()->GetView();
				Test.AddInfo(FString::Printf(TEXT("Gear client %s: netMode=%d invMatch=%d RPGMatch=%d inv=%lld items=%d bag=%d capacity=%d RPG=%lld invref=%lld Golems=%d currency=%lld demo=%d."),
					*Context.ToString(), static_cast<int32>(ClientWorld->GetNetMode()), ClientInventory == ServerInventory,
					ClientView == ServerView, ClientInventory.Revision, ClientInventory.Items.Num(), ClientInventory.GetBagCount(),
					ClientInventory.Capacity, ClientView.Revision, ClientView.InventoryRevision, ClientView.Golems.Num(),
					ClientView.Currency, ClientView.bGearDemo));
			}
		}

		FGolemRPGCommand Command(AGolemGearLabPlayerController* Player, EGolemRPGOperation Operation) const
		{
			const FGolemInventoryView Inventory = RPG(Player->GetWorld())->GetInventory()->GetView();
			const FGolemEntityId Id = Player->GetControlledGolem()->GetGolemEntityId();
			FGolemRPGCommand Result;
			Result.Operation = Operation;
			Result.RequestId = FGuid::NewGuid();
			Result.ExpectedGolemId = Id;
			Result.InventoryId = Inventory.InventoryId;
			Result.ExpectedInventoryRevision = Inventory.Revision;
			Result.ExpectedGolemRevision = RPG(Player->GetWorld())->GetRPG()->GetGolemView(Id).Revision;
			return Result;
		}

		void Send(AGolemGearLabPlayerController* Player, const FGolemRPGCommand& InCommand)
		{
			FScopedConditionalWorldSwitcher Switch(Player->GetWorld());
			Player->RequestRPGCommand(InCommand);
		}

		bool Success(const FGuid& Id)
		{
			const FGolemRPGCommandResult* Result = Replies->Values.Find(Id);
			if (Result == nullptr)
			{
				return false;
			}
			if (!CheckedReplies.Contains(Id))
			{
				CheckedReplies.Add(Id);
				Test.TestTrue(FString::Printf(TEXT("RPC %s committed [%s]"), *Id.ToString(), *Result->Result.Context.ToString()),
					Result->Result.IsSuccess());
			}
			return Result->Result.IsSuccess();
		}

		const FGolemItemInstance* FindItem(FName CatalogId, bool bBag = true) const
		{
			SearchItems = RPG(Server.Get())->GetInventory()->GetView().Items;
			return SearchItems.FindByPredicate([CatalogId, bBag](const FGolemItemInstance& Item)
			{
				return Item.DefinitionId == FGolemGearCatalog::DefinitionId(CatalogId)
					&& (!bBag || Item.LocationId == GolemGear::InventoryLocation);
			});
		}

		void Ready()
		{
			ClientContexts.Reset();
			for (const FWorldContext& Context : GEngine->GetWorldContexts())
			{
				if (Context.WorldType == EWorldType::PIE && Context.World() != nullptr)
				{
					if (Context.World()->GetNetMode() == (bNetwork ? NM_ListenServer : NM_Standalone))
					{
						Server = Context.World();
					}
					else if (bNetwork && Context.World()->GetNetMode() == NM_Client)
					{
						ClientContexts.Add(Context.ContextHandle);
					}
				}
			}
			Mode = Server.IsValid() ? Server->GetAuthGameMode<AGolemGearLabGameMode>() : nullptr;
			if (!Mode.IsValid() || !Mode->GetRPGInitializationResult().IsSuccess() || Party(Server.Get()) == nullptr
				|| !Party(Server.Get())->IsCanonicalRosterReady() || Party(Server.Get())->GetHumanCount() != (bNetwork ? 3 : 1)
				|| ClientContexts.Num() != (bNetwork ? 2 : 0) || !Converged())
			{
				return;
			}
			Host = LocalPlayer(Server.Get());
			if (!Host.IsValid())
			{
				return;
			}
			for (const FName Context : ClientContexts)
			{
				AGolemGearLabPlayerController* Player = LocalPlayer(ContextWorld(Context));
				if (Player == nullptr)
				{
					return;
				}
				Clients.Add(Player);
				Bind(Player);
			}
			Bind(Host.Get());
			for (const FGolemRosterEntry& Entry : Party(Server.Get())->GetRoster())
			{
				Place(Entry.Golem, Mode->GetUpgradeStation()->GetActorLocation() + FVector(60.0, Entry.Slot * 70.0, 100.0));
				const FGolemRPGGolemView View = RPG(Server.Get())->GetRPG()->GetGolemView(Entry.GolemId);
				Test.TestEqual(TEXT("Starter has8equipmentassignments, sockets outside12"), View.Assignments.Num(), 8);
				Test.TestEqual(TEXT("Catalog fixture level8"), View.Level, 8);
				Test.TestTrue(TEXT("Starter H1140 P110 A136 actual GAS"),
					FMath::IsNearlyEqual(Entry.Golem->GetGolemAttributeSet()->GetMaxHealth(), 1140.0f)
					&& FMath::IsNearlyEqual(Entry.Golem->GetGolemAttributeSet()->GetAttackPower(), 110.0f)
					&& FMath::IsNearlyEqual(Entry.Golem->GetGolemAttributeSet()->GetDefense(), 136.0f));
				UGolemGearAppearanceComponent* Appearance = Entry.Golem->FindComponentByClass<UGolemGearAppearanceComponent>();
				Test.TestTrue(TEXT("Paired fittings one slot/two genuine components"), Appearance != nullptr
					&& Appearance->GetVisiblePartCount(TEXT("Shoulders")) == 2 && Appearance->GetVisiblePartCount(TEXT("Feet")) == 2);
			}
			Test.TestTrue(TEXT("Fixture128 capacity and41definitioncatalog"),
				RPG(Server.Get())->GetInventory()->GetView().Capacity == 128 && FGolemGearCatalog::GetSeeds().Num() == 41);
			OwnershipInventory = RPG(Server.Get())->GetInventory()->GetView();
			OwnershipView = RPG(Server.Get())->GetRPG()->GetView();
			const FGolemRosterEntry* Other = Party(Server.Get())->GetRoster().FindByPredicate([this](const FGolemRosterEntry& Entry)
			{
				return Entry.Golem != Host->GetControlledGolem();
			});
			if (!bNetwork)
			{
				FGolemRPGCommand Transfer = Command(Host.Get(), EGolemRPGOperation::TransferEquipment);
				const FGolemRPGGolemView Source = RPG(Server.Get())->GetRPG()->GetGolemView(Transfer.ExpectedGolemId);
				Transfer.SlotTag = GolemGameplayTags::Slot_Equipment_Weapon;
				const FGolemEquipmentAssignment* Assignment = Source.FindAssignment(Transfer.SlotTag);
				if (!Test.TestTrue(TEXT("Transfer source has its confirmed starter weapon"), Assignment != nullptr))
				{
					BeginCleanup();
					return;
				}
				Transfer.ItemInstanceId = Assignment->ItemInstanceId;
				Transfer.TargetGolemId = Other->GolemId;
				Transfer.ExpectedTargetGolemRevision = RPG(Server.Get())->GetRPG()->GetGolemView(Other->GolemId).Revision;
				const FGolemItemInstance* Weapon = OwnershipInventory.Items.FindByPredicate([&Transfer](const FGolemItemInstance& Item)
				{
					return Item.InstanceId == Transfer.ItemInstanceId;
				});
				if (!Test.TestTrue(TEXT("Transfer source resolves its exact shared inventory GUID"), Weapon != nullptr))
				{
					BeginCleanup();
					return;
				}
				Transfer.ExpectedItemRevision = Weapon->ItemRevision;
				const FVector TargetLocation = Other->Golem->GetActorLocation();
				Place(Other->Golem, TargetLocation + FVector(1500.0, 0.0, 0.0));
				const FGolemRPGCommandResult Rejected = Mode->ExecuteRPGCommand(Host.Get(), Transfer);
				Test.TestTrue(TEXT("Companion transfer outside sanctuary rejects before any participant changes"),
					Rejected.Result.Error == EGolemError::InvalidState && Rejected.Result.Context == TEXT("RPGSafeZone")
					&& RPG(Server.Get())->GetInventory()->GetView() == OwnershipInventory
					&& RPG(Server.Get())->GetRPG()->GetView() == OwnershipView);
				Place(Other->Golem, TargetLocation);
			}
			Request = Command(Host.Get(), EGolemRPGOperation::ApplyGearPreset);
			Request.ExpectedGolemId = Other->GolemId;
			Request.ExpectedGolemRevision = RPG(Server.Get())->GetRPG()->GetGolemView(Other->GolemId).Revision;
			Request.PresetId = TEXT("Mend");
			Send(Host.Get(), Request);
			Enter(EPhase::Ownership);
		}

		void WaitOwnership()
		{
			const FGolemRPGCommandResult* Reply = Replies->Values.Find(Request.RequestId);
			if (Reply == nullptr || !Converged())
			{
				return;
			}
			if (bNetwork)
			{
				Test.TestTrue(TEXT("Authenticated host cannot edit another human's equipment"),
					!Reply->Result.IsSuccess() && RPG(Server.Get())->GetInventory()->GetView() == OwnershipInventory
					&& RPG(Server.Get())->GetRPG()->GetView() == OwnershipView);
			}
			else
			{
				Test.TestTrue(TEXT("Authenticated solo human may edit a permitted companion's real equipment"), Reply->Result.IsSuccess());
				AGolemCharacter* Companion = EntryForId(Server.Get(), Request.ExpectedGolemId)->Golem;
				Test.TestTrue(TEXT("Companion receives actual source stats without control transfer"),
					Companion != Host->GetControlledGolem()
					&& FMath::IsNearlyEqual(Companion->GetGolemAttributeSet()->GetAttackPower(), 132.0f));
			}
			Request = Command(Host.Get(), EGolemRPGOperation::ApplyGearPreset);
			Request.PresetId = TEXT("Guard");
			Send(Host.Get(), Request);
			Enter(EPhase::Preset);
		}

		void WaitPreset()
		{
			if (!Success(Request.RequestId) || !Converged())
			{
				return;
			}
			const UGolemAttributeSet* Attributes = Host->GetControlledGolem()->GetGolemAttributeSet();
			Test.TestTrue(TEXT("Guard runtime totals H1380/P128/A197"), FMath::IsNearlyEqual(Attributes->GetMaxHealth(), 1380.0f)
				&& FMath::IsNearlyEqual(Attributes->GetAttackPower(), 128.0f) && FMath::IsNearlyEqual(Attributes->GetDefense(), 197.0f));
			if (!bNetwork)
			{
				SendRune();
				return;
			}
			const FGolemItemInstance* FoundItem = FindItem(TEXT("W04"));
			if (!Test.TestNotNull(TEXT("Catalog race owns an inventory W04"), FoundItem))
			{
				return;
			}
			const FGolemItemInstance Item = *FoundItem;
			RaceItem = Item.InstanceId;
			for (const TWeakObjectPtr<AGolemGearLabPlayerController>& Player : Clients)
			{
				FGolemRPGCommand Equip = Command(Player.Get(), EGolemRPGOperation::Equip);
				Equip.ItemInstanceId = Item.InstanceId;
				Equip.ExpectedItemRevision = Item.ItemRevision;
				Equip.SlotTag = GolemGameplayTags::Slot_Equipment_Weapon;
				RaceRequests.Add(Equip);
				Send(Player.Get(), Equip);
			}
			Enter(EPhase::Race);
		}

		void WaitRace()
		{
			if (!Replies->Values.Contains(RaceRequests[0].RequestId) || !Replies->Values.Contains(RaceRequests[1].RequestId)
				|| !Converged())
			{
				return;
			}
			int32 Winners = 0;
			for (int32 Index = 0; Index < 2; ++Index)
			{
				if (Replies->Values[RaceRequests[Index].RequestId].Result.IsSuccess())
				{
					++Winners;
					Winner = Clients[Index];
					WinningRequest = RaceRequests[Index];
				}
			}
			Test.TestEqual(TEXT("Two real client RPCs: sameGUID equipped once"), Winners, 1);
			if (!Winner.IsValid())
			{
				BeginCleanup();
				return;
			}
			Replies->Values.Remove(WinningRequest.RequestId);
			Send(Winner.Get(), WinningRequest);
			Enter(EPhase::Replay);
		}

		void WaitReplay()
		{
			if (!Success(WinningRequest.RequestId))
			{
				return;
			}
			Test.TestTrue(TEXT("Historical exact replay returns original receipt"), Replies->Values[WinningRequest.RequestId].bWasReplay);
			SendRune();
		}

		void SendRune()
		{
			const FGolemRPGGolemView Golem = RPG(Server.Get())->GetRPG()->GetGolemView(Host->GetControlledGolem()->GetGolemEntityId());
			const FGolemEquipmentAssignment* Assignment = Golem.FindAssignment(GolemGameplayTags::Slot_Equipment_Weapon);
			const FGolemItemInstance* FoundRune = FindItem(TEXT("R01"));
			if (!Test.TestNotNull(TEXT("Guard owns an equipped weapon"), Assignment)
				|| !Test.TestNotNull(TEXT("Catalog owns an available R01"), FoundRune))
			{
				return;
			}
			WeaponId = Assignment->ItemInstanceId;
			const FGolemItemInstance Rune = *FoundRune;
			RuneId = Rune.InstanceId;
			const TArray<FGolemItemInstance> Items = RPG(Server.Get())->GetInventory()->GetView().Items;
			const FGolemItemInstance* Weapon = Items.FindByPredicate([this](const FGolemItemInstance& Item)
			{
				return Item.InstanceId == WeaponId;
			});
			if (!Test.TestNotNull(TEXT("Rune parent weapon exists"), Weapon))
			{
				return;
			}
			Request = Command(Host.Get(), EGolemRPGOperation::AttachWeaponRune);
			Request.ItemInstanceId = WeaponId;
			Request.ExpectedItemRevision = Weapon->ItemRevision;
			Request.WeaponRuneInstanceId = RuneId;
			Send(Host.Get(), Request);
			Enter(EPhase::Rune);
		}

		void WaitRune()
		{
			if (!Success(Request.RequestId) || !Converged())
			{
				return;
			}
			const TArray<FGolemItemInstance> Items = RPG(Server.Get())->GetInventory()->GetView().Items;
			const FGolemItemInstance* Weapon = Items.FindByPredicate([this](const FGolemItemInstance& Item)
			{
				return Item.InstanceId == WeaponId;
			});
			const FGolemItemInstance* Rune = Items.FindByPredicate([this](const FGolemItemInstance& Item)
			{
				return Item.InstanceId == RuneId;
			});
			if (!Test.TestNotNull(TEXT("Committed parent weapon exists"), Weapon)
				|| !Test.TestNotNull(TEXT("Committed Rune exists"), Rune))
			{
				return;
			}
			Test.TestTrue(TEXT("Rune exactparent exclusive location"), Weapon->RuneAttachmentId == RuneId
				&& Rune->ParentWeaponId == WeaponId && Rune->LocationId == GolemGear::WeaponSocketLocation);
			Request = Command(Host.Get(), EGolemRPGOperation::UpgradeItem);
			Request.ItemInstanceId = WeaponId;
			Request.ExpectedItemRevision = Weapon->ItemRevision;
			CurrencyBefore = RPG(Server.Get())->GetRPG()->GetView().Currency;
			Send(Host.Get(), Request);
			Enter(EPhase::Upgrade);
		}

		void WaitUpgrade()
		{
			if (!Success(Request.RequestId) || !Converged())
			{
				return;
			}
			const TArray<FGolemItemInstance> Items = RPG(Server.Get())->GetInventory()->GetView().Items;
			const FGolemItemInstance* Weapon = Items.FindByPredicate([this](const FGolemItemInstance& Item)
			{
				return Item.InstanceId == WeaponId;
			});
			Test.TestTrue(TEXT("Upgrade exactGUID/rank1/Rune and fixedcurrency20"), Weapon->UpgradeRank == 1
				&& Weapon->RuneAttachmentId == RuneId && RPG(Server.Get())->GetRPG()->GetView().Currency == CurrencyBefore - 20);
			Test.TestTrue(TEXT("Upgraded W02 P20/A28 applied once"),
				FMath::IsNearlyEqual(Host->GetControlledGolem()->GetGolemAttributeSet()->GetAttackPower(), 130.0f)
				&& FMath::IsNearlyEqual(Host->GetControlledGolem()->GetGolemAttributeSet()->GetDefense(), 200.0f));
			FGolemGearRewardBundle Bundle;
			Bundle.RewardId = FGuid::NewGuid();
			Bundle.DemoRunId = RPG(Server.Get())->GetRPG()->GetView().DemoRunId;
			Bundle.EncounterId = Mode->GetLabEncounterId();
			Bundle.Currency = 7;
			Bundle.Level = 8;
			FGolemGearRewardItem Reward;
			Reward.DefinitionId = FGolemGearCatalog::DefinitionId(TEXT("C01"));
			Reward.Count = 1;
			Bundle.Items.Add(Reward);
			Test.TestTrue(TEXT("Real fixture reward registered"), RPG(Server.Get())->GetRPG()->RegisterRewardBundle(Bundle).IsSuccess());
			RewardId = Bundle.RewardId;
			CurrencyBefore = RPG(Server.Get())->GetRPG()->GetView().Currency;
			RaceRequests.Reset();
			const TArray<TWeakObjectPtr<AGolemGearLabPlayerController>> Claimants = bNetwork ? Clients
				: TArray<TWeakObjectPtr<AGolemGearLabPlayerController>>{Host, Host};
			for (const TWeakObjectPtr<AGolemGearLabPlayerController>& Player : Claimants)
			{
				FGolemRPGCommand Claim = Command(Player.Get(), EGolemRPGOperation::ClaimReward);
				Claim.RewardId = RewardId;
				RaceRequests.Add(Claim);
				Send(Player.Get(), Claim);
			}
			Enter(EPhase::RewardRace);
		}

		void WaitRewardRace()
		{
			if (!Replies->Values.Contains(RaceRequests[0].RequestId) || !Replies->Values.Contains(RaceRequests[1].RequestId)
				|| !Converged())
			{
				return;
			}
			const FGolemRPGView View = RPG(Server.Get())->GetRPG()->GetView();
			Test.TestTrue(TEXT("Reward two-request race currency/receipt exactlyonce"), View.Currency == CurrencyBefore + 7
				&& View.ClaimedRewards.Contains(RewardId));
			const int32 Successes = (Replies->Values[RaceRequests[0].RequestId].Result.IsSuccess() ? 1 : 0)
				+ (Replies->Values[RaceRequests[1].RequestId].Result.IsSuccess() ? 1 : 0);
			Test.TestEqual(TEXT("One reward claim committed"), Successes, 1);
			AGolemGearLabPlayerController* Consumer = bNetwork ? Clients[0].Get() : Host.Get();
			const FGolemEntityId Id = Consumer->GetControlledGolem()->GetGolemEntityId();
			TransferSlot = EntryForId(Server.Get(), Id)->Slot;
			TransferGolem = EntryForId(Server.Get(), Id)->Golem;
			TransferGolem->GetGolemAbilitySystem()->SetNumericAttributeBase(UGolemAttributeSet::GetHealthAttribute(), 300.0f);
			QuickConsumer = Consumer;
			QuickRequest = Command(Consumer, EGolemRPGOperation::UseConsumable);
			QuickRequest.QuickSlot = 0;
			QuickInputStage = bNetwork ? 0 : 1;
			QuickInputStarted = Server->GetTimeSeconds();
			SetQuickKey(bNetwork ? EKeys::Gamepad_Special_Left : EKeys::One, true);
			Enter(EPhase::Consumable);
		}

		void SetQuickKey(const FKey& Key, const bool bPressed)
		{
			if (QuickConsumer.IsValid())
			{
				FScopedConditionalWorldSwitcher Switch(QuickConsumer->GetWorld());
				QuickConsumer->InputKey(FInputKeyEventArgs::CreateSimulated(Key,
					bPressed ? IE_Pressed : IE_Released, bPressed ? 1.0f : 0.0f));
			}
		}

		void ReleaseQuickInput()
		{
			SetQuickKey(EKeys::One, false);
			SetQuickKey(EKeys::Gamepad_DPad_Down, false);
			SetQuickKey(EKeys::Gamepad_Special_Left, false);
			QuickConsumer.Reset();
		}

		void WaitConsumable()
		{
			if (QuickConsumer.IsValid() && Server->GetTimeSeconds() - QuickInputStarted >= 0.10)
			{
				if (QuickInputStage == 0)
				{
					SetQuickKey(EKeys::Gamepad_DPad_Down, true);
					QuickInputStage = 1;
					QuickInputStarted = Server->GetTimeSeconds();
				}
				else
				{
					ReleaseQuickInput();
				}
			}
			UGolemSkillRuntimeComponent* Runtime = TransferGolem->FindComponentByClass<UGolemSkillRuntimeComponent>();
			if (QuickConsumer.IsValid() || Runtime->GetSkillState().ConsumableEnd <= Server->GetTimeSeconds() || !Converged())
			{
				return;
			}
			ConsumableEnd = Runtime->GetSkillState().ConsumableEnd;
			for (const TPair<FGuid, FGolemRPGCommandResult>& Entry : Replies->Values)
			{
				if (Entry.Value.Operation == EGolemRPGOperation::UseConsumable
					&& Entry.Value.GolemId == TransferGolem->GetGolemEntityId() && Entry.Value.Result.IsSuccess())
				{
					QuickRequest.RequestId = Entry.Key;
					break;
				}
			}
			TransferView = RPG(Server.Get())->GetRPG()->GetGolemView(TransferGolem->GetGolemEntityId());
			Test.TestTrue(bNetwork ? TEXT("Mapped View+DPad Down heals 20percent max and commits shared30sCD")
				: TEXT("Mapped keyboard1 heals 20percent max and commits shared30sCD"),
				FMath::IsNearlyEqual(TransferGolem->GetGolemAttributeSet()->GetHealth(),
					300.0f + 0.20f * TransferGolem->GetGolemAttributeSet()->GetMaxHealth(), 0.01f));
			if (bNetwork)
			{
				DepartingContext = ClientContexts[0];
				const TSharedPtr<const FInternetAddr> Address = Server->GetNetDriver()->GetLocalAddr();
				ReconnectURL = FString::Printf(TEXT("127.0.0.1:%d?GolemSlot=%d"), Address->GetPort(), TransferSlot);
				FScopedConditionalWorldSwitcher Switch(Clients[0]->GetWorld());
				Clients[0]->ClientTravel(DetachedMap, TRAVEL_Absolute);
				Enter(EPhase::Disconnect);
			}
			else
			{
				FScopedConditionalWorldSwitcher Switch(Server.Get());
				Host->RequestSwitchGolem((TransferSlot + 1) % 3);
				Enter(EPhase::SoloSwitch);
			}
		}

		bool TransferPreserved() const
		{
			return TransferGolem.IsValid() && RPG(Server.Get())->GetRPG()->GetGolemView(TransferGolem->GetGolemEntityId()) == TransferView
				&& FMath::IsNearlyEqual(TransferGolem->FindComponentByClass<UGolemSkillRuntimeComponent>()->GetSkillState().ConsumableEnd,
					ConsumableEnd);
		}

		void WaitDisconnect()
		{
			UWorld* Detached = ContextWorld(DepartingContext);
			if (Detached == nullptr || Detached->GetNetMode() != NM_Standalone || Detached->GetNetDriver() != nullptr
				|| Party(Server.Get())->GetHumanCount() != 2
				|| Cast<AAIController>(TransferGolem->GetController()) == nullptr)
			{
				return;
			}
			Test.TestTrue(TEXT("Disconnect AI takeover samecanonical gear/CD"), TransferPreserved());
			FScopedConditionalWorldSwitcher Switch(Detached);
			Detached->GetFirstPlayerController()->ClientTravel(ReconnectURL, TRAVEL_Absolute);
			Enter(EPhase::Rejoin);
		}

		void WaitRejoin()
		{
			AGolemGearLabPlayerController* Player = LocalPlayer(ContextWorld(DepartingContext));
			if (Player == nullptr || Player->GetWorld()->GetNetMode() != NM_Client || Party(Server.Get())->GetHumanCount() != 3
				|| !Converged() || Player->GetControlledGolem()->GetGolemEntityId() != TransferView.GolemId)
			{
				return;
			}
			Test.TestTrue(TEXT("Same-slot JIP preserves gear/source/CD snapshot"), TransferPreserved());
			Test.AddInfo(TEXT("Actual listen server +2clients, 50-100ms packet simulation: equip race, exact replay, parentRune upgrade, reward race, potion, disconnect/AI takeover/JIP and whole-state convergence."));
			BeginCleanup();
		}

		void WaitSoloSwitch()
		{
			if (Host->GetControlledGolem() == TransferGolem.Get())
			{
				return;
			}
			Test.TestTrue(TEXT("Solo human->AI canonical gear/consumable CD preserved"), TransferPreserved());
			const FGolemInventoryView BeforeInventory = RPG(Server.Get())->GetInventory()->GetView();
			const FGolemRPGView BeforeRPG = RPG(Server.Get())->GetRPG()->GetView();
			const float BeforeHealth = TransferGolem->GetGolemAttributeSet()->GetHealth();
			const FGolemRPGCommandResult Replayed = Mode->ExecuteRPGCommand(Host.Get(), QuickRequest);
			Test.TestTrue(TEXT("Authenticated exact potion replay after switch returns the original receipt without mutation"),
				Replayed.Result.IsSuccess() && Replayed.bWasReplay
				&& RPG(Server.Get())->GetInventory()->GetView() == BeforeInventory
				&& RPG(Server.Get())->GetRPG()->GetView() == BeforeRPG
				&& FMath::IsNearlyEqual(TransferGolem->GetGolemAttributeSet()->GetHealth(), BeforeHealth)
				&& TransferPreserved());
			QuickRequest.RequestId = FGuid::NewGuid();
			const FGolemRPGCommandResult Fresh = Mode->ExecuteRPGCommand(Host.Get(), QuickRequest);
			Test.TestTrue(TEXT("A fresh potion request cannot use a companion after switching control"),
				Fresh.Result.Error == EGolemError::NotOwned
				&& RPG(Server.Get())->GetInventory()->GetView() == BeforeInventory
				&& RPG(Server.Get())->GetRPG()->GetView() == BeforeRPG && TransferPreserved());
			Test.AddInfo(TEXT("Actual solo CatalogLab: starter/preset actual GAS, exact parentRune upgrade, atomic reward and potion, human/AI takeover with retained CD."));
			BeginCleanup();
		}

		void Unbind()
		{
			for (int32 Index = 0; Index < Handles.Num(); ++Index)
			{
				if (ObservedPlayers[Index].IsValid())
				{
					ObservedPlayers[Index]->OnRPGCommandResult.Remove(Handles[Index]);
				}
			}
			Handles.Reset();
			ObservedPlayers.Reset();
		}

		void BeginCleanup()
		{
			if (Phase >= EPhase::Cleanup)
			{
				return;
			}
			ReleaseQuickInput();
			Unbind();
			if (bOwnSession && GEditor != nullptr)
			{
				GEditor->RequestEndPlayMap();
			}
			Enter(EPhase::Cleanup);
		}

		void RestoreSettings()
		{
			if (Settings.IsValid())
			{
				ULevelEditorPlaySettings* Defaults = GetMutableDefault<ULevelEditorPlaySettings>();
				Defaults->MultipleInstancePositions = OriginalPositions;
				Defaults->LastSize = OriginalSize;
				Defaults->NewWindowPosition = OriginalPosition;
				Defaults->NewWindowWidth = OriginalWidth;
				Defaults->NewWindowHeight = OriginalHeight;
				Defaults->SaveConfig();
			}
		}

		void Cleanup()
		{
			if (GEditor->IsPlaySessionInProgress())
			{
				if (FPlatformTime::Seconds() - PhaseStarted > 15.0 && GEditor->IsPlayingSessionInEditor())
				{
					GEditor->EndPlayMap();
				}
				return;
			}
			RestoreSettings();
			Settings.Reset();
			bOwnSession = false;
			Enter(EPhase::Complete);
		}

		FAutomationTestBase& Test;
		bool bNetwork = false;
		bool bOwnSession = false;
		EPhase Phase = EPhase::Start;
		double Started = 0.0;
		double PhaseStarted = 0.0;
		TStrongObjectPtr<ULevelEditorPlaySettings> Settings;
		TArray<FIntPoint> OriginalPositions;
		FIntPoint OriginalSize;
		FIntPoint OriginalPosition;
		int32 OriginalWidth = 0;
		int32 OriginalHeight = 0;
		TWeakObjectPtr<UWorld> Server;
		TWeakObjectPtr<AGolemGearLabGameMode> Mode;
		TWeakObjectPtr<AGolemGearLabPlayerController> Host;
		TArray<TWeakObjectPtr<AGolemGearLabPlayerController>> Clients;
		TArray<FName> ClientContexts;
		TArray<TWeakObjectPtr<AGolemGearLabPlayerController>> ObservedPlayers;
		TArray<FDelegateHandle> Handles;
		TSharedPtr<FReplies> Replies;
		TSet<FGuid> CheckedReplies;
		mutable TArray<FGolemItemInstance> SearchItems;
		FGolemRPGCommand Request;
		FGolemInventoryView OwnershipInventory;
		FGolemRPGView OwnershipView;
		TArray<FGolemRPGCommand> RaceRequests;
		FGolemRPGCommand WinningRequest;
		FGolemRPGCommand QuickRequest;
		TWeakObjectPtr<AGolemGearLabPlayerController> Winner;
		FGuid RaceItem;
		FGuid WeaponId;
		FGuid RuneId;
		FGuid RewardId;
		int64 CurrencyBefore = 0;
		TWeakObjectPtr<AGolemCharacter> TransferGolem;
		TWeakObjectPtr<AGolemGearLabPlayerController> QuickConsumer;
		int32 QuickInputStage = 0;
		double QuickInputStarted = 0.0;
		FGolemRPGGolemView TransferView;
		int32 TransferSlot = INDEX_NONE;
		double ConsumableEnd = 0.0;
		FName DepartingContext;
		FString ReconnectURL;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGolemGearAuthoredAssetTest, "Golem.M56.Catalog.AuthoredAssets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGolemGearAuthoredAssetTest::RunTest(const FString& Parameters)
{
	UGolemAssetManager* Manager = UGolemAssetManager::GetGolemAssetManager();
	TArray<UGolemItemDefinition*> Definitions;
	for (const FGolemGearSeed& Seed : FGolemGearCatalog::GetSeeds())
	{
		UGolemDefinition* Resolved = nullptr;
		if (!TestTrue(*Seed.CatalogId.ToString(), Manager != nullptr && Manager->ResolveDefinition(Seed.DefinitionId,
			UGolemItemDefinition::StaticClass(), Resolved).IsSuccess()))
		{
			continue;
		}
		UGolemItemDefinition* Definition = CastChecked<UGolemItemDefinition>(Resolved);
		TestTrue(TEXT("Cold asset reload stats match seed"), Definition->GetStatBonuses() == Seed.Stats);
		Definitions.Add(Definition);
	}
	TestTrue(TEXT("41serialized valid mappeddefinitions"), FGolemGearCatalog::ValidateCatalog(Definitions).IsSuccess());
	TestTrue(TEXT("Genuine GearLab map exists"), FPackageName::DoesPackageExist(GolemGearLabIntegration::GearMap));
	TestTrue(TEXT("Genuine CatalogLab map exists"), FPackageName::DoesPackageExist(GolemGearLabIntegration::CatalogMap));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGolemGearSoloIntegrationTest, "Golem.M56.Integration.SoloGearAndTakeover",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGolemGearSoloIntegrationTest::RunTest(const FString& Parameters)
{
	ADD_LATENT_AUTOMATION_COMMAND(GolemGearLabIntegration::FGearNetworkCommand(*this, false));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGolemGearNetworkIntegrationTest, "Golem.M56.Integration.ListenClientsRaceAndJIP",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGolemGearNetworkIntegrationTest::RunTest(const FString& Parameters)
{
	ADD_LATENT_AUTOMATION_COMMAND(GolemGearLabIntegration::FGearNetworkCommand(*this, true));
	return true;
}
#endif
