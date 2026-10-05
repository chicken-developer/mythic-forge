#include "Components/KnightCombatComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Abilities/KnightRuneSlowEffect.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimInstance.h"
#include "Animation/KnightAnimNotifyState_TraceWindow.h"
#include "Assets/KnightAssetManager.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/KnightAbilitySystemComponent.h"
#include "Components/KnightLifeComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Effects/KnightDamageEffect.h"
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
#include "Tags/KnightGameplayTags.h"
#include "UObject/StrongObjectPtr.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKnightCombatDefinitionTest, "Knight.Combat.DefinitionContracts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FKnightCombatDefinitionTest::RunTest(const FString& Parameters)
{
	UKnightAssetManager* Manager = UKnightAssetManager::GetKnightAssetManager();
	UKnightDefinition* Definition = nullptr;
	if (!TestNotNull(TEXT("Project asset manager exists"), Manager) ||
		!TestTrue(TEXT("Real combat fixture resolves by stable ID"), Manager->ResolveDefinition(
			FPrimaryAssetId(TEXT("KnightCombatProfile"), TEXT("FoundationCombat")),
			UKnightCombatProfileDefinition::StaticClass(), Definition).IsSuccess()))
	{
		return false;
	}
	const UKnightCombatProfileDefinition* Profile = CastChecked<UKnightCombatProfileDefinition>(Definition);
	TestTrue(TEXT("Actual skeleton, sections and notify windows validate"), Profile->ValidateCombatProfile().IsSuccess());
	UKnightDefinition* Set = nullptr;
	TestTrue(TEXT("Combat source set has a resolvable persisted identity"),
		Manager->ResolveDefinition(Profile->GetCombatAbilitySetId(), UKnightAbilitySetDefinition::StaticClass(), Set).IsSuccess());
	const TStrongObjectPtr<UKnightCombatProfileDefinition> Edited(
		DuplicateObject<UKnightCombatProfileDefinition>(Profile, GetTransientPackage()));
	FKnightCombatTuning Invalid = Edited->GetTuning();
	Invalid.ParryWindow = std::numeric_limits<float>::quiet_NaN();
	TestEqual(TEXT("Nonfinite defense timing rejected"), Edited->TrySetTuning(Invalid).Error, EKnightError::InvalidData);
	TestEqual(TEXT("Rejected tuning preserves authored timing"), Edited->GetTuning().ParryWindow,
		Profile->GetTuning().ParryWindow);
	TestEqual(TEXT("Foreign definition type rejected for GAS source"),
		Edited->TrySetCombatAbilitySetId(FPrimaryAssetId(TEXT("KnightHero"), TEXT("Training_Red"))).Error,
		EKnightError::InvalidData);
	TArray<FKnightAttackDefinition> Attacks = Edited->GetAttacks();
	const TStrongObjectPtr<UAnimMontage> MissingNotifies(
		DuplicateObject<UAnimMontage>(Attacks[0].Montage.LoadSynchronous(), GetTransientPackage()));
	MissingNotifies->Notifies.Reset();
	Attacks[0].Montage = MissingNotifies.Get();
	TestEqual(TEXT("Valid animation without weapon notify cannot masquerade as an attack"),
		Edited->TrySetAttacks(Attacks).Error, EKnightError::InvalidData);
	TestTrue(TEXT("Failed attack edit preserves original montage"),
		Edited->GetAttacks()[0].Montage == Profile->GetAttacks()[0].Montage);
	Attacks = Profile->GetAttacks();
	TArray<int32> PrimaryIndices;
	for (int32 Index = 0; Index < Attacks.Num(); ++Index)
	{
		if (Attacks[Index].ActionTag == KnightGameplayTags::Ability_Combat_Primary)
		{
			PrimaryIndices.Add(Index);
		}
	}
	if (TestTrue(TEXT("Real fixture contains an ordered combo"), PrimaryIndices.Num() >= 2))
	{
		Attacks.Swap(PrimaryIndices[0], PrimaryIndices[1]);
		TestEqual(TEXT("Reversed section sequence cannot pass combo contract"),
			Edited->TrySetAttacks(Attacks).Error, EKnightError::InvalidData);
		Attacks = Profile->GetAttacks();
		const TStrongObjectPtr<UAnimMontage> DifferentMontage(DuplicateObject<UAnimMontage>(
			Attacks[PrimaryIndices[1]].Montage.LoadSynchronous(), GetTransientPackage()));
		Attacks[PrimaryIndices[1]].Montage = DifferentMontage.Get();
		TestEqual(TEXT("Separate otherwise-valid montages cannot masquerade as one section combo"),
			Edited->TrySetAttacks(Attacks).Error, EKnightError::InvalidData);
	}
	const TStrongObjectPtr<UAnimMontage> MissingWarping(
		DuplicateObject<UAnimMontage>(Profile->GetTraversalMontage(), GetTransientPackage()));
	MissingWarping->Notifies.Reset();
	TestEqual(TEXT("Traversal with real root motion but no real warp window is rejected"), Edited->TrySetActionMontages(
		Profile->GetDodgeMontage(), Profile->GetRuneMontage(), Profile->GetStaggerMontage(), MissingWarping.Get(),
		Profile->GetFinisherSourceMontage(), Profile->GetFinisherTargetMontage()).Error, EKnightError::InvalidData);
	TestTrue(TEXT("Failed traversal edit preserves valid motion warping content"),
		Edited->GetTraversalMontage() == Profile->GetTraversalMontage());
	const TStrongObjectPtr<UAnimMontage> EarlyBlendOut(
		DuplicateObject<UAnimMontage>(Profile->GetTraversalMontage(), GetTransientPackage()));
	EarlyBlendOut->BlendOutTriggerTime = -1.0f;
	EarlyBlendOut->BlendOut.SetBlendTime(0.1f);
	TestEqual(TEXT("Blending out during the warp window cannot lose the validated landing trajectory"),
		Edited->TrySetActionMontages(Profile->GetDodgeMontage(), Profile->GetRuneMontage(),
			Profile->GetStaggerMontage(), EarlyBlendOut.Get(), Profile->GetFinisherSourceMontage(),
			Profile->GetFinisherTargetMontage()).Error, EKnightError::InvalidData);
	TestTrue(TEXT("Rejected early blend preserves valid traversal content"),
		Edited->GetTraversalMontage() == Profile->GetTraversalMontage());
	return true;
}

