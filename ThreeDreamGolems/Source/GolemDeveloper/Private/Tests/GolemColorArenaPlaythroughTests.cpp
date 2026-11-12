#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Attributes/GolemAttributeSet.h"
#include "Character/GolemCharacter.h"
#include "ColorArena/GolemColorArenaGameMode.h"
#include "ColorArena/GolemColorArenaHUD.h"
#include "ColorAssist/GolemColorAssistComponent.h"
#include "Components/GolemAbilitySystemComponent.h"
#include "Components/GolemCombatComponent.h"
#include "Components/StateTreeAIComponent.h"
#include "Controllers/GolemArenaCompanionController.h"
#include "Party/GolemPartyState.h"
#include "PartyArena/GolemArenaPlayerController.h"
#include "Tags/GolemGameplayTags.h"
#include "Types/GolemElement.h"
#include "Voids/GolemColorVoidComponent.h"
#include "Voids/GolemVoidHostComponent.h"
#include "Xenon/GolemXenon.h"
#include "Xenon/GolemXenonBrainComponent.h"

#include "Editor.h"
#include "Engine/World.h"
#include "FileHelpers.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/PlatformTime.h"
#include "InputCoreTypes.h"
#include "InputKeyEventArgs.h"
#include "Misc/PackageName.h"
#include "PlayInEditorDataTypes.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Tests/AutomationEditorCommon.h"
#include "UObject/StrongObjectPtr.h"

DEFINE_LOG_CATEGORY_STATIC(LogGolemColorPlaythrough, Log, All);

namespace GolemColorArenaPlaythrough
{
	const TCHAR* MapPath = TEXT("/Game/Golem/Maps/Playables/L_M0_ColorArena");
	constexpr uint8 Earth = GolemElements::Mask(EGolemElement::Earth);
	constexpr uint8 Sky = GolemElements::Mask(EGolemElement::Sky);
	constexpr uint8 Sea = GolemElements::Mask(EGolemElement::Sea);

	enum class EPhase : uint8
	{
		Navigation,
		Start,
		Ready,
		PlayerSwing,
		CompanionFollowUp,
		NextVoid,
		NoInitiative,
		SignalResponse,
		XenonAttack,
		Guard,
		Barrier,
		Backflip,
		Cleanup,
		Complete
	};

	/**
	 * One real session on the M0 map, on the host: the player's own heavy swing has to reach the
	 * void, and the two AI companions have to follow the three rules of DEV_SPEC section 12. The
	 * colour each void asks for and where the Golems stand are set by the test; everything that
	 * happens after that is the game.
	 */
	class FScenario final : public IAutomationLatentCommand
	{
	public:
		explicit FScenario(FAutomationTestBase& InTest)
			: Test(InTest), RunStarted(FPlatformTime::Seconds()), PhaseStarted(RunStarted)
		{
		}

		virtual ~FScenario() override
		{
			ReleaseKey();
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
			if (Phase == EPhase::Complete)
			{
				return true;
			}
			if (Phase != EPhase::Cleanup && (FPlatformTime::Seconds() - RunStarted > 180.0
				|| FPlatformTime::Seconds() - PhaseStarted > PhaseTimeout))
			{
				Fail(FString::Printf(TEXT("Colour arena phase %d timed out."), static_cast<int32>(Phase)));
			}
			if (Phase != EPhase::Cleanup && Test.HasAnyErrors())
			{
				BeginCleanup();
			}
			if (World.IsValid() && bKeyDown && !bHoldKey && World->GetTimeSeconds() - KeyPressedAt > 0.08)
			{
				ReleaseKey();
			}
			switch (Phase)
			{
			case EPhase::Navigation:
				if (NavigationWait.Update())
				{
					Enter(EPhase::Start, 30.0);
				}
				break;
			case EPhase::Start:
				Start();
				break;
			case EPhase::Ready:
				Ready();
				break;
			case EPhase::PlayerSwing:
				PlayerSwing();
				break;
			case EPhase::CompanionFollowUp:
				CompanionFollowUp();
				break;
			case EPhase::NextVoid:
				NextVoid();
				break;
			case EPhase::NoInitiative:
				NoInitiative();
				break;
			case EPhase::SignalResponse:
				SignalResponse();
				break;
			case EPhase::XenonAttack:
				XenonAttack();
				break;
			case EPhase::Guard:
				Guard();
				break;
			case EPhase::Barrier:
				Barrier();
				break;
			case EPhase::Backflip:
				Backflip();
				break;
			case EPhase::Cleanup:
				Cleanup();
				break;
			default:
				break;
			}
			return Phase == EPhase::Complete;
		}

