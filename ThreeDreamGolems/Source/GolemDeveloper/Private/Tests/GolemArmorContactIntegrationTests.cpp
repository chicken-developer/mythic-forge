#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/GolemAnimNotifyState_TraceWindow.h"
#include "Attributes/GolemAttributeSet.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Components/CapsuleComponent.h"
#include "Components/GolemAbilitySystemComponent.h"
#include "Components/GolemCombatComponent.h"
#include "Components/GolemLifeComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "CoreGlobals.h"
#include "Effects/GolemDamageEffect.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/WorldSettings.h"
#include "HAL/PlatformTime.h"
#include "SkillLab/GolemBossArmorComponent.h"
#include "Skills/GolemSkillRuntimeComponent.h"
#include "Tags/GolemGameplayTags.h"
#include "UObject/StrongObjectPtr.h"

namespace GolemArmorContactTests
{
	enum class EScenario : uint8
	{
		LaterArmor,
		BodyOnly,
		CancelBeforeArmor,
		CancelDuringFlush
	};

	struct FActorFixture
	{
		TWeakObjectPtr<ACharacter> Actor;
		TWeakObjectPtr<UGolemAbilitySystemComponent> ASC;
		TWeakObjectPtr<UGolemCombatComponent> Combat;
		TWeakObjectPtr<UGolemBossArmorComponent> Armor;
	};

	struct FObservation
	{
		int32 DirectPackets = 0;
		int32 SecondaryPackets = 0;
		int32 Outcomes = 0;
		TWeakObjectPtr<UPrimitiveComponent> DirectComponent;
		TWeakObjectPtr<UPrimitiveComponent> SecondaryComponent;
	};

