#include "Components/GolemAbilitySystemComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Abilities/GolemReviveAbility.h"
#include "Abilities/GolemTestDamageAbility.h"
#include "Components/BoxComponent.h"
#include "Components/GolemLifeComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Events/GolemEventSubsystem.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/WorldSettings.h"
#include "GameplayEffect.h"
#include "HAL/PlatformTime.h"
#include "Misc/AutomationTest.h"
#include "Tags/GolemGameplayTags.h"
#include "UObject/StrongObjectPtr.h"
#include <limits>

namespace GolemGASTests
{
	struct FGolemFixture
	{
		TWeakObjectPtr<AActor> Actor;
		TWeakObjectPtr<UGolemAbilitySystemComponent> ASC;
		TWeakObjectPtr<UGolemAttributeSet> Attributes;
		TWeakObjectPtr<UGolemLifeComponent> Life;
	};

	class FGolemTestWorld final : public FNoncopyable
	{
	public:
		FGolemTestWorld()
			: GameInstance(NewObject<UGameInstance>(GEngine))
		{
			GameInstance->InitializeStandalone();
			World = GameInstance->GetWorld();
			World->SetShouldTick(false);
			World->GetWorldSettings()->DefaultGameMode = AGameModeBase::StaticClass();
			FURL URL;
			URL.Map = TEXT("GolemGASDomainTest");
			URL.AddOption(TEXT("game=/Script/Engine.GameModeBase"));
			World->SetGameMode(URL);
			World->InitializeActorsForPlay(URL);
			World->BeginPlay();
		}
		~FGolemTestWorld()
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
		bool StopPlay()
		{
			return World.IsValid() && World->HasBegunPlay() && World->EndPlay(EEndPlayReason::Quit);
		}
		TWeakObjectPtr<UWorld> World;

	private:
		TStrongObjectPtr<UGameInstance> GameInstance;
	};

	FGolemFixture SpawnFixture(UWorld* World, const FVector& Location, bool bParty = true, float Defense = 0.0f)
	{
		FGolemFixture Fixture;
		Fixture.Actor = World->SpawnActor<AActor>();
		USceneComponent* Root = NewObject<USceneComponent>(Fixture.Actor.Get());
		Fixture.Actor->AddInstanceComponent(Root);
		Fixture.Actor->SetRootComponent(Root);
		Root->RegisterComponent();
		Fixture.Actor->SetActorLocation(Location);
		Fixture.Actor->SetReplicates(true);
		Fixture.ASC = NewObject<UGolemAbilitySystemComponent>(Fixture.Actor.Get());
		Fixture.Actor->AddInstanceComponent(Fixture.ASC.Get());
		Fixture.ASC->RegisterComponent();
		Fixture.Attributes = NewObject<UGolemAttributeSet>(Fixture.Actor.Get());
		Fixture.Life = NewObject<UGolemLifeComponent>(Fixture.Actor.Get());
		Fixture.Actor->AddInstanceComponent(Fixture.Life.Get());
		Fixture.Life->RegisterComponent();
		FGolemAttributeDefaults Defaults;
		Defaults.Defense = Defense;
		Fixture.ASC->InitializeGolemActor(Fixture.Actor.Get(), Fixture.Attributes.Get(), Fixture.Life.Get(),
			FGolemEntityId::Create(), Defaults, bParty);
		if (!bParty)
		{
			Fixture.ASC->SetLooseGameplayTagCount(GolemGameplayTags::Role_Enemy, 1,
				EGameplayTagReplicationState::TagAndCountToAll);
		}
		return Fixture;
	}

