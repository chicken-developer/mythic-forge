#include "Voids/GolemVoidTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Attributes/GolemAttributeSet.h"
#include "Components/GolemAbilitySystemComponent.h"
#include "Components/GolemLifeComponent.h"
#include "Components/GolemCombatComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/WorldSettings.h"
#include "Misc/AutomationTest.h"
#include "Projectiles/GolemColorProjectile.h"
#include "Tags/GolemGameplayTags.h"
#include "UObject/StrongObjectPtr.h"
#include "Voids/GolemColorVoidComponent.h"
#include "Voids/GolemVoidHostComponent.h"
#include "Xenon/GolemXenon.h"
#include "Xenon/GolemXenonBrainComponent.h"
#include "Xenon/GolemXenonDefinition.h"

namespace GolemColorVoidTests
{
	constexpr uint8 Red = GolemElements::Mask(EGolemElement::Earth);
	constexpr uint8 Yellow = GolemElements::Mask(EGolemElement::Sky);
	constexpr uint8 Orange = GolemElements::Mask(EGolemElement::Earth, EGolemElement::Sky);
	constexpr uint8 Purple = GolemElements::Mask(EGolemElement::Earth, EGolemElement::Sea);
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

	class FTestWorld final : public FNoncopyable
	{
	public:
		FTestWorld()
			: GameInstance(NewObject<UGameInstance>(GEngine))
		{
			GameInstance->InitializeStandalone();
			World = GameInstance->GetWorld();
			World->SetShouldTick(false);
			World->GetWorldSettings()->DefaultGameMode = AGameModeBase::StaticClass();
			FURL URL;
			URL.Map = TEXT("GolemColorVoidTest");
			URL.AddOption(TEXT("game=/Script/Engine.GameModeBase"));
			World->SetGameMode(URL);
			World->InitializeActorsForPlay(URL);
			World->BeginPlay();
		}

		~FTestWorld()
		{
			if (World.IsValid() && World->HasBegunPlay())
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
			World.Reset();
		}

		TWeakObjectPtr<UWorld> World;

	private:
		TStrongObjectPtr<UGameInstance> GameInstance;
	};

	struct FCombatant
	{
		TWeakObjectPtr<AActor> Actor;
		TWeakObjectPtr<UGolemAbilitySystemComponent> ASC;
		TWeakObjectPtr<UGolemAttributeSet> Attributes;
		TWeakObjectPtr<UGolemLifeComponent> Life;
		TWeakObjectPtr<UGolemVoidHostComponent> Host;
		TArray<UGolemColorVoidComponent*> Slots;
		UGolemColorVoidComponent* Core = nullptr;
	};

	FCombatant SpawnCombatant(UWorld* World, const bool bParty, const float MaxHealth)
	{
		FCombatant Result;
		Result.Actor = World->SpawnActor<AActor>();
		USceneComponent* Root = NewObject<USceneComponent>(Result.Actor.Get());
		Result.Actor->AddInstanceComponent(Root);
		Result.Actor->SetRootComponent(Root);
		Root->RegisterComponent();
		Result.Actor->SetReplicates(true);
		Result.ASC = NewObject<UGolemAbilitySystemComponent>(Result.Actor.Get());
		Result.Actor->AddInstanceComponent(Result.ASC.Get());
		Result.ASC->RegisterComponent();
		Result.Attributes = NewObject<UGolemAttributeSet>(Result.Actor.Get());
		Result.Life = NewObject<UGolemLifeComponent>(Result.Actor.Get());
		Result.Actor->AddInstanceComponent(Result.Life.Get());
		Result.Life->RegisterComponent();
		FGolemAttributeDefaults Defaults;
		Defaults.MaxHealth = MaxHealth;
		Result.ASC->InitializeGolemActor(Result.Actor.Get(), Result.Attributes.Get(), Result.Life.Get(),
			FGolemEntityId::Create(), Defaults, bParty);
		if (!bParty)
		{
			Result.ASC->SetLooseGameplayTagCount(GolemGameplayTags::Role_Enemy, 1,
				EGameplayTagReplicationState::TagAndCountToAll);
		}
		return Result;
	}

	UGolemColorVoidComponent* AddVoid(AActor* Owner)
	{
		UGolemColorVoidComponent* Void = NewObject<UGolemColorVoidComponent>(Owner);
		Owner->AddInstanceComponent(Void);
		Void->SetupAttachment(Owner->GetRootComponent());
		Void->RegisterComponent();
		return Void;
	}

	FCombatant SpawnXenon(UWorld* World, const FGolemVoidHostTuning& Tuning, const TArray<uint8>& Plan,
		const int32 SlotCount, const float MaxHealth, FGolemResult& OutResult)
	{
		FCombatant Xenon = SpawnCombatant(World, false, MaxHealth);
		for (int32 Index = 0; Index < SlotCount; ++Index)
		{
			Xenon.Slots.Add(AddVoid(Xenon.Actor.Get()));
		}
		if (Tuning.bHasWhiteCore)
		{
			Xenon.Core = AddVoid(Xenon.Actor.Get());
		}
		Xenon.Host = NewObject<UGolemVoidHostComponent>(Xenon.Actor.Get());
		Xenon.Actor->AddInstanceComponent(Xenon.Host.Get());
		Xenon.Host->RegisterComponent();
		OutResult = Xenon.Host->InitializeHost(Xenon.ASC.Get(), Tuning, Xenon.Slots, Plan, Xenon.Core);
		return Xenon;
	}

	/** Stands in for the capsule of a real Xenon: a solid body the size of the grey torso. */
	USphereComponent* AddBody(AActor* Owner, const float Radius)
	{
		USphereComponent* Body = NewObject<USphereComponent>(Owner);
		Owner->AddInstanceComponent(Body);
		Body->SetupAttachment(Owner->GetRootComponent());
		Body->InitSphereRadius(Radius);
		Body->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Body->SetCollisionObjectType(ECC_Pawn);
		Body->RegisterComponent();
		return Body;
	}

