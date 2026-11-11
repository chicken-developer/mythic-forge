#include "Components/GolemCombatComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Abilities/GolemRuneSlowEffect.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimInstance.h"
#include "Animation/GolemAnimNotifyState_TraceWindow.h"
#include "Assets/GolemAssetManager.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/GolemAbilitySystemComponent.h"
#include "Components/GolemLifeComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Effects/GolemDamageEffect.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/WorldSettings.h"
#include "HAL/PlatformTime.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Skills/GolemSkillRuntimeComponent.h"
#include "Tags/GolemGameplayTags.h"
#include "UObject/StrongObjectPtr.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGolemCombatDefinitionTest, "Golem.Combat.DefinitionContracts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGolemCombatDefinitionTest::RunTest(const FString& Parameters)
{
	UGolemAssetManager* Manager = UGolemAssetManager::GetGolemAssetManager();
	UGolemDefinition* Definition = nullptr;
	if (!TestNotNull(TEXT("Project asset manager exists"), Manager) ||
		!TestTrue(TEXT("Real combat fixture resolves by stable ID"), Manager->ResolveDefinition(
			FPrimaryAssetId(TEXT("KnightCombatProfile"), TEXT("FoundationCombat")),
			UGolemCombatProfileDefinition::StaticClass(), Definition).IsSuccess()))
	{
		return false;
	}
	const UGolemCombatProfileDefinition* Profile = CastChecked<UGolemCombatProfileDefinition>(Definition);
	TestTrue(TEXT("Actual skeleton, sections and notify windows validate"), Profile->ValidateCombatProfile().IsSuccess());
	UGolemDefinition* Set = nullptr;
	TestTrue(TEXT("Combat source set has a resolvable persisted identity"),
		Manager->ResolveDefinition(Profile->GetCombatAbilitySetId(), UGolemAbilitySetDefinition::StaticClass(), Set).IsSuccess());
	const TStrongObjectPtr<UGolemCombatProfileDefinition> Edited(
		DuplicateObject<UGolemCombatProfileDefinition>(Profile, GetTransientPackage()));
	FGolemCombatTuning Invalid = Edited->GetTuning();
	Invalid.ParryWindow = std::numeric_limits<float>::quiet_NaN();
	TestEqual(TEXT("Nonfinite defense timing rejected"), Edited->TrySetTuning(Invalid).Error, EGolemError::InvalidData);
	TestEqual(TEXT("Rejected tuning preserves authored timing"), Edited->GetTuning().ParryWindow,
		Profile->GetTuning().ParryWindow);
	TestEqual(TEXT("Foreign definition type rejected for GAS source"),
		Edited->TrySetCombatAbilitySetId(FPrimaryAssetId(TEXT("KnightHero"), TEXT("Training_Red"))).Error,
		EGolemError::InvalidData);
	TArray<FGolemAttackDefinition> Attacks = Edited->GetAttacks();
	const TStrongObjectPtr<UAnimMontage> MissingNotifies(
		DuplicateObject<UAnimMontage>(Attacks[0].Montage.LoadSynchronous(), GetTransientPackage()));
	MissingNotifies->Notifies.Reset();
	Attacks[0].Montage = MissingNotifies.Get();
	TestEqual(TEXT("Valid animation without weapon notify cannot masquerade as an attack"),
		Edited->TrySetAttacks(Attacks).Error, EGolemError::InvalidData);
	TestTrue(TEXT("Failed attack edit preserves original montage"),
		Edited->GetAttacks()[0].Montage == Profile->GetAttacks()[0].Montage);
	Attacks = Profile->GetAttacks();
	TArray<int32> PrimaryIndices;
	for (int32 Index = 0; Index < Attacks.Num(); ++Index)
	{
		if (Attacks[Index].ActionTag == GolemGameplayTags::Ability_Combat_Primary)
		{
			PrimaryIndices.Add(Index);
		}
	}
	if (TestTrue(TEXT("Real fixture contains an ordered combo"), PrimaryIndices.Num() >= 2))
	{
		Attacks.Swap(PrimaryIndices[0], PrimaryIndices[1]);
		TestEqual(TEXT("Reversed section sequence cannot pass combo contract"),
			Edited->TrySetAttacks(Attacks).Error, EGolemError::InvalidData);
		Attacks = Profile->GetAttacks();
		const TStrongObjectPtr<UAnimMontage> DifferentMontage(DuplicateObject<UAnimMontage>(
			Attacks[PrimaryIndices[1]].Montage.LoadSynchronous(), GetTransientPackage()));
		Attacks[PrimaryIndices[1]].Montage = DifferentMontage.Get();
		TestEqual(TEXT("Separate otherwise-valid montages cannot masquerade as one section combo"),
			Edited->TrySetAttacks(Attacks).Error, EGolemError::InvalidData);
	}
	const TStrongObjectPtr<UAnimMontage> MissingWarping(
		DuplicateObject<UAnimMontage>(Profile->GetTraversalMontage(), GetTransientPackage()));
	MissingWarping->Notifies.Reset();
	TestEqual(TEXT("Traversal with real root motion but no real warp window is rejected"), Edited->TrySetActionMontages(
		Profile->GetDodgeMontage(), Profile->GetRuneMontage(), Profile->GetStaggerMontage(), MissingWarping.Get(),
		Profile->GetFinisherSourceMontage(), Profile->GetFinisherTargetMontage()).Error, EGolemError::InvalidData);
	TestTrue(TEXT("Failed traversal edit preserves valid motion warping content"),
		Edited->GetTraversalMontage() == Profile->GetTraversalMontage());
	const TStrongObjectPtr<UAnimMontage> EarlyBlendOut(
		DuplicateObject<UAnimMontage>(Profile->GetTraversalMontage(), GetTransientPackage()));
	EarlyBlendOut->BlendOutTriggerTime = -1.0f;
	EarlyBlendOut->BlendOut.SetBlendTime(0.1f);
	TestEqual(TEXT("Blending out during the warp window cannot lose the validated landing trajectory"),
		Edited->TrySetActionMontages(Profile->GetDodgeMontage(), Profile->GetRuneMontage(),
			Profile->GetStaggerMontage(), EarlyBlendOut.Get(), Profile->GetFinisherSourceMontage(),
			Profile->GetFinisherTargetMontage()).Error, EGolemError::InvalidData);
	TestTrue(TEXT("Rejected early blend preserves valid traversal content"),
		Edited->GetTraversalMontage() == Profile->GetTraversalMontage());
	return true;
}

namespace GolemCombatTests
{
	struct FGolemCombatFixture
	{
		TWeakObjectPtr<ACharacter> Actor;
		TWeakObjectPtr<UGolemAbilitySystemComponent> ASC;
		TWeakObjectPtr<UGolemAttributeSet> Attributes;
		TWeakObjectPtr<UGolemLifeComponent> Life;
		TWeakObjectPtr<UGolemCombatComponent> Combat;
	};

	struct FGolemParrySweepObservation
	{
		int32 Count = 0;
		float TargetHealth = 0.0f;
		float BystanderHealth = 0.0f;
		float WorldTime = 0.0f;
	};

	struct FGolemRuneApplicationObservation
	{
		int32 DamageAccepted = 0;
		int32 StatusRejected = 0;
	};

	class FGolemCombatWorld final : public FNoncopyable
	{
	public:
		FGolemCombatWorld()
			: GameInstance(NewObject<UGameInstance>(GEngine))
		{
			GameInstance->InitializeStandalone();
			World = GameInstance->GetWorld();
			World->SetShouldTick(false);
			World->GetWorldSettings()->DefaultGameMode = AGameModeBase::StaticClass();
			FURL URL;
			URL.Map = TEXT("GolemCombatNativeWorld");
			URL.AddOption(TEXT("game=/Script/Engine.GameModeBase"));
			World->SetGameMode(URL);
			World->InitializeActorsForPlay(URL);
			World->BeginPlay();
			AActor* Floor = World->SpawnActor<AActor>();
			UBoxComponent* Box = NewObject<UBoxComponent>(Floor);
			Floor->AddInstanceComponent(Box);
			Floor->SetRootComponent(Box);
			Box->SetBoxExtent(FVector(3000.0f, 3000.0f, 10.0f));
			Box->SetCollisionProfileName(TEXT("BlockAll"));
			Box->RegisterComponent();
			Floor->SetActorLocation(FVector(0.0f, 0.0f, -10.0f));
		}
		~FGolemCombatWorld()
		{
			StopPlay();
			UWorld* OwnedWorld = World.Get();
			GameInstance->Shutdown();
			if (OwnedWorld != nullptr)
			{
				OwnedWorld->DestroyWorld(false);
				GEngine->DestroyWorldContext(OwnedWorld);
			}
			World.Reset();
		}
		void StopPlay()
		{
			if (World.IsValid() && World->HasBegunPlay())
			{
				World->EndPlay(EEndPlayReason::Quit);
			}
		}
		TWeakObjectPtr<UWorld> World;

	private:
		TStrongObjectPtr<UGameInstance> GameInstance;
	};