	TStrongObjectPtr<UGolemAbilitySetDefinition> MakeSet()
	{
		TStrongObjectPtr<UGolemAbilitySetDefinition> Set(NewObject<UGolemAbilitySetDefinition>());
		Set->TrySetIdentity(TEXT("TestFoundation"), TEXT("GolemAbilitySet"));
		FGolemAbilitySetEntry Pulse;
		Pulse.Ability = UGolemTestDamageAbility::StaticClass();
		Pulse.InputTag = GolemGameplayTags::Input_Ability_Test;
		FGolemAbilitySetEntry Revive;
		Revive.Ability = UGolemReviveAbility::StaticClass();
		Revive.InputTag = GolemGameplayTags::Input_Party_Revive;
		Set->TrySetAbilities({Pulse, Revive});
		FGolemTagRelationship Relationship;
		Relationship.AbilityTag = GolemGameplayTags::Ability_Party_Revive;
		Relationship.ActivationRequiredTags.AddTag(GolemGameplayTags::State_Life_Alive);
		Relationship.ActivationBlockedTags.AddTag(GolemGameplayTags::State_Life_Downed);
		Relationship.AbilityTagsToBlock.AddTag(GolemGameplayTags::Ability_Test_Pulse);
		Relationship.AbilityTagsToBlock.AddTag(GolemGameplayTags::Ability_Party_Revive);
		Set->TrySetRelationships({Relationship});
		return Set;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGolemAbilityGrantTest, "Golem.GAS.SourceOwnedGrants",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGolemAbilityGrantTest::RunTest(const FString& Parameters)
{
	GolemGASTests::FGolemTestWorld Context;
	const GolemGASTests::FGolemFixture Golem = GolemGASTests::SpawnFixture(Context.World.Get(), FVector::ZeroVector);
	const GolemGASTests::FGolemFixture Other = GolemGASTests::SpawnFixture(Context.World.Get(), FVector(100, 0, 0));
	const TStrongObjectPtr<UGolemAbilitySystemComponent> Uninitialized(
		NewObject<UGolemAbilitySystemComponent>(Golem.Actor.Get()));
	Uninitialized->HandleControllerChanged();
	Uninitialized->ClearActorInfo();
	Uninitialized->HandleControllerChanged();
	TestTrue(TEXT("Uninitialized repeated lifecycle never creates ActorInfo ownership"), Uninitialized->GetOwnerActor() == nullptr);
	const TStrongObjectPtr<UGolemAbilitySetDefinition> Set = GolemGASTests::MakeSet();
	TestTrue(TEXT("Native ability set validates"), Set->ValidateAbilitySet().IsSuccess());
	FGolemAbilitySetHandle First;
	FGolemAbilitySetHandle Repeated;
	FGolemAbilitySetHandle Independent;
	int32 Notifications = 0;
	const FDelegateHandle Subscription = Golem.ASC->OnAbilityGrantsChanged.AddLambda([&Notifications]()
		{
			++Notifications;
		});
	TestTrue(TEXT("First source grants actual specs"), Golem.ASC->GrantAbilitySet(Set.Get(), TEXT("HeroDefaults"), First).IsSuccess());
	TestEqual(TEXT("Two native abilities granted"), Golem.ASC->GetActivatableAbilities().Num(), 2);
	TestTrue(TEXT("Repeated grant is accepted"), Golem.ASC->GrantAbilitySet(Set.Get(), TEXT("HeroDefaults"), Repeated).IsSuccess());
	TestTrue(TEXT("Repeated source returns identical capability"), First.GrantId == Repeated.GrantId);
	TestEqual(TEXT("Idempotence creates no extra specs or notification"), Notifications, 1);
	TestEqual(TEXT("Idempotence preserves spec count"), Golem.ASC->GetActivatableAbilities().Num(), 2);
	TestTrue(TEXT("Independent source owns separate specs"), Golem.ASC->GrantAbilitySet(Set.Get(), TEXT("Equipment"), Independent).IsSuccess());
	TestEqual(TEXT("Both authoritative sources are counted"), Golem.ASC->GetOwnedGrantCount(), 2);
	TestEqual(TEXT("Foreign ASC cannot revoke this capability"), Other.ASC->RevokeAbilitySet(First).Error, EGolemError::NotOwned);
	FGolemAbilitySetHandle Forged = First;
	Forged.GrantId = FGuid::NewGuid();
	TestEqual(TEXT("Stale capability rejected"), Golem.ASC->RevokeAbilitySet(Forged).Error, EGolemError::NotOwned);
	TestTrue(TEXT("Exact source capability revokes"), Golem.ASC->RevokeAbilitySet(First).IsSuccess());
	TestEqual(TEXT("Only unrelated source's specs remain"), Golem.ASC->GetActivatableAbilities().Num(), 2);
	TestEqual(TEXT("Only unrelated source count remains"), Golem.ASC->GetOwnedGrantCount(), 1);
	TestTrue(TEXT("Repeated exact revoke is idempotent success"), Golem.ASC->RevokeAbilitySet(First).IsSuccess());
	TestEqual(TEXT("Repeated revoke creates no extra notifications"), Notifications, 3);
	FGolemAbilitySetHandle Replacement;
	TestTrue(TEXT("Previously revoked source can grant again"), Golem.ASC->GrantAbilitySet(Set.Get(), TEXT("HeroDefaults"), Replacement).IsSuccess());
	TestTrue(TEXT("Regrant receives fresh capability"), Replacement.GrantId != First.GrantId);
	TestTrue(TEXT("Old exact revoke remains harmless success after regrant"), Golem.ASC->RevokeAbilitySet(First).IsSuccess());
	TestEqual(TEXT("Old capability cannot remove replacement specs"), Golem.ASC->GetActivatableAbilities().Num(), 4);
	TestEqual(TEXT("Old capability cannot mutate replacement count"), Golem.ASC->GetOwnedGrantCount(), 2);
	TestEqual(TEXT("Harmless old revoke creates no extra notifications"), Notifications, 4);
	TestTrue(TEXT("Replacement capability revokes its exact source"), Golem.ASC->RevokeAbilitySet(Replacement).IsSuccess());
	TestEqual(TEXT("Older than most-recent revoked token is rejected"), Golem.ASC->RevokeAbilitySet(First).Error, EGolemError::NotOwned);
	TestTrue(TEXT("Most-recent exact capability repeats successfully"), Golem.ASC->RevokeAbilitySet(Replacement).IsSuccess());
	TestEqual(TEXT("Most-recent repeat produces no extra notification"), Notifications, 5);
	TestTrue(TEXT("Remaining source still revokes"), Golem.ASC->RevokeAbilitySet(Independent).IsSuccess());
	TestEqual(TEXT("All owned specs removed"), Golem.ASC->GetActivatableAbilities().Num(), 0);
	Golem.ASC->OnAbilityGrantsChanged.Remove(Subscription);
	Golem.ASC->ClearActorInfo();
	Golem.ASC->HandleControllerChanged();
	Golem.ASC->ClearActorInfo();
	Golem.ASC->HandleControllerChanged();
	TestTrue(TEXT("Cleared ActorInfo remains cleared across repeated owner callbacks"), Golem.ASC->GetOwnerActor() == nullptr &&
		Golem.ASC->GetAvatarActor() == nullptr);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGolemDamagePipelineTest, "Golem.GAS.DamageAndResourceClamps",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGolemDamagePipelineTest::RunTest(const FString& Parameters)
{
	GolemGASTests::FGolemTestWorld Context;
	const GolemGASTests::FGolemFixture Enemy = GolemGASTests::SpawnFixture(Context.World.Get(), FVector::ZeroVector, false);
	const GolemGASTests::FGolemFixture Target = GolemGASTests::SpawnFixture(Context.World.Get(), FVector(100, 0, 0), true, 10.0f);
	TestTrue(TEXT("Golem is actual ASC owner and avatar"), Target.ASC->GetOwnerActor() == Target.Actor.Get() &&
		Target.ASC->GetAvatarActor() == Target.Actor.Get());
	int32 ConfirmedDamage = 0;
	UGolemEventSubsystem* Bus = Context.World->GetSubsystem<UGolemEventSubsystem>();
	const FGolemEventSubscription Subscription = Bus->Subscribe<FGolemDamageConfirmedEvent>(Target.Actor.Get(),
		[&ConfirmedDamage](const FGolemDamageConfirmedEvent&)
		{
			++ConfirmedDamage;
		});
	TestTrue(TEXT("Server damage uses native GE execution"), UGolemAbilitySystemComponent::ApplyDamage(Enemy.Actor.Get(), Target.Actor.Get(), 25.0f).IsSuccess());
	TestEqual(TEXT("Defense capture reduces damage before health"), Target.Attributes->GetHealth(), 85.0f);
	TestEqual(TEXT("IncomingDamage consumed exactly once"), Target.Attributes->GetIncomingDamage(), 0.0f);
	TestEqual(TEXT("Confirmed damage fact delivered"), ConfirmedDamage, 1);
	TestTrue(TEXT("Damage GameplayCue routed actual native handler"),
		Target.ASC->GetCueDiagnostics(GolemGameplayTags::GameplayCue_Damage).ExecutedCount > 0);
	Target.ASC->SetNumericAttributeBase(UGolemAttributeSet::GetMaxHealthAttribute(), 60.0f);
	Target.ASC->SetNumericAttributeBase(UGolemAttributeSet::GetMaxStaminaAttribute(), 30.0f);
	Target.ASC->SetNumericAttributeBase(UGolemAttributeSet::GetMaxRuneEnergyAttribute(), 20.0f);
	TestEqual(TEXT("Base MaxHealth decrease clamps current health"), Target.Attributes->GetHealth(), 60.0f);
	TestEqual(TEXT("Base MaxStamina decrease clamps current stamina"), Target.Attributes->GetStamina(), 30.0f);
	TestEqual(TEXT("Base MaxRuneEnergy decrease clamps current energy"), Target.Attributes->GetRuneEnergy(), 20.0f);
	UGameplayEffect* MaxEffect = NewObject<UGameplayEffect>();
	MaxEffect->DurationPolicy = EGameplayEffectDurationType::Infinite;
	FGameplayModifierInfo& Modifier = MaxEffect->Modifiers.AddDefaulted_GetRef();
	Modifier.Attribute = UGolemAttributeSet::GetMaxHealthAttribute();
	Modifier.ModifierOp = EGameplayModOp::Override;
	Modifier.ModifierMagnitude = FScalableFloat(45.0f);
	const FGameplayEffectSpec MaxSpec(MaxEffect, Target.ASC->MakeEffectContext(), 1.0f);
	const FActiveGameplayEffectHandle MaxHandle = Target.ASC->ApplyGameplayEffectSpecToSelf(MaxSpec);
	TestTrue(TEXT("Persistent max effect is real"), MaxHandle.IsValid());
	TestEqual(TEXT("Persistent max GE also clamps health"), Target.Attributes->GetHealth(), 45.0f);
	Target.ASC->RemoveActiveGameplayEffect(MaxHandle);
	TestTrue(TEXT("Fatal execution enters downed"), UGolemAbilitySystemComponent::ApplyDamage(Enemy.Actor.Get(), Target.Actor.Get(), 100.0f).IsSuccess());
	TestEqual(TEXT("Health cannot fall below zero"), Target.Attributes->GetHealth(), 0.0f);
	TestTrue(TEXT("Party Golem downed state and tag agree"), Target.Life->IsDowned() &&
		Target.ASC->HasMatchingGameplayTag(GolemGameplayTags::State_Life_Downed));
	TestTrue(TEXT("Downed cue is persistent GAS state"), Target.ASC->IsGameplayCueActive(GolemGameplayTags::GameplayCue_Downed));
	TestTrue(TEXT("Additional effective damage executes downed Golem"), UGolemAbilitySystemComponent::ApplyDamage(Enemy.Actor.Get(), Target.Actor.Get(), 20.0f).IsSuccess());
	TestTrue(TEXT("Downed damage commits dead state"), Target.Life->IsDead());
	TestFalse(TEXT("Dead state removes persistent downed cue"), Target.ASC->IsGameplayCueActive(GolemGameplayTags::GameplayCue_Downed));
	TestEqual(TEXT("Dead target rejects damage"), UGolemAbilitySystemComponent::ApplyDamage(Enemy.Actor.Get(), Target.Actor.Get(), 20.0f).Error, EGolemError::InvalidState);
	TestEqual(TEXT("Nonfinite damage rejected"), UGolemAbilitySystemComponent::ApplyDamage(Enemy.Actor.Get(), Target.Actor.Get(),
		std::numeric_limits<float>::quiet_NaN()).Error, EGolemError::InvalidData);
	TestTrue(TEXT("Checkpoint reset is explicit and restores real defaults"), Target.Life->ResetForCheckpoint().IsSuccess());
	TestTrue(TEXT("Checkpoint revives same actor"), Target.Life->IsAlive() && IsValid(Target.Actor.Get()));
	TestEqual(TEXT("Authored health default restored"), Target.Attributes->GetHealth(), 100.0f);
	Bus->Unsubscribe(Subscription);
	return true;
}

namespace GolemGASTests
{
	class FGolemReviveLifecycleCommand final : public IAutomationLatentCommand
	{
	public:
		explicit FGolemReviveLifecycleCommand(FAutomationTestBase* InTest)
			: Test(InTest), Set(MakeSet())
		{
			Source = SpawnFixture(Context.World.Get(), FVector::ZeroVector);
			Target = SpawnFixture(Context.World.Get(), FVector(100, 0, 0));
			Other = SpawnFixture(Context.World.Get(), FVector(0, 100, 0));
			Enemy = SpawnFixture(Context.World.Get(), FVector(200, 0, 0), false);
			Source.ASC->GrantAbilitySet(Set.Get(), TEXT("HeroDefaults"), SourceGrant);
			Other.ASC->GrantAbilitySet(Set.Get(), TEXT("HeroDefaults"), OtherGrant);
			Target.Life->TrySetLifeTuning(10.0f, 0.2f, 500.0f, 0.35f);
		}
		virtual bool Update() override
		{
			if (FPlatformTime::Seconds() - StartedAt > 20.0 || !Source.ASC.IsValid() ||
				!Target.ASC.IsValid() || !Other.ASC.IsValid() || !Enemy.ASC.IsValid())
			{
				Test->AddError(TEXT("Held revive lifecycle lost a fixture or exceeded its bounded execution time."));
				return true;
			}
			Context.World->Tick(LEVELTICK_All, 0.05f);
			if (WaitingUntil > Context.World->GetTimeSeconds())
			{
				return false;
			}
			switch (Phase++)
			{
			case 0:
				UGolemAbilitySystemComponent::ApplyDamage(Enemy.Actor.Get(), Target.Actor.Get(), 200.0f);
				Target.Actor->SetActorLocation(FVector(400, 0, 0));
				Test->TestTrue(TEXT("Server auto-acquisition respects target's authored revive range"), Source.ASC->RequestAbilityInput(
					GolemGameplayTags::Input_Party_Revive, true, nullptr).IsSuccess());
				Test->TestTrue(TEXT("Configured 500cm target can be reserved from 400cm"), Target.Life->GetActiveReviver() == Source.Actor.Get());
				Source.ASC->RequestAbilityInput(GolemGameplayTags::Input_Party_Revive, false);
				Test->TestFalse(TEXT("Auto-acquired channel releases its reservation cleanly"), Target.Life->IsReviveInProgress());
				Target.Actor->SetActorLocation(FVector(100, 0, 0));
				{
					FGuid Reservation;
					Test->TestEqual(TEXT("Enemy cannot revive a party Golem"), Target.Life->TryBeginRevive(Enemy.Actor.Get(), Reservation).Error,
						EGolemError::InvalidState);
					Test->TestTrue(TEXT("Native reservation starts for valid source"), Target.Life->TryBeginRevive(Source.Actor.Get(), Reservation).IsSuccess());
					Test->TestEqual(TEXT("Duration cannot be bypassed"), Target.Life->TryCompleteRevive(Source.Actor.Get(), Reservation).Error,
						EGolemError::InvalidState);
					Test->TestEqual(TEXT("Forged completion token rejected"), Target.Life->TryCompleteRevive(Source.Actor.Get(), FGuid::NewGuid()).Error,
						EGolemError::NotOwned);
					Target.Life->CancelRevive(Source.Actor.Get(), Reservation);
				}
				Test->TestTrue(TEXT("Held revive starts actual native ability"), Source.ASC->RequestAbilityInput(
					GolemGameplayTags::Input_Party_Revive, true, Target.Actor.Get()).IsSuccess());
				Test->TestTrue(TEXT("Exclusive reservation points to caster"), Target.Life->GetActiveReviver() == Source.Actor.Get());
				Test->TestTrue(TEXT("Caster owns native channel tag and persistent cue"), Source.ASC->HasMatchingGameplayTag(
					GolemGameplayTags::State_Life_Reviving) && Source.ASC->IsGameplayCueActive(GolemGameplayTags::GameplayCue_Revive));
				Test->TestEqual(TEXT("Second reviver cannot reserve same target"), Other.ASC->RequestAbilityInput(
					GolemGameplayTags::Input_Party_Revive, true, Target.Actor.Get()).Error, EGolemError::Conflict);
				Test->TestFalse(TEXT("Tag relationship blocks pulse during revive"), Source.ASC->RequestAbilityInput(
					GolemGameplayTags::Input_Ability_Test, true, Enemy.Actor.Get()).IsSuccess());
				Source.ASC->RequestAbilityInput(GolemGameplayTags::Input_Party_Revive, false);
				Test->TestFalse(TEXT("Release removes reservation"), Target.Life->IsReviveInProgress());
				Test->TestFalse(TEXT("Release removes channel tag"), Source.ASC->HasMatchingGameplayTag(GolemGameplayTags::State_Life_Reviving));
				Test->TestFalse(TEXT("Release removes caster cue"), Source.ASC->IsGameplayCueActive(GolemGameplayTags::GameplayCue_Revive));
				Source.ASC->RequestAbilityInput(GolemGameplayTags::Input_Party_Revive, true, Target.Actor.Get());
				Source.ASC->HandleControllerChanged();
				Test->TestFalse(TEXT("Controller change cancels actual channel and reservation"), Target.Life->IsReviveInProgress());
				Test->TestEqual(TEXT("Controller change preserves grants"), Source.ASC->GetOwnedGrantCount(), 1);
				Test->TestEqual(TEXT("Controller change preserves attributes"), Source.Attributes->GetHealth(), 100.0f);
				Source.ASC->RequestAbilityInput(GolemGameplayTags::Input_Party_Revive, true, Target.Actor.Get());
				Test->TestTrue(TEXT("Active source can revoke its exact grant"), Source.ASC->RevokeAbilitySet(SourceGrant).IsSuccess());
				Test->TestFalse(TEXT("Active revoke cancels reservation"), Target.Life->IsReviveInProgress());
				Test->TestFalse(TEXT("Active revoke removes channel cue"), Source.ASC->IsGameplayCueActive(GolemGameplayTags::GameplayCue_Revive));
				Source.ASC->GrantAbilitySet(Set.Get(), TEXT("HeroDefaults"), SourceGrant);
				Test->TestTrue(TEXT("Revoked relationship leaves no stale pulse blocker"), Source.ASC->RequestAbilityInput(
					GolemGameplayTags::Input_Ability_Test, true, Enemy.Actor.Get()).IsSuccess());
				Source.ASC->RequestAbilityInput(GolemGameplayTags::Input_Party_Revive, true, Target.Actor.Get());
				Source.ASC->ClearActorInfo();
				Source.ASC->HandleControllerChanged();
				Test->TestFalse(TEXT("Clearing ActorInfo cancels channel while owner context is still valid"), Target.Life->IsReviveInProgress());
				Test->TestFalse(TEXT("Clearing ActorInfo removes channel cue"), Source.ASC->IsGameplayCueActive(GolemGameplayTags::GameplayCue_Revive));
				Test->TestTrue(TEXT("Same actor context can be explicitly restored without resetting grants"), Source.ASC->InitializeGolemActor(
					Source.Actor.Get(), Source.Attributes.Get(), Source.Life.Get(), Source.ASC->GetEntityId()).IsSuccess());
				Test->TestEqual(TEXT("Restoring actor context preserves granted source count"), Source.ASC->GetOwnedGrantCount(), 1);
				Source.ASC->RequestAbilityInput(GolemGameplayTags::Input_Party_Revive, true, Target.Actor.Get());
				WaitingUntil = Context.World->GetTimeSeconds() + 0.35f;
				break;
			case 1:
				Test->TestTrue(TEXT("Real world timer completes held revive"), Target.Life->IsAlive());
				Test->TestEqual(TEXT("Revive heals through real GE to authored fraction"), Target.Attributes->GetHealth(), 35.0f);
				Test->TestFalse(TEXT("Successful revive removes target downed cue"), Target.ASC->IsGameplayCueActive(GolemGameplayTags::GameplayCue_Downed));
				Test->TestFalse(TEXT("Successful revive removes caster channel cue"), Source.ASC->IsGameplayCueActive(GolemGameplayTags::GameplayCue_Revive));
				Test->TestTrue(TEXT("Successful target revive burst reaches native handler"), Target.ASC->GetCueDiagnostics(
					GolemGameplayTags::GameplayCue_Revive).ExecutedCount > 0);
				UGolemAbilitySystemComponent::ApplyDamage(Enemy.Actor.Get(), Target.Actor.Get(), 200.0f);
				Source.ASC->RequestAbilityInput(GolemGameplayTags::Input_Party_Revive, true, Target.Actor.Get());
				Source.Actor->SetActorLocation(FVector(1000, 0, 0));
				WaitingUntil = Context.World->GetTimeSeconds() + 0.35f;
				break;
			case 2:
				Test->TestTrue(TEXT("Completion rechecks range and leaves target downed"), Target.Life->IsDowned());
				Test->TestFalse(TEXT("Range failure releases reservation"), Target.Life->IsReviveInProgress());
				Source.Actor->SetActorLocation(FVector::ZeroVector);
				Source.ASC->RequestAbilityInput(GolemGameplayTags::Input_Party_Revive, true, Target.Actor.Get());
				Obstacle = Context.World->SpawnActor<AActor>();
				{
					UBoxComponent* Box = NewObject<UBoxComponent>(Obstacle.Get());
					Obstacle->AddInstanceComponent(Box);
					Obstacle->SetRootComponent(Box);
					Box->SetBoxExtent(FVector(5, 50, 50));
					Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
					Box->SetCollisionObjectType(ECC_WorldStatic);
					Box->SetCollisionResponseToAllChannels(ECR_Block);
					Box->RegisterComponent();
					Obstacle->SetActorLocation(FVector(50, 0, 0));
				}
				WaitingUntil = Context.World->GetTimeSeconds() + 0.35f;
				break;
			case 3:
				Test->TestTrue(TEXT("Completion rechecks actual blocking collision LOS"), Target.Life->IsDowned());
				Test->TestFalse(TEXT("LOS failure releases reservation"), Target.Life->IsReviveInProgress());
				Test->TestFalse(TEXT("Blocked LOS rejects start"), Source.ASC->RequestAbilityInput(
					GolemGameplayTags::Input_Party_Revive, true, Target.Actor.Get()).IsSuccess());
				Obstacle->Destroy();
				Source.ASC->RequestAbilityInput(GolemGameplayTags::Input_Party_Revive, true, Target.Actor.Get());
				UGolemAbilitySystemComponent::ApplyDamage(Enemy.Actor.Get(), Source.Actor.Get(), 200.0f);
				Test->TestFalse(TEXT("Caster downed cancels channel immediately"), Target.Life->IsReviveInProgress());
				Test->TestFalse(TEXT("Caster downed clears activation-owned tag"), Source.ASC->HasMatchingGameplayTag(GolemGameplayTags::State_Life_Reviving));
				Source.Life->ResetForCheckpoint();
				Source.ASC->RequestAbilityInput(GolemGameplayTags::Input_Party_Revive, true, Target.Actor.Get());
				UGolemAbilitySystemComponent::ApplyDamage(Enemy.Actor.Get(), Target.Actor.Get(), 20.0f);
				Test->TestTrue(TEXT("Target execution commits death during channel"), Target.Life->IsDead());
				Test->TestFalse(TEXT("Target death cancels caster cue immediately"), Source.ASC->IsGameplayCueActive(GolemGameplayTags::GameplayCue_Revive));
				Target.Life->ResetForCheckpoint();
				Target.Life->TrySetLifeTuning(0.3f, 0.2f, 300.0f, 0.35f);
				UGolemAbilitySystemComponent::ApplyDamage(Enemy.Actor.Get(), Target.Actor.Get(), 200.0f);
				WaitingUntil = Context.World->GetTimeSeconds() + 0.45f;
				break;
			case 4:
				Test->TestTrue(TEXT("Real world bleedout timer reaches dead state"), Target.Life->IsDead());
				Test->TestFalse(TEXT("Bleedout removes persistent downed cue"), Target.ASC->IsGameplayCueActive(GolemGameplayTags::GameplayCue_Downed));
				Test->TestTrue(TEXT("Exact grant remains revocable after all lifecycle changes"), Source.ASC->RevokeAbilitySet(SourceGrant).IsSuccess());
				Source.ASC->GrantAbilitySet(Set.Get(), TEXT("HeroDefaults"), SourceGrant);
				Target.Life->ResetForCheckpoint();
				Target.Life->TrySetLifeTuning(10.0f, 0.2f, 300.0f, 0.35f);
				UGolemAbilitySystemComponent::ApplyDamage(Enemy.Actor.Get(), Target.Actor.Get(), 200.0f);
				Test->TestTrue(TEXT("Actual channel is active before world teardown"), Source.ASC->RequestAbilityInput(
					GolemGameplayTags::Input_Party_Revive, true, Target.Actor.Get()).IsSuccess() && Target.Life->IsReviveInProgress());
				Test->TestTrue(TEXT("World EndPlay routes real begun-play lifecycle"), Context.StopPlay());
				Test->TestFalse(TEXT("Ended world cannot remain in begun-play state"), Context.World->HasBegunPlay());
				Test->TestFalse(TEXT("Source actor receives EndPlay"), Source.Actor->HasActorBegunPlay());
				Test->TestFalse(TEXT("Source ASC receives EndPlay"), Source.ASC->HasBegunPlay());
				Test->TestFalse(TEXT("Target life component receives EndPlay"), Target.Life->HasBegunPlay());
				Test->TestFalse(TEXT("World teardown releases active revive reservation"), Target.Life->IsReviveInProgress());
				Test->TestFalse(TEXT("World teardown removes caster channel cue"), Source.ASC->IsGameplayCueActive(GolemGameplayTags::GameplayCue_Revive));
				Test->TestEqual(TEXT("World teardown revokes source-owned grants"), Source.ASC->GetOwnedGrantCount(), 0);
				Test->TestEqual(TEXT("World teardown also revokes the other Golem's independent grants"), Other.ASC->GetOwnedGrantCount(), 0);
				return true;
			default:
				return true;
			}
			return false;
		}

	private:
		FAutomationTestBase* Test;
		FGolemTestWorld Context;
		TStrongObjectPtr<UGolemAbilitySetDefinition> Set;
		FGolemFixture Source;
		FGolemFixture Target;
		FGolemFixture Other;
		FGolemFixture Enemy;
		TWeakObjectPtr<AActor> Obstacle;
		FGolemAbilitySetHandle SourceGrant;
		FGolemAbilitySetHandle OtherGrant;
		int32 Phase = 0;
		double WaitingUntil = 0.0;
		double StartedAt = FPlatformTime::Seconds();
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGolemReviveLifecycleTest, "Golem.GAS.HeldReviveLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGolemReviveLifecycleTest::RunTest(const FString& Parameters)
{
	FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<GolemGASTests::FGolemReviveLifecycleCommand>(this));
	return true;
}

#endif