	/** Runs authored GAS attacks and native physics, including the real boss armor response. */
	class FContactCommand final : public IAutomationLatentCommand
	{
	public:
		FContactCommand(FAutomationTestBase& InTest, EScenario InScenario)
			: Test(InTest), Scenario(InScenario), GameInstance(NewObject<UGameInstance>(GEngine)),
			Observation(MakeShared<FObservation>())
		{
			GameInstance->InitializeStandalone();
			World = GameInstance->GetWorld();
			if (!Test.TestTrue(TEXT("Standalone native World exists"), World.IsValid()))
			{
				return;
			}
			World->SetShouldTick(false);
			World->GetWorldSettings()->DefaultGameMode = AGameModeBase::StaticClass();
			FURL URL;
			URL.Map = TEXT("GolemArmorContactNativeWorld");
			URL.AddOption(TEXT("game=/Script/Engine.GameModeBase"));
			World->SetGameMode(URL);
			World->InitializeActorsForPlay(URL);
			World->BeginPlay();
			Source = Spawn(FVector(0.0f, 0.0f, 88.0f), true);
			Target = Spawn(FVector(84.0f, 0.0f, 100.0f), false);
			bReady = Source.Actor.IsValid() && Source.ASC.IsValid() && Source.Combat.IsValid()
				&& Target.Actor.IsValid() && Target.ASC.IsValid() && Target.Combat.IsValid()
				&& Source.Combat->GetProfile() != nullptr && Target.Combat->GetProfile() != nullptr;
			if (!bReady)
			{
				return;
			}
			AddArmor(Target);
			bReady = Target.Armor.IsValid() && IsValid(Target.Armor->GetArmorZone());
			if (!bReady)
			{
				Test.AddError(TEXT("Real boss armor zone failed to initialize."));
				return;
			}
			if (Scenario == EScenario::BodyOnly || Scenario == EScenario::CancelDuringFlush)
			{
				// A real registered zone outside this swing isolates the ordinary body-contact fallback.
				Target.Armor->GetArmorZone()->SetRelativeLocation(FVector(0.0f, 0.0f, -1000.0f));
			}
			if (Scenario == EScenario::CancelDuringFlush)
			{
				Bystander = Spawn(FVector(84.0f, 40.0f, 100.0f), false);
				bReady = Bystander.Actor.IsValid() && Bystander.ASC.IsValid() && Bystander.Combat.IsValid()
					&& Bystander.Combat->GetProfile() != nullptr;
				if (!bReady)
				{
					return;
				}
				AddArmor(Bystander);
				if (!Bystander.Armor.IsValid() || !IsValid(Bystander.Armor->GetArmorZone()))
				{
					bReady = false;
					Test.AddError(TEXT("Second real boss armor zone failed to initialize."));
					return;
				}
				Bystander.Armor->GetArmorZone()->SetRelativeLocation(FVector(0.0f, 0.0f, -1000.0f));
			}
			Runtime = NewObject<UGolemSkillRuntimeComponent>(Source.Actor.Get());
			Source.Actor->AddInstanceComponent(Runtime.Get());
			Runtime->RegisterComponent();
			FGolemSkillBuild Build;
			Build.SpecId = TEXT("Saturn");
			if (Scenario != EScenario::CancelDuringFlush)
			{
				Build.WeaponRune = TEXT("Weapon.Aftershock");
			}
			bReady &= Test.TestTrue(TEXT("Real Saturn and optional Aftershock source configures"),
				Runtime->ConfigureBuild(Build).IsSuccess());
			FGolemEquipmentCombatBuild Gear;
			Gear.bEnabled = true;
			Gear.bHasHammer = true;
			Gear.RelicEffectId = TEXT("Starfall");
			Gear.RelicInstanceId = FGuid::NewGuid();
			bReady &= Test.TestTrue(TEXT("Actual Starfall instance prepares"), Runtime->PrepareEquipmentBuild(Gear).IsSuccess());
			if (!bReady)
			{
				return;
			}
			Runtime->CommitPreparedEquipmentBuild();
			Runtime->PublishPreparedBuild();
			for (const FGolemAttackDefinition& Attack : Source.Combat->GetProfile()->GetAttacks())
			{
				if (Attack.ActionTag == GolemGameplayTags::Ability_Combat_Heavy)
				{
					Heavy = Attack;
					break;
				}
			}
			const UAnimMontage* HeavyMontage = Heavy.Montage.LoadSynchronous();
			bReady = Test.TestTrue(TEXT("Actual authored Heavy montage resolves"), IsValid(HeavyMontage));
			if (!bReady)
			{
				return;
			}
			for (const FAnimNotifyEvent& Notify : HeavyMontage->Notifies)
			{
				if (Cast<UGolemAnimNotifyState_TraceWindow>(Notify.NotifyStateClass) != nullptr)
				{
					TraceStart = Notify.GetTriggerTime();
					TraceEnd = Notify.GetEndTriggerTime();
					break;
				}
			}
			bReady &= Test.TestTrue(TEXT("Test uses the actual authored heavy trace notify"), TraceEnd > TraceStart);
			BindDamageObservation(Target);
			if (Bystander.Actor.IsValid())
			{
				BindDamageObservation(Bystander);
			}
			const TWeakPtr<FObservation> WeakObservation(Observation);
			const TWeakObjectPtr<UGolemCombatComponent> WeakCombat = Source.Combat;
			const bool bCancelInOutcome = Scenario == EScenario::CancelDuringFlush;
			OutcomeHandle = Source.Combat->OnCombatOutcome.AddLambda(
				[WeakObservation, WeakCombat, bCancelInOutcome](const FGolemCombatOutcome& Outcome)
				{
					if (Outcome.Outcome != EGolemCombatOutcome::HeavyHit)
					{
						return;
					}
					if (const TSharedPtr<FObservation> Capture = WeakObservation.Pin())
					{
						++Capture->Outcomes;
						if (bCancelInOutcome && Capture->Outcomes == 1 && WeakCombat.IsValid())
						{
							WeakCombat->ClearTransientActions();
						}
					}
				});
		}

		virtual ~FContactCommand() override
		{
			if (Source.Combat.IsValid())
			{
				Source.Combat->OnCombatOutcome.Remove(OutcomeHandle);
			}
			if (World.IsValid())
			{
				World->EndPlay(EEndPlayReason::Quit);
			}
			UWorld* OwnedWorld = World.Get();
			GameInstance->Shutdown();
			if (OwnedWorld != nullptr)
			{
				OwnedWorld->DestroyWorld(false);
				GEngine->DestroyWorldContext(OwnedWorld);
			}
		}

