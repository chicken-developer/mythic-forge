#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Abilities/GameplayAbility.h"
#include "Abilities/GolemHeavyAttackAbility.h"
#include "Abilities/GolemLightAttackAbility.h"
#include "AIController.h"
#include "AbilityYard/GolemAbilityDummy.h"
#include "Attributes/GolemAttributeSet.h"
#include "Blueprint/WidgetTree.h"
#include "Character/GolemCharacter.h"
#include "Commands/GolemRPGCommand.h"
#include "Components/CanvasPanel.h"
#include "Components/GolemAbilitySystemComponent.h"
#include "Components/GolemLifeComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Events/GolemEventSubsystem.h"
#include "Events/GolemSkillLabEvent.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/SlateUser.h"
#include "Framework/GolemGameState.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayAbilitySpec.h"
#include "HAL/PlatformTime.h"
#include "IPAddress.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "InputKeyEventArgs.h"
#include "Inventory/GolemInventoryComponent.h"
#include "Misc/PackageName.h"
#include "Party/GolemPartyState.h"
#include "PartyArena/GolemArenaEnemy.h"
#include "PartyArena/GolemPartyArenaStartMarker.h"
#include "PartyArena/GolemPartyArenaState.h"
#include "PlayInEditorDataTypes.h"
#include "Player/GolemPlayerState.h"
#include "Progression/GolemRPGComponent.h"
#include "RPGLoop/GolemRPGGameState.h"
#include "RPGLoop/GolemRPGStateActor.h"
#include "RPGLoop/GolemRPGUIPlayerController.h"
#include "RPGLoop/GolemRPGUpgradeStation.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "SkillLab/GolemSkillLabGameMode.h"
#include "SkillLab/GolemSkillLabPlayerController.h"
#include "SkillLab/GolemRunestone.h"
#include "SkillLab/GolemRunestonePickup.h"
#include "Skills/GolemSkillRuntimeComponent.h"
#include "Skills/GolemSkillTypes.h"
#include "UnrealEngine.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/GolemRPGActionButton.h"
#include "Widgets/GolemRPGPanel.h"

namespace GolemSkillLabIntegration
{
	const TCHAR* MapPath = TEXT("/Game/Golem/Maps/Playables/L_M05_5_SkillLab");
	const TCHAR* DetachedMap = TEXT("/Game/Golem/Maps/Playables/L_M00_Boot");
	constexpr uint16 ServerPort = 17955;

	enum class EPhase : uint8
	{
		Start, Ready, Sanctuary, PanelOpen, PanelClose, Build, PanelResyncOpen, PanelResyncClose, Stale, RaceSpecs,
		RaceRune, CrystalApproach, CrystalChannel, CrystalPickupApproach, CrystalPickupRace,
		Wave, Skills, PadSkill, PadHeavy, SwitchAway, SwitchBack, Disconnect, Rejoin, Cleanup, Complete
	};

	struct FGolemBuildReceiptAttributes
	{
		FGolemEntityId GolemId;
		float Stamina = 0.0f;
		float MaxStamina = 0.0f;
		double ServerTime = 0.0;
		bool bAuthority = false;
		bool bSynchronous = false;
	};

	struct FObservation
	{
		TMap<FGuid, FGolemRPGCommandResult> Replies;
		TMap<FGuid, TWeakObjectPtr<AGolemSkillLabPlayerController>> Recipients;
		TMap<FGuid, FGolemBuildReceiptAttributes> BuildReceiptAttributes;
		TSet<FGuid> SendingRequests;
		TMap<FGolemEntityId, TArray<int32>> Commits;
		TMap<FGolemEntityId, int32> HeavyCommits;
		TMap<FGolemEntityId, int32> LightCommits;
		TArray<FGolemSkillLabEvent> WorldEvents;
	};

	AGolemPartyState* FindParty(UWorld* World)
	{
		const AGolemGameState* State = World != nullptr ? World->GetGameState<AGolemGameState>() : nullptr;
		return State != nullptr ? State->GetPartyState() : nullptr;
	}

	AGolemRPGStateActor* FindRPG(UWorld* World)
	{
		const AGolemRPGGameState* State = World != nullptr ? World->GetGameState<AGolemRPGGameState>() : nullptr;
		return State != nullptr ? State->GetRPGState() : nullptr;
	}

	TArray<FGameplayAbilitySpecHandle> Specs(const UGolemAbilitySystemComponent* ASC)
	{
		TArray<FGameplayAbilitySpecHandle> Result;
		for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
		{
			Result.Add(Spec.Handle);
		}
		return Result;
	}

	/** Genuine map/PIE/RPC/UI/input gate. Explicit station/combat placement isolates the exercised contracts. */
	class FSkillLabCommand final : public IAutomationLatentCommand
	{
	public:
		FSkillLabCommand(FAutomationTestBase& InTest, bool bInNetwork)
			: Test(InTest), bNetwork(bInNetwork), Observation(MakeShared<FObservation>()),
			RunStarted(FPlatformTime::Seconds()), PhaseStarted(RunStarted)
		{
		}

		virtual ~FSkillLabCommand() override
		{
			Unbind();
			ReleaseKey();
			ReleaseInteractions();
			ReleasePadModifier();
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
			if (Phase < EPhase::Cleanup && (Test.HasAnyErrors() ||
				FPlatformTime::Seconds() - RunStarted > 180.0 ||
				Elapsed() > (Phase == EPhase::Ready || Phase == EPhase::Rejoin ? 60.0 : 25.0)))
			{
				if (!Test.HasAnyErrors())
				{
					Fail(FString::Printf(TEXT("SkillLab PIE timeout phase=%d elapsed=%.2f replies=%d %s"),
						static_cast<int32>(Phase), Elapsed(), Observation->Replies.Num(), *SkillDiagnostics()));
				}
				else
				{
					BeginCleanup();
				}
			}
			if (Server.IsValid() && PulseKey.IsValid() && Server->GetTimeSeconds() - PulseTime >= 0.1)
			{
				ReleaseKey();
			}
			if (Server.IsValid() && !HeldInteractPlayers.IsEmpty() &&
				Server->GetTimeSeconds() - InteractPulseTime >= 0.1)
			{
				ReleaseInteractions();
			}
			if (Phase > EPhase::Ready && Phase < EPhase::Cleanup && !HasCanonicalIdentities())
			{
				Fail(TEXT("SkillLab replaced a canonical actor, hero, ID or Character ASC owner/avatar."));
			}
			switch (Phase)
			{
			case EPhase::Start: Start(); break;
			case EPhase::Ready: Ready(); break;
			case EPhase::Sanctuary: Sanctuary(); break;
			case EPhase::PanelOpen: PanelOpen(); break;
			case EPhase::PanelClose: PanelClose(); break;
			case EPhase::Build: WaitBuild(); break;
			case EPhase::PanelResyncOpen: WaitPanelResyncOpen(); break;
			case EPhase::PanelResyncClose: WaitPanelResyncClose(); break;
			case EPhase::Stale: WaitStale(); break;
			case EPhase::RaceSpecs: WaitSpecRace(); break;
			case EPhase::RaceRune: WaitRuneRace(); break;
			case EPhase::CrystalApproach: WaitCrystalApproach(); break;
			case EPhase::CrystalChannel: WaitCrystalChannel(); break;
			case EPhase::CrystalPickupApproach: WaitCrystalPickupApproach(); break;
			case EPhase::CrystalPickupRace: WaitCrystalPickupRace(); break;
			case EPhase::Wave: WaitWave(); break;
			case EPhase::Skills: WaitSkills(); break;
			case EPhase::PadSkill: WaitPadSkill(); break;
			case EPhase::PadHeavy: WaitPadHeavy(); break;
			case EPhase::SwitchAway: WaitSwitchAway(); break;
			case EPhase::SwitchBack: WaitSwitchBack(); break;
			case EPhase::Disconnect: WaitDisconnect(); break;
			case EPhase::Rejoin: WaitRejoin(); break;
			case EPhase::Cleanup: Cleanup(); break;
			default: break;
			}
			return Phase == EPhase::Complete;
		}

