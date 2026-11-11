#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "AbilityYard/GolemAbilityDummy.h"
#include "AbilityYard/GolemAbilityStation.h"
#include "AbilityYard/GolemAbilityYardGameMode.h"
#include "AbilityYard/GolemAbilityYardHUD.h"
#include "AbilityYard/GolemAbilityYardState.h"
#include "Attributes/GolemAttributeSet.h"
#include "Character/GolemCharacter.h"
#include "Components/GolemAbilitySystemComponent.h"
#include "Components/GolemLifeComponent.h"
#include "Controllers/GolemCompanionController.h"
#include "Framework/GolemGameState.h"
#include "Input/GolemInputConfig.h"
#include "Party/GolemPartyState.h"
#include "Player/GolemPlayerController.h"
#include "Settings/GolemSettings.h"
#include "Tags/GolemGameplayTags.h"

#include "Editor.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "EnhancedInputSubsystems.h"
#include "FileHelpers.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/PlatformTime.h"
#include "InputCoreTypes.h"
#include "InputKeyEventArgs.h"
#include "Math/RotationMatrix.h"
#include "Misc/PackageName.h"
#include "PlayInEditorDataTypes.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Tests/AutomationEditorCommon.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectGlobals.h"

namespace GolemAbilityYardIntegration
{
	const TCHAR* MapPath = TEXT("/Game/Golem/Maps/Playables/L_M02_AbilityYard");
	constexpr double RunTimeout = 180.0;
	constexpr double StartupTimeout = 30.0;
	constexpr double ActionTimeout = 8.0;
	constexpr double WalkTimeout = 30.0;
	constexpr double DeathTimeout = 35.0;
	constexpr double CleanupTimeout = 15.0;
	constexpr double SettleDuration = 0.4;
	constexpr double InitialInputSettleDuration = 0.3;
	constexpr double StationApproachDistance = 120.0;
	constexpr double ReviveApproachDistance = 180.0;
	constexpr double RetryDriftTolerance = 25.0;

	enum class EGolemAbilityYardPhase : uint8
	{
		WaitNavigation,
		Start,
		Ready,
		InvalidInteract,
		InvalidRevive,
		Pulse,
		WalkDamage,
		Damage,
		SwitchAway,
		SwitchBack,
		WalkDown,
		WaitCompanion,
		Down,
		WalkRevive,
		WaitChannel,
		SwitchCancel,
		SwitchBackAfterCancel,
		WalkFinalRevive,
		Revive,
		Retry,
		RetrySettle,
		WaitDeath,
		InvalidDeadAbility,
		DeadRetry,
		DeadRetrySettle,
		Cleanup,
		Complete
	};

	/** Exercises mapped physical input and genuine GAS outcomes in an owned, bounded PIE session. */
	class FGolemAbilityYardScenario final : public IAutomationLatentCommand
	{
	public:
		explicit FGolemAbilityYardScenario(FAutomationTestBase& InTest)
			: Test(InTest), RunStarted(FPlatformTime::Seconds()), PhaseStarted(RunStarted)
		{
		}

		virtual ~FGolemAbilityYardScenario() override
		{
			ReleaseInput();
			Unsubscribe();
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
				if (!GEditor->IsPlaySessionInProgress())
				{
					RestoreSettings();
				}
			}
		}