	FGolemCombatFixture SpawnGolem(UWorld* World, const FVector& Location, bool bParty)
	{
		FGolemCombatFixture Fixture;
		Fixture.Actor = World->SpawnActor<ACharacter>();
		Fixture.Actor->SetActorLocation(Location);
		Fixture.Actor->SetReplicates(true);
		Fixture.Actor->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
		Fixture.ASC = NewObject<UGolemAbilitySystemComponent>(Fixture.Actor.Get());
		Fixture.Actor->AddInstanceComponent(Fixture.ASC.Get());
		Fixture.ASC->RegisterComponent();
		Fixture.Attributes = NewObject<UGolemAttributeSet>(Fixture.Actor.Get());
		Fixture.Life = NewObject<UGolemLifeComponent>(Fixture.Actor.Get());
		Fixture.Actor->AddInstanceComponent(Fixture.Life.Get());
		Fixture.Life->RegisterComponent();
		FGolemAttributeDefaults Defaults;
		Defaults.MaxHealth = 1000.0f;
		Defaults.MaxStamina = 1000.0f;
		Fixture.ASC->InitializeGolemActor(Fixture.Actor.Get(), Fixture.Attributes.Get(), Fixture.Life.Get(),
			FGolemEntityId::Create(), Defaults, bParty);
		if (!bParty)
		{
			Fixture.ASC->SetLooseGameplayTagCount(GolemGameplayTags::Role_Enemy, 1,
				EGameplayTagReplicationState::TagAndCountToAll);
		}
		Fixture.Combat = NewObject<UGolemCombatComponent>(Fixture.Actor.Get());
		Fixture.Actor->AddInstanceComponent(Fixture.Combat.Get());
		Fixture.Combat->RegisterComponent();
		return Fixture;
	}

	enum class EGolemCombatTestStep : uint8
	{
		Warmup,
		Light,
		Combo,
		Heavy,
		CanceledAttack,
		ReentrantParrySweep,
		Defense,
		Dodge,
		Rune,
		RuneExpiry,
		LockLOS,
		Finished
	};

	class FGolemCombatMechanicsCommand final : public IAutomationLatentCommand
	{
	public:
		explicit FGolemCombatMechanicsCommand(FAutomationTestBase* InTest)
			: Test(InTest)
		{
			Source = SpawnGolem(Context.World.Get(), FVector(0.0f, 0.0f, 88.0f), true);
			Target = SpawnGolem(Context.World.Get(), FVector(120.0f, 0.0f, 100.0f), false);
			// A broad collision target keeps this test about actual notify-window deduplication, independent of art reach.
			Target.Actor->GetCapsuleComponent()->SetCapsuleSize(140.0f, 150.0f);
			Target.Actor->GetCharacterMovement()->DisableMovement();
			const FPrimaryAssetId ProfileId(TEXT("KnightCombatProfile"), TEXT("FoundationCombat"));
			bReady = Test->TestTrue(TEXT("Source real combat profile configures"), Source.Combat->Configure(ProfileId).IsSuccess()) &&
				Test->TestTrue(TEXT("Enemy uses same native combat domain"), Target.Combat->Configure(ProfileId).IsSuccess());
			if (bReady)
			{
				Profile = Source.Combat->GetProfile();
				Test->TestEqual(TEXT("Configuring same source is idempotent"), Source.Combat->Configure(ProfileId).Error, EGolemError::None);
				Test->TestEqual(TEXT("Exactly one combat grant source"), Source.ASC->GetOwnedGrantCount(), 1);
				Test->TestTrue(TEXT("All nine actual combat GAS specs granted"), Source.ASC->GetActivatableAbilities().Num() == 9);
				Source.Combat->SetValidatedTarget(Target.Actor.Get());
			}
		}
		virtual ~FGolemCombatMechanicsCommand() override
		{
			CleanupParrySweep();
		}

