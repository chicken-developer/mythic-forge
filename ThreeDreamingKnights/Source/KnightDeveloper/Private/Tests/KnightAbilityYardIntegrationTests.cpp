#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "AbilityYard/KnightAbilityDummy.h"
#include "AbilityYard/KnightAbilityStation.h"
#include "AbilityYard/KnightAbilityYardGameMode.h"
#include "AbilityYard/KnightAbilityYardHUD.h"
#include "AbilityYard/KnightAbilityYardState.h"
#include "Attributes/KnightAttributeSet.h"
#include "Character/KnightCharacter.h"
#include "Components/KnightAbilitySystemComponent.h"
#include "Components/KnightLifeComponent.h"
#include "Controllers/KnightCompanionController.h"
#include "Framework/KnightGameState.h"
#include "Input/KnightInputConfig.h"
#include "Party/KnightPartyState.h"
#include "Player/KnightPlayerController.h"
#include "Settings/KnightSettings.h"
#include "Tags/KnightGameplayTags.h"

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

namespace KnightAbilityYardIntegration
{
	const TCHAR* MapPath = TEXT("/Game/Knight/Maps/Playables/L_M02_AbilityYard");
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

	enum class EKnightAbilityYardPhase : uint8
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
	class FKnightAbilityYardScenario final : public IAutomationLatentCommand
	{
	public:
		explicit FKnightAbilityYardScenario(FAutomationTestBase& InTest)
			: Test(InTest), RunStarted(FPlatformTime::Seconds()), PhaseStarted(RunStarted)
		{
		}

		virtual ~FKnightAbilityYardScenario() override
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
			if (Phase == EKnightAbilityYardPhase::Complete)
			{
				return true;
			}
			if (Phase != EKnightAbilityYardPhase::Cleanup)
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
				else if (Phase >= EKnightAbilityYardPhase::InvalidInteract && !ValidateCanonicalState())
				{
					BeginCleanup();
				}
			}