		virtual bool Update() override
		{
			if (!bReady || Test.HasAnyErrors())
			{
				return true;
			}
			if (FPlatformTime::Seconds() - Started > 30.0)
			{
				Test.AddError(TEXT("Real armor contact regression timed out."));
				return true;
			}
			if (LastFrame == GFrameCounter)
			{
				return false;
			}
			LastFrame = GFrameCounter;
			World->Tick(LEVELTICK_All, 0.025f);
			Elapsed += 0.025f;
			if (!bAttackStarted)
			{
				if (Elapsed >= 0.1f)
				{
					bAttackStarted = true;
					Test.TestTrue(TEXT("Authored Heavy activates through actual GAS input"),
						Source.ASC->RequestAbilityInput(GolemGameplayTags::Input_Combat_Heavy, true).IsSuccess());
					Source.ASC->RequestAbilityInput(GolemGameplayTags::Input_Combat_Heavy, false);
				}
				return false;
			}
			if (!bObservedBody && IsBodyOnlyTraceNow())
			{
				bObservedBody = true;
				Test.TestEqual(TEXT("First real capsule contact waits without HP damage"),
					Target.ASC->GetGolemAttributes()->GetHealth(), 1000.0f);
				Test.TestEqual(TEXT("First real capsule contact waits without Poise damage"),
					Target.Combat->GetSnapshot().Poise, Target.Combat->GetMaxPoise());
				Test.TestEqual(TEXT("Deferred body contact has no direct packet/proc"), Observation->DirectPackets, 0);
				Test.TestEqual(TEXT("Deferred body contact has no Rune child"), Observation->SecondaryPackets, 0);
				Test.TestEqual(TEXT("Deferred body contact has no relic count"), Runtime->GetSkillState().StarfallCount, 0);
				if (Scenario == EScenario::CancelBeforeArmor)
				{
					Source.Combat->ClearTransientActions();
				}
			}
			if (Scenario == EScenario::CancelDuringFlush && Observation->Outcomes == 1
				&& !Source.Combat->GetSnapshot().bAttacking && !bFreshAttackStarted)
			{
				// A later genuine window with no reachable targets must not inherit the retired pending contacts.
				Target.Actor->SetActorLocation(FVector(2000.0f, 0.0f, 100.0f));
				Bystander.Actor->SetActorLocation(FVector(2000.0f, 40.0f, 100.0f));
				bFreshAttackStarted = true;
				Test.TestTrue(TEXT("A fresh authored Heavy activates after callback cancellation"),
					Source.ASC->RequestAbilityInput(GolemGameplayTags::Input_Combat_Heavy, true).IsSuccess());
				Source.ASC->RequestAbilityInput(GolemGameplayTags::Input_Combat_Heavy, false);
			}
			if (Source.Combat->GetSnapshot().bAttacking || Elapsed < 2.5f)
			{
				return false;
			}
			Verify();
			return true;
		}

