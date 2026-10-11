#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Abilities/GameplayAbility.h"
#include "Abilities/KnightHeavyAttackAbility.h"
#include "Abilities/KnightLightAttackAbility.h"
#include "AIController.h"
#include "AbilityYard/KnightAbilityDummy.h"
#include "Attributes/KnightAttributeSet.h"
#include "Blueprint/WidgetTree.h"
#include "Character/KnightCharacter.h"
#include "Commands/KnightRPGCommand.h"
#include "Components/CanvasPanel.h"
#include "Components/KnightAbilitySystemComponent.h"
#include "Components/KnightLifeComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Events/KnightEventSubsystem.h"
#include "Events/KnightSkillLabEvent.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/SlateUser.h"
#include "Framework/KnightGameState.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayAbilitySpec.h"
#include "HAL/PlatformTime.h"
#include "IPAddress.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "InputKeyEventArgs.h"
#include "Inventory/KnightInventoryComponent.h"
#include "Misc/PackageName.h"
#include "Party/KnightPartyState.h"
#include "PartyArena/KnightArenaEnemy.h"
#include "PartyArena/KnightPartyArenaStartMarker.h"
#include "PartyArena/KnightPartyArenaState.h"
#include "PlayInEditorDataTypes.h"
#include "Player/KnightPlayerState.h"
#include "Progression/KnightRPGComponent.h"
#include "RPGLoop/KnightRPGGameState.h"
#include "RPGLoop/KnightRPGStateActor.h"
#include "RPGLoop/KnightRPGUIPlayerController.h"
#include "RPGLoop/KnightRPGUpgradeStation.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "SkillLab/KnightSkillLabGameMode.h"
#include "SkillLab/KnightSkillLabPlayerController.h"
#include "SkillLab/KnightRunestone.h"
#include "SkillLab/KnightRunestonePickup.h"
#include "Skills/KnightSkillRuntimeComponent.h"
#include "Skills/KnightSkillTypes.h"
#include "UnrealEngine.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/KnightRPGActionButton.h"
#include "Widgets/KnightRPGPanel.h"

namespace KnightSkillLabIntegration
{
	const TCHAR* MapPath = TEXT("/Game/Knight/Maps/Playables/L_M05_5_SkillLab");
	const TCHAR* DetachedMap = TEXT("/Game/Knight/Maps/Playables/L_M00_Boot");
	constexpr uint16 ServerPort = 17955;

	enum class EPhase : uint8
	{
		Start, Ready, Sanctuary, PanelOpen, PanelClose, Build, PanelResyncOpen, PanelResyncClose, Stale, RaceSpecs,
		RaceRune, CrystalApproach, CrystalChannel, CrystalPickupApproach, CrystalPickupRace,
		Wave, Skills, PadSkill, PadHeavy, SwitchAway, SwitchBack, Disconnect, Rejoin, Cleanup, Complete
	};

	struct FKnightBuildReceiptAttributes
	{
		FKnightEntityId KnightId;
		float Stamina = 0.0f;
		float MaxStamina = 0.0f;
		double ServerTime = 0.0;
		bool bAuthority = false;
		bool bSynchronous = false;
	};

	struct FObservation
	{
		TMap<FGuid, FKnightRPGCommandResult> Replies;
		TMap<FGuid, TWeakObjectPtr<AKnightSkillLabPlayerController>> Recipients;
		TMap<FGuid, FKnightBuildReceiptAttributes> BuildReceiptAttributes;
		TSet<FGuid> SendingRequests;
		TMap<FKnightEntityId, TArray<int32>> Commits;
		TMap<FKnightEntityId, int32> HeavyCommits;
		TMap<FKnightEntityId, int32> LightCommits;
		TArray<FKnightSkillLabEvent> WorldEvents;
	};

	AKnightPartyState* FindParty(UWorld* World)
	{
		const AKnightGameState* State = World != nullptr ? World->GetGameState<AKnightGameState>() : nullptr;
		return State != nullptr ? State->GetPartyState() : nullptr;
	}

	AKnightRPGStateActor* FindRPG(UWorld* World)
	{
		const AKnightRPGGameState* State = World != nullptr ? World->GetGameState<AKnightRPGGameState>() : nullptr;
		return State != nullptr ? State->GetRPGState() : nullptr;
	}