		virtual bool Update() override
		{
			if (Phase == EGolemAbilityYardPhase::Complete)
			{
				return true;
			}
			if (Phase != EGolemAbilityYardPhase::Cleanup)
			{
				if (Test.HasAnyErrors())
				{
					BeginCleanup();
				}
				else if (FPlatformTime::Seconds() - RunStarted > RunTimeout
					|| FPlatformTime::Seconds() - PhaseStarted > PhaseTimeout)
				{
					Fail(FString::Printf(TEXT("Ability yard timed out in phase %d after %.1f seconds."),
						static_cast<int32>(Phase), FPlatformTime::Seconds() - RunStarted));
				}
				else if (Phase >= EGolemAbilityYardPhase::InvalidInteract && !ValidateCanonicalState())
				{
					BeginCleanup();
				}
			}

			switch (Phase)
			{
			case EGolemAbilityYardPhase::WaitNavigation:
				if (NavigationWait.Update())
				{
					EnterPhase(EGolemAbilityYardPhase::Start, StartupTimeout);
				}
				break;
			case EGolemAbilityYardPhase::Start:
				StartSession();
				break;
			case EGolemAbilityYardPhase::Ready:
				WaitForReady();
				break;
			case EGolemAbilityYardPhase::InvalidInteract:
				if (ObserveCommand(false))
				{
					if (State->GetSnapshot().bPlayerDamaged || State->GetSnapshot().bCompanionDowned)
					{
						Fail(TEXT("Out-of-range station input changed authoritative objectives."));
						break;
					}
					BeginKeyCommand(EGolemAbilityYardPhase::InvalidRevive, EKeys::E);
				}
				break;
			case EGolemAbilityYardPhase::InvalidRevive:
				if (ObserveCommand(false))
				{
					Test.AddInfo(TEXT("Mapped F/E invalid attempts were rejected without changing health or objectives."));
					BeginKeyCommand(EGolemAbilityYardPhase::Pulse, EKeys::T);
				}
				break;
			case EGolemAbilityYardPhase::Pulse:
				if (ObserveCommand(true))
				{
					const UGolemAttributeSet* Attributes = Dummy->GetGolemAttributeSet();
					if (Attributes->GetHealth() >= InitialDummyHealth || !State->GetSnapshot().bDummyDamaged
						|| Dummy->GetGolemAbilitySystem()->GetCueDiagnostics(
							GolemGameplayTags::GameplayCue_Damage).ExecutedCount < 1)
					{
						Fail(TEXT("Mapped T did not produce real dummy damage and a real Gameplay Cue."));
						break;
					}
					Test.AddInfo(FString::Printf(TEXT("Mapped T damaged actual GAS dummy %.1f -> %.1f health."),
						InitialDummyHealth, Attributes->GetHealth()));
					EnterPhase(EGolemAbilityYardPhase::WalkDamage, WalkTimeout);
				}
				break;
			case EGolemAbilityYardPhase::WalkDamage:
				if (WalkTo(DamageStation->GetActorLocation(), StationApproachDistance))
				{
					BeginKeyCommand(EGolemAbilityYardPhase::Damage, EKeys::F);
				}
				break;
			case EGolemAbilityYardPhase::Damage:
				if (ObserveCommand(true))
				{
					DamagedHealth = Controller->GetControlledGolem()->GetGolemAttributeSet()->GetHealth();
					if (DamagedHealth >= InitialPlayerHealth || !State->GetSnapshot().bPlayerDamaged
						|| !FMath::IsNearlyEqual(HUD->GetDisplayedHealth(), DamagedHealth))
					{
						Fail(TEXT("Mapped F damage did not reach authoritative attributes and event-driven HUD."));
						break;
					}
					Test.AddInfo(FString::Printf(TEXT("Mapped F station damage: player %.1f -> %.1f, HUD agrees."),
						InitialPlayerHealth, DamagedHealth));
					BeginSwitch(EGolemAbilityYardPhase::SwitchAway, 1);
				}
				break;
			case EGolemAbilityYardPhase::SwitchAway:
				if (ObserveSwitch())
				{
					if (!FMath::IsNearlyEqual(Golems[0]->GetGolemAttributeSet()->GetHealth(), DamagedHealth))
					{
						Fail(TEXT("Switching to an AI Golem reset the original damaged Golem's attributes."));
						break;
					}
					BeginSwitch(EGolemAbilityYardPhase::SwitchBack, 0);
				}
				break;
			case EGolemAbilityYardPhase::SwitchBack:
				if (ObserveSwitch())
				{
					if (!FMath::IsNearlyEqual(HUD->GetDisplayedHealth(), DamagedHealth))
					{
						Fail(TEXT("HUD ownership rebind lost the original Golem's preserved health."));
						break;
					}
					EnterPhase(EGolemAbilityYardPhase::WalkDown, WalkTimeout);
				}
				break;
			case EGolemAbilityYardPhase::WalkDown:
				if (WalkTo(DownStation->GetActorLocation(), StationApproachDistance))
				{
					EnterPhase(EGolemAbilityYardPhase::WaitCompanion, ActionTimeout);
				}
				break;
			case EGolemAbilityYardPhase::WaitCompanion:
				if (HasNearbyCompanion() && WorldElapsed() >= SettleDuration)
				{
					BeginKeyCommand(EGolemAbilityYardPhase::Down, EKeys::F);
				}
				break;
			case EGolemAbilityYardPhase::Down:
				if (ObserveCommand(true))
				{
					const FGolemAbilityYardSnapshot& Snapshot = State->GetSnapshot();
					const FGolemRosterEntry* Entry = Party->FindSlot(Snapshot.ReviveSlot);
					if (!Snapshot.bCompanionDowned || Entry == nullptr || Entry->Golem == nullptr
						|| !Entry->Golem->GetLifeComponent()->IsDowned() || Entry->GolemId != Snapshot.ReviveGolemId)
					{
						Fail(TEXT("Real down station did not record a downed canonical teammate."));
						break;
					}
					ReviveTarget = Entry->Golem.Get();
					const UGolemAbilitySystemComponent* TargetASC = ReviveTarget->GetGolemAbilitySystem();
					const FGolemCueDiagnostics DownedCue = TargetASC->GetCueDiagnostics(
						GolemGameplayTags::GameplayCue_Downed);
					if (DownedCue.ActiveCount < 1 || !TargetASC->IsGameplayCueActive(GolemGameplayTags::GameplayCue_Downed))
					{
						Fail(TEXT("Real Downed state did not activate the persistent downed Gameplay Cue."));
						break;
					}
					CancelSwitchSlot = Entry->Slot == 1 ? 2 : 1;
					Test.AddInfo(FString::Printf(TEXT("Mapped F downed slot %d via real GAS; actor/GUID preserved."), Entry->Slot));
					EnterPhase(EGolemAbilityYardPhase::WalkRevive, WalkTimeout);
				}
				break;
			case EGolemAbilityYardPhase::WalkRevive:
				if (WalkTo(ReviveTarget->GetActorLocation(), ReviveApproachDistance))
				{
					BeginKeyCommand(EGolemAbilityYardPhase::WaitChannel, EKeys::E);
				}
				break;
			case EGolemAbilityYardPhase::WaitChannel:
				if (CommandCount > CommandBaseline && !LastResult.IsSuccess())
				{
					Fail(FString::Printf(TEXT("Mapped held E revive rejected: %s."), *LastResult.Message.ToString()));
				}
				else if (ReviveTarget->GetLifeComponent()->IsReviveInProgress())
				{
					// Keep E held through the mapped switch so possession cleanup must cancel the genuine task.
					BeginSwitch(EGolemAbilityYardPhase::SwitchCancel, CancelSwitchSlot, false);
				}
				break;
			case EGolemAbilityYardPhase::SwitchCancel:
				if (ObserveSwitch())
				{
					SetKey(EKeys::E, false);
					if (ReviveTarget->GetLifeComponent()->IsReviveInProgress()
						|| !ReviveTarget->GetLifeComponent()->IsDowned()
						|| Golems[0]->GetGolemAbilitySystem()->HasMatchingGameplayTag(GolemGameplayTags::State_Life_Reviving))
					{
						Fail(TEXT("Switching during held E leaked the revive reservation, task, or active tag."));
						break;
					}
					Test.AddInfo(TEXT("Mapped Golem switch canceled the active revive task/reservation and removed Reviving tag."));
					BeginSwitch(EGolemAbilityYardPhase::SwitchBackAfterCancel, 0);
				}
				break;
			case EGolemAbilityYardPhase::SwitchBackAfterCancel:
				if (ObserveSwitch())
				{
					EnterPhase(EGolemAbilityYardPhase::WalkFinalRevive, WalkTimeout);
				}
				break;
			case EGolemAbilityYardPhase::WalkFinalRevive:
				if (WalkTo(ReviveTarget->GetActorLocation(), ReviveApproachDistance))
				{
					BeginKeyCommand(EGolemAbilityYardPhase::Revive, EKeys::E);
				}
				break;
			case EGolemAbilityYardPhase::Revive:
				if (CommandCount > CommandBaseline && !LastResult.IsSuccess())
				{
					Fail(FString::Printf(TEXT("Final mapped E revive rejected: %s."), *LastResult.Message.ToString()));
				}
				else if (ReviveTarget->GetLifeComponent()->IsAlive())
				{
					SetKey(EKeys::E, false);
					const FGolemAbilityYardSnapshot& Snapshot = State->GetSnapshot();
					const FGolemCueDiagnostics DownedCue = ReviveTarget->GetGolemAbilitySystem()->GetCueDiagnostics(
						GolemGameplayTags::GameplayCue_Downed);
					if (!Snapshot.bCompleted || !Snapshot.bCompanionRevived || !HUD->IsDisplayingAbilityCompletion()
						|| ReviveTarget->GetGolemAttributeSet()->GetHealth() <= 0.0f
						|| ReviveTarget->GetLifeComponent()->IsReviveInProgress()
						|| DownedCue.ActiveCount != 0 || DownedCue.RemovedCount < 1
						|| ReviveTarget->GetGolemAbilitySystem()->IsGameplayCueActive(GolemGameplayTags::GameplayCue_Downed))
					{
						Fail(TEXT("Actual revive did not complete objectives/HUD or clean up its persistent downed cue."));
						break;
					}
					Test.AddInfo(TEXT("Held E completed the genuine timed revive; objective snapshot/HUD show 4/4 completion."));
					BeginRetry(EGolemAbilityYardPhase::Retry);
				}
				break;
			case EGolemAbilityYardPhase::Retry:
			case EGolemAbilityYardPhase::DeadRetry:
				if (ObserveCommand(true))
				{
					if (!ValidateReset())
					{
						break;
					}
					RetryOrigin = Controller->GetControlledGolem()->GetActorLocation();
					EnterPhase(Phase == EGolemAbilityYardPhase::Retry
						? EGolemAbilityYardPhase::RetrySettle : EGolemAbilityYardPhase::DeadRetrySettle, ActionTimeout);
				}
				break;
			case EGolemAbilityYardPhase::RetrySettle:
			case EGolemAbilityYardPhase::DeadRetrySettle:
				if (WorldElapsed() >= SettleDuration)
				{
					if (FVector::Dist2D(RetryOrigin, Controller->GetControlledGolem()->GetActorLocation()) > RetryDriftTolerance)
					{
						Fail(TEXT("Held movement input leaked through the GAS-aware exercise reset."));
						break;
					}
					SetKey(EKeys::W, false);
					if (Phase == EGolemAbilityYardPhase::DeadRetrySettle)
					{
						Test.AddInfo(TEXT("Mapped R recovered a genuinely dead Golem without respawn, duplicate grants, or stale input/cues."));
						BeginCleanup();
						break;
					}
					const FGolemResult Damage = UGolemAbilitySystemComponent::ApplyDamage(Dummy.Get(),
						Controller->GetControlledGolem(), InitialPlayerHealth * 2.0f);
					if (!Damage.IsSuccess() || !Controller->GetControlledGolem()->GetLifeComponent()->IsDowned())
					{
						Fail(TEXT("Named bleedout fault did not enter Downed through the real damage execution."));
						break;
					}
					Test.AddInfo(TEXT("Death retry fault injected real GAS damage; waiting for the actual bleedout timer."));
					EnterPhase(EGolemAbilityYardPhase::WaitDeath, DeathTimeout);
				}
				break;
			case EGolemAbilityYardPhase::WaitDeath:
				if (Controller->GetControlledGolem()->GetLifeComponent()->IsDead())
				{
					if (HUD->GetDisplayedLifeState() != EGolemLifeState::Dead)
					{
						Fail(TEXT("Genuine bleedout death did not reach the event-driven HUD."));
						break;
					}
					BeginKeyCommand(EGolemAbilityYardPhase::InvalidDeadAbility, EKeys::T);
				}
				break;
			case EGolemAbilityYardPhase::InvalidDeadAbility:
				if (ObserveCommand(false))
				{
					BeginRetry(EGolemAbilityYardPhase::DeadRetry);
				}
				break;
			case EGolemAbilityYardPhase::Cleanup:
				Cleanup();
				break;
			default:
				break;
			}
			return Phase == EGolemAbilityYardPhase::Complete;
		}

