#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Abilities/KnightFinisherAbility.h"
#include "Abilities/KnightTraversalAbility.h"
#include "Animation/KnightAnimNotifyState_TraceWindow.h"
#include "Attributes/KnightAttributeSet.h"
#include "Camera/KnightCameraModeComponent.h"
#include "Character/KnightCharacter.h"
#include "CombatChallenge/KnightCombatChallengeGameMode.h"
#include "CombatChallenge/KnightCombatChallengeHUD.h"
#include "CombatChallenge/KnightCombatChallengeState.h"
#include "CombatChallenge/KnightCombatFixture.h"
#include "Components/KnightAbilitySystemComponent.h"
#include "Components/KnightCombatComponent.h"
#include "Components/KnightLifeComponent.h"
#include "Definitions/KnightCombatProfileDefinition.h"
#include "Finisher/KnightFinisherComponent.h"
#include "Party/KnightPartyState.h"
#include "Player/KnightPlayerController.h"
#include "Tags/KnightGameplayTags.h"
#include "Traversal/KnightTraversalComponent.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "FileHelpers.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayAbilitySpec.h"
#include "HAL/PlatformTime.h"
#include "InputCoreTypes.h"
#include "InputKeyEventArgs.h"
#include "Misc/PackageName.h"
#include "MotionWarpingComponent.h"
#include "PlayInEditorDataTypes.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Tests/AutomationEditorCommon.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectGlobals.h"

namespace KnightCombatChallengeTest
{
	const TCHAR* MapPath = TEXT("/Game/Knight/Maps/Playables/L_M03_CombatChallenge");

	enum class EKnightChallengePhase : uint8
	{
		Navigation,
		Start,
		Ready,
		InvalidTraversal,
		InvalidFinisher,
		Lock,
		Light,
		Combo,
		Heavy,
		Finisher,
		SwitchDefense,
		Block,
		ParryWait,
		Parry,
		DodgeWait,
		Dodge,
		SwitchRune,
		Rune,
		Traversal,
		Retry,
		InterruptTraversal,
		InterruptRetry,
		Death,
		DeadRetry,
		Cleanup,
		Complete
	};

	class FKnightChallengeScenario final : public IAutomationLatentCommand
	{
	public:
		explicit FKnightChallengeScenario(FAutomationTestBase& InTest)
			: Test(InTest), RunStarted(FPlatformTime::Seconds()), PhaseStarted(RunStarted)
		{
		}