	/** Fires one shot along +X from the origin side and flies it until it stops. */
	AGolemColorProjectile* FireShot(UWorld* World, AActor* Source, const FVector& From, const FGolemProjectileShot& Shot)
	{
		AGolemColorProjectile* Projectile = World->SpawnActor<AGolemColorProjectile>(From, FRotator::ZeroRotator);
		if (Projectile == nullptr || !Projectile->Launch(Source, FVector::ForwardVector, Shot))
		{
			return nullptr;
		}
		for (int32 Step = 0; Step < 400 && !Projectile->HasFinished(); ++Step)
		{
			Projectile->AdvanceBy(0.01f);
		}
		return Projectile;
	}

	UGolemColorVoidComponent* OnlyOpenVoid(const FCombatant& Xenon)
	{
		const TArray<UGolemColorVoidComponent*> Open = Xenon.Host->GetOpenVoids();
		return Open.Num() == 1 ? Open[0] : nullptr;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGolemVoidWrongColorTest, "Golem.Color.Rules.WrongColorDullsForThreeSeconds",
	GolemColorVoidTests::Flags)

bool FGolemVoidWrongColorTest::RunTest(const FString& Parameters)
{
	using namespace GolemColorVoidTests;
	const FGolemVoidTuning Tuning;
	FGolemVoidRuntime Void;
	TestTrue(TEXT("Orange void configures open"), Void.Configure(Orange, false));
	TestEqual(TEXT("First correct colour primes"), Void.Apply(EGolemElement::Earth, 0.0f, 10.0, Tuning), EGolemVoidHit::Primed);
	TestEqual(TEXT("A colour the void does not need dulls it"), Void.Apply(EGolemElement::Sea, 0.0f, 11.0, Tuning), EGolemVoidHit::Dulled);
	TestEqual(TEXT("State is Dulled"), Void.State, EGolemVoidState::Dulled);
	TestEqual(TEXT("Wrong colour clears what was already placed"), Void.AppliedMask, static_cast<uint8>(0));
	TestEqual(TEXT("Lock lasts exactly the tuned three seconds"), Void.TimerEnd, 14.0);
	TestEqual(TEXT("A dulled void takes no hit"), Void.Apply(EGolemElement::Earth, 0.0f, 13.99, Tuning), EGolemVoidHit::Ignored);
	TestFalse(TEXT("Still locked just before three seconds"), Void.Update(13.99));
	TestTrue(TEXT("Lock ends at three seconds"), Void.Update(14.0));
	TestEqual(TEXT("Back to Open"), Void.State, EGolemVoidState::Open);
	TestEqual(TEXT("And it accepts colour again"), Void.Apply(EGolemElement::Sky, 0.0f, 14.0, Tuning), EGolemVoidHit::Primed);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGolemVoidFillTest, "Golem.Color.Rules.AllColorsInsideWindowFill",
	GolemColorVoidTests::Flags)

bool FGolemVoidFillTest::RunTest(const FString& Parameters)
{
	using namespace GolemColorVoidTests;
	const FGolemVoidTuning Tuning;
	FGolemVoidRuntime Single;
	Single.Configure(Red, false);
	TestEqual(TEXT("A one-colour void fills on the first correct hit"), Single.Apply(EGolemElement::Earth, 0.0f, 0.0, Tuning), EGolemVoidHit::Filled);
	TestEqual(TEXT("Filled is terminal"), Single.Apply(EGolemElement::Earth, 0.0f, 1.0, Tuning), EGolemVoidHit::Ignored);
	TestFalse(TEXT("A filled void cannot be hidden"), Single.Hide());

	FGolemVoidRuntime Mixed;
	Mixed.Configure(Orange, false);
	TestEqual(TEXT("First colour primes"), Mixed.Apply(EGolemElement::Sky, 0.0f, 5.0, Tuning), EGolemVoidHit::Primed);
	TestEqual(TEXT("Window is four seconds"), Mixed.TimerEnd, 9.0);
	TestEqual(TEXT("Second colour just inside the window fills"), Mixed.Apply(EGolemElement::Earth, 0.0f, 8.99, Tuning), EGolemVoidHit::Filled);
	TestEqual(TEXT("State is Filled"), Mixed.State, EGolemVoidState::Filled);
	TestEqual(TEXT("Nothing is missing"), Mixed.MissingMask(), static_cast<uint8>(0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGolemVoidExpiryTest, "Golem.Color.Rules.WindowExpiryReturnsToOpen",
	GolemColorVoidTests::Flags)

bool FGolemVoidExpiryTest::RunTest(const FString& Parameters)
{
	using namespace GolemColorVoidTests;
	const FGolemVoidTuning Tuning;
	FGolemVoidRuntime Void;
	Void.Configure(Purple, false);
	Void.Apply(EGolemElement::Earth, 0.0f, 0.0, Tuning);
	TestEqual(TEXT("Repeating a placed colour changes nothing"), Void.Apply(EGolemElement::Earth, 0.0f, 2.0, Tuning), EGolemVoidHit::Repeated);
	TestEqual(TEXT("Repeating does not refresh the timer"), Void.TimerEnd, 4.0);
	TestFalse(TEXT("Window still open just before four seconds"), Void.Update(3.99));
	TestTrue(TEXT("Window closes at four seconds"), Void.Update(4.0));
	TestEqual(TEXT("Expiry returns to Open, not Dulled"), Void.State, EGolemVoidState::Open);
	TestEqual(TEXT("Expiry clears placed colours"), Void.AppliedMask, static_cast<uint8>(0));
	TestEqual(TEXT("A late second colour starts a new window instead of filling"), Void.Apply(EGolemElement::Sea, 0.0f, 4.0, Tuning), EGolemVoidHit::Primed);
	TestEqual(TEXT("New window starts from the late hit"), Void.TimerEnd, 8.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGolemVoidColorHoldTest, "Golem.Color.Rules.ColorHoldExtendsWindow",
	GolemColorVoidTests::Flags)

bool FGolemVoidColorHoldTest::RunTest(const FString& Parameters)
{
	using namespace GolemColorVoidTests;
	const FGolemVoidTuning Tuning;
	FGolemVoidRuntime Void;
	Void.Configure(Purple, false);
	Void.Apply(EGolemElement::Sea, 0.5f, 0.0, Tuning);
	TestEqual(TEXT("ColorHold of the first Golem extends the window by exactly that many seconds"), Void.TimerEnd, 4.5);
	TestFalse(TEXT("Open past the base four seconds"), Void.Update(4.4));
	TestEqual(TEXT("Second colour lands inside the extended window"), Void.Apply(EGolemElement::Earth, 2.0f, 4.4, Tuning), EGolemVoidHit::Filled);

	FGolemVoidRuntime Other;
	Other.Configure(Purple, false);
	Other.Apply(EGolemElement::Earth, 0.0f, 0.0, Tuning);
	Other.Apply(EGolemElement::Earth, 3.0f, 1.0, Tuning);
	TestEqual(TEXT("Only the hit that opens the window contributes ColorHold"), Other.TimerEnd, 4.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGolemVoidWhiteCoreRuleTest, "Golem.Color.Rules.WhiteCoreWindowAndHiddenVoids",
	GolemColorVoidTests::Flags)

bool FGolemVoidWhiteCoreRuleTest::RunTest(const FString& Parameters)
{
	using namespace GolemColorVoidTests;
	const FGolemVoidTuning Tuning;
	FGolemVoidRuntime Core;
	TestTrue(TEXT("Core configures hidden"), Core.Configure(GolemElements::AllMask, true));
	TestEqual(TEXT("A hidden void cannot be hit"), Core.Apply(EGolemElement::Earth, 0.0f, 0.0, Tuning), EGolemVoidHit::Ignored);
	TestEqual(TEXT("Hidden stays hidden"), Core.State, EGolemVoidState::Hidden);
	TestTrue(TEXT("Reveal opens it"), Core.Reveal());
	TestEqual(TEXT("First of three primes"), Core.Apply(EGolemElement::Earth, 0.0f, 0.0, Tuning), EGolemVoidHit::Primed);
	TestEqual(TEXT("White core window is five seconds"), Core.TimerEnd, 5.0);
	TestEqual(TEXT("Second of three still primes"), Core.Apply(EGolemElement::Sky, 0.0f, 2.0, Tuning), EGolemVoidHit::Primed);
	TestEqual(TEXT("Third inside five seconds fills"), Core.Apply(EGolemElement::Sea, 0.0f, 4.9, Tuning), EGolemVoidHit::Filled);
	FGolemVoidRuntime Invalid;
	TestFalse(TEXT("A void must need at least one element"), Invalid.Configure(0, false));
	TestFalse(TEXT("Bits outside the three elements are rejected"), Invalid.Configure(0x8, false));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGolemVoidHostSequenceTest, "Golem.Color.Host.SequentialVoidsStaggerAndRestore",
	GolemColorVoidTests::Flags)

bool FGolemVoidHostSequenceTest::RunTest(const FString& Parameters)
{
	using namespace GolemColorVoidTests;
	FTestWorld Context;
	FGolemVoidHostTuning Tuning;
	Tuning.VoidsToFill = 3;
	Tuning.MaxOpenVoids = 1;
	FGolemResult Init;
	const FCombatant Golem = SpawnCombatant(Context.World.Get(), true, 100.0f);
	const FCombatant Xenon = SpawnXenon(Context.World.Get(), Tuning, { Red, Orange, Yellow }, 2, 300.0f, Init);
	TestTrue(TEXT("Host initializes"), Init.IsSuccess());
	UGolemColorVoidComponent* First = OnlyOpenVoid(Xenon);
	TestNotNull(TEXT("Exactly one void is revealed at the start"), First);
	if (First == nullptr)
	{
		return false;
	}
	TestEqual(TEXT("It asks for the first colour of the plan"), First->GetRequiredMask(), Red);

	TestTrue(TEXT("A normal hit lands on the grey body"), UGolemAbilitySystemComponent::ApplyCombatDamage(
		Golem.Actor.Get(), Xenon.Actor.Get(), 50.0f, 0.0f, true).IsSuccess());
	TestEqual(TEXT("Grey body takes ten percent"), Xenon.Attributes->GetHealth(), 295.0f);

	Xenon.Host->AdvanceTo(1.0);
	TestEqual(TEXT("Correct colour fills the one-colour void"), First->ApplyElementAt(EGolemElement::Earth, 0.0f, Golem.Actor.Get(), 1.0), EGolemVoidHit::Filled);
	TestEqual(TEXT("A fill removes one third of maximum health"), Xenon.Attributes->GetHealth(), 195.0f);
	TestEqual(TEXT("One fill counted"), Xenon.Host->GetFilledCount(), 1);
	TestTrue(TEXT("The Xenon is staggered"), Xenon.Host->IsStaggered()
		&& Xenon.ASC->HasMatchingGameplayTag(GolemGameplayTags::State_Combat_Staggered));
	TestEqual(TEXT("Stagger lasts the tuned 2.5 seconds"), Xenon.Host->GetStaggerEndServerTime(), 3.5);
	TestEqual(TEXT("No void is open during the stagger"), Xenon.Host->GetOpenVoids().Num(), 0);
	Xenon.Host->AdvanceTo(3.49);
	TestEqual(TEXT("Still none just before the stagger ends"), Xenon.Host->GetOpenVoids().Num(), 0);
	Xenon.Host->AdvanceTo(3.5);
	TestFalse(TEXT("Stagger ended"), Xenon.Host->IsStaggered()
		|| Xenon.ASC->HasMatchingGameplayTag(GolemGameplayTags::State_Combat_Staggered));
	UGolemColorVoidComponent* Second = OnlyOpenVoid(Xenon);
	TestNotNull(TEXT("The next void appears after the stagger"), Second);
	if (Second == nullptr)
	{
		return false;
	}
	TestTrue(TEXT("It appears at a different place"), Second != First);
	TestEqual(TEXT("It asks for the second colour of the plan"), Second->GetRequiredMask(), Orange);

	TestEqual(TEXT("First colour of a mixed void primes"), Second->ApplyElementAt(EGolemElement::Earth, 0.0f, Golem.Actor.Get(), 4.0), EGolemVoidHit::Primed);
	TestEqual(TEXT("Priming alone costs the Xenon nothing"), Xenon.Attributes->GetHealth(), 195.0f);
	Xenon.Host->AdvanceTo(5.0);
	TestEqual(TEXT("Second colour fills"), Second->ApplyElementAt(EGolemElement::Sky, 0.0f, Golem.Actor.Get(), 5.0), EGolemVoidHit::Filled);
	TestEqual(TEXT("Second slice removed"), Xenon.Attributes->GetHealth(), 95.0f);
	Xenon.Host->AdvanceTo(7.5);
	UGolemColorVoidComponent* Third = OnlyOpenVoid(Xenon);
	TestNotNull(TEXT("Third void appears"), Third);
	if (Third == nullptr)
	{
		return false;
	}
	TestTrue(TEXT("Slots are reused, again away from the last fill"), Third == First);
	TestFalse(TEXT("Not restored before the last fill"), Xenon.Host->IsRestored());

	TestEqual(TEXT("Last fill"), Third->ApplyElementAt(EGolemElement::Sky, 0.0f, Golem.Actor.Get(), 8.0), EGolemVoidHit::Filled);
	TestEqual(TEXT("The last fill empties the remaining health, grey-body damage included"), Xenon.Attributes->GetHealth(), 0.0f);
	TestTrue(TEXT("Xenon is restored"), Xenon.Host->IsRestored()
		&& Xenon.ASC->HasMatchingGameplayTag(GolemGameplayTags::State_Xenon_Restored));
	TestFalse(TEXT("A restored Xenon is not staggered"), Xenon.ASC->HasMatchingGameplayTag(GolemGameplayTags::State_Combat_Staggered));
	TestEqual(TEXT("A restored Xenon shows no void"), Xenon.Host->GetOpenVoids().Num(), 0);
	TestFalse(TEXT("It no longer fights"), Xenon.Life->IsAlive());
	TestTrue(TEXT("The actor stays in the world"), Xenon.Actor.IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGolemVoidHostCoreTest, "Golem.Color.Host.WhiteCoreAppearsAfterOtherVoids",
	GolemColorVoidTests::Flags)

bool FGolemVoidHostCoreTest::RunTest(const FString& Parameters)
{
	using namespace GolemColorVoidTests;
	FTestWorld Context;
	FGolemVoidHostTuning Tuning;
	Tuning.VoidsToFill = 2;
	Tuning.MaxOpenVoids = 2;
	Tuning.bHasWhiteCore = true;
	Tuning.StaggerSeconds = 1.5f;
	FGolemResult Init;
	const FCombatant Golem = SpawnCombatant(Context.World.Get(), true, 100.0f);
	const FCombatant Guardian = SpawnXenon(Context.World.Get(), Tuning, { Red, Yellow }, 2, 900.0f, Init);
	TestTrue(TEXT("Host initializes"), Init.IsSuccess());
	TestEqual(TEXT("Two voids open together"), Guardian.Host->GetOpenVoids().Num(), 2);
	TestEqual(TEXT("Core starts hidden"), Guardian.Core->GetVoidState(), EGolemVoidState::Hidden);
	TestEqual(TEXT("A hidden core cannot be hit"), Guardian.Core->ApplyElementAt(EGolemElement::Earth, 0.0f, Golem.Actor.Get(), 0.0), EGolemVoidHit::Ignored);

	Guardian.Host->AdvanceTo(1.0);
	TestEqual(TEXT("First void filled"), Guardian.Slots[0]->ApplyElementAt(EGolemElement::Earth, 0.0f, Golem.Actor.Get(), 1.0), EGolemVoidHit::Filled);
	TestEqual(TEXT("Each fill is worth a third when a core exists"), Guardian.Attributes->GetHealth(), 600.0f);
	TestEqual(TEXT("The other void stays open through the stagger"), Guardian.Host->GetOpenVoids().Num(), 1);
	TestFalse(TEXT("Core still hidden while a void remains"), Guardian.Host->IsWhiteCoreRevealed());
	Guardian.Host->AdvanceTo(2.0);
	TestEqual(TEXT("Second void filled"), Guardian.Slots[1]->ApplyElementAt(EGolemElement::Sky, 0.0f, Golem.Actor.Get(), 2.0), EGolemVoidHit::Filled);
	TestFalse(TEXT("Core waits for the stagger to end"), Guardian.Host->IsWhiteCoreRevealed());
	Guardian.Host->AdvanceTo(3.5);
	TestTrue(TEXT("Core appears once every other void is filled"), Guardian.Host->IsWhiteCoreRevealed());
	TestEqual(TEXT("Core needs all three elements"), Guardian.Core->GetRequiredMask(), GolemElements::AllMask);

	TestEqual(TEXT("Earth primes the core"), Guardian.Core->ApplyElementAt(EGolemElement::Earth, 0.0f, Golem.Actor.Get(), 4.0), EGolemVoidHit::Primed);
	TestEqual(TEXT("Core window is five seconds"), Guardian.Core->GetTimerEndServerTime(), 9.0);
	TestEqual(TEXT("Sky adds"), Guardian.Core->ApplyElementAt(EGolemElement::Sky, 0.0f, Golem.Actor.Get(), 6.0), EGolemVoidHit::Primed);
	TestFalse(TEXT("Not won yet"), Guardian.Host->IsRestored());
	TestEqual(TEXT("Sea completes the core"), Guardian.Core->ApplyElementAt(EGolemElement::Sea, 0.0f, Golem.Actor.Get(), 8.9), EGolemVoidHit::Filled);
	TestTrue(TEXT("Filling the core wins the fight"), Guardian.Host->IsRestored());
	TestEqual(TEXT("Health is empty"), Guardian.Attributes->GetHealth(), 0.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGolemVoidHostGrayBodyTest, "Golem.Color.Host.GrayBodyDamageAloneRestores",
	GolemColorVoidTests::Flags)

bool FGolemVoidHostGrayBodyTest::RunTest(const FString& Parameters)
{
	using namespace GolemColorVoidTests;
	FTestWorld Context;
	FGolemVoidHostTuning Tuning;
	FGolemResult Init;
	const FCombatant Golem = SpawnCombatant(Context.World.Get(), true, 100.0f);
	const FCombatant Xenon = SpawnXenon(Context.World.Get(), Tuning, { Red, Red, Red }, 1, 30.0f, Init);
	TestTrue(TEXT("Host initializes"), Init.IsSuccess());
	for (int32 Hit = 0; Hit < 2; ++Hit)
	{
		UGolemAbilitySystemComponent::ApplyCombatDamage(Golem.Actor.Get(), Xenon.Actor.Get(), 100.0f, 0.0f, true);
	}
	TestEqual(TEXT("Two body hits of 100 remove 20"), Xenon.Attributes->GetHealth(), 10.0f);
	TestFalse(TEXT("Not restored while health remains"), Xenon.Host->IsRestored());
	UGolemAbilitySystemComponent::ApplyCombatDamage(Golem.Actor.Get(), Xenon.Actor.Get(), 100.0f, 0.0f, true);
	TestEqual(TEXT("Third body hit empties health"), Xenon.Attributes->GetHealth(), 0.0f);
	TestTrue(TEXT("Body damage alone restores the Xenon, slowly"), Xenon.Host->IsRestored());
	TestEqual(TEXT("No void remains"), Xenon.Host->GetOpenVoids().Num(), 0);

	FGolemVoidHostTuning Bad;
	Bad.MaxOpenVoids = 2;
	FGolemResult BadInit;
	SpawnXenon(Context.World.Get(), Bad, { Red, Red, Red }, 1, 100.0f, BadInit);
	TestFalse(TEXT("A host needs one slot per simultaneously open void"), BadInit.IsSuccess());
	FGolemResult ShortPlan;
	SpawnXenon(Context.World.Get(), Tuning, { Red }, 1, 100.0f, ShortPlan);
	TestFalse(TEXT("The colour plan must cover every void to fill"), ShortPlan.IsSuccess());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGolemXenonDefinitionTest, "Golem.Color.Xenon.SeededPlanAndInitialization",
	GolemColorVoidTests::Flags)

bool FGolemXenonDefinitionTest::RunTest(const FString& Parameters)
{
	using namespace GolemColorVoidTests;
	UGolemXenonDefinition* Grunt = NewObject<UGolemXenonDefinition>();
	TestTrue(TEXT("Identity accepted"), Grunt->TrySetIdentity(TEXT("TestGrunt"), TEXT("GolemXenon")));
	FGolemVoidHostTuning Voids;
	Voids.VoidsToFill = 3;
	Voids.MaxOpenVoids = 1;
	TArray<FGolemXenonVoidSlot> Slots;
	Slots.SetNum(2);
	Slots[1].Offset = FVector(-45.0, 0.0, 30.0);
	TestTrue(TEXT("Grunt numbers accepted"), Grunt->TrySetXenon(300.0f, 20.0f, 2.5f, Voids, Slots, 0.3f, true).IsSuccess());
	TestFalse(TEXT("A Xenon needs positive health"), Grunt->TrySetXenon(0.0f, 20.0f, 2.5f, Voids, Slots, 0.3f, true).IsSuccess());
	TestEqual(TEXT("Rejected numbers leave the definition unchanged"), Grunt->GetMaxHealth(), 300.0f);
	TArray<FGolemXenonVoidSlot> TooFew;
	Voids.MaxOpenVoids = 2;
	TooFew.SetNum(1);
	TestFalse(TEXT("Slots must cover simultaneously open voids"), Grunt->TrySetXenon(300.0f, 20.0f, 2.5f, Voids, TooFew, 0.3f, true).IsSuccess());

	const TArray<uint8> Plan = Grunt->BuildColorPlan(1234);
	TestEqual(TEXT("One colour per void to fill"), Plan.Num(), 3);
	TestTrue(TEXT("The same seed gives the same plan"), Plan == Grunt->BuildColorPlan(1234));
	bool bAllValid = true;
	bool bFirstAlwaysBasic = true;
	bool bAllMixedWhenForced = true;
	bool bAllBasicWhenForced = true;
	for (int32 Seed = 0; Seed < 64; ++Seed)
	{
		const TArray<uint8> Default = Grunt->BuildColorPlan(Seed);
		const TArray<uint8> ForcedMixed = Grunt->BuildColorPlan(Seed, 1.0f);
		const TArray<uint8> ForcedBasic = Grunt->BuildColorPlan(Seed, 0.0f);
		bFirstAlwaysBasic &= GolemElements::Count(Default[0]) == 1 && GolemElements::Count(ForcedMixed[0]) == 1;
		for (int32 Index = 0; Index < 3; ++Index)
		{
			bAllValid &= GolemElements::IsValidMask(Default[Index]) && GolemElements::Count(Default[Index]) <= 2;
			bAllMixedWhenForced &= Index == 0 || GolemElements::Count(ForcedMixed[Index]) == 2;
			bAllBasicWhenForced &= GolemElements::Count(ForcedBasic[Index]) == 1;
		}
	}
	TestTrue(TEXT("Ordinary voids need one or two elements"), bAllValid);
	TestTrue(TEXT("The first void of an easy Grunt is always a single colour"), bFirstAlwaysBasic);
	TestTrue(TEXT("Difficulty 3 makes every other void mixed"), bAllMixedWhenForced);
	TestTrue(TEXT("Zero chance makes every void basic"), bAllBasicWhenForced);

	FTestWorld Context;
	AGolemXenon* Xenon = Context.World->SpawnActor<AGolemXenon>();
	TestNotNull(TEXT("Xenon spawns"), Xenon);
	if (Xenon == nullptr)
	{
		return false;
	}
	TestTrue(TEXT("Xenon initializes from its definition"), Xenon->InitializeXenon(Grunt, 1234).IsSuccess());
	TestFalse(TEXT("It initializes only once"), Xenon->InitializeXenon(Grunt, 1234).IsSuccess());
	TestEqual(TEXT("Health comes from data"), Xenon->GetGolemAttributeSet()->GetHealth(), 300.0f);
	TestTrue(TEXT("It is an enemy"), Xenon->GetGolemAbilitySystem()->HasMatchingGameplayTag(GolemGameplayTags::Role_Enemy));
	TestEqual(TEXT("Identity names the definition"), Xenon->GetDefinitionId(), FPrimaryAssetId(TEXT("GolemXenon"), TEXT("TestGrunt")));
	const TArray<UGolemColorVoidComponent*> Open = Xenon->GetVoidHost()->GetOpenVoids();
	TestEqual(TEXT("A Grunt shows one void"), Open.Num(), 1);
	if (Open.Num() == 1)
	{
		TestEqual(TEXT("The open void follows the seeded plan"), Open[0]->GetRequiredMask(), Plan[0]);
		TestTrue(TEXT("An open void can be traced"), Open[0]->IsQueryCollisionEnabled()
			&& Open[0]->ComponentHasTag(TEXT("GolemArmorZone")));
	}
	TestFalse(TEXT("A hidden void cannot be traced"), Xenon->GetVoidHost()->GetCoreSlot() != nullptr
		&& Xenon->GetVoidHost()->GetCoreSlot()->IsQueryCollisionEnabled());
	TestFalse(TEXT("Not restored at spawn"), Xenon->IsRestored());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGolemVoidSignalTest, "Golem.Color.Rules.SignalLastsSixSecondsOnAnOpenVoid",
	GolemColorVoidTests::Flags)

bool FGolemVoidSignalTest::RunTest(const FString& Parameters)
{
	using namespace GolemColorVoidTests;
	FTestWorld Context;
	const FCombatant Golem = SpawnCombatant(Context.World.Get(), true, 100.0f);
	const FCombatant Owner = SpawnCombatant(Context.World.Get(), false, 100.0f);
	UGolemColorVoidComponent* Void = AddVoid(Owner.Actor.Get());
	const FGolemVoidTuning Tuning;
	TestEqual(TEXT("The marker lasts six seconds by default"), Tuning.SignalSeconds, 6.0f);
	TestTrue(TEXT("Void configures hidden"), Void->ConfigureVoid(Orange, Tuning, true).IsSuccess());
	TestFalse(TEXT("A hidden void cannot be signalled"), Void->SignalAt(Golem.Actor.Get(), 10.0));
	Void->SetRevealed(true);
	int32 Signals = 0;
	AActor* SignalledBy = nullptr;
	Void->OnVoidSignaled.AddLambda([&Signals, &SignalledBy](UGolemColorVoidComponent*, AActor* By)
	{
		++Signals;
		SignalledBy = By;
	});
	TestTrue(TEXT("An open void takes a signal"), Void->SignalAt(Golem.Actor.Get(), 10.0));
	TestEqual(TEXT("The host announces it once"), Signals, 1);
	TestTrue(TEXT("The signalling Golem is known"), SignalledBy == Golem.Actor.Get());
	TestTrue(TEXT("Marked just before six seconds"), Void->IsSignaledAt(15.99));
	TestFalse(TEXT("Gone after six seconds"), Void->IsSignaledAt(16.01));
	TestEqual(TEXT("Start time replicates for companions"), Void->GetSignalStartServerTime(), 10.0);

	TestEqual(TEXT("Earth primes the orange void"), Void->ApplyElementAt(EGolemElement::Earth, 0.0f, nullptr, 11.0), EGolemVoidHit::Primed);
	TestTrue(TEXT("Priming keeps the marker"), Void->IsSignaledAt(12.0));
	TestEqual(TEXT("The marker shows what is still missing"), Void->GetMissingMask(), Yellow);
	TestEqual(TEXT("Sky fills it"), Void->ApplyElementAt(EGolemElement::Sky, 0.0f, nullptr, 12.0), EGolemVoidHit::Filled);
	TestFalse(TEXT("A filled void drops its marker"), Void->IsSignaledAt(12.5));
	TestFalse(TEXT("A filled void cannot be signalled"), Void->SignalAt(Golem.Actor.Get(), 13.0));

	// Aim: the void the view ray touches wins; otherwise the one nearest to the ray, within the cone.
	const FCombatant Other = SpawnCombatant(Context.World.Get(), false, 100.0f);
	UGolemColorVoidComponent* Left = AddVoid(Other.Actor.Get());
	UGolemColorVoidComponent* Right = AddVoid(Other.Actor.Get());
	Left->ConfigureVoid(Red, Tuning, false);
	Right->ConfigureVoid(Yellow, Tuning, false);
	Left->SetWorldLocation(FVector(1000.0, -200.0, 0.0));
	Right->SetWorldLocation(FVector(1000.0, 200.0, 0.0));
	UWorld* World = Context.World.Get();
	TestTrue(TEXT("Looking straight at a void picks it"), UGolemColorVoidComponent::FindAimedVoid(World,
		FVector::ZeroVector, FVector(1000.0, -200.0, 0.0), 3000.0f, 14.0f) == Left);
	TestTrue(TEXT("Looking slightly right of centre picks the right one"), UGolemColorVoidComponent::FindAimedVoid(World,
		FVector::ZeroVector, FVector(1000.0, 60.0, 0.0), 3000.0f, 14.0f) == Right);
	TestNull(TEXT("Looking away picks nothing"), UGolemColorVoidComponent::FindAimedVoid(World,
		FVector::ZeroVector, FVector(-1.0, 0.0, 0.0), 3000.0f, 14.0f));
	TestNull(TEXT("A void beyond the range is not picked"), UGolemColorVoidComponent::FindAimedVoid(World,
		FVector::ZeroVector, FVector(1000.0, -200.0, 0.0), 500.0f, 14.0f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGolemColorProjectileTest, "Golem.Color.Projectile.MustTouchTheVoidItself",
	GolemColorVoidTests::Flags)

bool FGolemColorProjectileTest::RunTest(const FString& Parameters)
{
	using namespace GolemColorVoidTests;
	FTestWorld Context;
	UWorld* World = Context.World.Get();
	FGolemVoidHostTuning Tuning;
	FGolemResult Init;
	const FCombatant Shooter = SpawnCombatant(World, true, 100.0f);
	Shooter.Actor->SetActorLocation(FVector(-1200.0, 0.0, 0.0));
	// Two slots: the first void faces the shooter, the second sits behind the body.
	const FCombatant Xenon = SpawnXenon(World, Tuning, { Yellow, Yellow, Yellow }, 2, 300.0f, Init);
	TestTrue(TEXT("Host initializes"), Init.IsSuccess());
	AddBody(Xenon.Actor.Get(), 80.0f);
	Xenon.Slots[0]->SetRelativeLocation(FVector(-100.0, 0.0, 0.0));
	Xenon.Slots[1]->SetRelativeLocation(FVector(100.0, 0.0, 0.0));
	UGolemColorVoidComponent* Front = OnlyOpenVoid(Xenon);
	TestTrue(TEXT("The first void is the front one"), Front == Xenon.Slots[0]);

	FGolemProjectileShot Shot;
	Shot.Speed = 3000.0f;
	Shot.Range = 2500.0f;
	Shot.Radius = 18.0f;
	Shot.Damage = 25.0f;
	Shot.bAppliesColor = true;
	Shot.Element = EGolemElement::Sky;
	const FVector Muzzle(-1150.0, 0.0, 0.0);

	// 1. An AI shot that was not sent to this void treats it as grey body: no colour, 10% damage.
	FGolemProjectileShot Undirected = Shot;
	Undirected.bRestrictColor = true;
	AGolemColorProjectile* First = FireShot(World, Shooter.Actor.Get(), Muzzle, Undirected);
	TestNotNull(TEXT("Shot launches"), First);
	TestEqual(TEXT("An undirected AI shot places no colour"), Front->GetVoidState(), EGolemVoidState::Open);
	TestEqual(TEXT("It still counts as a body hit at 10%"), Xenon.Attributes->GetHealth(), 297.5f);

	// 2. A follow-up shot may not open a void.
	FGolemProjectileShot FollowUp = Shot;
	FollowUp.bRestrictColor = true;
	FollowUp.OnlyVoid = Front;
	FollowUp.bOnlyIfPrimed = true;
	FireShot(World, Shooter.Actor.Get(), Muzzle, FollowUp);
	TestEqual(TEXT("A follow-up shot never opens a void"), Front->GetVoidState(), EGolemVoidState::Open);

	// 3. A player shot that touches the void places its colour and deals no direct damage.
	const float HealthBefore = Xenon.Attributes->GetHealth();
	AGolemColorProjectile* Hit = FireShot(World, Shooter.Actor.Get(), Muzzle, Shot);
	TestTrue(TEXT("The shot stops at the void"), Hit != nullptr && Hit->HasFinished() && Hit->GetDistanceFlown() < 1200.0f);
	TestEqual(TEXT("A single-colour void is filled by the first correct shot"), Xenon.Host->GetFilledCount(), 1);
	TestEqual(TEXT("The fill, not the shot, removes health: one third"), Xenon.Attributes->GetHealth(), HealthBefore - 100.0f);

	// 4. After the stagger the next void is at the back; a shot from the front reaches the body first.
	Xenon.Host->AdvanceTo(World->GetTimeSeconds() + Tuning.StaggerSeconds + 0.1);
	UGolemColorVoidComponent* Back = OnlyOpenVoid(Xenon);
	TestTrue(TEXT("The next void opens on the other slot"), Back == Xenon.Slots[1]);
	const float BeforeBody = Xenon.Attributes->GetHealth();
	FireShot(World, Shooter.Actor.Get(), Muzzle, Shot);
	TestEqual(TEXT("The void behind the body takes no colour from the front"), Back != nullptr ? Back->GetVoidState() : EGolemVoidState::Hidden,
		EGolemVoidState::Open);
	TestEqual(TEXT("The body takes 10% of the shot"), Xenon.Attributes->GetHealth(), BeforeBody - 2.5f);
	TestTrue(TEXT("The line of fire reports the body, not the void"), AGolemColorProjectile::FindFirstStop(World,
		Shooter.Actor.Get(), Muzzle, Back->GetComponentLocation(), Shot.Radius) != Back);
	TestTrue(TEXT("From behind the same void is the first thing in the way"), AGolemColorProjectile::FindFirstStop(World,
		Shooter.Actor.Get(), FVector(1200.0, 0.0, 0.0), Back->GetComponentLocation(), Shot.Radius) == Back);

	// 5. A shot that touches nothing fades after its range: 25 m.
	AGolemColorProjectile* Miss = FireShot(World, Shooter.Actor.Get(), FVector(-1150.0, 2000.0, 0.0), Shot);
	TestTrue(TEXT("A miss ends"), Miss != nullptr && Miss->HasFinished());
	TestEqual(TEXT("It flew exactly its range"), Miss != nullptr ? Miss->GetDistanceFlown() : 0.0f, 2500.0f);

	// 6. Shots pass through teammates.
	const FCombatant Friend = SpawnCombatant(World, true, 100.0f);
	Friend.Actor->SetActorLocation(FVector(-600.0, 3000.0, 0.0));
	AddBody(Friend.Actor.Get(), 40.0f);
	AGolemColorProjectile* Through = FireShot(World, Shooter.Actor.Get(), FVector(-1150.0, 3000.0, 0.0), Shot);
	TestEqual(TEXT("A teammate does not stop the shot"), Through != nullptr ? Through->GetDistanceFlown() : 0.0f, 2500.0f);
	TestEqual(TEXT("A teammate takes no damage"), Friend.Attributes->GetHealth(), 100.0f);
	TestTrue(TEXT("Party and enemy are hostile"), UGolemCombatComponent::AreHostile(Shooter.Actor.Get(), Xenon.Actor.Get()));
	TestFalse(TEXT("Two party Golems are not"), UGolemCombatComponent::AreHostile(Shooter.Actor.Get(), Friend.Actor.Get()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGolemXenonBrainTest, "Golem.Color.Xenon.ShowsEveryAttackBeforeItLands",
	GolemColorVoidTests::Flags)

bool FGolemXenonBrainTest::RunTest(const FString& Parameters)
{
	using namespace GolemColorVoidTests;
	FGolemXenonMove Fast;
	Fast.TelegraphSeconds = 0.5f;
	TestFalse(TEXT("An attack shown for less than 0.6 s is rejected"), Fast.Validate().IsSuccess());
	FGolemXenonMove Short;
	Short.Range = Short.StartRange - 10.0f;
	TestFalse(TEXT("An attack must reach at least as far as where it starts"), Short.Validate().IsSuccess());

	UGolemXenonDefinition* Grunt = NewObject<UGolemXenonDefinition>(GetTransientPackage());
	Grunt->TrySetIdentity(TEXT("BrainGrunt"), TEXT("GolemXenon"));
	FGolemVoidHostTuning Voids;
	TArray<FGolemXenonVoidSlot> Slots;
	Slots.SetNum(2);
	TestTrue(TEXT("Grunt numbers accepted"), Grunt->TrySetXenon(300.0f, 20.0f, 2.5f, Voids, Slots, 0.0f, true).IsSuccess());
	FGolemXenonBehavior Behavior;
	FGolemXenonMove& Sweep = Behavior.Moves.AddDefaulted_GetRef();
	Sweep.StartRange = 290.0f;
	Sweep.TelegraphSeconds = 1.0f;
	Sweep.RecoverSeconds = 1.4f;
	Sweep.Range = 340.0f;
	Sweep.HalfAngleDegrees = 80.0f;
	TestTrue(TEXT("Behaviour accepted"), Grunt->TrySetBehavior(Behavior).IsSuccess());

	FTestWorld Context;
	UWorld* World = Context.World.Get();
	AGolemXenon* Xenon = World->SpawnActor<AGolemXenon>(FVector::ZeroVector, FRotator::ZeroRotator);
	if (Xenon == nullptr || !Xenon->InitializeXenon(Grunt, 5).IsSuccess())
	{
		AddError(TEXT("Xenon did not initialize."));
		return false;
	}
	UGolemXenonBrainComponent* Brain = Xenon->GetBrain();
	Brain->Step(0.1f, 1.0);
	TestEqual(TEXT("With nobody around the Xenon idles"), Brain->GetAction(), EGolemXenonAction::Idle);

	// Front, side and behind; all inside the reach of the sweep. The Xenon faces +X.
	const FCombatant Front = SpawnCombatant(World, true, 140.0f);
	const FCombatant Side = SpawnCombatant(World, true, 90.0f);
	const FCombatant Behind = SpawnCombatant(World, true, 80.0f);
	Front.Actor->SetActorLocation(FVector(250.0, 0.0, 0.0));
	Side.Actor->SetActorLocation(FVector(100.0, 300.0, 0.0));
	Behind.Actor->SetActorLocation(FVector(-300.0, 0.0, 0.0));

	Brain->Step(0.1f, 2.0);
	TestEqual(TEXT("A Golem in front and in range starts the attack warning"), Brain->GetAction(), EGolemXenonAction::Telegraph);
	TestTrue(TEXT("The nearest Golem is the target"), Brain->GetTarget() == Front.Actor.Get());
	TestEqual(TEXT("The warning lasts the move's telegraph time"), Brain->GetActionEndServerTime(), 3.0);
	TestTrue(TEXT("The Golem in front stands in the shown zone"), Brain->IsThreatening(Front.Actor->GetActorLocation()));
	TestTrue(TEXT("Companions can ask which Xenon threatens a spot"),
		UGolemXenonBrainComponent::FindThreat(World, Front.Actor->GetActorLocation()) == Xenon);
	TestFalse(TEXT("The Golem behind is outside the zone"), Brain->IsThreatening(Behind.Actor->GetActorLocation()));
	Brain->Step(0.1f, 2.95);
	TestEqual(TEXT("No damage before the warning ends"), Front.Attributes->GetHealth(), 140.0f);
	TestEqual(TEXT("Still warning"), Brain->GetAction(), EGolemXenonAction::Telegraph);

	// The side Golem steps out during the warning; the front one stays.
	Side.Actor->SetActorLocation(FVector(100.0, 600.0, 0.0));
	Brain->Step(0.1f, 3.0);
	TestEqual(TEXT("The hit lands when the warning ends: 20 damage"), Front.Attributes->GetHealth(), 120.0f);
	TestEqual(TEXT("A Golem that stepped out is not hit"), Side.Attributes->GetHealth(), 90.0f);
	TestEqual(TEXT("A Golem behind the Xenon is not hit"), Behind.Attributes->GetHealth(), 80.0f);
	TestEqual(TEXT("One strike so far"), Brain->GetStrikeCount(), 1);
	TestEqual(TEXT("Then it recovers"), Brain->GetAction(), EGolemXenonAction::Recover);
	Brain->Step(0.1f, 4.3);
	TestEqual(TEXT("It stands still for the recovery time"), Brain->GetAction(), EGolemXenonAction::Recover);
	Brain->Step(0.1f, 4.4);
	TestEqual(TEXT("And then shows the next attack"), Brain->GetAction(), EGolemXenonAction::Telegraph);

	// A fill during the warning stuns the Xenon and cancels the attack.
	UGolemColorVoidComponent* Open = Xenon->GetVoidHost()->GetOpenVoids().Num() == 1 ? Xenon->GetVoidHost()->GetOpenVoids()[0] : nullptr;
	if (Open == nullptr)
	{
		AddError(TEXT("Grunt has no open void."));
		return false;
	}
	for (const EGolemElement Element : { EGolemElement::Earth, EGolemElement::Sky, EGolemElement::Sea })
	{
		if ((Open->GetMissingMask() & GolemElements::Mask(Element)) != 0)
		{
			Open->ApplyElementAt(Element, 0.0f, Front.Actor.Get(), 4.5);
		}
	}
	TestEqual(TEXT("The void is filled"), Xenon->GetVoidHost()->GetFilledCount(), 1);
	Brain->Step(0.1f, 5.5);
	TestEqual(TEXT("A fill stuns the Xenon"), Brain->GetAction(), EGolemXenonAction::Staggered);
	TestEqual(TEXT("The cancelled attack never lands"), Front.Attributes->GetHealth(), 120.0f);
	TestEqual(TEXT("Still one strike"), Brain->GetStrikeCount(), 1);
	TestFalse(TEXT("A stunned Xenon threatens nothing"), Brain->IsThreatening(Front.Actor->GetActorLocation()));

	// A switched-off brain idles; a restored Xenon stops for good.
	Xenon->GetVoidHost()->AdvanceTo(World->GetTimeSeconds() + Voids.StaggerSeconds + 0.1);
	Brain->SetBrainEnabled(false);
	Brain->Step(0.1f, 20.0);
	TestEqual(TEXT("A disabled brain idles"), Brain->GetAction(), EGolemXenonAction::Idle);
	Brain->SetBrainEnabled(true);
	UGolemAbilitySystemComponent::ApplyCombatDamage(Front.Actor.Get(), Xenon, 100000.0f, 0.0f, true);
	TestTrue(TEXT("Enough body damage restores the Xenon"), Xenon->IsRestored());
	Brain->Step(0.1f, 30.0);
	TestEqual(TEXT("A restored Xenon no longer fights"), Brain->GetAction(), EGolemXenonAction::Restored);
	Brain->Step(0.1f, 40.0);
	TestEqual(TEXT("And it never hits again"), Brain->GetStrikeCount(), 1);
	return true;
}

#endif