	private:
		void EnterPhase(EGolemAbilityYardPhase NextPhase, double Timeout)
		{
			Phase = NextPhase;
			PhaseStarted = FPlatformTime::Seconds();
			PhaseTimeout = Timeout;
			PhaseWorldTime = World.IsValid() ? World->GetTimeSeconds() : 0.0;
		}

		double WorldElapsed() const
		{
			return World.IsValid() ? World->GetTimeSeconds() - PhaseWorldTime : 0.0;
		}

		void Fail(const FString& Message)
		{
			if (State.IsValid())
			{
				const FGolemAbilityYardSnapshot& Snapshot = State->GetSnapshot();
				Test.AddInfo(FString::Printf(TEXT("Failure snapshot: phase=%d revision=%d dummy=%.1f/%.1f "
					"dummyDamaged=%d playerDamaged=%d companionDowned=%d companionRevived=%d completed=%d "
					"reviveSlot=%d HUDCompleted=%d HUDHealth=%.1f."), static_cast<int32>(Phase), Snapshot.Revision,
					Snapshot.DummyHealth, Snapshot.DummyMaxHealth, Snapshot.bDummyDamaged, Snapshot.bPlayerDamaged,
					Snapshot.bCompanionDowned, Snapshot.bCompanionRevived, Snapshot.bCompleted, Snapshot.ReviveSlot,
					HUD.IsValid() && HUD->IsDisplayingAbilityCompletion(), HUD.IsValid() ? HUD->GetDisplayedHealth() : -1.0f));
			}
			if (ReviveTarget.IsValid())
			{
				const UGolemAbilitySystemComponent* ASC = ReviveTarget->GetGolemAbilitySystem();
				const FGolemCueDiagnostics Cue = ASC->GetCueDiagnostics(GolemGameplayTags::GameplayCue_Downed);
				Test.AddInfo(FString::Printf(TEXT("Failure revive target: actor=%s life=%d health=%.1f "
					"reservation=%d downedCueActiveCount=%d removedCount=%d IsGameplayCueActive=%d."),
					*ReviveTarget->GetPathName(), static_cast<int32>(ReviveTarget->GetLifeComponent()->GetLifeState()),
					ReviveTarget->GetGolemAttributeSet()->GetHealth(), ReviveTarget->GetLifeComponent()->IsReviveInProgress(),
					Cue.ActiveCount, Cue.RemovedCount, ASC->IsGameplayCueActive(GolemGameplayTags::GameplayCue_Downed)));
			}
			Test.AddError(Message);
			BeginCleanup();
		}