	private:
		double Elapsed() const
		{
			return FPlatformTime::Seconds() - PhaseStarted;
		}
		void Enter(EPhase Next)
		{
			Phase = Next;
			PhaseStarted = FPlatformTime::Seconds();
			Test.AddInfo(FString::Printf(TEXT("SkillLab actual PIE phase %d."), static_cast<int32>(Next)));
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
		AGolemSkillLabPlayerController* LocalOwner(UWorld* World) const
		{
			if (World == nullptr || FindParty(World) == nullptr)
			{
				return nullptr;
			}
			for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
			{
				AGolemSkillLabPlayerController* Player = Cast<AGolemSkillLabPlayerController>(It->Get());
				if (Player != nullptr && Player->IsLocalController() && Player->GetControlledGolem() != nullptr &&
					Player->GetControlledGolem()->IsLocallyControlled() &&
					FindParty(World)->GetControlledSlot(Player) != INDEX_NONE)
				{
					return Player;
				}
			}
			return nullptr;
		}
		void Start()
		{
			if (GEditor == nullptr || GEngine == nullptr || GEditor->IsPlaySessionInProgress() ||
				!FPackageName::DoesPackageExist(MapPath) || !FPackageName::DoesPackageExist(DetachedMap))
			{
				Fail(TEXT("SkillLab integration requires an idle Editor and genuine SkillLab/M0 bootstrap maps."));
				return;
			}
			const ULevelEditorPlaySettings* Defaults = GetDefault<ULevelEditorPlaySettings>();
			OriginalPositions = Defaults->MultipleInstancePositions;
			OriginalLastSize = Defaults->LastSize;
			OriginalPosition = Defaults->NewWindowPosition;
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
			Settings->SetClientWindowSize(bNetwork ? FIntPoint(640, 360) : FIntPoint(1280, 720));
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
			Request.GlobalMapOverride = MapPath;
			Request.bAllowOnlineSubsystem = false;
			GEditor->RequestPlaySession(Request);
			bOwnsSession = true;
			Enter(EPhase::Ready);
		}
		void BindPlayer(AGolemSkillLabPlayerController* Player)
		{
			const TWeakPtr<FObservation> Weak(Observation);
			const TWeakObjectPtr<AGolemSkillLabPlayerController> WeakPlayer(Player);
			ReplyHandles.Add(Player->OnRPGCommandResult.AddLambda(
				[Weak, WeakPlayer](const FGolemRPGCommandResult& Reply)
				{
					if (const TSharedPtr<FObservation> Current = Weak.Pin())
					{
						Current->Replies.Add(Reply.RequestId, Reply);
						Current->Recipients.Add(Reply.RequestId, WeakPlayer);
						AGolemCharacter* Golem = WeakPlayer.IsValid() ? WeakPlayer->GetControlledGolem() : nullptr;
						if (Reply.Operation == EGolemRPGOperation::ApplyBuild && Golem != nullptr &&
							Golem->GetGolemAttributeSet() != nullptr && Golem->GetWorld() != nullptr)
						{
							FGolemBuildReceiptAttributes Attributes;
							Attributes.GolemId = Golem->GetGolemEntityId();
							Attributes.Stamina = Golem->GetGolemAttributeSet()->GetStamina();
							Attributes.MaxStamina = Golem->GetGolemAttributeSet()->GetMaxStamina();
							Attributes.ServerTime = Golem->GetWorld()->GetTimeSeconds();
							Attributes.bAuthority = Golem->HasAuthority();
							Attributes.bSynchronous = Current->SendingRequests.Contains(Reply.RequestId);
							Current->BuildReceiptAttributes.Add(Reply.RequestId, Attributes);
						}
					}
				}));
			ObservedPlayers.Add(Player);
		}
		void Ready()
		{
			ClientContexts.Reset();
			for (const FWorldContext& Context : GEngine->GetWorldContexts())
			{
				UWorld* World = Context.World();
				if (Context.WorldType != EWorldType::PIE || World == nullptr)
				{
					continue;
				}
				if (World->GetNetMode() == (bNetwork ? NM_ListenServer : NM_Standalone))
				{
					Server = World;
				}
				else if (bNetwork && World->GetNetMode() == NM_Client)
				{
					ClientContexts.Add(Context.ContextHandle);
				}
			}
			Mode = Server.IsValid() ? Server->GetAuthGameMode<AGolemSkillLabGameMode>() : nullptr;
			AGolemPartyState* Party = FindParty(Server.Get());
			if (!Mode.IsValid() || Party == nullptr || !Party->IsCanonicalRosterReady() ||
				Party->GetHumanCount() != (bNetwork ? 3 : 1) ||
				!Mode->GetRPGInitializationResult().IsSuccess() || Mode->GetRPGState() == nullptr ||
				ClientContexts.Num() != (bNetwork ? 2 : 0))
			{
				return;
			}
			Host = LocalOwner(Server.Get());
			if (!Host.IsValid())
			{
				return;
			}
			Players.Reset();
			if (bNetwork)
			{
				for (FName Context : ClientContexts)
				{
					AGolemSkillLabPlayerController* Player = LocalOwner(ContextWorld(Context));
					if (Player == nullptr)
					{
						return;
					}
					Players.Add(Player);
				}
#if DO_ENABLE_NET_TEST
				if (Server->GetNetDriver() == nullptr ||
					Server->GetNetDriver()->PacketSimulationSettings.PktLagMin != 50 ||
					Server->GetNetDriver()->PacketSimulationSettings.PktLagMax != 100)
				{
					Fail(TEXT("SkillLab actual NetDriver did not apply 50-100 ms outgoing latency."));
					return;
				}
#else
				Fail(TEXT("This build cannot verify actual network emulation."));
				return;
#endif
			}
			else
			{
				Players.Add(Host);
			}
			RPG = Mode->GetRPGState();
			if (!Converged())
			{
				return;
			}
			for (int32 Index = 0; Index < 3; ++Index)
			{
				AGolemCharacter* Golem = Party->FindSlot(Index)->Golem;
				UGolemAbilitySystemComponent* ASC = Golem->GetGolemAbilitySystem();
				UGolemSkillRuntimeComponent* Runtime = Golem->FindComponentByClass<UGolemSkillRuntimeComponent>();
				if (Runtime == nullptr || Runtime->GetBuild().SpecId.IsNone() ||
					!FMath::IsNearlyEqual(Golem->GetGolemAttributeSet()->GetMaxHealth(), 1000.0f) ||
					!FMath::IsNearlyEqual(Golem->GetGolemAttributeSet()->GetAttackPower(), 100.0f) ||
					!FMath::IsNearlyEqual(Golem->GetGolemAttributeSet()->GetDefense(), 100.0f))
				{
					Fail(TEXT("Genuine SkillLab canonical attributes/kit do not match 1000 HP / 100 Power / 100 Armor."));
					return;
				}
				Golems.Add(Golem);
				Ids.Add(Golem->GetGolemEntityId());
				Heroes.Add(Golem->GetHeroDefinitionId());
				TArray<int32> Counts;
				Counts.Init(0, 4);
				Observation->Commits.Add(Golem->GetGolemEntityId(), Counts);
				Observation->HeavyCommits.Add(Golem->GetGolemEntityId(), 0);
				Observation->LightCommits.Add(Golem->GetGolemEntityId(), 0);
				const TWeakPtr<FObservation> Weak(Observation);
				const TWeakObjectPtr<UGolemSkillRuntimeComponent> WeakRuntime(Runtime);
				const FGolemEntityId Id = Golem->GetGolemEntityId();
				CommitHandles.Add(ASC->AbilityCommittedCallbacks.AddLambda(
					[Weak, WeakRuntime, Id](UGameplayAbility* Ability)
					{
						const TSharedPtr<FObservation> Current = Weak.Pin();
						if (!Current.IsValid() || !WeakRuntime.IsValid() || Ability == nullptr)
						{
							return;
						}
						if (Ability->IsA<UGolemHeavyAttackAbility>())
						{
							++Current->HeavyCommits.FindChecked(Id);
						}
						if (Ability->IsA<UGolemLightAttackAbility>())
						{
							++Current->LightCommits.FindChecked(Id);
						}
						for (int32 Action = 1; Action <= 4; ++Action)
						{
							if (WeakRuntime->GetSkillHandle(Action) == Ability->GetCurrentAbilitySpecHandle())
							{
								++Current->Commits.FindChecked(Id)[Action - 1];
							}
						}
					}));
			}
			for (const TWeakObjectPtr<AGolemSkillLabPlayerController>& Player : Players)
			{
				BindPlayer(Player.Get());
			}
			OriginalSpecHandles = Specs(Host->GetControlledGolem()->GetGolemAbilitySystem());
			UGolemEventSubsystem* Events = Server->GetSubsystem<UGolemEventSubsystem>();
			if (Events == nullptr)
			{
				Fail(TEXT("Actual SkillLab confirmed event adapter bus is missing."));
				return;
			}
			const TWeakPtr<FObservation> WeakObservation(Observation);
			WorldEventSubscription = Events->Subscribe<FGolemSkillLabEvent>(Mode.Get(),
				[WeakObservation](const FGolemSkillLabEvent& Event)
				{
					if (const TSharedPtr<FObservation> Current = WeakObservation.Pin())
					{
						Current->WorldEvents.Add(Event);
					}
				});
			BaseCaptureCount = CaptureCount();
			const FVector Station = Mode->GetUpgradeStation()->GetActorLocation();
			for (int32 Index = 0; Index < Golems.Num(); ++Index)
			{
				Place(Golems[Index].Get(), Station + FVector((Index - 1) * 130.0, -100.0, 100.0));
			}
			Enter(EPhase::Sanctuary);
		}
		bool HasCanonicalIdentities() const
		{
			AGolemPartyState* Party = FindParty(Server.Get());
			if (Party == nullptr || Golems.Num() != 3)
			{
				return false;
			}
			int32 Count = 0;
			for (TActorIterator<AGolemCharacter> It(Server.Get()); It; ++It)
			{
				++Count;
			}
			if (Count != 3)
			{
				return false;
			}
			for (int32 Index = 0; Index < 3; ++Index)
			{
				const FGolemRosterEntry* Entry = Party->FindSlot(Index);
				if (!Golems[Index].IsValid() || Entry == nullptr || Entry->Golem != Golems[Index].Get() ||
					Entry->GolemId != Ids[Index] || Entry->HeroDefinitionId != Heroes[Index] ||
					Golems[Index]->GetGolemAbilitySystem()->GetOwnerActor() != Golems[Index].Get() ||
					Golems[Index]->GetGolemAbilitySystem()->GetAvatarActor() != Golems[Index].Get())
				{
					return false;
				}
			}
			return true;
		}
		bool Converged() const
		{
			if (!RPG.IsValid())
			{
				return false;
			}
			for (FName Context : ClientContexts)
			{
				UWorld* World = ContextWorld(Context);
				AGolemRPGStateActor* Other = FindRPG(World);
				AGolemPartyState* Party = FindParty(World);
				if (World == nullptr || World->GetNetMode() != NM_Client || Other == nullptr || Party == nullptr ||
					!Party->IsCanonicalRosterReady() || !(Other->GetRPG()->GetView() == RPG->GetRPG()->GetView()) ||
					!(Other->GetInventory()->GetView() == RPG->GetInventory()->GetView()))
				{
					return false;
				}
				for (TActorIterator<AAIController> It(World); It; ++It)
				{
					return false;
				}
				for (const FGolemRosterEntry& Entry : Party->GetRoster())
				{
					UGolemSkillRuntimeComponent* Runtime = Entry.Golem != nullptr ?
						Entry.Golem->FindComponentByClass<UGolemSkillRuntimeComponent>() : nullptr;
					if (Runtime == nullptr || !(Runtime->GetBuild() == Other->GetRPG()->GetGolemView(Entry.GolemId).Build))
					{
						return false;
					}
				}
			}
			return true;
		}
		void Place(AGolemCharacter* Golem, const FVector& Position)
		{
			FScopedConditionalWorldSwitcher Switch(Server.Get());
			Golem->GetCharacterMovement()->StopMovementImmediately();
			Golem->SetActorLocationAndRotation(Position, FRotator::ZeroRotator, false, nullptr, ETeleportType::TeleportPhysics);
			Golem->ForceNetUpdate();
		}
		AGolemCharacter* AuthorityGolem(AGolemSkillLabPlayerController* Player) const
		{
			if (Player == nullptr || Player->GetControlledGolem() == nullptr)
			{
				return nullptr;
			}
			const FGolemEntityId Id = Player->GetControlledGolem()->GetGolemEntityId();
			for (const TWeakObjectPtr<AGolemCharacter>& Golem : Golems)
			{
				if (Golem.IsValid() && Golem->GetGolemEntityId() == Id)
				{
					return Golem.Get();
				}
			}
			return nullptr;
		}
		FGolemRPGCommand CommandFor(AGolemSkillLabPlayerController* Player) const
		{
			AGolemRPGStateActor* Source = FindRPG(Player->GetWorld());
			FGolemRPGCommand Command;
			Command.Operation = EGolemRPGOperation::ApplyBuild;
			Command.RequestId = FGuid::NewGuid();
			Command.ExpectedGolemId = Player->GetControlledGolem()->GetGolemEntityId();
			Command.InventoryId = Source->GetInventory()->GetView().InventoryId;
			Command.ExpectedInventoryRevision = Source->GetInventory()->GetView().Revision;
			const FGolemRPGGolemView Golem = Source->GetRPG()->GetGolemView(Command.ExpectedGolemId);
			Command.ExpectedGolemRevision = Golem.Revision;
			Command.ProposedBuild = Golem.Build;
			Command.WeaponRuneInstanceId = Golem.WeaponRuneInstanceId;
			Command.SkillRuneInstanceId = Golem.SkillRuneInstanceId;
			return Command;
		}
		void Send(AGolemSkillLabPlayerController* Player, const FGolemRPGCommand& Command)
		{
			FScopedConditionalWorldSwitcher Switch(Player->GetWorld());
			Observation->SendingRequests.Add(Command.RequestId);
			Player->RequestRPGCommand(Command);
			Observation->SendingRequests.Remove(Command.RequestId);
		}
		bool ReplyOwned(const FGolemRPGCommand& Command, AGolemSkillLabPlayerController* Expected) const
		{
			const TWeakObjectPtr<AGolemSkillLabPlayerController>* Actual = Observation->Recipients.Find(Command.RequestId);
			return Actual != nullptr && Actual->Get() == Expected;
		}
		void Sanctuary()
		{
			if (Elapsed() < 0.6 || !Converged())
			{
				return;
			}
			for (const TWeakObjectPtr<AGolemSkillLabPlayerController>& Player : Players)
			{
				if (!Mode->IsInUpgradeSafeZone(AuthorityGolem(Player.Get())))
				{
					Fail(TEXT("Explicit SkillLab transaction fixture is not inside the actual sanctuary."));
					return;
				}
			}
			if (!bNetwork)
			{
				Pulse(Host.Get(), EKeys::I);
				Enter(EPhase::PanelOpen);
				return;
			}
			Race.Reset();
			for (const TWeakObjectPtr<AGolemSkillLabPlayerController>& Player : Players)
			{
				FGolemRPGCommand Command = CommandFor(Player.Get());
				Command.ProposedBuild.SpecId = FGolemSkillCatalog::SpecsForHero(
					AuthorityGolem(Player.Get())->GetHeroDefinitionId())[1];
				Command.ProposedBuild.ClassChoice = TEXT("Reservoir");
				Race.Add(Command);
			}
			for (int32 Index = 0; Index < Race.Num(); ++Index)
			{
				Send(Players[Index].Get(), Race[Index]);
			}
			Enter(EPhase::RaceSpecs);
		}
		int32 CaptureCount() const
		{
			int32 Count = 0;
			for (TActorIterator<AActor> It(Server.Get()); It; ++It)
			{
				if (It->FindComponentByClass<USceneCaptureComponent2D>() != nullptr)
				{
					++Count;
				}
			}
			return Count;
		}
		bool HasPanelFocus() const
		{
			UGolemRPGPanel* Panel = Host->GetRPGPanel();
			return Panel != nullptr && Panel->IsActivated() && FSlateApplication::IsInitialized() &&
				(Panel->HasUserFocus(Host.Get()) || Panel->HasUserFocusedDescendants(Host.Get()));
		}
		void SlateKey(FKey Key)
		{
			ULocalPlayer* Local = Host->GetLocalPlayer();
			const TSharedPtr<FSlateUser> User = Local != nullptr ? Local->GetSlateUser() : nullptr;
			if (!User.IsValid() || !HasPanelFocus())
			{
				Fail(TEXT("SkillLab CommonUI has no actual local Slate focus."));
				return;
			}
			FScopedConditionalWorldSwitcher Switch(Server.Get());
			const FKeyEvent Event(Key, FModifierKeysState(), User->GetUserIndex(), false, 0, 0);
			if (!FSlateApplication::Get().ProcessKeyDownEvent(Event))
			{
				Fail(TEXT("SkillLab actual focused CommonUI did not handle Back."));
				return;
			}
			FSlateApplication::Get().ProcessKeyUpEvent(Event);
		}
		void PanelOpen()
		{
			if (PulseKey.IsValid() || !HasPanelFocus())
			{
				return;
			}
			UGolemRPGPanel* Panel = Host->GetRPGPanel();
			const FGolemRPGPanelSnapshot& Snapshot = Panel->GetSnapshot();
			if (Snapshot.GolemId != Host->GetControlledGolem()->GetGolemEntityId() ||
				Snapshot.Specializations.Num() != 3 || Snapshot.StatRows.Num() < 10 ||
				Snapshot.Runes.Num() != 4 || !Snapshot.bCanEditBuild || CaptureCount() != BaseCaptureCount + 1)
			{
				Fail(TEXT("Actual Character UI lacks confirmed canonical data, three specs, stats, four rune items or one preview."));
				return;
			}
			if (OpenCycles == 0)
			{
				const FGolemRPGRuneRow* Aftershock = Snapshot.Runes.FindByPredicate([](const FGolemRPGRuneRow& Rune)
					{
						return Rune.RuneId == TEXT("Weapon.Aftershock");
					});
				if (Aftershock == nullptr || Aftershock->bCompatible || Aftershock->IncompatibilityReason.IsEmpty())
				{
					Fail(TEXT("Confirmed Rune UI offered Aftershock without an equipped M5 heavy hammer."));
					return;
				}
				for (int32 Tab = 0; Tab < 5; ++Tab)
				{
					Panel->SetActiveTab(static_cast<EGolemRPGPanelTab>(Tab));
					int32 CanvasCount = 0;
					int32 ButtonCount = 0;
					Panel->WidgetTree->ForEachWidget([&CanvasCount, &ButtonCount](UWidget* Widget)
					{
						CanvasCount += Cast<UCanvasPanel>(Widget) != nullptr ? 1 : 0;
						ButtonCount += Cast<UGolemRPGActionButton>(Widget) != nullptr ? 1 : 0;
					});
					if ((Tab == static_cast<int32>(EGolemRPGPanelTab::Talents) && CanvasCount != 2) || ButtonCount < 7)
					{
						Fail(TEXT("SkillLab native tabs are missing their CommonButtons or both talent node canvases."));
						return;
					}
				}
				Panel->SetActiveTab(EGolemRPGPanelTab::Character);
				// Mapping is suppressed while the actual CommonUI input layer is active.
				const int32 Before = Observation->Commits.FindChecked(Snapshot.GolemId)[3];
				Host->RequestSkillSlot(4);
				if (Observation->Commits.FindChecked(Snapshot.GolemId)[3] != Before)
				{
					Fail(TEXT("An open RPG menu allowed an immediate skill activation."));
					return;
				}
			}
			SlateKey(OpenCycles % 2 == 0 ? EKeys::Escape : EKeys::Gamepad_FaceButton_Right);
			if (Phase != EPhase::Cleanup)
			{
				Enter(EPhase::PanelClose);
			}
		}
		void PanelClose()
		{
			if (Host->GetRPGPanel()->IsActivated() || Elapsed() < 0.15)
			{
				return;
			}
			if (Host->IsMoveInputIgnored() || Host->IsLookInputIgnored() || Host->bShowMouseCursor ||
				CaptureCount() != BaseCaptureCount)
			{
				Fail(TEXT("CommonUI close leaked gameplay input lock/cursor or a preview actor."));
				return;
			}
			if (++OpenCycles < 20)
			{
				Pulse(Host.Get(), EKeys::I);
				Enter(EPhase::PanelOpen);
				return;
			}
			Test.AddInfo(TEXT("Actual mapped I + focused Slate Escape/gamepad B reopened/closed twenty times; five native tabs, rich snapshots, focus and preview/input cleanup verified."));
			Pending = CommandFor(Host.Get());
			Pending.ProposedBuild.ClassChoice = TEXT("Endurance");
			Pending.ProposedBuild.SpecChoice = TEXT("A");
			OldCommand = Pending;
			BuildStaminaBefore = Host->GetControlledGolem()->GetGolemAttributeSet()->GetStamina();
			BuildMaxStaminaBefore = Host->GetControlledGolem()->GetGolemAttributeSet()->GetMaxStamina();
			ClosedPanelRevision = Host->GetRPGPanel()->GetSnapshot().Revision;
			Host->GetRPGPanel()->WidgetTree->ForEachWidget([this](UWidget* Widget)
			{
				ClosedPanelWidgets.Add(Widget);
			});
			Send(Host.Get(), Pending);
			Enter(EPhase::Build);
		}
		void WaitBuild()
		{
			const FGolemRPGCommandResult* Reply = Observation->Replies.Find(Pending.RequestId);
			if (Reply == nullptr || !Converged())
			{
				return;
			}
			const FGolemBuildReceiptAttributes* ReceiptAttributes = Observation->BuildReceiptAttributes.Find(Pending.RequestId);
			const bool bImmediateNoRefill = ReceiptAttributes != nullptr && ReceiptAttributes->bAuthority &&
				ReceiptAttributes->bSynchronous && ReceiptAttributes->GolemId == Pending.ExpectedGolemId &&
				FMath::IsNearlyEqual(BuildStaminaBefore, 100.0f) && FMath::IsNearlyEqual(BuildMaxStaminaBefore, 100.0f) &&
				FMath::IsNearlyEqual(ReceiptAttributes->Stamina, BuildStaminaBefore) &&
				FMath::IsNearlyEqual(ReceiptAttributes->MaxStamina, 110.0f);
			// The owning authority receipt runs inside Send: no timer can refill during the projection.
			// The later latent query may cross the runtime's existing 0.1s/+2.5 stamina timer phase.
			constexpr float NativeStaminaTickIntervalSeconds = 0.1f;
			constexpr float NativeStaminaPerTick = 2.5f;
			const double StaminaElapsed = ReceiptAttributes != nullptr ?
				FMath::Max(0.0, Server->GetTimeSeconds() - ReceiptAttributes->ServerTime) : 0.0;
			const int32 MaximumStaminaTicks = FMath::CeilToInt(StaminaElapsed / NativeStaminaTickIntervalSeconds);
			const float AllowedStamina = ReceiptAttributes != nullptr ? FMath::Min(110.0f,
				ReceiptAttributes->Stamina + MaximumStaminaTicks * NativeStaminaPerTick) : 0.0f;
			const float ActualStamina = Host->GetControlledGolem()->GetGolemAttributeSet()->GetStamina();
			const bool bLaterStaminaBound = ReceiptAttributes != nullptr &&
				ActualStamina >= ReceiptAttributes->Stamina && ActualStamina <= AllowedStamina;
			if (!ReplyOwned(Pending, Host.Get()) || !Reply->Result.IsSuccess() ||
				!(RPG->GetRPG()->GetGolemView(Pending.ExpectedGolemId).Build == Pending.ProposedBuild) ||
				!FMath::IsNearlyEqual(Host->GetControlledGolem()->GetGolemAttributeSet()->GetMaxStamina(), 110.0f) ||
				!bImmediateNoRefill || !bLaterStaminaBound ||
				Specs(Host->GetControlledGolem()->GetGolemAbilitySystem()) != OriginalSpecHandles)
			{
				const FGolemRPGGolemView Confirmed = RPG->GetRPG()->GetGolemView(Pending.ExpectedGolemId);
				const UGolemAttributeSet* Attributes = Host->GetControlledGolem()->GetGolemAttributeSet();
				const TArray<FGameplayAbilitySpecHandle> ActualHandles = Specs(
					Host->GetControlledGolem()->GetGolemAbilitySystem());
				const TWeakObjectPtr<AGolemSkillLabPlayerController>* Recipient =
					Observation->Recipients.Find(Pending.RequestId);
				FString ExpectedHandleText;
				FString ActualHandleText;
				for (const FGameplayAbilitySpecHandle& Handle : OriginalSpecHandles)
				{
					ExpectedHandleText += Handle.ToString() + TEXT(" ");
				}
				for (const FGameplayAbilitySpecHandle& Handle : ActualHandles)
				{
					ActualHandleText += Handle.ToString() + TEXT(" ");
				}
				Test.AddInfo(FString::Printf(TEXT("WaitBuild predicates: owner%d success%d build%d maxStamina%d noRefill%d grants%d; "
					"maxStamina %.9g/expected110, laterStamina %.9g; "
					"error%d context='%s' message='%s'; recipient='%s'/expected'%s'; "
					"request=%s Golem=%s/reply%s; expected revisions Golem%lld/inventory%lld, "
					"confirmed Golem%lld/inventory%lld, receipt Golem%lld/inventory%lld, replay%d; "
					"serverTime%.9g phaseElapsed%.9g openCycles%d."),
					ReplyOwned(Pending, Host.Get()), Reply->Result.IsSuccess(), Confirmed.Build == Pending.ProposedBuild,
					FMath::IsNearlyEqual(Attributes->GetMaxStamina(), 110.0f),
					bImmediateNoRefill, ActualHandles == OriginalSpecHandles,
					Attributes->GetMaxStamina(), Attributes->GetStamina(), static_cast<int32>(Reply->Result.Error),
					*Reply->Result.Context.ToString(), *Reply->Result.Message.ToString(),
					*GetNameSafe(Recipient != nullptr ? Recipient->Get() : nullptr), *GetNameSafe(Host.Get()),
					*Pending.RequestId.ToString(), *Pending.ExpectedGolemId.Value.ToString(), *Reply->GolemId.Value.ToString(),
					Pending.ExpectedGolemRevision, Pending.ExpectedInventoryRevision, Confirmed.Revision,
					RPG->GetInventory()->GetView().Revision, Reply->CommittedGolemRevision,
					Reply->CommittedInventoryRevision, Reply->bWasReplay, Server->GetTimeSeconds(), Elapsed(), OpenCycles));
				Test.AddInfo(FString::Printf(TEXT("WaitBuild expected Spec/Class/Choice/WeaponRune/SkillRune: %s/%s/%s/%s/%s; "
					"actual: %s/%s/%s/%s/%s; expected handles[%d]=%s; actual handles[%d]=%s."),
					*Pending.ProposedBuild.SpecId.ToString(), *Pending.ProposedBuild.ClassChoice.ToString(),
					*Pending.ProposedBuild.SpecChoice.ToString(), *Pending.ProposedBuild.WeaponRune.ToString(),
					*Pending.ProposedBuild.SkillRune.ToString(), *Confirmed.Build.SpecId.ToString(),
					*Confirmed.Build.ClassChoice.ToString(), *Confirmed.Build.SpecChoice.ToString(),
					*Confirmed.Build.WeaponRune.ToString(), *Confirmed.Build.SkillRune.ToString(),
					OriginalSpecHandles.Num(), *ExpectedHandleText, ActualHandles.Num(), *ActualHandleText));
				Test.AddInfo(FString::Printf(TEXT("WaitBuild stamina transition: before%.9g/max%.9g; "
					"receipt present%d authority%d synchronous%d value%.9g/max%.9g; "
					"later bound%d actual%.9g allowed%.9g ticks%d elapsed%.9g."),
					BuildStaminaBefore, BuildMaxStaminaBefore, ReceiptAttributes != nullptr,
					ReceiptAttributes != nullptr && ReceiptAttributes->bAuthority,
					ReceiptAttributes != nullptr && ReceiptAttributes->bSynchronous,
					ReceiptAttributes != nullptr ? ReceiptAttributes->Stamina : -1.0f,
					ReceiptAttributes != nullptr ? ReceiptAttributes->MaxStamina : -1.0f,
					bLaterStaminaBound, ActualStamina, AllowedStamina, MaximumStaminaTicks, StaminaElapsed));
				Fail(TEXT("Real sanctuary ApplyBuild failed atomic build, max-stamina/no refill, owner receipt or grant preservation."));
				return;
			}
			Test.AddInfo(FString::Printf(TEXT("Synchronous authority receipt proves stamina%.3f→%.3f while max%.3f→%.3f; "
				"later actual%.3f <= %.3f (%d native0.1s ticks across%.6fs)."),
				BuildStaminaBefore, ReceiptAttributes->Stamina, BuildMaxStaminaBefore, ReceiptAttributes->MaxStamina,
				ActualStamina, AllowedStamina, MaximumStaminaTicks, StaminaElapsed));
			TArray<TWeakObjectPtr<UWidget>> ActualClosedWidgets;
			UGolemRPGPanel* Panel = Host->GetRPGPanel();
			Panel->WidgetTree->ForEachWidget([&ActualClosedWidgets](UWidget* Widget)
			{
				ActualClosedWidgets.Add(Widget);
			});
			if (Panel->IsActivated() || Panel->GetSnapshot().Revision != ClosedPanelRevision ||
				ActualClosedWidgets != ClosedPanelWidgets)
			{
				Fail(TEXT("A deactivated panel rebuilt its retained native tree or accepted a live snapshot during real ApplyBuild attribute/revision events."));
				return;
			}
			Pulse(Host.Get(), EKeys::I);
			Enter(EPhase::PanelResyncOpen);
		}
		void WaitPanelResyncOpen()
		{
			if (PulseKey.IsValid() || !HasPanelFocus())
			{
				return;
			}
			UGolemRPGPanel* Panel = Host->GetRPGPanel();
			const FGolemRPGGolemView Confirmed = RPG->GetRPG()->GetGolemView(Pending.ExpectedGolemId);
			if (!Panel->IsActivated() || Panel->HasPendingCommand() ||
				Panel->GetSnapshot().Revision != Confirmed.Revision || !(Panel->GetSnapshot().Build == Confirmed.Build))
			{
				Fail(TEXT("Mapped reopen did not install the current confirmed build and revision before native CommonUI focus."));
				return;
			}
			SlateKey(EKeys::Escape);
			if (Phase != EPhase::Cleanup)
			{
				Enter(EPhase::PanelResyncClose);
			}
		}
		void WaitPanelResyncClose()
		{
			if (Host->GetRPGPanel()->IsActivated() || Elapsed() < 0.15)
			{
				return;
			}
			if (Host->IsMoveInputIgnored() || Host->IsLookInputIgnored() || Host->bShowMouseCursor ||
				CaptureCount() != BaseCaptureCount)
			{
				Fail(TEXT("Confirmed-state reopen/close leaked gameplay input, cursor or a preview actor."));
				return;
			}
			Test.AddInfo(TEXT("Real closed-panel ApplyBuild attribute/revision events preserved the native tree; mapped I reopen installed fresh confirmed state and focused Back restored gameplay input."));
			BeforeRejectedRPG = RPG->GetRPG()->GetView();
			BeforeRejectedInventory = RPG->GetInventory()->GetView();
			Pending = OldCommand;
			Pending.RequestId = FGuid::NewGuid();
			Pending.ProposedBuild.ClassChoice = TEXT("Reservoir");
			Send(Host.Get(), Pending);
			Enter(EPhase::Stale);
		}
		void WaitStale()
		{
			const FGolemRPGCommandResult* Reply = Observation->Replies.Find(Pending.RequestId);
			if (Reply == nullptr)
			{
				return;
			}
			if (Reply->Result.Error != EGolemError::InvalidRevision ||
				!(RPG->GetRPG()->GetView() == BeforeRejectedRPG) ||
				!(RPG->GetInventory()->GetView() == BeforeRejectedInventory) ||
				Specs(Host->GetControlledGolem()->GetGolemAbilitySystem()) != OriginalSpecHandles)
			{
				Fail(TEXT("Actual stale build RPC did not reject without changing domains or grants."));
				return;
			}
			Test.AddInfo(TEXT("Sanctuary build applied through owning controller; stale revision rejected with exact RPG/inventory/spec preservation."));
			for (TActorIterator<AGolemPartyArenaStartMarker> It(Server.Get()); It; ++It)
			{
				Place(Host->GetControlledGolem(), It->GetActorLocation() + FVector(0.0, -150.0, 100.0));
				FScopedConditionalWorldSwitcher Switch(Server.Get());
				Host->RequestInteract();
				Enter(EPhase::Wave);
				return;
			}
			Fail(TEXT("Genuine SkillLab arena start marker is missing."));
		}
		bool RaceResult(int32 ExpectedSuccesses)
		{
			int32 Successes = 0;
			for (int32 Index = 0; Index < Race.Num(); ++Index)
			{
				const FGolemRPGCommandResult* Reply = Observation->Replies.Find(Race[Index].RequestId);
				if (Reply == nullptr)
				{
					return false;
				}
				if (!ReplyOwned(Race[Index], Players[Index].Get()))
				{
					Fail(TEXT("Real race reply was delivered to a different owning controller."));
					return false;
				}
				if (Reply->Result.IsSuccess())
				{
					if (!(RPG->GetRPG()->GetGolemView(Race[Index].ExpectedGolemId).Build == Race[Index].ProposedBuild))
					{
						Fail(TEXT("Successful concurrent command did not publish its exact proposed Golem build."));
						return false;
					}
					++Successes;
					WinnerIndex = Index;
				}
				else if (Reply->Result.Error != EGolemError::InvalidRevision && Reply->Result.Error != EGolemError::Conflict)
				{
					Fail(FString::Printf(TEXT("Race loser unexpected error %d: %s"),
						static_cast<int32>(Reply->Result.Error), *Reply->Result.Message.ToString()));
					return false;
				}
			}
			if (Successes != ExpectedSuccesses)
			{
				Fail(FString::Printf(TEXT("Concurrent commands produced%d successes; expected%d for this ownership scope."),
					Successes, ExpectedSuccesses));
				return false;
			}
			return Converged();
		}
		void WaitSpecRace()
		{
			if (!RaceResult(2))
			{
				return;
			}
			Test.AddInfo(TEXT("Concurrent talent changes for two separately owned Golems both committed; inventory-free changes preserved the shared inventory revision."));
			PrepareRuneRace();
		}
		void PrepareRuneRace()
		{
			const FGolemInventoryView Inventory = RPG->GetInventory()->GetView();
			const FGolemItemInstance* Rune = Inventory.Items.FindByPredicate(
				[](const FGolemItemInstance& Item)
				{
					return Item.DefinitionId.PrimaryAssetName == TEXT("Propagation");
				});
			if (Rune == nullptr)
			{
				Fail(TEXT("Genuine definition-backed Propagation seed item is missing."));
				return;
			}
			RuneInstanceId = Rune->InstanceId;
			Race.Reset();
			for (const TWeakObjectPtr<AGolemSkillLabPlayerController>& Player : Players)
			{
				FGolemRPGCommand Command = CommandFor(Player.Get());
				Command.ProposedBuild.SkillRune = TEXT("Skill.Propagation");
				Command.SkillRuneInstanceId = RuneInstanceId;
				Race.Add(Command);
			}
			for (int32 Index = 0; Index < Race.Num(); ++Index)
			{
				Send(Players[Index].Get(), Race[Index]);
			}
			Enter(EPhase::RaceRune);
		}
		void WaitRuneRace()
		{
			if (!RaceResult(1))
			{
				return;
			}
			int32 Attached = 0;
			for (const FGolemRPGGolemView& Golem : RPG->GetRPG()->GetView().Golems)
			{
				Attached += Golem.SkillRuneInstanceId == RuneInstanceId ? 1 : 0;
			}
			Winner = Players[WinnerIndex];
			if (Attached != 1 || AuthorityGolem(Winner.Get())->FindComponentByClass<UGolemSkillRuntimeComponent>()->
				GetBuild().SkillRune != TEXT("Skill.Propagation"))
			{
				Fail(TEXT("Same-instance Attach Rune race did not preserve exclusive ownership and actual runtime tuning."));
				return;
			}
			Test.AddInfo(TEXT("Two latency-emulated clients applied independent builds then raced the same Propagation item: one attachment success, one stale/conflict, exact owner responses, exclusive rune and both client snapshots converged."));
			for (TActorIterator<AGolemRunestone> It(Server.Get()); It; ++It)
			{
				if (It->GetKind() == EGolemRunestoneKind::Crystal)
				{
					CrystalStone = *It;
					break;
				}
			}
			if (!CrystalStone.IsValid() || CrystalStone->GetState() != EGolemRunestoneState::Ready)
			{
				Fail(TEXT("Actual ready RS.Crystal actor is missing from the playable map."));
				return;
			}
			CrystalBasePowers.Reset();
			for (const TWeakObjectPtr<AGolemCharacter>& Golem : Golems)
			{
				CrystalBasePowers.Add(Golem->GetGolemAttributeSet()->GetAttackPower());
			}
			for (int32 Index = 0; Index < Players.Num(); ++Index)
			{
				Place(AuthorityGolem(Players[Index].Get()),
					CrystalStone->GetActorLocation() + FVector(-120.0, Index == 0 ? -60.0 : 60.0, 40.0));
			}
			Enter(EPhase::CrystalApproach);
		}
		AGolemRunestone* ClientCrystalStone(UWorld* World) const
		{
			for (TActorIterator<AGolemRunestone> It(World); It; ++It)
			{
				if (It->GetKind() == EGolemRunestoneKind::Crystal)
				{
					return *It;
				}
			}
			return nullptr;
		}
		AGolemRunestonePickup* ClientCrystalPickup(UWorld* World) const
		{
			for (TActorIterator<AGolemRunestonePickup> It(World); It; ++It)
			{
				if (It->GetKind() == EGolemRunestonePickupKind::PowerCrystal)
				{
					return *It;
				}
			}
			return nullptr;
		}
		int32 CountCrystalEvents(FName EventName) const
		{
			int32 Count = 0;
			for (const FGolemSkillLabEvent& Event : Observation->WorldEvents)
			{
				if (Event.EventName == EventName && Event.SubjectId == TEXT("RS.Crystal"))
				{
					++Count;
				}
			}
			return Count;
		}
		void WaitCrystalApproach()
		{
			if (Elapsed() < 0.4)
			{
				return;
			}
			for (const TWeakObjectPtr<AGolemSkillLabPlayerController>& Player : Players)
			{
				AGolemRunestone* Replica = ClientCrystalStone(Player->GetWorld());
				if (Replica == nullptr || Replica->GetState() != EGolemRunestoneState::Ready ||
					FVector::DistSquared(Player->GetControlledGolem()->GetActorLocation(), Replica->GetActorLocation()) > FMath::Square(200.0))
				{
					return;
				}
			}
			PulseInteractions();
			Enter(EPhase::CrystalChannel);
		}
		void WaitCrystalChannel()
		{
			if (CrystalStone->GetState() != EGolemRunestoneState::Cooldown)
			{
				return;
			}
			for (int32 Index = 0; Index < Golems.Num(); ++Index)
			{
				if (!FMath::IsNearlyEqual(Golems[Index]->GetGolemAttributeSet()->GetAttackPower(), CrystalBasePowers[Index]))
				{
					Fail(TEXT("Actual mapped Runestone activation incorrectly buffed a party Golem before collection."));
					return;
				}
			}
			int32 PickupCount = 0;
			for (TActorIterator<AGolemRunestonePickup> It(Server.Get()); It; ++It)
			{
				if (It->GetKind() == EGolemRunestonePickupKind::PowerCrystal)
				{
					CrystalPickup = *It;
					++PickupCount;
				}
			}
			if (PickupCount != 1 || CountCrystalEvents(TEXT("RunestoneActivated")) != 1)
			{
				Fail(TEXT("Two actual mapped F activation RPCs did not produce exactly one world crystal and one confirmed activation."));
				return;
			}
			for (int32 Index = 0; Index < Players.Num(); ++Index)
			{
				AGolemRunestone* Replica = ClientCrystalStone(Players[Index]->GetWorld());
				AGolemRunestonePickup* PickupReplica = ClientCrystalPickup(Players[Index]->GetWorld());
				if (Replica == nullptr || Replica->GetState() != EGolemRunestoneState::Cooldown ||
					PickupReplica == nullptr || PickupReplica->GetRemainingCharges() != 1)
				{
					return;
				}
			}
			for (int32 Index = 0; Index < Players.Num(); ++Index)
			{
				Place(AuthorityGolem(Players[Index].Get()),
					CrystalPickup->GetActorLocation() + FVector(0.0, Index == 0 ? -70.0 : 70.0, 100.0));
			}
			Enter(EPhase::CrystalPickupApproach);
		}
		void WaitCrystalPickupApproach()
		{
			if (Elapsed() < 0.4)
			{
				return;
			}
			for (const TWeakObjectPtr<AGolemSkillLabPlayerController>& Player : Players)
			{
				AGolemRunestonePickup* Replica = ClientCrystalPickup(Player->GetWorld());
				if (Replica == nullptr || FVector::DistSquared(Player->GetControlledGolem()->GetActorLocation(),
					Replica->GetActorLocation()) > FMath::Square(200.0))
				{
					return;
				}
			}
			PulseInteractions();
			Enter(EPhase::CrystalPickupRace);
		}
		void WaitCrystalPickupRace()
		{
			if (CrystalPickup->GetRemainingCharges() != 0 || !HeldInteractPlayers.IsEmpty())
			{
				return;
			}
			int32 BuffedCount = 0;
			for (int32 Index = 0; Index < Golems.Num(); ++Index)
			{
				const float Power = Golems[Index]->GetGolemAttributeSet()->GetAttackPower();
				if (FMath::IsNearlyEqual(Power, CrystalBasePowers[Index] * 1.15f))
				{
					++BuffedCount;
				}
				else if (!FMath::IsNearlyEqual(Power, CrystalBasePowers[Index]))
				{
					Fail(TEXT("Crystal race produced an unexpected Power value on a canonical Golem."));
					return;
				}
			}
			if (BuffedCount != 1 || CountCrystalEvents(TEXT("PickupConsumed")) != 1)
			{
				Fail(TEXT("Mapped F pickup race did not have exactly one buffed collector and one confirmed charge consumption."));
				return;
			}
			for (const FGolemSkillLabEvent& Event : Observation->WorldEvents)
			{
				if (Event.SubjectId == TEXT("RS.Crystal") && (!Event.EncounterId.IsValid() || !Event.RequestId.IsValid()
					|| !Event.SourceId.IsValid() || !Event.TargetId.IsValid() || !Event.ConfirmationId.IsValid() || Event.Revision < 1))
				{
					Fail(TEXT("Actual world adapter event omitted required encounter/request/source/target/confirmation/revision metadata."));
					return;
				}
			}
			for (const TWeakObjectPtr<AGolemSkillLabPlayerController>& Player : Players)
			{
				AGolemRunestonePickup* Replica = ClientCrystalPickup(Player->GetWorld());
				if (Replica == nullptr || Replica->GetRemainingCharges() != 0)
				{
					return;
				}
				for (const TWeakObjectPtr<AGolemCharacter>& Golem : Golems)
				{
					const FGolemRosterEntry* Entry = FindParty(Player->GetWorld())->FindSlot(
						FindParty(Server.Get())->GetControlledSlot(Golem->GetController()));
					if (Entry == nullptr || Entry->Golem == nullptr || !FMath::IsNearlyEqual(
						Entry->Golem->GetGolemAttributeSet()->GetAttackPower(), Golem->GetGolemAttributeSet()->GetAttackPower()))
					{
						return;
					}
				}
			}
			Test.AddInfo(TEXT("Two real clients mapped F through interaction RPCs: one channel/result, no activation party buff, one crystal collector/charge, +15% collector-only Power, replica convergence and complete confirmed M6 adapter metadata."));
			ActiveSlot = 3;
			Enter(EPhase::Skills);
		}
		void WaitWave()
		{
			if (Mode->GetArenaState()->GetSnapshot().AliveEnemies > 0)
			{
				Winner = Host;
				for (TActorIterator<AGolemAbilityDummy> It(Server.Get()); It; ++It)
				{
					if (It->GetLifeComponent()->IsAlive())
					{
						SkillTarget = *It;
						break;
					}
				}
				if (!SkillTarget.IsValid())
				{
					Fail(TEXT("Real SkillLab training dummy is missing from the playable map."));
					return;
				}
				int32 OffsetIndex = 0;
				for (const TWeakObjectPtr<AGolemCharacter>& Golem : Golems)
				{
					Place(Golem.Get(), SkillTarget->GetActorLocation() + FVector(-150.0, 150.0 * OffsetIndex++, 0.0));
				}
				ActiveSlot = 1;
				Enter(EPhase::Skills);
			}
		}
		int32 CommitCount(int32 Action) const
		{
			return Observation->Commits.FindChecked(AuthorityGolem(Winner.Get())->GetGolemEntityId())[Action - 1];
		}
		FString SkillDiagnostics() const
		{
			AGolemCharacter* Golem = AuthorityGolem(Winner.Get());
			if (Golem == nullptr)
			{
				return TEXT("No owned skill-test Golem.");
			}
			const UGolemAbilitySystemComponent* ASC = Golem->GetGolemAbilitySystem();
			const UGolemSkillRuntimeComponent* Runtime = Golem->FindComponentByClass<UGolemSkillRuntimeComponent>();
			if (ASC == nullptr || Runtime == nullptr || Golem->GetGolemAttributeSet() == nullptr)
			{
				return TEXT("Skill-test Golem is missing actual ASC/runtime/attributes.");
			}
			FString ActiveAbilities;
			for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
			{
				if (Spec.IsActive())
				{
					ActiveAbilities += Spec.Ability->GetClass()->GetName() + TEXT(" ");
				}
			}
			FGameplayTagContainer Tags;
			ASC->GetOwnedGameplayTags(Tags);
			const FGolemSkillRuntimeState State = Runtime->GetSkillState();
			const TArray<int32>* Counts = Observation->Commits.Find(Golem->GetGolemEntityId());
			const double CooldownEnd = State.CooldownEnds.IsValidIndex(ActiveSlot - 1)
				? State.CooldownEnds[ActiveSlot - 1] : -1.0;
			return FString::Printf(TEXT("Skill slot%d sent%d spec%s HP%.2f resource%.2f time%.2f CDend%.2f commits%d active[%s] tags[%s] panel%d moveLock%d lookLock%d position%s"),
				ActiveSlot, bSkillSent, *State.Build.SpecId.ToString(), Golem->GetGolemAttributeSet()->GetHealth(),
				Golem->GetGolemAttributeSet()->GetRuneEnergy(), Server->GetTimeSeconds(), CooldownEnd,
				Counts != nullptr && Counts->IsValidIndex(ActiveSlot - 1) ? (*Counts)[ActiveSlot - 1] : -1,
				*ActiveAbilities, *Tags.ToStringSimple(),
				Winner->GetRPGPanel() != nullptr && Winner->GetRPGPanel()->IsActivated(),
				Winner->IsMoveInputIgnored(), Winner->IsLookInputIgnored(), *Golem->GetActorLocation().ToCompactString());
		}
		void WaitSkills()
		{
			AGolemCharacter* Golem = AuthorityGolem(Winner.Get());
			UGolemSkillRuntimeComponent* Runtime = Golem->FindComponentByClass<UGolemSkillRuntimeComponent>();
			if (!bSkillSent)
			{
				if (!Runtime->IsSkillAvailable(ActiveSlot))
				{
					return;
				}
				for (const FGameplayAbilitySpec& Spec : Golem->GetGolemAbilitySystem()->GetActivatableAbilities())
				{
					if (Spec.IsActive())
					{
						return;
					}
				}
				if (ActiveSlot <= 2)
				{
					if (!SkillTarget.IsValid() || !SkillTarget->GetLifeComponent()->IsAlive())
					{
						Fail(TEXT("Existing training dummy became unavailable before human skill input."));
						return;
					}
					Place(Golem, SkillTarget->GetActorLocation() - FVector(150.0, 0.0, 0.0));
				}
				CommitsBefore = CommitCount(ActiveSlot);
				Test.AddInfo(SkillDiagnostics());
				Pulse(Winner.Get(), ActiveSlot == 1 ? EKeys::Z : ActiveSlot == 2 ? EKeys::X :
					ActiveSlot == 3 ? EKeys::C : EKeys::V);
				bSkillSent = true;
				return;
			}
			const FGolemSkillRuntimeState State = Runtime->GetSkillState();
			if (PulseKey.IsValid() || CommitCount(ActiveSlot) <= CommitsBefore ||
				!State.CooldownEnds.IsValidIndex(ActiveSlot - 1) ||
				State.CooldownEnds[ActiveSlot - 1] <= Server->GetTimeSeconds())
			{
				return;
			}
			if (ActiveSlot < 4)
			{
				++ActiveSlot;
				bSkillSent = false;
				return;
			}
			if (State.BuffEnd <= Server->GetTimeSeconds())
			{
				Fail(TEXT("Mapped A4 committed without its real specialization buff window."));
				return;
			}
			if (!bNetwork && !bPadVerified)
			{
				Enter(EPhase::PadSkill);
				return;
			}
			if (!bCapturedTransfer)
			{
				TransferGolem = Golem;
				TransferBuild = Runtime->GetBuild();
				TransferCooldown = State.CooldownEnds[3];
				TransferSpecs = Specs(Golem->GetGolemAbilitySystem());
				TransferResource = Golem->GetGolemAttributeSet()->GetRuneEnergy();
				TransferTime = Server->GetTimeSeconds();
				TransferSlot = FindParty(Server.Get())->GetControlledSlot(Golem->GetController());
				bCapturedTransfer = true;
			}
			// Wait for the committed ability's recovery before transferring control.
			if (Server->GetTimeSeconds() - TransferTime < 0.8)
			{
				return;
			}
			Test.AddInfo(bNetwork ? TEXT("Real client mapped C/V reached the server GAS commit callbacks, spent resource and entered the A4 cooldown/buff.")
				: TEXT("Human mapped Z/X/C/V reached actual GAS commits in the SkillLab encounter; skill cooldowns/resource and A4 buff are live."));
			if (bNetwork)
			{
				const TSharedPtr<const FInternetAddr> Address = Server->GetNetDriver()->GetLocalAddr();
				if (!Address.IsValid())
				{
					Fail(TEXT("Listening address unavailable for actual JIP."));
					return;
				}
				DepartingContext = ClientContexts[WinnerIndex];
				ReconnectURL = FString::Printf(TEXT("127.0.0.1:%d?GolemSlot=%d"), Address->GetPort(), TransferSlot);
				FScopedConditionalWorldSwitcher Switch(Winner->GetWorld());
				Winner->ClientTravel(DetachedMap, TRAVEL_Absolute);
				Enter(EPhase::Disconnect);
			}
			else
			{
				FScopedConditionalWorldSwitcher Switch(Server.Get());
				Host->RequestSwitchGolem((TransferSlot + 1) % 3);
				Enter(EPhase::SwitchAway);
			}
		}
		void WaitPadSkill()
		{
			AGolemCharacter* Golem = AuthorityGolem(Winner.Get());
			UGolemSkillRuntimeComponent* Runtime = Golem->FindComponentByClass<UGolemSkillRuntimeComponent>();
			if (!bPadModifierHeld)
			{
				if (!Runtime->IsSkillAvailable(1))
				{
					return;
				}
				for (const FGameplayAbilitySpec& Spec : Golem->GetGolemAbilitySystem()->GetActivatableAbilities())
				{
					if (Spec.IsActive())
					{
						return;
					}
				}
				PadCommitsBefore = CommitCount(1);
				PadHeavyCommitsBefore = Observation->HeavyCommits.FindChecked(Golem->GetGolemEntityId());
				PadModifierTime = Server->GetTimeSeconds();
				bPadModifierHeld = true;
				FScopedConditionalWorldSwitcher Switch(Winner->GetWorld());
				Winner->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::Gamepad_LeftTrigger, IE_Pressed, 1.0f));
				return;
			}
			if (Observation->HeavyCommits.FindChecked(Golem->GetGolemEntityId()) != PadHeavyCommitsBefore)
			{
				Fail(TEXT("Holding the actual SkillLab LT modifier committed the legacy heavy attack."));
				return;
			}
			for (const FGameplayAbilitySpec& Spec : Golem->GetGolemAbilitySystem()->GetActivatableAbilities())
			{
				if (Spec.IsActive() && Spec.Ability->IsA<UGolemHeavyAttackAbility>())
				{
					Fail(TEXT("Actual LT + Dpad skill input activated the legacy heavy attack."));
					return;
				}
			}
			if (!bPadDirectionSent)
			{
				if (Server->GetTimeSeconds() - PadModifierTime < 0.15)
				{
					return;
				}
				Pulse(Winner.Get(), EKeys::Gamepad_DPad_Up);
				bPadDirectionSent = true;
				return;
			}
			if (PulseKey.IsValid() || CommitCount(1) <= PadCommitsBefore)
			{
				return;
			}
			const FGolemSkillRuntimeState State = Runtime->GetSkillState();
			if (!State.CooldownEnds.IsValidIndex(0) || State.CooldownEnds[0] <= Server->GetTimeSeconds())
			{
				Fail(TEXT("Actual held-LT + Dpad Up did not start A1's real cooldown."));
				return;
			}
			ReleasePadModifier();
			bPadVerified = true;
			Test.AddInfo(TEXT("Actual held-LT + Dpad Up reached the A1 GAS commit and cooldown without activating or committing legacy Heavy; physical Z/X/C/V checks remain intact."));
			Enter(EPhase::PadHeavy);
		}
		void WaitPadHeavy()
		{
			AGolemCharacter* Golem = AuthorityGolem(Winner.Get());
			const FGolemEntityId Id = Golem->GetGolemEntityId();
			if (!bPadModifierHeld)
			{
				for (const FGameplayAbilitySpec& Spec : Golem->GetGolemAbilitySystem()->GetActivatableAbilities())
				{
					if (Spec.IsActive())
					{
						return;
					}
				}
				PadHeavyCommitsBefore = Observation->HeavyCommits.FindChecked(Id);
				PadLightCommitsBefore = Observation->LightCommits.FindChecked(Id);
				PadModifierTime = Server->GetTimeSeconds();
				bPadModifierHeld = true;
				FScopedConditionalWorldSwitcher Switch(Winner->GetWorld());
				Winner->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::Gamepad_LeftTrigger, IE_Pressed, 1.0f));
				return;
			}
			if (Observation->LightCommits.FindChecked(Id) != PadLightCommitsBefore)
			{
				Fail(TEXT("Actual held-LT + RT chord incorrectly committed Light instead of exclusive Heavy."));
				return;
			}
			if (!bPadHeavySent)
			{
				if (Server->GetTimeSeconds() - PadModifierTime < 0.15)
				{
					return;
				}
				if (Observation->HeavyCommits.FindChecked(Id) != PadHeavyCommitsBefore)
				{
					Fail(TEXT("Actual LT alone committed Heavy before the RT chord."));
					return;
				}
				Pulse(Winner.Get(), EKeys::Gamepad_RightTrigger);
				bPadHeavySent = true;
				return;
			}
			if (PulseKey.IsValid() || Observation->HeavyCommits.FindChecked(Id) <= PadHeavyCommitsBefore)
			{
				return;
			}
			for (const FGameplayAbilitySpec& Spec : Golem->GetGolemAbilitySystem()->GetActivatableAbilities())
			{
				if (Spec.IsActive())
				{
					return;
				}
			}
			ReleasePadModifier();
			Test.AddInfo(TEXT("Actual held-LT + RT commits the preserved existing Heavy GAS action, with zero Light commits; montage recovery completes before control transfer."));
			Enter(EPhase::Skills);
		}
		void ReleasePadModifier()
		{
			if (bPadModifierHeld && Winner.IsValid())
			{
				FScopedConditionalWorldSwitcher Switch(Winner->GetWorld());
				Winner->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::Gamepad_LeftTrigger, IE_Released, 0.0f));
			}
			bPadModifierHeld = false;
		}
		bool TransferPreserved() const
		{
			if (!TransferGolem.IsValid())
			{
				return false;
			}
			const UGolemSkillRuntimeComponent* Runtime = TransferGolem->FindComponentByClass<UGolemSkillRuntimeComponent>();
			const FGolemSkillRuntimeState State = Runtime->GetSkillState();
			// AdvanceRuntime restores one point every 0.1s; ceil includes its existing timer phase, not a refill.
			constexpr float NativeResourceTickIntervalSeconds = 0.1f;
			constexpr float NativeResourcePerTick = 1.0f;
			const double ResourceElapsed = FMath::Max(0.0, Server->GetTimeSeconds() - TransferTime);
			const int32 MaximumResourceTicks = FMath::CeilToInt(ResourceElapsed / NativeResourceTickIntervalSeconds);
			const float AllowedResource = FMath::Min(TransferGolem->GetGolemAttributeSet()->GetMaxRuneEnergy(),
				TransferResource + MaximumResourceTicks * NativeResourcePerTick);
			const bool bPreserved = State.Build == TransferBuild && State.CooldownEnds.IsValidIndex(3) &&
				FMath::IsNearlyEqual(State.CooldownEnds[3], TransferCooldown) &&
				Specs(TransferGolem->GetGolemAbilitySystem()) == TransferSpecs &&
				TransferGolem->GetGolemAttributeSet()->GetRuneEnergy() <= AllowedResource;
			if (!bPreserved)
			{
				Test.AddInfo(FString::Printf(TEXT("Transfer preservation detail: buildMatch=%d handlesMatch=%d oldHandles=%d newHandles=%d oldCD=%.6f newCD=%.6f capturedResource=%.3f actualResource=%.3f allowedResource=%.3f elapsed=%.6f."),
					State.Build == TransferBuild, Specs(TransferGolem->GetGolemAbilitySystem()) == TransferSpecs,
					TransferSpecs.Num(), Specs(TransferGolem->GetGolemAbilitySystem()).Num(), TransferCooldown,
					State.CooldownEnds.IsValidIndex(3) ? State.CooldownEnds[3] : -1.0,
					TransferResource, TransferGolem->GetGolemAttributeSet()->GetRuneEnergy(), AllowedResource,
					Server->GetTimeSeconds() - TransferTime));
			}
			return bPreserved;
		}
		void WaitSwitchAway()
		{
			if (Host->GetControlledGolem() == TransferGolem.Get() || Elapsed() < 0.8)
			{
				return;
			}
			if (!TransferPreserved())
			{
				Fail(TEXT("Solo human/AI switch reset build, grants, cooldown or resource."));
				return;
			}
			FScopedConditionalWorldSwitcher Switch(Server.Get());
			Host->RequestSwitchGolem(TransferSlot);
			Enter(EPhase::SwitchBack);
		}
		void WaitSwitchBack()
		{
			if (Host->GetControlledGolem() != TransferGolem.Get())
			{
				return;
			}
			if (!TransferPreserved())
			{
				Fail(TEXT("Returning to the same canonical Golem reset combat build state."));
				return;
			}
			Test.AddInfo(TEXT("Solo switch away/back preserved Character ASC, same skill handles/build, running SpecMajor cooldown and bounded natural resource regeneration."));
			BeginCleanup();
		}
		void WaitDisconnect()
		{
			UWorld* Detached = ContextWorld(DepartingContext);
			const FGolemRosterEntry* Entry = FindParty(Server.Get())->FindSlot(TransferSlot);
			if (Detached == nullptr || Detached->GetNetMode() != NM_Standalone || Detached->GetNetDriver() != nullptr ||
				Detached->GetFirstPlayerController() == nullptr || Entry->OwnerKind != EGolemControllerOwner::Companion ||
				Cast<AAIController>(TransferGolem->GetController()) == nullptr ||
				FindParty(Server.Get())->GetHumanCount() != 2)
			{
				return;
			}
			if (!TransferPreserved())
			{
				Fail(TEXT("Actual disconnect/AI takeover reset specialization runtime."));
				return;
			}
			FScopedConditionalWorldSwitcher Switch(Detached);
			Detached->GetFirstPlayerController()->ClientTravel(ReconnectURL, TRAVEL_Absolute);
			Enter(EPhase::Rejoin);
		}
		void WaitRejoin()
		{
			AGolemSkillLabPlayerController* Rejoined = LocalOwner(ContextWorld(DepartingContext));
			if (Rejoined == nullptr || Rejoined->GetWorld()->GetNetMode() != NM_Client ||
				Rejoined->GetPlayerState<AGolemPlayerState>() == nullptr || Rejoined->GetControlledGolem() == nullptr ||
				Rejoined->GetPlayerState<AGolemPlayerState>()->GetAssignedSlot() != TransferSlot ||
				FindParty(Server.Get())->GetHumanCount() != 3 || !Converged())
			{
				return;
			}
			if (!TransferPreserved() ||
				Specs(Rejoined->GetControlledGolem()->GetGolemAbilitySystem()) != TransferSpecs)
			{
				Fail(TEXT("Actual JIP/reclaim reset runtime or failed owning-client ability-spec replication."));
				return;
			}
			Test.AddInfo(TEXT("Actual client travel/disconnect -> companion takeover -> same-slot JIP retained canonical actors/ASCs, build/rune, source ability handles, running A4 cooldown and natural resource budget; no client AI."));
			BeginCleanup();
		}
		void PulseInteractions()
		{
			InteractPulseTime = Server->GetTimeSeconds();
			for (const TWeakObjectPtr<AGolemSkillLabPlayerController>& Player : Players)
			{
				FScopedConditionalWorldSwitcher Switch(Player->GetWorld());
				Player->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::F, IE_Pressed, 1.0f));
				HeldInteractPlayers.Add(Player);
			}
		}
		void ReleaseInteractions()
		{
			for (const TWeakObjectPtr<AGolemSkillLabPlayerController>& Player : HeldInteractPlayers)
			{
				if (Player.IsValid())
				{
					FScopedConditionalWorldSwitcher Switch(Player->GetWorld());
					Player->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::F, IE_Released, 0.0f));
				}
			}
			HeldInteractPlayers.Reset();
		}
		void Pulse(AGolemSkillLabPlayerController* Player, FKey Key)
		{
			if (PulseKey.IsValid())
			{
				return;
			}
			PulsePlayer = Player;
			PulseKey = Key;
			PulseTime = Server->GetTimeSeconds();
			FScopedConditionalWorldSwitcher Switch(Player->GetWorld());
			Player->InputKey(FInputKeyEventArgs::CreateSimulated(Key, IE_Pressed, 1.0f));
		}
		void ReleaseKey()
		{
			if (PulseKey.IsValid() && PulsePlayer.IsValid())
			{
				FScopedConditionalWorldSwitcher Switch(PulsePlayer->GetWorld());
				PulsePlayer->InputKey(FInputKeyEventArgs::CreateSimulated(PulseKey, IE_Released, 0.0f));
			}
			PulseKey = FKey();
			PulsePlayer.Reset();
		}
		void Unbind()
		{
			if (WorldEventSubscription.Bus.IsValid())
			{
				WorldEventSubscription.Bus->Unsubscribe(WorldEventSubscription);
			}
			WorldEventSubscription = FGolemEventSubscription();
			for (int32 Index = 0; Index < ObservedPlayers.Num(); ++Index)
			{
				if (ObservedPlayers[Index].IsValid())
				{
					ObservedPlayers[Index]->OnRPGCommandResult.Remove(ReplyHandles[Index]);
				}
			}
			ObservedPlayers.Reset();
			ReplyHandles.Reset();
			for (int32 Index = 0; Index < CommitHandles.Num(); ++Index)
			{
				if (Golems[Index].IsValid())
				{
					Golems[Index]->GetGolemAbilitySystem()->AbilityCommittedCallbacks.Remove(CommitHandles[Index]);
				}
			}
			CommitHandles.Reset();
		}
		void BeginCleanup()
		{
			if (Phase >= EPhase::Cleanup)
			{
				return;
			}
			ReleaseKey();
			ReleaseInteractions();
			Unbind();
			ReleasePadModifier();
			Enter(EPhase::Cleanup);
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
			if (!Settings.IsValid())
			{
				return;
			}
			ULevelEditorPlaySettings* Defaults = GetMutableDefault<ULevelEditorPlaySettings>();
			Defaults->MultipleInstancePositions = OriginalPositions;
			Defaults->LastSize = OriginalLastSize;
			Defaults->NewWindowPosition = OriginalPosition;
			Defaults->NewWindowWidth = OriginalWidth;
			Defaults->NewWindowHeight = OriginalHeight;
			Defaults->SaveConfig();
		}
		void Cleanup()
		{
			if (bOwnsSession && GEditor != nullptr && GEditor->IsPlaySessionInProgress())
			{
				if (Elapsed() < 15.0)
				{
					return;
				}
				Test.AddError(TEXT("SkillLab PIE cleanup timed out; forcing synchronous EndPlay."));
				if (GEditor->IsPlayingSessionInEditor())
				{
					GEditor->EndPlayMap();
				}
				else if (GEditor->IsPlaySessionRequestQueued())
				{
					GEditor->CancelRequestPlaySession();
				}
			}
			RestoreSettings();
			Settings.Reset();
			bOwnsSession = false;
			Enter(EPhase::Complete);
		}

		FAutomationTestBase& Test;
		bool bNetwork = false;
		TSharedPtr<FObservation> Observation;
		EPhase Phase = EPhase::Start;
		double RunStarted = 0.0;
		double PhaseStarted = 0.0;
		bool bOwnsSession = false;
		TStrongObjectPtr<ULevelEditorPlaySettings> Settings;
		TArray<FIntPoint> OriginalPositions;
		FIntPoint OriginalLastSize;
		FIntPoint OriginalPosition;
		int32 OriginalWidth = 0;
		int32 OriginalHeight = 0;
		TWeakObjectPtr<UWorld> Server;
		TWeakObjectPtr<AGolemSkillLabGameMode> Mode;
		TWeakObjectPtr<AGolemRPGStateActor> RPG;
		TWeakObjectPtr<AGolemSkillLabPlayerController> Host;
		TWeakObjectPtr<AGolemSkillLabPlayerController> Winner;
		TWeakObjectPtr<AGolemAbilityDummy> SkillTarget;
		TWeakObjectPtr<AGolemRunestone> CrystalStone;
		TWeakObjectPtr<AGolemRunestonePickup> CrystalPickup;
		TArray<float> CrystalBasePowers;
		FGolemEventSubscription WorldEventSubscription;
		TArray<TWeakObjectPtr<AGolemSkillLabPlayerController>> HeldInteractPlayers;
		double InteractPulseTime = 0.0;
		TArray<FName> ClientContexts;
		TArray<TWeakObjectPtr<AGolemSkillLabPlayerController>> Players;
		TArray<TWeakObjectPtr<AGolemSkillLabPlayerController>> ObservedPlayers;
		TArray<FDelegateHandle> ReplyHandles;
		TArray<FDelegateHandle> CommitHandles;
		TArray<TWeakObjectPtr<AGolemCharacter>> Golems;
		TArray<FGolemEntityId> Ids;
		TArray<FPrimaryAssetId> Heroes;
		TArray<FGameplayAbilitySpecHandle> OriginalSpecHandles;
		FGolemRPGCommand Pending;
		FGolemRPGCommand OldCommand;
		TArray<FGolemRPGCommand> Race;
		FGolemRPGView BeforeRejectedRPG;
		FGolemInventoryView BeforeRejectedInventory;
		FGuid RuneInstanceId;
		int32 WinnerIndex = INDEX_NONE;
		int32 OpenCycles = 0;
		int32 BaseCaptureCount = 0;
		float BuildStaminaBefore = 0.0f;
		float BuildMaxStaminaBefore = 0.0f;
		int64 ClosedPanelRevision = 0;
		TArray<TWeakObjectPtr<UWidget>> ClosedPanelWidgets;
		FKey PulseKey;
		double PulseTime = 0.0;
		TWeakObjectPtr<AGolemSkillLabPlayerController> PulsePlayer;
		int32 ActiveSlot = 1;
		int32 CommitsBefore = 0;
		bool bSkillSent = false;
		bool bCapturedTransfer = false;
		bool bPadModifierHeld = false;
		bool bPadDirectionSent = false;
		bool bPadVerified = false;
		bool bPadHeavySent = false;
		int32 PadCommitsBefore = 0;
		int32 PadHeavyCommitsBefore = 0;
		int32 PadLightCommitsBefore = 0;
		double PadModifierTime = 0.0;
		TWeakObjectPtr<AGolemCharacter> TransferGolem;
		FGolemSkillBuild TransferBuild;
		double TransferCooldown = 0.0;
		float TransferResource = 0.0f;
		double TransferTime = 0.0;
		int32 TransferSlot = INDEX_NONE;
		TArray<FGameplayAbilitySpecHandle> TransferSpecs;
		FName DepartingContext;
		FString ReconnectURL;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGolemSkillLabSoloIntegrationTest,
	"Golem.M55.Integration.SoloUIInputAndBuild",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGolemSkillLabSoloIntegrationTest::RunTest(const FString& Parameters)
{
	FAutomationTestFramework::Get().EnqueueLatentCommand(
		MakeShared<GolemSkillLabIntegration::FSkillLabCommand>(*this, false));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGolemSkillLabNetworkIntegrationTest,
	"Golem.M55.Integration.ListenClientBuildRaceAndTakeover",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGolemSkillLabNetworkIntegrationTest::RunTest(const FString& Parameters)
{
	FAutomationTestFramework::Get().EnqueueLatentCommand(
		MakeShared<GolemSkillLabIntegration::FSkillLabCommand>(*this, true));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGolemRPGUIClosedReceiptTest,
	"Golem.M56.UI.ClosedReceiptReconciliation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGolemRPGUIClosedReceiptTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<UGameInstance> Instance(NewObject<UGameInstance>(GEngine));
	Instance->InitializeStandalone();
	UWorld* World = Instance->GetWorld();
	AGolemRPGUIPlayerController* Controller = World->SpawnActor<AGolemRPGUIPlayerController>();
	if (!TestNotNull(TEXT("Native receipt fixture owns an actual controller"), Controller))
	{
		Instance->Shutdown();
		World->DestroyWorld(false);
		GEngine->DestroyWorldContext(World);
		return false;
	}
	Controller->RPGPanel = NewObject<UGolemRPGPanel>(Controller);
	UGolemRPGPanel* Panel = Controller->RPGPanel;
	TArray<FGolemRPGPanelCommand> BuildCommands;
	TArray<FGolemGearPanelIntent> FixtureCommands;
	const FDelegateHandle BuildHandle = Panel->OnCommandRequested.AddLambda(
		[&BuildCommands](const FGolemRPGPanelCommand& Command)
		{
			BuildCommands.Add(Command);
		});
	const FDelegateHandle FixtureHandle = Panel->OnGearCommandRequested.AddLambda(
		[&FixtureCommands](const FGolemGearPanelIntent& Command)
		{
			FixtureCommands.Add(Command);
		});
	FGolemRPGPanelSnapshot Confirmed;
	Confirmed.GolemId = FGolemEntityId::Create();
	Confirmed.Build.SpecId = TEXT("Uranus");
	Confirmed.bCanEditBuild = true;
	Panel->SetSnapshot(Confirmed);
	Panel->StageChoice(true, TEXT("Reservoir"));
	Panel->SubmitCommand(EGolemRPGPanelAction::ApplyBuild);
	if (TestEqual(TEXT("Native staging creates one pending request"), BuildCommands.Num(), 1))
	{
		Controller->PendingRequestId = BuildCommands[0].RequestId;
		FGolemRPGCommandResult Receipt;
		Receipt.RequestId = FGuid::NewGuid();
		Controller->HandleRPGResult(Receipt);
		TestTrue(TEXT("An unrelated closed RPG receipt preserves the exact pending request"),
			Panel->HasPendingCommand() && Panel->GetPendingRequestId() == BuildCommands[0].RequestId);
		Receipt.RequestId = BuildCommands[0].RequestId;
		Controller->HandleRPGResult(Receipt);
		TestFalse(TEXT("The matching closed RPG receipt reconciles widget and controller Pending"),
			Panel->HasPendingCommand() || Controller->PendingRequestId.IsValid());
		Receipt.RequestId = FGuid::NewGuid();
		Controller->HandleRPGResult(Receipt);
		TestFalse(TEXT("A later gameplay receipt cannot strand the consumed UI request"),
			Panel->HasPendingCommand() || Panel->GetPendingRequestId().IsValid());
	}
	Confirmed.Gear.bCanRunFixtures = true;
	Panel->SetSnapshot(Confirmed);
	Panel->RequestGearFixture(TEXT("SanctuaryRecovery"));
	if (TestEqual(TEXT("Native fixture intent creates one pending request"), FixtureCommands.Num(), 1))
	{
		Controller->PendingRequestId = FixtureCommands[0].RequestId;
		Controller->HandleGearFixtureResult(FGuid::NewGuid(), FGolemResult::Success());
		TestTrue(TEXT("An unrelated closed fixture receipt preserves the exact pending request"),
			Panel->HasPendingCommand() && Panel->GetPendingRequestId() == FixtureCommands[0].RequestId);
		Controller->HandleGearFixtureResult(FixtureCommands[0].RequestId, FGolemResult::Success());
		TestFalse(TEXT("The matching closed fixture receipt reconciles widget and controller Pending"),
			Panel->HasPendingCommand() || Controller->PendingRequestId.IsValid());
		Controller->HandleRPGResult(FGolemRPGCommandResult());
		TestFalse(TEXT("A later non-UI result cannot restore fixture Pending"), Panel->HasPendingCommand());
	}
	TestFalse(TEXT("Receipt reconciliation never activates a closed panel"), Panel->IsActivated());
	Panel->OnCommandRequested.Remove(BuildHandle);
	Panel->OnGearCommandRequested.Remove(FixtureHandle);
	Controller->Destroy();
	Instance->Shutdown();
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	return true;
}

#endif