		virtual bool Update() override
		{
			if (!bReady)
			{
				return true;
			}
			if (FPlatformTime::Seconds() - Started > 45.0)
			{
				Test->AddError(FString::Printf(TEXT("Combat mechanics timeout at step %d"), static_cast<int32>(Step)));
				return true;
			}
			Context.World->Tick(LEVELTICK_All, 0.025f);
			Elapsed += 0.025f;
			switch (Step)
			{
			case EGolemCombatTestStep::Warmup:
				if (Elapsed < 0.1f)
				{
					return false;
				}
				BeforeHealth = Target.Attributes->GetHealth();
				Press(GolemGameplayTags::Input_Combat_Primary);
				Test->TestTrue(TEXT("Actual primary ability remains active after release"), Source.Combat->GetSnapshot().bAttacking);
				Next(EGolemCombatTestStep::Light);
				break;
			case EGolemCombatTestStep::Light:
				if (Target.ASC->GetCurrentMontage() == Profile->GetStaggerMontage() &&
					Target.Actor->GetMesh()->GetAnimInstance()->Montage_IsPlaying(Profile->GetStaggerMontage()) &&
					!Target.Combat->GetSnapshot().bStaggered)
				{
					bObservedFlinch = true;
				}
				if (Source.Combat->GetSnapshot().bAttacking)
				{
					return false;
				}
				Test->TestEqual(TEXT("Many real notify-tick sweeps cause exactly one light hit"),
					Source.Combat->GetSnapshot().GetOutcomeCount(EGolemCombatOutcome::LightHit), 1);
				Test->TestTrue(TEXT("Non-stagger hit plays actual reaction montage while target is idle"), bObservedFlinch);
				Test->TestEqual(TEXT("One window applies authored health damage exactly once"),
					BeforeHealth - Target.Attributes->GetHealth(), FirstAttack(GolemGameplayTags::Ability_Combat_Primary).Damage);
				Test->TestEqual(TEXT("Light costs the authored stamina via GE"), Source.Attributes->GetStamina(),
					1000.0f - FirstAttack(GolemGameplayTags::Ability_Combat_Primary).StaminaCost);
				ResetActors();
				BeforeHealth = Target.Attributes->GetHealth();
				Press(GolemGameplayTags::Input_Combat_Primary);
				BufferedIndex = 0;
				Next(EGolemCombatTestStep::Combo);
				break;
			case EGolemCombatTestStep::Combo:
				if (Source.Combat->HasOpenComboWindow() && Source.Combat->GetSnapshot().ComboIndex != BufferedIndex)
				{
					BufferedIndex = Source.Combat->GetSnapshot().ComboIndex;
					Press(GolemGameplayTags::Input_Combat_Primary);
				}
				if (Source.Combat->GetSnapshot().bAttacking)
				{
					return false;
				}
				{
					int32 LightCount = 0;
					float Damage = 0.0f;
					float Cost = 0.0f;
					for (const FGolemAttackDefinition& Attack : Profile->GetAttacks())
					{
						if (Attack.ActionTag == GolemGameplayTags::Ability_Combat_Primary)
						{
							++LightCount;
							Damage += Attack.Damage;
							Cost += Attack.StaminaCost;
						}
					}
					Test->TestEqual(TEXT("Input in actual montage notify window reaches every light section"),
						Source.Combat->GetSnapshot().ComboIndex, LightCount);
					Test->TestEqual(TEXT("Every section transition is a confirmed combo fact"),
						Source.Combat->GetSnapshot().GetOutcomeCount(EGolemCombatOutcome::Combo), LightCount - 1);
					Test->TestEqual(TEXT("Combo sections do not multiply each window's damage"),
						BeforeHealth - Target.Attributes->GetHealth(), Damage);
					Test->TestEqual(TEXT("Every executed combo section commits one stamina cost"),
						Source.Attributes->GetStamina(), 1000.0f - Cost);
				}
				ResetActors();
				BeforeHealth = Target.Attributes->GetHealth();
				Press(GolemGameplayTags::Input_Combat_Heavy);
				Next(EGolemCombatTestStep::Heavy);
				break;
			case EGolemCombatTestStep::Heavy:
				if (Source.Combat->GetSnapshot().bAttacking)
				{
					return false;
				}
				Test->TestEqual(TEXT("Heavy executes real trace once"),
					Source.Combat->GetSnapshot().GetOutcomeCount(EGolemCombatOutcome::HeavyHit), 1);
				Test->TestEqual(TEXT("Heavy applies its data-driven damage"), BeforeHealth - Target.Attributes->GetHealth(),
					FirstAttack(GolemGameplayTags::Ability_Combat_Heavy).Damage);
				ResetActors();
				BeforeHealth = Target.Attributes->GetHealth();
				Press(GolemGameplayTags::Input_Combat_Primary);
				Source.ASC->HandleControllerChanged();
				Test->TestFalse(TEXT("Owner change cancels active attack"), Source.Combat->GetSnapshot().bAttacking);
				Test->TestEqual(TEXT("Owner change preserves combat grant"), Source.ASC->GetOwnedGrantCount(), 1);
				Next(EGolemCombatTestStep::CanceledAttack);
				break;
			case EGolemCombatTestStep::CanceledAttack:
				if (Elapsed < 0.75f)
				{
					return false;
				}
				Test->TestEqual(TEXT("Canceled montage cannot emit delayed trace damage"), Target.Attributes->GetHealth(), BeforeHealth);
				if (!PrepareParrySweep())
				{
					return true;
				}
				Next(EGolemCombatTestStep::ReentrantParrySweep);
				break;
			case EGolemCombatTestStep::ReentrantParrySweep:
				StartParryBeforeTrace();
				if (ParryObservation->Count == 0)
				{
					if (Source.Combat->GetSnapshot().bAttacking)
					{
						return false;
					}
					Test->AddError(TEXT("Actual montage trace did not reach the active parry collider."));
					return true;
				}
				if (Context.World->GetTimeSeconds() - ParryObservation->WorldTime < 0.1f)
				{
					return false;
				}
				Test->TestTrue(TEXT("Real sweep parry synchronously staggers its attacking actor"),
					Source.Combat->GetSnapshot().bStaggered);
				Test->TestFalse(TEXT("Real sweep parry cancels the actual attack before remaining hits"),
					Source.Combat->GetSnapshot().bAttacking);
				Test->TestEqual(TEXT("Cleared dedup cannot repeat parry for another collider in the canceled sweep"),
					ParryObservation->Count, 1);
				Test->TestEqual(TEXT("No target damage occurs after the observed parry"),
					Target.Attributes->GetHealth(), ParryObservation->TargetHealth);
				Test->TestEqual(TEXT("No later sweep hit damages the bystander regardless of collision order"),
					Bystander.Attributes->GetHealth(), ParryObservation->BystanderHealth);
				CleanupParrySweep();
				ResetActors();
				Source.Actor->SetActorRotation(FRotator::ZeroRotator);
				Target.Actor->SetActorLocation(Source.Actor->GetActorLocation() + FVector(180.0f, 0.0f, 0.0f));
				Test->TestTrue(TEXT("Held block activates actual GAS path"),
					Source.ASC->RequestAbilityInput(GolemGameplayTags::Input_Combat_Block, true).IsSuccess());
				BeforeHealth = Source.Attributes->GetHealth();
				UGolemAbilitySystemComponent::ApplyCombatDamage(Target.Actor.Get(), Source.Actor.Get(), 20.0f, 20.0f, true);
				Test->TestEqual(TEXT("Front block scales actual execution damage"), BeforeHealth - Source.Attributes->GetHealth(),
					20.0f * Profile->GetTuning().BlockDamageFraction);
				Test->TestEqual(TEXT("Blocked hit consumes stamina through GE"), Source.Attributes->GetStamina(),
					1000.0f - Profile->GetTuning().BlockStaminaCost);
				Target.Actor->SetActorLocation(Source.Actor->GetActorLocation() - FVector(180.0f, 0.0f, 0.0f));
				BeforeHealth = Source.Attributes->GetHealth();
				UGolemAbilitySystemComponent::ApplyCombatDamage(Target.Actor.Get(), Source.Actor.Get(), 20.0f, 0.0f, true);
				Test->TestEqual(TEXT("Rear attack bypasses directional block"), BeforeHealth - Source.Attributes->GetHealth(), 20.0f);
				Source.ASC->RequestAbilityInput(GolemGameplayTags::Input_Combat_Block, false);
				Test->TestFalse(TEXT("Released block leaves no state"), Source.Combat->GetSnapshot().bBlocking);
				ResetActors();
				Target.Actor->SetActorLocation(Source.Actor->GetActorLocation() + FVector(180.0f, 0.0f, 0.0f));
				Press(GolemGameplayTags::Input_Combat_Parry);
				BeforeHealth = Source.Attributes->GetHealth();
				UGolemAbilitySystemComponent::ApplyCombatDamage(Target.Actor.Get(), Source.Actor.Get(), 20.0f, 20.0f, true);
				Test->TestEqual(TEXT("Timed front parry prevents health damage"), Source.Attributes->GetHealth(), BeforeHealth);
				Test->TestTrue(TEXT("Parry staggers the actual attacking actor"), Target.Combat->GetSnapshot().bStaggered);
				Test->TestTrue(TEXT("Parry has actual GAS cue notification"),
					Source.ASC->GetCueDiagnostics(GolemGameplayTags::GameplayCue_Parry).ExecutedCount > 0);
				Next(EGolemCombatTestStep::Defense);
				break;
			case EGolemCombatTestStep::Defense:
				if (Elapsed <= Profile->GetTuning().ParryWindow + 0.05f)
				{
					return false;
				}
				Test->TestFalse(TEXT("Parry state expires via actual ability timer"), Source.Combat->GetSnapshot().bParrying);
				ResetActors();
				Press(GolemGameplayTags::Input_Combat_Dodge);
				Test->TestTrue(TEXT("Dodge persists after release"), Source.Combat->GetSnapshot().bDodging);
				Test->TestEqual(TEXT("Dodge charges authored cost"), Source.Attributes->GetStamina(),
					1000.0f - Profile->GetTuning().DodgeStaminaCost);
				Next(EGolemCombatTestStep::Dodge);
				break;
			case EGolemCombatTestStep::Dodge:
				if (Elapsed <= Profile->GetTuning().DodgeInvulnerabilityStart + 0.01f)
				{
					return false;
				}
				BeforeHealth = Source.Attributes->GetHealth();
				UGolemAbilitySystemComponent::ApplyCombatDamage(Target.Actor.Get(), Source.Actor.Get(), 20.0f, 20.0f, true);
				Test->TestEqual(TEXT("Only timed dodge window prevents execution damage"), Source.Attributes->GetHealth(), BeforeHealth);
				Test->TestEqual(TEXT("Dodge confirms actual incoming avoided hit"),
					Source.Combat->GetSnapshot().GetOutcomeCount(EGolemCombatOutcome::Dodged), 1);
				Source.ASC->HandleControllerChanged();
				Test->TestFalse(TEXT("Owner change removes dodge immunity"), Source.Combat->GetSnapshot().bDodging);
				UGolemAbilitySystemComponent::ApplyCombatDamage(Target.Actor.Get(), Source.Actor.Get(), 20.0f, 0.0f, true);
				Test->TestEqual(TEXT("No immunity survives cancellation"), BeforeHealth - Source.Attributes->GetHealth(), 20.0f);
				ResetActors();
				BeforeHealth = Target.Attributes->GetHealth();
				BeforeSpeed = Target.Actor->GetCharacterMovement()->MaxWalkSpeed;
				Press(GolemGameplayTags::Input_Combat_Rune, Target.Actor.Get());
				Test->TestEqual(TEXT("Rune damage uses shared GAS execution"), BeforeHealth - Target.Attributes->GetHealth(),
					Profile->GetTuning().RuneDamage);
				Test->TestEqual(TEXT("Rune consumes authored energy via GE"), Source.Attributes->GetRuneEnergy(),
					100.0f - Profile->GetTuning().RuneEnergyCost);
				Test->TestTrue(TEXT("Rune applies actual timed GAS status"),
					Target.ASC->HasMatchingGameplayTag(GolemGameplayTags::State_Combat_RuneSlowed));
				Test->TestEqual(TEXT("Rune status changes generic target movement"),
					Target.Actor->GetCharacterMovement()->MaxWalkSpeed, BeforeSpeed * 0.5f);
				// Reproduce a late replicated hero baseline arriving after the actual rune GE and tag listener.
				BeforeSpeed = 450.0f;
				Target.Actor->GetCharacterMovement()->MaxWalkSpeed = BeforeSpeed;
				Test->TestTrue(TEXT("Late native movement baseline reapplies the active GAS status"),
					Target.Combat->RefreshBaseWalkSpeed(BeforeSpeed).IsSuccess());
				Test->TestEqual(TEXT("Late hero baseline remains slowed while the real GE is active"),
					Target.Actor->GetCharacterMovement()->MaxWalkSpeed, 225.0f);
				Test->TestEqual(TEXT("Nonfinite baseline is rejected without changing status presentation"),
					Target.Combat->RefreshBaseWalkSpeed(std::numeric_limits<float>::quiet_NaN()).Error,
					EGolemError::InvalidData);
				Test->TestEqual(TEXT("Rejected baseline preserves active slow and updated authored speed"),
					Target.Actor->GetCharacterMovement()->MaxWalkSpeed, 225.0f);
				Next(EGolemCombatTestStep::Rune);
				break;
			case EGolemCombatTestStep::Rune:
				if (Elapsed <= Profile->GetTuning().RuneSlowDuration + 0.1f)
				{
					return false;
				}
				Test->TestFalse(TEXT("Timed GE removes status without manual mutation"),
					Target.ASC->HasMatchingGameplayTag(GolemGameplayTags::State_Combat_RuneSlowed));
				Test->TestEqual(TEXT("Status expiry restores authored movement speed"),
					Target.Actor->GetCharacterMovement()->MaxWalkSpeed, BeforeSpeed);
				ResetActors();
				TestRejectedRuneStatus();
				ResetActors();
				Test->TestTrue(TEXT("Reset fixture starts with its validated combat target"),
					Source.Combat->GetLockedTarget() == Target.Actor.Get());
				Press(GolemGameplayTags::Input_Combat_LockOn, Target.Actor.Get());
				Test->TestTrue(TEXT("Lock input toggles the already-locked target off"),
					Source.Combat->GetLockedTarget() == nullptr);
				Test->TestEqual(TEXT("Unlock does not publish a new target-lock confirmation"),
					Source.Combat->GetSnapshot().GetOutcomeCount(EGolemCombatOutcome::TargetLocked), 0);
				Press(GolemGameplayTags::Input_Combat_LockOn, Target.Actor.Get());
				Test->TestTrue(TEXT("Lock validates hostile target"), Source.Combat->GetLockedTarget() == Target.Actor.Get());
				Test->TestEqual(TEXT("Actual lock acquisition publishes one confirmed target"),
					Source.Combat->GetSnapshot().GetOutcomeCount(EGolemCombatOutcome::TargetLocked), 1);
				{
					Wall = Context.World->SpawnActor<AActor>();
					UBoxComponent* Box = NewObject<UBoxComponent>(Wall.Get());
					Wall->AddInstanceComponent(Box);
					Wall->SetRootComponent(Box);
					Box->SetBoxExtent(FVector(10.0f, 200.0f, 200.0f));
					Box->SetCollisionProfileName(TEXT("BlockAll"));
					Box->RegisterComponent();
					Wall->SetActorLocation((Source.Actor->GetActorLocation() + Target.Actor->GetActorLocation()) * 0.5f);
				}
				Next(EGolemCombatTestStep::LockLOS);
				break;
			case EGolemCombatTestStep::LockLOS:
				if (Elapsed < 0.2f)
				{
					return false;
				}
				Test->TestTrue(TEXT("Real visibility obstruction invalidates target lock"), Source.Combat->GetLockedTarget() == nullptr);
				Context.StopPlay();
				Test->TestEqual(TEXT("EndPlay revokes only owned combat source"), Source.ASC->GetOwnedGrantCount(), 0);
				Test->TestFalse(TEXT("EndPlay cleans combat actions"), Source.Combat->GetSnapshot().bAttacking);
				Step = EGolemCombatTestStep::Finished;
				return true;
			default:
				return true;
			}
			return false;
		}