		void StartSession()
		{
			if (GEditor == nullptr || GEditor->IsPlaySessionInProgress())
			{
				Fail(TEXT("Ability yard requires an idle Editor and will preserve any user PIE session."));
				return;
			}
			const ULevelEditorPlaySettings* Defaults = GetDefault<ULevelEditorPlaySettings>();
			OriginalPositions = Defaults->MultipleInstancePositions;
			OriginalLastSize = Defaults->LastSize;
			OriginalPosition = Defaults->NewWindowPosition;
			OriginalWidth = Defaults->NewWindowWidth;
			OriginalHeight = Defaults->NewWindowHeight;
			Settings.Reset(DuplicateObject<ULevelEditorPlaySettings>(Defaults, GetTransientPackage()));
			Settings->SetPlayNetMode(PIE_Standalone);
			Settings->SetPlayNumberOfClients(1);
			Settings->SetRunUnderOneProcess(true);
			Settings->bLaunchSeparateServer = false;
			Settings->EnableGameSound = false;
			Settings->SetClientWindowSize(FIntPoint(960, 720));
			FRequestPlaySessionParams Request;
			Request.EditorPlaySettings = Settings.Get();
			Request.bAllowOnlineSubsystem = false;
			GEditor->RequestPlaySession(Request);
			bOwnsSession = true;
			EnterPhase(EGolemAbilityYardPhase::Ready, StartupTimeout);
		}

