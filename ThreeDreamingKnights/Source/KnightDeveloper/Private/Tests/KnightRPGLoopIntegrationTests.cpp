#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Abilities/KnightHeavyAttackAbility.h"
#include "Abilities/KnightLightAttackAbility.h"
#include "Animation/AnimInstance.h"
#include "Attributes/KnightAttributeSet.h"
#include "Character/KnightCharacter.h"
#include "Components/KnightAbilitySystemComponent.h"
#include "Components/KnightCombatComponent.h"
#include "Components/KnightLifeComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Editor.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "FileHelpers.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/SlateUser.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayAbilitySpec.h"
#include "GameplayEffectTypes.h"
#include "HAL/PlatformTime.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "InputKeyEventArgs.h"
#include "Inventory/KnightInventoryComponent.h"
#include "Loot/KnightItemPickup.h"
#include "Misc/PackageName.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "Party/KnightPartyState.h"
#include "PartyArena/KnightArenaEnemy.h"
#include "PartyArena/KnightPartyArenaStartMarker.h"
#include "PartyArena/KnightPartyArenaState.h"
#include "PlayInEditorDataTypes.h"
#include "Progression/KnightRPGComponent.h"
#include "RPGLoop/KnightRPGCharacter.h"
#include "RPGLoop/KnightRPGLoopGameMode.h"
#include "RPGLoop/KnightRPGStateActor.h"
#include "RPGLoop/KnightRPGUIPlayerController.h"
#include "RPGLoop/KnightRPGUpgradeStation.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Tags/KnightGameplayTags.h"
#include "Tests/AutomationEditorCommon.h"
#include "UnrealEngine.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectGlobals.h"
#include "Widgets/KnightRPGPanel.h"

namespace KnightRPGPlaythrough
{
	const TCHAR* MapPath = TEXT("/Game/Knight/Maps/Playables/L_M05_RPGLoop");
	constexpr double TotalTimeout = 450.0;
	constexpr double EncounterTimeout = 120.0;
	constexpr double TravelTimeout = 45.0;
	constexpr double InputPulseDuration = 0.08;
	constexpr float MeleeReach = 130.0f;
	constexpr float InteractionReach = 135.0f;

	enum class EKnightRPGPlayPhase : uint8
	{
		Navigation, Start, Ready, WalkStartA, StartA, FightA, WalkLoot, Pickup, WalkStation,
		OpenPanel, EquipWeapon, EquipRune, PurchaseHammer, PurchaseRune, Respec, RebuyHammer,
		Continue, ClosePanel, WalkStartB, StartB, FightB, CompleteRetry, WalkWipeStart,
		WipeStart, Wipe, DefeatRetry, Cleanup, Complete
	};

	struct FKnightRPGPlayObservation
	{
		int32 Replies = 0;
		FKnightRPGCommandResult LastReply;
		int32 LightHitsA = 0;
		int32 LightHitsB = 0;
		int32 RuneHitsB = 0;
		float MaximumRuneSpendB = 0.0f;
		bool bObservedHumanMontage = false;
	};

	TArray<FGameplayAbilitySpecHandle> GetSpecs(const UKnightAbilitySystemComponent* ASC)
	{
		TArray<FGameplayAbilitySpecHandle> Result;
		for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
		{
			Result.Add(Spec.Handle);
		}
		return Result;
	}

	class FKnightRPGPlaythroughCommand final : public IAutomationLatentCommand
	{
	public:
		explicit FKnightRPGPlaythroughCommand(FAutomationTestBase& InTest)
			: Test(InTest), Observation(MakeShared<FKnightRPGPlayObservation>()),
			RunStarted(FPlatformTime::Seconds()), PhaseStarted(RunStarted)
		{
		}

		virtual ~FKnightRPGPlaythroughCommand() override
		{
			UnbindObservers();
			ReleaseInputs();
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
			if (Phase == EKnightRPGPlayPhase::Complete)
			{
				return true;
			}
			if (Phase < EKnightRPGPlayPhase::Cleanup
				&& (FPlatformTime::Seconds() - RunStarted > TotalTimeout || Elapsed() > PhaseTimeout))
			{
				Fail(GetDiagnostics(TEXT("Bounded genuine RPG playthrough timed out")));
			}
			if (Phase < EKnightRPGPlayPhase::Cleanup && Test.HasAnyErrors())
			{
				BeginCleanup();
			}
			if (World.IsValid() && Phase > EKnightRPGPlayPhase::Ready && Phase < EKnightRPGPlayPhase::Cleanup)
			{
				if (!ValidateCanonicalParty())
				{
					Fail(TEXT("Normal RPG playthrough replaced a canonical actor, hero, ID or ASC owner/avatar."));
				}
				else
				{
					ObserveAnimation();
					if (PulseKey.IsValid() && World->GetTimeSeconds() - PulseStarted >= InputPulseDuration)
					{
						SetKey(PulseKey, false);
						PulseKey = FKey();
					}
					const FVector Position = Controller->GetControlledKnight()->GetActorLocation();
					const double Motion = FVector::Dist2D(Position, LastPosition);
					if (DownKeys.Contains(EKeys::W) && Motion < 150.0)
					{
						PhysicalDistance += Motion;
					}
					LastPosition = Position;
				}
			}
			switch (Phase)
			{
			case EKnightRPGPlayPhase::Navigation:
				if (NavigationWait.Update())
				{
					Enter(EKnightRPGPlayPhase::Start, 20.0);
				}
				break;
			case EKnightRPGPlayPhase::Start: Start(); break;
			case EKnightRPGPlayPhase::Ready: Ready(); break;
			case EKnightRPGPlayPhase::WalkStartA: WalkStart(false); break;
			case EKnightRPGPlayPhase::StartA:
				if (Arena->GetSnapshot().Stage == EKnightArenaStage::Wave)
				{
					Enter(EKnightRPGPlayPhase::FightA, EncounterTimeout);
				}
				break;
			case EKnightRPGPlayPhase::FightA: FightEncounter(false); break;
			case EKnightRPGPlayPhase::WalkLoot: WalkLoot(); break;
			case EKnightRPGPlayPhase::Pickup: WaitPickup(); break;
			case EKnightRPGPlayPhase::WalkStation: WalkStation(); break;
			case EKnightRPGPlayPhase::OpenPanel: WaitPanel(); break;
			case EKnightRPGPlayPhase::EquipWeapon: WaitEquipWeapon(); break;
			case EKnightRPGPlayPhase::EquipRune: WaitEquipRune(); break;
			case EKnightRPGPlayPhase::PurchaseHammer: WaitPurchaseHammer(); break;
			case EKnightRPGPlayPhase::PurchaseRune: WaitPurchaseRune(); break;
			case EKnightRPGPlayPhase::Respec: WaitRespec(); break;
			case EKnightRPGPlayPhase::RebuyHammer: WaitRebuyHammer(); break;
			case EKnightRPGPlayPhase::Continue: WaitContinue(); break;
			case EKnightRPGPlayPhase::ClosePanel: WaitClosedPanel(); break;
			case EKnightRPGPlayPhase::WalkStartB: WalkStart(true); break;
			case EKnightRPGPlayPhase::StartB:
				if (Arena->GetSnapshot().Stage == EKnightArenaStage::Wave)
				{
					Enter(EKnightRPGPlayPhase::FightB, EncounterTimeout);
				}
				break;
			case EKnightRPGPlayPhase::FightB: FightEncounter(true); break;
			case EKnightRPGPlayPhase::CompleteRetry: WaitCompletionRetry(); break;
			case EKnightRPGPlayPhase::WalkWipeStart: WalkWipeStart(); break;
			case EKnightRPGPlayPhase::WipeStart: BeginWipe(); break;
			case EKnightRPGPlayPhase::Wipe: WaitWipe(); break;
			case EKnightRPGPlayPhase::DefeatRetry: WaitDefeatRetry(); break;
			case EKnightRPGPlayPhase::Cleanup: Cleanup(); break;
			default: break;
			}
			return Phase == EKnightRPGPlayPhase::Complete;
		}