	TArray<FGameplayAbilitySpecHandle> Specs(const UKnightAbilitySystemComponent* ASC)
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
		AKnightSkillLabPlayerController* LocalOwner(UWorld* World) const
		{
			if (World == nullptr || FindParty(World) == nullptr)
			{
				return nullptr;
			}
			for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
			{
				AKnightSkillLabPlayerController* Player = Cast<AKnightSkillLabPlayerController>(It->Get());
				if (Player != nullptr && Player->IsLocalController() && Player->GetControlledKnight() != nullptr &&
					Player->GetControlledKnight()->IsLocallyControlled() &&
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
		void BindPlayer(AKnightSkillLabPlayerController* Player)
		{
			const TWeakPtr<FObservation> Weak(Observation);
			const TWeakObjectPtr<AKnightSkillLabPlayerController> WeakPlayer(Player);
			ReplyHandles.Add(Player->OnRPGCommandResult.AddLambda(
				[Weak, WeakPlayer](const FKnightRPGCommandResult& Reply)
				{
					if (const TSharedPtr<FObservation> Current = Weak.Pin())
					{
						Current->Replies.Add(Reply.RequestId, Reply);
						Current->Recipients.Add(Reply.RequestId, WeakPlayer);
						AKnightCharacter* Knight = WeakPlayer.IsValid() ? WeakPlayer->GetControlledKnight() : nullptr;
						if (Reply.Operation == EKnightRPGOperation::ApplyBuild && Knight != nullptr &&
							Knight->GetKnightAttributeSet() != nullptr && Knight->GetWorld() != nullptr)
						{
							FKnightBuildReceiptAttributes Attributes;
							Attributes.KnightId = Knight->GetKnightEntityId();
							Attributes.Stamina = Knight->GetKnightAttributeSet()->GetStamina();
							Attributes.MaxStamina = Knight->GetKnightAttributeSet()->GetMaxStamina();
							Attributes.ServerTime = Knight->GetWorld()->GetTimeSeconds();
							Attributes.bAuthority = Knight->HasAuthority();
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
			Mode = Server.IsValid() ? Server->GetAuthGameMode<AKnightSkillLabGameMode>() : nullptr;
			AKnightPartyState* Party = FindParty(Server.Get());
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
					AKnightSkillLabPlayerController* Player = LocalOwner(ContextWorld(Context));
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
				AKnightCharacter* Knight = Party->FindSlot(Index)->Knight;
				UKnightAbilitySystemComponent* ASC = Knight->GetKnightAbilitySystem();
				UKnightSkillRuntimeComponent* Runtime = Knight->FindComponentByClass<UKnightSkillRuntimeComponent>();
				if (Runtime == nullptr || Runtime->GetBuild().SpecId.IsNone() ||
					!FMath::IsNearlyEqual(Knight->GetKnightAttributeSet()->GetMaxHealth(), 1000.0f) ||
					!FMath::IsNearlyEqual(Knight->GetKnightAttributeSet()->GetAttackPower(), 100.0f) ||
					!FMath::IsNearlyEqual(Knight->GetKnightAttributeSet()->GetDefense(), 100.0f))
				{
					Fail(TEXT("Genuine SkillLab canonical attributes/kit do not match 1000 HP / 100 Power / 100 Armor."));
					return;
				}
				Knights.Add(Knight);
				Ids.Add(Knight->GetKnightEntityId());
				Heroes.Add(Knight->GetHeroDefinitionId());
				TArray<int32> Counts;
				Counts.Init(0, 4);
				Observation->Commits.Add(Knight->GetKnightEntityId(), Counts);
				Observation->HeavyCommits.Add(Knight->GetKnightEntityId(), 0);
				Observation->LightCommits.Add(Knight->GetKnightEntityId(), 0);
				const TWeakPtr<FObservation> Weak(Observation);
				const TWeakObjectPtr<UKnightSkillRuntimeComponent> WeakRuntime(Runtime);
				const FKnightEntityId Id = Knight->GetKnightEntityId();
				CommitHandles.Add(ASC->AbilityCommittedCallbacks.AddLambda(
					[Weak, WeakRuntime, Id](UGameplayAbility* Ability)
					{
						const TSharedPtr<FObservation> Current = Weak.Pin();
						if (!Current.IsValid() || !WeakRuntime.IsValid() || Ability == nullptr)
						{
							return;
						}
						if (Ability->IsA<UKnightHeavyAttackAbility>())
						{
							++Current->HeavyCommits.FindChecked(Id);
						}
						if (Ability->IsA<UKnightLightAttackAbility>())
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
			for (const TWeakObjectPtr<AKnightSkillLabPlayerController>& Player : Players)
			{
				BindPlayer(Player.Get());
			}
			OriginalSpecHandles = Specs(Host->GetControlledKnight()->GetKnightAbilitySystem());
			UKnightEventSubsystem* Events = Server->GetSubsystem<UKnightEventSubsystem>();
			if (Events == nullptr)
			{
				Fail(TEXT("Actual SkillLab confirmed event adapter bus is missing."));
				return;
			}
			const TWeakPtr<FObservation> WeakObservation(Observation);
			WorldEventSubscription = Events->Subscribe<FKnightSkillLabEvent>(Mode.Get(),
				[WeakObservation](const FKnightSkillLabEvent& Event)
				{
					if (const TSharedPtr<FObservation> Current = WeakObservation.Pin())
					{
						Current->WorldEvents.Add(Event);
					}
				});
			BaseCaptureCount = CaptureCount();
			const FVector Station = Mode->GetUpgradeStation()->GetActorLocation();
			for (int32 Index = 0; Index < Knights.Num(); ++Index)
			{
				Place(Knights[Index].Get(), Station + FVector((Index - 1) * 130.0, -100.0, 100.0));
			}
			Enter(EPhase::Sanctuary);
		}
		bool HasCanonicalIdentities() const
		{
			AKnightPartyState* Party = FindParty(Server.Get());
			if (Party == nullptr || Knights.Num() != 3)
			{
				return false;
			}
			int32 Count = 0;
			for (TActorIterator<AKnightCharacter> It(Server.Get()); It; ++It)
			{
				++Count;
			}
			if (Count != 3)
			{
				return false;
			}
			for (int32 Index = 0; Index < 3; ++Index)
			{
				const FKnightRosterEntry* Entry = Party->FindSlot(Index);
				if (!Knights[Index].IsValid() || Entry == nullptr || Entry->Knight != Knights[Index].Get() ||
					Entry->KnightId != Ids[Index] || Entry->HeroDefinitionId != Heroes[Index] ||
					Knights[Index]->GetKnightAbilitySystem()->GetOwnerActor() != Knights[Index].Get() ||
					Knights[Index]->GetKnightAbilitySystem()->GetAvatarActor() != Knights[Index].Get())
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
				AKnightRPGStateActor* Other = FindRPG(World);
				AKnightPartyState* Party = FindParty(World);
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
				for (const FKnightRosterEntry& Entry : Party->GetRoster())
				{
					UKnightSkillRuntimeComponent* Runtime = Entry.Knight != nullptr ?
						Entry.Knight->FindComponentByClass<UKnightSkillRuntimeComponent>() : nullptr;
					if (Runtime == nullptr || !(Runtime->GetBuild() == Other->GetRPG()->GetKnightView(Entry.KnightId).Build))
					{
						return false;
					}
				}
			}
			return true;
		}
		void Place(AKnightCharacter* Knight, const FVector& Position)
		{
			FScopedConditionalWorldSwitcher Switch(Server.Get());
			Knight->GetCharacterMovement()->StopMovementImmediately();
			Knight->SetActorLocationAndRotation(Position, FRotator::ZeroRotator, false, nullptr, ETeleportType::TeleportPhysics);
			Knight->ForceNetUpdate();
		}
		AKnightCharacter* AuthorityKnight(AKnightSkillLabPlayerController* Player) const
		{
			if (Player == nullptr || Player->GetControlledKnight() == nullptr)
			{
				return nullptr;
			}
			const FKnightEntityId Id = Player->GetControlledKnight()->GetKnightEntityId();
			for (const TWeakObjectPtr<AKnightCharacter>& Knight : Knights)
			{
				if (Knight.IsValid() && Knight->GetKnightEntityId() == Id)
				{
					return Knight.Get();
				}
			}
			return nullptr;
		}
		FKnightRPGCommand CommandFor(AKnightSkillLabPlayerController* Player) const
		{
			AKnightRPGStateActor* Source = FindRPG(Player->GetWorld());
			FKnightRPGCommand Command;
			Command.Operation = EKnightRPGOperation::ApplyBuild;
			Command.RequestId = FGuid::NewGuid();
			Command.ExpectedKnightId = Player->GetControlledKnight()->GetKnightEntityId();
			Command.InventoryId = Source->GetInventory()->GetView().InventoryId;
			Command.ExpectedInventoryRevision = Source->GetInventory()->GetView().Revision;
			const FKnightRPGKnightView Knight = Source->GetRPG()->GetKnightView(Command.ExpectedKnightId);
			Command.ExpectedKnightRevision = Knight.Revision;
			Command.ProposedBuild = Knight.Build;
			Command.WeaponRuneInstanceId = Knight.WeaponRuneInstanceId;
			Command.SkillRuneInstanceId = Knight.SkillRuneInstanceId;
			return Command;
		}
		void Send(AKnightSkillLabPlayerController* Player, const FKnightRPGCommand& Command)
		{
			FScopedConditionalWorldSwitcher Switch(Player->GetWorld());
			Observation->SendingRequests.Add(Command.RequestId);
			Player->RequestRPGCommand(Command);
			Observation->SendingRequests.Remove(Command.RequestId);
		}
		bool ReplyOwned(const FKnightRPGCommand& Command, AKnightSkillLabPlayerController* Expected) const
		{
			const TWeakObjectPtr<AKnightSkillLabPlayerController>* Actual = Observation->Recipients.Find(Command.RequestId);
			return Actual != nullptr && Actual->Get() == Expected;
		}
		void Sanctuary()
		{
			if (Elapsed() < 0.6 || !Converged())
			{
				return;
			}
			for (const TWeakObjectPtr<AKnightSkillLabPlayerController>& Player : Players)
			{
				if (!Mode->IsInUpgradeSafeZone(AuthorityKnight(Player.Get())))
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
			for (const TWeakObjectPtr<AKnightSkillLabPlayerController>& Player : Players)
			{
				FKnightRPGCommand Command = CommandFor(Player.Get());
				Command.ProposedBuild.SpecId = FKnightSkillCatalog::SpecsForHero(
					AuthorityKnight(Player.Get())->GetHeroDefinitionId())[1];
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
			UKnightRPGPanel* Panel = Host->GetRPGPanel();
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
			UKnightRPGPanel* Panel = Host->GetRPGPanel();
			const FKnightRPGPanelSnapshot& Snapshot = Panel->GetSnapshot();
			if (Snapshot.KnightId != Host->GetControlledKnight()->GetKnightEntityId() ||
				Snapshot.Specializations.Num() != 3 || Snapshot.StatRows.Num() < 10 ||
				Snapshot.Runes.Num() != 4 || !Snapshot.bCanEditBuild || CaptureCount() != BaseCaptureCount + 1)
			{
				Fail(TEXT("Actual Character UI lacks confirmed canonical data, three specs, stats, four rune items or one preview."));
				return;
			}
			if (OpenCycles == 0)
			{
				const FKnightRPGRuneRow* Aftershock = Snapshot.Runes.FindByPredicate([](const FKnightRPGRuneRow& Rune)
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
					Panel->SetActiveTab(static_cast<EKnightRPGPanelTab>(Tab));
					int32 CanvasCount = 0;
					int32 ButtonCount = 0;
					Panel->WidgetTree->ForEachWidget([&CanvasCount, &ButtonCount](UWidget* Widget)
					{
						CanvasCount += Cast<UCanvasPanel>(Widget) != nullptr ? 1 : 0;
						ButtonCount += Cast<UKnightRPGActionButton>(Widget) != nullptr ? 1 : 0;
					});
					if ((Tab == static_cast<int32>(EKnightRPGPanelTab::Talents) && CanvasCount != 2) || ButtonCount < 7)
					{
						Fail(TEXT("SkillLab native tabs are missing their CommonButtons or both talent node canvases."));
						return;
					}
				}
				Panel->SetActiveTab(EKnightRPGPanelTab::Character);
				// Mapping is suppressed while the actual CommonUI input layer is active.
				const int32 Before = Observation->Commits.FindChecked(Snapshot.KnightId)[3];
				Host->RequestSkillSlot(4);
				if (Observation->Commits.FindChecked(Snapshot.KnightId)[3] != Before)
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
			BuildStaminaBefore = Host->GetControlledKnight()->GetKnightAttributeSet()->GetStamina();
			BuildMaxStaminaBefore = Host->GetControlledKnight()->GetKnightAttributeSet()->GetMaxStamina();
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
			const FKnightRPGCommandResult* Reply = Observation->Replies.Find(Pending.RequestId);
			if (Reply == nullptr || !Converged())
			{
				return;
			}
			const FKnightBuildReceiptAttributes* ReceiptAttributes = Observation->BuildReceiptAttributes.Find(Pending.RequestId);
			const bool bImmediateNoRefill = ReceiptAttributes != nullptr && ReceiptAttributes->bAuthority &&
				ReceiptAttributes->bSynchronous && ReceiptAttributes->KnightId == Pending.ExpectedKnightId &&
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
			const float ActualStamina = Host->GetControlledKnight()->GetKnightAttributeSet()->GetStamina();
			const bool bLaterStaminaBound = ReceiptAttributes != nullptr &&
				ActualStamina >= ReceiptAttributes->Stamina && ActualStamina <= AllowedStamina;
			if (!ReplyOwned(Pending, Host.Get()) || !Reply->Result.IsSuccess() ||
				!(RPG->GetRPG()->GetKnightView(Pending.ExpectedKnightId).Build == Pending.ProposedBuild) ||
				!FMath::IsNearlyEqual(Host->GetControlledKnight()->GetKnightAttributeSet()->GetMaxStamina(), 110.0f) ||
				!bImmediateNoRefill || !bLaterStaminaBound ||
				Specs(Host->GetControlledKnight()->GetKnightAbilitySystem()) != OriginalSpecHandles)
			{
				const FKnightRPGKnightView Confirmed = RPG->GetRPG()->GetKnightView(Pending.ExpectedKnightId);
				const UKnightAttributeSet* Attributes = Host->GetControlledKnight()->GetKnightAttributeSet();
				const TArray<FGameplayAbilitySpecHandle> ActualHandles = Specs(
					Host->GetControlledKnight()->GetKnightAbilitySystem());
				const TWeakObjectPtr<AKnightSkillLabPlayerController>* Recipient =
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
					"request=%s Knight=%s/reply%s; expected revisions Knight%lld/inventory%lld, "
					"confirmed Knight%lld/inventory%lld, receipt Knight%lld/inventory%lld, replay%d; "
					"serverTime%.9g phaseElapsed%.9g openCycles%d."),
					ReplyOwned(Pending, Host.Get()), Reply->Result.IsSuccess(), Confirmed.Build == Pending.ProposedBuild,
					FMath::IsNearlyEqual(Attributes->GetMaxStamina(), 110.0f),
					bImmediateNoRefill, ActualHandles == OriginalSpecHandles,
					Attributes->GetMaxStamina(), Attributes->GetStamina(), static_cast<int32>(Reply->Result.Error),
					*Reply->Result.Context.ToString(), *Reply->Result.Message.ToString(),
					*GetNameSafe(Recipient != nullptr ? Recipient->Get() : nullptr), *GetNameSafe(Host.Get()),
					*Pending.RequestId.ToString(), *Pending.ExpectedKnightId.Value.ToString(), *Reply->KnightId.Value.ToString(),
					Pending.ExpectedKnightRevision, Pending.ExpectedInventoryRevision, Confirmed.Revision,
					RPG->GetInventory()->GetView().Revision, Reply->CommittedKnightRevision,
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
			UKnightRPGPanel* Panel = Host->GetRPGPanel();
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
			UKnightRPGPanel* Panel = Host->GetRPGPanel();
			const FKnightRPGKnightView Confirmed = RPG->GetRPG()->GetKnightView(Pending.ExpectedKnightId);
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
			const FKnightRPGCommandResult* Reply = Observation->Replies.Find(Pending.RequestId);
			if (Reply == nullptr)
			{
				return;
			}
			if (Reply->Result.Error != EKnightError::InvalidRevision ||
				!(RPG->GetRPG()->GetView() == BeforeRejectedRPG) ||
				!(RPG->GetInventory()->GetView() == BeforeRejectedInventory) ||
				Specs(Host->GetControlledKnight()->GetKnightAbilitySystem()) != OriginalSpecHandles)
			{
				Fail(TEXT("Actual stale build RPC did not reject without changing domains or grants."));
				return;
			}
			Test.AddInfo(TEXT("Sanctuary build applied through owning controller; stale revision rejected with exact RPG/inventory/spec preservation."));
			for (TActorIterator<AKnightPartyArenaStartMarker> It(Server.Get()); It; ++It)
			{
				Place(Host->GetControlledKnight(), It->GetActorLocation() + FVector(0.0, -150.0, 100.0));
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
				const FKnightRPGCommandResult* Reply = Observation->Replies.Find(Race[Index].RequestId);
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
					if (!(RPG->GetRPG()->GetKnightView(Race[Index].ExpectedKnightId).Build == Race[Index].ProposedBuild))
					{
						Fail(TEXT("Successful concurrent command did not publish its exact proposed Knight build."));
						return false;
					}
					++Successes;
					WinnerIndex = Index;
				}
				else if (Reply->Result.Error != EKnightError::InvalidRevision && Reply->Result.Error != EKnightError::Conflict)
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
			Test.AddInfo(TEXT("Concurrent talent changes for two separately owned Knights both committed; inventory-free changes preserved the shared inventory revision."));
			PrepareRuneRace();
		}
		void PrepareRuneRace()
		{
			const FKnightInventoryView Inventory = RPG->GetInventory()->GetView();
			const FKnightItemInstance* Rune = Inventory.Items.FindByPredicate(
				[](const FKnightItemInstance& Item)
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
			for (const TWeakObjectPtr<AKnightSkillLabPlayerController>& Player : Players)
			{
				FKnightRPGCommand Command = CommandFor(Player.Get());
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
			for (const FKnightRPGKnightView& Knight : RPG->GetRPG()->GetView().Knights)
			{
				Attached += Knight.SkillRuneInstanceId == RuneInstanceId ? 1 : 0;
			}
			Winner = Players[WinnerIndex];
			if (Attached != 1 || AuthorityKnight(Winner.Get())->FindComponentByClass<UKnightSkillRuntimeComponent>()->
				GetBuild().SkillRune != TEXT("Skill.Propagation"))
			{
				Fail(TEXT("Same-instance Attach Rune race did not preserve exclusive ownership and actual runtime tuning."));
				return;
			}
			Test.AddInfo(TEXT("Two latency-emulated clients applied independent builds then raced the same Propagation item: one attachment success, one stale/conflict, exact owner responses, exclusive rune and both client snapshots converged."));
			for (TActorIterator<AKnightRunestone> It(Server.Get()); It; ++It)
			{
				if (It->GetKind() == EKnightRunestoneKind::Crystal)
				{
					CrystalStone = *It;
					break;
				}
			}
			if (!CrystalStone.IsValid() || CrystalStone->GetState() != EKnightRunestoneState::Ready)
			{
				Fail(TEXT("Actual ready RS.Crystal actor is missing from the playable map."));
				return;
			}
			CrystalBasePowers.Reset();
			for (const TWeakObjectPtr<AKnightCharacter>& Knight : Knights)
			{
				CrystalBasePowers.Add(Knight->GetKnightAttributeSet()->GetAttackPower());
			}
			for (int32 Index = 0; Index < Players.Num(); ++Index)
			{
				Place(AuthorityKnight(Players[Index].Get()),
					CrystalStone->GetActorLocation() + FVector(-120.0, Index == 0 ? -60.0 : 60.0, 40.0));
			}
			Enter(EPhase::CrystalApproach);
		}
		AKnightRunestone* ClientCrystalStone(UWorld* World) const
		{
			for (TActorIterator<AKnightRunestone> It(World); It; ++It)
			{
				if (It->GetKind() == EKnightRunestoneKind::Crystal)
				{
					return *It;
				}
			}
			return nullptr;
		}
		AKnightRunestonePickup* ClientCrystalPickup(UWorld* World) const
		{
			for (TActorIterator<AKnightRunestonePickup> It(World); It; ++It)
			{
				if (It->GetKind() == EKnightRunestonePickupKind::PowerCrystal)
				{
					return *It;
				}
			}
			return nullptr;
		}
		int32 CountCrystalEvents(FName EventName) const
		{
			int32 Count = 0;
			for (const FKnightSkillLabEvent& Event : Observation->WorldEvents)
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
			for (const TWeakObjectPtr<AKnightSkillLabPlayerController>& Player : Players)
			{
				AKnightRunestone* Replica = ClientCrystalStone(Player->GetWorld());
				if (Replica == nullptr || Replica->GetState() != EKnightRunestoneState::Ready ||
					FVector::DistSquared(Player->GetControlledKnight()->GetActorLocation(), Replica->GetActorLocation()) > FMath::Square(200.0))
				{
					return;
				}
			}
			PulseInteractions();
			Enter(EPhase::CrystalChannel);
		}
		void WaitCrystalChannel()
		{
			if (CrystalStone->GetState() != EKnightRunestoneState::Cooldown)
			{
				return;
			}
			for (int32 Index = 0; Index < Knights.Num(); ++Index)
			{
				if (!FMath::IsNearlyEqual(Knights[Index]->GetKnightAttributeSet()->GetAttackPower(), CrystalBasePowers[Index]))
				{
					Fail(TEXT("Actual mapped Runestone activation incorrectly buffed a party Knight before collection."));
					return;
				}
			}
			int32 PickupCount = 0;
			for (TActorIterator<AKnightRunestonePickup> It(Server.Get()); It; ++It)
			{
				if (It->GetKind() == EKnightRunestonePickupKind::PowerCrystal)
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
				AKnightRunestone* Replica = ClientCrystalStone(Players[Index]->GetWorld());
				AKnightRunestonePickup* PickupReplica = ClientCrystalPickup(Players[Index]->GetWorld());
				if (Replica == nullptr || Replica->GetState() != EKnightRunestoneState::Cooldown ||
					PickupReplica == nullptr || PickupReplica->GetRemainingCharges() != 1)
				{
					return;
				}
			}
			for (int32 Index = 0; Index < Players.Num(); ++Index)
			{
				Place(AuthorityKnight(Players[Index].Get()),
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
			for (const TWeakObjectPtr<AKnightSkillLabPlayerController>& Player : Players)
			{
				AKnightRunestonePickup* Replica = ClientCrystalPickup(Player->GetWorld());
				if (Replica == nullptr || FVector::DistSquared(Player->GetControlledKnight()->GetActorLocation(),
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
			for (int32 Index = 0; Index < Knights.Num(); ++Index)
			{
				const float Power = Knights[Index]->GetKnightAttributeSet()->GetAttackPower();
				if (FMath::IsNearlyEqual(Power, CrystalBasePowers[Index] * 1.15f))
				{
					++BuffedCount;
				}
				else if (!FMath::IsNearlyEqual(Power, CrystalBasePowers[Index]))
				{
					Fail(TEXT("Crystal race produced an unexpected Power value on a canonical Knight."));
					return;
				}
			}
			if (BuffedCount != 1 || CountCrystalEvents(TEXT("PickupConsumed")) != 1)
			{
				Fail(TEXT("Mapped F pickup race did not have exactly one buffed collector and one confirmed charge consumption."));
				return;
			}
			for (const FKnightSkillLabEvent& Event : Observation->WorldEvents)
			{
				if (Event.SubjectId == TEXT("RS.Crystal") && (!Event.EncounterId.IsValid() || !Event.RequestId.IsValid()
					|| !Event.SourceId.IsValid() || !Event.TargetId.IsValid() || !Event.ConfirmationId.IsValid() || Event.Revision < 1))
				{
					Fail(TEXT("Actual world adapter event omitted required encounter/request/source/target/confirmation/revision metadata."));
					return;
				}
			}
			for (const TWeakObjectPtr<AKnightSkillLabPlayerController>& Player : Players)
			{
				AKnightRunestonePickup* Replica = ClientCrystalPickup(Player->GetWorld());
				if (Replica == nullptr || Replica->GetRemainingCharges() != 0)
				{
					return;
				}
				for (const TWeakObjectPtr<AKnightCharacter>& Knight : Knights)
				{
					const FKnightRosterEntry* Entry = FindParty(Player->GetWorld())->FindSlot(
						FindParty(Server.Get())->GetControlledSlot(Knight->GetController()));
					if (Entry == nullptr || Entry->Knight == nullptr || !FMath::IsNearlyEqual(
						Entry->Knight->GetKnightAttributeSet()->GetAttackPower(), Knight->GetKnightAttributeSet()->GetAttackPower()))
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
				for (TActorIterator<AKnightAbilityDummy> It(Server.Get()); It; ++It)
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
				for (const TWeakObjectPtr<AKnightCharacter>& Knight : Knights)
				{
					Place(Knight.Get(), SkillTarget->GetActorLocation() + FVector(-150.0, 150.0 * OffsetIndex++, 0.0));
				}
				ActiveSlot = 1;
				Enter(EPhase::Skills);
			}
		}
		int32 CommitCount(int32 Action) const
		{
			return Observation->Commits.FindChecked(AuthorityKnight(Winner.Get())->GetKnightEntityId())[Action - 1];
		}
		FString SkillDiagnostics() const
		{
			AKnightCharacter* Knight = AuthorityKnight(Winner.Get());
			if (Knight == nullptr)
			{
				return TEXT("No owned skill-test Knight.");
			}
			const UKnightAbilitySystemComponent* ASC = Knight->GetKnightAbilitySystem();
			const UKnightSkillRuntimeComponent* Runtime = Knight->FindComponentByClass<UKnightSkillRuntimeComponent>();
			if (ASC == nullptr || Runtime == nullptr || Knight->GetKnightAttributeSet() == nullptr)
			{
				return TEXT("Skill-test Knight is missing actual ASC/runtime/attributes.");
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
			const FKnightSkillRuntimeState State = Runtime->GetSkillState();
			const TArray<int32>* Counts = Observation->Commits.Find(Knight->GetKnightEntityId());
			const double CooldownEnd = State.CooldownEnds.IsValidIndex(ActiveSlot - 1)
				? State.CooldownEnds[ActiveSlot - 1] : -1.0;
			return FString::Printf(TEXT("Skill slot%d sent%d spec%s HP%.2f resource%.2f time%.2f CDend%.2f commits%d active[%s] tags[%s] panel%d moveLock%d lookLock%d position%s"),
				ActiveSlot, bSkillSent, *State.Build.SpecId.ToString(), Knight->GetKnightAttributeSet()->GetHealth(),
				Knight->GetKnightAttributeSet()->GetRuneEnergy(), Server->GetTimeSeconds(), CooldownEnd,
				Counts != nullptr && Counts->IsValidIndex(ActiveSlot - 1) ? (*Counts)[ActiveSlot - 1] : -1,
				*ActiveAbilities, *Tags.ToStringSimple(),
				Winner->GetRPGPanel() != nullptr && Winner->GetRPGPanel()->IsActivated(),
				Winner->IsMoveInputIgnored(), Winner->IsLookInputIgnored(), *Knight->GetActorLocation().ToCompactString());
		}
		void WaitSkills()
		{
			AKnightCharacter* Knight = AuthorityKnight(Winner.Get());
			UKnightSkillRuntimeComponent* Runtime = Knight->FindComponentByClass<UKnightSkillRuntimeComponent>();
			if (!bSkillSent)
			{
				if (!Runtime->IsSkillAvailable(ActiveSlot))
				{
					return;
				}
				for (const FGameplayAbilitySpec& Spec : Knight->GetKnightAbilitySystem()->GetActivatableAbilities())
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
					Place(Knight, SkillTarget->GetActorLocation() - FVector(150.0, 0.0, 0.0));
				}
				CommitsBefore = CommitCount(ActiveSlot);
				Test.AddInfo(SkillDiagnostics());
				Pulse(Winner.Get(), ActiveSlot == 1 ? EKeys::Z : ActiveSlot == 2 ? EKeys::X :
					ActiveSlot == 3 ? EKeys::C : EKeys::V);
				bSkillSent = true;
				return;
			}
			const FKnightSkillRuntimeState State = Runtime->GetSkillState();
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
				TransferKnight = Knight;
				TransferBuild = Runtime->GetBuild();
				TransferCooldown = State.CooldownEnds[3];
				TransferSpecs = Specs(Knight->GetKnightAbilitySystem());
				TransferResource = Knight->GetKnightAttributeSet()->GetRuneEnergy();
				TransferTime = Server->GetTimeSeconds();
				TransferSlot = FindParty(Server.Get())->GetControlledSlot(Knight->GetController());
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
				ReconnectURL = FString::Printf(TEXT("127.0.0.1:%d?KnightSlot=%d"), Address->GetPort(), TransferSlot);
				FScopedConditionalWorldSwitcher Switch(Winner->GetWorld());
				Winner->ClientTravel(DetachedMap, TRAVEL_Absolute);
				Enter(EPhase::Disconnect);
			}
			else
			{
				FScopedConditionalWorldSwitcher Switch(Server.Get());
				Host->RequestSwitchKnight((TransferSlot + 1) % 3);
				Enter(EPhase::SwitchAway);
			}
		}
		void WaitPadSkill()
		{
			AKnightCharacter* Knight = AuthorityKnight(Winner.Get());
			UKnightSkillRuntimeComponent* Runtime = Knight->FindComponentByClass<UKnightSkillRuntimeComponent>();
			if (!bPadModifierHeld)
			{
				if (!Runtime->IsSkillAvailable(1))
				{
					return;
				}
				for (const FGameplayAbilitySpec& Spec : Knight->GetKnightAbilitySystem()->GetActivatableAbilities())
				{
					if (Spec.IsActive())
					{
						return;
					}
				}
				PadCommitsBefore = CommitCount(1);
				PadHeavyCommitsBefore = Observation->HeavyCommits.FindChecked(Knight->GetKnightEntityId());
				PadModifierTime = Server->GetTimeSeconds();
				bPadModifierHeld = true;
				FScopedConditionalWorldSwitcher Switch(Winner->GetWorld());
				Winner->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::Gamepad_LeftTrigger, IE_Pressed, 1.0f));
				return;
			}
			if (Observation->HeavyCommits.FindChecked(Knight->GetKnightEntityId()) != PadHeavyCommitsBefore)
			{
				Fail(TEXT("Holding the actual SkillLab LT modifier committed the legacy heavy attack."));
				return;
			}
			for (const FGameplayAbilitySpec& Spec : Knight->GetKnightAbilitySystem()->GetActivatableAbilities())
			{
				if (Spec.IsActive() && Spec.Ability->IsA<UKnightHeavyAttackAbility>())
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
			const FKnightSkillRuntimeState State = Runtime->GetSkillState();
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
			AKnightCharacter* Knight = AuthorityKnight(Winner.Get());
			const FKnightEntityId Id = Knight->GetKnightEntityId();
			if (!bPadModifierHeld)
			{
				for (const FGameplayAbilitySpec& Spec : Knight->GetKnightAbilitySystem()->GetActivatableAbilities())
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
			for (const FGameplayAbilitySpec& Spec : Knight->GetKnightAbilitySystem()->GetActivatableAbilities())
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
			if (!TransferKnight.IsValid())
			{
				return false;
			}
			const UKnightSkillRuntimeComponent* Runtime = TransferKnight->FindComponentByClass<UKnightSkillRuntimeComponent>();
			const FKnightSkillRuntimeState State = Runtime->GetSkillState();
			// AdvanceRuntime restores one point every 0.1s; ceil includes its existing timer phase, not a refill.
			constexpr float NativeResourceTickIntervalSeconds = 0.1f;
			constexpr float NativeResourcePerTick = 1.0f;
			const double ResourceElapsed = FMath::Max(0.0, Server->GetTimeSeconds() - TransferTime);
			const int32 MaximumResourceTicks = FMath::CeilToInt(ResourceElapsed / NativeResourceTickIntervalSeconds);
			const float AllowedResource = FMath::Min(TransferKnight->GetKnightAttributeSet()->GetMaxRuneEnergy(),
				TransferResource + MaximumResourceTicks * NativeResourcePerTick);
			const bool bPreserved = State.Build == TransferBuild && State.CooldownEnds.IsValidIndex(3) &&
				FMath::IsNearlyEqual(State.CooldownEnds[3], TransferCooldown) &&
				Specs(TransferKnight->GetKnightAbilitySystem()) == TransferSpecs &&
				TransferKnight->GetKnightAttributeSet()->GetRuneEnergy() <= AllowedResource;
			if (!bPreserved)
			{
				Test.AddInfo(FString::Printf(TEXT("Transfer preservation detail: buildMatch=%d handlesMatch=%d oldHandles=%d newHandles=%d oldCD=%.6f newCD=%.6f capturedResource=%.3f actualResource=%.3f allowedResource=%.3f elapsed=%.6f."),
					State.Build == TransferBuild, Specs(TransferKnight->GetKnightAbilitySystem()) == TransferSpecs,
					TransferSpecs.Num(), Specs(TransferKnight->GetKnightAbilitySystem()).Num(), TransferCooldown,
					State.CooldownEnds.IsValidIndex(3) ? State.CooldownEnds[3] : -1.0,
					TransferResource, TransferKnight->GetKnightAttributeSet()->GetRuneEnergy(), AllowedResource,
					Server->GetTimeSeconds() - TransferTime));
			}
			return bPreserved;
		}
		void WaitSwitchAway()
		{
			if (Host->GetControlledKnight() == TransferKnight.Get() || Elapsed() < 0.8)
			{
				return;
			}
			if (!TransferPreserved())
			{
				Fail(TEXT("Solo human/AI switch reset build, grants, cooldown or resource."));
				return;
			}
			FScopedConditionalWorldSwitcher Switch(Server.Get());
			Host->RequestSwitchKnight(TransferSlot);
			Enter(EPhase::SwitchBack);
		}
		void WaitSwitchBack()
		{
			if (Host->GetControlledKnight() != TransferKnight.Get())
			{
				return;
			}
			if (!TransferPreserved())
			{
				Fail(TEXT("Returning to the same canonical Knight reset combat build state."));
				return;
			}
			Test.AddInfo(TEXT("Solo switch away/back preserved Character ASC, same skill handles/build, running SpecMajor cooldown and bounded natural resource regeneration."));
			BeginCleanup();
		}
		void WaitDisconnect()
		{
			UWorld* Detached = ContextWorld(DepartingContext);
			const FKnightRosterEntry* Entry = FindParty(Server.Get())->FindSlot(TransferSlot);
			if (Detached == nullptr || Detached->GetNetMode() != NM_Standalone || Detached->GetNetDriver() != nullptr ||
				Detached->GetFirstPlayerController() == nullptr || Entry->OwnerKind != EKnightControllerOwner::Companion ||
				Cast<AAIController>(TransferKnight->GetController()) == nullptr ||
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
			AKnightSkillLabPlayerController* Rejoined = LocalOwner(ContextWorld(DepartingContext));
			if (Rejoined == nullptr || Rejoined->GetWorld()->GetNetMode() != NM_Client ||
				Rejoined->GetPlayerState<AKnightPlayerState>() == nullptr || Rejoined->GetControlledKnight() == nullptr ||
				Rejoined->GetPlayerState<AKnightPlayerState>()->GetAssignedSlot() != TransferSlot ||
				FindParty(Server.Get())->GetHumanCount() != 3 || !Converged())
			{
				return;
			}
			if (!TransferPreserved() ||
				Specs(Rejoined->GetControlledKnight()->GetKnightAbilitySystem()) != TransferSpecs)
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
			for (const TWeakObjectPtr<AKnightSkillLabPlayerController>& Player : Players)
			{
				FScopedConditionalWorldSwitcher Switch(Player->GetWorld());
				Player->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::F, IE_Pressed, 1.0f));
				HeldInteractPlayers.Add(Player);
			}
		}
		void ReleaseInteractions()
		{
			for (const TWeakObjectPtr<AKnightSkillLabPlayerController>& Player : HeldInteractPlayers)
			{
				if (Player.IsValid())
				{
					FScopedConditionalWorldSwitcher Switch(Player->GetWorld());
					Player->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::F, IE_Released, 0.0f));
				}
			}
			HeldInteractPlayers.Reset();
		}
		void Pulse(AKnightSkillLabPlayerController* Player, FKey Key)
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
			WorldEventSubscription = FKnightEventSubscription();
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
				if (Knights[Index].IsValid())
				{
					Knights[Index]->GetKnightAbilitySystem()->AbilityCommittedCallbacks.Remove(CommitHandles[Index]);
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
		TWeakObjectPtr<AKnightSkillLabGameMode> Mode;
		TWeakObjectPtr<AKnightRPGStateActor> RPG;
		TWeakObjectPtr<AKnightSkillLabPlayerController> Host;
		TWeakObjectPtr<AKnightSkillLabPlayerController> Winner;
		TWeakObjectPtr<AKnightAbilityDummy> SkillTarget;
		TWeakObjectPtr<AKnightRunestone> CrystalStone;
		TWeakObjectPtr<AKnightRunestonePickup> CrystalPickup;
		TArray<float> CrystalBasePowers;
		FKnightEventSubscription WorldEventSubscription;
		TArray<TWeakObjectPtr<AKnightSkillLabPlayerController>> HeldInteractPlayers;
		double InteractPulseTime = 0.0;
		TArray<FName> ClientContexts;
		TArray<TWeakObjectPtr<AKnightSkillLabPlayerController>> Players;
		TArray<TWeakObjectPtr<AKnightSkillLabPlayerController>> ObservedPlayers;
		TArray<FDelegateHandle> ReplyHandles;
		TArray<FDelegateHandle> CommitHandles;
		TArray<TWeakObjectPtr<AKnightCharacter>> Knights;
		TArray<FKnightEntityId> Ids;
		TArray<FPrimaryAssetId> Heroes;
		TArray<FGameplayAbilitySpecHandle> OriginalSpecHandles;
		FKnightRPGCommand Pending;
		FKnightRPGCommand OldCommand;
		TArray<FKnightRPGCommand> Race;
		FKnightRPGView BeforeRejectedRPG;
		FKnightInventoryView BeforeRejectedInventory;
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
		TWeakObjectPtr<AKnightSkillLabPlayerController> PulsePlayer;
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
		TWeakObjectPtr<AKnightCharacter> TransferKnight;
		FKnightSkillBuild TransferBuild;
		double TransferCooldown = 0.0;
		float TransferResource = 0.0f;
		double TransferTime = 0.0;
		int32 TransferSlot = INDEX_NONE;
		TArray<FGameplayAbilitySpecHandle> TransferSpecs;
		FName DepartingContext;
		FString ReconnectURL;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKnightSkillLabSoloIntegrationTest,
	"Knight.M55.Integration.SoloUIInputAndBuild",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FKnightSkillLabSoloIntegrationTest::RunTest(const FString& Parameters)
{
	FAutomationTestFramework::Get().EnqueueLatentCommand(
		MakeShared<KnightSkillLabIntegration::FSkillLabCommand>(*this, false));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKnightSkillLabNetworkIntegrationTest,
	"Knight.M55.Integration.ListenClientBuildRaceAndTakeover",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FKnightSkillLabNetworkIntegrationTest::RunTest(const FString& Parameters)
{
	FAutomationTestFramework::Get().EnqueueLatentCommand(
		MakeShared<KnightSkillLabIntegration::FSkillLabCommand>(*this, true));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKnightRPGUIClosedReceiptTest,
	"Knight.M56.UI.ClosedReceiptReconciliation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FKnightRPGUIClosedReceiptTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<UGameInstance> Instance(NewObject<UGameInstance>(GEngine));
	Instance->InitializeStandalone();
	UWorld* World = Instance->GetWorld();
	AKnightRPGUIPlayerController* Controller = World->SpawnActor<AKnightRPGUIPlayerController>();
	if (!TestNotNull(TEXT("Native receipt fixture owns an actual controller"), Controller))
	{
		Instance->Shutdown();
		World->DestroyWorld(false);
		GEngine->DestroyWorldContext(World);
		return false;
	}
	Controller->RPGPanel = NewObject<UKnightRPGPanel>(Controller);
	UKnightRPGPanel* Panel = Controller->RPGPanel;
	TArray<FKnightRPGPanelCommand> BuildCommands;
	TArray<FKnightGearPanelIntent> FixtureCommands;
	const FDelegateHandle BuildHandle = Panel->OnCommandRequested.AddLambda(
		[&BuildCommands](const FKnightRPGPanelCommand& Command)
		{
			BuildCommands.Add(Command);
		});
	const FDelegateHandle FixtureHandle = Panel->OnGearCommandRequested.AddLambda(
		[&FixtureCommands](const FKnightGearPanelIntent& Command)
		{
			FixtureCommands.Add(Command);
		});
	FKnightRPGPanelSnapshot Confirmed;
	Confirmed.KnightId = FKnightEntityId::Create();
	Confirmed.Build.SpecId = TEXT("Uranus");
	Confirmed.bCanEditBuild = true;
	Panel->SetSnapshot(Confirmed);
	Panel->StageChoice(true, TEXT("Reservoir"));
	Panel->SubmitCommand(EKnightRPGPanelAction::ApplyBuild);
	if (TestEqual(TEXT("Native staging creates one pending request"), BuildCommands.Num(), 1))
	{
		Controller->PendingRequestId = BuildCommands[0].RequestId;
		FKnightRPGCommandResult Receipt;
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
		Controller->HandleGearFixtureResult(FGuid::NewGuid(), FKnightResult::Success());
		TestTrue(TEXT("An unrelated closed fixture receipt preserves the exact pending request"),
			Panel->HasPendingCommand() && Panel->GetPendingRequestId() == FixtureCommands[0].RequestId);
		Controller->HandleGearFixtureResult(FixtureCommands[0].RequestId, FKnightResult::Success());
		TestFalse(TEXT("The matching closed fixture receipt reconciles widget and controller Pending"),
			Panel->HasPendingCommand() || Controller->PendingRequestId.IsValid());
		Controller->HandleRPGResult(FKnightRPGCommandResult());
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
