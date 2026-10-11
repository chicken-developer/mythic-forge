#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "AIController.h"
#include "Assets/KnightAssetManager.h"
#include "Attributes/KnightAttributeSet.h"
#include "Catalog/KnightGearCatalog.h"
#include "Character/KnightCharacter.h"
#include "Components/KnightAbilitySystemComponent.h"
#include "Components/KnightLifeComponent.h"
#include "Definitions/KnightItemDefinition.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/KnightGameState.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GearLab/KnightGearAppearanceComponent.h"
#include "GearLab/KnightGearLabGameMode.h"
#include "GearLab/KnightGearLabPlayerController.h"
#include "GearLab/KnightGearLabState.h"
#include "HAL/PlatformTime.h"
#include "IPAddress.h"
#include "Inventory/KnightInventoryComponent.h"
#include "InputCoreTypes.h"
#include "InputKeyEventArgs.h"
#include "Misc/PackageName.h"
#include "Party/KnightPartyState.h"
#include "PlayInEditorDataTypes.h"
#include "Progression/KnightRPGComponent.h"
#include "RPGLoop/KnightRPGGameState.h"
#include "RPGLoop/KnightRPGStateActor.h"
#include "RPGLoop/KnightRPGUpgradeStation.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Skills/KnightSkillRuntimeComponent.h"
#include "Tags/KnightGameplayTags.h"
#include "UnrealEngine.h"
#include "UObject/StrongObjectPtr.h"

namespace KnightGearLabIntegration
{
	const TCHAR* CatalogMap = TEXT("/Game/Knight/Maps/Playables/L_M05_6_CatalogLab");
	const TCHAR* GearMap = TEXT("/Game/Knight/Maps/Playables/L_M05_6_GearLab");
	const TCHAR* DetachedMap = TEXT("/Game/Knight/Maps/Playables/L_M00_Boot");

	AKnightPartyState* Party(UWorld* World)
	{
		const AKnightGameState* State = World != nullptr ? World->GetGameState<AKnightGameState>() : nullptr;
		return State != nullptr ? State->GetPartyState() : nullptr;
	}

	AKnightRPGStateActor* RPG(UWorld* World)
	{
		const AKnightRPGGameState* State = World != nullptr ? World->GetGameState<AKnightRPGGameState>() : nullptr;
		return State != nullptr ? State->GetRPGState() : nullptr;
	}

	const FKnightRosterEntry* EntryForId(UWorld* World, const FKnightEntityId& Id)
	{
		AKnightPartyState* State = Party(World);
		return State != nullptr ? State->GetRoster().FindByPredicate([&Id](const FKnightRosterEntry& Entry)
		{
			return Entry.KnightId == Id;
		}) : nullptr;
	}

	AKnightGearLabPlayerController* LocalPlayer(UWorld* World)
	{
		if (World != nullptr)
		{
			for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
			{
				AKnightGearLabPlayerController* Player = Cast<AKnightGearLabPlayerController>(It->Get());
				if (Player != nullptr && Player->IsLocalController() && Player->GetControlledKnight() != nullptr)
				{
					return Player;
				}
			}
		}
		return nullptr;
	}

	void Place(AKnightCharacter* Knight, const FVector& Point)
	{
		Knight->SetActorLocation(Point, false, nullptr, ETeleportType::TeleportPhysics);
		Knight->GetCharacterMovement()->StopMovementImmediately();
	}

	struct FReplies
	{
		TMap<FGuid, FKnightRPGCommandResult> Values;
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