	private:
		void Enter(const EPhase Next, const double Timeout = 15.0)
		{
			Phase = Next;
			PhaseStarted = FPlatformTime::Seconds();
			PhaseWorldStarted = World.IsValid() ? World->GetTimeSeconds() : 0.0;
			PhaseTimeout = Timeout;
			UE_LOG(LogGolemColorPlaythrough, Display, TEXT("Colour arena phase %d started."), static_cast<int32>(Phase));
		}

		double WorldElapsed() const
		{
			return World.IsValid() ? World->GetTimeSeconds() - PhaseWorldStarted : 0.0;
		}

		void PressKey(const FKey& Key)
		{
			ReleaseKey();
			HeldKey = Key;
			bKeyDown = true;
			KeyPressedAt = World->GetTimeSeconds();
			Controller->InputKey(FInputKeyEventArgs::CreateSimulated(Key, IE_Pressed, 1.0f));
		}

		void ReleaseKey()
		{
			if (bKeyDown && Controller.IsValid())
			{
				Controller->InputKey(FInputKeyEventArgs::CreateSimulated(HeldKey, IE_Released, 0.0f));
			}
			bKeyDown = false;
		}

		void Start()
		{
			if (GEditor == nullptr || GEditor->IsPlaySessionInProgress())
			{
				Fail(TEXT("Colour arena automation requires an idle Editor."));
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
			Enter(EPhase::Ready, 35.0);
		}

		AGolemCharacter* FindGolem(const TCHAR* HeroName) const
		{
			for (const FGolemRosterEntry& Entry : Party->GetRoster())
			{
				if (Entry.Golem != nullptr && Entry.HeroDefinitionId.PrimaryAssetName == FName(HeroName))
				{
					return Entry.Golem.Get();
				}
			}
			return nullptr;
		}

		void Place(AGolemCharacter* Golem, const FVector& Location, const float Yaw)
		{
			Golem->GetCharacterMovement()->StopMovementImmediately();
			Golem->SetActorLocationAndRotation(Location, FRotator(0.0, Yaw, 0.0), false, nullptr, ETeleportType::TeleportPhysics);
			if (Golem->GetController() != nullptr)
			{
				Golem->GetController()->SetControlRotation(FRotator(0.0, Yaw, 0.0));
				Golem->GetController()->StopMovement();
			}
		}

		void Watch(UGolemColorVoidComponent* Void)
		{
			Void->OnVoidChanged.AddLambda([this](UGolemColorVoidComponent* Changed)
			{
				bSawDulled |= Changed->GetVoidState() == EGolemVoidState::Dulled;
			});
			Void->OnVoidFilled.AddLambda([this](UGolemColorVoidComponent*, AActor* By)
			{
				LastFilledBy = By;
			});
		}

		void Ready()
		{
			UWorld* PIEWorld = GEditor != nullptr ? GEditor->PlayWorld.Get() : nullptr;
			if (PIEWorld == nullptr || !PIEWorld->HasBegunPlay())
			{
				return;
			}
			World = PIEWorld;
			Mode = PIEWorld->GetAuthGameMode<AGolemColorArenaGameMode>();
			Controller = Cast<AGolemArenaPlayerController>(PIEWorld->GetFirstPlayerController());
			Party = Mode.IsValid() ? Mode->GetPartyState() : nullptr;
			if (!Mode.IsValid() || !Party.IsValid() || !Controller.IsValid() || !Party->IsCanonicalRosterReady()
				|| Controller->GetControlledGolem() == nullptr || Mode->GetXenons().Num() != 2)
			{
				return;
			}
			if (!bXenonsReset)
			{
				// Xenons start hunting as soon as the map opens. The colour phases need them at their spawn
				// points, standing still; their own behaviour is checked in the later phases.
				if (!Mode->RespawnXenons().IsSuccess())
				{
					Fail(TEXT("The arena could not place fresh Xenons."));
					return;
				}
				for (AGolemXenon* Candidate : Mode->GetXenons())
				{
					if (Candidate != nullptr)
					{
						Candidate->GetBrain()->SetBrainEnabled(false);
					}
				}
				bXenonsReset = true;
			}
			if (ReadyWorldTime < 0.0)
			{
				ReadyWorldTime = World->GetTimeSeconds();
			}
			if (World->GetTimeSeconds() - ReadyWorldTime < 0.75)
			{
				return;
			}
			Mars = FindGolem(TEXT("Mars"));
			Jupiter = FindGolem(TEXT("Jupiter"));
			Neptune = FindGolem(TEXT("Neptune"));
			if (!Mars.IsValid() || !Jupiter.IsValid() || !Neptune.IsValid() || Controller->GetControlledGolem() != Mars.Get())
			{
				Fail(TEXT("The colour arena did not start with the player on Mars and companions on Jupiter and Neptune."));
				return;
			}
			// DEV_SPEC section 4: each Golem starts from its own stats, and Agility scales move speed.
			Test.TestEqual(TEXT("Mars health"), Mars->GetGolemAbilitySystem()->GetGolemAttributes()->GetMaxHealth(), 140.0f);
			Test.TestEqual(TEXT("Jupiter power"), Jupiter->GetGolemAbilitySystem()->GetGolemAttributes()->GetPower(), 10.0f);
			Test.TestEqual(TEXT("Neptune colour hold"), Neptune->GetGolemAbilitySystem()->GetGolemAttributes()->GetColorHold(), 0.5f);
			Test.TestTrue(TEXT("Jupiter (Agility 110) walks faster than Mars (Agility 90)"),
				Jupiter->GetCharacterMovement()->MaxWalkSpeed > Mars->GetCharacterMovement()->MaxWalkSpeed * 1.2f);
			Test.TestTrue(TEXT("The colour HUD is in use"), Cast<AGolemColorArenaHUD>(Controller->GetHUD()) != nullptr);
			for (AGolemCharacter* Companion : { Jupiter.Get(), Neptune.Get() })
			{
				const AGolemArenaCompanionController* AI = Cast<AGolemArenaCompanionController>(Companion->GetController());
				if (AI == nullptr || !AI->GetArenaInitializationResult().IsSuccess() || AI->GetColorAssist() == nullptr)
				{
					Fail(TEXT("A companion is not driven by the arena companion controller with colour assist."));
					return;
				}
			}
			for (AGolemXenon* Candidate : Mode->GetXenons())
			{
				if (Candidate != nullptr && Candidate->GetDefinitionId().PrimaryAssetName == FName(TEXT("Grunt")))
				{
					Xenon = Candidate;
				}
				else if (Candidate != nullptr)
				{
					Shield = Candidate;
				}
			}
			UGolemColorVoidComponent* Void = OpenVoid();
			if (!Xenon.IsValid() || Void == nullptr)
			{
				Fail(TEXT("The Grunt did not start with exactly one open void."));
				return;
			}
			// An orange void: Mars opens it with a real swing, Jupiter has to finish it.
			Void->ConfigureVoid(Earth | Sky, Xenon->GetVoidHost()->GetTuning().Void, false);
			Watch(Void);
			ActiveVoid = Void;
			const FVector VoidLocation = Void->GetComponentLocation();
			const FVector Outward = (VoidLocation - Xenon->GetActorLocation()).GetSafeNormal2D();
			const FVector Stand = VoidLocation + Outward * (Void->GetScaledSphereRadius() + 60.0f);
			const FVector Right = FVector::CrossProduct(FVector::UpVector, Outward);
			Place(Mars.Get(), FVector(Stand.X, Stand.Y, 100.0), (-Outward).Rotation().Yaw);
			Place(Jupiter.Get(), FVector(Stand.X, Stand.Y, 100.0) + Right * 250.0 + Outward * 450.0, (-Outward).Rotation().Yaw);
			Place(Neptune.Get(), FVector(Stand.X, Stand.Y, 100.0) - Right * 250.0 + Outward * 450.0, (-Outward).Rotation().Yaw);
			Controller->SetControlRotation((-Outward).Rotation());
			Enter(EPhase::PlayerSwing, 8.0);
		}

		UGolemColorVoidComponent* OpenVoid() const
		{
			if (!Xenon.IsValid())
			{
				return nullptr;
			}
			const TArray<UGolemColorVoidComponent*> Open = Xenon->GetVoidHost()->GetOpenVoids();
			return Open.Num() == 1 ? Open[0] : nullptr;
		}

		void PlayerSwing()
		{
			if (!bPressedHeavy)
			{
				// Let the teleported Golem settle on the floor before the swing.
				if (WorldElapsed() < 0.4)
				{
					return;
				}
				bPressedHeavy = true;
				HeavyPressedAt = World->GetTimeSeconds();
				PressKey(EKeys::RightMouseButton);
				return;
			}
			// Phase 2 is the active part of the swing: the wind-up before it is the designed charge time.
			if (SwingStartedAt < 0.0 && Mars->GetCombatComponent()->GetSnapshot().AttackPhase == 2)
			{
				SwingStartedAt = World->GetTimeSeconds();
			}
			if (ActiveVoid.IsValid() && ActiveVoid->GetVoidState() == EGolemVoidState::Primed)
			{
				const double Charge = SwingStartedAt - HeavyPressedAt;
				const double Contact = World->GetTimeSeconds() - HeavyPressedAt;
				Test.TestEqual(TEXT("Mars's real heavy swing placed Earth on the void"), ActiveVoid->GetAppliedMask(), Earth);
				Test.TestTrue(FString::Printf(TEXT("Mars's heavy attack winds up for about 1.0 s (measured %.2f s)"), Charge),
					SwingStartedAt > 0.0 && Charge > 0.9 && Charge < 1.2);
				Test.AddInfo(FString::Printf(TEXT("The hammer touched the void %.2f s after the press."), Contact));
				Test.TestTrue(TEXT("The swing itself is not slowed down"), Contact < Charge + 0.9);
				Test.TestEqual(TEXT("A colour hit deals no direct damage"),
					Xenon->GetGolemAttributeSet()->GetHealth(), Xenon->GetGolemAttributeSet()->GetMaxHealth());
				PrimedAt = World->GetTimeSeconds();
				Enter(EPhase::CompanionFollowUp, 8.0);
			}
			else if (World->GetTimeSeconds() - HeavyPressedAt > 4.0)
			{
				Fail(FString::Printf(TEXT("Mars's heavy swing did not reach the void (state %d, health %.1f)."),
					ActiveVoid.IsValid() ? static_cast<int32>(ActiveVoid->GetVoidState()) : -1,
					Xenon->GetGolemAttributeSet()->GetHealth()));
			}
		}

		void CompanionFollowUp()
		{
			if (Xenon->GetVoidHost()->GetFilledCount() == 1)
			{
				const double Delay = World->GetTimeSeconds() - PrimedAt;
				Test.TestTrue(TEXT("Jupiter, not the player, added the missing colour"), LastFilledBy.Get() == Jupiter.Get());
				Test.TestTrue(FString::Printf(TEXT("The follow-up waits at least 1.5 s (measured %.2f s)"), Delay), Delay >= 1.5);
				Test.TestTrue(TEXT("The fill arrived inside the 4 s window"), Delay < 4.0);
				Test.TestEqual(TEXT("One fill of three removes a third of the Grunt's health"),
					Xenon->GetGolemAttributeSet()->GetHealth(), 200.0f);
				Test.TestTrue(TEXT("The Xenon is staggered by the fill"), Xenon->GetVoidHost()->IsStaggered());
				Enter(EPhase::NextVoid, 8.0);
			}
			else if (World->GetTimeSeconds() - PrimedAt > 4.3)
			{
				Fail(TEXT("No companion finished the Primed void inside its window."));
			}
		}

		void NextVoid()
		{
			UGolemColorVoidComponent* Void = OpenVoid();
			if (Void == nullptr || Xenon->GetVoidHost()->IsStaggered())
			{
				return;
			}
			Test.TestTrue(TEXT("The next void appears somewhere else"), Void != ActiveVoid.Get());
			// A blue void: only Neptune can fill it, and Neptune is a companion.
			Void->ConfigureVoid(Sea, Xenon->GetVoidHost()->GetTuning().Void, false);
			Watch(Void);
			ActiveVoid = Void;
			Enter(EPhase::NoInitiative, 10.0);
		}

		void NoInitiative()
		{
			if (WorldElapsed() < 3.0)
			{
				return;
			}
			Test.TestEqual(TEXT("Without a signal no companion opens a void, not even one of its own colour"),
				ActiveVoid->GetVoidState(), EGolemVoidState::Open);
			const FGolemResult Signal = Controller->ApplySignal(ActiveVoid.Get());
			Test.TestTrue(TEXT("The player signals the blue void"), Signal.IsSuccess() && ActiveVoid->IsSignaled());
			Test.TestEqual(TEXT("The marker shows the element still needed"),
				AGolemColorArenaHUD::DescribeVoid(ActiveVoid.Get(), ActiveVoid->GetHostTimeSeconds()), FString(TEXT("~")));
			SignalledAt = World->GetTimeSeconds();
			Enter(EPhase::SignalResponse, 12.0);
		}

		void SignalResponse()
		{
			if (Xenon->GetVoidHost()->GetFilledCount() == 2)
			{
				const double Delay = World->GetTimeSeconds() - SignalledAt;
				Test.TestTrue(TEXT("Neptune answered the signal"), LastFilledBy.Get() == Neptune.Get());
				Test.TestTrue(FString::Printf(TEXT("The answer comes no sooner than 0.5 s plus the 0.8 s charge (measured %.2f s)"), Delay),
					Delay >= 1.3);
				Test.TestFalse(TEXT("No void was ever locked by a wrong colour"), bSawDulled);
				if (!Shield.IsValid())
				{
					Fail(TEXT("The arena has no Shield Xenon."));
					return;
				}
				// Last: the Shield's own attack. Mars stands in front of it, inside the range where it starts.
				const FVector Forward = Shield->GetActorForwardVector();
				const FVector Spot = Shield->GetActorLocation() + Forward * 230.0;
				Place(Mars.Get(), FVector(Spot.X, Spot.Y, 100.0), (-Forward).Rotation().Yaw);
				MarsHealthBefore = Mars->GetGolemAbilitySystem()->GetGolemAttributes()->GetHealth();
				Shield->GetBrain()->SetBrainEnabled(true);
				Enter(EPhase::XenonAttack, 10.0);
			}
			else if (World->GetTimeSeconds() - SignalledAt > 9.0)
			{
				Fail(TEXT("Neptune did not fill the signalled void."));
			}
		}

		void XenonAttack()
		{
			const UGolemXenonBrainComponent* Brain = Shield->GetBrain();
			if (TelegraphAt < 0.0)
			{
				if (Brain->GetAction() == EGolemXenonAction::Telegraph)
				{
					TelegraphAt = World->GetTimeSeconds();
					Test.TestTrue(TEXT("Mars stands inside the zone the Shield shows"), Brain->IsThreatening(Mars->GetActorLocation()));
					Test.TestEqual(TEXT("Nothing has hit Mars yet"),
						Mars->GetGolemAbilitySystem()->GetGolemAttributes()->GetHealth(), MarsHealthBefore);
					MarsHitFrom = Mars->GetActorLocation();
				}
				return;
			}
			const float Health = Mars->GetGolemAbilitySystem()->GetGolemAttributes()->GetHealth();
			if (StruckAt < 0.0 && Brain->GetStrikeCount() > 0)
			{
				StruckAt = World->GetTimeSeconds();
				const double Warning = StruckAt - TelegraphAt;
				Test.TestTrue(FString::Printf(TEXT("The attack is shown for at least 0.6 s before it lands (measured %.2f s)"), Warning),
					Warning >= 0.6 && Warning < 1.2);
				Test.TestEqual(TEXT("The Shield's hit deals its 25 damage"), Health, MarsHealthBefore - 25.0f);
			}
			if (StruckAt >= 0.0 && World->GetTimeSeconds() - StruckAt > 0.6)
			{
				Test.TestTrue(TEXT("The shove throws Mars back"),
					FVector::Dist2D(Mars->GetActorLocation(), Shield->GetActorLocation())
						> FVector::Dist2D(MarsHitFrom, Shield->GetActorLocation()) + 100.0);
				// Next: the same shove against a Warrior's Guard. Mars steps back in front of the Shield.
				const FVector Forward = Shield->GetActorForwardVector();
				const FVector Spot = Shield->GetActorLocation() + Forward * 230.0;
				Place(Mars.Get(), FVector(Spot.X, Spot.Y, 100.0), (-Forward).Rotation().Yaw);
				MarsHealthBefore = Mars->GetGolemAbilitySystem()->GetGolemAttributes()->GetHealth();
				StrikesBeforeGuard = Shield->GetBrain()->GetStrikeCount();
				Enter(EPhase::Guard, 15.0);
			}
		}

		void Guard()
		{
			const UGolemXenonBrainComponent* Brain = Shield->GetBrain();
			const UGolemCombatComponent* Combat = Mars->GetCombatComponent();
			if (!bGuardPressed)
			{
				// The player raises the Guard when the Shield shows its next shove, and keeps the button down.
				if (Brain->GetAction() == EGolemXenonAction::Telegraph && Brain->IsThreatening(Mars->GetActorLocation()))
				{
					bGuardPressed = true;
					bHoldKey = true;
					PressKey(EKeys::Q);
				}
				return;
			}
			if (GuardStruckAt < 0.0)
			{
				if (Brain->GetStrikeCount() > StrikesBeforeGuard)
				{
					GuardStruckAt = World->GetTimeSeconds();
					Test.TestTrue(TEXT("Mars was guarding when the shove landed"), Combat->GetSnapshot().bBlocking);
					Test.TestEqual(TEXT("A Guard lets 20% of a frontal hit through: 5 of 25"),
						Mars->GetGolemAbilitySystem()->GetGolemAttributes()->GetHealth(), MarsHealthBefore - 5.0f);
					Shield->GetBrain()->SetBrainEnabled(false);
				}
				return;
			}
			// The button stays down: the Guard has to end by itself after its 3 seconds.
			if (Combat->GetSnapshot().bBlocking)
			{
				return;
			}
			const double Held = World->GetTimeSeconds() - KeyPressedAt;
			Test.TestTrue(FString::Printf(TEXT("A held Guard ends after 3 s (measured %.2f s)"), Held), Held > 2.8 && Held < 3.4);
			Test.TestTrue(TEXT("The Guard then recovers for 6 s"),
				Combat->GetClassMoveCooldownRemaining(World->GetTimeSeconds()) > 5.0f);
			bHoldKey = false;
			ReleaseKey();
			// Next: Neptune's barrier. Neptune stands beside Mars; Jupiter is far away.
			const FVector Beside = Mars->GetActorLocation() + FVector(150.0, 0.0, 0.0);
			Place(Neptune.Get(), FVector(Beside.X, Beside.Y, 100.0), Mars->GetActorRotation().Yaw);
			Place(Jupiter.Get(), FVector(0.0, -1200.0, 100.0), 90.0f);
			Enter(EPhase::Barrier, 10.0);
		}

		void Barrier()
		{
			UGolemCombatComponent* MarsCombat = Mars->GetCombatComponent();
			if (!bBarrierCast)
			{
				if (WorldElapsed() < 0.5)
				{
					return;
				}
				// Companions do not use class moves by themselves yet; the host triggers Neptune's the way input would.
				UGolemAbilitySystemComponent* ASC = Neptune->GetGolemAbilitySystem();
				const FGolemResult Used = ASC->RequestAbilityInput(GolemGameplayTags::Input_Combat_Block, true);
				ASC->RequestAbilityInput(GolemGameplayTags::Input_Combat_Block, false);
				if (!Used.IsSuccess())
				{
					// The companion may be in the middle of its own action; try again next frame.
					return;
				}
				bBarrierCast = true;
				const double Now = World->GetTimeSeconds();
				Test.TestTrue(TEXT("Mars, 1.5 m away, is covered by the barrier"), MarsCombat->HasBarrierAt(Now));
				Test.TestTrue(TEXT("The Mage is covered too"), Neptune->GetCombatComponent()->HasBarrierAt(Now));
				Test.TestFalse(TEXT("Jupiter, more than 4 m away, is not covered"), Jupiter->GetCombatComponent()->HasBarrierAt(Now));
				Test.TestTrue(TEXT("The barrier lasts 2 s"), MarsCombat->HasBarrierAt(Now + 1.9) && !MarsCombat->HasBarrierAt(Now + 2.1));
				Test.TestTrue(TEXT("The barrier recovers for 12 s"),
					Neptune->GetCombatComponent()->GetClassMoveCooldownRemaining(Now) > 11.0f);
				const float Before = Mars->GetGolemAbilitySystem()->GetGolemAttributes()->GetHealth();
				UGolemAbilitySystemComponent::ApplyCombatDamage(Shield.Get(), Mars.Get(), 25.0f, 0.0f, true);
				Test.TestEqual(TEXT("The barrier stops one hit completely"),
					Mars->GetGolemAbilitySystem()->GetGolemAttributes()->GetHealth(), Before);
				Test.TestFalse(TEXT("And is used up by it"), MarsCombat->HasBarrierAt(World->GetTimeSeconds()));
				UGolemAbilitySystemComponent::ApplyCombatDamage(Shield.Get(), Mars.Get(), 25.0f, 0.0f, true);
				Test.TestEqual(TEXT("The second hit lands in full"),
					Mars->GetGolemAbilitySystem()->GetGolemAttributes()->GetHealth(), Before - 25.0f);
				// Last: Jupiter's backflip shot, facing the Shield from 7 m.
				const FVector Forward = Shield->GetActorForwardVector();
				const FVector Spot = Shield->GetActorLocation() + Forward * 700.0;
				Place(Mars.Get(), FVector(-1200.0, -1200.0, 100.0), 0.0f);
				Place(Neptune.Get(), FVector(1200.0, -1200.0, 100.0), 0.0f);
				// Only the leap may move Jupiter while it is measured: its ordinary decisions wait.
				if (AGolemArenaCompanionController* AI = Cast<AGolemArenaCompanionController>(Jupiter->GetController()))
				{
					AI->GetFollowComponent()->PauseLogic(TEXT("Backflip measurement"));
					AI->StopMovement();
				}
				Place(Jupiter.Get(), FVector(Spot.X, Spot.Y, 100.0), (-Forward).Rotation().Yaw);
				Enter(EPhase::Backflip, 10.0);
			}
		}

		void Backflip()
		{
			if (!bBackflipCast)
			{
				if (WorldElapsed() < 0.6)
				{
					return;
				}
				// Keep Jupiter facing the Shield until the move starts; its own decisions may have turned it.
				const FVector ToShield = Shield->GetActorLocation() - Jupiter->GetActorLocation();
				Jupiter->SetActorRotation(FRotator(0.0, ToShield.Rotation().Yaw, 0.0));
				UGolemAbilitySystemComponent* ASC = Jupiter->GetGolemAbilitySystem();
				const FGolemResult Used = ASC->RequestAbilityInput(GolemGameplayTags::Input_Combat_Block, true);
				ASC->RequestAbilityInput(GolemGameplayTags::Input_Combat_Block, false);
				if (!Used.IsSuccess())
				{
					return;
				}
				bBackflipCast = true;
				BackflipFrom = Jupiter->GetActorLocation();
				BackflipAt = World->GetTimeSeconds();
				ShieldHealthBefore = Shield->GetGolemAttributeSet()->GetHealth();
				ShieldFilledBefore = Shield->GetVoidHost()->GetFilledCount();
				return;
			}
			if (World->GetTimeSeconds() - BackflipAt < 1.6)
			{
				return;
			}
			const double Leap = FVector::Dist2D(Jupiter->GetActorLocation(), BackflipFrom);
			const double Away = FVector::Dist2D(Jupiter->GetActorLocation(), Shield->GetActorLocation())
				- FVector::Dist2D(BackflipFrom, Shield->GetActorLocation());
			Test.TestTrue(FString::Printf(TEXT("The backflip covers about 5 m (measured %.0f cm)"), Leap), Leap > 400.0 && Leap < 600.0);
			Test.TestTrue(TEXT("It goes away from what Jupiter was facing"), Away > 350.0);
			// 1.5 x Power 10 = 15, of which the grey body takes 10%.
			Test.TestEqual(TEXT("The shot deals 1.5 x Power to the grey body"), Shield->GetGolemAttributeSet()->GetHealth(),
				ShieldHealthBefore - 1.5f);
			Test.TestEqual(TEXT("The shot places no colour"), Shield->GetVoidHost()->GetFilledCount(), ShieldFilledBefore);
			Test.TestTrue(TEXT("The backflip shot recovers for 8 s"),
				Jupiter->GetCombatComponent()->GetClassMoveCooldownRemaining(World->GetTimeSeconds()) > 5.5f);
			BeginCleanup();
		}

		void Fail(const FString& Message)
		{
			if (Phase == EPhase::Cleanup || Phase == EPhase::Complete)
			{
				return;
			}
			if (Xenon.IsValid())
			{
				for (AGolemCharacter* Golem : { Mars.Get(), Jupiter.Get(), Neptune.Get() })
				{
					const AGolemArenaCompanionController* AI = Golem != nullptr
						? Cast<AGolemArenaCompanionController>(Golem->GetController()) : nullptr;
					Test.AddInfo(FString::Printf(TEXT("%s at %s job%d swing%d attacking%d"),
						Golem != nullptr ? *Golem->GetName() : TEXT("none"),
						Golem != nullptr ? *Golem->GetActorLocation().ToCompactString() : TEXT("-"),
						AI != nullptr && AI->GetColorAssist()->HasJob(), AI != nullptr && AI->GetColorAssist()->IsSwinging(),
						Golem != nullptr && Golem->GetCombatComponent()->GetSnapshot().bAttacking));
				}
				Test.AddInfo(FString::Printf(TEXT("Xenon at %s filled %d; void %s state %d applied %d required %d"),
					*Xenon->GetActorLocation().ToCompactString(), Xenon->GetVoidHost()->GetFilledCount(),
					ActiveVoid.IsValid() ? *ActiveVoid->GetComponentLocation().ToCompactString() : TEXT("-"),
					ActiveVoid.IsValid() ? static_cast<int32>(ActiveVoid->GetVoidState()) : -1,
					ActiveVoid.IsValid() ? ActiveVoid->GetAppliedMask() : 0, ActiveVoid.IsValid() ? ActiveVoid->GetRequiredMask() : 0));
			}
			Test.AddError(Message);
			BeginCleanup();
		}

		void BeginCleanup()
		{
			if (Phase == EPhase::Cleanup || Phase == EPhase::Complete)
			{
				return;
			}
			ReleaseKey();
			Enter(EPhase::Cleanup, 15.0);
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
				Test.AddError(TEXT("Colour arena PIE cleanup exceeded its bound."));
				if (GEditor->IsPlayingSessionInEditor())
				{
					GEditor->EndPlayMap();
				}
			}
			RestoreSettings();
			Settings.Reset();
			bOwnsSession = false;
			Enter(EPhase::Complete);
		}