	private:
		bool PrepareParrySweep()
		{
			ResetActors();
			Bystander = SpawnGolem(Context.World.Get(), Target.Actor->GetActorLocation(), false);
			Bystander.Actor->GetCapsuleComponent()->SetCapsuleSize(140.0f, 150.0f);
			Bystander.Actor->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
			Bystander.Actor->GetCharacterMovement()->DisableMovement();
			if (!Test->TestTrue(TEXT("Second real enemy collider uses the same combat profile"),
				Bystander.Combat->Configure(Profile->GetPrimaryAssetId()).IsSuccess()))
			{
				return false;
			}
			TargetVisibility = Target.Actor->GetCapsuleComponent()->GetCollisionResponseToChannel(ECC_Visibility);
			Target.Actor->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
			Target.Actor->SetActorRotation((Source.Actor->GetActorLocation() - Target.Actor->GetActorLocation()).Rotation());
			ExtraParryCollider = NewObject<UBoxComponent>(Target.Actor.Get(), TEXT("GolemParryRegressionCollider"));
			Target.Actor->AddInstanceComponent(ExtraParryCollider.Get());
			ExtraParryCollider->SetupAttachment(Target.Actor->GetRootComponent());
			ExtraParryCollider->SetBoxExtent(FVector(140.0f, 140.0f, 150.0f));
			ExtraParryCollider->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
			ExtraParryCollider->SetCollisionObjectType(ECC_Pawn);
			ExtraParryCollider->SetCollisionResponseToAllChannels(ECR_Ignore);
			ExtraParryCollider->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
			ExtraParryCollider->RegisterComponent();
			ParryObservation = MakeShared<FGolemParrySweepObservation>();
			const TSharedRef<FGolemParrySweepObservation> Observation = ParryObservation.ToSharedRef();
			const TWeakObjectPtr<UGolemAttributeSet> TargetAttributes = Target.Attributes;
			const TWeakObjectPtr<UGolemAttributeSet> BystanderAttributes = Bystander.Attributes;
			const TWeakObjectPtr<UWorld> World = Context.World;
			ParryHandle = Target.Combat->OnCombatOutcome.AddLambda(
				[Observation, TargetAttributes, BystanderAttributes, World](const FGolemCombatOutcome& Event)
				{
					if (Event.Outcome == EGolemCombatOutcome::Parried && TargetAttributes.IsValid() &&
						BystanderAttributes.IsValid() && World.IsValid())
					{
						if (Observation->Count == 0)
						{
							Observation->TargetHealth = TargetAttributes->GetHealth();
							Observation->BystanderHealth = BystanderAttributes->GetHealth();
							Observation->WorldTime = World->GetTimeSeconds();
						}
						++Observation->Count;
					}
				});
			const FGolemAttackDefinition& Attack = FirstAttack(GolemGameplayTags::Ability_Combat_Primary);
			UAnimMontage* Montage = Attack.Montage.LoadSynchronous();
			float SectionStart = 0.0f;
			float SectionEnd = 0.0f;
			Montage->GetSectionStartAndEndTime(Montage->GetSectionIndex(Attack.SectionName), SectionStart, SectionEnd);
			ParryTriggerPosition = SectionStart;
			bool bFoundTrace = false;
			for (const FAnimNotifyEvent& Notify : Montage->Notifies)
			{
				if (Cast<UGolemAnimNotifyState_TraceWindow>(Notify.NotifyStateClass) != nullptr &&
					Notify.GetTriggerTime() >= SectionStart && Notify.GetTriggerTime() < SectionEnd)
				{
					ParryTriggerPosition = FMath::Max(SectionStart, Notify.GetTriggerTime() - 0.075f);
					bFoundTrace = true;
					break;
				}
			}
			if (!Test->TestTrue(TEXT("Reentrant regression uses an actual authored trace notify"), bFoundTrace))
			{
				return false;
			}
			Press(GolemGameplayTags::Input_Combat_Primary);
			StartParryBeforeTrace();
			return true;
		}
		void StartParryBeforeTrace()
		{
			const UAnimInstance* Animation = Source.Actor->GetMesh()->GetAnimInstance();
			if (!bSweepParryStarted && Animation != nullptr &&
				Animation->Montage_GetPosition(Source.ASC->GetCurrentMontage()) >= ParryTriggerPosition)
			{
				bSweepParryStarted = true;
				Test->TestTrue(TEXT("Actual target GAS parry starts before the real trace window"),
					Target.ASC->RequestAbilityInput(GolemGameplayTags::Input_Combat_Parry, true).IsSuccess());
				Target.ASC->RequestAbilityInput(GolemGameplayTags::Input_Combat_Parry, false);
			}
		}
		void CleanupParrySweep()
		{
			if (Target.Combat.IsValid() && ParryHandle.IsValid())
			{
				Target.Combat->OnCombatOutcome.Remove(ParryHandle);
			}
			ParryHandle.Reset();
			if (ExtraParryCollider.IsValid())
			{
				ExtraParryCollider->DestroyComponent();
				ExtraParryCollider.Reset();
				if (Target.Actor.IsValid())
				{
					Target.Actor->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, TargetVisibility);
				}
			}
			if (Bystander.Actor.IsValid())
			{
				Bystander.Actor->Destroy();
			}
		}
		void TestRejectedRuneStatus()
		{
			const TSharedRef<FGolemRuneApplicationObservation> Observation = MakeShared<FGolemRuneApplicationObservation>();
			FGameplayEffectApplicationQuery Query = FGameplayEffectApplicationQuery::CreateLambda(
				[Observation](const FActiveGameplayEffectsContainer& ActiveEffects, const FGameplayEffectSpec& Spec)
				{
					if (Spec.Def != nullptr && Spec.Def->IsA(UGolemRuneSlowEffect::StaticClass()))
					{
						++Observation->StatusRejected;
						return false;
					}
					if (Spec.Def != nullptr && Spec.Def->IsA(UGolemDamageEffect::StaticClass()))
					{
						++Observation->DamageAccepted;
					}
					return true;
				});
			const FDelegateHandle QueryHandle = Query.GetHandle();
			Target.ASC->GameplayEffectApplicationQueries.Add(MoveTemp(Query));
			ON_SCOPE_EXIT
			{
				if (Target.ASC.IsValid())
				{
					const int32 Index = Target.ASC->GameplayEffectApplicationQueries.IndexOfByPredicate(
						[QueryHandle](const FGameplayEffectApplicationQuery& Entry)
						{
							return Entry.GetHandle() == QueryHandle;
						});
					if (Index != INDEX_NONE)
					{
						Target.ASC->GameplayEffectApplicationQueries.RemoveAt(Index);
					}
				}
			};
			const float Health = Target.Attributes->GetHealth();
			const float Energy = Source.Attributes->GetRuneEnergy();
			const int32 Cues = Target.ASC->GetCueDiagnostics(GolemGameplayTags::GameplayCue_Rune).ExecutedCount;
			const int32 Confirmations = Source.Combat->GetSnapshot().GetOutcomeCount(EGolemCombatOutcome::RuneHit);
			const FGolemResult Result = Source.ASC->RequestAbilityInput(
				GolemGameplayTags::Input_Combat_Rune, true, Target.Actor.Get());
			Source.ASC->RequestAbilityInput(GolemGameplayTags::Input_Combat_Rune, false);
			Test->TestEqual(TEXT("Rejected real duration GE returns a typed rune failure"), Result.Error, EGolemError::InvalidState);
			Test->TestEqual(TEXT("Rejected rune reports the actual failed status operation"), Result.Context, FName(TEXT("RuneStatusRejected")));
			Test->TestEqual(TEXT("Application query allows the actual instant damage execution"), Observation->DamageAccepted, 1);
			Test->TestEqual(TEXT("Application query rejects only the actual RuneSlow GE"), Observation->StatusRejected, 1);
			Test->TestEqual(TEXT("Failure retains damage that already happened"), Health - Target.Attributes->GetHealth(),
				Profile->GetTuning().RuneDamage);
			Test->TestEqual(TEXT("Failure retains the committed energy cost without fake rollback"),
				Energy - Source.Attributes->GetRuneEnergy(), Profile->GetTuning().RuneEnergyCost);
			Test->TestFalse(TEXT("Rejected actual status cannot leave a slow tag"),
				Target.ASC->HasMatchingGameplayTag(GolemGameplayTags::State_Combat_RuneSlowed));
			Test->TestFalse(TEXT("Rejected rune cancels its ability-owned action state"),
				Source.ASC->HasMatchingGameplayTag(GolemGameplayTags::State_Combat_Attacking));
			Test->TestEqual(TEXT("Rejected status never confirms a complete RuneHit"),
				Source.Combat->GetSnapshot().GetOutcomeCount(EGolemCombatOutcome::RuneHit), Confirmations);
			Test->TestEqual(TEXT("Rejected status never emits the rune success cue"),
				Target.ASC->GetCueDiagnostics(GolemGameplayTags::GameplayCue_Rune).ExecutedCount, Cues);
		}
		const FGolemAttackDefinition& FirstAttack(FGameplayTag Tag) const
		{
			for (const FGolemAttackDefinition& Attack : Profile->GetAttacks())
			{
				if (Attack.ActionTag == Tag)
				{
					return Attack;
				}
			}
			return Profile->GetAttacks()[0];
		}
		void Press(FGameplayTag Input, AActor* TargetActor = nullptr)
		{
			Test->TestTrue(*FString::Printf(TEXT("Native GAS input %s accepted"), *Input.ToString()),
				Source.ASC->RequestAbilityInput(Input, true, TargetActor).IsSuccess());
			Source.ASC->RequestAbilityInput(Input, false);
		}
		void ResetActors()
		{
			Source.Combat->ResetForCheckpoint();
			Target.Combat->ResetForCheckpoint();
			Source.Life->ResetForCheckpoint();
			Target.Life->ResetForCheckpoint();
			Source.Actor->SetActorLocation(FVector(0.0f, 0.0f, 88.0f));
			Source.Actor->SetActorRotation(FRotator::ZeroRotator);
			Source.Actor->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
			Target.Actor->SetActorLocation(FVector(120.0f, 0.0f, 100.0f));
			Target.Actor->GetCharacterMovement()->DisableMovement();
			Source.Combat->SetValidatedTarget(Target.Actor.Get());
		}
		void Next(EGolemCombatTestStep InStep)
		{
			Step = InStep;
			Elapsed = 0.0f;
		}
		FAutomationTestBase* Test;
		FGolemCombatWorld Context;
		FGolemCombatFixture Source;
		FGolemCombatFixture Target;
		FGolemCombatFixture Bystander;
		TWeakObjectPtr<UBoxComponent> ExtraParryCollider;
		TSharedPtr<FGolemParrySweepObservation> ParryObservation;
		FDelegateHandle ParryHandle;
		ECollisionResponse TargetVisibility = ECR_Ignore;
		TWeakObjectPtr<AActor> Wall;
		TWeakObjectPtr<UGolemCombatProfileDefinition> Profile;
		EGolemCombatTestStep Step = EGolemCombatTestStep::Warmup;
		float Elapsed = 0.0f;
		float BeforeHealth = 0.0f;
		float BeforeSpeed = 0.0f;
		float ParryTriggerPosition = 0.0f;
		int32 BufferedIndex = 0;
		bool bReady = false;
		bool bObservedFlinch = false;
		bool bSweepParryStarted = false;
		double Started = FPlatformTime::Seconds();
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGolemCombatMechanicsTest, "Golem.Combat.NativeMechanics",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGolemCombatMechanicsTest::RunTest(const FString& Parameters)
{
	ADD_LATENT_AUTOMATION_COMMAND(GolemCombatTests::FGolemCombatMechanicsCommand(this));
	return true;
}

namespace GolemCombatTests
{
	class FGolemProjectionCombatCommand final : public IAutomationLatentCommand
	{
	public:
		explicit FGolemProjectionCombatCommand(FAutomationTestBase* InTest)
			: Test(InTest)
		{
			Source = SpawnGolem(Context.World.Get(), FVector(0, 0, 88), true);
			Target = SpawnGolem(Context.World.Get(), FVector(120, 0, 100), false);
			Target.Actor->GetCapsuleComponent()->SetCapsuleSize(140.0f, 150.0f);
			Target.Actor->GetCharacterMovement()->DisableMovement();
			const FPrimaryAssetId Id(TEXT("KnightCombatProfile"), TEXT("FoundationCombat"));
			bReady = Test->TestTrue(TEXT("Projected source uses real combat fixture"), Source.Combat->Configure(Id).IsSuccess())
				&& Test->TestTrue(TEXT("Projected target uses shared combat fixture"), Target.Combat->Configure(Id).IsSuccess());
			if (bReady)
			{
				Profile = Source.Combat->GetProfile();
				for (const FGameplayAbilitySpec& Spec : Source.ASC->GetActivatableAbilities())
				{
					if (Spec.GetDynamicSpecSourceTags().HasTagExact(GolemGameplayTags::Input_Combat_Rune))
					{
						RuneSpec = Spec.Handle;
					}
				}
			}
		}

