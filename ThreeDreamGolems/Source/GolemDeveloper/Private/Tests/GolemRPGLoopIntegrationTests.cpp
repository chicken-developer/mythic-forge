#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Abilities/GolemHeavyAttackAbility.h"
#include "Abilities/GolemLightAttackAbility.h"
#include "Animation/AnimInstance.h"
#include "Attributes/GolemAttributeSet.h"
#include "Blueprint/WidgetTree.h"
#include "Character/GolemCharacter.h"
#include "Components/GolemAbilitySystemComponent.h"
#include "Components/GolemCombatComponent.h"
#include "Components/GolemLifeComponent.h"
#include "Components/ScrollBox.h"
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
#include "Inventory/GolemInventoryComponent.h"
#include "Loot/GolemItemPickup.h"
#include "Misc/PackageName.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "Party/GolemPartyState.h"
#include "PartyArena/GolemArenaEnemy.h"
#include "PartyArena/GolemPartyArenaStartMarker.h"
#include "PartyArena/GolemPartyArenaState.h"
#include "PlayInEditorDataTypes.h"
#include "Progression/GolemRPGComponent.h"
#include "RPGLoop/GolemRPGCharacter.h"
#include "RPGLoop/GolemRPGLoopGameMode.h"
#include "RPGLoop/GolemRPGStateActor.h"
#include "RPGLoop/GolemRPGUIPlayerController.h"
#include "RPGLoop/GolemRPGUpgradeStation.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Tags/GolemGameplayTags.h"
#include "Tests/AutomationEditorCommon.h"
#include "UnrealEngine.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectGlobals.h"
#include "Widgets/GolemRPGPanel.h"
#include "Widgets/GolemRPGActionButton.h"

namespace GolemRPGPlaythrough
{
	const TCHAR* MapPath = TEXT("/Game/Knight/Maps/Playables/L_M05_RPGLoop");
	constexpr double TotalTimeout = 450.0;
	constexpr double EncounterTimeout = 120.0;
	constexpr double TravelTimeout = 45.0;
	constexpr double InputPulseDuration = 0.08;
	constexpr float MeleeReach = 130.0f;
	constexpr float InteractionReach = 135.0f;

	enum class EGolemRPGPlayPhase : uint8
	{
		Navigation, Start, Ready, WalkStartA, StartA, FightA, WalkLoot, Pickup, WalkStation,
		OpenPanel, EquipWeapon, EquipRune, PurchaseHammer, PurchaseRune, Respec, RebuyHammer,
		Continue, ClosePanel, WalkStartB, StartB, FightB, CompleteRetry, WalkWipeStart,
		WipeStart, Wipe, DefeatRetry, Cleanup, Complete
	};

	struct FGolemRPGPlayObservation
	{
		int32 Replies = 0;
		FGolemRPGCommandResult LastReply;
		int32 LightHitsA = 0;
		int32 LightHitsB = 0;
		int32 RuneHitsB = 0;
		int32 LightRequests = 0;
		int32 LightCommits = 0;
		float MaximumRuneSpendB = 0.0f;
		bool bObservedHumanMontage = false;
	};

	TArray<FGameplayAbilitySpecHandle> GetSpecs(const UGolemAbilitySystemComponent* ASC)
	{
		TArray<FGameplayAbilitySpecHandle> Result;
		for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
		{
			Result.Add(Spec.Handle);
		}
		return Result;
	}

	class FGolemRPGPlaythroughCommand final : public IAutomationLatentCommand
	{
	public:
		explicit FGolemRPGPlaythroughCommand(FAutomationTestBase& InTest)
			: Test(InTest), Observation(MakeShared<FGolemRPGPlayObservation>()),
			RunStarted(FPlatformTime::Seconds()), PhaseStarted(RunStarted)
		{
		}

