#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Campaign/KnightCampaignPlayerController.h"
#include "Campaign/KnightEncounterQualityGameMode.h"
#include "Boss/KnightBossPhaseComponent.h"
#include "Animation/AnimMontage.h"
#include "Character/KnightCharacter.h"
#include "Components/KnightAbilitySystemComponent.h"
#include "Components/KnightCombatComponent.h"
#include "Components/KnightLifeComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "GenericPlatform/GenericPlatformInputDeviceMapper.h"
#include "Framework/Application/SlateApplication.h"
#include "ImageUtils.h"
#include "InputKeyEventArgs.h"
#include "Inventory/KnightInventoryComponent.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "Party/KnightPartyState.h"
#include "PartyArena/KnightArenaEnemy.h"
#include "PlayInEditorDataTypes.h"
#include "Progression/KnightRPGComponent.h"
#include "RPGLoop/KnightRPGStateActor.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Skills/KnightSkillRuntimeComponent.h"
#include "Story/KnightStoryState.h"
#include "SkillLab/KnightBossArmorComponent.h"
#include "SkillLab/KnightFrozenSurface.h"
#include "SkillLab/KnightRunestone.h"
#include "Tags/KnightGameplayTags.h"
#include "UObject/StrongObjectPtr.h"
#include "UnrealEngine.h"
#include "Widgets/KnightRPGPanel.h"
#include "Widgets/SViewport.h"