		FAutomationTestBase& Test;
		EPhase Phase = EPhase::Navigation;
		double RunStarted = 0.0;
		double PhaseStarted = 0.0;
		double PhaseWorldStarted = 0.0;
		double PhaseTimeout = 30.0;
		double ReadyWorldTime = -1.0;
		double KeyPressedAt = 0.0;
		double HeavyPressedAt = 0.0;
		double SwingStartedAt = -1.0;
		double PrimedAt = 0.0;
		double SignalledAt = 0.0;
		double TelegraphAt = -1.0;
		double StruckAt = -1.0;
		float MarsHealthBefore = 0.0f;
		FVector MarsHitFrom = FVector::ZeroVector;
		bool bOwnsSession = false;
		bool bKeyDown = false;
		bool bPressedHeavy = false;
		bool bXenonsReset = false;
		bool bHoldKey = false;
		bool bGuardPressed = false;
		bool bBarrierCast = false;
		bool bBackflipCast = false;
		int32 StrikesBeforeGuard = 0;
		int32 ShieldFilledBefore = 0;
		double GuardStruckAt = -1.0;
		double BackflipAt = 0.0;
		float ShieldHealthBefore = 0.0f;
		FVector BackflipFrom = FVector::ZeroVector;
		bool bSawDulled = false;
		FKey HeldKey;
		TWeakObjectPtr<UWorld> World;
		TWeakObjectPtr<AGolemColorArenaGameMode> Mode;
		TWeakObjectPtr<AGolemArenaPlayerController> Controller;
		TWeakObjectPtr<AGolemPartyState> Party;
		TWeakObjectPtr<AGolemCharacter> Mars;
		TWeakObjectPtr<AGolemCharacter> Jupiter;
		TWeakObjectPtr<AGolemCharacter> Neptune;
		TWeakObjectPtr<AGolemXenon> Xenon;
		TWeakObjectPtr<AGolemXenon> Shield;
		TWeakObjectPtr<UGolemColorVoidComponent> ActiveVoid;
		TWeakObjectPtr<AActor> LastFilledBy;
		TStrongObjectPtr<ULevelEditorPlaySettings> Settings;
		TArray<FIntPoint> OriginalPositions;
		FIntPoint OriginalLastSize = FIntPoint::ZeroValue;
		FIntPoint OriginalPosition = FIntPoint::ZeroValue;
		int32 OriginalWidth = 0;
		int32 OriginalHeight = 0;
		FWaitForEditorNavMeshBuildCommand NavigationWait;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGolemColorArenaPlaythroughTest, "Golem.Color.Arena.Playthrough",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGolemColorArenaPlaythroughTest::RunTest(const FString& Parameters)
{
	if (GEditor == nullptr || GEditor->IsPlaySessionInProgress()
		|| !FPackageName::DoesPackageExist(GolemColorArenaPlaythrough::MapPath)
		|| !FEditorFileUtils::LoadMap(GolemColorArenaPlaythrough::MapPath, false, false))
	{
		AddError(TEXT("Run GolemBootstrap -Milestone=10 on the integrated build and use an idle Editor."));
		return false;
	}
	AddInfo(TEXT("A real PIE session on the host. The test chooses each void's colour and where the Golems stand; the swing, the shots, the timers and both AI companions are the game's own."));
	AddCommand(new GolemColorArenaPlaythrough::FScenario(*this));
	return true;
}

#endif