		void Bind(AKnightGearLabPlayerController* Player)
		{
			const TWeakPtr<FReplies> Weak(Replies);
			Handles.Add(Player->OnRPGCommandResult.AddLambda([Weak](const FKnightRPGCommandResult& Result)
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
			const FKnightInventoryView ServerInventory = RPG(Server.Get())->GetInventory()->GetView();
			const FKnightRPGView ServerView = RPG(Server.Get())->GetRPG()->GetView();
			Test.AddInfo(FString::Printf(TEXT("Gear server: humans=%d clientContexts=%d inv=%lld items=%d bag=%d capacity=%d RPG=%lld invref=%lld Knights=%d currency=%lld demo=%d."),
				Party(Server.Get()) != nullptr ? Party(Server.Get())->GetHumanCount() : -1, ClientContexts.Num(),
				ServerInventory.Revision, ServerInventory.Items.Num(), ServerInventory.GetBagCount(), ServerInventory.Capacity,
				ServerView.Revision, ServerView.InventoryRevision, ServerView.Knights.Num(), ServerView.Currency, ServerView.bGearDemo));
			for (const FName Context : ClientContexts)
			{
				UWorld* ClientWorld = ContextWorld(Context);
				AKnightRPGStateActor* State = RPG(ClientWorld);
				if (State == nullptr)
				{
					Test.AddInfo(FString::Printf(TEXT("Gear client %s: RPG state is unavailable."), *Context.ToString()));
					continue;
				}
				const FKnightInventoryView ClientInventory = State->GetInventory()->GetView();
				const FKnightRPGView ClientView = State->GetRPG()->GetView();
				Test.AddInfo(FString::Printf(TEXT("Gear client %s: netMode=%d invMatch=%d RPGMatch=%d inv=%lld items=%d bag=%d capacity=%d RPG=%lld invref=%lld Knights=%d currency=%lld demo=%d."),
					*Context.ToString(), static_cast<int32>(ClientWorld->GetNetMode()), ClientInventory == ServerInventory,
					ClientView == ServerView, ClientInventory.Revision, ClientInventory.Items.Num(), ClientInventory.GetBagCount(),
					ClientInventory.Capacity, ClientView.Revision, ClientView.InventoryRevision, ClientView.Knights.Num(),
					ClientView.Currency, ClientView.bGearDemo));
			}
		}

		FKnightRPGCommand Command(AKnightGearLabPlayerController* Player, EKnightRPGOperation Operation) const
		{
			const FKnightInventoryView Inventory = RPG(Player->GetWorld())->GetInventory()->GetView();
			const FKnightEntityId Id = Player->GetControlledKnight()->GetKnightEntityId();
			FKnightRPGCommand Result;
			Result.Operation = Operation;
			Result.RequestId = FGuid::NewGuid();
			Result.ExpectedKnightId = Id;
			Result.InventoryId = Inventory.InventoryId;
			Result.ExpectedInventoryRevision = Inventory.Revision;
			Result.ExpectedKnightRevision = RPG(Player->GetWorld())->GetRPG()->GetKnightView(Id).Revision;
			return Result;
		}

		void Send(AKnightGearLabPlayerController* Player, const FKnightRPGCommand& InCommand)
		{
			FScopedConditionalWorldSwitcher Switch(Player->GetWorld());
			Player->RequestRPGCommand(InCommand);
		}

		bool Success(const FGuid& Id)
		{
			const FKnightRPGCommandResult* Result = Replies->Values.Find(Id);
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

		const FKnightItemInstance* FindItem(FName CatalogId, bool bBag = true) const
		{
			SearchItems = RPG(Server.Get())->GetInventory()->GetView().Items;
			return SearchItems.FindByPredicate([CatalogId, bBag](const FKnightItemInstance& Item)
			{
				return Item.DefinitionId == FKnightGearCatalog::DefinitionId(CatalogId)
					&& (!bBag || Item.LocationId == KnightGear::InventoryLocation);
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
			Mode = Server.IsValid() ? Server->GetAuthGameMode<AKnightGearLabGameMode>() : nullptr;
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
				AKnightGearLabPlayerController* Player = LocalPlayer(ContextWorld(Context));
				if (Player == nullptr)
				{
					return;
				}
				Clients.Add(Player);
				Bind(Player);
			}
			Bind(Host.Get());
			for (const FKnightRosterEntry& Entry : Party(Server.Get())->GetRoster())
			{
				Place(Entry.Knight, Mode->GetUpgradeStation()->GetActorLocation() + FVector(60.0, Entry.Slot * 70.0, 100.0));
				const FKnightRPGKnightView View = RPG(Server.Get())->GetRPG()->GetKnightView(Entry.KnightId);
				Test.TestEqual(TEXT("Starter has8equipmentassignments, sockets outside12"), View.Assignments.Num(), 8);
				Test.TestEqual(TEXT("Catalog fixture level8"), View.Level, 8);
				Test.TestTrue(TEXT("Starter H1140 P110 A136 actual GAS"),
					FMath::IsNearlyEqual(Entry.Knight->GetKnightAttributeSet()->GetMaxHealth(), 1140.0f)
					&& FMath::IsNearlyEqual(Entry.Knight->GetKnightAttributeSet()->GetAttackPower(), 110.0f)
					&& FMath::IsNearlyEqual(Entry.Knight->GetKnightAttributeSet()->GetDefense(), 136.0f));
				UKnightGearAppearanceComponent* Appearance = Entry.Knight->FindComponentByClass<UKnightGearAppearanceComponent>();
				Test.TestTrue(TEXT("Paired fittings one slot/two genuine components"), Appearance != nullptr
					&& Appearance->GetVisiblePartCount(TEXT("Shoulders")) == 2 && Appearance->GetVisiblePartCount(TEXT("Feet")) == 2);
			}
			Test.TestTrue(TEXT("Fixture128 capacity and41definitioncatalog"),
				RPG(Server.Get())->GetInventory()->GetView().Capacity == 128 && FKnightGearCatalog::GetSeeds().Num() == 41);
			OwnershipInventory = RPG(Server.Get())->GetInventory()->GetView();
			OwnershipView = RPG(Server.Get())->GetRPG()->GetView();
			const FKnightRosterEntry* Other = Party(Server.Get())->GetRoster().FindByPredicate([this](const FKnightRosterEntry& Entry)
			{
				return Entry.Knight != Host->GetControlledKnight();
			});
			if (!bNetwork)
			{
				FKnightRPGCommand Transfer = Command(Host.Get(), EKnightRPGOperation::TransferEquipment);
				const FKnightRPGKnightView Source = RPG(Server.Get())->GetRPG()->GetKnightView(Transfer.ExpectedKnightId);
				Transfer.SlotTag = KnightGameplayTags::Slot_Equipment_Weapon;
				const FKnightEquipmentAssignment* Assignment = Source.FindAssignment(Transfer.SlotTag);
				if (!Test.TestTrue(TEXT("Transfer source has its confirmed starter weapon"), Assignment != nullptr))
				{
					BeginCleanup();
					return;
				}
				Transfer.ItemInstanceId = Assignment->ItemInstanceId;
				Transfer.TargetKnightId = Other->KnightId;
				Transfer.ExpectedTargetKnightRevision = RPG(Server.Get())->GetRPG()->GetKnightView(Other->KnightId).Revision;
				const FKnightItemInstance* Weapon = OwnershipInventory.Items.FindByPredicate([&Transfer](const FKnightItemInstance& Item)
				{
					return Item.InstanceId == Transfer.ItemInstanceId;
				});
				if (!Test.TestTrue(TEXT("Transfer source resolves its exact shared inventory GUID"), Weapon != nullptr))
				{
					BeginCleanup();
					return;
				}
				Transfer.ExpectedItemRevision = Weapon->ItemRevision;
				const FVector TargetLocation = Other->Knight->GetActorLocation();
				Place(Other->Knight, TargetLocation + FVector(1500.0, 0.0, 0.0));
				const FKnightRPGCommandResult Rejected = Mode->ExecuteRPGCommand(Host.Get(), Transfer);
				Test.TestTrue(TEXT("Companion transfer outside sanctuary rejects before any participant changes"),
					Rejected.Result.Error == EKnightError::InvalidState && Rejected.Result.Context == TEXT("RPGSafeZone")
					&& RPG(Server.Get())->GetInventory()->GetView() == OwnershipInventory
					&& RPG(Server.Get())->GetRPG()->GetView() == OwnershipView);
				Place(Other->Knight, TargetLocation);
			}
			Request = Command(Host.Get(), EKnightRPGOperation::ApplyGearPreset);
			Request.ExpectedKnightId = Other->KnightId;
			Request.ExpectedKnightRevision = RPG(Server.Get())->GetRPG()->GetKnightView(Other->KnightId).Revision;
			Request.PresetId = TEXT("Mend");
			Send(Host.Get(), Request);
			Enter(EPhase::Ownership);
		}

		void WaitOwnership()
		{
			const FKnightRPGCommandResult* Reply = Replies->Values.Find(Request.RequestId);
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
				AKnightCharacter* Companion = EntryForId(Server.Get(), Request.ExpectedKnightId)->Knight;
				Test.TestTrue(TEXT("Companion receives actual source stats without control transfer"),
					Companion != Host->GetControlledKnight()
					&& FMath::IsNearlyEqual(Companion->GetKnightAttributeSet()->GetAttackPower(), 132.0f));
			}
			Request = Command(Host.Get(), EKnightRPGOperation::ApplyGearPreset);
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
			const UKnightAttributeSet* Attributes = Host->GetControlledKnight()->GetKnightAttributeSet();
			Test.TestTrue(TEXT("Guard runtime totals H1380/P128/A197"), FMath::IsNearlyEqual(Attributes->GetMaxHealth(), 1380.0f)
				&& FMath::IsNearlyEqual(Attributes->GetAttackPower(), 128.0f) && FMath::IsNearlyEqual(Attributes->GetDefense(), 197.0f));
			if (!bNetwork)
			{
				SendRune();
				return;
			}
			const FKnightItemInstance* FoundItem = FindItem(TEXT("W04"));
			if (!Test.TestNotNull(TEXT("Catalog race owns an inventory W04"), FoundItem))
			{
				return;
			}
			const FKnightItemInstance Item = *FoundItem;
			RaceItem = Item.InstanceId;
			for (const TWeakObjectPtr<AKnightGearLabPlayerController>& Player : Clients)
			{
				FKnightRPGCommand Equip = Command(Player.Get(), EKnightRPGOperation::Equip);
				Equip.ItemInstanceId = Item.InstanceId;
				Equip.ExpectedItemRevision = Item.ItemRevision;
				Equip.SlotTag = KnightGameplayTags::Slot_Equipment_Weapon;
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
			const FKnightRPGKnightView Knight = RPG(Server.Get())->GetRPG()->GetKnightView(Host->GetControlledKnight()->GetKnightEntityId());
			const FKnightEquipmentAssignment* Assignment = Knight.FindAssignment(KnightGameplayTags::Slot_Equipment_Weapon);
			const FKnightItemInstance* FoundRune = FindItem(TEXT("R01"));
			if (!Test.TestNotNull(TEXT("Guard owns an equipped weapon"), Assignment)
				|| !Test.TestNotNull(TEXT("Catalog owns an available R01"), FoundRune))
			{
				return;
			}
			WeaponId = Assignment->ItemInstanceId;
			const FKnightItemInstance Rune = *FoundRune;
			RuneId = Rune.InstanceId;
			const TArray<FKnightItemInstance> Items = RPG(Server.Get())->GetInventory()->GetView().Items;
			const FKnightItemInstance* Weapon = Items.FindByPredicate([this](const FKnightItemInstance& Item)
			{
				return Item.InstanceId == WeaponId;
			});
			if (!Test.TestNotNull(TEXT("Rune parent weapon exists"), Weapon))
			{
				return;
			}
			Request = Command(Host.Get(), EKnightRPGOperation::AttachWeaponRune);
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
			const TArray<FKnightItemInstance> Items = RPG(Server.Get())->GetInventory()->GetView().Items;
			const FKnightItemInstance* Weapon = Items.FindByPredicate([this](const FKnightItemInstance& Item)
			{
				return Item.InstanceId == WeaponId;
			});
			const FKnightItemInstance* Rune = Items.FindByPredicate([this](const FKnightItemInstance& Item)
			{
				return Item.InstanceId == RuneId;
			});
			if (!Test.TestNotNull(TEXT("Committed parent weapon exists"), Weapon)
				|| !Test.TestNotNull(TEXT("Committed Rune exists"), Rune))
			{
				return;
			}
			Test.TestTrue(TEXT("Rune exactparent exclusive location"), Weapon->RuneAttachmentId == RuneId
				&& Rune->ParentWeaponId == WeaponId && Rune->LocationId == KnightGear::WeaponSocketLocation);
			Request = Command(Host.Get(), EKnightRPGOperation::UpgradeItem);
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
			const TArray<FKnightItemInstance> Items = RPG(Server.Get())->GetInventory()->GetView().Items;
			const FKnightItemInstance* Weapon = Items.FindByPredicate([this](const FKnightItemInstance& Item)
			{
				return Item.InstanceId == WeaponId;
			});
			Test.TestTrue(TEXT("Upgrade exactGUID/rank1/Rune and fixedcurrency20"), Weapon->UpgradeRank == 1
				&& Weapon->RuneAttachmentId == RuneId && RPG(Server.Get())->GetRPG()->GetView().Currency == CurrencyBefore - 20);
			Test.TestTrue(TEXT("Upgraded W02 P20/A28 applied once"),
				FMath::IsNearlyEqual(Host->GetControlledKnight()->GetKnightAttributeSet()->GetAttackPower(), 130.0f)
				&& FMath::IsNearlyEqual(Host->GetControlledKnight()->GetKnightAttributeSet()->GetDefense(), 200.0f));
			FKnightGearRewardBundle Bundle;
			Bundle.RewardId = FGuid::NewGuid();
			Bundle.DemoRunId = RPG(Server.Get())->GetRPG()->GetView().DemoRunId;
			Bundle.EncounterId = Mode->GetLabEncounterId();
			Bundle.Currency = 7;
			Bundle.Level = 8;
			FKnightGearRewardItem Reward;
			Reward.DefinitionId = FKnightGearCatalog::DefinitionId(TEXT("C01"));
			Reward.Count = 1;
			Bundle.Items.Add(Reward);
			Test.TestTrue(TEXT("Real fixture reward registered"), RPG(Server.Get())->GetRPG()->RegisterRewardBundle(Bundle).IsSuccess());
			RewardId = Bundle.RewardId;
			CurrencyBefore = RPG(Server.Get())->GetRPG()->GetView().Currency;
			RaceRequests.Reset();
			const TArray<TWeakObjectPtr<AKnightGearLabPlayerController>> Claimants = bNetwork ? Clients
				: TArray<TWeakObjectPtr<AKnightGearLabPlayerController>>{Host, Host};
			for (const TWeakObjectPtr<AKnightGearLabPlayerController>& Player : Claimants)
			{
				FKnightRPGCommand Claim = Command(Player.Get(), EKnightRPGOperation::ClaimReward);
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
			const FKnightRPGView View = RPG(Server.Get())->GetRPG()->GetView();
			Test.TestTrue(TEXT("Reward two-request race currency/receipt exactlyonce"), View.Currency == CurrencyBefore + 7
				&& View.ClaimedRewards.Contains(RewardId));
			const int32 Successes = (Replies->Values[RaceRequests[0].RequestId].Result.IsSuccess() ? 1 : 0)
				+ (Replies->Values[RaceRequests[1].RequestId].Result.IsSuccess() ? 1 : 0);
			Test.TestEqual(TEXT("One reward claim committed"), Successes, 1);
			AKnightGearLabPlayerController* Consumer = bNetwork ? Clients[0].Get() : Host.Get();
			const FKnightEntityId Id = Consumer->GetControlledKnight()->GetKnightEntityId();
			TransferSlot = EntryForId(Server.Get(), Id)->Slot;
			TransferKnight = EntryForId(Server.Get(), Id)->Knight;
			TransferKnight->GetKnightAbilitySystem()->SetNumericAttributeBase(UKnightAttributeSet::GetHealthAttribute(), 300.0f);
			QuickConsumer = Consumer;
			QuickRequest = Command(Consumer, EKnightRPGOperation::UseConsumable);
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
			UKnightSkillRuntimeComponent* Runtime = TransferKnight->FindComponentByClass<UKnightSkillRuntimeComponent>();
			if (QuickConsumer.IsValid() || Runtime->GetSkillState().ConsumableEnd <= Server->GetTimeSeconds() || !Converged())
			{
				return;
			}
			ConsumableEnd = Runtime->GetSkillState().ConsumableEnd;
			for (const TPair<FGuid, FKnightRPGCommandResult>& Entry : Replies->Values)
			{
				if (Entry.Value.Operation == EKnightRPGOperation::UseConsumable
					&& Entry.Value.KnightId == TransferKnight->GetKnightEntityId() && Entry.Value.Result.IsSuccess())
				{
					QuickRequest.RequestId = Entry.Key;
					break;
				}
			}
			TransferView = RPG(Server.Get())->GetRPG()->GetKnightView(TransferKnight->GetKnightEntityId());
			Test.TestTrue(bNetwork ? TEXT("Mapped View+DPad Down heals 20percent max and commits shared30sCD")
				: TEXT("Mapped keyboard1 heals 20percent max and commits shared30sCD"),
				FMath::IsNearlyEqual(TransferKnight->GetKnightAttributeSet()->GetHealth(),
					300.0f + 0.20f * TransferKnight->GetKnightAttributeSet()->GetMaxHealth(), 0.01f));
			if (bNetwork)
			{
				DepartingContext = ClientContexts[0];
				const TSharedPtr<const FInternetAddr> Address = Server->GetNetDriver()->GetLocalAddr();
				ReconnectURL = FString::Printf(TEXT("127.0.0.1:%d?KnightSlot=%d"), Address->GetPort(), TransferSlot);
				FScopedConditionalWorldSwitcher Switch(Clients[0]->GetWorld());
				Clients[0]->ClientTravel(DetachedMap, TRAVEL_Absolute);
				Enter(EPhase::Disconnect);
			}
			else
			{
				FScopedConditionalWorldSwitcher Switch(Server.Get());
				Host->RequestSwitchKnight((TransferSlot + 1) % 3);
				Enter(EPhase::SoloSwitch);
			}
		}

		bool TransferPreserved() const
		{
			return TransferKnight.IsValid() && RPG(Server.Get())->GetRPG()->GetKnightView(TransferKnight->GetKnightEntityId()) == TransferView
				&& FMath::IsNearlyEqual(TransferKnight->FindComponentByClass<UKnightSkillRuntimeComponent>()->GetSkillState().ConsumableEnd,
					ConsumableEnd);
		}

		void WaitDisconnect()
		{
			UWorld* Detached = ContextWorld(DepartingContext);
			if (Detached == nullptr || Detached->GetNetMode() != NM_Standalone || Detached->GetNetDriver() != nullptr
				|| Party(Server.Get())->GetHumanCount() != 2
				|| Cast<AAIController>(TransferKnight->GetController()) == nullptr)
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
			AKnightGearLabPlayerController* Player = LocalPlayer(ContextWorld(DepartingContext));
			if (Player == nullptr || Player->GetWorld()->GetNetMode() != NM_Client || Party(Server.Get())->GetHumanCount() != 3
				|| !Converged() || Player->GetControlledKnight()->GetKnightEntityId() != TransferView.KnightId)
			{
				return;
			}
			Test.TestTrue(TEXT("Same-slot JIP preserves gear/source/CD snapshot"), TransferPreserved());
			Test.AddInfo(TEXT("Actual listen server +2clients, 50-100ms packet simulation: equip race, exact replay, parentRune upgrade, reward race, potion, disconnect/AI takeover/JIP and whole-state convergence."));
			BeginCleanup();
		}

		void WaitSoloSwitch()
		{
			if (Host->GetControlledKnight() == TransferKnight.Get())
			{
				return;
			}
			Test.TestTrue(TEXT("Solo human->AI canonical gear/consumable CD preserved"), TransferPreserved());
			const FKnightInventoryView BeforeInventory = RPG(Server.Get())->GetInventory()->GetView();
			const FKnightRPGView BeforeRPG = RPG(Server.Get())->GetRPG()->GetView();
			const float BeforeHealth = TransferKnight->GetKnightAttributeSet()->GetHealth();
			const FKnightRPGCommandResult Replayed = Mode->ExecuteRPGCommand(Host.Get(), QuickRequest);
			Test.TestTrue(TEXT("Authenticated exact potion replay after switch returns the original receipt without mutation"),
				Replayed.Result.IsSuccess() && Replayed.bWasReplay
				&& RPG(Server.Get())->GetInventory()->GetView() == BeforeInventory
				&& RPG(Server.Get())->GetRPG()->GetView() == BeforeRPG
				&& FMath::IsNearlyEqual(TransferKnight->GetKnightAttributeSet()->GetHealth(), BeforeHealth)
				&& TransferPreserved());
			QuickRequest.RequestId = FGuid::NewGuid();
			const FKnightRPGCommandResult Fresh = Mode->ExecuteRPGCommand(Host.Get(), QuickRequest);
			Test.TestTrue(TEXT("A fresh potion request cannot use a companion after switching control"),
				Fresh.Result.Error == EKnightError::NotOwned
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
		TWeakObjectPtr<AKnightGearLabGameMode> Mode;
		TWeakObjectPtr<AKnightGearLabPlayerController> Host;
		TArray<TWeakObjectPtr<AKnightGearLabPlayerController>> Clients;
		TArray<FName> ClientContexts;
		TArray<TWeakObjectPtr<AKnightGearLabPlayerController>> ObservedPlayers;
		TArray<FDelegateHandle> Handles;
		TSharedPtr<FReplies> Replies;
		TSet<FGuid> CheckedReplies;
		mutable TArray<FKnightItemInstance> SearchItems;
		FKnightRPGCommand Request;
		FKnightInventoryView OwnershipInventory;
		FKnightRPGView OwnershipView;
		TArray<FKnightRPGCommand> RaceRequests;
		FKnightRPGCommand WinningRequest;
		FKnightRPGCommand QuickRequest;
		TWeakObjectPtr<AKnightGearLabPlayerController> Winner;
		FGuid RaceItem;
		FGuid WeaponId;
		FGuid RuneId;
		FGuid RewardId;
		int64 CurrencyBefore = 0;
		TWeakObjectPtr<AKnightCharacter> TransferKnight;
		TWeakObjectPtr<AKnightGearLabPlayerController> QuickConsumer;
		int32 QuickInputStage = 0;
		double QuickInputStarted = 0.0;
		FKnightRPGKnightView TransferView;
		int32 TransferSlot = INDEX_NONE;
		double ConsumableEnd = 0.0;
		FName DepartingContext;
		FString ReconnectURL;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKnightGearAuthoredAssetTest, "Knight.M56.Catalog.AuthoredAssets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FKnightGearAuthoredAssetTest::RunTest(const FString& Parameters)
{
	UKnightAssetManager* Manager = UKnightAssetManager::GetKnightAssetManager();
	TArray<UKnightItemDefinition*> Definitions;
	for (const FKnightGearSeed& Seed : FKnightGearCatalog::GetSeeds())
	{
		UKnightDefinition* Resolved = nullptr;
		if (!TestTrue(*Seed.CatalogId.ToString(), Manager != nullptr && Manager->ResolveDefinition(Seed.DefinitionId,
			UKnightItemDefinition::StaticClass(), Resolved).IsSuccess()))
		{
			continue;
		}
		UKnightItemDefinition* Definition = CastChecked<UKnightItemDefinition>(Resolved);
		TestTrue(TEXT("Cold asset reload stats match seed"), Definition->GetStatBonuses() == Seed.Stats);
		Definitions.Add(Definition);
	}
	TestTrue(TEXT("41serialized valid mappeddefinitions"), FKnightGearCatalog::ValidateCatalog(Definitions).IsSuccess());
	TestTrue(TEXT("Genuine GearLab map exists"), FPackageName::DoesPackageExist(KnightGearLabIntegration::GearMap));
	TestTrue(TEXT("Genuine CatalogLab map exists"), FPackageName::DoesPackageExist(KnightGearLabIntegration::CatalogMap));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKnightGearSoloIntegrationTest, "Knight.M56.Integration.SoloGearAndTakeover",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FKnightGearSoloIntegrationTest::RunTest(const FString& Parameters)
{
	ADD_LATENT_AUTOMATION_COMMAND(KnightGearLabIntegration::FGearNetworkCommand(*this, false));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKnightGearNetworkIntegrationTest, "Knight.M56.Integration.ListenClientsRaceAndJIP",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FKnightGearNetworkIntegrationTest::RunTest(const FString& Parameters)
{
	ADD_LATENT_AUTOMATION_COMMAND(KnightGearLabIntegration::FGearNetworkCommand(*this, true));
	return true;
}
#endif