namespace KnightEncounterQualityTests
{
	class FRun final : public IAutomationLatentCommand
	{
	public:
		FRun(FAutomationTestBase& InTest, const FString& InScenario)
			: Test(InTest), Scenario(InScenario), Started(FPlatformTime::Seconds())
		{
			TArray<FString> Parts;
			Scenario.ParseIntoArray(Parts, TEXT("."));
			Difficulty = FName(*Parts[0]);
			Composition = FName(*Parts[1]);
			Humans = FCString::Atoi(*Parts[2]);
		}
		~FRun()
		{
			for (const TWeakObjectPtr<AKnightCampaignPlayerController>& Player : Players)
			{
				if (Player.IsValid())
				{
					Player->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::W, IE_Released, 0.0f));
					Player->ResetControlInput();
				}
			}
			if (GEditor->IsPlaySessionInProgress())
			{
				GEditor->EndPlayMap();
			}
		}
		virtual bool Update() override
		{
			if (Test.HasAnyErrors())
			{
				return true;
			}
			if (FPlatformTime::Seconds() - Started > 180.0)
			{
				Record(TEXT("TIMEOUT"));
				Test.AddError(FString::Printf(TEXT("Forge trial timeout phase%d composition=%s humans=%d"), Phase, *Composition.ToString(), Humans));
				return true;
			}
			if (Phase == 0)
			{
				Settings.Reset(DuplicateObject<ULevelEditorPlaySettings>(GetDefault<ULevelEditorPlaySettings>(), GetTransientPackage()));
				Settings->SetPlayNetMode(Humans == 1 ? PIE_Standalone : PIE_ListenServer);
				Settings->SetPlayNumberOfClients(Humans);
				Settings->SetRunUnderOneProcess(true);
				Settings->SetServerPort(17978);
				Settings->EnableGameSound = false;
				Settings->GameGetsMouseControl = false;
				FRequestPlaySessionParams Request;
				Request.EditorPlaySettings = Settings.Get();
				Request.GlobalMapOverride = TEXT("/Game/Knight/Maps/Playables/L_M07_5_ForgeTrial");
				Request.bAllowOnlineSubsystem = false;
				GEditor->RequestPlaySession(Request);
				Phase = 1;
				return false;
			}
			if (Phase == 7)
			{
				return !GEditor->IsPlaySessionInProgress();
			}
			RefreshSources();
			if (Phase == 8 && Mode.IsValid() && Mode->GetPartyState()->GetHumanCount() == 1)
			{
				Test.TestEqual(TEXT("Boss disconnect transfers control without another canonical Knight"), Mode->GetPartyState()->GetRoster().Num(), 3);
				GEditor->RequestLateJoin();
				Phase = 9;
				return false;
			}
			if (!Mode.IsValid() || !Host.IsValid() || Players.Num() != Humans
				|| !Mode->GetStoryInitializationResult().IsSuccess() || Mode->GetPartyState()->GetHumanCount() != Humans)
			{
				return false;
			}
			if (Phase == 9)
			{
				bool bConverged = false;
				for (const TWeakObjectPtr<AKnightCampaignPlayerController>& Player : Players)
				{
					if (Player != Host && Player.IsValid())
					{
						AKnightStoryState* State = Player->GetWorld()->GetGameState<AKnightStoryState>();
						for (TActorIterator<AKnightArenaEnemy> It(Player->GetWorld()); It; ++It)
						{
							UKnightBossArmorComponent* Armor = It->FindComponentByClass<UKnightBossArmorComponent>();
							bConverged |= Armor != nullptr && Armor->GetArmorZone() != nullptr && State != nullptr
								&& State->WorldState->GetSnapshot().EncounterRunId == Mode->GetStoryState()->WorldState->GetSnapshot().EncounterRunId;
						}
					}
				}
				if (!bConverged)
				{
					return false;
				}
				for (int32 Index = 0; Index < 3; ++Index)
				{
					Test.TestTrue(TEXT("Boss late join preserves the same canonical IDs"),
						Mode->GetPartyState()->GetRoster()[Index].KnightId == JoinIds[Index]);
				}
				Test.AddInfo(TEXT("Actual disconnect/rejoin reconstructed the active encounter generation and replicated boss armor."));
				Phase = 4;
			}
			if (Phase == 1)
			{
				if (!Host->GetCampaignPresentation().bEnabled)
				{
					return false;
				}
				Test.TestEqual(TEXT("Canonical party remains exactly three"), Mode->GetPartyState()->GetRoster().Num(), 3);
				Test.TestNotNull(TEXT("Rendered canonical Knight has a real skeletal mesh"), Host->GetControlledKnight()->GetMesh()->GetSkeletalMeshAsset());
				Host->RequestCampaignAction(TEXT("Difficulty"), Difficulty);
				Phase = 2;
				return false;
			}
			if (Phase == 2)
			{
				if (Host->GetCampaignPresentation().bPending)
				{
					return false;
				}
				Test.TestTrue(TEXT("Host difficulty request confirms through campaign RPC"), Host->GetLastCampaignResult().IsSuccess());
				Test.TestEqual(TEXT("Authoritative difficulty fact"), Mode->GetDifficulty(), Difficulty);
				Phase = 3;
			}
			if (Phase == 3)
			{
				if (BuildRequest.IsValid())
				{
					const FKnightRPGCommandResult& Reply = BuildPlayer->GetLastRPGResult();
					if (Reply.RequestId != BuildRequest)
					{
						return false;
					}
					Test.TestTrue(TEXT("Sanctuary build uses confirmed RPG transaction"), Reply.Result.IsSuccess());
					BuildRequest.Invalidate();
					++BuildIndex;
				}
				if (BuildIndex < 3)
				{
					const FKnightRosterEntry& Entry = Mode->GetPartyState()->GetRoster()[BuildIndex];
					AKnightCampaignPlayerController* Sender = Host.Get();
					for (const TWeakObjectPtr<AKnightCampaignPlayerController>& Player : Players)
					{
						if (Player->GetControlledKnight() != nullptr && Player->GetControlledKnight()->GetKnightEntityId() == Entry.KnightId)
						{
							Sender = Player.Get();
						}
					}
					AKnightStoryState* State = Sender->GetWorld()->GetGameState<AKnightStoryState>();
					if (State == nullptr || State->GetRPGState() == nullptr)
					{
						return false;
					}
					const FKnightRPGKnightView Knight = State->GetRPGState()->GetRPG()->GetKnightView(Entry.KnightId);
					if (!Knight.IsInitialized())
					{
						return false;
					}
					FKnightRPGCommand Command;
					Command.Operation = EKnightRPGOperation::ApplyBuild;
					Command.RequestId = FGuid::NewGuid();
					Command.ExpectedKnightId = Entry.KnightId;
					Command.InventoryId = State->GetRPGState()->GetInventory()->GetView().InventoryId;
					Command.ExpectedInventoryRevision = State->GetRPGState()->GetInventory()->GetView().Revision;
					Command.ExpectedKnightRevision = Knight.Revision;
					Command.ProposedBuild = Knight.Build;
					if (Composition != TEXT("Mixed"))
					{
						const EKnightSkillRole Desired = Composition == TEXT("DPS") ? EKnightSkillRole::Damage
							: Composition == TEXT("Tanks") ? EKnightSkillRole::Tank : EKnightSkillRole::Healer;
						for (const FName Spec : FKnightSkillCatalog::SpecsForHero(Entry.HeroDefinitionId))
						{
							if (FKnightSkillCatalog::Role(Spec) == Desired)
							{
								Command.ProposedBuild.SpecId = Spec;
							}
						}
					}
					BuildPlayer = Sender;
					BuildRequest = Command.RequestId;
					Sender->RequestRPGCommand(Command);
					return false;
				}
				if (Humans == 1)
				{
					if (Scenario.EndsWith(TEXT(".Blue")) || Scenario.EndsWith(TEXT(".Green")))
					{
						const int32 Slot = Scenario.EndsWith(TEXT(".Green")) ? 2 : 1;
						Host->RequestSwitchKnight(Slot);
						Test.TestTrue(TEXT("Solo ownership handoff lets the Red kit execute under the real companion AI"),
							Host->GetControlledKnight()->GetKnightEntityId() == Mode->GetPartyState()->GetRoster()[Slot].KnightId);
					}
					Host->RequestAbilityInput(KnightGameplayTags::Input_Combat_Block, true);
					Test.TestTrue(TEXT("Real GAS block is held before synthetic device-disconnect event"), Host->GetControlledKnight()->GetCombatComponent()->GetSnapshot().bBlocking);
					const FPlatformUserId User = Host->GetLocalPlayer()->GetPlatformUserId();
					IPlatformInputDeviceMapper::Get().GetOnInputDeviceConnectionChange().Broadcast(
						EInputDeviceConnectionState::Disconnected, User, IPlatformInputDeviceMapper::Get().GetPrimaryInputDeviceForUser(User));
					Test.TestFalse(TEXT("Device disconnect releases authoritative held block"), Host->GetControlledKnight()->GetCombatComponent()->GetSnapshot().bBlocking);
					Test.TestTrue(TEXT("Device disconnect opens recoverable campaign menu"), Host->GetRPGPanel() != nullptr && Host->GetRPGPanel()->IsActivated());
					Host->RequestCampaignAction(TEXT("Resume"));
					Test.TestFalse(TEXT("Keyboard resume after device event releases solo pause"), Mode->GetWorld()->IsPaused());
				}
				Host->RequestInteract();
				CombatStarted = Mode->GetWorld()->GetTimeSeconds();
				Phase = 4;
				return false;
			}
			if (Phase == 4)
			{
				const FKnightPartyArenaSnapshot& Arena = Mode->GetArenaState()->GetSnapshot();
				SampleFrame();
				if (!bCheckedCombatDifficulty && Arena.Stage == EKnightArenaStage::Wave)
				{
					bCheckedCombatDifficulty = true;
					Test.TestFalse(TEXT("Difficulty cannot discard an active encounter at an unsafe boundary"),
						Mode->TrySetDifficulty(Host.Get(), Difficulty == TEXT("Story") ? FName(TEXT("Standard")) : FName(TEXT("Story"))).IsSuccess());
					Test.TestEqual(TEXT("Rejected mid-combat change preserves authored difficulty"), Mode->GetDifficulty(), Difficulty);
				}
				bSawBoss |= Arena.Stage == EKnightArenaStage::Boss;
				if (Arena.Stage == EKnightArenaStage::Boss && Scenario.EndsWith(TEXT(".JIP")) && !bJoinStarted)
				{
					bJoinStarted = true;
					for (const FKnightRosterEntry& Entry : Mode->GetPartyState()->GetRoster())
					{
						JoinIds.Add(Entry.KnightId);
					}
					for (const TWeakObjectPtr<AKnightCampaignPlayerController>& Player : Players)
					{
						Player->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::W, IE_Released, 0.0f));
						if (Player != Host)
						{
							Player->ClientTravel(TEXT("/Game/Knight/Maps/Playables/L_M00_Boot"), TRAVEL_Absolute);
						}
					}
					Phase = 8;
					return false;
				}
				if (Arena.Stage == EKnightArenaStage::Boss && !bCheckedArmor)
				{
					bCheckedArmor = true;
					UKnightBossArmorComponent* Armor = Mode->GetEncounterEnemies()[0]->FindComponentByClass<UKnightBossArmorComponent>();
					Test.TestTrue(TEXT("Quality boss owns an authoritative armor zone"), Armor != nullptr && Armor->GetArmorZone() != nullptr);
				}
				MaxBossPhase = FMath::Max(MaxBossPhase, Arena.BossPhase);
				for (const FKnightRosterEntry& Entry : Mode->GetPartyState()->GetRoster())
				{
					if (UKnightSkillRuntimeComponent* Runtime = Entry.Knight->FindComponentByClass<UKnightSkillRuntimeComponent>())
					{
						const FKnightSkillRuntimeState Skills = Runtime->GetSkillState();
						if (Skills.CooldownEnds.ContainsByPredicate([this](const double End)
						{
							return End > Mode->GetWorld()->GetTimeSeconds();
						}))
						{
							(Entry.Knight->IsPlayerControlled() ? HumanExecutedSpecs : AIExecutedSpecs).Add(Skills.Build.SpecId);
						}
					}
					const EKnightLifeState Current = Entry.Knight->GetLifeComponent()->GetLifeState();
					if (const EKnightLifeState* Previous = PreviousLife.Find(Entry.KnightId))
					{
						Downed += *Previous != EKnightLifeState::Downed && Current == EKnightLifeState::Downed ? 1 : 0;
						Revived += *Previous == EKnightLifeState::Downed && Current == EKnightLifeState::Alive ? 1 : 0;
					}
					PreviousLife.Add(Entry.KnightId, Current);
				}
				if (Arena.Stage == EKnightArenaStage::Victory || Arena.Stage == EKnightArenaStage::Defeat)
				{
					Record(Arena.Stage == EKnightArenaStage::Victory ? TEXT("VICTORY") : TEXT("DEFEAT"));
					Test.TestEqual(TEXT("Representative composition has a viable route"), Arena.Stage, EKnightArenaStage::Victory);
					Test.TestTrue(TEXT("Actual wave advances into boss"), bSawBoss);
					Test.TestTrue(TEXT("Actual second boss phase reached"), MaxBossPhase >= 1);
					Test.TestTrue(TEXT("Retry after the measured victory restores downed canonical members through the real life owner"),
						Mode->TryRestartDrill(Host.Get()).IsSuccess());
					// Lifecycle fixture setup follows the measured fight; it contributes no viability or TTK evidence.
					for (TActorIterator<AKnightFrozenSurface> It(Mode->GetWorld()); It; ++It)
					{
						It->FreezeSurface();
						Test.TestTrue(TEXT("Retry fixture has a real active frozen surface"), It->IsFrozen());
					}
					AKnightRunestone* Stone = nullptr;
					for (TActorIterator<AKnightRunestone> It(Mode->GetWorld()); It; ++It)
					{
						Stone = *It;
					}
					if (Test.TestNotNull(TEXT("Retry fixture contains an authored Runestone"), Stone))
					{
						FKnightResult Channel;
						for (const FVector Offset : {FVector(0.0, -140.0, 70.0), FVector(140.0, 0.0, 70.0),
							FVector(0.0, 140.0, 70.0), FVector(-140.0, 0.0, 70.0)})
						{
							Host->GetControlledKnight()->SetActorLocation(Stone->GetActorLocation() + Offset);
							Channel = Stone->TryActivate(Host->GetControlledKnight(), FGuid::NewGuid());
							if (Channel.IsSuccess())
							{
								break;
							}
						}
						Test.TestTrue(TEXT("Retry fixture starts a real reserved channel from an unobstructed side"), Channel.IsSuccess());
						Test.TestTrue(TEXT("Host Retry succeeds through the original arena owner"), Mode->TryRestartDrill(Host.Get()).IsSuccess());
						Test.TestEqual(TEXT("Retry immediately cancels Runestone channels"), Stone->GetState(), EKnightRunestoneState::Ready);
					}
					for (TActorIterator<AKnightFrozenSurface> It(Mode->GetWorld()); It; ++It)
					{
						Test.TestFalse(TEXT("Retry immediately retires frozen collision and timers"), It->IsFrozen());
					}
					Test.TestEqual(TEXT("Retry removes the previous boss and armor actors"), Mode->GetEncounterEnemies().Num(), 0);
					GEditor->EndPlayMap();
					Phase = 7;
					return false;
				}
				if (Mode->GetWorld()->GetTimeSeconds() >= NextInput)
				{
					NextInput = Mode->GetWorld()->GetTimeSeconds() + 0.18;
					for (const TWeakObjectPtr<AKnightCampaignPlayerController>& Player : Players)
					{
						DrivePlayer(Player.Get());
					}
				}
			}
			return false;
		}

	private:
		FString EvidenceDirectory() const
		{
			FString Run;
			FParse::Value(FCommandLine::Get(), TEXT("KnightQualityEvidence="), Run);
			if (Run.IsEmpty() || Run.Contains(TEXT("..")) || Run.Contains(TEXT("/")) || Run.Contains(TEXT("\\")))
			{
				Run = TEXT("latest");
			}
			return FPaths::ProjectDir() / TEXT("Saved/Evidence/M75/encounters") / Run;
		}
		void SampleFrame()
		{
			if (!FParse::Param(FCommandLine::Get(), TEXT("KnightQualityCapture")))
			{
				return;
			}
			AKnightCharacter* Knight = Host->GetControlledKnight();
			const FKnightCombatSnapshot& Combat = Knight->GetCombatComponent()->GetSnapshot();
			FVector Camera;
			FRotator Rotation;
			Host->GetPlayerViewPoint(Camera, Rotation);
			Frames += FString::Printf(TEXT("%.6f,%llu,%d,%d,%d,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f\n"),
				Mode->GetWorld()->GetTimeSeconds(), GFrameCounter, Combat.AttackPhase, Combat.ComboIndex,
				IsValid(Combat.LockedTarget) ? 1 : 0, Camera.X, Camera.Y, Camera.Z, Rotation.Pitch, Rotation.Yaw, Rotation.Roll);
			if (Combat.AttackPhase == 0 || CapturedPhases.Contains(Combat.AttackPhase))
			{
				return;
			}
			ULocalPlayer* Local = Host->GetLocalPlayer();
			UGameViewportClient* Viewport = Local != nullptr ? Local->ViewportClient.Get() : nullptr;
			const TSharedPtr<SViewport> Widget = Viewport != nullptr ? Viewport->GetGameViewportWidget() : nullptr;
			if (!Widget.IsValid())
			{
				return;
			}
			TArray<FColor> Pixels;
			FIntVector Size;
			if (FSlateApplication::Get().TakeScreenshot(Widget.ToSharedRef(), Pixels, Size))
			{
				for (FColor& Pixel : Pixels)
				{
					Pixel.A = 255;
				}
				TArray64<uint8> PNG;
				FImageUtils::PNGCompressImageArray(Size.X, Size.Y, TArrayView64<const FColor>(Pixels.GetData(), Pixels.Num()), PNG);
				IFileManager::Get().MakeDirectory(*EvidenceDirectory(), true);
				FFileHelper::SaveArrayToFile(PNG, *(EvidenceDirectory() / FString::Printf(TEXT("%s_Phase%d.png"), *Scenario, Combat.AttackPhase)));
				CapturedPhases.Add(Combat.AttackPhase);
			}
		}
		void RefreshSources()
		{
			Players.Reset();
			for (const FWorldContext& Context : GEngine->GetWorldContexts())
			{
				UWorld* World = Context.World();
				if (Context.WorldType != EWorldType::PIE || World == nullptr)
				{
					continue;
				}
				AKnightCampaignPlayerController* Player = Cast<AKnightCampaignPlayerController>(World->GetFirstPlayerController());
				if (Player != nullptr && Player->IsLocalController() && Player->GetControlledKnight() != nullptr)
				{
					Players.Add(Player);
				}
				if (AKnightEncounterQualityGameMode* Candidate = World->GetAuthGameMode<AKnightEncounterQualityGameMode>(); Candidate != nullptr)
				{
					Mode = Candidate;
					Host = Player;
				}
			}
		}
		void DrivePlayer(AKnightCampaignPlayerController* Player)
		{
			AKnightCharacter* Knight = Player->GetControlledKnight();
			if (Knight == nullptr || !Knight->GetLifeComponent()->IsAlive())
			{
				return;
			}
			AActor* Target = nullptr;
			double BestDistance = TNumericLimits<double>::Max();
			bool bRescue = false;
			if (AKnightStoryState* State = Player->GetWorld()->GetGameState<AKnightStoryState>();
				State != nullptr && State->GetPartyState() != nullptr)
			{
				for (const FKnightRosterEntry& Entry : State->GetPartyState()->GetRoster())
				{
					if (IsValid(Entry.Knight) && Entry.Knight != Knight && Entry.Knight->GetLifeComponent()->IsDowned())
					{
						const double Distance = FVector::DistSquared2D(Knight->GetActorLocation(), Entry.Knight->GetActorLocation());
						if (Distance < BestDistance)
						{
							BestDistance = Distance;
							Target = Entry.Knight;
							bRescue = true;
						}
					}
				}
			}
			if (!bRescue)
			{
				Player->RequestAbilityInput(KnightGameplayTags::Input_Party_Revive, false);
				for (TActorIterator<AKnightArenaEnemy> It(Player->GetWorld()); It; ++It)
				{
					const double Distance = FVector::DistSquared2D(Knight->GetActorLocation(), It->GetActorLocation());
					if (It->GetLifeComponent()->IsAlive() && Distance < BestDistance)
					{
						BestDistance = Distance;
						Target = *It;
					}
				}
			}
			if (Target == nullptr)
			{
				return;
			}
			FVector Heading = Target->GetActorLocation() - Knight->GetActorLocation();
			const double StopDistance = bRescue ? Target->FindComponentByClass<UKnightLifeComponent>()->GetReviveRange() * 0.7 : 170.0;
			const bool bWalk = BestDistance > FMath::Square(StopDistance);
			if (bWalk)
			{
				FScopedConditionalWorldSwitcher Switch(Player->GetWorld());
				UNavigationPath* Path = UNavigationSystemV1::FindPathToLocationSynchronously(Player->GetWorld(),
					Knight->GetActorLocation(), Target->GetActorLocation(), Knight);
				if (Path != nullptr && Path->IsValid() && !Path->IsPartial() && Path->PathPoints.Num() > 1)
				{
					Heading = Path->PathPoints[1] - Knight->GetActorLocation();
				}
			}
			Player->SetControlRotation(Heading.GetSafeNormal2D().Rotation());
			Player->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::W, bWalk ? IE_Pressed : IE_Released, bWalk ? 1.0f : 0.0f));
			if (bRescue)
			{
				Player->RequestAbilityInput(KnightGameplayTags::Input_Party_Revive, !bWalk);
				return;
			}
			if (Knight->GetCombatComponent()->GetLockedTarget() == nullptr && BestDistance < FMath::Square(1000.0))
			{
				Player->RequestAbilityInput(KnightGameplayTags::Input_Combat_LockOn, true);
				Player->RequestAbilityInput(KnightGameplayTags::Input_Combat_LockOn, false);
			}
			for (int32 Slot = 4; Slot >= 1; --Slot)
			{
				UKnightSkillRuntimeComponent* Runtime = Knight->FindComponentByClass<UKnightSkillRuntimeComponent>();
				const FKnightSpecActionDefinition Definition = FKnightSkillCatalog::GetSkill(Runtime->GetBuild(), Slot);
				AActor* SkillTarget = Definition.bFriendly ? Runtime->SelectLowestHealthAlly(Definition.Range) : Target;
				if (Runtime->ValidateSkill(Slot, SkillTarget).IsSuccess())
				{
					Player->RequestCampaignSkill(Slot);
					break;
				}
			}
			if (!bWalk)
			{
				const FGameplayTag Action = CastChecked<AKnightArenaEnemy>(Target)->GetBossPhaseComponent()->GetDefinitionId().IsValid()
					? FGameplayTag(KnightGameplayTags::Input_Combat_Heavy) : FGameplayTag(KnightGameplayTags::Input_Combat_Primary);
				Player->RequestAbilityInput(Action, true);
				Player->RequestAbilityInput(Action, false);
			}
		}
		void Record(const TCHAR* Result)
		{
			const double TTK = Mode.IsValid() && CombatStarted > 0.0 ? Mode->GetWorld()->GetTimeSeconds() - CombatStarted : 0.0;
			const FString Data = FString::Printf(TEXT("scenario,result,ttk_seconds,downed,revived,potions,canonical,boss_phase\n%s,%s,%.3f,%d,%d,0,3,%d\n"),
				*Scenario, Result, TTK, Downed, Revived, MaxBossPhase);
			const FString Directory = EvidenceDirectory();
			IFileManager::Get().MakeDirectory(*Directory, true);
			FFileHelper::SaveStringToFile(Data, *(Directory / (Scenario + TEXT(".csv"))));
			FString KitData = TEXT("spec,controller,observed_committed_cooldown\n");
			for (const FName Spec : HumanExecutedSpecs)
			{
				KitData += Spec.ToString() + TEXT(",HumanPCBot,1\n");
			}
			for (const FName Spec : AIExecutedSpecs)
			{
				KitData += Spec.ToString() + TEXT(",StateTreeAI,1\n");
			}
			FFileHelper::SaveStringToFile(KitData, *(Directory / (Scenario + TEXT("_kits.csv"))));
			if (!Frames.IsEmpty())
			{
				FFileHelper::SaveStringToFile(TEXT("server_time,frame,attack_phase,combo,locked,camera_x,camera_y,camera_z,pitch,yaw,roll\n") + Frames,
					*(Directory / (Scenario + TEXT("_frames.csv"))));
			}
			Test.AddInfo(Data);
		}
		FAutomationTestBase& Test;
		FString Scenario;
		FName Difficulty;
		FName Composition;
		int32 Humans = 1;
		int32 Phase = 0;
		int32 BuildIndex = 0;
		int32 Downed = 0;
		int32 Revived = 0;
		int32 MaxBossPhase = INDEX_NONE;
		bool bSawBoss = false;
		bool bCheckedCombatDifficulty = false;
		bool bCheckedArmor = false;
		bool bJoinStarted = false;
		TArray<FKnightEntityId> JoinIds;
		FString Frames;
		TSet<uint8> CapturedPhases;
		TSet<FName> HumanExecutedSpecs;
		TSet<FName> AIExecutedSpecs;
		double Started;
		double CombatStarted = 0.0;
		double NextInput = 0.0;
		FGuid BuildRequest;
		TMap<FKnightEntityId, EKnightLifeState> PreviousLife;
		TStrongObjectPtr<ULevelEditorPlaySettings> Settings;
		TWeakObjectPtr<AKnightEncounterQualityGameMode> Mode;
		TWeakObjectPtr<AKnightCampaignPlayerController> Host;
		TWeakObjectPtr<AKnightCampaignPlayerController> BuildPlayer;
		TArray<TWeakObjectPtr<AKnightCampaignPlayerController>> Players;
	};
}

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FKnightEncounterQualityTest, "Knight.M75.Encounter",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::NonNullRHI)

