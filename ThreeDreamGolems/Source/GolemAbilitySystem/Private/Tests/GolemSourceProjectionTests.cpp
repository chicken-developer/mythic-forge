#include "Projection/GolemSourceProjection.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Assets/GolemAssetManager.h"
#include "Attributes/GolemAttributeSet.h"
#include "Components/GolemAbilitySystemComponent.h"
#include "Components/GolemLifeComponent.h"
#include "Components/SceneComponent.h"
#include "Effects/GolemSourceProjectionEffect.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/WorldSettings.h"
#include "Misc/AutomationTest.h"
#include "Tags/GolemGameplayTags.h"
#include "UObject/StrongObjectPtr.h"
#include <limits>

namespace GolemProjectionTests
{
	struct FGolemFixture
	{
		TWeakObjectPtr<AActor> Actor;
		TWeakObjectPtr<UGolemAbilitySystemComponent> ASC;
		TWeakObjectPtr<UGolemAttributeSet> Attributes;
		TWeakObjectPtr<UGolemLifeComponent> Life;
	};

	class FGolemWorld final : public FNoncopyable
	{
	public:
		FGolemWorld()
			: GameInstance(NewObject<UGameInstance>(GEngine))
		{
			GameInstance->InitializeStandalone();
			World = GameInstance->GetWorld();
			World->SetShouldTick(false);
			World->GetWorldSettings()->DefaultGameMode = AGameModeBase::StaticClass();
			FURL URL;
			URL.Map = TEXT("GolemProjectionDomainTest");
			URL.AddOption(TEXT("game=/Script/Engine.GameModeBase"));
			World->SetGameMode(URL);
			World->InitializeActorsForPlay(URL);
			World->BeginPlay();
		}
		~FGolemWorld()
		{
			UWorld* OwnedWorld = World.Get();
			if (OwnedWorld != nullptr && OwnedWorld->HasBegunPlay())
			{
				OwnedWorld->EndPlay(EEndPlayReason::Quit);
			}
			GameInstance->Shutdown();
			if (OwnedWorld != nullptr)
			{
				OwnedWorld->DestroyWorld(false);
				GEngine->DestroyWorldContext(OwnedWorld);
			}
		}
		TWeakObjectPtr<UWorld> World;

	private:
		TStrongObjectPtr<UGameInstance> GameInstance;
	};

	FGolemFixture Spawn(UWorld* World, const FVector& Location, bool bParty = true)
	{
		FGolemFixture Fixture;
		Fixture.Actor = World->SpawnActor<AActor>();
		USceneComponent* Root = NewObject<USceneComponent>(Fixture.Actor.Get());
		Fixture.Actor->AddInstanceComponent(Root);
		Fixture.Actor->SetRootComponent(Root);
		Root->RegisterComponent();
		Fixture.Actor->SetActorLocation(Location);
		Fixture.ASC = NewObject<UGolemAbilitySystemComponent>(Fixture.Actor.Get());
		Fixture.Actor->AddInstanceComponent(Fixture.ASC.Get());
		Fixture.ASC->RegisterComponent();
		Fixture.Attributes = NewObject<UGolemAttributeSet>(Fixture.Actor.Get());
		Fixture.Life = NewObject<UGolemLifeComponent>(Fixture.Actor.Get());
		Fixture.Actor->AddInstanceComponent(Fixture.Life.Get());
		Fixture.Life->RegisterComponent();
		Fixture.ASC->InitializeGolemActor(Fixture.Actor.Get(), Fixture.Attributes.Get(), Fixture.Life.Get(),
			FGolemEntityId::Create(), FGolemAttributeDefaults(), bParty);
		if (!bParty)
		{
			Fixture.ASC->SetLooseGameplayTagCount(GolemGameplayTags::Role_Enemy, 1,
				EGameplayTagReplicationState::TagAndCountToAll);
		}
		return Fixture;
	}

	FGolemGASProjectionPlan Plan()
	{
		FGolemGASProjectionPlan Result;
		Result.SourceKey = TEXT("RPG");
		Result.DefinitionId = FPrimaryAssetId(TEXT("GolemHero"), TEXT("Training_Red"));
		Result.MaxHealthBonus = 50.0f;
		Result.MaxStaminaBonus = 40.0f;
		Result.MaxRuneEnergyBonus = 25.0f;
		Result.AttackPowerBonus = 10.0f;
		return Result;
	}

	bool Commit(UGolemAbilitySystemComponent* ASC, const FGolemGASProjectionPlan& Plan)
	{
		FGolemMutationFence Fence;
		FGolemProjectionCapability Capability;
		return ASC->BeginMutationFence(FGuid::NewGuid(), Fence).IsSuccess()
			&& ASC->PrepareSourceProjection(Fence, Plan, ASC->GetSourceProjection(Plan.SourceKey).Revision, Capability).IsSuccess()
			&& ASC->StageSourceProjection(Fence, Capability).IsSuccess()
			&& ASC->CommitSourceProjection(Fence, Capability).IsSuccess()
			&& ASC->PublishSourceProjection(Fence, Capability).IsSuccess()
			&& ASC->EndMutationFence(Fence).IsSuccess();
	}