namespace KnightCombatTests
{
	struct FKnightCombatFixture
	{
		TWeakObjectPtr<ACharacter> Actor;
		TWeakObjectPtr<UKnightAbilitySystemComponent> ASC;
		TWeakObjectPtr<UKnightAttributeSet> Attributes;
		TWeakObjectPtr<UKnightLifeComponent> Life;
		TWeakObjectPtr<UKnightCombatComponent> Combat;
	};

	struct FKnightParrySweepObservation
	{
		int32 Count = 0;
		float TargetHealth = 0.0f;
		float BystanderHealth = 0.0f;
		float WorldTime = 0.0f;
	};

	struct FKnightRuneApplicationObservation
	{
		int32 DamageAccepted = 0;
		int32 StatusRejected = 0;
	};

	class FKnightCombatWorld final : public FNoncopyable
	{
	public:
		FKnightCombatWorld()
			: GameInstance(NewObject<UGameInstance>(GEngine))
		{
			GameInstance->InitializeStandalone();
			World = GameInstance->GetWorld();
			World->SetShouldTick(false);
			World->GetWorldSettings()->DefaultGameMode = AGameModeBase::StaticClass();
			FURL URL;
			URL.Map = TEXT("KnightCombatNativeWorld");
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
		~FKnightCombatWorld()
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

	FKnightCombatFixture SpawnKnight(UWorld* World, const FVector& Location, bool bParty)
	{
		FKnightCombatFixture Fixture;
		Fixture.Actor = World->SpawnActor<ACharacter>();
		Fixture.Actor->SetActorLocation(Location);
		Fixture.Actor->SetReplicates(true);
		Fixture.Actor->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
		Fixture.ASC = NewObject<UKnightAbilitySystemComponent>(Fixture.Actor.Get());
		Fixture.Actor->AddInstanceComponent(Fixture.ASC.Get());
		Fixture.ASC->RegisterComponent();
		Fixture.Attributes = NewObject<UKnightAttributeSet>(Fixture.Actor.Get());
		Fixture.Life = NewObject<UKnightLifeComponent>(Fixture.Actor.Get());
		Fixture.Actor->AddInstanceComponent(Fixture.Life.Get());
		Fixture.Life->RegisterComponent();
		FKnightAttributeDefaults Defaults;
		Defaults.MaxHealth = 1000.0f;
		Defaults.MaxStamina = 1000.0f;
		Fixture.ASC->InitializeKnightActor(Fixture.Actor.Get(), Fixture.Attributes.Get(), Fixture.Life.Get(),
			FKnightEntityId::Create(), Defaults, bParty);
		if (!bParty)
		{
			Fixture.ASC->SetLooseGameplayTagCount(KnightGameplayTags::Role_Enemy, 1,
				EGameplayTagReplicationState::TagAndCountToAll);
		}
		Fixture.Combat = NewObject<UKnightCombatComponent>(Fixture.Actor.Get());
		Fixture.Actor->AddInstanceComponent(Fixture.Combat.Get());
		Fixture.Combat->RegisterComponent();
		return Fixture;
	}

	enum class EKnightCombatTestStep : uint8
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

	class FKnightCombatMechanicsCommand final : public IAutomationLatentCommand
	{
	public:
		explicit FKnightCombatMechanicsCommand(FAutomationTestBase* InTest)
			: Test(InTest)
		{
			Source = SpawnKnight(Context.World.Get(), FVector(0.0f, 0.0f, 88.0f), true);
			Target = SpawnKnight(Context.World.Get(), FVector(120.0f, 0.0f, 100.0f), false);
			// A broad collision target keeps this test about actual notify-window deduplication, independent of art reach.
			Target.Actor->GetCapsuleComponent()->SetCapsuleSize(140.0f, 150.0f);
			Target.Actor->GetCharacterMovement()->DisableMovement();
			const FPrimaryAssetId ProfileId(TEXT("KnightCombatProfile"), TEXT("FoundationCombat"));
			bReady = Test->TestTrue(TEXT("Source real combat profile configures"), Source.Combat->Configure(ProfileId).IsSuccess()) &&
				Test->TestTrue(TEXT("Enemy uses same native combat domain"), Target.Combat->Configure(ProfileId).IsSuccess());
			if (bReady)
			{
				Profile = Source.Combat->GetProfile();
				Test->TestEqual(TEXT("Configuring same source is idempotent"), Source.Combat->Configure(ProfileId).Error, EKnightError::None);
				Test->TestEqual(TEXT("Exactly one combat grant source"), Source.ASC->GetOwnedGrantCount(), 1);
				Test->TestTrue(TEXT("All nine actual combat GAS specs granted"), Source.ASC->GetActivatableAbilities().Num() == 9);
				Source.Combat->SetValidatedTarget(Target.Actor.Get());
			}
		}
		virtual ~FKnightCombatMechanicsCommand() override
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
			case EKnightCombatTestStep::Warmup:
				if (Elapsed < 0.1f)
				{
					return false;
				}
				BeforeHealth = Target.Attributes->GetHealth();
				Press(KnightGameplayTags::Input_Combat_Primary);
				Test->TestTrue(TEXT("Actual primary ability remains active after release"), Source.Combat->GetSnapshot().bAttacking);
				Next(EKnightCombatTestStep::Light);
				break;
			case EKnightCombatTestStep::Light:
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
					Source.Combat->GetSnapshot().GetOutcomeCount(EKnightCombatOutcome::LightHit), 1);
				Test->TestTrue(TEXT("Non-stagger hit plays actual reaction montage while target is idle"), bObservedFlinch);
				Test->TestEqual(TEXT("One window applies authored health damage exactly once"),
					BeforeHealth - Target.Attributes->GetHealth(), FirstAttack(KnightGameplayTags::Ability_Combat_Primary).Damage);
				Test->TestEqual(TEXT("Light costs the authored stamina via GE"), Source.Attributes->GetStamina(),
					1000.0f - FirstAttack(KnightGameplayTags::Ability_Combat_Primary).StaminaCost);
				ResetActors();
				BeforeHealth = Target.Attributes->GetHealth();
				Press(KnightGameplayTags::Input_Combat_Primary);
				BufferedIndex = 0;
				Next(EKnightCombatTestStep::Combo);
				break;
			case EKnightCombatTestStep::Combo:
				if (Source.Combat->HasOpenComboWindow() && Source.Combat->GetSnapshot().ComboIndex != BufferedIndex)
				{
					BufferedIndex = Source.Combat->GetSnapshot().ComboIndex;
					Press(KnightGameplayTags::Input_Combat_Primary);
				}
				if (Source.Combat->GetSnapshot().bAttacking)
				{
					return false;
				}
				{
					int32 LightCount = 0;
					float Damage = 0.0f;
					float Cost = 0.0f;
					for (const FKnightAttackDefinition& Attack : Profile->GetAttacks())
					{
						if (Attack.ActionTag == KnightGameplayTags::Ability_Combat_Primary)
						{
							++LightCount;
							Damage += Attack.Damage;
							Cost += Attack.StaminaCost;
						}
					}
					Test->TestEqual(TEXT("Input in actual montage notify window reaches every light section"),
						Source.Combat->GetSnapshot().ComboIndex, LightCount);
					Test->TestEqual(TEXT("Every section transition is a confirmed combo fact"),
						Source.Combat->GetSnapshot().GetOutcomeCount(EKnightCombatOutcome::Combo), LightCount - 1);
					Test->TestEqual(TEXT("Combo sections do not multiply each window's damage"),
						BeforeHealth - Target.Attributes->GetHealth(), Damage);
					Test->TestEqual(TEXT("Every executed combo section commits one stamina cost"),
						Source.Attributes->GetStamina(), 1000.0f - Cost);
				}
				ResetActors();
				BeforeHealth = Target.Attributes->GetHealth();
				Press(KnightGameplayTags::Input_Combat_Heavy);
				Next(EKnightCombatTestStep::Heavy);
				break;
			case EKnightCombatTestStep::Heavy:
				if (Source.Combat->GetSnapshot().bAttacking)
				{
					return false;
				}
				Test->TestEqual(TEXT("Heavy executes real trace once"),
					Source.Combat->GetSnapshot().GetOutcomeCount(EKnightCombatOutcome::HeavyHit), 1);
				Test->TestEqual(TEXT("Heavy applies its data-driven damage"), BeforeHealth - Target.Attributes->GetHealth(),
					FirstAttack(KnightGameplayTags::Ability_Combat_Heavy).Damage);
				ResetActors();
				BeforeHealth = Target.Attributes->GetHealth();
				Press(KnightGameplayTags::Input_Combat_Primary);
				Source.ASC->HandleControllerChanged();
				Test->TestFalse(TEXT("Owner change cancels active attack"), Source.Combat->GetSnapshot().bAttacking);
				Test->TestEqual(TEXT("Owner change preserves combat grant"), Source.ASC->GetOwnedGrantCount(), 1);
				Next(EKnightCombatTestStep::CanceledAttack);
				break;
			case EKnightCombatTestStep::CanceledAttack:
				if (Elapsed < 0.75f)
				{
					return false;
				}
				Test->TestEqual(TEXT("Canceled montage cannot emit delayed trace damage"), Target.Attributes->GetHealth(), BeforeHealth);
				if (!PrepareParrySweep())
				{
					return true;
				}
				Next(EKnightCombatTestStep::ReentrantParrySweep);
				break;
			case EKnightCombatTestStep::ReentrantParrySweep:
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
					Source.ASC->RequestAbilityInput(KnightGameplayTags::Input_Combat_Block, true).IsSuccess());
				BeforeHealth = Source.Attributes->GetHealth();
				UKnightAbilitySystemComponent::ApplyCombatDamage(Target.Actor.Get(), Source.Actor.Get(), 20.0f, 20.0f, true);
				Test->TestEqual(TEXT("Front block scales actual execution damage"), BeforeHealth - Source.Attributes->GetHealth(),
					20.0f * Profile->GetTuning().BlockDamageFraction);
				Test->TestEqual(TEXT("Blocked hit consumes stamina through GE"), Source.Attributes->GetStamina(),
					1000.0f - Profile->GetTuning().BlockStaminaCost);
				Target.Actor->SetActorLocation(Source.Actor->GetActorLocation() - FVector(180.0f, 0.0f, 0.0f));
				BeforeHealth = Source.Attributes->GetHealth();
				UKnightAbilitySystemComponent::ApplyCombatDamage(Target.Actor.Get(), Source.Actor.Get(), 20.0f, 0.0f, true);
				Test->TestEqual(TEXT("Rear attack bypasses directional block"), BeforeHealth - Source.Attributes->GetHealth(), 20.0f);
				Source.ASC->RequestAbilityInput(KnightGameplayTags::Input_Combat_Block, false);
				Test->TestFalse(TEXT("Released block leaves no state"), Source.Combat->GetSnapshot().bBlocking);
				ResetActors();
				Target.Actor->SetActorLocation(Source.Actor->GetActorLocation() + FVector(180.0f, 0.0f, 0.0f));
				Press(KnightGameplayTags::Input_Combat_Parry);
				BeforeHealth = Source.Attributes->GetHealth();
				UKnightAbilitySystemComponent::ApplyCombatDamage(Target.Actor.Get(), Source.Actor.Get(), 20.0f, 20.0f, true);
				Test->TestEqual(TEXT("Timed front parry prevents health damage"), Source.Attributes->GetHealth(), BeforeHealth);
				Test->TestTrue(TEXT("Parry staggers the actual attacking actor"), Target.Combat->GetSnapshot().bStaggered);
				Test->TestTrue(TEXT("Parry has actual GAS cue notification"),
					Source.ASC->GetCueDiagnostics(KnightGameplayTags::GameplayCue_Parry).ExecutedCount > 0);
				Next(EKnightCombatTestStep::Defense);
				break;
			case EKnightCombatTestStep::Defense:
				if (Elapsed <= Profile->GetTuning().ParryWindow + 0.05f)
				{
					return false;
				}
				Test->TestFalse(TEXT("Parry state expires via actual ability timer"), Source.Combat->GetSnapshot().bParrying);
				ResetActors();
				Press(KnightGameplayTags::Input_Combat_Dodge);
				Test->TestTrue(TEXT("Dodge persists after release"), Source.Combat->GetSnapshot().bDodging);
				Test->TestEqual(TEXT("Dodge charges authored cost"), Source.Attributes->GetStamina(),
					1000.0f - Profile->GetTuning().DodgeStaminaCost);
				Next(EKnightCombatTestStep::Dodge);
				break;
			case EKnightCombatTestStep::Dodge:
				if (Elapsed <= Profile->GetTuning().DodgeInvulnerabilityStart + 0.01f)
				{
					return false;
				}
				BeforeHealth = Source.Attributes->GetHealth();
				UKnightAbilitySystemComponent::ApplyCombatDamage(Target.Actor.Get(), Source.Actor.Get(), 20.0f, 20.0f, true);
				Test->TestEqual(TEXT("Only timed dodge window prevents execution damage"), Source.Attributes->GetHealth(), BeforeHealth);
				Test->TestEqual(TEXT("Dodge confirms actual incoming avoided hit"),
					Source.Combat->GetSnapshot().GetOutcomeCount(EKnightCombatOutcome::Dodged), 1);
				Source.ASC->HandleControllerChanged();
				Test->TestFalse(TEXT("Owner change removes dodge immunity"), Source.Combat->GetSnapshot().bDodging);
				UKnightAbilitySystemComponent::ApplyCombatDamage(Target.Actor.Get(), Source.Actor.Get(), 20.0f, 0.0f, true);
				Test->TestEqual(TEXT("No immunity survives cancellation"), BeforeHealth - Source.Attributes->GetHealth(), 20.0f);
				ResetActors();
				BeforeHealth = Target.Attributes->GetHealth();
				BeforeSpeed = Target.Actor->GetCharacterMovement()->MaxWalkSpeed;
				Press(KnightGameplayTags::Input_Combat_Rune, Target.Actor.Get());
				Test->TestEqual(TEXT("Rune damage uses shared GAS execution"), BeforeHealth - Target.Attributes->GetHealth(),
					Profile->GetTuning().RuneDamage);
				Test->TestEqual(TEXT("Rune consumes authored energy via GE"), Source.Attributes->GetRuneEnergy(),
					100.0f - Profile->GetTuning().RuneEnergyCost);
				Test->TestTrue(TEXT("Rune applies actual timed GAS status"),
					Target.ASC->HasMatchingGameplayTag(KnightGameplayTags::State_Combat_RuneSlowed));
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
					EKnightError::InvalidData);
				Test->TestEqual(TEXT("Rejected baseline preserves active slow and updated authored speed"),
					Target.Actor->GetCharacterMovement()->MaxWalkSpeed, 225.0f);
				Next(EKnightCombatTestStep::Rune);
				break;
			case EKnightCombatTestStep::Rune:
				if (Elapsed <= Profile->GetTuning().RuneSlowDuration + 0.1f)
				{
					return false;
				}
				Test->TestFalse(TEXT("Timed GE removes status without manual mutation"),
					Target.ASC->HasMatchingGameplayTag(KnightGameplayTags::State_Combat_RuneSlowed));
				Test->TestEqual(TEXT("Status expiry restores authored movement speed"),
					Target.Actor->GetCharacterMovement()->MaxWalkSpeed, BeforeSpeed);
				ResetActors();
				TestRejectedRuneStatus();
				ResetActors();
				Test->TestTrue(TEXT("Reset fixture starts with its validated combat target"),
					Source.Combat->GetLockedTarget() == Target.Actor.Get());
				Press(KnightGameplayTags::Input_Combat_LockOn, Target.Actor.Get());
				Test->TestTrue(TEXT("Lock input toggles the already-locked target off"),
					Source.Combat->GetLockedTarget() == nullptr);
				Test->TestEqual(TEXT("Unlock does not publish a new target-lock confirmation"),
					Source.Combat->GetSnapshot().GetOutcomeCount(EKnightCombatOutcome::TargetLocked), 0);
				Press(KnightGameplayTags::Input_Combat_LockOn, Target.Actor.Get());
				Test->TestTrue(TEXT("Lock validates hostile target"), Source.Combat->GetLockedTarget() == Target.Actor.Get());
				Test->TestEqual(TEXT("Actual lock acquisition publishes one confirmed target"),
					Source.Combat->GetSnapshot().GetOutcomeCount(EKnightCombatOutcome::TargetLocked), 1);
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
				Next(EKnightCombatTestStep::LockLOS);
				break;
			case EKnightCombatTestStep::LockLOS:
				if (Elapsed < 0.2f)
				{
					return false;
				}
				Test->TestTrue(TEXT("Real visibility obstruction invalidates target lock"), Source.Combat->GetLockedTarget() == nullptr);
				Context.StopPlay();
				Test->TestEqual(TEXT("EndPlay revokes only owned combat source"), Source.ASC->GetOwnedGrantCount(), 0);
				Test->TestFalse(TEXT("EndPlay cleans combat actions"), Source.Combat->GetSnapshot().bAttacking);
				Step = EKnightCombatTestStep::Finished;
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
			Bystander = SpawnKnight(Context.World.Get(), Target.Actor->GetActorLocation(), false);
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
			ExtraParryCollider = NewObject<UBoxComponent>(Target.Actor.Get(), TEXT("KnightParryRegressionCollider"));
			Target.Actor->AddInstanceComponent(ExtraParryCollider.Get());
			ExtraParryCollider->SetupAttachment(Target.Actor->GetRootComponent());
			ExtraParryCollider->SetBoxExtent(FVector(140.0f, 140.0f, 150.0f));
			ExtraParryCollider->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
			ExtraParryCollider->SetCollisionObjectType(ECC_Pawn);
			ExtraParryCollider->SetCollisionResponseToAllChannels(ECR_Ignore);
			ExtraParryCollider->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
			ExtraParryCollider->RegisterComponent();
			ParryObservation = MakeShared<FKnightParrySweepObservation>();
			const TSharedRef<FKnightParrySweepObservation> Observation = ParryObservation.ToSharedRef();
			const TWeakObjectPtr<UKnightAttributeSet> TargetAttributes = Target.Attributes;
			const TWeakObjectPtr<UKnightAttributeSet> BystanderAttributes = Bystander.Attributes;
			const TWeakObjectPtr<UWorld> World = Context.World;
			ParryHandle = Target.Combat->OnCombatOutcome.AddLambda(
				[Observation, TargetAttributes, BystanderAttributes, World](const FKnightCombatOutcome& Event)
				{
					if (Event.Outcome == EKnightCombatOutcome::Parried && TargetAttributes.IsValid() &&
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
			const FKnightAttackDefinition& Attack = FirstAttack(KnightGameplayTags::Ability_Combat_Primary);
			UAnimMontage* Montage = Attack.Montage.LoadSynchronous();
			float SectionStart = 0.0f;
			float SectionEnd = 0.0f;
			Montage->GetSectionStartAndEndTime(Montage->GetSectionIndex(Attack.SectionName), SectionStart, SectionEnd);
			ParryTriggerPosition = SectionStart;
			bool bFoundTrace = false;
			for (const FAnimNotifyEvent& Notify : Montage->Notifies)
			{
				if (Cast<UKnightAnimNotifyState_TraceWindow>(Notify.NotifyStateClass) != nullptr &&
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
			Press(KnightGameplayTags::Input_Combat_Primary);
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
					Target.ASC->RequestAbilityInput(KnightGameplayTags::Input_Combat_Parry, true).IsSuccess());
				Target.ASC->RequestAbilityInput(KnightGameplayTags::Input_Combat_Parry, false);
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
			const TSharedRef<FKnightRuneApplicationObservation> Observation = MakeShared<FKnightRuneApplicationObservation>();
			FGameplayEffectApplicationQuery Query = FGameplayEffectApplicationQuery::CreateLambda(
				[Observation](const FActiveGameplayEffectsContainer& ActiveEffects, const FGameplayEffectSpec& Spec)
				{
					if (Spec.Def != nullptr && Spec.Def->IsA(UKnightRuneSlowEffect::StaticClass()))
					{
						++Observation->StatusRejected;
						return false;
					}
					if (Spec.Def != nullptr && Spec.Def->IsA(UKnightDamageEffect::StaticClass()))
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
			const int32 Cues = Target.ASC->GetCueDiagnostics(KnightGameplayTags::GameplayCue_Rune).ExecutedCount;
			const int32 Confirmations = Source.Combat->GetSnapshot().GetOutcomeCount(EKnightCombatOutcome::RuneHit);
			const FKnightResult Result = Source.ASC->RequestAbilityInput(
				KnightGameplayTags::Input_Combat_Rune, true, Target.Actor.Get());
			Source.ASC->RequestAbilityInput(KnightGameplayTags::Input_Combat_Rune, false);
			Test->TestEqual(TEXT("Rejected real duration GE returns a typed rune failure"), Result.Error, EKnightError::InvalidState);
			Test->TestEqual(TEXT("Rejected rune reports the actual failed status operation"), Result.Context, FName(TEXT("RuneStatusRejected")));
			Test->TestEqual(TEXT("Application query allows the actual instant damage execution"), Observation->DamageAccepted, 1);
			Test->TestEqual(TEXT("Application query rejects only the actual RuneSlow GE"), Observation->StatusRejected, 1);
			Test->TestEqual(TEXT("Failure retains damage that already happened"), Health - Target.Attributes->GetHealth(),
				Profile->GetTuning().RuneDamage);
			Test->TestEqual(TEXT("Failure retains the committed energy cost without fake rollback"),
				Energy - Source.Attributes->GetRuneEnergy(), Profile->GetTuning().RuneEnergyCost);
			Test->TestFalse(TEXT("Rejected actual status cannot leave a slow tag"),
				Target.ASC->HasMatchingGameplayTag(KnightGameplayTags::State_Combat_RuneSlowed));
			Test->TestFalse(TEXT("Rejected rune cancels its ability-owned action state"),
				Source.ASC->HasMatchingGameplayTag(KnightGameplayTags::State_Combat_Attacking));
			Test->TestEqual(TEXT("Rejected status never confirms a complete RuneHit"),
				Source.Combat->GetSnapshot().GetOutcomeCount(EKnightCombatOutcome::RuneHit), Confirmations);
			Test->TestEqual(TEXT("Rejected status never emits the rune success cue"),
				Target.ASC->GetCueDiagnostics(KnightGameplayTags::GameplayCue_Rune).ExecutedCount, Cues);
		}
		const FKnightAttackDefinition& FirstAttack(FGameplayTag Tag) const
		{
			for (const FKnightAttackDefinition& Attack : Profile->GetAttacks())
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
		void Next(EKnightCombatTestStep InStep)
		{
			Step = InStep;
			Elapsed = 0.0f;
		}
		FAutomationTestBase* Test;
		FKnightCombatWorld Context;
		FKnightCombatFixture Source;
		FKnightCombatFixture Target;
		FKnightCombatFixture Bystander;
		TWeakObjectPtr<UBoxComponent> ExtraParryCollider;
		TSharedPtr<FKnightParrySweepObservation> ParryObservation;
		FDelegateHandle ParryHandle;
		ECollisionResponse TargetVisibility = ECR_Ignore;
		TWeakObjectPtr<AActor> Wall;
		TWeakObjectPtr<UKnightCombatProfileDefinition> Profile;
		EKnightCombatTestStep Step = EKnightCombatTestStep::Warmup;
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKnightCombatMechanicsTest, "Knight.Combat.NativeMechanics",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FKnightCombatMechanicsTest::RunTest(const FString& Parameters)
{
	ADD_LATENT_AUTOMATION_COMMAND(KnightCombatTests::FKnightCombatMechanicsCommand(this));
	return true;
}

namespace KnightCombatTests
{
	class FKnightProjectionCombatCommand final : public IAutomationLatentCommand
	{
	public:
		explicit FKnightProjectionCombatCommand(FAutomationTestBase* InTest)
			: Test(InTest)
		{
			Source = SpawnKnight(Context.World.Get(), FVector(0, 0, 88), true);
			Target = SpawnKnight(Context.World.Get(), FVector(120, 0, 100), false);
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
					if (Spec.GetDynamicSpecSourceTags().HasTagExact(KnightGameplayTags::Input_Combat_Rune))
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
				Press(KnightGameplayTags::Input_Combat_Primary);
				Step = 1;
			}
			else if (Step == 1 && !Source.Combat->GetSnapshot().bAttacking)
			{
				const FKnightAttackDefinition& Attack = Profile->GetAttacks()[0];
				Test->TestEqual(TEXT("No-projection real trace retains authored damage"), Before - Target.Attributes->GetHealth(), Attack.Damage);
				Reset();
				Before = Target.Attributes->GetHealth();
				const float Energy = Source.Attributes->GetRuneEnergy();
				Press(KnightGameplayTags::Input_Combat_Rune);
				Test->TestEqual(TEXT("Baseline real Rune damage unchanged"), Before - Target.Attributes->GetHealth(), Profile->GetTuning().RuneDamage);
				Test->TestEqual(TEXT("Baseline real Rune GE cost unchanged"), Energy - Source.Attributes->GetRuneEnergy(), Profile->GetTuning().RuneEnergyCost);
				Reset();
				FKnightGASProjectionPlan Plan;
				Plan.SourceKey = TEXT("RPG");
				Plan.DefinitionId = FPrimaryAssetId(TEXT("KnightHero"), TEXT("Training_Red"));
				Plan.AttackPowerBonus = 10.0f;
				Plan.MaxHealthBonus = 50.0f;
				FKnightActionModifier Modifier;
				Modifier.AbilityTag = KnightGameplayTags::Ability_Combat_Rune;
				Modifier.DamageScale = 1.5f;
				Modifier.CostScale = 0.5f;
				Modifier.DurationScale = 2.0f;
				Plan.ActionModifiers.Add(Modifier);
				FKnightMutationFence Fence;
				FKnightProjectionCapability Capability;
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
				Press(KnightGameplayTags::Input_Combat_Primary);
				Step = 2;
			}
			else if (Step == 2 && !Source.Combat->GetSnapshot().bAttacking)
			{
				Test->TestEqual(TEXT("Actual notify trace consumes AP delta once"),
					Before - Target.Attributes->GetHealth(), Profile->GetAttacks()[0].Damage + 10.0f);
				Test->TestEqual(TEXT("Projected trace window still hits once"),
					Source.Combat->GetSnapshot().GetOutcomeCount(EKnightCombatOutcome::LightHit), 1);
				Reset();
				Before = Target.Attributes->GetHealth();
				const float Energy = Source.Attributes->GetRuneEnergy();
				Press(KnightGameplayTags::Input_Combat_Rune);
				Test->TestEqual(TEXT("Same real Rune uses AP delta and committed damage multiplier"),
					Before - Target.Attributes->GetHealth(), (Profile->GetTuning().RuneDamage + 10.0f) * 1.5f);
				Test->TestEqual(TEXT("Same real Rune uses discounted resource GE"),
					Energy - Source.Attributes->GetRuneEnergy(), Profile->GetTuning().RuneEnergyCost * 0.5f);
				bool bRealDuration = false;
				for (FActiveGameplayEffectHandle Handle : Target.ASC->GetActiveEffects(FGameplayEffectQuery()))
				{
					const FActiveGameplayEffect* Effect = Target.ASC->GetActiveGameplayEffect(Handle);
					if (Effect != nullptr && Effect->Spec.Def->IsA<UKnightRuneSlowEffect>())
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
						Target.ASC->HasMatchingGameplayTag(KnightGameplayTags::State_Combat_RuneSlowed));
					return false;
				}
				if (Elapsed < Profile->GetTuning().RuneSlowDuration * 2.0f + 0.2f)
				{
					return false;
				}
				Test->TestFalse(TEXT("Real extended slow eventually expires"),
					Target.ASC->HasMatchingGameplayTag(KnightGameplayTags::State_Combat_RuneSlowed));
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
		FKnightCombatWorld Context;
		FKnightCombatFixture Source;
		FKnightCombatFixture Target;
		TWeakObjectPtr<UKnightCombatProfileDefinition> Profile;
		FGameplayAbilitySpecHandle RuneSpec;
		uint64 LastFrame = MAX_uint64;
		int32 Step = 0;
		float Elapsed = 0.0f;
		float Before = 0.0f;
		bool bReady = false;
		double Started = FPlatformTime::Seconds();
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKnightCombatProjectionTest, "Knight.Combat.RPGProjection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FKnightCombatProjectionTest::RunTest(const FString& Parameters)
{
	ADD_LATENT_AUTOMATION_COMMAND(KnightCombatTests::FKnightProjectionCombatCommand(this));
	return true;
}

#endif