	private:
		FActorFixture Spawn(const FVector& Location, bool bParty)
		{
			FActorFixture Fixture;
			Fixture.Actor = World->SpawnActor<ACharacter>();
			if (!Test.TestTrue(TEXT("Native Character spawns"), Fixture.Actor.IsValid()))
			{
				return Fixture;
			}
			Fixture.Actor->SetActorLocation(Location);
			Fixture.Actor->GetCharacterMovement()->DisableMovement();
			Fixture.Actor->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
			if (!bParty)
			{
				// Broad body contact gives deterministic capsule-before-zone ordering without changing weapon reach.
				Fixture.Actor->GetCapsuleComponent()->SetCapsuleSize(140.0f, 150.0f);
			}
			Fixture.ASC = NewObject<UGolemAbilitySystemComponent>(Fixture.Actor.Get());
			Fixture.Actor->AddInstanceComponent(Fixture.ASC.Get());
			Fixture.ASC->RegisterComponent();
			UGolemAttributeSet* Attributes = NewObject<UGolemAttributeSet>(Fixture.Actor.Get());
			UGolemLifeComponent* Life = NewObject<UGolemLifeComponent>(Fixture.Actor.Get());
			Fixture.Actor->AddInstanceComponent(Life);
			Life->RegisterComponent();
			FGolemAttributeDefaults Defaults;
			Defaults.MaxHealth = Defaults.MaxStamina = 1000.0f;
			Defaults.AttackPower = 100.0f;
			Defaults.Defense = 0.0f;
			Test.TestTrue(TEXT("Native Character ASC initializes"), Fixture.ASC->InitializeGolemActor(
				Fixture.Actor.Get(), Attributes, Life, FGolemEntityId::Create(), Defaults, bParty).IsSuccess());
			if (!bParty)
			{
				Fixture.ASC->SetLooseGameplayTagCount(GolemGameplayTags::Role_Enemy, 1,
					EGameplayTagReplicationState::TagAndCountToAll);
			}
			Fixture.Combat = NewObject<UGolemCombatComponent>(Fixture.Actor.Get());
			Fixture.Actor->AddInstanceComponent(Fixture.Combat.Get());
			Fixture.Combat->RegisterComponent();
			Test.TestTrue(TEXT("Actual combat profile configures"), Fixture.Combat->Configure(
				FPrimaryAssetId(TEXT("KnightCombatProfile"), TEXT("FoundationCombat"))).IsSuccess());
			return Fixture;
		}

		void AddArmor(FActorFixture& Fixture)
		{
			Fixture.Armor = NewObject<UGolemBossArmorComponent>(Fixture.Actor.Get());
			Fixture.Actor->AddInstanceComponent(Fixture.Armor.Get());
			Fixture.Armor->RegisterComponent();
			Fixture.Armor->EnableArmorZone(FGuid::NewGuid());
			Test.TestTrue(TEXT("Real armor becomes Brittle through its authoritative command"), Fixture.Armor->MakeBrittle());
		}

		void BindDamageObservation(const FActorFixture& Fixture)
		{
			const TWeakPtr<FObservation> Weak(Observation);
			FGameplayEffectApplicationQuery Query;
			Query.BindLambda([Weak](const FActiveGameplayEffectsContainer&, const FGameplayEffectSpec& Spec)
			{
				if (Spec.Def != nullptr && Spec.Def->IsA<UGolemDamageEffect>())
				{
					if (const TSharedPtr<FObservation> Capture = Weak.Pin())
					{
						if (Spec.GetSetByCallerMagnitude(FName(TEXT("SkillDirect")), false, 0.0f) > 0.0f)
						{
							++Capture->DirectPackets;
							const FHitResult* Hit = Spec.GetContext().GetHitResult();
							Capture->DirectComponent = Hit != nullptr ? Hit->GetComponent() : nullptr;
						}
						else
						{
							++Capture->SecondaryPackets;
							const FHitResult* Hit = Spec.GetContext().GetHitResult();
							Capture->SecondaryComponent = Hit != nullptr ? Hit->GetComponent() : nullptr;
						}
					}
				}
				return true;
			});
			Fixture.ASC->GameplayEffectApplicationQueries.Add(MoveTemp(Query));
		}