		void WaitForReady()
		{
			UWorld* PIEWorld = GEditor != nullptr ? GEditor->PlayWorld.Get() : nullptr;
			if (PIEWorld == nullptr || !PIEWorld->HasBegunPlay())
			{
				return;
			}
			World = PIEWorld;
			Mode = PIEWorld->GetAuthGameMode<AGolemAbilityYardGameMode>();
			Controller = Cast<AGolemPlayerController>(PIEWorld->GetFirstPlayerController());
			Party = Mode.IsValid() ? Mode->GetPartyState() : nullptr;
			State = Mode.IsValid() ? Mode->GetYardState() : nullptr;
			HUD = Controller.IsValid() ? Cast<AGolemAbilityYardHUD>(Controller->GetHUD()) : nullptr;
			if (!Controller.IsValid() || !Party.IsValid() || !State.IsValid() || !HUD.IsValid()
				|| !Party->IsCanonicalRosterReady() || Controller->GetControlledGolem() == nullptr || State->GetDummy() == nullptr)
			{
				return;
			}
			Dummy = State->GetDummy();
			const UGolemInputConfig* Config = Controller->GetGolemInputConfig();
			ULocalPlayer* LocalPlayer = Controller->GetLocalPlayer();
			UEnhancedInputLocalPlayerSubsystem* Input = LocalPlayer != nullptr
				? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
			if (Config == nullptr || Input == nullptr || !Input->HasMappingContext(Config->GetMappingContext())
				|| !Dummy->GetInitializationResult().IsSuccess() || Dummy->GetGolemAttributeSet()->GetHealth() <= 0.0f)
			{
				return;
			}
			if (InputReadyStarted < 0.0)
			{
				InputReadyStarted = PIEWorld->GetTimeSeconds();
			}
			if (PIEWorld->GetTimeSeconds() - InputReadyStarted < InitialInputSettleDuration)
			{
				return;
			}
			for (const FGolemRosterEntry& Entry : Party->GetRoster())
			{
				if (Entry.Golem == nullptr || !Entry.Golem->GetAbilityInitializationResult().IsSuccess())
				{
					return;
				}
			}
			for (TActorIterator<AGolemAbilityStation> Iterator(PIEWorld); Iterator; ++Iterator)
			{
				TWeakObjectPtr<AGolemAbilityStation>& Destination =
					Iterator->GetStationKind() == EGolemAbilityStationKind::DamagePlayer ? DamageStation : DownStation;
				if (Destination.IsValid())
				{
					Fail(TEXT("Ability yard contains a duplicate exercise station kind."));
					return;
				}
				Destination = *Iterator;
			}
			if (!DamageStation.IsValid() || !DownStation.IsValid())
			{
				Fail(TEXT("Ability yard is missing an authored damage or down/revive station."));
				return;
			}
			for (int32 Slot = 0; Slot < UGolemSettings::CanonicalPartySize; ++Slot)
			{
				const FGolemRosterEntry* Entry = Party->FindSlot(Slot);
				Golems.Add(Entry->Golem.Get());
				Identities.Add(Entry->GolemId);
				Definitions.Add(Entry->HeroDefinitionId);
				GrantCounts.Add(Entry->Golem->GetGolemAbilitySystem()->GetOwnedGrantCount());
				if (GrantCounts.Last() != 1 || Entry->Golem->GetGolemAbilitySystem()->GetActivatableAbilities().Num() != 2)
				{
					Fail(TEXT("A canonical Golem is missing the unique real Foundation source with two granted abilities."));
					return;
				}
			}
			InitialPlayerHealth = Controller->GetControlledGolem()->GetGolemAttributeSet()->GetHealth();
			InitialDummyHealth = Dummy->GetGolemAttributeSet()->GetHealth();
			const TSharedRef<FGolemAbilityYardScenario> Self = StaticCastSharedRef<FGolemAbilityYardScenario>(AsShared());
			CommandHandle = Controller->OnCommandResult.AddSP(Self, &FGolemAbilityYardScenario::HandleCommandResult);
			Test.AddInfo(TEXT("Real M2 PIE ready: exactly three canonical ASCs, one GAS dummy, two physical stations."));
			BeginKeyCommand(EGolemAbilityYardPhase::InvalidInteract, EKeys::F);
		}