	TArray<FActiveGameplayEffectHandle> StatHandles(UGolemAbilitySystemComponent* ASC)
	{
		TArray<FActiveGameplayEffectHandle> Result;
		for (FActiveGameplayEffectHandle Handle : ASC->GetActiveEffects(FGameplayEffectQuery()))
		{
			const FActiveGameplayEffect* Effect = ASC->GetActiveGameplayEffect(Handle);
			if (Effect != nullptr && Effect->Spec.Def->IsA<UGolemSourceProjectionEffect>())
			{
				Result.Add(Handle);
			}
		}
		return Result;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGolemProjectionValidationTest, "Golem.GAS.Projection.DefinitionContracts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGolemProjectionValidationTest::RunTest(const FString& Parameters)
{
	FGolemGASProjectionPlan Plan = GolemProjectionTests::Plan();
	TestTrue(TEXT("Finite aggregate source validates"), Plan.Validate().IsSuccess());
	Plan.AttackPowerBonus = std::numeric_limits<float>::quiet_NaN();
	TestEqual(TEXT("NaN bonus rejected"), Plan.Validate().Error, EGolemError::InvalidData);
	Plan = GolemProjectionTests::Plan();
	FGolemActionModifier Modifier;
	Modifier.AbilityTag = GolemGameplayTags::Ability_Combat_Rune;
	Modifier.CostScale = 0.5f;
	Plan.ActionModifiers = { Modifier, Modifier };
	TestEqual(TEXT("Duplicate exact action rejected"), Plan.Validate().Error, EGolemError::DuplicateId);
	Plan.ActionModifiers = { Modifier };
	Plan.OptionalAbilitySetId = FPrimaryAssetId(TEXT("GolemAbilitySet"), TEXT("Foundation"));
	Plan.OptionalAbilitySetIds = { Plan.OptionalAbilitySetId };
	TestEqual(TEXT("Legacy and array duplicate set rejected"), Plan.Validate().Error, EGolemError::DuplicateId);
	Plan = GolemProjectionTests::Plan();
	Plan.MaxHealthBonus = Plan.MaxStaminaBonus = Plan.MaxRuneEnergyBonus = Plan.AttackPowerBonus = 0.0f;
	TestTrue(TEXT("Empty source is a valid unequip projection"), Plan.Validate().IsSuccess() && !Plan.HasStatBonuses());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGolemProjectionTransactionTest, "Golem.GAS.Projection.AtomicSourcesAndFence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGolemProjectionTransactionTest::RunTest(const FString& Parameters)
{
	GolemProjectionTests::FGolemWorld World;
	const GolemProjectionTests::FGolemFixture Source = GolemProjectionTests::Spawn(World.World.Get(), FVector::ZeroVector);
	const GolemProjectionTests::FGolemFixture Enemy = GolemProjectionTests::Spawn(World.World.Get(), FVector(100, 0, 0), false);
	UGolemDefinition* Definition = nullptr;
	UGolemAssetManager* Manager = UGolemAssetManager::GetGolemAssetManager();
	if (!TestNotNull(TEXT("Actual catalog manager exists"), Manager) || !TestTrue(TEXT("Real foundation set resolves"),
		Manager->ResolveDefinition(FPrimaryAssetId(TEXT("GolemAbilitySet"), TEXT("Foundation")),
			UGolemAbilitySetDefinition::StaticClass(), Definition).IsSuccess()))
	{
		return false;
	}
	FGolemAbilitySetHandle BaselineGrant;
	TestTrue(TEXT("Baseline shared GAS set granted"), Source.ASC->GrantAbilitySet(
		CastChecked<UGolemAbilitySetDefinition>(Definition), TEXT("HeroDefaults"), BaselineGrant).IsSuccess());
	FGameplayAbilitySpecHandle Pulse;
	for (const FGameplayAbilitySpec& Spec : Source.ASC->GetActivatableAbilities())
	{
		if (Spec.GetDynamicSpecSourceTags().HasTagExact(GolemGameplayTags::Input_Ability_Test))
		{
			Pulse = Spec.Handle;
		}
	}
	Source.ASC->SetNumericAttributeBase(UGolemAttributeSet::GetHealthAttribute(), 73.0f);
	FGolemGASProjectionPlan Plan = GolemProjectionTests::Plan();
	TestTrue(TEXT("Real infinite passive projection commits"), GolemProjectionTests::Commit(Source.ASC.Get(), Plan));
	TestEqual(TEXT("Equip raises max through additive GE"), Source.Attributes->GetMaxHealth(), 150.0f);
	TestEqual(TEXT("Equip never heals current health"), Source.Attributes->GetHealth(), 73.0f);
	TestEqual(TEXT("Real AP bonus applied"), Source.Attributes->GetAttackPower(), 35.0f);
	const TArray<FActiveGameplayEffectHandle> OriginalHandles = GolemProjectionTests::StatHandles(Source.ASC.Get());
	if (!TestEqual(TEXT("Exactly one owned passive stat effect"), OriginalHandles.Num(), 1))
	{
		return false;
	}
	const FActiveGameplayEffectHandle Original = OriginalHandles[0];
	int32 Published = 0;
	const FDelegateHandle Publication = Source.ASC->OnSourceProjectionChanged.AddLambda(
		[&Published](const FGolemSourceProjection&)
		{
			++Published;
		});
	int32 Rejected = 0;
	FGameplayEffectApplicationQuery Reject = FGameplayEffectApplicationQuery::CreateLambda(
		[&Rejected](const FActiveGameplayEffectsContainer&, const FGameplayEffectSpec& Spec)
		{
			if (Spec.Def->IsA<UGolemSourceProjectionEffect>())
			{
				++Rejected;
				return false;
			}
			return true;
		});
	const FDelegateHandle RejectHandle = Reject.GetHandle();
	Source.ASC->GameplayEffectApplicationQueries.Add(MoveTemp(Reject));
	FGolemMutationFence Fence;
	FGolemProjectionCapability Capability;
	Plan.AttackPowerBonus = 20.0f;
	TestTrue(TEXT("Replacement acquires exact fence"), Source.ASC->BeginMutationFence(FGuid::NewGuid(), Fence).IsSuccess());
	TestTrue(TEXT("Replacement prepares without mutating"), Source.ASC->PrepareSourceProjection(Fence, Plan, 1, Capability).IsSuccess());
	TestEqual(TEXT("Actual GE application rejection fails stage"), Source.ASC->StageSourceProjection(Fence, Capability).Error,
		EGolemError::InvalidState);
	TestEqual(TEXT("Query rejected the actual candidate GE"), Rejected, 1);
	TestTrue(TEXT("Failed exact capability abort is idempotent"), Source.ASC->AbortSourceProjection(Fence, Capability).IsSuccess());
	TestTrue(TEXT("Failed stage releases only its fence"), Source.ASC->EndMutationFence(Fence).IsSuccess());
	TestTrue(TEXT("Rollback preserves identical active old handle"), Source.ASC->GetActiveGameplayEffect(Original) != nullptr);
	TestEqual(TEXT("Rollback retains committed AP"), Source.Attributes->GetAttackPower(), 35.0f);
	TestEqual(TEXT("Rollback retains revision"), Source.ASC->GetSourceProjection(TEXT("RPG")).Revision, int64(1));
	TestEqual(TEXT("Rollback publishes nothing"), Published, 0);
	Source.ASC->GameplayEffectApplicationQueries.RemoveAll([RejectHandle](const FGameplayEffectApplicationQuery& Query)
		{
			return Query.GetHandle() == RejectHandle;
		});
	int32 RawCallbacks = 0;
	const FDelegateHandle RawObserver = Source.ASC->OnActiveGameplayEffectAddedDelegateToSelf.AddLambda(
		[this, &Source, &Enemy, &RawCallbacks, Pulse, &BaselineGrant](UAbilitySystemComponent*,
			const FGameplayEffectSpec& Spec, FActiveGameplayEffectHandle)
		{
			if (!Spec.Def->IsA<UGolemSourceProjectionEffect>())
			{
				return;
			}
			++RawCallbacks;
			TestEqual(TEXT("Raw observer reads old committed projection"),
				Source.ASC->GetSourceProjection(TEXT("RPG")).Revision, int64(1));
			TestEqual(TEXT("Reentrant semantic input is fenced"), Source.ASC->RequestAbilityInput(
				GolemGameplayTags::Input_Ability_Test, true, Enemy.Actor.Get()).Error, EGolemError::Conflict);
			TestFalse(TEXT("Direct real GAS activation is fenced"), Source.ASC->TryActivateAbility(Pulse, false));
			TestEqual(TEXT("Reentrant source damage is fenced"), UGolemAbilitySystemComponent::ApplyDamage(
				Source.Actor.Get(), Enemy.Actor.Get(), 20.0f).Error, EGolemError::Conflict);
			TestEqual(TEXT("Reentrant target damage is fenced"), UGolemAbilitySystemComponent::ApplyDamage(
				Enemy.Actor.Get(), Source.Actor.Get(), 20.0f).Error, EGolemError::Conflict);
			TestEqual(TEXT("Reentrant unrelated source revoke is fenced"),
				Source.ASC->RevokeAbilitySet(BaselineGrant).Error, EGolemError::Conflict);
			FGolemMutationFence Nested;
			TestEqual(TEXT("Nested transaction cannot acquire another fence"),
				Source.ASC->BeginMutationFence(FGuid::NewGuid(), Nested).Error, EGolemError::Conflict);
			TestTrue(TEXT("Input release remains safe while fenced"),
				Source.ASC->RequestAbilityInput(GolemGameplayTags::Input_Ability_Test, false).IsSuccess());
		});
	int32 RetiredRemovals = 0;
	const FDelegateHandle RemovalObserver = Source.ASC->OnAnyGameplayEffectRemovedDelegate().AddLambda(
		[this, &Source, &Enemy, &RetiredRemovals, Original, Pulse](const FActiveGameplayEffect& Effect)
		{
			if (Effect.Handle != Original)
			{
				return;
			}
			++RetiredRemovals;
			TestEqual(TEXT("Actual old-effect removal reads final projection revision"),
				Source.ASC->GetSourceProjection(TEXT("RPG")).Revision, int64(2));
			TestEqual(TEXT("Removal observer reads final AP after real aggregator removal"),
				Source.Attributes->GetAttackPower(), 45.0f);
			TestTrue(TEXT("Retired cleanup retains the transaction fence"), Source.ASC->IsMutationFenced());
			TestEqual(TEXT("Removal callback cannot issue semantic gameplay input"), Source.ASC->RequestAbilityInput(
				GolemGameplayTags::Input_Ability_Test, true, Enemy.Actor.Get()).Error, EGolemError::Conflict);
			TestFalse(TEXT("Removal callback cannot activate the actual GAS spec"), Source.ASC->TryActivateAbility(Pulse, false));
			TestEqual(TEXT("Removal callback cannot apply gameplay damage"), UGolemAbilitySystemComponent::ApplyDamage(
				Source.Actor.Get(), Enemy.Actor.Get(), 20.0f).Error, EGolemError::Conflict);
			FGolemMutationFence Nested;
			TestEqual(TEXT("Removal callback cannot open another projection transaction"),
				Source.ASC->BeginMutationFence(FGuid::NewGuid(), Nested).Error, EGolemError::Conflict);
		});
	TestTrue(TEXT("Replacement fences before actual staged callbacks"), Source.ASC->BeginMutationFence(FGuid::NewGuid(), Fence).IsSuccess());
	TestTrue(TEXT("Replacement prepares exact capability"), Source.ASC->PrepareSourceProjection(Fence, Plan, 1, Capability).IsSuccess());
	TestTrue(TEXT("Real replacement stages under raw application callbacks"), Source.ASC->StageSourceProjection(Fence, Capability).IsSuccess());
	TestTrue(TEXT("Pure staged preflight validates genuine candidate handles"),
		Source.ASC->ValidateStagedSourceProjection(Fence, Capability).IsSuccess());
	TestEqual(TEXT("Staged preflight retains old committed readmodel"), Source.ASC->GetSourceProjection(TEXT("RPG")).Revision, int64(1));
	TestTrue(TEXT("Replacement promotes values without removal callbacks"), Source.ASC->CommitSourceProjection(Fence, Capability).IsSuccess());
	TestEqual(TEXT("Callback-free commit emits no old-effect removal"), RetiredRemovals, 0);
	TestEqual(TEXT("Callback-free commit emits no projection event"), Published, 0);
	TestEqual(TEXT("True commit makes final immutable value available"), Source.ASC->GetSourceProjection(TEXT("RPG")).Revision, int64(2));
	TestNotNull(TEXT("Old exact effect remains until coordinated publication"), Source.ASC->GetActiveGameplayEffect(Original));
	TestTrue(TEXT("Publication drains old handles and emits final event"), Source.ASC->PublishSourceProjection(Fence, Capability).IsSuccess());
	TestEqual(TEXT("Actual retired effect removed exactly once"), RetiredRemovals, 1);
	TestTrue(TEXT("Exact repeat publication is idempotent"), Source.ASC->PublishSourceProjection(Fence, Capability).IsSuccess());
	TestEqual(TEXT("Repeat publication never drains retired effect again"), RetiredRemovals, 1);
	TestTrue(TEXT("Successful publication releases exact fence"), Source.ASC->EndMutationFence(Fence).IsSuccess());
	Source.ASC->OnAnyGameplayEffectRemovedDelegate().Remove(RemovalObserver);
	Source.ASC->OnActiveGameplayEffectAddedDelegateToSelf.Remove(RawObserver);
	TestEqual(TEXT("Raw GAS application callback actually executed"), RawCallbacks, 1);
	TestEqual(TEXT("One final projection publication"), Published, 1);
	TestEqual(TEXT("Successful replacement revision advances once"), Source.ASC->GetSourceProjection(TEXT("RPG")).Revision, int64(2));
	TestEqual(TEXT("Replacement AP is final, not old plus candidate"), Source.Attributes->GetAttackPower(), 45.0f);
	TestEqual(TEXT("Fenced callbacks caused no source health mutation"), Source.Attributes->GetHealth(), 73.0f);
	TestEqual(TEXT("Fenced callbacks caused no target damage"), Enemy.Attributes->GetHealth(), 100.0f);
	TestTrue(TEXT("Old handle removed only after successful commit"), Source.ASC->GetActiveGameplayEffect(Original) == nullptr);
	TestNotNull(TEXT("Original action spec survives stat changes"), Source.ASC->FindAbilitySpecFromHandle(Pulse));
	const FActiveGameplayEffectHandle Current = GolemProjectionTests::StatHandles(Source.ASC.Get())[0];
	TestTrue(TEXT("Checkpoint fills actual projected maxima"), Source.Life->ResetForCheckpoint().IsSuccess());
	TestEqual(TEXT("Retry projected health"), Source.Attributes->GetHealth(), 150.0f);
	TestEqual(TEXT("Retry projected stamina"), Source.Attributes->GetStamina(), 140.0f);
	TestEqual(TEXT("Retry projected energy"), Source.Attributes->GetRuneEnergy(), 125.0f);
	TestTrue(TEXT("Retry keeps exact passive handle"), Source.ASC->GetActiveGameplayEffect(Current) != nullptr);
	TestEqual(TEXT("Retry keeps projection revision"), Source.ASC->GetSourceProjection(TEXT("RPG")).Revision, int64(2));
	const GolemProjectionTests::FGolemFixture Reconstructed = GolemProjectionTests::Spawn(World.World.Get(), FVector(300, 0, 0));
	TestTrue(TEXT("Value-only projection reconstructs on a fresh ASC"), GolemProjectionTests::Commit(
		Reconstructed.ASC.Get(), Source.ASC->GetSourceProjection(TEXT("RPG")).Plan));
	TestEqual(TEXT("Reconstruction AP matches committed source"), Reconstructed.Attributes->GetAttackPower(), 45.0f);
	TestEqual(TEXT("Reconstruction max matches committed source"), Reconstructed.Attributes->GetMaxHealth(), 150.0f);
	TestEqual(TEXT("Reconstruction does not heal on equip"), Reconstructed.Attributes->GetHealth(), 100.0f);
	Plan = FGolemGASProjectionPlan();
	Plan.SourceKey = TEXT("RPG");
	Plan.DefinitionId = FPrimaryAssetId(TEXT("GolemHero"), TEXT("Training_Red"));
	TestTrue(TEXT("Empty projection performs real unequip"), GolemProjectionTests::Commit(Source.ASC.Get(), Plan));
	TestEqual(TEXT("Max shrink clamps health"), Source.Attributes->GetHealth(), 100.0f);
	TestEqual(TEXT("Max shrink clamps stamina"), Source.Attributes->GetStamina(), 100.0f);
	TestEqual(TEXT("Max shrink clamps energy"), Source.Attributes->GetRuneEnergy(), 100.0f);
	TestEqual(TEXT("Unequip removes AP bonus"), Source.Attributes->GetAttackPower(), 25.0f);
	TestEqual(TEXT("Empty source still advances revision"), Source.ASC->GetSourceProjection(TEXT("RPG")).Revision, int64(3));
	TestEqual(TEXT("No RPG passive remains after unequip"), GolemProjectionTests::StatHandles(Source.ASC.Get()).Num(), 0);
	Source.ASC->OnSourceProjectionChanged.Remove(Publication);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGolemProjectionGrantLifecycleTest, "Golem.GAS.Projection.OptionalGrantsAndLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGolemProjectionGrantLifecycleTest::RunTest(const FString& Parameters)
{
	GolemProjectionTests::FGolemWorld World;
	const GolemProjectionTests::FGolemFixture Source = GolemProjectionTests::Spawn(World.World.Get(), FVector::ZeroVector);
	const GolemProjectionTests::FGolemFixture Foreign = GolemProjectionTests::Spawn(World.World.Get(), FVector(300, 0, 0));
	FGolemGASProjectionPlan Plan = GolemProjectionTests::Plan();
	Plan.OptionalAbilitySetIds = { FPrimaryAssetId(TEXT("GolemAbilitySet"), TEXT("Foundation")),
		FPrimaryAssetId(TEXT("GolemAbilitySet"), TEXT("FoundationCombat")) };
	FGolemActionModifier Modifier;
	Modifier.AbilityTag = GolemGameplayTags::Ability_Combat_Rune;
	Modifier.CostScale = 0.5f;
	Plan.ActionModifiers.Add(Modifier);
	if (!TestTrue(TEXT("Both real optional sets commit on one aggregate source"), GolemProjectionTests::Commit(Source.ASC.Get(), Plan)))
	{
		return false;
	}
	TestEqual(TEXT("Both optional sources own actual grants"), Source.ASC->GetOwnedGrantCount(), 2);
	TestEqual(TEXT("Both unique action sets give actual specs"), Source.ASC->GetActivatableAbilities().Num(), 11);
	TArray<FGameplayAbilitySpecHandle> OriginalSpecs;
	for (const FGameplayAbilitySpec& Spec : Source.ASC->GetActivatableAbilities())
	{
		OriginalSpecs.Add(Spec.Handle);
	}
	const FActiveGameplayEffectHandle OriginalEffect = GolemProjectionTests::StatHandles(Source.ASC.Get())[0];
	int32 CandidateSpecCount = 0;
	FGameplayEffectApplicationQuery Reject = FGameplayEffectApplicationQuery::CreateLambda(
		[&Source, &CandidateSpecCount](const FActiveGameplayEffectsContainer&, const FGameplayEffectSpec& Spec)
		{
			if (Spec.Def->IsA<UGolemSourceProjectionEffect>())
			{
				CandidateSpecCount = Source.ASC->GetActivatableAbilities().Num();
				return false;
			}
			return true;
		});
	const FDelegateHandle QueryHandle = Reject.GetHandle();
	Source.ASC->GameplayEffectApplicationQueries.Add(MoveTemp(Reject));
	FGolemMutationFence Fence;
	FGolemProjectionCapability Capability;
	Plan.AttackPowerBonus = 20.0f;
	TestTrue(TEXT("Multi-set replacement fences"), Source.ASC->BeginMutationFence(FGuid::NewGuid(), Fence).IsSuccess());
	TestTrue(TEXT("Old own input tags may be replaced atomically"), Source.ASC->PrepareSourceProjection(Fence, Plan, 1, Capability).IsSuccess());
	TestEqual(TEXT("Real passive rejection rolls back already staged specs"),
		Source.ASC->StageSourceProjection(Fence, Capability).Error, EGolemError::InvalidState);
	TestEqual(TEXT("Query actually observed new specs alongside originals before rejection"), CandidateSpecCount, 22);
	TestEqual(TEXT("Failed candidate clears only its own eleven specs"), Source.ASC->GetActivatableAbilities().Num(), 11);
	for (FGameplayAbilitySpecHandle Handle : OriginalSpecs)
	{
		TestNotNull(TEXT("Every exact original spec handle survives rollback"), Source.ASC->FindAbilitySpecFromHandle(Handle));
	}
	TestNotNull(TEXT("Exact original passive handle survives partial-grant rollback"), Source.ASC->GetActiveGameplayEffect(OriginalEffect));
	TestEqual(TEXT("Committed grant count remains two"), Source.ASC->GetOwnedGrantCount(), 2);
	TestTrue(TEXT("Exact failed capability abort repeats safely"), Source.ASC->AbortSourceProjection(Fence, Capability).IsSuccess());
	TestTrue(TEXT("Exact failed fence ends"), Source.ASC->EndMutationFence(Fence).IsSuccess());
	Source.ASC->GameplayEffectApplicationQueries.RemoveAll([QueryHandle](const FGameplayEffectApplicationQuery& Query)
		{
			return Query.GetHandle() == QueryHandle;
		});
	TestTrue(TEXT("Source prepares real staged ownership transfer regression"), Source.ASC->BeginMutationFence(FGuid::NewGuid(), Fence).IsSuccess()
		&& Source.ASC->PrepareSourceProjection(Fence, Plan, 1, Capability).IsSuccess());
	const FDelegateHandle OwnershipCallback = Source.ASC->OnActiveGameplayEffectAddedDelegateToSelf.AddLambda(
		[&Source](UAbilitySystemComponent*, const FGameplayEffectSpec& Spec, FActiveGameplayEffectHandle)
		{
			if (Spec.Def->IsA<UGolemSourceProjectionEffect>())
			{
				Source.ASC->HandleControllerChanged();
			}
		});
	TestEqual(TEXT("Synchronous lifecycle callback cancels staged generation"),
		Source.ASC->StageSourceProjection(Fence, Capability).Error, EGolemError::Cancelled);
	Source.ASC->OnActiveGameplayEffectAddedDelegateToSelf.Remove(OwnershipCallback);
	TestFalse(TEXT("Lifecycle clears its obsolete fence"), Source.ASC->IsMutationFenced());
	TestEqual(TEXT("Lifecycle rollback keeps committed revision"), Source.ASC->GetSourceProjection(TEXT("RPG")).Revision, int64(1));
	TestEqual(TEXT("Lifecycle rollback keeps committed AP"), Source.Attributes->GetAttackPower(), 35.0f);
	TestEqual(TEXT("Lifecycle rollback removes all staged optional specs"), Source.ASC->GetActivatableAbilities().Num(), 11);
	for (FGameplayAbilitySpecHandle Handle : OriginalSpecs)
	{
		TestNotNull(TEXT("Lifecycle retains each original granted spec"), Source.ASC->FindAbilitySpecFromHandle(Handle));
	}
	TestNotNull(TEXT("Lifecycle retains original passive handle"), Source.ASC->GetActiveGameplayEffect(OriginalEffect));
	TestTrue(TEXT("Fresh generation can prepare after callback cancellation"), Source.ASC->BeginMutationFence(FGuid::NewGuid(), Fence).IsSuccess()
		&& Source.ASC->PrepareSourceProjection(Fence, Plan, 1, Capability).IsSuccess());
	FGolemProjectionCapability Forged = Capability;
	Forged.Token = FGuid::NewGuid();
	TestEqual(TEXT("Forged projection cannot stage"), Source.ASC->StageSourceProjection(Fence, Forged).Error, EGolemError::NotOwned);
	TestEqual(TEXT("Foreign ASC cannot consume capability"), Foreign.ASC->StageSourceProjection(Fence, Capability).Error, EGolemError::NotOwned);
	TestTrue(TEXT("Own exact abort succeeds"), Source.ASC->AbortSourceProjection(Fence, Capability).IsSuccess());
	TestTrue(TEXT("Own exact abort is idempotent"), Source.ASC->AbortSourceProjection(Fence, Capability).IsSuccess());
	TestTrue(TEXT("Own exact fence ends"), Source.ASC->EndMutationFence(Fence).IsSuccess());
	TestTrue(TEXT("Own fence end repeats harmlessly"), Source.ASC->EndMutationFence(Fence).IsSuccess());
	TestTrue(TEXT("Actual candidate stages for final preflight invalidation"),
		Source.ASC->BeginMutationFence(FGuid::NewGuid(), Fence).IsSuccess()
		&& Source.ASC->PrepareSourceProjection(Fence, Plan, 1, Capability).IsSuccess()
		&& Source.ASC->StageSourceProjection(Fence, Capability).IsSuccess());
	FActiveGameplayEffectHandle StagedEffect;
	for (FActiveGameplayEffectHandle Handle : GolemProjectionTests::StatHandles(Source.ASC.Get()))
	{
		if (Handle != OriginalEffect)
		{
			StagedEffect = Handle;
		}
	}
	TestTrue(TEXT("Raw SDK removes the exact staged candidate effect"),
		StagedEffect.IsValid() && Source.ASC->RemoveActiveGameplayEffect(StagedEffect));
	TestEqual(TEXT("Pure final preflight detects real staged-effect removal"),
		Source.ASC->ValidateStagedSourceProjection(Fence, Capability).Error, EGolemError::InvalidState);
	TestEqual(TEXT("Commit rejects invalidated candidate before value promotion"),
		Source.ASC->CommitSourceProjection(Fence, Capability).Error, EGolemError::InvalidState);
	TestEqual(TEXT("Final preflight failure preserves original public revision"),
		Source.ASC->GetSourceProjection(TEXT("RPG")).Revision, int64(1));
	TestNotNull(TEXT("Final preflight failure preserves exact original effect"), Source.ASC->GetActiveGameplayEffect(OriginalEffect));
	TestTrue(TEXT("Invalidated candidate abort drains only staged specs"),
		Source.ASC->AbortSourceProjection(Fence, Capability).IsSuccess()
		&& Source.ASC->EndMutationFence(Fence).IsSuccess());
	TestEqual(TEXT("Final preflight rollback restores original spec count"), Source.ASC->GetActivatableAbilities().Num(), 11);
	Plan = GolemProjectionTests::Plan();
	Plan.MaxHealthBonus = Plan.MaxStaminaBonus = Plan.MaxRuneEnergyBonus = Plan.AttackPowerBonus = 0.0f;
	TestTrue(TEXT("Unequip revokes every optional set and passive"), GolemProjectionTests::Commit(Source.ASC.Get(), Plan));
	TestEqual(TEXT("No source specs remain after multi-set unequip"), Source.ASC->GetActivatableAbilities().Num(), 0);
	TestEqual(TEXT("No source grants remain after multi-set unequip"), Source.ASC->GetOwnedGrantCount(), 0);
	TestEqual(TEXT("No source effect remains after multi-set unequip"), GolemProjectionTests::StatHandles(Source.ASC.Get()).Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGolemProjectionPublicationLifetimeTest, "Golem.GAS.Projection.PublicationLifetime",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGolemProjectionPublicationLifetimeTest::RunTest(const FString& Parameters)
{
	GolemProjectionTests::FGolemWorld World;
	const GolemProjectionTests::FGolemFixture Source = GolemProjectionTests::Spawn(World.World.Get(), FVector::ZeroVector);
	TStrongObjectPtr<AActor> OwnedActor(Source.Actor.Get());
	TStrongObjectPtr<UGolemAbilitySystemComponent> OwnedASC(Source.ASC.Get());
	FGolemGASProjectionPlan Plan = GolemProjectionTests::Plan();
	Plan.OptionalAbilitySetIds = { FPrimaryAssetId(TEXT("GolemAbilitySet"), TEXT("Foundation")) };
	if (!TestTrue(TEXT("Lifecycle fixture owns actual initial projection and optional grant"),
		GolemProjectionTests::Commit(OwnedASC.Get(), Plan)))
	{
		return false;
	}
	const FActiveGameplayEffectHandle Original = GolemProjectionTests::StatHandles(OwnedASC.Get())[0];
	int32 RemovedEffects = 0;
	int32 OwnerEndCallbacks = 0;
	int32 ProjectionEvents = 0;
	int32 GrantEvents = 0;
	const FDelegateHandle ProjectionObserver = OwnedASC->OnSourceProjectionChanged.AddLambda(
		[&ProjectionEvents](const FGolemSourceProjection&)
		{
			++ProjectionEvents;
		});
	const FDelegateHandle GrantObserver = OwnedASC->OnAbilityGrantsChanged.AddLambda(
		[&GrantEvents]()
		{
			++GrantEvents;
		});
	const FDelegateHandle RemovalObserver = OwnedASC->OnAnyGameplayEffectRemovedDelegate().AddLambda(
		[this, &OwnedActor, &OwnedASC, &RemovedEffects, &OwnerEndCallbacks, Original](const FActiveGameplayEffect& Effect)
		{
			if (!Effect.Spec.Def->IsA<UGolemSourceProjectionEffect>())
			{
				return;
			}
			++RemovedEffects;
			if (Effect.Handle == Original)
			{
				++OwnerEndCallbacks;
				TestEqual(TEXT("Real removal callback sees already committed replacement revision"),
					OwnedASC->GetSourceProjection(TEXT("RPG")).Revision, int64(2));
				TestEqual(TEXT("Real removal callback sees already committed grant count"), OwnedASC->GetOwnedGrantCount(), 0);
				TestTrue(TEXT("Actual owner destruction occurs inside retired-effect callback"), OwnedActor->Destroy());
			}
		});
	Plan.OptionalAbilitySetIds.Reset();
	Plan.AttackPowerBonus = 20.0f;
	FGolemMutationFence Fence;
	FGolemProjectionCapability Capability;
	TestTrue(TEXT("Replacement stages before owner-end injection"), OwnedASC->BeginMutationFence(FGuid::NewGuid(), Fence).IsSuccess()
		&& OwnedASC->PrepareSourceProjection(Fence, Plan, 1, Capability).IsSuccess()
		&& OwnedASC->StageSourceProjection(Fence, Capability).IsSuccess());
	TestTrue(TEXT("Lifetime candidate passes pure actual-handle preflight"),
		OwnedASC->ValidateStagedSourceProjection(Fence, Capability).IsSuccess());
	TestTrue(TEXT("Lifetime candidate commits without callbacks"), OwnedASC->CommitSourceProjection(Fence, Capability).IsSuccess());
	TestEqual(TEXT("Commit cannot invoke premature owner destruction"), OwnerEndCallbacks, 0);
	TestTrue(TEXT("Committed publication remains success after actual teardown callback"),
		OwnedASC->PublishSourceProjection(Fence, Capability).IsSuccess());
	TestEqual(TEXT("Owner-end injection actually ran once"), OwnerEndCallbacks, 1);
	TestEqual(TEXT("Retired and committed stat handles both drain once"), RemovedEffects, 2);
	TestEqual(TEXT("No post-EndPlay projection delegate emitted"), ProjectionEvents, 0);
	TestEqual(TEXT("No post-EndPlay grant delegate emitted"), GrantEvents, 0);
	TestTrue(TEXT("Actual owner is unavailable after destruction"), !IsValid(OwnedActor.Get()) || OwnedActor->IsActorBeingDestroyed());
	TestEqual(TEXT("Teardown drains all source specs"), OwnedASC->GetActivatableAbilities().Num(), 0);
	TestEqual(TEXT("Teardown drains every source stat effect"), GolemProjectionTests::StatHandles(OwnedASC.Get()).Num(), 0);
	TestEqual(TEXT("Ended lifecycle invalidates exact fence capability"), OwnedASC->EndMutationFence(Fence).Error, EGolemError::NotOwned);
	OwnedASC->OnAnyGameplayEffectRemovedDelegate().Remove(RemovalObserver);
	OwnedASC->OnSourceProjectionChanged.Remove(ProjectionObserver);
	OwnedASC->OnAbilityGrantsChanged.Remove(GrantObserver);
	return true;
}

#endif