		bool IsBodyOnlyTraceNow()
		{
			UAnimInstance* Animation = Source.Actor->GetMesh()->GetAnimInstance();
			const UAnimMontage* Montage = Source.ASC->GetCurrentMontage();
			if (Animation == nullptr || Montage != Heavy.Montage.Get() || !Source.Combat->GetSnapshot().bAttacking)
			{
				return false;
			}
			const float Position = Animation->Montage_GetPosition(Montage);
			if (Position < TraceStart || Position >= TraceEnd)
			{
				return false;
			}
			TInlineComponentArray<UStaticMeshComponent*> Meshes(Source.Actor.Get());
			const UStaticMeshComponent* Hammer = nullptr;
			for (const UStaticMeshComponent* Mesh : Meshes)
			{
				if (Mesh->GetFName() == TEXT("GolemCombatHammer"))
				{
					Hammer = Mesh;
					break;
				}
			}
			if (Hammer == nullptr)
			{
				return false;
			}
			FCollisionObjectQueryParams Objects;
			Objects.AddObjectTypesToQuery(ECC_Pawn);
			Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
			const FCollisionQueryParams Query(SCENE_QUERY_STAT(GolemArmorContactRegression), false, Source.Actor.Get());
			TArray<FHitResult> Hits;
			World->SweepMultiByObjectType(Hits, Source.Actor->GetMesh()->GetSocketLocation(TEXT("hand_r")),
				Hammer->GetComponentLocation() + Hammer->GetUpVector() * 50.0f, FQuat::Identity, Objects,
				FCollisionShape::MakeSphere(Heavy.TraceRadius), Query);
			bool bBody = false;
			bool bArmor = false;
			for (const FHitResult& Hit : Hits)
			{
				bBody |= Hit.GetComponent() == Target.Actor->GetCapsuleComponent();
				bArmor |= Hit.GetComponent() == Target.Armor->GetArmorZone();
				if (Bystander.Actor.IsValid() && Hit.GetComponent() == Bystander.Actor->GetCapsuleComponent())
				{
					bObservedBystanderBody = true;
				}
			}
			return bBody && !bArmor;
		}

		void Verify()
		{
			Test.TestTrue(TEXT("Actual physics established capsule contact before armor contact"), bObservedBody);
			if (Scenario == EScenario::CancelBeforeArmor)
			{
				Test.TestEqual(TEXT("Cancellation discards deferred HP damage"), Target.ASC->GetGolemAttributes()->GetHealth(), 1000.0f);
				Test.TestEqual(TEXT("Cancellation discards deferred Poise damage"), Target.Combat->GetSnapshot().Poise,
					Target.Combat->GetMaxPoise());
				Test.TestEqual(TEXT("Canceled contact creates no direct packet"), Observation->DirectPackets, 0);
				Test.TestEqual(TEXT("Canceled contact creates no Rune packet"), Observation->SecondaryPackets, 0);
				Test.TestEqual(TEXT("Canceled contact creates no outcome"), Observation->Outcomes, 0);
				Test.TestEqual(TEXT("Canceled contact creates no relic proc"), Runtime->GetSkillState().StarfallCount, 0);
				Test.TestEqual(TEXT("Canceled contact leaves actual armor durability"), Target.Armor->GetDurability(), 100.0f);
				return;
			}
			Test.TestEqual(TEXT("One direct damage packet for the actor/window"), Observation->DirectPackets, 1);
			Test.TestEqual(TEXT("One confirmed Heavy outcome"), Observation->Outcomes, 1);
			Test.TestEqual(TEXT("One eligible Starfall activation"), Runtime->GetSkillState().StarfallCount, 1);
			if (Scenario == EScenario::CancelDuringFlush)
			{
				Test.TestTrue(TEXT("Both real capsules contacted before reentrant flush"), bObservedBystanderBody);
				Test.TestTrue(TEXT("Fresh window verifies retired-contact generation cleanup"), bFreshAttackStarted);
				const float Damage = 2000.0f - Target.ASC->GetGolemAttributes()->GetHealth()
					- Bystander.ASC->GetGolemAttributes()->GetHealth();
				Test.TestTrue(TEXT("Reentrant cancellation stops all remaining pending packets"),
					FMath::IsNearlyEqual(Damage, Heavy.Damage, 0.01f));
				Test.TestEqual(TEXT("Reentrant flush creates no extra secondary packet"), Observation->SecondaryPackets, 0);
				Test.TestFalse(TEXT("Callback cancellation closes the old attack"), Source.Combat->GetSnapshot().bAttacking);
				Test.TestEqual(TEXT("Both actual armor zones remain untouched"),
					Target.Armor->GetDurability() + Bystander.Armor->GetDurability(), 200.0f);
				return;
			}
			const bool bZone = Scenario == EScenario::LaterArmor;
			Test.TestTrue(TEXT("Direct packet preserves the original native selected component"),
				Observation->DirectComponent.Get() == (bZone
					? static_cast<UPrimitiveComponent*>(Target.Armor->GetArmorZone())
					: static_cast<UPrimitiveComponent*>(Target.Actor->GetCapsuleComponent())));
			Test.TestEqual(TEXT("Aftershock emits only one Rune child"), Observation->SecondaryPackets, 1);
			if (bZone)
			{
				Test.TestTrue(TEXT("Aftershock independently traces the real armor zone"),
					Observation->SecondaryComponent.Get() == Target.Armor->GetArmorZone());
			}
			// The existing Rune is a normal skill hit with its own activation, not an M5.6 ItemProc.
			// A real zone contact therefore receives Brittle mitigation and ten doubled durability damage.
			const float ExpectedDamage = Heavy.Damage * (bZone ? 0.5f : 1.0f) +
				Source.ASC->GetGolemAttributes()->GetAttackPower() * 0.30f * (bZone ? 0.5f : 1.0f);
			Test.TestTrue(TEXT("HP uses true selected armor mitigation and one Rune child"), FMath::IsNearlyEqual(
				1000.0f - Target.ASC->GetGolemAttributes()->GetHealth(), ExpectedDamage, 0.01f));
			Test.TestTrue(TEXT("Poise applies once and clamps at the real authored pool"), FMath::IsNearlyEqual(
				Target.Combat->GetMaxPoise() - Target.Combat->GetSnapshot().Poise,
				FMath::Min(Target.Combat->GetMaxPoise(), Heavy.PoiseDamage + 5.0f), 0.01f));
			Test.TestEqual(TEXT("Saturn receives only one Heavy stack"), Runtime->GetSkillState().Stacks, 1);
			Test.TestEqual(TEXT("Actual durability changes only when the native zone was selected"),
				Target.Armor->GetDurability(), bZone ? 40.0f : 100.0f);
		}