		bool ValidateCanonicalState()
		{
			if (!World.IsValid() || !Party.IsValid() || !Controller.IsValid() || !State.IsValid()
				|| !Dummy.IsValid() || Controller->GetControlledGolem() == nullptr || Golems.Num() != 3)
			{
				Fail(TEXT("Ability yard lost its live world, controller, canonical roster, dummy, or objectives."));
				return false;
			}
			int32 Count = 0;
			for (TActorIterator<AGolemCharacter> Iterator(World.Get()); Iterator; ++Iterator)
			{
				++Count;
			}
			if (Count != UGolemSettings::CanonicalPartySize || Party->GetHumanCount() != 1)
			{
				Fail(TEXT("GAS fixtures broke the exactly-three-Golems / one-human invariant."));
				return false;
			}
			for (int32 Slot = 0; Slot < Golems.Num(); ++Slot)
			{
				const FGolemRosterEntry* Entry = Party->FindSlot(Slot);
				if (Entry == nullptr || !Golems[Slot].IsValid() || Entry->Golem != Golems[Slot].Get()
					|| Entry->GolemId != Identities[Slot] || Entry->HeroDefinitionId != Definitions[Slot]
					|| Golems[Slot]->GetGolemAbilitySystem()->GetOwnedGrantCount() != GrantCounts[Slot]
					|| Golems[Slot]->GetGolemAbilitySystem()->GetActivatableAbilities().Num() != 2
					|| Golems[Slot]->GetGolemAbilitySystem()->GetOwnerActor() != Golems[Slot].Get()
					|| Golems[Slot]->GetGolemAbilitySystem()->GetAvatarActor() != Golems[Slot].Get())
				{
					Fail(TEXT("Possession, revive, or retry replaced identity, ASC actor info, or doubled grants."));
					return false;
				}
			}
			return true;
		}

		void BeginKeyCommand(EGolemAbilityYardPhase NextPhase, const FKey& Key)
		{
			ReleaseInput();
			CommandBaseline = CommandCount;
			PulseKey = Key;
			EnterPhase(NextPhase, ActionTimeout);
			SetKey(Key, true);
		}

		bool ObserveCommand(bool bExpectedSuccess)
		{
			if (CommandCount <= CommandBaseline)
			{
				return false;
			}
			SetKey(PulseKey, false);
			if (LastResult.IsSuccess() != bExpectedSuccess)
			{
				Fail(FString::Printf(TEXT("Mapped %s result differs from expected %s: %s."), *PulseKey.ToString(),
					bExpectedSuccess ? TEXT("success") : TEXT("rejection"), *LastResult.Message.ToString()));
				return false;
			}
			return true;
		}

		void BeginSwitch(EGolemAbilityYardPhase NextPhase, int32 Slot, bool bReleaseExisting = true)
		{
			if (bReleaseExisting)
			{
				ReleaseInput();
			}
			DesiredSlot = Slot;
			CommandBaseline = CommandCount;
			PulseKey = Slot == 0 ? EKeys::One : (Slot == 1 ? EKeys::Two : EKeys::Three);
			EnterPhase(NextPhase, ActionTimeout);
			SetKey(PulseKey, true);
		}