	private:
		void Enter(EKnightRPGPlayPhase Next, double Timeout = 10.0)
		{
			Phase = Next;
			PhaseStarted = FPlatformTime::Seconds();
			PhaseTimeout = Timeout;
			WalkPath.Reset();
			NextPathQueryAt = -1.0;
			Test.AddInfo(FString::Printf(TEXT("Actual RPG playthrough phase %d started."), static_cast<int32>(Next)));
		}

		double Elapsed() const
		{
			return FPlatformTime::Seconds() - PhaseStarted;
		}

		void Start()
		{
			if (GEditor == nullptr || GEditor->IsPlaySessionInProgress())
			{
				Fail(TEXT("RPG playthrough requires an idle Editor."));
				return;
			}
			const ULevelEditorPlaySettings* Defaults = GetDefault<ULevelEditorPlaySettings>();
			OriginalPositions = Defaults->MultipleInstancePositions;
			OriginalLastSize = Defaults->LastSize;
			OriginalWindowPosition = Defaults->NewWindowPosition;
			OriginalWidth = Defaults->NewWindowWidth;
			OriginalHeight = Defaults->NewWindowHeight;
			Settings.Reset(DuplicateObject<ULevelEditorPlaySettings>(Defaults, GetTransientPackage()));
			Settings->SetPlayNetMode(PIE_Standalone);
			Settings->SetPlayNumberOfClients(1);
			Settings->SetRunUnderOneProcess(true);
			Settings->bLaunchSeparateServer = false;
			Settings->EnableGameSound = false;
			Settings->GameGetsMouseControl = true;
			Settings->SetClientWindowSize(FIntPoint(1280, 960));
			FRequestPlaySessionParams Request;
			Request.EditorPlaySettings = Settings.Get();
			Request.GlobalMapOverride = MapPath;
			Request.bAllowOnlineSubsystem = false;
			GEditor->RequestPlaySession(Request);
			bOwnsSession = true;
			Enter(EKnightRPGPlayPhase::Ready, 45.0);
		}

		void Ready()
		{
			UWorld* PIE = GEditor != nullptr ? GEditor->PlayWorld.Get() : nullptr;
			if (PIE == nullptr || !PIE->HasBegunPlay())
			{
				return;
			}
			World = PIE;
			Mode = PIE->GetAuthGameMode<AKnightRPGLoopGameMode>();
			Controller = Cast<AKnightRPGUIPlayerController>(PIE->GetFirstPlayerController());
			Party = Mode.IsValid() ? Mode->GetPartyState() : nullptr;
			Arena = Mode.IsValid() ? Mode->GetArenaState() : nullptr;
			RPGState = Mode.IsValid() ? Mode->GetRPGState() : nullptr;
			if (!Mode.IsValid() || !Controller.IsValid() || !Party.IsValid() || !Arena.IsValid()
				|| !RPGState.IsValid() || !Party->IsCanonicalRosterReady() || !RPGState->GetRPG()->IsInitialized()
				|| Controller->GetControlledKnight() == nullptr)
			{
				return;
			}
			if (!Mode->GetRPGInitializationResult().IsSuccess())
			{
				Fail(Mode->GetRPGInitializationResult().Message.ToString());
				return;
			}
			if (ReadyWorldTime < 0.0)
			{
				ReadyWorldTime = World->GetTimeSeconds();
			}
			if (World->GetTimeSeconds() - ReadyWorldTime < 0.5)
			{
				return;
			}
			ULocalPlayer* Local = Controller->GetLocalPlayer();
			const TSharedPtr<FSlateUser> SlateUser = Local != nullptr ? Local->GetSlateUser() : nullptr;
			if (!FSlateApplication::IsInitialized() || !SlateUser.IsValid())
			{
				Fail(TEXT("Genuine CommonUI gate requires an actual registered local Slate user."));
				return;
			}
			SlateUserIndex = SlateUser->GetUserIndex();
			for (int32 Slot = 0; Slot < 3; ++Slot)
			{
				const FKnightRosterEntry* Entry = Party->FindSlot(Slot);
				AKnightRPGCharacter* Knight = Entry != nullptr ? Cast<AKnightRPGCharacter>(Entry->Knight) : nullptr;
				if (Knight == nullptr || Knight->GetCombatComponent()->GetProfile() == nullptr
					|| Knight->GetWeaponUpgradeVisual() == nullptr || Knight->GetRuneVisual() == nullptr)
				{
					Fail(TEXT("Actual M5 canonical Knights lack their combat profile or native equipment presentation."));
					return;
				}
				Knights.Add(Knight);
				Ids.Add(Knight->GetKnightEntityId());
				Heroes.Add(Knight->GetHeroDefinitionId());
			}
			ControlledSlot = Party->GetControlledSlot(Controller.Get());
			if (!Knights.IsValidIndex(ControlledSlot))
			{
				Fail(TEXT("The actual solo controller has no valid canonical slot."));
				return;
			}
			ControlledId = Controller->GetControlledKnight()->GetKnightEntityId();
			BaseAttackPower = Controller->GetControlledKnight()->GetKnightAttributeSet()->GetAttackPower();
			BaseRuneCost = Controller->GetControlledKnight()->GetCombatComponent()->GetProfile()->GetTuning().RuneEnergyCost;
			for (TActorIterator<AKnightPartyArenaStartMarker> Iterator(World.Get()); Iterator; ++Iterator)
			{
				StartMarker = *Iterator;
				break;
			}
			if (!StartMarker.IsValid() || Mode->GetUpgradeStation() == nullptr)
			{
				Fail(TEXT("Actual RPG start marker/upgrade station is missing."));
				return;
			}
			BindObservers();
			LastPosition = Controller->GetControlledKnight()->GetActorLocation();
			Test.AddInfo(TEXT("Normal encounter A/B victory uses physical mapped W movement and actual LMB/G GAS attacks, companion StateTrees, F start, P pickups and focused Slate/CommonUI 1/2/3/4/5/Enter/Escape. No teleport or damage injection creates either victory."));
			Enter(EKnightRPGPlayPhase::WalkStartA, TravelTimeout);
		}