		virtual bool Update() override
		{
			if (!bReady)
			{
				return true;
			}
			if (FPlatformTime::Seconds() - Started > 35.0)
			{
				Test->AddError(TEXT("Projected combat real-world timing exceeded its bound."));
				return true;
			}
			if (LastFrame == GFrameCounter)
			{
				return false;
			}
			LastFrame = GFrameCounter;
			Context.World->Tick(LEVELTICK_All, 0.025f);
			Elapsed += 0.025f;
			if (Step == 0 && Elapsed >= 0.1f)
			{
				Before = Target.Attributes->GetHealth();
				Press(GolemGameplayTags::Input_Combat_Primary);
				Step = 1;
			}
			else if (Step == 1 && !Source.Combat->GetSnapshot().bAttacking)
			{
				const FGolemAttackDefinition& Attack = Profile->GetAttacks()[0];
				Test->TestEqual(TEXT("No-projection real trace retains authored damage"), Before - Target.Attributes->GetHealth(), Attack.Damage);
				Reset();
				Before = Target.Attributes->GetHealth();
				const float Energy = Source.Attributes->GetRuneEnergy();
				Press(GolemGameplayTags::Input_Combat_Rune);
				Test->TestEqual(TEXT("Baseline real Rune damage unchanged"), Before - Target.Attributes->GetHealth(), Profile->GetTuning().RuneDamage);
				Test->TestEqual(TEXT("Baseline real Rune GE cost unchanged"), Energy - Source.Attributes->GetRuneEnergy(), Profile->GetTuning().RuneEnergyCost);
				Reset();
				FGolemGASProjectionPlan Plan;
				Plan.SourceKey = TEXT("RPG");
				Plan.DefinitionId = FPrimaryAssetId(TEXT("KnightHero"), TEXT("Training_Red"));
				Plan.AttackPowerBonus = 10.0f;
				Plan.MaxHealthBonus = 50.0f;
				FGolemActionModifier Modifier;
				Modifier.AbilityTag = GolemGameplayTags::Ability_Combat_Rune;
				Modifier.DamageScale = 1.5f;
				Modifier.CostScale = 0.5f;
				Modifier.DurationScale = 2.0f;
				Plan.ActionModifiers.Add(Modifier);
				FGolemMutationFence Fence;
				FGolemProjectionCapability Capability;
				bReady = Test->TestTrue(TEXT("Combat projection acquires fence"), Source.ASC->BeginMutationFence(FGuid::NewGuid(), Fence).IsSuccess())
					&& Test->TestTrue(TEXT("Combat projection validates original Rune action"), Source.ASC->PrepareSourceProjection(Fence, Plan, 0, Capability).IsSuccess())
					&& Test->TestTrue(TEXT("Combat projection stages real GE"), Source.ASC->StageSourceProjection(Fence, Capability).IsSuccess())
					&& Test->TestTrue(TEXT("Combat projection commits real source"), Source.ASC->CommitSourceProjection(Fence, Capability).IsSuccess())
					&& Test->TestTrue(TEXT("Combat projection publishes final modifiers"), Source.ASC->PublishSourceProjection(Fence, Capability).IsSuccess())
					&& Test->TestTrue(TEXT("Combat projection releases fence"), Source.ASC->EndMutationFence(Fence).IsSuccess());
				if (!bReady)
				{
					return true;
				}
				Test->TestEqual(TEXT("Projection adds no duplicate Rune or other combat spec"), Source.ASC->GetActivatableAbilities().Num(), 9);
				Test->TestNotNull(TEXT("Original exact Rune spec remains"), Source.ASC->FindAbilitySpecFromHandle(RuneSpec));
				Test->TestEqual(TEXT("Equip does not heal"), Source.Attributes->GetHealth(), 1000.0f);
				Before = Target.Attributes->GetHealth();
				Press(GolemGameplayTags::Input_Combat_Primary);
				Step = 2;
			}
			else if (Step == 2 && !Source.Combat->GetSnapshot().bAttacking)
			{
				Test->TestEqual(TEXT("Actual notify trace consumes AP delta once"),
					Before - Target.Attributes->GetHealth(), Profile->GetAttacks()[0].Damage + 10.0f);
				Test->TestEqual(TEXT("Projected trace window still hits once"),
					Source.Combat->GetSnapshot().GetOutcomeCount(EGolemCombatOutcome::LightHit), 1);
				Reset();
				Before = Target.Attributes->GetHealth();
				const float Energy = Source.Attributes->GetRuneEnergy();
				Press(GolemGameplayTags::Input_Combat_Rune);
				Test->TestEqual(TEXT("Same real Rune uses AP delta and committed damage multiplier"),
					Before - Target.Attributes->GetHealth(), (Profile->GetTuning().RuneDamage + 10.0f) * 1.5f);
				Test->TestEqual(TEXT("Same real Rune uses discounted resource GE"),
					Energy - Source.Attributes->GetRuneEnergy(), Profile->GetTuning().RuneEnergyCost * 0.5f);
				bool bRealDuration = false;
				for (FActiveGameplayEffectHandle Handle : Target.ASC->GetActiveEffects(FGameplayEffectQuery()))
				{
					const FActiveGameplayEffect* Effect = Target.ASC->GetActiveGameplayEffect(Handle);
					if (Effect != nullptr && Effect->Spec.Def->IsA<UGolemRuneSlowEffect>())
					{
						bRealDuration = true;
						Test->TestEqual(TEXT("Real status spec carries projected duration"),
							Effect->Spec.GetDuration(), Profile->GetTuning().RuneSlowDuration * 2.0f);
					}
				}
				Test->TestTrue(TEXT("Real projected slow effect exists"), bRealDuration);
				Elapsed = 0.0f;
				Step = 3;
			}
			else if (Step == 3)
			{
				if (Elapsed < Profile->GetTuning().RuneSlowDuration * 2.0f - 0.1f)
				{
					Test->TestTrue(TEXT("Projected slow remains during its real extended lifetime"),
						Target.ASC->HasMatchingGameplayTag(GolemGameplayTags::State_Combat_RuneSlowed));
					return false;
				}
				if (Elapsed < Profile->GetTuning().RuneSlowDuration * 2.0f + 0.2f)
				{
					return false;
				}
				Test->TestFalse(TEXT("Real extended slow eventually expires"),
					Target.ASC->HasMatchingGameplayTag(GolemGameplayTags::State_Combat_RuneSlowed));
				Test->TestNotNull(TEXT("Original Rune spec survives lifetime and retry"), Source.ASC->FindAbilitySpecFromHandle(RuneSpec));
				Test->TestEqual(TEXT("No additional source grants were fabricated"), Source.ASC->GetOwnedGrantCount(), 1);
				return true;
			}
			return false;
		}