		bool ObserveSwitch()
		{
			if (!ObserveCommand(true))
			{
				return false;
			}
			if (Party->GetControlledSlot(Controller.Get()) != DesiredSlot
				|| Controller->GetControlledGolem() != Golems[DesiredSlot].Get())
			{
				Fail(TEXT("Mapped Golem selection succeeded without the intended canonical ownership change."));
				return false;
			}
			return true;
		}

		bool WalkTo(const FVector& Target, double AcceptanceDistance)
		{
			AGolemCharacter* Golem = Controller->GetControlledGolem();
			FVector Direction = Target - Golem->GetActorLocation();
			Direction.Z = 0.0;
			if (Direction.Size() <= AcceptanceDistance)
			{
				ReleaseInput();
				return true;
			}
			Direction.Normalize();
			const FRotationMatrix Basis(FRotator(0.0, Controller->GetControlRotation().Yaw, 0.0));
			const double Forward = FVector::DotProduct(Direction, Basis.GetUnitAxis(EAxis::X));
			const double Right = FVector::DotProduct(Direction, Basis.GetUnitAxis(EAxis::Y));
			SetKey(EKeys::W, Forward > 0.2);
			SetKey(EKeys::S, Forward < -0.2);
			SetKey(EKeys::D, Right > 0.2);
			SetKey(EKeys::A, Right < -0.2);
			return false;
		}

		bool HasNearbyCompanion() const
		{
			for (const TWeakObjectPtr<AGolemCharacter>& Golem : Golems)
			{
				if (Golem.Get() != Controller->GetControlledGolem() && Golem->GetLifeComponent()->IsAlive()
					&& FVector::Dist2D(Golem->GetActorLocation(), Controller->GetControlledGolem()->GetActorLocation()) < 600.0)
				{
					return true;
				}
			}
			return false;
		}

		void BeginRetry(EGolemAbilityYardPhase NextPhase)
		{
			PreviousRevision = State->GetSnapshot().Revision;
			BeginKeyCommand(NextPhase, EKeys::R);
			SetKey(EKeys::W, true);
		}

		bool ValidateReset()
		{
			const FGolemAbilityYardSnapshot& Snapshot = State->GetSnapshot();
			if (Snapshot.Revision <= PreviousRevision || Snapshot.bCompleted || Snapshot.bDummyDamaged
				|| Snapshot.bPlayerDamaged || Snapshot.bCompanionDowned || Snapshot.bCompanionRevived
				|| Snapshot.ReviveSlot != INDEX_NONE || Snapshot.ReviveGolemId.IsValid()
				|| HUD->IsDisplayingAbilityCompletion() || !Dummy->GetLifeComponent()->IsAlive()
				|| !FMath::IsNearlyEqual(Dummy->GetGolemAttributeSet()->GetHealth(), InitialDummyHealth))
			{
				Fail(TEXT("Mapped R did not reset actual objective snapshot and dummy through GAS."));
				return false;
			}
			for (const TWeakObjectPtr<AGolemCharacter>& Golem : Golems)
			{
				const UGolemAttributeSet* Attributes = Golem->GetGolemAttributeSet();
				if (!Golem->GetLifeComponent()->IsAlive() || Golem->GetLifeComponent()->IsReviveInProgress()
					|| !FMath::IsNearlyEqual(Attributes->GetHealth(), Attributes->GetMaxHealth())
					|| !FMath::IsNearlyEqual(Attributes->GetStamina(), Attributes->GetMaxStamina())
					|| !FMath::IsNearlyEqual(Attributes->GetRuneEnergy(), Attributes->GetMaxRuneEnergy())
					|| Golem->GetGolemAbilitySystem()->HasMatchingGameplayTag(GolemGameplayTags::State_Life_Reviving)
					|| Golem->GetGolemAbilitySystem()->HasMatchingGameplayTag(GolemGameplayTags::State_Life_Downed)
					|| Golem->GetGolemAbilitySystem()->HasMatchingGameplayTag(GolemGameplayTags::State_Life_Dead)
					|| Golem->GetCharacterMovement()->MovementMode == MOVE_None)
				{
					Fail(TEXT("Mapped R left stale GAS attributes, life tags, reservation, or disabled movement."));
					return false;
				}
				if (AGolemCompanionController* AI = Cast<AGolemCompanionController>(Golem->GetController());
					AI != nullptr && !AI->IsFollowing())
				{
					Fail(TEXT("Mapped R left a living companion's StateTree stopped."));
					return false;
				}
			}
			return ValidateCanonicalState();
		}

		void SetKey(const FKey& Key, bool bIsDown)
		{
			if (!Controller.IsValid() || DownKeys.Contains(Key) == bIsDown)
			{
				return;
			}
			Controller->InputKey(FInputKeyEventArgs::CreateSimulated(Key,
				bIsDown ? IE_Pressed : IE_Released, bIsDown ? 1.0f : 0.0f));
			if (bIsDown)
			{
				DownKeys.Add(Key);
			}
			else
			{
				DownKeys.Remove(Key);
			}
		}