		virtual ~FGolemRPGPlaythroughCommand() override
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
			if (Phase == EGolemRPGPlayPhase::Complete)
			{
				return true;
			}
			if (Phase < EGolemRPGPlayPhase::Cleanup
				&& (FPlatformTime::Seconds() - RunStarted > TotalTimeout || Elapsed() > PhaseTimeout))
			{
				Fail(GetDiagnostics(TEXT("Bounded genuine RPG playthrough timed out")));
			}
			if (Phase < EGolemRPGPlayPhase::Cleanup && Test.HasAnyErrors())
			{
				BeginCleanup();
			}
			if (World.IsValid() && Phase > EGolemRPGPlayPhase::Ready && Phase < EGolemRPGPlayPhase::Cleanup)
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
					const FVector Position = Controller->GetControlledGolem()->GetActorLocation();
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
			case EGolemRPGPlayPhase::Navigation:
				if (NavigationWait.Update())
				{
					Enter(EGolemRPGPlayPhase::Start, 20.0);
				}
				break;
			case EGolemRPGPlayPhase::Start: Start(); break;
			case EGolemRPGPlayPhase::Ready: Ready(); break;
			case EGolemRPGPlayPhase::WalkStartA: WalkStart(false); break;
			case EGolemRPGPlayPhase::StartA:
				if (Arena->GetSnapshot().Stage == EGolemArenaStage::Wave)
				{
					Enter(EGolemRPGPlayPhase::FightA, EncounterTimeout);
				}
				break;
			case EGolemRPGPlayPhase::FightA: FightEncounter(false); break;
			case EGolemRPGPlayPhase::WalkLoot: WalkLoot(); break;
			case EGolemRPGPlayPhase::Pickup: WaitPickup(); break;
			case EGolemRPGPlayPhase::WalkStation: WalkStation(); break;
			case EGolemRPGPlayPhase::OpenPanel: WaitPanel(); break;
			case EGolemRPGPlayPhase::EquipWeapon: WaitEquipWeapon(); break;
			case EGolemRPGPlayPhase::EquipRune: WaitEquipRune(); break;
			case EGolemRPGPlayPhase::PurchaseHammer: WaitPurchaseHammer(); break;
			case EGolemRPGPlayPhase::PurchaseRune: WaitPurchaseRune(); break;
			case EGolemRPGPlayPhase::Respec: WaitRespec(); break;
			case EGolemRPGPlayPhase::RebuyHammer: WaitRebuyHammer(); break;
			case EGolemRPGPlayPhase::Continue: WaitContinue(); break;
			case EGolemRPGPlayPhase::ClosePanel: WaitClosedPanel(); break;
			case EGolemRPGPlayPhase::WalkStartB: WalkStart(true); break;
			case EGolemRPGPlayPhase::StartB:
				if (Arena->GetSnapshot().Stage == EGolemArenaStage::Wave)
				{
					Enter(EGolemRPGPlayPhase::FightB, EncounterTimeout);
				}
				break;
			case EGolemRPGPlayPhase::FightB: FightEncounter(true); break;
			case EGolemRPGPlayPhase::CompleteRetry: WaitCompletionRetry(); break;
			case EGolemRPGPlayPhase::WalkWipeStart: WalkWipeStart(); break;
			case EGolemRPGPlayPhase::WipeStart: BeginWipe(); break;
			case EGolemRPGPlayPhase::Wipe: WaitWipe(); break;
			case EGolemRPGPlayPhase::DefeatRetry: WaitDefeatRetry(); break;
			case EGolemRPGPlayPhase::Cleanup: Cleanup(); break;
			default: break;
			}
			return Phase == EGolemRPGPlayPhase::Complete;
		}

	private:
		void Enter(EGolemRPGPlayPhase Next, double Timeout = 10.0)
		{
			Phase = Next;
			PhaseStarted = FPlatformTime::Seconds();
			PhaseTimeout = Timeout;
			WalkPath.Reset();
			NextPathQueryAt = -1.0;
			NextCombatDiagnosticAt = 0.0;
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
			Enter(EGolemRPGPlayPhase::Ready, 45.0);
		}

		void Ready()
		{
			UWorld* PIE = GEditor != nullptr ? GEditor->PlayWorld.Get() : nullptr;
			if (PIE == nullptr || !PIE->HasBegunPlay())
			{
				return;
			}
			World = PIE;
			Mode = PIE->GetAuthGameMode<AGolemRPGLoopGameMode>();
			Controller = Cast<AGolemRPGUIPlayerController>(PIE->GetFirstPlayerController());
			Party = Mode.IsValid() ? Mode->GetPartyState() : nullptr;
			Arena = Mode.IsValid() ? Mode->GetArenaState() : nullptr;
			RPGState = Mode.IsValid() ? Mode->GetRPGState() : nullptr;
			if (!Mode.IsValid() || !Controller.IsValid() || !Party.IsValid() || !Arena.IsValid()
				|| !RPGState.IsValid() || !Party->IsCanonicalRosterReady() || !RPGState->GetRPG()->IsInitialized()
				|| Controller->GetControlledGolem() == nullptr)
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
				const FGolemRosterEntry* Entry = Party->FindSlot(Slot);
				AGolemRPGCharacter* Golem = Entry != nullptr ? Cast<AGolemRPGCharacter>(Entry->Golem) : nullptr;
				if (Golem == nullptr || Golem->GetCombatComponent()->GetProfile() == nullptr
					|| Golem->GetWeaponUpgradeVisual() == nullptr || Golem->GetRuneVisual() == nullptr)
				{
					Fail(TEXT("Actual M5 canonical Golems lack their combat profile or native equipment presentation."));
					return;
				}
				Golems.Add(Golem);
				Ids.Add(Golem->GetGolemEntityId());
				Heroes.Add(Golem->GetHeroDefinitionId());
			}
			ControlledSlot = Party->GetControlledSlot(Controller.Get());
			if (!Golems.IsValidIndex(ControlledSlot))
			{
				Fail(TEXT("The actual solo controller has no valid canonical slot."));
				return;
			}
			ControlledId = Controller->GetControlledGolem()->GetGolemEntityId();
			BaseAttackPower = Controller->GetControlledGolem()->GetGolemAttributeSet()->GetAttackPower();
			BaseRuneCost = Controller->GetControlledGolem()->GetCombatComponent()->GetProfile()->GetTuning().RuneEnergyCost;
			for (TActorIterator<AGolemPartyArenaStartMarker> Iterator(World.Get()); Iterator; ++Iterator)
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
			LastPosition = Controller->GetControlledGolem()->GetActorLocation();
			Test.AddInfo(TEXT("Normal encounter A/B victory uses physical mapped W movement and actual LMB/G GAS attacks, companion StateTrees, F start, P pickups and visible CommonUI buttons activated with focused Slate Enter/Escape. No teleport or damage injection creates either victory."));
			Enter(EGolemRPGPlayPhase::WalkStartA, TravelTimeout);
		}

		void BindObservers()
		{
			const TWeakPtr<FGolemRPGPlayObservation> WeakObservation(Observation);
			const TWeakObjectPtr<AGolemRPGLoopGameMode> WeakMode(Mode);
			ReplyHandle = Controller->OnRPGCommandResult.AddLambda(
				[WeakObservation](const FGolemRPGCommandResult& Reply)
				{
					if (TSharedPtr<FGolemRPGPlayObservation> Current = WeakObservation.Pin())
					{
						++Current->Replies;
						Current->LastReply = Reply;
					}
				});
			ObservedCombat = Controller->GetControlledGolem()->GetCombatComponent();
			CombatHandle = ObservedCombat->OnCombatOutcome.AddLambda(
				[WeakObservation, WeakMode](const FGolemCombatOutcome& Outcome)
				{
					TSharedPtr<FGolemRPGPlayObservation> Current = WeakObservation.Pin();
					if (!Current.IsValid() || !WeakMode.IsValid() || WeakMode->GetRPGState() == nullptr)
					{
						return;
					}
					const EGolemRPGLoopStage Stage = WeakMode->GetRPGState()->GetLoopSnapshot().Stage;
					if (Outcome.Outcome == EGolemCombatOutcome::LightHit && Outcome.Amount > 0.0f)
					{
						Current->LightHitsA += Stage == EGolemRPGLoopStage::EncounterA
							|| Stage == EGolemRPGLoopStage::RewardAvailable ? 1 : 0;
						Current->LightHitsB += Stage == EGolemRPGLoopStage::EncounterB
							|| Stage == EGolemRPGLoopStage::Complete ? 1 : 0;
					}
					if (Outcome.Outcome == EGolemCombatOutcome::RuneHit && Outcome.Amount > 0.0f
						&& (Stage == EGolemRPGLoopStage::EncounterB || Stage == EGolemRPGLoopStage::Complete))
					{
						++Current->RuneHitsB;
					}
				});
			ObservedASC = Controller->GetControlledGolem()->GetGolemAbilitySystem();
			AbilityCommitHandle = ObservedASC->AbilityCommittedCallbacks.AddLambda(
				[WeakObservation](UGameplayAbility* Ability)
				{
					if (TSharedPtr<FGolemRPGPlayObservation> Current = WeakObservation.Pin();
						Current.IsValid() && Ability != nullptr && Ability->IsA<UGolemLightAttackAbility>())
					{
						++Current->LightCommits;
					}
				});
			EnergyHandle = ObservedASC->GetGameplayAttributeValueChangeDelegate(
				UGolemAttributeSet::GetRuneEnergyAttribute()).AddLambda(
					[WeakObservation, WeakMode](const FOnAttributeChangeData& Change)
					{
						TSharedPtr<FGolemRPGPlayObservation> Current = WeakObservation.Pin();
						if (Current.IsValid() && WeakMode.IsValid() && WeakMode->GetRPGState() != nullptr
							&& WeakMode->GetRPGState()->GetLoopSnapshot().Stage == EGolemRPGLoopStage::EncounterB)
						{
							Current->MaximumRuneSpendB = FMath::Max(Current->MaximumRuneSpendB,
								Change.OldValue - Change.NewValue);
						}
					});
		}

		bool ValidateCanonicalParty() const
		{
			if (!Party.IsValid() || Golems.Num() != 3 || Controller->GetControlledGolem() != Golems[ControlledSlot].Get())
			{
				return false;
			}
			int32 Count = 0;
			for (TActorIterator<AGolemCharacter> Iterator(World.Get()); Iterator; ++Iterator)
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
			AGolemCharacter* Golem = Controller->GetControlledGolem();
			if (FVector::DistSquared2D(Golem->GetActorLocation(), Goal) <= FMath::Square(Reach))
			{
				SetKey(EKeys::W, false);
				return true;
			}
			if (!Golem->GetLifeComponent()->IsAlive())
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
					World.Get(), Golem->GetActorLocation(), Goal, Golem);
				WalkPath = Path != nullptr && Path->IsValid() && !Path->IsPartial() ? Path->PathPoints : TArray<FVector>();
				NextWaypoint = WalkPath.Num() > 1 ? 1 : 0;
			}
			if (WalkPath.IsEmpty())
			{
				SetKey(EKeys::W, false);
				return false;
			}
			while (NextWaypoint < WalkPath.Num() - 1
				&& FVector::DistSquared2D(Golem->GetActorLocation(), WalkPath[NextWaypoint]) < FMath::Square(70.0f))
			{
				++NextWaypoint;
			}
			FVector Direction = WalkPath[NextWaypoint] - Golem->GetActorLocation();
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
				Enter(bSecond ? EGolemRPGPlayPhase::StartB : EGolemRPGPlayPhase::StartA);
			}
		}

		AGolemArenaEnemy* FindNearestEnemy() const
		{
			AGolemArenaEnemy* Result = nullptr;
			double Best = TNumericLimits<double>::Max();
			for (AGolemArenaEnemy* Enemy : Mode->GetEncounterEnemies())
			{
				if (IsValid(Enemy) && Enemy->GetLifeComponent()->IsAlive())
				{
					const double Distance = FVector::DistSquared2D(Controller->GetControlledGolem()->GetActorLocation(),
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
			AGolemCharacter* Golem = Controller->GetControlledGolem();
			UGolemAbilitySystemComponent* ASC = Golem->GetGolemAbilitySystem();
			const FGameplayAbilitySpec* Light = ASC->FindAbilitySpecFromClass(UGolemLightAttackAbility::StaticClass());
			const FGameplayAbilitySpec* Heavy = ASC->FindAbilitySpecFromClass(UGolemHeavyAttackAbility::StaticClass());
			UAnimInstance* Animation = Golem->GetMesh()->GetAnimInstance();
			Observation->bObservedHumanMontage |= ((Light != nullptr && Light->IsActive()) || (Heavy != nullptr && Heavy->IsActive()))
				&& ASC->GetCurrentMontage() != nullptr && Animation != nullptr
				&& Animation->Montage_IsPlaying(ASC->GetCurrentMontage());
		}

		void FightEncounter(bool bSecond)
		{
			if (Arena->GetSnapshot().Stage == EGolemArenaStage::Defeat)
			{
				Fail(GetDiagnostics(TEXT("The party wiped during a genuine mapped RPG victory playthrough")));
				return;
			}
			if (Arena->GetSnapshot().Stage == EGolemArenaStage::Victory)
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
					if (RPGState->GetLoopSnapshot().Stage != EGolemRPGLoopStage::Complete)
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
					Enter(EGolemRPGPlayPhase::CompleteRetry);
				}
				else if (RPGState->GetLoopSnapshot().Stage == EGolemRPGLoopStage::RewardAvailable)
				{
					Enter(EGolemRPGPlayPhase::WalkLoot, TravelTimeout);
				}
				return;
			}
			AGolemCharacter* Golem = Controller->GetControlledGolem();
			if (!Golem->GetLifeComponent()->IsAlive())
			{
				SetKey(EKeys::W, false);
				return;
			}
			AGolemArenaEnemy* Target = FindNearestEnemy();
			if (Target == nullptr)
			{
				SetKey(EKeys::W, false);
				return;
			}
			if (Elapsed() >= NextCombatDiagnosticAt && CombatDiagnostics < 40)
			{
				NextCombatDiagnosticAt = Elapsed() + 1.0;
				++CombatDiagnostics;
				const FVector Direction = (Target->GetActorLocation() - Golem->GetActorLocation()).GetSafeNormal2D();
				Test.AddInfo(FString::Printf(TEXT("Native M5 combat phase%d t%.2f humanHP%.2f targetHP%.2f range%.2f facingDot%.3f transfer%d attacking%d staggered%d lightRequests%d commits%d AHit%d BHit%d delta%.4f"),
					static_cast<int32>(Phase), Elapsed(), Golem->GetGolemAttributeSet()->GetHealth(),
					Target->GetGolemAbilitySystem()->GetNumericAttribute(UGolemAttributeSet::GetHealthAttribute()),
					FVector::Dist2D(Golem->GetActorLocation(), Target->GetActorLocation()),
					FVector::DotProduct(Golem->GetActorForwardVector(), Direction),
					Golem->GetCombatComponent()->CanTransferControl(), Golem->GetCombatComponent()->GetSnapshot().bAttacking,
					Golem->GetCombatComponent()->GetSnapshot().bStaggered, Observation->LightRequests,
					Observation->LightCommits, Observation->LightHitsA, Observation->LightHitsB, World->GetDeltaSeconds()));
			}
			if (PulseKey.IsValid() || !WalkToward(Target->GetActorLocation(), MeleeReach))
			{
				return;
			}
			UGolemCombatComponent* Combat = Golem->GetCombatComponent();
			const double Now = World->GetTimeSeconds();
			if (!Combat->CanTransferControl() || Now - LastAttackAt < 0.95)
			{
				return;
			}
			const FVector Facing = (Target->GetActorLocation() - Golem->GetActorLocation()).GetSafeNormal2D();
			Controller->SetControlRotation(Facing.Rotation());
			if (FVector::DotProduct(Golem->GetActorForwardVector(), Facing) < 0.98)
			{
				// Camera yaw cannot turn this movement-oriented Character while standing in melee range.
				// Use the real mapped input before attacking; preserve the entire pulse on later updates.
				Pulse(EKeys::W);
				return;
			}
			if (bSecond && Observation->RuneHitsB == 0 && Now - LastRuneRequestAt >= 3.0
				&& Golem->GetGolemAttributeSet()->GetRuneEnergy() >= ExpectedRuneCost)
			{
				LastRuneRequestAt = Now;
				LastAttackAt = Now;
				Pulse(EKeys::G);
				return;
			}
			LastAttackAt = Now;
			++Observation->LightRequests;
			Pulse(EKeys::LeftMouseButton);
		}

		void WalkLoot()
		{
			if (!SelectedPickup.IsValid() || SelectedPickup->IsClaimed())
			{
				SelectedPickup.Reset();
				double Best = TNumericLimits<double>::Max();
				for (TActorIterator<AGolemItemPickup> Iterator(World.Get()); Iterator; ++Iterator)
				{
					if (!Iterator->IsClaimed())
					{
						const double Distance = FVector::DistSquared2D(Controller->GetControlledGolem()->GetActorLocation(),
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
				Enter(EGolemRPGPlayPhase::WalkStation, TravelTimeout);
				return;
			}
			if (WalkToward(SelectedPickup->GetActorLocation(), InteractionReach) && !PulseKey.IsValid())
			{
				RepliesBefore = Observation->Replies;
				Pulse(EKeys::P);
				Enter(EGolemRPGPlayPhase::Pickup);
			}
		}

		bool HasUIReceipt(EGolemRPGOperation Operation)
		{
			if (Observation->Replies <= RepliesBefore)
			{
				return false;
			}
			const FGolemRPGCommandResult& Reply = Observation->LastReply;
			if (!Reply.Result.IsSuccess() || Reply.Operation != Operation || Reply.GolemId != ControlledId)
			{
				Fail(FString::Printf(TEXT("Mapped/Slate RPG action failed op%d actual%d error%d Golem%s: %s"),
					static_cast<int32>(Operation), static_cast<int32>(Reply.Operation), static_cast<int32>(Reply.Result.Error),
					*Reply.GolemId.Value.ToString(), *Reply.Result.Message.ToString()));
				return false;
			}
			return true;
		}

		void WaitPickup()
		{
			if (!HasUIReceipt(EGolemRPGOperation::Pickup) || PulseKey.IsValid())
			{
				return;
			}
			if (!SelectedPickup.IsValid() || !SelectedPickup->IsClaimed())
			{
				Fail(TEXT("Mapped P receipt succeeded without claiming the actual targeted pickup."));
				return;
			}
			SelectedPickup.Reset();
			Enter(EGolemRPGPlayPhase::WalkLoot, TravelTimeout);
		}

		void WalkStation()
		{
			if (WalkToward(Mode->GetUpgradeStation()->GetActorLocation(), InteractionReach) && !PulseKey.IsValid())
			{
				if (!Mode->IsInUpgradeSafeZone(Controller->GetControlledGolem()))
				{
					return;
				}
				Pulse(EKeys::I);
				Enter(EGolemRPGPlayPhase::OpenPanel);
			}
		}

		bool HasFocusedPanel() const
		{
			UGolemRPGPanel* Panel = Controller.IsValid() ? Controller->GetRPGPanel() : nullptr;
			return Panel != nullptr && Panel->IsActivated() && Panel->GetSnapshot().GolemId == ControlledId
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

		bool ActivateVisibleButton(const FString& ExpectedCaption)
		{
			UGolemRPGPanel* Panel = Controller->GetRPGPanel();
			if (!HasFocusedPanel() || Panel->WidgetTree == nullptr)
			{
				return false;
			}
			TArray<UWidget*> Widgets;
			Panel->WidgetTree->GetAllWidgets(Widgets);
			UGolemRPGActionButton* MatchingButton = nullptr;
			for (UWidget* Widget : Widgets)
			{
				UGolemRPGActionButton* Button = Cast<UGolemRPGActionButton>(Widget);
				if (Button != nullptr && Button->IsVisible() && Button->GetIsEnabled()
					&& Button->GetCaption().ToString() == ExpectedCaption)
				{
					MatchingButton = Button;
					break;
				}
			}
			if (MatchingButton == nullptr)
			{
				Fail(FString::Printf(TEXT("Actual visible CommonUI action button is missing: %s"), *ExpectedCaption));
				return false;
			}
			if (PendingSlateButton.Get() != MatchingButton)
			{
				PendingSlateButton = MatchingButton;
				for (UWidget* Parent = MatchingButton->GetParent(); Parent != nullptr; Parent = Parent->GetParent())
				{
					if (UScrollBox* Scroll = Cast<UScrollBox>(Parent))
					{
						Scroll->ScrollWidgetIntoView(MatchingButton, false, EDescendantScrollDestination::Center);
					}
				}
				MatchingButton->SetUserFocus(Controller.Get());
				return false;
			}
			if (!MatchingButton->HasUserFocus(Controller.Get()) &&
				!MatchingButton->HasUserFocusedDescendants(Controller.Get()))
			{
				Fail(TEXT("Visible CommonUI action button did not receive local Slate focus."));
				return false;
			}
			PendingSlateButton.Reset();
			return SlateKey(EKeys::Enter);
		}

		void WaitPanel()
		{
			if (!HasFocusedPanel() || PulseKey.IsValid())
			{
				return;
			}
			if (ActivateVisibleButton(TEXT("Equip available weapon")))
			{
				Enter(EGolemRPGPlayPhase::EquipWeapon);
			}
		}

		bool HasExpectedVisuals(bool bWeapon, bool bRune) const
		{
			for (int32 Slot = 0; Slot < 3; ++Slot)
			{
				const bool bSource = Slot == ControlledSlot;
				if (Golems[Slot]->GetWeaponUpgradeVisual()->IsVisible() != (bSource && bWeapon)
					|| Golems[Slot]->GetRuneVisual()->IsVisible() != (bSource && bRune))
				{
					return false;
				}
			}
			return true;
		}

		void WaitEquipWeapon()
		{
			if (!HasUIReceipt(EGolemRPGOperation::Equip))
			{
				return;
			}
			const FGolemRPGGolemView View = RPGState->GetRPG()->GetGolemView(ControlledId);
			if (View.FindAssignment(GolemGameplayTags::Slot_Equipment_Weapon) == nullptr
				|| Controller->GetControlledGolem()->GetGolemAttributeSet()->GetAttackPower() <= BaseAttackPower
				|| !HasExpectedVisuals(true, false))
			{
				Fail(TEXT("Focused Equip button did not equip the correct Golem with real GAS stat bonus/native weapon visual."));
				return;
			}
			EquippedAttackPower = Controller->GetControlledGolem()->GetGolemAttributeSet()->GetAttackPower();
			if (ActivateVisibleButton(TEXT("Equip available rune")))
			{
				Enter(EGolemRPGPlayPhase::EquipRune);
			}
		}

		void WaitEquipRune()
		{
			if (!HasUIReceipt(EGolemRPGOperation::Rune))
			{
				return;
			}
			const FGolemRPGGolemView View = RPGState->GetRPG()->GetGolemView(ControlledId);
			if (View.FindAssignment(GolemGameplayTags::Slot_Equipment_Rune) == nullptr || !HasExpectedVisuals(true, true))
			{
				Fail(TEXT("Focused Rune button did not equip the correct Golem's real rune assignment/native visual."));
				return;
			}
			RuneBeforeSkills = View.RuneTuning;
			BeforeSkillsPoints = View.SkillPoints;
			if (ActivateVisibleButton(TEXT("M5 upgrade: Hammer Force")))
			{
				Enter(EGolemRPGPlayPhase::PurchaseHammer);
			}
		}

		void WaitPurchaseHammer()
		{
			if (!HasUIReceipt(EGolemRPGOperation::PurchaseSkill))
			{
				return;
			}
			const FGolemRPGGolemView View = RPGState->GetRPG()->GetGolemView(ControlledId);
			if (!View.HasSkill(FPrimaryAssetId(TEXT("KnightSkill"), TEXT("HammerForce")))
				|| View.SkillPoints >= BeforeSkillsPoints
				|| Controller->GetControlledGolem()->GetGolemAttributeSet()->GetAttackPower() <= EquippedAttackPower)
			{
				Fail(TEXT("Focused Hammer Force button did not atomically buy HammerForce and apply its actual stat bonus."));
				return;
			}
			if (ActivateVisibleButton(TEXT("M5 upgrade: Rune Focus")))
			{
				Enter(EGolemRPGPlayPhase::PurchaseRune);
			}
		}

		void WaitPurchaseRune()
		{
			if (!HasUIReceipt(EGolemRPGOperation::PurchaseSkill))
			{
				return;
			}
			const FGolemRPGGolemView View = RPGState->GetRPG()->GetGolemView(ControlledId);
			if (!View.HasSkill(FPrimaryAssetId(TEXT("KnightSkill"), TEXT("RuneFocus"))) || View.SkillPoints != 0
				|| View.RuneTuning.DurationScale <= RuneBeforeSkills.DurationScale)
			{
				Fail(TEXT("Focused Rune Focus button did not buy the real prerequisite-gated RuneFocus/rune duration modifier."));
				return;
			}
			if (ActivateVisibleButton(TEXT("Refund M5 purchased upgrades")))
			{
				Enter(EGolemRPGPlayPhase::Respec);
			}
		}

		void WaitRespec()
		{
			if (!HasUIReceipt(EGolemRPGOperation::Respec))
			{
				return;
			}
			const FGolemRPGGolemView View = RPGState->GetRPG()->GetGolemView(ControlledId);
			if (!View.PurchasedSkills.IsEmpty() || View.SkillPoints != BeforeSkillsPoints
				|| RPGState->GetInventory()->GetView().Items != BeforeSkillsInventory.Items
				|| !(View.RuneTuning == RuneBeforeSkills)
				|| !FMath::IsNearlyEqual(Controller->GetControlledGolem()->GetGolemAttributeSet()->GetAttackPower(), EquippedAttackPower))
			{
				Fail(TEXT("Focused Refund button did not atomically refund exact paid item identities/points and remove skill modifiers."));
				return;
			}
			if (ActivateVisibleButton(TEXT("M5 upgrade: Hammer Force")))
			{
				Enter(EGolemRPGPlayPhase::RebuyHammer);
			}
		}

		void WaitRebuyHammer()
		{
			if (!HasUIReceipt(EGolemRPGOperation::PurchaseSkill))
			{
				return;
			}
			const FGolemActionModifier Modifier = Controller->GetControlledGolem()->GetGolemAbilitySystem()->GetActionModifier(
				GolemGameplayTags::Ability_Combat_Rune);
			ExpectedRuneCost = BaseRuneCost * Modifier.CostScale;
			if (!RPGState->GetRPG()->GetGolemView(ControlledId).HasSkill(FPrimaryAssetId(TEXT("KnightSkill"), TEXT("HammerForce")))
				|| !HasExpectedVisuals(true, true) || !Controller->GetRPGPanel()->GetSnapshot().bCanContinue)
			{
				Fail(TEXT("Real rebuilt upgraded loadout did not enable the CommonUI host Continue action."));
				return;
			}
			if (ActivateVisibleButton(TEXT("Host Continue")))
			{
				Enter(EGolemRPGPlayPhase::Continue);
			}
		}

		void WaitContinue()
		{
			if (RPGState->GetLoopSnapshot().Stage != EGolemRPGLoopStage::EncounterB
				|| Arena->GetSnapshot().Stage != EGolemArenaStage::AwaitingStart)
			{
				return;
			}
			if (!Controller->GetLastRPGLoopResult().IsSuccess() || !HasExpectedVisuals(true, true))
			{
				Fail(TEXT("Focused Slate Enter did not perform the real host continuation while preserving loadout visuals."));
				return;
			}
			UGolemRPGPanel* Panel = Controller->GetRPGPanel();
			if (Panel != nullptr && Panel->IsActivated())
			{
				if (!SlateKey(EKeys::Escape))
				{
					return;
				}
			}
			Enter(EGolemRPGPlayPhase::ClosePanel);
		}

		void WaitClosedPanel()
		{
			UGolemRPGPanel* Panel = Controller->GetRPGPanel();
			if (Panel != nullptr && Panel->IsActivated())
			{
				return;
			}
			Enter(EGolemRPGPlayPhase::WalkStartB, TravelTimeout);
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
			for (const TWeakObjectPtr<AGolemRPGCharacter>& Golem : Golems)
			{
				UGolemAbilitySystemComponent* ASC = Golem->GetGolemAbilitySystem();
				PersistentSpecs.Add(GetSpecs(ASC));
				PersistentGrants.Add(ASC->GetOwnedGrantCount());
				PersistentProjections.Add(ASC->GetSourceProjection(GolemRPG::ProjectionSource));
			}
			PersistentPickupIds.Reset();
			for (TActorIterator<AGolemItemPickup> Iterator(World.Get()); Iterator; ++Iterator)
			{
				PersistentPickupIds.Add(Iterator->GetPickupId());
			}
			PersistentPickupIds.Sort();
			return true;
		}

		bool HasPersistentState() const
		{
			FGolemRPGSnapshot Current;
			if (!RPGState->GetRPG()->CaptureSnapshot(Current).IsSuccess() || !(Current == PersistentSnapshot)
				|| !HasExpectedVisuals(true, true))
			{
				return false;
			}
			TArray<FGuid> CurrentPickupIds;
			for (TActorIterator<AGolemItemPickup> Iterator(World.Get()); Iterator; ++Iterator)
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
				const UGolemAbilitySystemComponent* ASC = Golems[Slot]->GetGolemAbilitySystem();
				const FGolemSourceProjection Projection = ASC->GetSourceProjection(GolemRPG::ProjectionSource);
				const FGolemSourceProjection& Previous = PersistentProjections[Slot];
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
			if (PulseKey.IsValid() || Arena->GetSnapshot().Stage != EGolemArenaStage::AwaitingStart
				|| RPGState->GetLoopSnapshot().Stage != EGolemRPGLoopStage::EncounterB)
			{
				return;
			}
			if (!HasPersistentState())
			{
				Fail(TEXT("Mapped completion retry changed reward/pickup IDs, receipts, inventory, progression or source grants."));
				return;
			}
			Enter(EGolemRPGPlayPhase::WalkWipeStart, TravelTimeout);
		}

		void WalkWipeStart()
		{
			if (WalkToward(StartMarker->GetActorLocation(), InteractionReach) && !PulseKey.IsValid())
			{
				Pulse(EKeys::F);
				Enter(EGolemRPGPlayPhase::WipeStart);
			}
		}

		void BeginWipe()
		{
			if (Arena->GetSnapshot().Stage != EGolemArenaStage::Wave || PulseKey.IsValid())
			{
				return;
			}
			AGolemArenaEnemy* Enemy = FindNearestEnemy();
			if (Enemy == nullptr)
			{
				Fail(TEXT("Retry/wipe fault requires a genuine living hostile GAS source."));
				return;
			}
			ReleaseInputs();
			Test.AddInfo(TEXT("After both genuine mapped victories, explicit server GAS damage injects a three-Golem wipe solely to verify failure/retry persistence."));
			FScopedConditionalWorldSwitcher Switch(World.Get());
			for (const TWeakObjectPtr<AGolemRPGCharacter>& Golem : Golems)
			{
				const float Amount = Golem->GetGolemAttributeSet()->GetMaxHealth()
					+ Golem->GetGolemAttributeSet()->GetDefense() + 1000.0f;
				if (!UGolemAbilitySystemComponent::ApplyDamage(Enemy, Golem.Get(), Amount).IsSuccess())
				{
					Fail(TEXT("Explicit post-victory wipe fault was rejected."));
					return;
				}
			}
			Enter(EGolemRPGPlayPhase::Wipe);
		}

		void WaitWipe()
		{
			if (Arena->GetSnapshot().Stage != EGolemArenaStage::Defeat || Elapsed() < 0.3)
			{
				return;
			}
			if (!HasPersistentState())
			{
				Fail(TEXT("Real wipe changed persistent host inventory/receipts or Golem loadout/grants."));
				return;
			}
			Pulse(EKeys::R);
			Enter(EGolemRPGPlayPhase::DefeatRetry);
		}

		void WaitDefeatRetry()
		{
			if (Arena->GetSnapshot().Stage != EGolemArenaStage::AwaitingStart || PulseKey.IsValid() || Elapsed() < 0.5)
			{
				return;
			}
			if (!HasPersistentState())
			{
				Fail(TEXT("Mapped defeat retry changed persistent inventory/rewards/loadout or duplicated grants."));
				return;
			}
			for (const TWeakObjectPtr<AGolemRPGCharacter>& Golem : Golems)
			{
				if (!Golem->GetLifeComponent()->IsAlive())
				{
					Fail(TEXT("Real RPG retry did not recover the existing canonical Golems' life state."));
					return;
				}
			}
			Test.AddInfo(TEXT("Actual completion and wipe retries preserved one reward issuance, all pickup/item IDs, exact receipts, three Golem views, GAS projections/specs/grants and equipment visuals."));
			BeginCleanup();
		}

		FString GetDiagnostics(const TCHAR* Prefix) const
		{
			AGolemCharacter* Golem = Controller.IsValid() ? Controller->GetControlledGolem() : nullptr;
			return FString::Printf(TEXT("%s phase%d elapsed%.2f arena%d loop%d pos%s path%d walk%.0fcm replies%d op%d error%d AHit%d BHit%d BRune%d RuneSpend%.2f expected%.2f panelFocus%d"),
				Prefix, static_cast<int32>(Phase), Elapsed(),
				Arena.IsValid() ? static_cast<int32>(Arena->GetSnapshot().Stage) : -1,
				RPGState.IsValid() ? static_cast<int32>(RPGState->GetLoopSnapshot().Stage) : -1,
				Golem != nullptr ? *Golem->GetActorLocation().ToString() : TEXT("none"), WalkPath.Num(), PhysicalDistance,
				Observation->Replies, static_cast<int32>(Observation->LastReply.Operation),
				static_cast<int32>(Observation->LastReply.Result.Error), Observation->LightHitsA, Observation->LightHitsB,
				Observation->RuneHitsB, Observation->MaximumRuneSpendB, ExpectedRuneCost,
				Controller.IsValid() && FSlateApplication::IsInitialized() ? HasFocusedPanel() : false);
		}

		void Fail(const FString& Message)
		{
			if (Phase >= EGolemRPGPlayPhase::Cleanup)
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
				ObservedASC->AbilityCommittedCallbacks.Remove(AbilityCommitHandle);
				ObservedASC->GetGameplayAttributeValueChangeDelegate(UGolemAttributeSet::GetRuneEnergyAttribute()).Remove(EnergyHandle);
			}
			ReplyHandle.Reset();
			CombatHandle.Reset();
			EnergyHandle.Reset();
			AbilityCommitHandle.Reset();
		}

		void BeginCleanup()
		{
			if (Phase >= EGolemRPGPlayPhase::Cleanup)
			{
				return;
			}
			UnbindObservers();
			ReleaseInputs();
			Enter(EGolemRPGPlayPhase::Cleanup, 15.0);
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
			Enter(EGolemRPGPlayPhase::Complete);
		}

		FAutomationTestBase& Test;
		TSharedPtr<FGolemRPGPlayObservation> Observation;
		EGolemRPGPlayPhase Phase = EGolemRPGPlayPhase::Navigation;
		double RunStarted = 0.0;
		double PhaseStarted = 0.0;
		double PhaseTimeout = 45.0;
		double ReadyWorldTime = -1.0;
		double NextPathQueryAt = -1.0;
		double LastAttackAt = -10.0;
		double LastRuneRequestAt = -10.0;
		double NextCombatDiagnosticAt = 0.0;
		int32 CombatDiagnostics = 0;
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
		TWeakObjectPtr<AGolemRPGLoopGameMode> Mode;
		TWeakObjectPtr<AGolemRPGUIPlayerController> Controller;
		TWeakObjectPtr<AGolemPartyState> Party;
		TWeakObjectPtr<AGolemPartyArenaState> Arena;
		TWeakObjectPtr<AGolemRPGStateActor> RPGState;
		TWeakObjectPtr<AGolemPartyArenaStartMarker> StartMarker;
		TWeakObjectPtr<AGolemItemPickup> SelectedPickup;
		TWeakObjectPtr<UGolemCombatComponent> ObservedCombat;
		TWeakObjectPtr<UGolemAbilitySystemComponent> ObservedASC;
		TWeakObjectPtr<UGolemRPGActionButton> PendingSlateButton;
		TArray<TWeakObjectPtr<AGolemRPGCharacter>> Golems;
		TArray<FGolemEntityId> Ids;
		TArray<FPrimaryAssetId> Heroes;
		FGolemEntityId ControlledId;
		FGolemInventoryView BeforeSkillsInventory;
		FGolemRuneTuning RuneBeforeSkills;
		FGolemRPGSnapshot PersistentSnapshot;
		TArray<TArray<FGameplayAbilitySpecHandle>> PersistentSpecs;
		TArray<int32> PersistentGrants;
		TArray<FGolemSourceProjection> PersistentProjections;
		TArray<FGuid> PersistentPickupIds;
		FDelegateHandle ReplyHandle;
		FDelegateHandle CombatHandle;
		FDelegateHandle EnergyHandle;
		FDelegateHandle AbilityCommitHandle;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGolemRPGNormalPlaythroughTest, "Golem.Items.RPG.NormalPlaythrough",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGolemRPGNormalPlaythroughTest::RunTest(const FString& Parameters)
{
	if (GEditor == nullptr || GEditor->IsPlaySessionInProgress() || !FPackageName::DoesPackageExist(GolemRPGPlaythrough::MapPath))
	{
		AddError(TEXT("Genuine RPG playthrough requires an idle Editor and the actual M5 bootstrap map."));
		return false;
	}
	if (!FEditorFileUtils::LoadMap(GolemRPGPlaythrough::MapPath, false, false))
	{
		AddError(TEXT("The actual M5 RPG map could not be opened."));
		return false;
	}
	FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<GolemRPGPlaythrough::FGolemRPGPlaythroughCommand>(*this));
	return true;
}

#endif