		void BindObservers()
		{
			const TWeakPtr<FKnightRPGPlayObservation> WeakObservation(Observation);
			const TWeakObjectPtr<AKnightRPGLoopGameMode> WeakMode(Mode);
			ReplyHandle = Controller->OnRPGCommandResult.AddLambda(
				[WeakObservation](const FKnightRPGCommandResult& Reply)
				{
					if (TSharedPtr<FKnightRPGPlayObservation> Current = WeakObservation.Pin())
					{
						++Current->Replies;
						Current->LastReply = Reply;
					}
				});
			ObservedCombat = Controller->GetControlledKnight()->GetCombatComponent();
			CombatHandle = ObservedCombat->OnCombatOutcome.AddLambda(
				[WeakObservation, WeakMode](const FKnightCombatOutcome& Outcome)
				{
					TSharedPtr<FKnightRPGPlayObservation> Current = WeakObservation.Pin();
					if (!Current.IsValid() || !WeakMode.IsValid() || WeakMode->GetRPGState() == nullptr)
					{
						return;
					}
					const EKnightRPGLoopStage Stage = WeakMode->GetRPGState()->GetLoopSnapshot().Stage;
					if (Outcome.Outcome == EKnightCombatOutcome::LightHit && Outcome.Amount > 0.0f)
					{
						Current->LightHitsA += Stage == EKnightRPGLoopStage::EncounterA
							|| Stage == EKnightRPGLoopStage::RewardAvailable ? 1 : 0;
						Current->LightHitsB += Stage == EKnightRPGLoopStage::EncounterB
							|| Stage == EKnightRPGLoopStage::Complete ? 1 : 0;
					}
					if (Outcome.Outcome == EKnightCombatOutcome::RuneHit && Outcome.Amount > 0.0f
						&& (Stage == EKnightRPGLoopStage::EncounterB || Stage == EKnightRPGLoopStage::Complete))
					{
						++Current->RuneHitsB;
					}
				});
			ObservedASC = Controller->GetControlledKnight()->GetKnightAbilitySystem();
			EnergyHandle = ObservedASC->GetGameplayAttributeValueChangeDelegate(
				UKnightAttributeSet::GetRuneEnergyAttribute()).AddLambda(
					[WeakObservation, WeakMode](const FOnAttributeChangeData& Change)
					{
						TSharedPtr<FKnightRPGPlayObservation> Current = WeakObservation.Pin();
						if (Current.IsValid() && WeakMode.IsValid() && WeakMode->GetRPGState() != nullptr
							&& WeakMode->GetRPGState()->GetLoopSnapshot().Stage == EKnightRPGLoopStage::EncounterB)
						{
							Current->MaximumRuneSpendB = FMath::Max(Current->MaximumRuneSpendB,
								Change.OldValue - Change.NewValue);
						}
					});
		}