			switch (Phase)
			{
			case EKnightAbilityYardPhase::WaitNavigation:
				if (NavigationWait.Update())
				{
					EnterPhase(EKnightAbilityYardPhase::Start, StartupTimeout);
				}
				break;
			case EKnightAbilityYardPhase::Start:
				StartSession();
				break;
			case EKnightAbilityYardPhase::Ready:
				WaitForReady();
				break;
			case EKnightAbilityYardPhase::InvalidInteract:
				if (ObserveCommand(false))
				{
					if (State->GetSnapshot().bPlayerDamaged || State->GetSnapshot().bCompanionDowned)
					{
						Fail(TEXT("Out-of-range station input changed authoritative objectives."));
						break;
					}
					BeginKeyCommand(EKnightAbilityYardPhase::InvalidRevive, EKeys::E);
				}
				break;
			case EKnightAbilityYardPhase::InvalidRevive:
				if (ObserveCommand(false))
				{
					Test.AddInfo(TEXT("Mapped F/E invalid attempts were rejected without changing health or objectives."));
					BeginKeyCommand(EKnightAbilityYardPhase::Pulse, EKeys::T);
				}
				break;
			case EKnightAbilityYardPhase::Pulse:
				if (ObserveCommand(true))
				{
					const UKnightAttributeSet* Attributes = Dummy->GetKnightAttributeSet();
					if (Attributes->GetHealth() >= InitialDummyHealth || !State->GetSnapshot().bDummyDamaged
						|| Dummy->GetKnightAbilitySystem()->GetCueDiagnostics(
							KnightGameplayTags::GameplayCue_Damage).ExecutedCount < 1)
					{
						Fail(TEXT("Mapped T did not produce real dummy damage and a real Gameplay Cue."));
						break;
					}
					Test.AddInfo(FString::Printf(TEXT("Mapped T damaged actual GAS dummy %.1f -> %.1f health."),
						InitialDummyHealth, Attributes->GetHealth()));
					EnterPhase(EKnightAbilityYardPhase::WalkDamage, WalkTimeout);
				}
				break;
			case EKnightAbilityYardPhase::WalkDamage:
				if (WalkTo(DamageStation->GetActorLocation(), StationApproachDistance))
				{
					BeginKeyCommand(EKnightAbilityYardPhase::Damage, EKeys::F);
				}
				break;
			case EKnightAbilityYardPhase::Damage:
				if (ObserveCommand(true))
				{
					DamagedHealth = Controller->GetControlledKnight()->GetKnightAttributeSet()->GetHealth();
					if (DamagedHealth >= InitialPlayerHealth || !State->GetSnapshot().bPlayerDamaged
						|| !FMath::IsNearlyEqual(HUD->GetDisplayedHealth(), DamagedHealth))
					{
						Fail(TEXT("Mapped F damage did not reach authoritative attributes and event-driven HUD."));
						break;
					}
					Test.AddInfo(FString::Printf(TEXT("Mapped F station damage: player %.1f -> %.1f, HUD agrees."),
						InitialPlayerHealth, DamagedHealth));
					BeginSwitch(EKnightAbilityYardPhase::SwitchAway, 1);
				}
				break;
			case EKnightAbilityYardPhase::SwitchAway:
				if (ObserveSwitch())
				{
					if (!FMath::IsNearlyEqual(Knights[0]->GetKnightAttributeSet()->GetHealth(), DamagedHealth))
					{
						Fail(TEXT("Switching to an AI Knight reset the original damaged Knight's attributes."));
						break;
					}
					BeginSwitch(EKnightAbilityYardPhase::SwitchBack, 0);
				}
				break;
			case EKnightAbilityYardPhase::SwitchBack:
				if (ObserveSwitch())
				{
					if (!FMath::IsNearlyEqual(HUD->GetDisplayedHealth(), DamagedHealth))
					{
						Fail(TEXT("HUD ownership rebind lost the original Knight's preserved health."));
						break;
					}
					EnterPhase(EKnightAbilityYardPhase::WalkDown, WalkTimeout);
				}
				break;
			case EKnightAbilityYardPhase::WalkDown:
				if (WalkTo(DownStation->GetActorLocation(), StationApproachDistance))
				{
					EnterPhase(EKnightAbilityYardPhase::WaitCompanion, ActionTimeout);
				}
				break;
			case EKnightAbilityYardPhase::WaitCompanion:
				if (HasNearbyCompanion() && WorldElapsed() >= SettleDuration)
				{
					BeginKeyCommand(EKnightAbilityYardPhase::Down, EKeys::F);
				}
				break;
			case EKnightAbilityYardPhase::Down:
				if (ObserveCommand(true))
				{
					const FKnightAbilityYardSnapshot& Snapshot = State->GetSnapshot();
					const FKnightRosterEntry* Entry = Party->FindSlot(Snapshot.ReviveSlot);
					if (!Snapshot.bCompanionDowned || Entry == nullptr || Entry->Knight == nullptr
						|| !Entry->Knight->GetLifeComponent()->IsDowned() || Entry->KnightId != Snapshot.ReviveKnightId)
					{
						Fail(TEXT("Real down station did not record a downed canonical teammate."));
						break;
					}
					ReviveTarget = Entry->Knight.Get();
					const UKnightAbilitySystemComponent* TargetASC = ReviveTarget->GetKnightAbilitySystem();
					const FKnightCueDiagnostics DownedCue = TargetASC->GetCueDiagnostics(
						KnightGameplayTags::GameplayCue_Downed);
					if (DownedCue.ActiveCount < 1 || !TargetASC->IsGameplayCueActive(KnightGameplayTags::GameplayCue_Downed))
					{
						Fail(TEXT("Real Downed state did not activate the persistent downed Gameplay Cue."));
						break;
					}
					CancelSwitchSlot = Entry->Slot == 1 ? 2 : 1;
					Test.AddInfo(FString::Printf(TEXT("Mapped F downed slot %d via real GAS; actor/GUID preserved."), Entry->Slot));
					EnterPhase(EKnightAbilityYardPhase::WalkRevive, WalkTimeout);
				}
				break;
			case EKnightAbilityYardPhase::WalkRevive:
				if (WalkTo(ReviveTarget->GetActorLocation(), ReviveApproachDistance))
				{
					BeginKeyCommand(EKnightAbilityYardPhase::WaitChannel, EKeys::E);
				}
				break;
			case EKnightAbilityYardPhase::WaitChannel:
				if (CommandCount > CommandBaseline && !LastResult.IsSuccess())
				{
					Fail(FString::Printf(TEXT("Mapped held E revive rejected: %s."), *LastResult.Message.ToString()));
				}
				else if (ReviveTarget->GetLifeComponent()->IsReviveInProgress())
				{
					// Keep E held through the mapped switch so possession cleanup must cancel the genuine task.
					BeginSwitch(EKnightAbilityYardPhase::SwitchCancel, CancelSwitchSlot, false);
				}
				break;
			case EKnightAbilityYardPhase::SwitchCancel:
				if (ObserveSwitch())
				{
					SetKey(EKeys::E, false);
					if (ReviveTarget->GetLifeComponent()->IsReviveInProgress()
						|| !ReviveTarget->GetLifeComponent()->IsDowned()
						|| Knights[0]->GetKnightAbilitySystem()->HasMatchingGameplayTag(KnightGameplayTags::State_Life_Reviving))
					{
						Fail(TEXT("Switching during held E leaked the revive reservation, task, or active tag."));
						break;
					}
					Test.AddInfo(TEXT("Mapped Knight switch canceled the active revive task/reservation and removed Reviving tag."));
					BeginSwitch(EKnightAbilityYardPhase::SwitchBackAfterCancel, 0);
				}
				break;
			case EKnightAbilityYardPhase::SwitchBackAfterCancel:
				if (ObserveSwitch())
				{
					EnterPhase(EKnightAbilityYardPhase::WalkFinalRevive, WalkTimeout);
				}
				break;
			case EKnightAbilityYardPhase::WalkFinalRevive:
				if (WalkTo(ReviveTarget->GetActorLocation(), ReviveApproachDistance))
				{
					BeginKeyCommand(EKnightAbilityYardPhase::Revive, EKeys::E);
				}
				break;
			case EKnightAbilityYardPhase::Revive:
				if (CommandCount > CommandBaseline && !LastResult.IsSuccess())
				{
					Fail(FString::Printf(TEXT("Final mapped E revive rejected: %s."), *LastResult.Message.ToString()));
				}
				else if (ReviveTarget->GetLifeComponent()->IsAlive())
				{
					SetKey(EKeys::E, false);
					const FKnightAbilityYardSnapshot& Snapshot = State->GetSnapshot();
					const FKnightCueDiagnostics DownedCue = ReviveTarget->GetKnightAbilitySystem()->GetCueDiagnostics(
						KnightGameplayTags::GameplayCue_Downed);
					if (!Snapshot.bCompleted || !Snapshot.bCompanionRevived || !HUD->IsDisplayingAbilityCompletion()
						|| ReviveTarget->GetKnightAttributeSet()->GetHealth() <= 0.0f
						|| ReviveTarget->GetLifeComponent()->IsReviveInProgress()
						|| DownedCue.ActiveCount != 0 || DownedCue.RemovedCount < 1
						|| ReviveTarget->GetKnightAbilitySystem()->IsGameplayCueActive(KnightGameplayTags::GameplayCue_Downed))
					{
						Fail(TEXT("Actual revive did not complete objectives/HUD or clean up its persistent downed cue."));
						break;
					}
					Test.AddInfo(TEXT("Held E completed the genuine timed revive; objective snapshot/HUD show 4/4 completion."));
					BeginRetry(EKnightAbilityYardPhase::Retry);
				}
				break;
			case EKnightAbilityYardPhase::Retry:
			case EKnightAbilityYardPhase::DeadRetry:
				if (ObserveCommand(true))
				{
					if (!ValidateReset())
					{
						break;
					}
					RetryOrigin = Controller->GetControlledKnight()->GetActorLocation();
					EnterPhase(Phase == EKnightAbilityYardPhase::Retry
						? EKnightAbilityYardPhase::RetrySettle : EKnightAbilityYardPhase::DeadRetrySettle, ActionTimeout);
				}
				break;
			case EKnightAbilityYardPhase::RetrySettle:
			case EKnightAbilityYardPhase::DeadRetrySettle:
				if (WorldElapsed() >= SettleDuration)
				{
					if (FVector::Dist2D(RetryOrigin, Controller->GetControlledKnight()->GetActorLocation()) > RetryDriftTolerance)
					{
						Fail(TEXT("Held movement input leaked through the GAS-aware exercise reset."));
						break;
					}
					SetKey(EKeys::W, false);
					if (Phase == EKnightAbilityYardPhase::DeadRetrySettle)
					{
						Test.AddInfo(TEXT("Mapped R recovered a genuinely dead Knight without respawn, duplicate grants, or stale input/cues."));
						BeginCleanup();
						break;
					}
					const FKnightResult Damage = UKnightAbilitySystemComponent::ApplyDamage(Dummy.Get(),
						Controller->GetControlledKnight(), InitialPlayerHealth * 2.0f);
					if (!Damage.IsSuccess() || !Controller->GetControlledKnight()->GetLifeComponent()->IsDowned())
					{
						Fail(TEXT("Named bleedout fault did not enter Downed through the real damage execution."));
						break;
					}
					Test.AddInfo(TEXT("Death retry fault injected real GAS damage; waiting for the actual bleedout timer."));
					EnterPhase(EKnightAbilityYardPhase::WaitDeath, DeathTimeout);
				}
				break;
			case EKnightAbilityYardPhase::WaitDeath:
				if (Controller->GetControlledKnight()->GetLifeComponent()->IsDead())
				{
					if (HUD->GetDisplayedLifeState() != EKnightLifeState::Dead)
					{
						Fail(TEXT("Genuine bleedout death did not reach the event-driven HUD."));
						break;
					}
					BeginKeyCommand(EKnightAbilityYardPhase::InvalidDeadAbility, EKeys::T);
				}
				break;
			case EKnightAbilityYardPhase::InvalidDeadAbility:
				if (ObserveCommand(false))
				{
					BeginRetry(EKnightAbilityYardPhase::DeadRetry);
				}
				break;
			case EKnightAbilityYardPhase::Cleanup:
				Cleanup();
				break;
			default:
				break;
			}
			return Phase == EKnightAbilityYardPhase::Complete;
		}

	private:
		void EnterPhase(EKnightAbilityYardPhase NextPhase, double Timeout)
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
				const FKnightAbilityYardSnapshot& Snapshot = State->GetSnapshot();
				Test.AddInfo(FString::Printf(TEXT("Failure snapshot: phase=%d revision=%d dummy=%.1f/%.1f "
					"dummyDamaged=%d playerDamaged=%d companionDowned=%d companionRevived=%d completed=%d "
					"reviveSlot=%d HUDCompleted=%d HUDHealth=%.1f."), static_cast<int32>(Phase), Snapshot.Revision,
					Snapshot.DummyHealth, Snapshot.DummyMaxHealth, Snapshot.bDummyDamaged, Snapshot.bPlayerDamaged,
					Snapshot.bCompanionDowned, Snapshot.bCompanionRevived, Snapshot.bCompleted, Snapshot.ReviveSlot,
					HUD.IsValid() && HUD->IsDisplayingAbilityCompletion(), HUD.IsValid() ? HUD->GetDisplayedHealth() : -1.0f));
			}
			if (ReviveTarget.IsValid())
			{
				const UKnightAbilitySystemComponent* ASC = ReviveTarget->GetKnightAbilitySystem();
				const FKnightCueDiagnostics Cue = ASC->GetCueDiagnostics(KnightGameplayTags::GameplayCue_Downed);
				Test.AddInfo(FString::Printf(TEXT("Failure revive target: actor=%s life=%d health=%.1f "
					"reservation=%d downedCueActiveCount=%d removedCount=%d IsGameplayCueActive=%d."),
					*ReviveTarget->GetPathName(), static_cast<int32>(ReviveTarget->GetLifeComponent()->GetLifeState()),
					ReviveTarget->GetKnightAttributeSet()->GetHealth(), ReviveTarget->GetLifeComponent()->IsReviveInProgress(),
					Cue.ActiveCount, Cue.RemovedCount, ASC->IsGameplayCueActive(KnightGameplayTags::GameplayCue_Downed)));
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
			EnterPhase(EKnightAbilityYardPhase::Ready, StartupTimeout);
		}

		void WaitForReady()
		{
			UWorld* PIEWorld = GEditor != nullptr ? GEditor->PlayWorld.Get() : nullptr;
			if (PIEWorld == nullptr || !PIEWorld->HasBegunPlay())
			{
				return;
			}
			World = PIEWorld;
			Mode = PIEWorld->GetAuthGameMode<AKnightAbilityYardGameMode>();
			Controller = Cast<AKnightPlayerController>(PIEWorld->GetFirstPlayerController());
			Party = Mode.IsValid() ? Mode->GetPartyState() : nullptr;
			State = Mode.IsValid() ? Mode->GetYardState() : nullptr;
			HUD = Controller.IsValid() ? Cast<AKnightAbilityYardHUD>(Controller->GetHUD()) : nullptr;
			if (!Controller.IsValid() || !Party.IsValid() || !State.IsValid() || !HUD.IsValid()
				|| !Party->IsCanonicalRosterReady() || Controller->GetControlledKnight() == nullptr || State->GetDummy() == nullptr)
			{
				return;
			}
			Dummy = State->GetDummy();
			const UKnightInputConfig* Config = Controller->GetKnightInputConfig();
			ULocalPlayer* LocalPlayer = Controller->GetLocalPlayer();
			UEnhancedInputLocalPlayerSubsystem* Input = LocalPlayer != nullptr
				? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
			if (Config == nullptr || Input == nullptr || !Input->HasMappingContext(Config->GetMappingContext())
				|| !Dummy->GetInitializationResult().IsSuccess() || Dummy->GetKnightAttributeSet()->GetHealth() <= 0.0f)
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
			for (const FKnightRosterEntry& Entry : Party->GetRoster())
			{
				if (Entry.Knight == nullptr || !Entry.Knight->GetAbilityInitializationResult().IsSuccess())
				{
					return;
				}
			}
			for (TActorIterator<AKnightAbilityStation> Iterator(PIEWorld); Iterator; ++Iterator)
			{
				TWeakObjectPtr<AKnightAbilityStation>& Destination =
					Iterator->GetStationKind() == EKnightAbilityStationKind::DamagePlayer ? DamageStation : DownStation;
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
			for (int32 Slot = 0; Slot < UKnightSettings::CanonicalPartySize; ++Slot)
			{
				const FKnightRosterEntry* Entry = Party->FindSlot(Slot);
				Knights.Add(Entry->Knight.Get());
				Identities.Add(Entry->KnightId);
				Definitions.Add(Entry->HeroDefinitionId);
				GrantCounts.Add(Entry->Knight->GetKnightAbilitySystem()->GetOwnedGrantCount());
				if (GrantCounts.Last() != 1 || Entry->Knight->GetKnightAbilitySystem()->GetActivatableAbilities().Num() != 2)
				{
					Fail(TEXT("A canonical Knight is missing the unique real Foundation source with two granted abilities."));
					return;
				}
			}
			InitialPlayerHealth = Controller->GetControlledKnight()->GetKnightAttributeSet()->GetHealth();
			InitialDummyHealth = Dummy->GetKnightAttributeSet()->GetHealth();
			const TSharedRef<FKnightAbilityYardScenario> Self = StaticCastSharedRef<FKnightAbilityYardScenario>(AsShared());
			CommandHandle = Controller->OnCommandResult.AddSP(Self, &FKnightAbilityYardScenario::HandleCommandResult);
			Test.AddInfo(TEXT("Real M2 PIE ready: exactly three canonical ASCs, one GAS dummy, two physical stations."));
			BeginKeyCommand(EKnightAbilityYardPhase::InvalidInteract, EKeys::F);
		}

		bool ValidateCanonicalState()
		{
			if (!World.IsValid() || !Party.IsValid() || !Controller.IsValid() || !State.IsValid()
				|| !Dummy.IsValid() || Controller->GetControlledKnight() == nullptr || Knights.Num() != 3)
			{
				Fail(TEXT("Ability yard lost its live world, controller, canonical roster, dummy, or objectives."));
				return false;
			}
			int32 Count = 0;
			for (TActorIterator<AKnightCharacter> Iterator(World.Get()); Iterator; ++Iterator)
			{
				++Count;
			}
			if (Count != UKnightSettings::CanonicalPartySize || Party->GetHumanCount() != 1)
			{
				Fail(TEXT("GAS fixtures broke the exactly-three-Knights / one-human invariant."));
				return false;
			}
			for (int32 Slot = 0; Slot < Knights.Num(); ++Slot)
			{
				const FKnightRosterEntry* Entry = Party->FindSlot(Slot);
				if (Entry == nullptr || !Knights[Slot].IsValid() || Entry->Knight != Knights[Slot].Get()
					|| Entry->KnightId != Identities[Slot] || Entry->HeroDefinitionId != Definitions[Slot]
					|| Knights[Slot]->GetKnightAbilitySystem()->GetOwnedGrantCount() != GrantCounts[Slot]
					|| Knights[Slot]->GetKnightAbilitySystem()->GetActivatableAbilities().Num() != 2
					|| Knights[Slot]->GetKnightAbilitySystem()->GetOwnerActor() != Knights[Slot].Get()
					|| Knights[Slot]->GetKnightAbilitySystem()->GetAvatarActor() != Knights[Slot].Get())
				{
					Fail(TEXT("Possession, revive, or retry replaced identity, ASC actor info, or doubled grants."));
					return false;
				}
			}
			return true;
		}

		void BeginKeyCommand(EKnightAbilityYardPhase NextPhase, const FKey& Key)
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

		void BeginSwitch(EKnightAbilityYardPhase NextPhase, int32 Slot, bool bReleaseExisting = true)
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
				|| Controller->GetControlledKnight() != Knights[DesiredSlot].Get())
			{
				Fail(TEXT("Mapped Knight selection succeeded without the intended canonical ownership change."));
				return false;
			}
			return true;
		}

		bool WalkTo(const FVector& Target, double AcceptanceDistance)
		{
			AKnightCharacter* Knight = Controller->GetControlledKnight();
			FVector Direction = Target - Knight->GetActorLocation();
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
			for (const TWeakObjectPtr<AKnightCharacter>& Knight : Knights)
			{
				if (Knight.Get() != Controller->GetControlledKnight() && Knight->GetLifeComponent()->IsAlive()
					&& FVector::Dist2D(Knight->GetActorLocation(), Controller->GetControlledKnight()->GetActorLocation()) < 600.0)
				{
					return true;
				}
			}
			return false;
		}

		void BeginRetry(EKnightAbilityYardPhase NextPhase)
		{
			PreviousRevision = State->GetSnapshot().Revision;
			BeginKeyCommand(NextPhase, EKeys::R);
			SetKey(EKeys::W, true);
		}

		bool ValidateReset()
		{
			const FKnightAbilityYardSnapshot& Snapshot = State->GetSnapshot();
			if (Snapshot.Revision <= PreviousRevision || Snapshot.bCompleted || Snapshot.bDummyDamaged
				|| Snapshot.bPlayerDamaged || Snapshot.bCompanionDowned || Snapshot.bCompanionRevived
				|| Snapshot.ReviveSlot != INDEX_NONE || Snapshot.ReviveKnightId.IsValid()
				|| HUD->IsDisplayingAbilityCompletion() || !Dummy->GetLifeComponent()->IsAlive()
				|| !FMath::IsNearlyEqual(Dummy->GetKnightAttributeSet()->GetHealth(), InitialDummyHealth))
			{
				Fail(TEXT("Mapped R did not reset actual objective snapshot and dummy through GAS."));
				return false;
			}
			for (const TWeakObjectPtr<AKnightCharacter>& Knight : Knights)
			{
				const UKnightAttributeSet* Attributes = Knight->GetKnightAttributeSet();
				if (!Knight->GetLifeComponent()->IsAlive() || Knight->GetLifeComponent()->IsReviveInProgress()
					|| !FMath::IsNearlyEqual(Attributes->GetHealth(), Attributes->GetMaxHealth())
					|| !FMath::IsNearlyEqual(Attributes->GetStamina(), Attributes->GetMaxStamina())
					|| !FMath::IsNearlyEqual(Attributes->GetRuneEnergy(), Attributes->GetMaxRuneEnergy())
					|| Knight->GetKnightAbilitySystem()->HasMatchingGameplayTag(KnightGameplayTags::State_Life_Reviving)
					|| Knight->GetKnightAbilitySystem()->HasMatchingGameplayTag(KnightGameplayTags::State_Life_Downed)
					|| Knight->GetKnightAbilitySystem()->HasMatchingGameplayTag(KnightGameplayTags::State_Life_Dead)
					|| Knight->GetCharacterMovement()->MovementMode == MOVE_None)
				{
					Fail(TEXT("Mapped R left stale GAS attributes, life tags, reservation, or disabled movement."));
					return false;
				}
				if (AKnightCompanionController* AI = Cast<AKnightCompanionController>(Knight->GetController());
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

		void HandleCommandResult(const FKnightResult& Result)
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
			if (Phase == EKnightAbilityYardPhase::Cleanup || Phase == EKnightAbilityYardPhase::Complete)
			{
				return;
			}
			ReleaseInput();
			Unsubscribe();
			EnterPhase(EKnightAbilityYardPhase::Cleanup, CleanupTimeout);
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
			EnterPhase(EKnightAbilityYardPhase::Complete, 0.0);
		}

		FAutomationTestBase& Test;
		EKnightAbilityYardPhase Phase = EKnightAbilityYardPhase::WaitNavigation;
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
		FKnightResult LastResult;
		FDelegateHandle CommandHandle;
		TSet<FKey> DownKeys;
		TArray<TWeakObjectPtr<AKnightCharacter>> Knights;
		TArray<FKnightEntityId> Identities;
		TArray<FPrimaryAssetId> Definitions;
		TArray<int32> GrantCounts;
		TWeakObjectPtr<UWorld> World;
		TWeakObjectPtr<AKnightAbilityYardGameMode> Mode;
		TWeakObjectPtr<AKnightPartyState> Party;
		TWeakObjectPtr<AKnightPlayerController> Controller;
		TWeakObjectPtr<AKnightAbilityYardState> State;
		TWeakObjectPtr<AKnightAbilityYardHUD> HUD;
		TWeakObjectPtr<AKnightAbilityDummy> Dummy;
		TWeakObjectPtr<AKnightAbilityStation> DamageStation;
		TWeakObjectPtr<AKnightAbilityStation> DownStation;
		TWeakObjectPtr<AKnightCharacter> ReviveTarget;
		TStrongObjectPtr<ULevelEditorPlaySettings> Settings;
		TArray<FIntPoint> OriginalPositions;
		FIntPoint OriginalLastSize = FIntPoint::ZeroValue;
		FIntPoint OriginalPosition = FIntPoint::ZeroValue;
		int32 OriginalWidth = 0;
		int32 OriginalHeight = 0;
		FWaitForEditorNavMeshBuildCommand NavigationWait;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKnightAbilityYardPlaythroughTest, "Knight.GAS.AbilityYardPlaythrough",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FKnightAbilityYardPlaythroughTest::RunTest(const FString& Parameters)
{
	if (GEditor == nullptr || GEditor->IsPlaySessionInProgress())
	{
		AddError(TEXT("Ability yard automation requires an idle Editor."));
		return false;
	}
	if (!FPackageName::DoesPackageExist(KnightAbilityYardIntegration::MapPath)
		|| !FEditorFileUtils::LoadMap(KnightAbilityYardIntegration::MapPath, false, false))
	{
		AddError(TEXT("Open the real M2 map after running KnightBootstrap -Milestone=2 on the integrated build."));
		return false;
	}
	AddInfo(TEXT("M2 gate uses physical mapped W/A/S/D, T/F/held E, 1/2/3 and R; no health assignment or fake progression."));
	AddCommand(new KnightAbilityYardIntegration::FKnightAbilityYardScenario(*this));
	return true;
}

#endif