		virtual ~FKnightChallengeScenario() override
		{
			ReleaseInput();
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
			if (Phase == EKnightChallengePhase::Complete)
			{
				return true;
			}
			if (Phase != EKnightChallengePhase::Cleanup &&
				(FPlatformTime::Seconds() - RunStarted > 240.0 || FPlatformTime::Seconds() - PhaseStarted > PhaseTimeout))
			{
				Fail(TEXT("The bounded real combat challenge phase timed out."));
			}
			if (Phase != EKnightChallengePhase::Cleanup && Test.HasAnyErrors())
			{
				BeginCleanup();
			}
			if (Phase >= EKnightChallengePhase::InvalidTraversal && Phase < EKnightChallengePhase::Cleanup)
			{
				if (!ValidateIdentityAndGrants())
				{
					BeginCleanup();
				}
				else if (WorldElapsed() > 0.08 && PulseKey.IsValid())
				{
					SetKey(PulseKey, false);
					PulseKey = FKey();
				}
			}
			AKnightCharacter* Knight = Controller.IsValid() ? Controller->GetControlledKnight() : nullptr;
			UKnightCombatComponent* Combat = Knight != nullptr ? Knight->FindComponentByClass<UKnightCombatComponent>() : nullptr;
			switch (Phase)
			{
			case EKnightChallengePhase::Navigation:
				if (NavigationWait.Update())
				{
					Enter(EKnightChallengePhase::Start, 30.0);
				}
				break;
			case EKnightChallengePhase::Start:
				Start();
				break;
			case EKnightChallengePhase::Ready:
				Ready();
				break;
			case EKnightChallengePhase::InvalidTraversal:
				if (WorldElapsed() > 0.35)
				{
					if (Controller->GetLastCommandResult().IsSuccess() || State->GetSnapshot().TraversalCount != 0)
					{
						Fail(TEXT("V without a physical obstacle was accepted."));
						break;
					}
					Press(EKnightChallengePhase::InvalidFinisher, EKeys::H);
				}
				break;
			case EKnightChallengePhase::InvalidFinisher:
				if (WorldElapsed() > 0.35)
				{
					if (Controller->GetLastCommandResult().IsSuccess()
						|| Knight->FindComponentByClass<UKnightFinisherComponent>()->IsReserved())
					{
						Fail(TEXT("H without a staggered victim was accepted or leaked a reservation."));
						break;
					}
					PlaceNear(Melee.Get(), 110.0);
					Press(EKnightChallengePhase::Lock, EKeys::MiddleMouseButton);
				}
				break;
			case EKnightChallengePhase::Lock:
				if (WorldElapsed() > 0.25 && Combat->GetLockedTarget() == Melee.Get() && State->GetSnapshot().bTargetLocked)
				{
					TargetHealthBefore = Melee->GetKnightAttributeSet()->GetHealth();
					Press(EKnightChallengePhase::Light, EKeys::LeftMouseButton);
				}
				break;
			case EKnightChallengePhase::Light:
				if (State->GetSnapshot().LightHitCount > 0 && !Combat->GetSnapshot().bAttacking)
				{
					const float Actual = TargetHealthBefore - Melee->GetKnightAttributeSet()->GetHealth();
					if (!FMath::IsNearlyEqual(Actual, Combat->GetProfile()->GetAttacks()[0].Damage)
						|| State->GetSnapshot().LightHitCount != 1)
					{
						Fail(TEXT("A multi-frame swing damaged its target more than once in one trace window."));
						break;
					}
					Test.AddInfo(TEXT("Actual mapped LMB montage/native trace notify applied exactly one configured hit."));
					Press(EKnightChallengePhase::Combo, EKeys::LeftMouseButton);
				}
				break;
			case EKnightChallengePhase::Combo:
				if (!bBuffered && Combat->HasOpenComboWindow())
				{
					SetKey(EKeys::LeftMouseButton, true);
					bBuffered = true;
					BufferPressedAt = World->GetTimeSeconds();
				}
				if (bBuffered && World->GetTimeSeconds() - BufferPressedAt > 0.08)
				{
					SetKey(EKeys::LeftMouseButton, false);
				}
				if (State->GetSnapshot().ComboCount > 0 && !Combat->GetSnapshot().bAttacking
					&& !Melee->GetCombatComponent()->GetSnapshot().bStaggered)
				{
					if (State->GetSnapshot().LightHitCount < 3)
					{
						Fail(TEXT("Actual combo section transition did not produce both configured melee hits."));
						break;
					}
					Test.AddInfo(TEXT("Mapped LMB inside the real native combo notify advanced sections and dealt real hits."));
					Press(EKnightChallengePhase::Heavy, EKeys::RightMouseButton);
				}
				break;
			case EKnightChallengePhase::Heavy:
				if (State->GetSnapshot().HeavyHitCount > 0 && State->GetSnapshot().StaggerCount > 0
					&& !Combat->GetSnapshot().bAttacking)
				{
					if (!Melee->GetCombatComponent()->GetSnapshot().bStaggered)
					{
						Fail(TEXT("The authored heavy-hit stagger expired before the playable finisher opportunity."));
						break;
					}
					FinisherCameraModesBefore = Knight->FindComponentByClass<UKnightCameraModeComponent>()->GetModeCount();
					Press(EKnightChallengePhase::Finisher, EKeys::H);
				}
				break;
			case EKnightChallengePhase::Finisher:
				if (Knight->FindComponentByClass<UKnightFinisherComponent>()->HasActiveFinisher())
				{
					ObserveFinisher(Knight, Combat);
				}
				if (State->GetSnapshot().FinisherCount > 0)
				{
					if (FinisherCompletedAt < 0.0)
					{
						FinisherCompletedAt = World->GetTimeSeconds();
					}
					// The independent lock validator releases its layer on the next bounded validation tick.
					if (World->GetTimeSeconds() - FinisherCompletedAt < 0.3)
					{
						break;
					}
					if (!bObservedFinisher)
					{
						Fail(TEXT("Finisher did not show both authored montages advancing with a paired reservation and its own camera/input layer."));
						break;
					}
					if (!Melee->GetLifeComponent()->IsDead())
					{
						Fail(TEXT("Confirmed finisher did not kill its actual enemy victim through GAS."));
						break;
					}
					if (Melee->FindComponentByClass<UKnightFinisherComponent>()->IsReserved()
						|| Knight->FindComponentByClass<UKnightFinisherComponent>()->IsReserved())
					{
						Fail(TEXT("Finisher completion left a source or victim reservation active."));
						break;
					}
					if (Knight->FindComponentByClass<UKnightCameraModeComponent>()->GetModeCount() != 0
						|| Controller->IsMoveInputIgnored() || Controller->IsLookInputIgnored())
					{
						Fail(TEXT("Finisher completion or invalid-target cleanup left a camera/input layer active."));
						break;
					}
					UKnightAbilitySystemComponent* ASC = Knight->GetKnightAbilitySystem();
					const FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromClass(UKnightFinisherAbility::StaticClass());
					if (Spec == nullptr || Spec->IsActive()
						|| ASC->HasMatchingGameplayTag(KnightGameplayTags::State_Combat_Finishing)
						|| Melee->GetKnightAbilitySystem()->HasMatchingGameplayTag(KnightGameplayTags::State_Combat_Finishing))
					{
						Fail(TEXT("Finisher completion did not end the real GAS ability or remove both Finishing tags."));
						break;
					}
					const FKnightCombatChallengeSnapshot BeforeReplay = State->GetSnapshot();
					const float VictimHealthBeforeReplay = Melee->GetKnightAttributeSet()->GetHealth();
					const int32 GrantsBeforeReplay = ASC->GetOwnedGrantCount();
					const int32 AbilitiesBeforeReplay = ASC->GetActivatableAbilities().Num();
					const FKnightResult Replay = Knight->FindComponentByClass<UKnightFinisherComponent>()->EndFinisher(
						ObservedFinisherReservation, true);
					if (Replay.Error != EKnightError::NotOwned || State->GetSnapshot().Revision != BeforeReplay.Revision
						|| BeforeReplay.FinisherCount != 1 || State->GetSnapshot().FinisherCount != 1
						|| !Melee->GetLifeComponent()->IsDead()
						|| !FMath::IsNearlyEqual(Melee->GetKnightAttributeSet()->GetHealth(), VictimHealthBeforeReplay)
						|| ASC->GetOwnedGrantCount() != GrantsBeforeReplay
						|| ASC->GetActivatableAbilities().Num() != AbilitiesBeforeReplay || Spec->IsActive()
						|| Knight->FindComponentByClass<UKnightFinisherComponent>()->IsReserved()
						|| Melee->FindComponentByClass<UKnightFinisherComponent>()->IsReserved()
						|| Knight->FindComponentByClass<UKnightCameraModeComponent>()->GetModeCount() != 0
						|| Controller->IsMoveInputIgnored() || Controller->IsLookInputIgnored()
						|| ASC->HasMatchingGameplayTag(KnightGameplayTags::State_Combat_Finishing)
						|| Melee->GetKnightAbilitySystem()->HasMatchingGameplayTag(KnightGameplayTags::State_Combat_Finishing))
					{
						Fail(TEXT("A stale completed finisher reservation was accepted or changed death/outcomes/grants/camera/input."));
						break;
					}
					if (!ValidateIdentityAndGrants())
					{
						break;
					}
					Test.AddInfo(TEXT("Completed finisher GA is inactive, both Finishing tags are absent, and stale reservation replay "
						"was rejected NotOwned without changing confirmed outcome, GAS death, grants, camera or input."));
					Press(EKnightChallengePhase::SwitchDefense, EKeys::Two);
				}
				break;
			case EKnightChallengePhase::SwitchDefense:
				if (Party->GetControlledSlot(Controller.Get()) == 1 && WorldElapsed() > 0.4)
				{
					PlaceNear(Scripted.Get(), 110.0);
					PlayerHealthBefore = Knight->GetKnightAttributeSet()->GetHealth();
					Press(EKnightChallengePhase::Block, EKeys::Gamepad_DPad_Left);
					SetKey(EKeys::Gamepad_LeftShoulder, true);
				}
				break;
			case EKnightChallengePhase::Block:
				if (State->GetSnapshot().BlockCount > 0 && !Scripted->GetCombatComponent()->GetSnapshot().bAttacking)
				{
					const float Expected = Scripted->GetCombatComponent()->GetProfile()->GetAttacks()[0].Damage
						* Combat->GetProfile()->GetTuning().BlockDamageFraction;
					if (!FMath::IsNearlyEqual(PlayerHealthBefore - Knight->GetKnightAttributeSet()->GetHealth(), Expected))
					{
						Fail(TEXT("Mapped D-pad Left/held LeftShoulder did not telegraph/block the real scripted GAS hit."));
						break;
					}
					ReleaseInput();
					Enter(EKnightChallengePhase::ParryWait);
				}
				break;
			case EKnightChallengePhase::ParryWait:
				if (WorldElapsed() > 0.4 && !Combat->GetSnapshot().bBlocking)
				{
					PlaceNear(Scripted.Get(), 110.0);
					PlayerHealthBefore = Knight->GetKnightAttributeSet()->GetHealth();
					Press(EKnightChallengePhase::Parry, EKeys::F);
				}
				break;
			case EKnightChallengePhase::Parry:
				if (!bDefensePressed && IsScriptedTraceImminent(0.1f))
				{
					SetKey(EKeys::C, true);
					bDefensePressed = true;
				}
				if (State->GetSnapshot().ParryCount > 0 && !Scripted->GetCombatComponent()->GetSnapshot().bAttacking)
				{
					if (!bDefensePressed || !FMath::IsNearlyEqual(PlayerHealthBefore, Knight->GetKnightAttributeSet()->GetHealth())
						|| Knight->GetKnightAbilitySystem()->GetCueDiagnostics(KnightGameplayTags::GameplayCue_Parry).ExecutedCount < 1)
					{
						Fail(TEXT("Timed mapped C parry did not prevent health damage and execute the real cue."));
						break;
					}
					ReleaseInput();
					Enter(EKnightChallengePhase::DodgeWait, 12.0);
				}
				break;
			case EKnightChallengePhase::DodgeWait:
				if (WorldElapsed() > 0.4 && !Scripted->GetCombatComponent()->GetSnapshot().bStaggered
					&& !Combat->GetSnapshot().bParrying)
				{
					PlaceNear(Scripted.Get(), 110.0);
					PlayerHealthBefore = Knight->GetKnightAttributeSet()->GetHealth();
					bDefensePressed = false;
					Press(EKnightChallengePhase::Dodge, EKeys::F);
				}
				break;
			case EKnightChallengePhase::Dodge:
				if (!bDefensePressed && IsScriptedTraceImminent(0.11f))
				{
					SetKey(EKeys::LeftShift, true);
					bDefensePressed = true;
				}
				if (State->GetSnapshot().DodgeCount > 0 && !Combat->GetSnapshot().bDodging
					&& !Scripted->GetCombatComponent()->GetSnapshot().bAttacking)
				{
					if (!FMath::IsNearlyEqual(PlayerHealthBefore, Knight->GetKnightAttributeSet()->GetHealth()))
					{
						Fail(TEXT("A real scripted hit inside dodge invulnerability damaged the Knight."));
						break;
					}
					Press(EKnightChallengePhase::SwitchRune, EKeys::Three);
				}
				break;
			case EKnightChallengePhase::SwitchRune:
				if (Party->GetControlledSlot(Controller.Get()) == 2 && WorldElapsed() > 0.4)
				{
					PlaceNear(Rune.Get(), 250.0);
					TargetHealthBefore = Rune->GetKnightAttributeSet()->GetHealth();
					Press(EKnightChallengePhase::Rune, EKeys::G);
				}
				break;
			case EKnightChallengePhase::Rune:
				if (State->GetSnapshot().RuneHitCount > 0 && !bObservedRuneStatus)
				{
					if (!FMath::IsNearlyEqual(TargetHealthBefore - Rune->GetKnightAttributeSet()->GetHealth(),
						Combat->GetProfile()->GetTuning().RuneDamage)
						|| !Rune->GetKnightAbilitySystem()->HasMatchingGameplayTag(KnightGameplayTags::State_Combat_RuneSlowed)
						|| Rune->GetKnightAbilitySystem()->GetCueDiagnostics(KnightGameplayTags::GameplayCue_Rune).ExecutedCount < 1
						|| Knight->GetKnightAttributeSet()->GetRuneEnergy() >= Knight->GetKnightAttributeSet()->GetMaxRuneEnergy()
						|| Rune->GetCharacterMovement()->MaxWalkSpeed >= 150.0f)
					{
						Fail(TEXT("Mapped G did not apply real rune damage, slow effect, resource cost and cue."));
						break;
					}
					bObservedRuneStatus = true;
				}
				if (bObservedRuneStatus && WorldElapsed() > 0.4 && Combat->CanTransferControl()
					&& !Knight->GetKnightAbilitySystem()->HasMatchingGameplayTag(KnightGameplayTags::State_Combat_Attacking))
				{
					PlaceTraversal();
					Press(EKnightChallengePhase::Traversal, EKeys::V);
				}
				break;
			case EKnightChallengePhase::Traversal:
				ObserveTraversal();
				if (State->GetSnapshot().TraversalCount > 0)
				{
					if (!bObservedTraversal || MaximumTraversalZ < TraversalOrigin.Z + 85.0
						|| Knight->GetActorLocation().X < TraversalOrigin.X + 250.0
						|| Knight->FindComponentByClass<UKnightTraversalComponent>()->HasActiveTraversal()
						|| Knight->FindComponentByClass<UKnightCameraModeComponent>()->GetModeCount() != 0
						|| Controller->IsMoveInputIgnored() || Controller->IsLookInputIgnored()
						|| !State->GetSnapshot().bCompleted || !HUD->IsDisplayingChallengeCompletion())
					{
						Fail(TEXT("Traversal did not physically cross the hurdle with a real root arc/warp and clean completion HUD."));
						break;
					}
					const FGameplayAbilitySpec* Spec = Knight->GetKnightAbilitySystem()->FindAbilitySpecFromClass(
						UKnightTraversalAbility::StaticClass());
					if (Spec == nullptr || Spec->IsActive() || State->GetSnapshot().TraversalCount != 1
						|| Knight->GetKnightAbilitySystem()->HasMatchingGameplayTag(KnightGameplayTags::State_Combat_Traversing))
					{
						Fail(TEXT("Physical traversal completion did not end its real GAS ability and Traversing tag exactly once before Retry."));
						break;
					}
					Test.AddInfo(TEXT("Physical traversal completed exactly once with its GAS spec inactive and Traversing tag absent before R."));
					Test.AddInfo(FString::Printf(TEXT("Real challenge completed: MotionWarping moved %s -> %s; peak Z %.1f."),
						*TraversalOrigin.ToString(), *Knight->GetActorLocation().ToString(), MaximumTraversalZ));
					Press(EKnightChallengePhase::Retry, EKeys::R);
				}
				break;
			case EKnightChallengePhase::Retry:
				if (WorldElapsed() > 0.5 && ValidateReset())
				{
					PlaceTraversal();
					Press(EKnightChallengePhase::InterruptTraversal, EKeys::V);
				}
				break;
			case EKnightChallengePhase::InterruptTraversal:
				if (Knight->FindComponentByClass<UKnightTraversalComponent>()->HasActiveTraversal())
				{
					Press(EKnightChallengePhase::InterruptRetry, EKeys::R);
				}
				break;
			case EKnightChallengePhase::InterruptRetry:
				if (WorldElapsed() > 0.5 && ValidateReset())
				{
					const FKnightResult Damage = UKnightAbilitySystemComponent::ApplyDamage(Scripted.Get(), Knight, 200.0f);
					if (!Damage.IsSuccess() || !Knight->GetLifeComponent()->IsDowned())
					{
						Fail(TEXT("Genuine GAS death fault injection did not down the controlled Knight."));
						break;
					}
					Enter(EKnightChallengePhase::Death, 35.0);
				}
				break;
			case EKnightChallengePhase::Death:
				if (Knight->GetLifeComponent()->IsDead())
				{
					if (!State->GetSnapshot().bFailed || !HUD->IsDisplayingChallengeFailure())
					{
						Fail(TEXT("Actual controlled Knight bleedout death did not fail the challenge and update the HUD."));
						break;
					}
					Press(EKnightChallengePhase::DeadRetry, EKeys::R);
				}
				break;
			case EKnightChallengePhase::DeadRetry:
				if (WorldElapsed() > 0.5 && ValidateReset())
				{
					Test.AddInfo(TEXT("Mapped R reset completion, active traversal, and actual death without replacing canonical IDs or grants."));
					BeginCleanup();
				}
				break;
			case EKnightChallengePhase::Cleanup:
				Cleanup();
				break;
			default:
				break;
			}
			return Phase == EKnightChallengePhase::Complete;
		}