		FAutomationTestBase& Test;
		EScenario Scenario;
		TStrongObjectPtr<UGameInstance> GameInstance;
		TWeakObjectPtr<UWorld> World;
		FActorFixture Source;
		FActorFixture Target;
		FActorFixture Bystander;
		TWeakObjectPtr<UGolemSkillRuntimeComponent> Runtime;
		TSharedPtr<FObservation> Observation;
		FGolemAttackDefinition Heavy;
		FDelegateHandle OutcomeHandle;
		uint64 LastFrame = MAX_uint64;
		double Started = FPlatformTime::Seconds();
		float Elapsed = 0.0f;
		float TraceStart = 0.0f;
		float TraceEnd = 0.0f;
		bool bReady = false;
		bool bAttackStarted = false;
		bool bObservedBody = false;
		bool bObservedBystanderBody = false;
		bool bFreshAttackStarted = false;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGolemArmorLaterContactTest, "Golem.M56.Combat.CapsuleThenArmorOnce",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGolemArmorLaterContactTest::RunTest(const FString& Parameters)
{
	ADD_LATENT_AUTOMATION_COMMAND(GolemArmorContactTests::FContactCommand(*this,
		GolemArmorContactTests::EScenario::LaterArmor));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGolemArmorBodyFallbackTest, "Golem.M56.Combat.BodyOnlyFallbackOnce",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGolemArmorBodyFallbackTest::RunTest(const FString& Parameters)
{
	ADD_LATENT_AUTOMATION_COMMAND(GolemArmorContactTests::FContactCommand(*this,
		GolemArmorContactTests::EScenario::BodyOnly));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGolemArmorCanceledContactTest, "Golem.M56.Combat.CancelDeferredArmorContact",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGolemArmorCanceledContactTest::RunTest(const FString& Parameters)
{
	ADD_LATENT_AUTOMATION_COMMAND(GolemArmorContactTests::FContactCommand(*this,
		GolemArmorContactTests::EScenario::CancelBeforeArmor));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGolemArmorReentrantFlushTest, "Golem.M56.Combat.CancelDuringDeferredFlush",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGolemArmorReentrantFlushTest::RunTest(const FString& Parameters)
{
	ADD_LATENT_AUTOMATION_COMMAND(GolemArmorContactTests::FContactCommand(*this,
		GolemArmorContactTests::EScenario::CancelDuringFlush));
	return true;
}

#endif