		bool ValidateCanonicalParty() const
		{
			if (!Party.IsValid() || Knights.Num() != 3 || Controller->GetControlledKnight() != Knights[ControlledSlot].Get())
			{
				return false;
			}
			int32 Count = 0;
			for (TActorIterator<AKnightCharacter> Iterator(World.Get()); Iterator; ++Iterator)
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

		void SetKey(const FKey& Key, bool bPressed)
		{
			if (!Controller.IsValid())
			{
				return;
			}
			Controller->InputKey(FInputKeyEventArgs::CreateSimulated(Key,
				bPressed ? IE_Pressed : IE_Released, bPressed ? 1.0f : 0.0f));
			if (bPressed)
			{
				DownKeys.Add(Key);
			}
			else
			{
				DownKeys.Remove(Key);
			}
		}

		void Pulse(const FKey& Key)
		{
			if (PulseKey.IsValid())
			{
				return;
			}
			PulseKey = Key;
			PulseStarted = World->GetTimeSeconds();
			SetKey(Key, true);
		}

		void ReleaseInputs()
		{
			const TArray<FKey> Keys = DownKeys.Array();
			for (const FKey& Key : Keys)
			{
				SetKey(Key, false);
			}
			PulseKey = FKey();
		}

		bool WalkToward(const FVector& Goal, float Reach)
		{
			AKnightCharacter* Knight = Controller->GetControlledKnight();
			if (FVector::DistSquared2D(Knight->GetActorLocation(), Goal) <= FMath::Square(Reach))
			{
				SetKey(EKeys::W, false);
				return true;
			}
			if (!Knight->GetLifeComponent()->IsAlive())
			{
				SetKey(EKeys::W, false);
				return false;
			}
			const double Now = World->GetTimeSeconds();
			if (Now >= NextPathQueryAt || WalkPath.IsEmpty() || FVector::DistSquared2D(PathGoal, Goal) > FMath::Square(100.0f))
			{
				NextPathQueryAt = Now + 0.4;
				PathGoal = Goal;
				FScopedConditionalWorldSwitcher Switch(World.Get());
				UNavigationPath* Path = UNavigationSystemV1::FindPathToLocationSynchronously(
					World.Get(), Knight->GetActorLocation(), Goal, Knight);
				WalkPath = Path != nullptr && Path->IsValid() && !Path->IsPartial() ? Path->PathPoints : TArray<FVector>();
				NextWaypoint = WalkPath.Num() > 1 ? 1 : 0;
			}
			if (WalkPath.IsEmpty())
			{
				SetKey(EKeys::W, false);
				return false;
			}
			while (NextWaypoint < WalkPath.Num() - 1
				&& FVector::DistSquared2D(Knight->GetActorLocation(), WalkPath[NextWaypoint]) < FMath::Square(70.0f))
			{
				++NextWaypoint;
			}
			FVector Direction = WalkPath[NextWaypoint] - Knight->GetActorLocation();
			Direction.Z = 0.0;
			if (Direction.IsNearlyZero())
			{
				SetKey(EKeys::W, false);
				return false;
			}
			// Aim the third-person control frame; CharacterMovement and the real mapped action perform all translation.
			Controller->SetControlRotation(Direction.Rotation());
			if (!DownKeys.Contains(EKeys::W))
			{
				SetKey(EKeys::W, true);
			}
			return false;
		}

		void WalkStart(bool bSecond)
		{
			if (WalkToward(StartMarker->GetActorLocation(), InteractionReach) && !PulseKey.IsValid())
			{
				Pulse(EKeys::F);
				Enter(bSecond ? EKnightRPGPlayPhase::StartB : EKnightRPGPlayPhase::StartA);
			}
		}

		AKnightArenaEnemy* FindNearestEnemy() const
		{
			AKnightArenaEnemy* Result = nullptr;
			double Best = TNumericLimits<double>::Max();
			for (AKnightArenaEnemy* Enemy : Mode->GetEncounterEnemies())
			{
				if (IsValid(Enemy) && Enemy->GetLifeComponent()->IsAlive())
				{
					const double Distance = FVector::DistSquared2D(Controller->GetControlledKnight()->GetActorLocation(),
						Enemy->GetActorLocation());
					if (Distance < Best)
					{
						Result = Enemy;
						Best = Distance;
					}
				}
			}
			return Result;
		}

		void ObserveAnimation()
		{
			AKnightCharacter* Knight = Controller->GetControlledKnight();
			UKnightAbilitySystemComponent* ASC = Knight->GetKnightAbilitySystem();
			const FGameplayAbilitySpec* Light = ASC->FindAbilitySpecFromClass(UKnightLightAttackAbility::StaticClass());
			const FGameplayAbilitySpec* Heavy = ASC->FindAbilitySpecFromClass(UKnightHeavyAttackAbility::StaticClass());
			UAnimInstance* Animation = Knight->GetMesh()->GetAnimInstance();
			Observation->bObservedHumanMontage |= ((Light != nullptr && Light->IsActive()) || (Heavy != nullptr && Heavy->IsActive()))
				&& ASC->GetCurrentMontage() != nullptr && Animation != nullptr
				&& Animation->Montage_IsPlaying(ASC->GetCurrentMontage());
		}

		void FightEncounter(bool bSecond)
		{
			if (Arena->GetSnapshot().Stage == EKnightArenaStage::Defeat)
			{
				Fail(TEXT("The party wiped during a genuine mapped RPG victory playthrough."));
				return;
			}
			if (Arena->GetSnapshot().Stage == EKnightArenaStage::Victory)
			{
				SetKey(EKeys::W, false);
				ReleaseInputs();
				if (!Observation->bObservedHumanMontage || (bSecond ? Observation->LightHitsB : Observation->LightHitsA) < 1)
				{
					Fail(TEXT("Actual encounter victory had no confirmed human trace damage/real montage."));
					return;
				}
				if (bSecond)
				{
					if (RPGState->GetLoopSnapshot().Stage != EKnightRPGLoopStage::Complete)
					{
						return;
					}
					if (Observation->RuneHitsB < 1 || !FMath::IsNearlyEqual(Observation->MaximumRuneSpendB, ExpectedRuneCost, 0.1f)
						|| ExpectedRuneCost >= BaseRuneCost || !HasExpectedVisuals(true, true))
					{
						Fail(TEXT("Encounter B did not execute the upgraded rune with actual reduced GAS energy cost and source-owned visuals."));
						return;
					}
					if (!CapturePersistentState())
					{
						return;
					}
					Test.AddInfo(FString::Printf(TEXT("Normal A/B victory confirmed human light hits A%d B%d, rune hits%d, actual rune cost %.2f versus base %.2f; physical travel %.0fcm."),
						Observation->LightHitsA, Observation->LightHitsB, Observation->RuneHitsB,
						Observation->MaximumRuneSpendB, BaseRuneCost, PhysicalDistance));
					Pulse(EKeys::R);
					Enter(EKnightRPGPlayPhase::CompleteRetry);
				}
				else if (RPGState->GetLoopSnapshot().Stage == EKnightRPGLoopStage::RewardAvailable)
				{
					Enter(EKnightRPGPlayPhase::WalkLoot, TravelTimeout);
				}
				return;
			}
			AKnightCharacter* Knight = Controller->GetControlledKnight();
			if (!Knight->GetLifeComponent()->IsAlive())
			{
				SetKey(EKeys::W, false);
				return;
			}
			AKnightArenaEnemy* Target = FindNearestEnemy();
			if (Target == nullptr)
			{
				SetKey(EKeys::W, false);
				return;
			}
			if (!WalkToward(Target->GetActorLocation(), MeleeReach) || PulseKey.IsValid())
			{
				return;
			}
			UKnightCombatComponent* Combat = Knight->GetCombatComponent();
			const double Now = World->GetTimeSeconds();
			if (!Combat->CanTransferControl() || Now - LastAttackAt < 0.95)
			{
				return;
			}
			if (bSecond && Observation->RuneHitsB == 0 && Now - LastRuneRequestAt >= 3.0
				&& Knight->GetKnightAttributeSet()->GetRuneEnergy() >= ExpectedRuneCost)
			{
				LastRuneRequestAt = Now;
				LastAttackAt = Now;
				Pulse(EKeys::G);
				return;
			}
			LastAttackAt = Now;
			Pulse(EKeys::LeftMouseButton);
		}

		void WalkLoot()
		{
			if (!SelectedPickup.IsValid() || SelectedPickup->IsClaimed())
			{
				SelectedPickup.Reset();
				double Best = TNumericLimits<double>::Max();
				for (TActorIterator<AKnightItemPickup> Iterator(World.Get()); Iterator; ++Iterator)
				{
					if (!Iterator->IsClaimed())
					{
						const double Distance = FVector::DistSquared2D(Controller->GetControlledKnight()->GetActorLocation(),
							Iterator->GetActorLocation());
						if (Distance < Best)
						{
							SelectedPickup = *Iterator;
							Best = Distance;
						}
					}
				}
			}
			if (!SelectedPickup.IsValid())
			{
				BeforeSkillsInventory = RPGState->GetInventory()->GetView();
				if (BeforeSkillsInventory.Items.Num() < 3)
				{
					Fail(TEXT("Genuine A reward/pickup input did not produce the material/equipment/rune inventory."));
					return;
				}
				Enter(EKnightRPGPlayPhase::WalkStation, TravelTimeout);
				return;
			}
			if (WalkToward(SelectedPickup->GetActorLocation(), InteractionReach) && !PulseKey.IsValid())
			{
				RepliesBefore = Observation->Replies;
				Pulse(EKeys::P);
				Enter(EKnightRPGPlayPhase::Pickup);
			}
		}

		bool HasUIReceipt(EKnightRPGOperation Operation)
		{
			if (Observation->Replies <= RepliesBefore)
			{
				return false;
			}
			const FKnightRPGCommandResult& Reply = Observation->LastReply;
			if (!Reply.Result.IsSuccess() || Reply.Operation != Operation || Reply.KnightId != ControlledId)
			{
				Fail(FString::Printf(TEXT("Mapped/Slate RPG action failed op%d actual%d error%d Knight%s: %s"),
					static_cast<int32>(Operation), static_cast<int32>(Reply.Operation), static_cast<int32>(Reply.Result.Error),
					*Reply.KnightId.Value.ToString(), *Reply.Result.Message.ToString()));
				return false;
			}
			return true;
		}

		void WaitPickup()
		{
			if (!HasUIReceipt(EKnightRPGOperation::Pickup) || PulseKey.IsValid())
			{
				return;
			}
			if (!SelectedPickup.IsValid() || !SelectedPickup->IsClaimed())
			{
				Fail(TEXT("Mapped P receipt succeeded without claiming the actual targeted pickup."));
				return;
			}
			SelectedPickup.Reset();
			Enter(EKnightRPGPlayPhase::WalkLoot, TravelTimeout);
		}

		void WalkStation()
		{
			if (WalkToward(Mode->GetUpgradeStation()->GetActorLocation(), InteractionReach) && !PulseKey.IsValid())
			{
				if (!Mode->IsInUpgradeSafeZone(Controller->GetControlledKnight()))
				{
					return;
				}
				Pulse(EKeys::I);
				Enter(EKnightRPGPlayPhase::OpenPanel);
			}
		}

		bool HasFocusedPanel() const
		{
			UKnightRPGPanel* Panel = Controller.IsValid() ? Controller->GetRPGPanel() : nullptr;
			return Panel != nullptr && Panel->IsActivated() && Panel->GetSnapshot().KnightId == ControlledId
				&& (Panel->HasUserFocus(Controller.Get()) || Panel->HasUserFocusedDescendants(Controller.Get()))
				&& FSlateApplication::Get().GetUserFocusedWidget(SlateUserIndex).IsValid();
		}

		bool SlateKey(const FKey& Key)
		{
			if (!HasFocusedPanel())
			{
				Fail(TEXT("Actual CommonUI panel does not own local Slate focus; no direct intent bypass is allowed."));
				return false;
			}
			RepliesBefore = Observation->Replies;
			const FKeyEvent Event(Key, FModifierKeysState(), SlateUserIndex, false, 0, 0);
			FScopedConditionalWorldSwitcher Switch(World.Get());
			const bool bHandled = FSlateApplication::Get().ProcessKeyDownEvent(Event);
			FSlateApplication::Get().ProcessKeyUpEvent(Event);
			if (!bHandled)
			{
				Fail(TEXT("Real Slate key event was not handled by the focused CommonUI RPG panel."));
			}
			return bHandled;
		}

		void WaitPanel()
		{
			if (!HasFocusedPanel() || PulseKey.IsValid())
			{
				return;
			}
			if (SlateKey(EKeys::One))
			{
				Enter(EKnightRPGPlayPhase::EquipWeapon);
			}
		}

		bool HasExpectedVisuals(bool bWeapon, bool bRune) const
		{
			for (int32 Slot = 0; Slot < 3; ++Slot)
			{
				const bool bSource = Slot == ControlledSlot;
				if (Knights[Slot]->GetWeaponUpgradeVisual()->IsVisible() != (bSource && bWeapon)
					|| Knights[Slot]->GetRuneVisual()->IsVisible() != (bSource && bRune))
				{
					return false;
				}
			}
			return true;
		}

		void WaitEquipWeapon()
		{
			if (!HasUIReceipt(EKnightRPGOperation::Equip))
			{
				return;
			}
			const FKnightRPGKnightView View = RPGState->GetRPG()->GetKnightView(ControlledId);
			if (View.FindAssignment(KnightGameplayTags::Slot_Equipment_Weapon) == nullptr
				|| Controller->GetControlledKnight()->GetKnightAttributeSet()->GetAttackPower() <= BaseAttackPower
				|| !HasExpectedVisuals(true, false))
			{
				Fail(TEXT("Slate key1 did not equip the correct Knight with real GAS stat bonus/native weapon visual."));
				return;
			}
			EquippedAttackPower = Controller->GetControlledKnight()->GetKnightAttributeSet()->GetAttackPower();
			if (SlateKey(EKeys::Two))
			{
				Enter(EKnightRPGPlayPhase::EquipRune);
			}
		}

		void WaitEquipRune()
		{
			if (!HasUIReceipt(EKnightRPGOperation::Rune))
			{
				return;
			}
			const FKnightRPGKnightView View = RPGState->GetRPG()->GetKnightView(ControlledId);
			if (View.FindAssignment(KnightGameplayTags::Slot_Equipment_Rune) == nullptr || !HasExpectedVisuals(true, true))
			{
				Fail(TEXT("Slate key2 did not equip the correct Knight's real rune assignment/native visual."));
				return;
			}
			RuneBeforeSkills = View.RuneTuning;
			BeforeSkillsPoints = View.SkillPoints;
			if (SlateKey(EKeys::Three))
			{
				Enter(EKnightRPGPlayPhase::PurchaseHammer);
			}
		}

		void WaitPurchaseHammer()
		{
			if (!HasUIReceipt(EKnightRPGOperation::PurchaseSkill))
			{
				return;
			}
			const FKnightRPGKnightView View = RPGState->GetRPG()->GetKnightView(ControlledId);
			if (!View.HasSkill(FPrimaryAssetId(TEXT("KnightSkill"), TEXT("HammerForce")))
				|| View.SkillPoints >= BeforeSkillsPoints
				|| Controller->GetControlledKnight()->GetKnightAttributeSet()->GetAttackPower() <= EquippedAttackPower)
			{
				Fail(TEXT("Slate key3 did not atomically buy HammerForce and apply its actual stat bonus."));
				return;
			}
			if (SlateKey(EKeys::Four))
			{
				Enter(EKnightRPGPlayPhase::PurchaseRune);
			}
		}

		void WaitPurchaseRune()
		{
			if (!HasUIReceipt(EKnightRPGOperation::PurchaseSkill))
			{
				return;
			}
			const FKnightRPGKnightView View = RPGState->GetRPG()->GetKnightView(ControlledId);
			if (!View.HasSkill(FPrimaryAssetId(TEXT("KnightSkill"), TEXT("RuneFocus"))) || View.SkillPoints != 0
				|| View.RuneTuning.DurationScale <= RuneBeforeSkills.DurationScale)
			{
				Fail(TEXT("Slate key4 did not buy the real prerequisite-gated RuneFocus/rune duration modifier."));
				return;
			}
			if (SlateKey(EKeys::Five))
			{
				Enter(EKnightRPGPlayPhase::Respec);
			}
		}

		void WaitRespec()
		{
			if (!HasUIReceipt(EKnightRPGOperation::Respec))
			{
				return;
			}
			const FKnightRPGKnightView View = RPGState->GetRPG()->GetKnightView(ControlledId);
			if (!View.PurchasedSkills.IsEmpty() || View.SkillPoints != BeforeSkillsPoints
				|| RPGState->GetInventory()->GetView().Items != BeforeSkillsInventory.Items
				|| !(View.RuneTuning == RuneBeforeSkills)
				|| !FMath::IsNearlyEqual(Controller->GetControlledKnight()->GetKnightAttributeSet()->GetAttackPower(), EquippedAttackPower))
			{
				Fail(TEXT("Slate key5 respec did not atomically refund exact paid item identities/points and remove skill modifiers."));
				return;
			}
			if (SlateKey(EKeys::Three))
			{
				Enter(EKnightRPGPlayPhase::RebuyHammer);
			}
		}

		void WaitRebuyHammer()
		{
			if (!HasUIReceipt(EKnightRPGOperation::PurchaseSkill))
			{
				return;
			}
			const FKnightActionModifier Modifier = Controller->GetControlledKnight()->GetKnightAbilitySystem()->GetActionModifier(
				KnightGameplayTags::Ability_Combat_Rune);
			ExpectedRuneCost = BaseRuneCost * Modifier.CostScale;
			if (!RPGState->GetRPG()->GetKnightView(ControlledId).HasSkill(FPrimaryAssetId(TEXT("KnightSkill"), TEXT("HammerForce")))
				|| !HasExpectedVisuals(true, true) || !Controller->GetRPGPanel()->GetSnapshot().bCanContinue)
			{
				Fail(TEXT("Real rebuilt upgraded loadout did not enable the CommonUI host Continue action."));
				return;
			}
			if (SlateKey(EKeys::Enter))
			{
				Enter(EKnightRPGPlayPhase::Continue);
			}
		}

		void WaitContinue()
		{
			if (RPGState->GetLoopSnapshot().Stage != EKnightRPGLoopStage::EncounterB
				|| Arena->GetSnapshot().Stage != EKnightArenaStage::AwaitingStart)
			{
				return;
			}
			if (!Controller->GetLastRPGLoopResult().IsSuccess() || !HasExpectedVisuals(true, true))
			{
				Fail(TEXT("Focused Slate Enter did not perform the real host continuation while preserving loadout visuals."));
				return;
			}
			UKnightRPGPanel* Panel = Controller->GetRPGPanel();
			if (Panel != nullptr && Panel->IsActivated())
			{
				if (!SlateKey(EKeys::Escape))
				{
					return;
				}
			}
			Enter(EKnightRPGPlayPhase::ClosePanel);
		}

		void WaitClosedPanel()
		{
			UKnightRPGPanel* Panel = Controller->GetRPGPanel();
			if (Panel != nullptr && Panel->IsActivated())
			{
				return;
			}
			Enter(EKnightRPGPlayPhase::WalkStartB, TravelTimeout);
		}

		bool CapturePersistentState()
		{
			if (!RPGState->GetRPG()->CaptureSnapshot(PersistentSnapshot).IsSuccess())
			{
				Fail(TEXT("The completed genuine run could not capture its real transaction/ID snapshot."));
				return false;
			}
			PersistentSpecs.Reset();
			PersistentGrants.Reset();
			PersistentProjections.Reset();
			for (const TWeakObjectPtr<AKnightRPGCharacter>& Knight : Knights)
			{
				UKnightAbilitySystemComponent* ASC = Knight->GetKnightAbilitySystem();
				PersistentSpecs.Add(GetSpecs(ASC));
				PersistentGrants.Add(ASC->GetOwnedGrantCount());
				PersistentProjections.Add(ASC->GetSourceProjection(KnightRPG::ProjectionSource));
			}
			PersistentPickupIds.Reset();
			for (TActorIterator<AKnightItemPickup> Iterator(World.Get()); Iterator; ++Iterator)
			{
				PersistentPickupIds.Add(Iterator->GetPickupId());
			}
			PersistentPickupIds.Sort();
			return true;
		}

		bool HasPersistentState() const
		{
			FKnightRPGSnapshot Current;
			if (!RPGState->GetRPG()->CaptureSnapshot(Current).IsSuccess() || !(Current == PersistentSnapshot)
				|| !HasExpectedVisuals(true, true))
			{
				return false;
			}
			TArray<FGuid> CurrentPickupIds;
			for (TActorIterator<AKnightItemPickup> Iterator(World.Get()); Iterator; ++Iterator)
			{
				if (!Iterator->IsClaimed())
				{
					return false;
				}
				CurrentPickupIds.Add(Iterator->GetPickupId());
			}
			CurrentPickupIds.Sort();
			if (CurrentPickupIds != PersistentPickupIds || CurrentPickupIds.Num() != RPGState->GetLoopSnapshot().RewardPickupCount
				|| !RPGState->GetLoopSnapshot().bRewardIssued)
			{
				return false;
			}
			for (int32 Slot = 0; Slot < 3; ++Slot)
			{
				const UKnightAbilitySystemComponent* ASC = Knights[Slot]->GetKnightAbilitySystem();
				const FKnightSourceProjection Projection = ASC->GetSourceProjection(KnightRPG::ProjectionSource);
				const FKnightSourceProjection& Previous = PersistentProjections[Slot];
				if (GetSpecs(ASC) != PersistentSpecs[Slot] || ASC->GetOwnedGrantCount() != PersistentGrants[Slot]
					|| Projection.EntityId != Previous.EntityId || Projection.Revision != Previous.Revision
					|| !(Projection.Plan == Previous.Plan))
				{
					return false;
				}
			}
			return true;
		}

		void WaitCompletionRetry()
		{
			if (PulseKey.IsValid() || Arena->GetSnapshot().Stage != EKnightArenaStage::AwaitingStart
				|| RPGState->GetLoopSnapshot().Stage != EKnightRPGLoopStage::EncounterB)
			{
				return;
			}
			if (!HasPersistentState())
			{
				Fail(TEXT("Mapped completion retry changed reward/pickup IDs, receipts, inventory, progression or source grants."));
				return;
			}
			Enter(EKnightRPGPlayPhase::WalkWipeStart, TravelTimeout);
		}

		void WalkWipeStart()
		{
			if (WalkToward(StartMarker->GetActorLocation(), InteractionReach) && !PulseKey.IsValid())
			{
				Pulse(EKeys::F);
				Enter(EKnightRPGPlayPhase::WipeStart);
			}
		}

		void BeginWipe()
		{
			if (Arena->GetSnapshot().Stage != EKnightArenaStage::Wave || PulseKey.IsValid())
			{
				return;
			}
			AKnightArenaEnemy* Enemy = FindNearestEnemy();
			if (Enemy == nullptr)
			{
				Fail(TEXT("Retry/wipe fault requires a genuine living hostile GAS source."));
				return;
			}
			ReleaseInputs();
			Test.AddInfo(TEXT("After both genuine mapped victories, explicit server GAS damage injects a three-Knight wipe solely to verify failure/retry persistence."));
			FScopedConditionalWorldSwitcher Switch(World.Get());
			for (const TWeakObjectPtr<AKnightRPGCharacter>& Knight : Knights)
			{
				const float Amount = Knight->GetKnightAttributeSet()->GetMaxHealth()
					+ Knight->GetKnightAttributeSet()->GetDefense() + 1000.0f;
				if (!UKnightAbilitySystemComponent::ApplyDamage(Enemy, Knight.Get(), Amount).IsSuccess())
				{
					Fail(TEXT("Explicit post-victory wipe fault was rejected."));
					return;
				}
			}
			Enter(EKnightRPGPlayPhase::Wipe);
		}

		void WaitWipe()
		{
			if (Arena->GetSnapshot().Stage != EKnightArenaStage::Defeat || Elapsed() < 0.3)
			{
				return;
			}
			if (!HasPersistentState())
			{
				Fail(TEXT("Real wipe changed persistent host inventory/receipts or Knight loadout/grants."));
				return;
			}
			Pulse(EKeys::R);
			Enter(EKnightRPGPlayPhase::DefeatRetry);
		}

		void WaitDefeatRetry()
		{
			if (Arena->GetSnapshot().Stage != EKnightArenaStage::AwaitingStart || PulseKey.IsValid() || Elapsed() < 0.5)
			{
				return;
			}
			if (!HasPersistentState())
			{
				Fail(TEXT("Mapped defeat retry changed persistent inventory/rewards/loadout or duplicated grants."));
				return;
			}
			for (const TWeakObjectPtr<AKnightRPGCharacter>& Knight : Knights)
			{
				if (!Knight->GetLifeComponent()->IsAlive())
				{
					Fail(TEXT("Real RPG retry did not recover the existing canonical Knights' life state."));
					return;
				}
			}
			Test.AddInfo(TEXT("Actual completion and wipe retries preserved one reward issuance, all pickup/item IDs, exact receipts, three Knight views, GAS projections/specs/grants and equipment visuals."));
			BeginCleanup();
		}

		FString GetDiagnostics(const TCHAR* Prefix) const
		{
			AKnightCharacter* Knight = Controller.IsValid() ? Controller->GetControlledKnight() : nullptr;
			return FString::Printf(TEXT("%s phase%d elapsed%.2f arena%d loop%d pos%s path%d walk%.0fcm replies%d op%d error%d AHit%d BHit%d BRune%d RuneSpend%.2f expected%.2f panelFocus%d"),
				Prefix, static_cast<int32>(Phase), Elapsed(),
				Arena.IsValid() ? static_cast<int32>(Arena->GetSnapshot().Stage) : -1,
				RPGState.IsValid() ? static_cast<int32>(RPGState->GetLoopSnapshot().Stage) : -1,
				Knight != nullptr ? *Knight->GetActorLocation().ToString() : TEXT("none"), WalkPath.Num(), PhysicalDistance,
				Observation->Replies, static_cast<int32>(Observation->LastReply.Operation),
				static_cast<int32>(Observation->LastReply.Result.Error), Observation->LightHitsA, Observation->LightHitsB,
				Observation->RuneHitsB, Observation->MaximumRuneSpendB, ExpectedRuneCost,
				Controller.IsValid() && FSlateApplication::IsInitialized() ? HasFocusedPanel() : false);
		}

		void Fail(const FString& Message)
		{
			if (Phase >= EKnightRPGPlayPhase::Cleanup)
			{
				return;
			}
			Test.AddError(Message);
			BeginCleanup();
		}

		void UnbindObservers()
		{
			if (Controller.IsValid())
			{
				Controller->OnRPGCommandResult.Remove(ReplyHandle);
			}
			if (ObservedCombat.IsValid())
			{
				ObservedCombat->OnCombatOutcome.Remove(CombatHandle);
			}
			if (ObservedASC.IsValid())
			{
				ObservedASC->GetGameplayAttributeValueChangeDelegate(UKnightAttributeSet::GetRuneEnergyAttribute()).Remove(EnergyHandle);
			}
			ReplyHandle.Reset();
			CombatHandle.Reset();
			EnergyHandle.Reset();
		}

		void BeginCleanup()
		{
			if (Phase >= EKnightRPGPlayPhase::Cleanup)
			{
				return;
			}
			UnbindObservers();
			ReleaseInputs();
			Enter(EKnightRPGPlayPhase::Cleanup, 15.0);
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
				if (Elapsed() < 12.0)
				{
					return;
				}
				Test.AddError(TEXT("RPG playthrough PIE teardown timed out; forcing synchronous EndPlay."));
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
			Enter(EKnightRPGPlayPhase::Complete);
		}

		FAutomationTestBase& Test;
		TSharedPtr<FKnightRPGPlayObservation> Observation;
		EKnightRPGPlayPhase Phase = EKnightRPGPlayPhase::Navigation;
		double RunStarted = 0.0;
		double PhaseStarted = 0.0;
		double PhaseTimeout = 45.0;
		double ReadyWorldTime = -1.0;
		double NextPathQueryAt = -1.0;
		double LastAttackAt = -10.0;
		double LastRuneRequestAt = -10.0;
		double PulseStarted = 0.0;
		double PhysicalDistance = 0.0;
		TArray<FVector> WalkPath;
		FVector PathGoal = FVector::ZeroVector;
		FVector LastPosition = FVector::ZeroVector;
		int32 NextWaypoint = 0;
		int32 ControlledSlot = INDEX_NONE;
		uint32 SlateUserIndex = 0;
		int32 RepliesBefore = 0;
		int32 BeforeSkillsPoints = 0;
		float BaseAttackPower = 0.0f;
		float EquippedAttackPower = 0.0f;
		float BaseRuneCost = 0.0f;
		float ExpectedRuneCost = 0.0f;
		bool bOwnsSession = false;
		FKey PulseKey;
		TSet<FKey> DownKeys;
		FWaitForEditorNavMeshBuildCommand NavigationWait;
		TStrongObjectPtr<ULevelEditorPlaySettings> Settings;
		TArray<FIntPoint> OriginalPositions;
		FIntPoint OriginalLastSize;
		FIntPoint OriginalWindowPosition;
		int32 OriginalWidth = 0;
		int32 OriginalHeight = 0;
		TWeakObjectPtr<UWorld> World;
		TWeakObjectPtr<AKnightRPGLoopGameMode> Mode;
		TWeakObjectPtr<AKnightRPGUIPlayerController> Controller;
		TWeakObjectPtr<AKnightPartyState> Party;
		TWeakObjectPtr<AKnightPartyArenaState> Arena;
		TWeakObjectPtr<AKnightRPGStateActor> RPGState;
		TWeakObjectPtr<AKnightPartyArenaStartMarker> StartMarker;
		TWeakObjectPtr<AKnightItemPickup> SelectedPickup;
		TWeakObjectPtr<UKnightCombatComponent> ObservedCombat;
		TWeakObjectPtr<UKnightAbilitySystemComponent> ObservedASC;
		TArray<TWeakObjectPtr<AKnightRPGCharacter>> Knights;
		TArray<FKnightEntityId> Ids;
		TArray<FPrimaryAssetId> Heroes;
		FKnightEntityId ControlledId;
		FKnightInventoryView BeforeSkillsInventory;
		FKnightRuneTuning RuneBeforeSkills;
		FKnightRPGSnapshot PersistentSnapshot;
		TArray<TArray<FGameplayAbilitySpecHandle>> PersistentSpecs;
		TArray<int32> PersistentGrants;
		TArray<FKnightSourceProjection> PersistentProjections;
		TArray<FGuid> PersistentPickupIds;
		FDelegateHandle ReplyHandle;
		FDelegateHandle CombatHandle;
		FDelegateHandle EnergyHandle;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKnightRPGNormalPlaythroughTest, "Knight.Items.RPG.NormalPlaythrough",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FKnightRPGNormalPlaythroughTest::RunTest(const FString& Parameters)
{
	if (GEditor == nullptr || GEditor->IsPlaySessionInProgress() || !FPackageName::DoesPackageExist(KnightRPGPlaythrough::MapPath))
	{
		AddError(TEXT("Genuine RPG playthrough requires an idle Editor and the actual M5 bootstrap map."));
		return false;
	}
	if (!FEditorFileUtils::LoadMap(KnightRPGPlaythrough::MapPath, false, false))
	{
		AddError(TEXT("The actual M5 RPG map could not be opened."));
		return false;
	}
	FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<KnightRPGPlaythrough::FKnightRPGPlaythroughCommand>(*this));
	return true;
}

#endif