		void ReleaseInput()
		{
			for (const FKey& Key : DownKeys.Array())
			{
				SetKey(Key, false);
			}
			DownKeys.Reset();
		}

		void HandleCommandResult(const FGolemResult& Result)
		{
			++CommandCount;
			LastResult = Result;
		}

		void Unsubscribe()
		{
			if (Controller.IsValid())
			{
				Controller->OnCommandResult.Remove(CommandHandle);
			}
			CommandHandle.Reset();
		}

		void BeginCleanup()
		{
			if (Phase == EGolemAbilityYardPhase::Cleanup || Phase == EGolemAbilityYardPhase::Complete)
			{
				return;
			}
			ReleaseInput();
			Unsubscribe();
			EnterPhase(EGolemAbilityYardPhase::Cleanup, CleanupTimeout);
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
				if (FPlatformTime::Seconds() - PhaseStarted < CleanupTimeout)
				{
					return;
				}
				Test.AddError(TEXT("Ability yard PIE cleanup exceeded its bounded timeout."));
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
			EnterPhase(EGolemAbilityYardPhase::Complete, 0.0);
		}

		FAutomationTestBase& Test;
		EGolemAbilityYardPhase Phase = EGolemAbilityYardPhase::WaitNavigation;
		double RunStarted = 0.0;
		double PhaseStarted = 0.0;
		double PhaseWorldTime = 0.0;
		double InputReadyStarted = -1.0;
		double PhaseTimeout = StartupTimeout;
		bool bOwnsSession = false;
		int32 CommandCount = 0;
		int32 CommandBaseline = 0;
		int32 DesiredSlot = INDEX_NONE;
		int32 CancelSwitchSlot = INDEX_NONE;
		int32 PreviousRevision = 0;
		float InitialPlayerHealth = 0.0f;
		float InitialDummyHealth = 0.0f;
		float DamagedHealth = 0.0f;
		FVector RetryOrigin = FVector::ZeroVector;
		FKey PulseKey;
		FGolemResult LastResult;
		FDelegateHandle CommandHandle;
		TSet<FKey> DownKeys;
		TArray<TWeakObjectPtr<AGolemCharacter>> Golems;
		TArray<FGolemEntityId> Identities;
		TArray<FPrimaryAssetId> Definitions;
		TArray<int32> GrantCounts;
		TWeakObjectPtr<UWorld> World;
		TWeakObjectPtr<AGolemAbilityYardGameMode> Mode;
		TWeakObjectPtr<AGolemPartyState> Party;
		TWeakObjectPtr<AGolemPlayerController> Controller;
		TWeakObjectPtr<AGolemAbilityYardState> State;
		TWeakObjectPtr<AGolemAbilityYardHUD> HUD;
		TWeakObjectPtr<AGolemAbilityDummy> Dummy;
		TWeakObjectPtr<AGolemAbilityStation> DamageStation;
		TWeakObjectPtr<AGolemAbilityStation> DownStation;
		TWeakObjectPtr<AGolemCharacter> ReviveTarget;
		TStrongObjectPtr<ULevelEditorPlaySettings> Settings;
		TArray<FIntPoint> OriginalPositions;
		FIntPoint OriginalLastSize = FIntPoint::ZeroValue;
		FIntPoint OriginalPosition = FIntPoint::ZeroValue;
		int32 OriginalWidth = 0;
		int32 OriginalHeight = 0;
		FWaitForEditorNavMeshBuildCommand NavigationWait;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGolemAbilityYardPlaythroughTest, "Golem.GAS.AbilityYardPlaythrough",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGolemAbilityYardPlaythroughTest::RunTest(const FString& Parameters)
{
	if (GEditor == nullptr || GEditor->IsPlaySessionInProgress())
	{
		AddError(TEXT("Ability yard automation requires an idle Editor."));
		return false;
	}
	if (!FPackageName::DoesPackageExist(GolemAbilityYardIntegration::MapPath)
		|| !FEditorFileUtils::LoadMap(GolemAbilityYardIntegration::MapPath, false, false))
	{
		AddError(TEXT("Open the real M2 map after running GolemBootstrap -Milestone=2 on the integrated build."));
		return false;
	}
	AddInfo(TEXT("M2 gate uses physical mapped W/A/S/D, T/F/held E, 1/2/3 and R; no health assignment or fake progression."));
	AddCommand(new GolemAbilityYardIntegration::FGolemAbilityYardScenario(*this));
	return true;
}

#endif