	private:
		void Press(FGameplayTag Input)
		{
			Test->TestTrue(TEXT("Actual shared GAS action executes"), Source.ASC->RequestAbilityInput(Input, true, Target.Actor.Get()).IsSuccess());
			Source.ASC->RequestAbilityInput(Input, false);
		}
		void Reset()
		{
			Source.Combat->ResetForCheckpoint();
			Target.Combat->ResetForCheckpoint();
			Source.Life->ResetForCheckpoint();
			Target.Life->ResetForCheckpoint();
			Source.Actor->SetActorLocation(FVector(0, 0, 88));
			Source.Actor->SetActorRotation(FRotator::ZeroRotator);
			Source.Actor->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
			Target.Actor->SetActorLocation(FVector(120, 0, 100));
			Target.Actor->GetCharacterMovement()->DisableMovement();
			Source.Combat->SetValidatedTarget(Target.Actor.Get());
		}
		FAutomationTestBase* Test;
		FGolemCombatWorld Context;
		FGolemCombatFixture Source;
		FGolemCombatFixture Target;
		TWeakObjectPtr<UGolemCombatProfileDefinition> Profile;
		FGameplayAbilitySpecHandle RuneSpec;
		uint64 LastFrame = MAX_uint64;
		int32 Step = 0;
		float Elapsed = 0.0f;
		float Before = 0.0f;
		bool bReady = false;
		double Started = FPlatformTime::Seconds();
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGolemCombatProjectionTest, "Golem.Combat.RPGProjection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGolemCombatProjectionTest::RunTest(const FString& Parameters)
{
	ADD_LATENT_AUTOMATION_COMMAND(GolemCombatTests::FGolemProjectionCombatCommand(this));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGolemEquipmentBulwarkTest, "Golem.Equipment.Combat.BulwarkConfirmedBlockParry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGolemEquipmentBulwarkTest::RunTest(const FString& Parameters)
{
	using namespace GolemCombatTests;
	FGolemCombatWorld Context;
	const FGolemCombatFixture Source = SpawnGolem(Context.World.Get(), FVector(0, 0, 88), true);
	const FGolemCombatFixture Enemy = SpawnGolem(Context.World.Get(), FVector(180, 0, 88), false);
	const FPrimaryAssetId ProfileId(TEXT("KnightCombatProfile"), TEXT("FoundationCombat"));
	if (!TestTrue(TEXT("Actual combat profile and GAS actions configure"), Source.Combat->Configure(ProfileId).IsSuccess())
		|| !TestTrue(TEXT("Enemy uses actual shared combat"), Enemy.Combat->Configure(ProfileId).IsSuccess()))
	{
		return false;
	}
	Source.ASC->SetNumericAttributeBase(UGolemAttributeSet::GetAttackPowerAttribute(), 100.0f);
	Source.ASC->SetNumericAttributeBase(UGolemAttributeSet::GetDefenseAttribute(), 0.0f);
	Source.ASC->SetNumericAttributeBase(UGolemAttributeSet::GetVersatilityAttribute(), 0.20f);
	Source.ASC->SetNumericAttributeBase(UGolemAttributeSet::GetMasteryBonusAttribute(), 0.30f);
	Source.ASC->SetNumericAttributeBase(UGolemAttributeSet::GetCritChanceAttribute(), 0.40f);
	UGolemSkillRuntimeComponent* Runtime = NewObject<UGolemSkillRuntimeComponent>(Source.Actor.Get());
	Source.Actor->AddInstanceComponent(Runtime);
	Runtime->RegisterComponent();
	FGolemEquipmentCombatBuild Gear;
	Gear.bEnabled = true;
	Gear.bHasHammer = true;
	Gear.RelicEffectId = TEXT("Bulwark");
	Gear.RelicInstanceId = FGuid::NewGuid();
	TestTrue(TEXT("Bulwark exact instance prepares"), Runtime->PrepareEquipmentBuild(Gear).IsSuccess());
	Runtime->CommitPreparedEquipmentBuild();
	Runtime->PublishPreparedBuild();
	TestTrue(TEXT("Actual GAS block starts"), Source.ASC->RequestAbilityInput(GolemGameplayTags::Input_Combat_Block, true).IsSuccess());
	TestTrue(TEXT("Actual damage execution resolves a direct hostile blocked hit"),
		UGolemAbilitySystemComponent::ApplyCombatDamage(Enemy.Actor.Get(), Source.Actor.Get(), 20.0f, 20.0f,
			true, nullptr, false, FGuid::NewGuid()).IsSuccess());
	const float BlockedDamage = 20.0f * Source.Combat->GetProfile()->GetTuning().BlockDamageFraction;
	TestTrue(TEXT("Bulwark creates .60 Power shield and the confirmed hit consumes its blocked portion"),
		FMath::IsNearlyEqual(Runtime->GetSkillState().Shield, 60.0f - BlockedDamage, 0.01f));
	TestEqual(TEXT("Blocked hit is absorbed without HP loss"), Source.Attributes->GetHealth(), 1000.0f);
	TestTrue(TEXT("Confirmed defense starts the twelve-second owner-family ICD"),
		FMath::IsNearlyEqual(Runtime->CaptureSnapshot().BulwarkRemaining, 12.0f, 0.01f));
	UGolemAbilitySystemComponent::ApplyCombatDamage(Enemy.Actor.Get(), Source.Actor.Get(), 20.0f, 0.0f,
		true, nullptr, false, FGuid::NewGuid());
	TestTrue(TEXT("Further blocks during ICD consume the existing pool without refreshing it"),
		FMath::IsNearlyEqual(Runtime->GetSkillState().Shield, 60.0f - 2.0f * BlockedDamage, 0.01f));
	Source.ASC->RequestAbilityInput(GolemGameplayTags::Input_Combat_Block, false);
	const uint64 OldGeneration = Runtime->GetItemGeneration();
	FGolemEquipmentCombatBuild Empty = Gear;
	Empty.RelicEffectId = NAME_None;
	Empty.RelicInstanceId.Invalidate();
	TestTrue(TEXT("Exact relic source withdrawal prepares"), Runtime->PrepareEquipmentBuild(Empty).IsSuccess());
	Runtime->CommitPreparedEquipmentBuild();
	Runtime->PublishPreparedBuild();
	TestEqual(TEXT("Unequip cleans only the old item shield"), Runtime->GetSkillState().Shield, 0.0f);
	Gear.RelicInstanceId = FGuid::NewGuid();
	Runtime->PrepareEquipmentBuild(Gear);
	Runtime->CommitPreparedEquipmentBuild();
	Runtime->PublishPreparedBuild();
	TestTrue(TEXT("New instance retains the same owner's family ICD"),
		FMath::IsNearlyEqual(Runtime->CaptureSnapshot().BulwarkRemaining, 12.0f, 0.01f));
	Runtime->NotifyItemDefenseSuccess(Enemy.Actor.Get(), FGuid::NewGuid(), true, OldGeneration);
	TestEqual(TEXT("Stale source generation cannot create an item effect"), Runtime->GetSkillState().Shield, 0.0f);
	Runtime->ResetForCheckpoint();
	TestTrue(TEXT("Retry does not reset the family ICD"),
		FMath::IsNearlyEqual(Runtime->CaptureSnapshot().BulwarkRemaining, 12.0f, 0.01f));
	FGolemSkillRuntimeSnapshot Cleared = Runtime->CaptureSnapshot();
	Cleared.BulwarkRemaining = 0.0f;
	TestTrue(TEXT("Test reconstruction installs an elapsed ICD fragment"), Runtime->RestoreSnapshot(Cleared).IsSuccess());
	TestTrue(TEXT("Actual shared GAS parry starts"), Source.ASC->RequestAbilityInput(GolemGameplayTags::Input_Combat_Parry, true).IsSuccess());
	UGolemAbilitySystemComponent::ApplyCombatDamage(Enemy.Actor.Get(), Source.Actor.Get(), 20.0f, 20.0f,
		true, nullptr, false, FGuid::NewGuid());
	TestEqual(TEXT("Parry confirms zero incoming HP damage and creates the full Bulwark pool"), Runtime->GetSkillState().Shield, 60.0f);
	TestEqual(TEXT("Actual combat reports a successful parry"), Source.Combat->GetSnapshot().GetOutcomeCount(EGolemCombatOutcome::Parried), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGolemEquipmentHammerCombatTest, "Golem.Equipment.Combat.EmptyHammerRejectsLightHeavy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGolemEquipmentHammerCombatTest::RunTest(const FString& Parameters)
{
	using namespace GolemCombatTests;
	FGolemCombatWorld Context;
	const FGolemCombatFixture Source = SpawnGolem(Context.World.Get(), FVector(0, 0, 88), true);
	if (!TestTrue(TEXT("Actual combat profile grants real light and heavy GAS actions"),
		Source.Combat->Configure(FPrimaryAssetId(TEXT("KnightCombatProfile"), TEXT("FoundationCombat"))).IsSuccess()))
	{
		return false;
	}
	UGolemSkillRuntimeComponent* Runtime = NewObject<UGolemSkillRuntimeComponent>(Source.Actor.Get());
	Source.Actor->AddInstanceComponent(Runtime);
	Runtime->RegisterComponent();
	FGolemEquipmentCombatBuild Empty;
	Empty.bEnabled = true;
	TestTrue(TEXT("M56 empty weapon descriptor prepares"), Runtime->PrepareEquipmentBuild(Empty).IsSuccess());
	Runtime->CommitPreparedEquipmentBuild();
	Runtime->PublishPreparedBuild();
	int32 Commits = 0;
	const FDelegateHandle Handle = Source.ASC->AbilityCommittedCallbacks.AddLambda([&Commits](UGameplayAbility*)
	{
		++Commits;
	});
	const FGameplayTag AttackInputs[] { GolemGameplayTags::Input_Combat_Primary, GolemGameplayTags::Input_Combat_Heavy };
	for (FGameplayTag Input : AttackInputs)
	{
		Source.ASC->RequestAbilityInput(Input, true);
		Source.ASC->RequestAbilityInput(Input, false);
	}
	TestEqual(TEXT("RequiresHammer rejects before real GAS commit"), Commits, 0);
	TestEqual(TEXT("RequiresHammer rejects before stamina spend"), Source.Attributes->GetStamina(), 1000.0f);
	TestFalse(TEXT("Rejected attacks leave no active montage or attack state"), Source.Combat->GetSnapshot().bAttacking);
	TestTrue(TEXT("An empty weapon still permits actual shared block"),
		Source.ASC->RequestAbilityInput(GolemGameplayTags::Input_Combat_Block, true).IsSuccess());
	TestTrue(TEXT("Allowed block reaches confirmed combat state"), Source.Combat->GetSnapshot().bBlocking);
	Source.ASC->RequestAbilityInput(GolemGameplayTags::Input_Combat_Block, false);
	Source.ASC->AbilityCommittedCallbacks.Remove(Handle);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGolemEquipmentPoiseSnapshotTest, "Golem.Equipment.Combat.PoiseSnapshotPoolValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGolemEquipmentPoiseSnapshotTest::RunTest(const FString& Parameters)
{
	using namespace GolemCombatTests;
	FGolemCombatWorld Context;
	const FGolemCombatFixture Original = SpawnGolem(Context.World.Get(), FVector(0, 0, 88), true);
	const FGolemCombatFixture Restored = SpawnGolem(Context.World.Get(), FVector(0, 300, 88), true);
	const FGolemCombatFixture Enemy = SpawnGolem(Context.World.Get(), FVector(180, 0, 88), false);
	const FPrimaryAssetId ProfileId(TEXT("KnightCombatProfile"), TEXT("FoundationCombat"));
	if (!TestTrue(TEXT("Original combat pool uses the real authored profile"), Original.Combat->Configure(ProfileId).IsSuccess())
		|| !TestTrue(TEXT("Fresh reconstructed pool uses the same authored profile"), Restored.Combat->Configure(ProfileId).IsSuccess()))
	{
		return false;
	}
	const float InitialMax = Original.Combat->GetMaxPoise();
	TestTrue(TEXT("Real GAS hit consumes the combat-owned Poise pool"), UGolemAbilitySystemComponent::ApplyCombatDamage(
		Enemy.Actor.Get(), Original.Actor.Get(), 10.0f, 20.0f, false, nullptr, false, FGuid::NewGuid()).IsSuccess());
	const float Saved = Original.Combat->GetSnapshot().Poise;
	TestEqual(TEXT("Actual hit removes twenty Poise"), Saved, InitialMax - 20.0f);
	Original.ASC->SetNumericAttributeBase(UGolemAttributeSet::GetPoiseBonusAttribute(), 10.0f);
	Restored.ASC->SetNumericAttributeBase(UGolemAttributeSet::GetPoiseBonusAttribute(), 10.0f);
	TestEqual(TEXT("Gear extends maximum Poise without refilling the consumed pool"), Original.Combat->GetSnapshot().Poise, Saved);
	TestEqual(TEXT("Effective maximum includes exactly one gear Poise bonus"), Restored.Combat->GetMaxPoise(), InitialMax + 10.0f);
	int32 Publications = 0;
	float PublishedPoise = -1.0f;
	const FDelegateHandle Handle = Restored.Combat->OnCombatSnapshotChanged.AddLambda(
		[&Publications, &PublishedPoise](const FGolemCombatSnapshot& Snapshot)
		{
			++Publications;
			PublishedPoise = Snapshot.Poise;
		});
	TestTrue(TEXT("Fresh reconstruction restores the consumed pool instead of the baseline"), Restored.Combat->RestoreSnapshotPoise(Saved).IsSuccess());
	TestEqual(TEXT("Confirmed pool remains the captured absolute value"), Restored.Combat->GetSnapshot().Poise, Saved);
	TestEqual(TEXT("One real snapshot publication observes the restored value"), Publications, 1);
	TestEqual(TEXT("Published snapshot contains actual restored pool"), PublishedPoise, Saved);
	const int32 Revision = Restored.Combat->GetSnapshot().Revision;
	for (float Invalid : { -1.0f, InitialMax + 11.0f, std::numeric_limits<float>::quiet_NaN() })
	{
		TestEqual(TEXT("Out-of-range and nonfinite pool fragments reject"), Restored.Combat->RestoreSnapshotPoise(Invalid).Error, EGolemError::InvalidData);
	}
	TestEqual(TEXT("Rejected reconstruction never mutates the pool"), Restored.Combat->GetSnapshot().Poise, Saved);
	TestEqual(TEXT("Rejected reconstruction never advances revision"), Restored.Combat->GetSnapshot().Revision, Revision);
	TestEqual(TEXT("Rejected reconstruction never publishes success"), Publications, 1);
	Restored.Combat->OnCombatSnapshotChanged.Remove(Handle);
	return true;
}

namespace GolemCombatTests
{
	struct FGolemPoisePublicationObservation
	{
		int32 Publications = 0;
		bool bPublishedWhileFenced = false;
		float PublishedMaximum = 0.0f;
		float PublishedPool = 0.0f;
		int64 PublishedSourceRevision = 0;
	};

	class FGolemPoiseFenceCommand final : public IAutomationLatentCommand
	{
	public:
		explicit FGolemPoiseFenceCommand(FAutomationTestBase* InTest)
			: Test(InTest), Observation(MakeShared<FGolemPoisePublicationObservation>())
		{
			Source = SpawnGolem(Context.World.Get(), FVector(0, 0, 88), true);
			bReady = Test->TestTrue(TEXT("Poise observer uses the actual authored combat profile"),
				Source.Combat->Configure(FPrimaryAssetId(TEXT("KnightCombatProfile"), TEXT("FoundationCombat"))).IsSuccess());
			BaseMaximum = Source.Combat->GetMaxPoise();
			if (bReady)
			{
				bReady = Stage(30.0f, 0)
					&& Test->TestTrue(TEXT("Initial real Poise source commits"), Source.ASC->CommitSourceProjection(Fence, Capability).IsSuccess())
					&& Test->TestTrue(TEXT("Initial real Poise source publishes"), Source.ASC->PublishSourceProjection(Fence, Capability).IsSuccess())
					&& Test->TestTrue(TEXT("Initial source fence ends"), Source.ASC->EndMutationFence(Fence).IsSuccess());
			}
		}

		virtual ~FGolemPoiseFenceCommand() override
		{
			if (Source.Combat.IsValid())
			{
				Source.Combat->OnCombatSnapshotChanged.Remove(ObserverHandle);
			}
			if (Source.ASC.IsValid() && Source.ASC->IsMutationFenced())
			{
				Source.ASC->AbortSourceProjection(Fence, Capability);
				Source.ASC->EndMutationFence(Fence);
			}
		}

		virtual bool Update() override
		{
			if (!bReady)
			{
				return true;
			}
			Context.World->Tick(LEVELTICK_All, 0.025f);
			if (Step == 0)
			{
				Test->TestTrue(TEXT("An actual extended Poise pool is available before replacement"),
					Source.Combat->RestoreSnapshotPoise(BaseMaximum + 25.0f).IsSuccess());
				const TWeakObjectPtr<UGolemAbilitySystemComponent> ASC = Source.ASC;
				const TWeakObjectPtr<UGolemCombatComponent> Combat = Source.Combat;
				ObserverHandle = Source.Combat->OnCombatSnapshotChanged.AddLambda(
					[ASC, Combat, Observed = Observation](const FGolemCombatSnapshot& Snapshot)
					{
						++Observed->Publications;
						Observed->bPublishedWhileFenced |= ASC->IsMutationFenced();
						Observed->PublishedMaximum = Combat->GetMaxPoise();
						Observed->PublishedPool = Snapshot.Poise;
						Observed->PublishedSourceRevision = ASC->GetSourceProjection(GolemProjection::RPGSourceKey).Revision;
					});
				bReady = Stage(10.0f, 1);
				Test->TestEqual(TEXT("Staging exposes both real GE contributions only behind the mutation fence"),
					Source.Combat->GetMaxPoise(), BaseMaximum + 40.0f);
				Test->TestEqual(TEXT("No confirmed combat observer runs during double-source staging"), Observation->Publications, 0);
				++Step;
				return false;
			}
			if (Step == 1)
			{
				Test->TestEqual(TEXT("An intervening world tick cannot publish the staged Poise aggregate"), Observation->Publications, 0);
				bReady = Test->TestTrue(TEXT("Replacement source commits behind its owning fence"), Source.ASC->CommitSourceProjection(Fence, Capability).IsSuccess())
					&& Test->TestTrue(TEXT("Publication retires the old real Poise effect"), Source.ASC->PublishSourceProjection(Fence, Capability).IsSuccess());
				Test->TestEqual(TEXT("Final Poise maximum includes only the committed replacement"), Source.Combat->GetMaxPoise(), BaseMaximum + 10.0f);
				Test->TestEqual(TEXT("Reduced maximum clamps the real pool without a fenced callback"), Source.Combat->GetSnapshot().Poise, BaseMaximum + 10.0f);
				Test->TestEqual(TEXT("Source retirement still publishes no confirmed combat callback under fence"), Observation->Publications, 0);
				bReady &= Test->TestTrue(TEXT("The committed source fence ends before combat publication"), Source.ASC->EndMutationFence(Fence).IsSuccess());
				++Step;
				return false;
			}
			if (Step == 2)
			{
				Test->TestEqual(TEXT("Exactly one deferred combat snapshot follows committed source order"), Observation->Publications, 1);
				Test->TestFalse(TEXT("Confirmed Poise publication never observes an active mutation fence"), Observation->bPublishedWhileFenced);
				Test->TestEqual(TEXT("Observer receives the final effective maximum"), Observation->PublishedMaximum, BaseMaximum + 10.0f);
				Test->TestEqual(TEXT("Observer receives the final clamped pool"), Observation->PublishedPool, BaseMaximum + 10.0f);
				Test->TestEqual(TEXT("Observer receives the committed source revision"), Observation->PublishedSourceRevision, int64(2));
				bReady = Stage(50.0f, 2)
					&& Test->TestTrue(TEXT("A rejected real candidate aborts its exact source"), Source.ASC->AbortSourceProjection(Fence, Capability).IsSuccess())
					&& Test->TestTrue(TEXT("Rollback ends the owning source fence"), Source.ASC->EndMutationFence(Fence).IsSuccess());
				Test->TestEqual(TEXT("Rollback cannot emit an intermediate confirmed combat snapshot"), Observation->Publications, 1);
				++Step;
				return false;
			}
			if (Step == 3)
			{
				Test->TestEqual(TEXT("Rollback coalesces to one final confirmed publication"), Observation->Publications, 2);
				Test->TestEqual(TEXT("Rollback preserves the prior committed maximum"), Observation->PublishedMaximum, BaseMaximum + 10.0f);
				Test->TestEqual(TEXT("Rollback preserves the consumed Poise pool"), Observation->PublishedPool, BaseMaximum + 10.0f);
				Test->TestEqual(TEXT("Rollback retains the existing source revision"), Observation->PublishedSourceRevision, int64(2));
				++Step;
				return false;
			}
			Test->TestEqual(TEXT("Further ticks do not repeat the deferred publication"), Observation->Publications, 2);
			return true;
		}

	private:
		bool Stage(float PoiseBonus, int64 ExpectedRevision)
		{
			FGolemGASProjectionPlan Plan;
			Plan.SourceKey = GolemProjection::RPGSourceKey;
			Plan.DefinitionId = FPrimaryAssetId(TEXT("KnightHero"), TEXT("Training_Red"));
			Plan.MaxPoiseBonus = PoiseBonus;
			return Test->TestTrue(TEXT("Poise transaction obtains an actual ASC mutation fence"), Source.ASC->BeginMutationFence(FGuid::NewGuid(), Fence).IsSuccess())
				&& Test->TestTrue(TEXT("Poise transaction prepares its exact source revision"), Source.ASC->PrepareSourceProjection(Fence, Plan, ExpectedRevision, Capability).IsSuccess())
				&& Test->TestTrue(TEXT("Poise transaction stages a native stat GE"), Source.ASC->StageSourceProjection(Fence, Capability).IsSuccess());
		}
		FAutomationTestBase* Test;
		FGolemCombatWorld Context;
		FGolemCombatFixture Source;
		TSharedRef<FGolemPoisePublicationObservation> Observation;
		FGolemMutationFence Fence;
		FGolemProjectionCapability Capability;
		FDelegateHandle ObserverHandle;
		float BaseMaximum = 0.0f;
		int32 Step = 0;
		bool bReady = false;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGolemEquipmentPoiseFenceTest, "Golem.Equipment.Combat.PoiseProjectionObserverFence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGolemEquipmentPoiseFenceTest::RunTest(const FString& Parameters)
{
	ADD_LATENT_AUTOMATION_COMMAND(GolemCombatTests::FGolemPoiseFenceCommand(this));
	return true;
}

#endif