	private:
		void Enter(const EKnightChallengePhase Next, const double Timeout = 12.0)
		{
			Phase = Next;
			PhaseStarted = FPlatformTime::Seconds();
			PhaseWorldStarted = World.IsValid() ? World->GetTimeSeconds() : 0.0;
			PhaseTimeout = Timeout;
			PulseKey = FKey();
		}

		double WorldElapsed() const
		{
			return World.IsValid() ? World->GetTimeSeconds() - PhaseWorldStarted : 0.0;
		}

		void Press(const EKnightChallengePhase Next, const FKey Key)
		{
			ReleaseInput();
			Enter(Next);
			PulseKey = Key;
			SetKey(Key, true);
		}

		void Start()
		{
			if (GEditor == nullptr || GEditor->IsPlaySessionInProgress())
			{
				Fail(TEXT("Combat automation requires an idle Editor and preserves existing PIE sessions."));
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
			Settings->SetClientWindowSize(FIntPoint(1280, 960));
			FRequestPlaySessionParams Request;
			Request.EditorPlaySettings = Settings.Get();
			Request.bAllowOnlineSubsystem = false;
			GEditor->RequestPlaySession(Request);
			bOwnsSession = true;
			Enter(EKnightChallengePhase::Ready, 30.0);
		}

		void Ready()
		{
			UWorld* PIEWorld = GEditor != nullptr ? GEditor->PlayWorld.Get() : nullptr;
			if (PIEWorld == nullptr || !PIEWorld->HasBegunPlay())
			{
				return;
			}
			World = PIEWorld;
			Mode = PIEWorld->GetAuthGameMode<AKnightCombatChallengeGameMode>();
			Controller = Cast<AKnightPlayerController>(PIEWorld->GetFirstPlayerController());
			Party = Mode.IsValid() ? Mode->GetPartyState() : nullptr;
			State = Mode.IsValid() ? Mode->GetChallengeState() : nullptr;
			HUD = Controller.IsValid() ? Cast<AKnightCombatChallengeHUD>(Controller->GetHUD()) : nullptr;
			if (!Party.IsValid() || !State.IsValid() || !Controller.IsValid() || !HUD.IsValid()
				|| !Party->IsCanonicalRosterReady() || Controller->GetControlledKnight() == nullptr)
			{
				return;
			}
			if (!Mode->GetCombatInitializationResult().IsSuccess())
			{
				Fail(Mode->GetCombatInitializationResult().Message.ToString());
				return;
			}
			for (TActorIterator<AKnightCombatFixture> Iterator(PIEWorld); Iterator; ++Iterator)
			{
				if (!Iterator->GetInitializationResult().IsSuccess() || !Iterator->GetFixtureId().IsValid())
				{
					return;
				}
				switch (Iterator->GetFixtureKind())
				{
				case EKnightCombatFixtureKind::MeleeTarget:
					Melee = *Iterator;
					break;
				case EKnightCombatFixtureKind::ScriptedAttacker:
					Scripted = *Iterator;
					break;
				case EKnightCombatFixtureKind::RuneTarget:
					Rune = *Iterator;
					break;
				}
			}
			if (!Melee.IsValid() || !Scripted.IsValid() || !Rune.IsValid())
			{
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
			for (const FKnightRosterEntry& Entry : Party->GetRoster())
			{
				Knights.Add(Entry.Knight.Get());
				Ids.Add(Entry.KnightId);
				Definitions.Add(Entry.HeroDefinitionId);
			}
			if (!ValidateIdentityAndGrants())
			{
				return;
			}
			Test.AddInfo(TEXT("Real cumulative M3 PIE: three canonical Knights, real authored combat sets, Manny and native montages."));
			Press(EKnightChallengePhase::InvalidTraversal, EKeys::V);
		}

		bool ValidateIdentityAndGrants()
		{
			if (!World.IsValid() || !Party.IsValid() || !State.IsValid() || !Controller.IsValid() || Knights.Num() != 3)
			{
				Fail(TEXT("The challenge lost its live canonical party or feedback."));
				return false;
			}
			int32 Count = 0;
			for (TActorIterator<AKnightCharacter> Iterator(World.Get()); Iterator; ++Iterator)
			{
				++Count;
			}
			if (Count != 3 || Party->GetHumanCount() != 1)
			{
				Fail(TEXT("Combat fixtures broke the exactly-three-canonical-Knights invariant."));
				return false;
			}
			for (int32 Slot = 0; Slot < 3; ++Slot)
			{
				const FKnightRosterEntry* Entry = Party->FindSlot(Slot);
				if (Entry == nullptr || Entry->Knight != Knights[Slot].Get() || Entry->KnightId != Ids[Slot]
					|| Entry->HeroDefinitionId != Definitions[Slot]
					|| Entry->Knight->GetKnightAbilitySystem()->GetOwnedGrantCount() != 2
					|| Entry->Knight->GetKnightAbilitySystem()->GetActivatableAbilities().Num() != 11)
				{
					Fail(TEXT("M3 duplicated or lost canonical actors, IDs, HeroDefaults or authored CombatProfile grants."));
					return false;
				}
			}
			return true;
		}

		void PlaceNear(AKnightCombatFixture* Target, const double Distance)
		{
			ReleaseInput();
			AKnightCharacter* Knight = Controller->GetControlledKnight();
			Knight->GetCharacterMovement()->StopMovementImmediately();
			Knight->SetActorLocationAndRotation(Target->GetActorLocation() - FVector(Distance, 0.0, 0.0),
				FRotator::ZeroRotator, false, nullptr, ETeleportType::TeleportPhysics);
			Controller->SetControlRotation(FRotator::ZeroRotator);
		}

		void PlaceTraversal()
		{
			ReleaseInput();
			AKnightCharacter* Knight = Controller->GetControlledKnight();
			Knight->GetCharacterMovement()->StopMovementImmediately();
			Knight->SetActorLocationAndRotation(FVector(-1210.0, 1200.0, 100.0), FRotator::ZeroRotator,
				false, nullptr, ETeleportType::TeleportPhysics);
			Controller->SetControlRotation(FRotator::ZeroRotator);
			TraversalOrigin = Knight->GetActorLocation();
			MaximumTraversalZ = TraversalOrigin.Z;
		}

		bool IsScriptedTraceImminent(const float Lead) const
		{
			if (!Scripted->GetCombatComponent()->GetSnapshot().bAttacking)
			{
				return false;
			}
			const UAnimMontage* Montage = Scripted->GetKnightAbilitySystem()->GetCurrentMontage();
			const UAnimInstance* Animation = Scripted->GetMesh()->GetAnimInstance();
			if (Montage == nullptr || Animation == nullptr)
			{
				return false;
			}
			for (const FAnimNotifyEvent& Notify : Montage->Notifies)
			{
				if (Notify.NotifyStateClass != nullptr && Notify.NotifyStateClass->IsA<UKnightAnimNotifyState_TraceWindow>())
				{
					return Animation->Montage_GetPosition(Montage) >= Notify.GetTriggerTime() - Lead;
				}
			}
			return false;
		}

		void ObserveFinisher(AKnightCharacter* Knight, UKnightCombatComponent* Combat)
		{
			const UKnightCombatProfileDefinition* Profile = Combat->GetProfile();
			const UKnightFinisherComponent* Source = Knight->FindComponentByClass<UKnightFinisherComponent>();
			const UKnightFinisherComponent* Victim = Melee->FindComponentByClass<UKnightFinisherComponent>();
			UAnimMontage* SourceMontage = Knight->GetKnightAbilitySystem()->GetCurrentMontage();
			UAnimMontage* VictimMontage = Melee->GetKnightAbilitySystem()->GetCurrentMontage();
			const UAnimInstance* SourceAnimation = Knight->GetMesh()->GetAnimInstance();
			const UAnimInstance* VictimAnimation = Melee->GetMesh()->GetAnimInstance();
			if (Profile == nullptr || !Victim->IsReserved() || !Source->GetReservation().IsValid()
				|| Source->GetReservation() != Victim->GetReservation()
				|| SourceMontage != Profile->GetFinisherSourceMontage()
				|| VictimMontage != Profile->GetFinisherTargetMontage()
				|| SourceAnimation == nullptr || VictimAnimation == nullptr
				|| !SourceAnimation->Montage_IsPlaying(SourceMontage) || !VictimAnimation->Montage_IsPlaying(VictimMontage)
				|| Knight->FindComponentByClass<UKnightCameraModeComponent>()->GetModeCount() <= FinisherCameraModesBefore
				|| !Controller->IsMoveInputIgnored() || !Controller->IsLookInputIgnored())
			{
				return;
			}
			const float SourcePosition = SourceAnimation->Montage_GetPosition(SourceMontage);
			const float VictimPosition = VictimAnimation->Montage_GetPosition(VictimMontage);
			if (!FMath::IsFinite(SourcePosition) || !FMath::IsFinite(VictimPosition))
			{
				return;
			}
			if (!ObservedFinisherReservation.IsValid())
			{
				ObservedFinisherReservation = Source->GetReservation();
				FirstFinisherSourcePosition = SourcePosition;
				FirstFinisherVictimPosition = VictimPosition;
			}
			++PairedFinisherObservationCount;
			if (!bObservedFinisher && Source->GetReservation() == ObservedFinisherReservation
				&& SourcePosition > FirstFinisherSourcePosition + 0.01f
				&& VictimPosition > FirstFinisherVictimPosition + 0.01f)
			{
				// Observation is historical: natural montage completion must not erase the active-window proof.
				bObservedFinisher = true;
				Test.AddInfo(FString::Printf(TEXT("Observed paired finisher %s: source %s %.3f -> %.3f, victim %s "
					"%.3f -> %.3f, camera layers %d -> %d, both input locks active."),
					*ObservedFinisherReservation.ToString(), *SourceMontage->GetPathName(), FirstFinisherSourcePosition,
					SourcePosition, *VictimMontage->GetPathName(), FirstFinisherVictimPosition, VictimPosition,
					FinisherCameraModesBefore, Knight->FindComponentByClass<UKnightCameraModeComponent>()->GetModeCount()));
			}
		}

		void ObserveTraversal()
		{
			AKnightCharacter* Knight = Controller->GetControlledKnight();
			MaximumTraversalZ = FMath::Max(MaximumTraversalZ, Knight->GetActorLocation().Z);
			UKnightTraversalComponent* Traversal = Knight->FindComponentByClass<UKnightTraversalComponent>();
			if (Traversal->HasActiveTraversal())
			{
				ObservedTraversalLanding = Traversal->GetSnapshot().Landing.GetLocation();
				bObservedTraversalLanding = true;
				UMotionWarpingComponent* Warp = Knight->FindComponentByClass<UMotionWarpingComponent>();
				bObservedTraversal |= Warp != nullptr && Warp->FindWarpTarget(TEXT("KnightTraversal")) != nullptr
					&& Knight->GetCharacterMovement()->MovementMode == MOVE_Flying
					&& Knight->GetKnightAbilitySystem()->GetCurrentMontage() != nullptr;
			}
		}

		bool ValidateReset()
		{
			if (State->GetSnapshot().bCompleted || State->GetSnapshot().bFailed || State->GetSnapshot().LightHitCount != 0
				|| State->GetSnapshot().TraversalCount != 0 || HUD->IsDisplayingChallengeCompletion()
				|| HUD->IsDisplayingChallengeFailure())
			{
				Fail(TEXT("R failed to clear confirmed challenge feedback and HUD."));
				return false;
			}
			for (const TWeakObjectPtr<AKnightCharacter>& Knight : Knights)
			{
				const UKnightAttributeSet* Attributes = Knight->GetKnightAttributeSet();
				if (!Knight->GetLifeComponent()->IsAlive() || !FMath::IsNearlyEqual(Attributes->GetHealth(), Attributes->GetMaxHealth())
					|| !FMath::IsNearlyEqual(Attributes->GetStamina(), Attributes->GetMaxStamina())
					|| !FMath::IsNearlyEqual(Attributes->GetRuneEnergy(), Attributes->GetMaxRuneEnergy())
					|| Knight->FindComponentByClass<UKnightCameraModeComponent>()->GetModeCount() != 0
					|| Knight->FindComponentByClass<UKnightFinisherComponent>()->IsReserved()
					|| Knight->FindComponentByClass<UKnightTraversalComponent>()->HasActiveTraversal()
					|| !Knight->FindComponentByClass<UKnightCombatComponent>()->CanTransferControl())
				{
					Fail(TEXT("R left stale combat/resource/camera/input/reservation state on a canonical Knight."));
					return false;
				}
			}
			for (const TWeakObjectPtr<AKnightCombatFixture>& Fixture : { Melee, Scripted, Rune })
			{
				if (!Fixture->GetLifeComponent()->IsAlive() || Fixture->IsTelegraphing()
					|| !FMath::IsNearlyEqual(Fixture->GetKnightAttributeSet()->GetHealth(),
						Fixture->GetKnightAttributeSet()->GetMaxHealth())
					|| Fixture->FindComponentByClass<UKnightFinisherComponent>()->IsReserved())
				{
					Fail(TEXT("R did not restore actual GAS fixtures and clear their timers/reservations."));
					return false;
				}
			}
			return ValidateIdentityAndGrants();
		}

		void SetKey(const FKey Key, const bool bDown)
		{
			if (!Controller.IsValid() || DownKeys.Contains(Key) == bDown)
			{
				return;
			}
			Controller->InputKey(FInputKeyEventArgs::CreateSimulated(Key,
				bDown ? IE_Pressed : IE_Released, bDown ? 1.0f : 0.0f));
			if (bDown)
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
			for (const FKey Key : DownKeys.Array())
			{
				SetKey(Key, false);
			}
			DownKeys.Reset();
		}

		void Fail(const FString& Message)
		{
			if (State.IsValid())
			{
				const FKnightCombatChallengeSnapshot& Snapshot = State->GetSnapshot();
				Test.AddInfo(FString::Printf(TEXT("Failure phase %d: light %d heavy %d combo %d block %d parry %d dodge %d "
					"stagger %d rune %d finisher %d traversal %d lock %d completed %d failed %d; last command %s."),
					static_cast<int32>(Phase), Snapshot.LightHitCount, Snapshot.HeavyHitCount, Snapshot.ComboCount,
					Snapshot.BlockCount, Snapshot.ParryCount, Snapshot.DodgeCount, Snapshot.StaggerCount, Snapshot.RuneHitCount,
					Snapshot.FinisherCount, Snapshot.TraversalCount, Snapshot.bTargetLocked, Snapshot.bCompleted,
					Snapshot.bFailed, Controller.IsValid() ? *Controller->GetLastCommandResult().Message.ToString() : TEXT("none")));
			}
			AKnightCharacter* Knight = Controller.IsValid() ? Controller->GetControlledKnight() : nullptr;
			if (Knight != nullptr && Melee.IsValid())
			{
				const UKnightFinisherComponent* Source = Knight->FindComponentByClass<UKnightFinisherComponent>();
				const UKnightFinisherComponent* Victim = Melee->FindComponentByClass<UKnightFinisherComponent>();
				const UKnightCombatComponent* Combat = Knight->FindComponentByClass<UKnightCombatComponent>();
				UAnimMontage* SourceMontage = Knight->GetKnightAbilitySystem()->GetCurrentMontage();
				UAnimMontage* VictimMontage = Melee->GetKnightAbilitySystem()->GetCurrentMontage();
				const UAnimInstance* SourceAnimation = Knight->GetMesh()->GetAnimInstance();
				const UAnimInstance* VictimAnimation = Melee->GetMesh()->GetAnimInstance();
				Test.AddInfo(FString::Printf(TEXT("Finisher diagnostics: observed=%d pairedSamples=%d observedReservation=%s "
					"sourceActive=%d sourceReservation=%s victimReservation=%s victimLife=%d victimHealth=%.3f "
					"cameraLayers=%d before=%d moveIgnored=%d lookIgnored=%d lockedTarget=%s "
					"sourceMontage=%s position=%.3f victimMontage=%s position=%.3f."),
					bObservedFinisher, PairedFinisherObservationCount, *ObservedFinisherReservation.ToString(),
					Source->HasActiveFinisher(), *Source->GetReservation().ToString(), *Victim->GetReservation().ToString(),
					static_cast<int32>(Melee->GetLifeComponent()->GetLifeState()), Melee->GetKnightAttributeSet()->GetHealth(),
					Knight->FindComponentByClass<UKnightCameraModeComponent>()->GetModeCount(), FinisherCameraModesBefore,
					Controller->IsMoveInputIgnored(), Controller->IsLookInputIgnored(), *GetNameSafe(Combat->GetLockedTarget()),
					*GetPathNameSafe(SourceMontage), SourceAnimation != nullptr && SourceMontage != nullptr
						? SourceAnimation->Montage_GetPosition(SourceMontage) : -1.0f,
					*GetPathNameSafe(VictimMontage), VictimAnimation != nullptr && VictimMontage != nullptr
						? VictimAnimation->Montage_GetPosition(VictimMontage) : -1.0f));
			}
			if (Knight != nullptr && State.IsValid() && (Phase == EKnightChallengePhase::Traversal
				|| Phase == EKnightChallengePhase::InterruptTraversal || Phase == EKnightChallengePhase::InterruptRetry))
			{
				const UKnightTraversalComponent* Traversal = Knight->FindComponentByClass<UKnightTraversalComponent>();
				const FGameplayAbilitySpec* Spec = Knight->GetKnightAbilitySystem()->FindAbilitySpecFromClass(
					UKnightTraversalAbility::StaticClass());
				const FVector Position = Knight->GetActorLocation();
				Test.AddInfo(FString::Printf(TEXT("Traversal diagnostics: observed=%d origin=%s position=%s delta=%s "
					"hasLanding=%d landing=%s landingDistance=%.3f peakZ=%.3f requiredPeakZ=%.3f "
					"activeTraversal=%d specPresent=%d specActive=%d traversingTag=%d movementMode=%d "
					"cameraLayers=%d moveIgnored=%d lookIgnored=%d confirmedCount=%d completed=%d failed=%d "
					"hudComplete=%d hudFailed=%d."), bObservedTraversal, *TraversalOrigin.ToString(), *Position.ToString(),
					*(Position - TraversalOrigin).ToString(), bObservedTraversalLanding, *ObservedTraversalLanding.ToString(),
					bObservedTraversalLanding ? FVector::Dist(Position, ObservedTraversalLanding) : -1.0,
					MaximumTraversalZ, TraversalOrigin.Z + 85.0, Traversal->HasActiveTraversal(), Spec != nullptr,
					Spec != nullptr && Spec->IsActive(),
					Knight->GetKnightAbilitySystem()->HasMatchingGameplayTag(KnightGameplayTags::State_Combat_Traversing),
					static_cast<int32>(Knight->GetCharacterMovement()->MovementMode),
					Knight->FindComponentByClass<UKnightCameraModeComponent>()->GetModeCount(),
					Controller->IsMoveInputIgnored(), Controller->IsLookInputIgnored(), State->GetSnapshot().TraversalCount,
					State->GetSnapshot().bCompleted, State->GetSnapshot().bFailed,
					HUD.IsValid() && HUD->IsDisplayingChallengeCompletion(), HUD.IsValid() && HUD->IsDisplayingChallengeFailure()));
			}
			Test.AddError(Message);
			BeginCleanup();
		}

		void BeginCleanup()
		{
			if (Phase == EKnightChallengePhase::Cleanup || Phase == EKnightChallengePhase::Complete)
			{
				return;
			}
			ReleaseInput();
			Enter(EKnightChallengePhase::Cleanup, 15.0);
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
			if (Settings.IsValid())
			{
				ULevelEditorPlaySettings* Defaults = GetMutableDefault<ULevelEditorPlaySettings>();
				Defaults->MultipleInstancePositions = OriginalPositions;
				Defaults->LastSize = OriginalLastSize;
				Defaults->NewWindowPosition = OriginalPosition;
				Defaults->NewWindowWidth = OriginalWidth;
				Defaults->NewWindowHeight = OriginalHeight;
				Defaults->SaveConfig();
			}
		}

		void Cleanup()
		{
			if (bOwnsSession && GEditor != nullptr && GEditor->IsPlaySessionInProgress())
			{
				if (FPlatformTime::Seconds() - PhaseStarted < 12.0)
				{
					return;
				}
				Test.AddError(TEXT("Combat PIE cleanup exceeded its bound."));
				if (GEditor->IsPlayingSessionInEditor())
				{
					GEditor->EndPlayMap();
				}
			}
			RestoreSettings();
			Settings.Reset();
			bOwnsSession = false;
			Enter(EKnightChallengePhase::Complete);
		}

		FAutomationTestBase& Test;
		EKnightChallengePhase Phase = EKnightChallengePhase::Navigation;
		double RunStarted = 0.0;
		double PhaseStarted = 0.0;
		double PhaseWorldStarted = 0.0;
		double PhaseTimeout = 30.0;
		double ReadyWorldTime = -1.0;
		double BufferPressedAt = 0.0;
		double FinisherCompletedAt = -1.0;
		double MaximumTraversalZ = 0.0;
		float TargetHealthBefore = 0.0f;
		float PlayerHealthBefore = 0.0f;
		float FirstFinisherSourcePosition = 0.0f;
		float FirstFinisherVictimPosition = 0.0f;
		int32 FinisherCameraModesBefore = 0;
		int32 PairedFinisherObservationCount = 0;
		FGuid ObservedFinisherReservation;
		bool bOwnsSession = false;
		bool bBuffered = false;
		bool bDefensePressed = false;
		bool bObservedFinisher = false;
		bool bObservedTraversal = false;
		bool bObservedTraversalLanding = false;
		bool bObservedRuneStatus = false;
		FVector TraversalOrigin = FVector::ZeroVector;
		FVector ObservedTraversalLanding = FVector::ZeroVector;
		FKey PulseKey;
		TSet<FKey> DownKeys;
		TArray<TWeakObjectPtr<AKnightCharacter>> Knights;
		TArray<FKnightEntityId> Ids;
		TArray<FPrimaryAssetId> Definitions;
		TWeakObjectPtr<UWorld> World;
		TWeakObjectPtr<AKnightCombatChallengeGameMode> Mode;
		TWeakObjectPtr<AKnightPlayerController> Controller;
		TWeakObjectPtr<AKnightPartyState> Party;
		TWeakObjectPtr<AKnightCombatChallengeState> State;
		TWeakObjectPtr<AKnightCombatChallengeHUD> HUD;
		TWeakObjectPtr<AKnightCombatFixture> Melee;
		TWeakObjectPtr<AKnightCombatFixture> Scripted;
		TWeakObjectPtr<AKnightCombatFixture> Rune;
		TStrongObjectPtr<ULevelEditorPlaySettings> Settings;
		TArray<FIntPoint> OriginalPositions;
		FIntPoint OriginalLastSize = FIntPoint::ZeroValue;
		FIntPoint OriginalPosition = FIntPoint::ZeroValue;
		int32 OriginalWidth = 0;
		int32 OriginalHeight = 0;
		FWaitForEditorNavMeshBuildCommand NavigationWait;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKnightCombatChallengePlaythroughTest, "Knight.Combat.ChallengePlaythrough",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FKnightCombatChallengePlaythroughTest::RunTest(const FString& Parameters)
{
	if (GEditor == nullptr || GEditor->IsPlaySessionInProgress()
		|| !FPackageName::DoesPackageExist(KnightCombatChallengeTest::MapPath)
		|| !FEditorFileUtils::LoadMap(KnightCombatChallengeTest::MapPath, false, false))
	{
		AddError(TEXT("Run KnightBootstrap -Milestone=3 on the integrated build and use an idle Editor."));
		return false;
	}
	AddInfo(TEXT("M3 gate uses actual mapped combat keys; explicit actor placement is bounded fixture setup only."));
	AddCommand(new KnightCombatChallengeTest::FKnightChallengeScenario(*this));
	return true;
}

#endif