void FKnightEncounterQualityTest::GetTests(TArray<FString>& OutNames, TArray<FString>& OutCommands) const
{
	for (const TCHAR* Scenario : {TEXT("Story.DPS.1"), TEXT("Story.Mixed.1"), TEXT("Story.Healers.1"),
		TEXT("Standard.DPS.1"), TEXT("Standard.Mixed.1"), TEXT("Standard.Healers.1"), TEXT("Standard.Mixed.2"),
		TEXT("Standard.Mixed.3"), TEXT("Standard.DPS.3"), TEXT("Standard.Healers.3"), TEXT("Standard.Tanks.3"),
		TEXT("Standard.DPS.1.Blue"), TEXT("Standard.Healers.1.Blue"), TEXT("Standard.Tanks.1.Blue"), TEXT("Standard.Mixed.2.JIP"),
		TEXT("Standard.DPS.1.Green"), TEXT("Standard.Healers.1.Green"), TEXT("Standard.Tanks.1.Green")})
	{
		OutNames.Add(FString(Scenario).Replace(TEXT(".1.Blue"), TEXT(".1_BlueHost"))
			.Replace(TEXT(".1.Green"), TEXT(".1_GreenHost")).Replace(TEXT(".2.JIP"), TEXT(".2_Join")));
		OutCommands.Add(Scenario);
	}
}

bool FKnightEncounterQualityTest::RunTest(const FString& Parameters)
{
	FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<KnightEncounterQualityTests::FRun>(*this, Parameters));
	return true;
}

#endif
