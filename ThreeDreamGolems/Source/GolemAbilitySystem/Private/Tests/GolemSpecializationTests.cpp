#include "Skills/GolemSkillRuntimeComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Attributes/GolemAttributeSet.h"
#include "Components/GolemAbilitySystemComponent.h"
#include "Components/GolemLifeComponent.h"
#include "Components/SceneComponent.h"
#include "CoreGlobals.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Effects/GolemConsumableEffect.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/WorldSettings.h"
#include "HAL/PlatformTime.h"
#include "Misc/AutomationTest.h"
#include "Tags/GolemGameplayTags.h"
#include "UObject/StrongObjectPtr.h"

namespace GolemSpecializationTests
{
	class FWorld final : public FNoncopyable
	{
	public:
		FWorld() : Instance(NewObject<UGameInstance>(GEngine))
		{
			Instance->InitializeStandalone();
			World = Instance->GetWorld();
			World->SetShouldTick(false);
			World->GetWorldSettings()->DefaultGameMode = AGameModeBase::StaticClass();
			FURL URL;
			URL.Map = TEXT("GolemSpecializationDomainTest");
			URL.AddOption(TEXT("game=/Script/Engine.GameModeBase"));
			World->SetGameMode(URL);
			World->InitializeActorsForPlay(URL);
			World->BeginPlay();
		}
		~FWorld()
		{
			World->EndPlay(EEndPlayReason::Quit);
			UWorld* Previous = World.Get();
			Instance->Shutdown();
			Previous->DestroyWorld(false);
			GEngine->DestroyWorldContext(Previous);
		}
		bool TickOnce()
		{
			if (LastTickedFrame == GFrameCounter)
			{
				return false;
			}
			LastTickedFrame = GFrameCounter;
			World->Tick(LEVELTICK_All, 0.05f);
			return true;
		}
		TWeakObjectPtr<UWorld> World;

	private:
		TStrongObjectPtr<UGameInstance> Instance;
		uint64 LastTickedFrame = MAX_uint64;
	};
	struct FFixture
	{
		AActor* Actor = nullptr;
		UGolemAbilitySystemComponent* ASC = nullptr;
		UGolemAttributeSet* Attributes = nullptr;
		UGolemSkillRuntimeComponent* Runtime = nullptr;
	};
	FFixture Spawn(UWorld* World, FVector Location, bool bParty, FName Spec = NAME_None,
		float Power = 100.0f, float Armor = 0.0f)
	{
		FFixture F;
		F.Actor = World->SpawnActor<AActor>();
		USceneComponent* Root = NewObject<USceneComponent>(F.Actor);
		F.Actor->AddInstanceComponent(Root);
		F.Actor->SetRootComponent(Root);
		Root->RegisterComponent();
		F.Actor->SetActorLocation(Location);
		F.ASC = NewObject<UGolemAbilitySystemComponent>(F.Actor);
		F.Actor->AddInstanceComponent(F.ASC);
		F.ASC->RegisterComponent();
		F.Attributes = NewObject<UGolemAttributeSet>(F.Actor);
		UGolemLifeComponent* Life = NewObject<UGolemLifeComponent>(F.Actor);
		F.Actor->AddInstanceComponent(Life);
		Life->RegisterComponent();
		FGolemAttributeDefaults Defaults;
		Defaults.MaxHealth = 1000.0f;
		Defaults.AttackPower = Power;
		Defaults.Defense = Armor;
		F.ASC->InitializeGolemActor(F.Actor, F.Attributes, Life, FGolemEntityId::Create(), Defaults, bParty);
		F.ASC->SetNumericAttributeBase(UGolemAttributeSet::GetCritChanceAttribute(), 0.0f);
		F.Runtime = NewObject<UGolemSkillRuntimeComponent>(F.Actor);
		F.Actor->AddInstanceComponent(F.Runtime);
		F.Runtime->RegisterComponent();
		FGolemSkillBuild Build;
		Build.SpecId = Spec;
		F.Runtime->ConfigureBuild(Build);
		return F;
	}

	/** The engine timer manager accepts one tick per engine frame, including isolated test worlds. */
	class FWaitForSkillTime final : public IAutomationLatentCommand
	{
	public:
		FWaitForSkillTime(FAutomationTestBase* InTest, const TSharedRef<FWorld>& InContext,
			float Seconds, TFunction<void()> InCompletion)
			: Test(InTest), Context(InContext), Completion(MoveTemp(InCompletion)),
			TargetTime(InContext->World->GetTimeSeconds() + Seconds), StartedAt(FPlatformTime::Seconds())
		{
		}
		virtual bool Update() override
		{
			if (!Context->World.IsValid() || !Context->World->HasBegunPlay() ||
				FPlatformTime::Seconds() - StartedAt > 20.0)
			{
				Test->AddError(TEXT("Skill timer fixture lost its owned world or exceeded the bounded wait."));
				return true;
			}
			if (!Context->TickOnce() || Context->World->GetTimeSeconds() < TargetTime)
			{
				return false;
			}
			Completion();
			Completion = nullptr;
			return true;
		}

	private:
		FAutomationTestBase* Test;
		TSharedRef<FWorld> Context;
		TFunction<void()> Completion;
		double TargetTime;
		double StartedAt;
	};

	void Wait(FAutomationTestBase* Test, const TSharedRef<FWorld>& Context,
		float Seconds, TFunction<void()> Completion)
	{
		FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FWaitForSkillTime>(
			Test, Context, Seconds, MoveTemp(Completion)));
	}

	struct FSkillPhase
	{
		float Delay;
		TFunction<void()> Action;
	};

	class FRunSkillPhases final : public IAutomationLatentCommand
	{
	public:
		FRunSkillPhases(FAutomationTestBase* InTest, const TSharedRef<FWorld>& InContext, TArray<FSkillPhase> InPhases)
			: Test(InTest), Context(InContext), Phases(MoveTemp(InPhases)), StartedAt(FPlatformTime::Seconds())
		{
			TargetTime = Context->World->GetTimeSeconds() + Phases[0].Delay;
		}
		virtual bool Update() override
		{
			if (!Context->World.IsValid() || FPlatformTime::Seconds() - StartedAt > 60.0)
			{
				Test->AddError(TEXT("The bounded passive/periodic sequence lost its world or exceeded sixty seconds."));
				return true;
			}
			if (!Context->TickOnce() || Context->World->GetTimeSeconds() + 0.001 < TargetTime)
			{
				return false;
			}
			Phases[PhaseIndex].Action();
			++PhaseIndex;
			if (!Phases.IsValidIndex(PhaseIndex))
			{
				return true;
			}
			TargetTime = Context->World->GetTimeSeconds() + Phases[PhaseIndex].Delay;
			return false;
		}

	private:
		FAutomationTestBase* Test;
		TSharedRef<FWorld> Context;
		TArray<FSkillPhase> Phases;
		int32 PhaseIndex = 0;
		double TargetTime = 0.0;
		double StartedAt;
	};

	void RunPhases(FAutomationTestBase* Test, const TSharedRef<FWorld>& Context, TArray<FSkillPhase> Phases)
	{
		FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FRunSkillPhases>(Test, Context, MoveTemp(Phases)));
	}

	void CheckValue(FAutomationTestBase* Test, const TCHAR* Label, float Actual, float Expected)
	{
		Test->TestTrue(FString::Printf(TEXT("%s: expected %.4f, actual %.4f"), Label, Expected, Actual),
			FMath::IsNearlyEqual(Actual, Expected, 0.01f));
	}

	struct FNineKitsState
	{
		TArray<FName> Specs = FGolemSkillCatalog::AllSpecs();
		TSharedPtr<FWorld> Context;
		FFixture Caster;
		FFixture Ally;
		FFixture Enemy;
		int32 SpecIndex = 0;
		int32 Slot = 1;
	};

	void ContinueNineKits(FAutomationTestBase* Test, const TSharedRef<FNineKitsState>& State)
	{
		if (!State->Specs.IsValidIndex(State->SpecIndex))
		{
			return;
		}
		const FName Spec = State->Specs[State->SpecIndex];
		if (!State->Context.IsValid())
		{
			State->Context = MakeShared<FWorld>();
			State->Caster = Spawn(State->Context->World.Get(), FVector::ZeroVector, true, Spec);
			State->Ally = Spawn(State->Context->World.Get(), FVector(100, 50, 0), true);
			State->Enemy = Spawn(State->Context->World.Get(), FVector(200, 0, 0), false);
			State->Ally.ASC->SetNumericAttributeBase(UGolemAttributeSet::GetHealthAttribute(), 100.0f);
			const int32 Count = State->Caster.ASC->GetActivatableAbilities().Num();
			Test->TestEqual(*FString::Printf(TEXT("%s has four source-owned GAS actions"), *Spec.ToString()), Count, 4);
			State->Caster.Runtime->ConfigureBuild(State->Caster.Runtime->GetBuild());
			Test->TestEqual(TEXT("Repeated projection never duplicates grants"),
				State->Caster.ASC->GetActivatableAbilities().Num(), Count);
		}
		if (State->Slot <= 4)
		{
			const FGolemSpecActionDefinition Definition = FGolemSkillCatalog::GetSkill(
				State->Caster.Runtime->GetBuild(), State->Slot);
			AActor* Target = Definition.bFriendly ? State->Ally.Actor : State->Enemy.Actor;
			Test->TestTrue(*FString::Printf(TEXT("%s A%d accepted through GAS"), *Spec.ToString(), State->Slot),
				State->Caster.Runtime->RequestSkill(State->Slot, Target).IsSuccess());
			Wait(Test, State->Context.ToSharedRef(), 1.5f, [Test, State]()
			{
				++State->Slot;
				ContinueNineKits(Test, State);
			});
			return;
		}
		Test->TestTrue(*FString::Printf(TEXT("%s active buff has confirmed duration"), *Spec.ToString()),
			State->Caster.Runtime->GetSkillState().BuffEnd > State->Context->World->GetTimeSeconds());
		if (FGolemSkillCatalog::Role(Spec) == EGolemSkillRole::Healer)
		{
			Test->TestTrue(*FString::Printf(TEXT("%s really healed ally"), *Spec.ToString()),
				State->Ally.Attributes->GetHealth() > 100.0f);
		}
		if (Spec == TEXT("Jupiter") || Spec == TEXT("Mars"))
		{
			Test->TestTrue(TEXT("Builder did real enemy damage"), State->Enemy.Attributes->GetHealth() < 1000.0f);
		}
		const FGolemSkillRuntimeSnapshot Saved = State->Caster.Runtime->CaptureSnapshot();
		Test->TestTrue(TEXT("Snapshot fragment validates"), UGolemSkillRuntimeComponent::ValidateSnapshot(Saved).IsSuccess());
		const float ResourceBefore = State->Caster.Attributes->GetRuneEnergy();
		FGolemSkillBuild Changed = State->Caster.Runtime->GetBuild();
		Changed.SpecChoice = TEXT("A");
		Test->TestTrue(TEXT("Talent update uses existing stable specs"), State->Caster.Runtime->ConfigureBuild(Changed).IsSuccess());
		Test->TestEqual(TEXT("Talent edit never restores resource"), State->Caster.Attributes->GetRuneEnergy(), ResourceBefore);
		Test->TestEqual(TEXT("Talent edit preserves A4 cooldown"),
			State->Caster.Runtime->CaptureSnapshot().CooldownRemaining[3], Saved.CooldownRemaining[3]);
		State->Caster.Runtime->ConfigureBuild(FGolemSkillBuild());
		Test->TestEqual(TEXT("Removing exact skill source cleans only its four handles"),
			State->Caster.ASC->GetActivatableAbilities().Num(), 0);
		State->Context.Reset();
		State->Slot = 1;
		++State->SpecIndex;
		ContinueNineKits(Test, State);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGolemSkillCatalogTest, "Golem.Skills.CatalogAndPurePreview",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGolemSkillCatalogTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Nine distinct specializations"), FGolemSkillCatalog::AllSpecs().Num(), 9);
	FGolemSkillBuild Build;
	Build.SpecId = TEXT("Terra");
	TestTrue(TEXT("Canonical Earth identity owns Terra"), FGolemSkillCatalog::ValidateBuild(
		FPrimaryAssetId(TEXT("GolemHero"), TEXT("Training_Green")), Build).IsSuccess());
	TestFalse(TEXT("Sky cannot select Earth specialization"), FGolemSkillCatalog::ValidateBuild(
		FPrimaryAssetId(TEXT("GolemHero"), TEXT("Training_Red")), Build).IsSuccess());
	const FGolemSpecActionDefinition Base = FGolemSkillCatalog::GetSkill(Build, 2);
	Build.SkillRune = TEXT("Skill.Mobile");
	const FGolemSpecActionDefinition Mobile = FGolemSkillCatalog::GetSkill(Build, 2);
	TestEqual(TEXT("Mobile radius is 3 meters"), Mobile.Radius, 300.0f);
	TestEqual(TEXT("Mobile duration is 4 seconds"), Mobile.Duration, 4.0f);
	TestEqual(TEXT("Mobile preserves tick magnitude"), Mobile.Coefficient, Base.Coefficient);
	TestEqual(TEXT("Mobile preserves cost"), Mobile.Cost, Base.Cost);
	TestEqual(TEXT("Mobile exposes changed targeting for the real tooltip"), Mobile.Targeting,
		EGolemSkillTargeting::FollowingAlly);
	Build.SpecId = TEXT("Mars");
	Build.SkillRune = NAME_None;
	const FGolemSpecActionDefinition Cone = FGolemSkillCatalog::GetSkill(Build, 2);
	Build.SkillRune = TEXT("Skill.Convergence");
	const FGolemSpecActionDefinition Single = FGolemSkillCatalog::GetSkill(Build, 2);
	TestEqual(TEXT("Seed Mars A2 is a cone"), Cone.Targeting, EGolemSkillTargeting::Cone);
	TestEqual(TEXT("Convergence preview is a single target"), Single.Targeting, EGolemSkillTargeting::Single);
	TestEqual(TEXT("Convergence removes secondary target capacity"), Single.MaxTargets, 1);
	TestFalse(TEXT("Convergence cannot attach to Terra"), FGolemSkillCatalog::IsRuneCompatible(TEXT("Terra"), TEXT("Skill.Convergence")));
	TestEqual(TEXT("Frost penetration retains negative resistance"), FGolemSkillFormula::Mitigate(
		100.0f, EGolemSkillSchool::Frost, 0.0f, -0.25f, 0.0f, 0.0f, 0.20f), 125.0f);
	TestEqual(TEXT("Frost penetration floors positive resistance"), FGolemSkillFormula::Mitigate(
		100.0f, EGolemSkillSchool::Frost, 0.0f, 0.10f, 0.0f, 0.0f, 0.20f), 100.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGolemSkillDamageExecutionTest, "Golem.Skills.SeedDamageExecutionAndMaxHealth",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGolemSkillDamageExecutionTest::RunTest(const FString& Parameters)
{
	const TSharedRef<GolemSpecializationTests::FWorld> Context = MakeShared<GolemSpecializationTests::FWorld>();
	const GolemSpecializationTests::FFixture Enemy = GolemSpecializationTests::Spawn(Context->World.Get(), FVector(200, 0, 0), false);
	const GolemSpecializationTests::FFixture Golem = GolemSpecializationTests::Spawn(Context->World.Get(), FVector::ZeroVector,
		true, TEXT("Uranus"), 50.0f, 100.0f);
	TestTrue(TEXT("A3 executes through GAS"), Golem.Runtime->RequestSkill(3, Golem.Actor).IsSuccess());
	GolemSpecializationTests::Wait(this, Context, 0.8f, [this, Context, Enemy, Golem]()
	{
		TestEqual(TEXT("A3 has real 100 shield pool"), Golem.Runtime->GetSkillState().Shield, 100.0f);
		FGolemSkillDamagePacket Packet;
		Packet.RawDamage = 100.0f * 2.0f * 1.20f * 1.50f;
		Packet.ActivationId = FGuid::NewGuid();
		TestTrue(TEXT("Actual native execution accepts damage packet"),
			UGolemSkillRuntimeComponent::ApplySkillDamage(Enemy.Actor, Golem.Actor, Packet).IsSuccess());
		TestEqual(TEXT("360 raw becomes 288 mitigated, shield100 leaves188 HP damage"), Golem.Attributes->GetHealth(), 812.0f);
		TestEqual(TEXT("Shield consumed before HP"), Golem.Runtime->GetSkillState().Shield, 0.0f);
		Golem.ASC->SetNumericAttributeBase(UGolemAttributeSet::GetMaxHealthAttribute(), 1200.0f);
		TestEqual(TEXT("Increasing max health grants no free current health"), Golem.Attributes->GetHealth(), 812.0f);
		Golem.ASC->SetNumericAttributeBase(UGolemAttributeSet::GetMaxHealthAttribute(), 700.0f);
		TestEqual(TEXT("Decreasing max health clamps absolute health"), Golem.Attributes->GetHealth(), 700.0f);
		Golem.ASC->SetNumericAttributeBase(UGolemAttributeSet::GetFrostResistanceAttribute(), -0.25f);
		TestEqual(TEXT("Negative resistance is retained by GAS clamp"), Golem.Attributes->GetFrostResistance(), -0.25f);
	});
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGolemNeptuneDebtTest, "Golem.Skills.NeptuneTranchesShieldAndCleanse",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGolemNeptuneDebtTest::RunTest(const FString& Parameters)
{
	const TSharedRef<GolemSpecializationTests::FWorld> Context = MakeShared<GolemSpecializationTests::FWorld>();
	const GolemSpecializationTests::FFixture Enemy = GolemSpecializationTests::Spawn(Context->World.Get(), FVector(200, 0, 0), false);
	const GolemSpecializationTests::FFixture Golem = GolemSpecializationTests::Spawn(Context->World.Get(), FVector::ZeroVector,
		true, TEXT("Neptune"), 50.0f);
	TestTrue(TEXT("Neptune obtains shield through A3"), Golem.Runtime->RequestSkill(3, Golem.Actor).IsSuccess());
	GolemSpecializationTests::Wait(this, Context, 0.8f, [this, Context, Enemy, Golem]()
	{
		FGolemSkillDamagePacket Packet;
		Packet.RawDamage = 200.0f;
		Packet.ActivationId = FGuid::NewGuid();
		UGolemSkillRuntimeComponent::ApplySkillDamage(Enemy.Actor, Golem.Actor, Packet);
		Packet.RawDamage = 100.0f;
		Packet.ActivationId = FGuid::NewGuid();
		UGolemSkillRuntimeComponent::ApplySkillDamage(Enemy.Actor, Golem.Actor, Packet);
		TestEqual(TEXT("100 shield absorbs immediate budget, not deferred"), Golem.Attributes->GetHealth(), 875.0f);
		TestEqual(TEXT("Overlapping tranches preserve75 debt"), Golem.Runtime->GetDeferredDamage(), 75.0f);
		FGolemSkillBuild Other;
		Other.SpecId = TEXT("Mercury");
		TestFalse(TEXT("Spec change cannot discard outstanding debt"), Golem.Runtime->ConfigureBuild(Other).IsSuccess());
		GolemSpecializationTests::Wait(this, Context, 1.1f, [this, Context, Golem]()
		{
			TestTrue(TEXT("Tick bypasses depleted shield"), FMath::IsNearlyEqual(Golem.Attributes->GetHealth(), 860.0f));
			TestTrue(TEXT("Still Water commits normally"), Golem.Runtime->RequestSkill(4).IsSuccess());
			GolemSpecializationTests::Wait(this, Context, 0.8f, [this, Context, Golem]()
			{
				TestTrue(TEXT("Cleanse removes exactly half remaining debt"),
					FMath::IsNearlyEqual(Golem.Runtime->GetDeferredDamage(), 30.0f));
				GolemSpecializationTests::Wait(this, Context, 4.0f, [this, Context, Golem]()
				{
					TestTrue(TEXT("Debt eventually reaches zero"), FMath::IsNearlyZero(Golem.Runtime->GetDeferredDamage()));
					TestTrue(TEXT("Damage conservation300-100shield-30cleanse=170HP"),
						FMath::IsNearlyEqual(Golem.Attributes->GetHealth(), 830.0f));
				});
			});
		});
	});
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGolemNineSpecializationTest, "Golem.Skills.NineSpecializationGASLoops",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGolemNineSpecializationTest::RunTest(const FString& Parameters)
{
	GolemSpecializationTests::ContinueNineKits(this, MakeShared<GolemSpecializationTests::FNineKitsState>());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGolemDefenseLayerMasteryTest, "Golem.Skills.Passives.DefenseLayersMastery",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGolemDefenseLayerMasteryTest::RunTest(const FString& Parameters)
{
	using namespace GolemSpecializationTests;
	const TSharedRef<FWorld> Context = MakeShared<FWorld>();
	const FFixture Uranus = Spawn(Context->World.Get(), FVector::ZeroVector, true, TEXT("Uranus"));
	const FFixture Saturn = Spawn(Context->World.Get(), FVector(0, 100, 0), true, TEXT("Saturn"));
	const FFixture Neptune = Spawn(Context->World.Get(), FVector(0, -100, 0), true, TEXT("Neptune"));
	const FFixture Ally = Spawn(Context->World.Get(), FVector(-100, 0, 0), true);
	const FFixture Front = Spawn(Context->World.Get(), FVector(200, 0, 0), false);
	const FFixture Back = Spawn(Context->World.Get(), FVector(-200, 0, 0), false);
	Uranus.ASC->SetNumericAttributeBase(UGolemAttributeSet::GetMasteryBonusAttribute(), 0.30f);
	Saturn.ASC->SetNumericAttributeBase(UGolemAttributeSet::GetMasteryBonusAttribute(), 0.20f);
	Neptune.ASC->SetNumericAttributeBase(UGolemAttributeSet::GetMasteryBonusAttribute(), 0.30f);
	RunPhases(this, Context,
	{
		{ 0.0f, [this, Uranus, Saturn, Front]()
		{
			Uranus.Runtime->NotifyDefenseSuccess();
			Uranus.Runtime->NotifyDefenseSuccess();
			Saturn.Runtime->NotifyHeavyHit(Front.Actor);
			Saturn.Runtime->NotifyHeavyHit(Front.Actor);
			TestEqual(TEXT("Defense gain is limited to one per second"), Uranus.Runtime->GetSkillState().Stacks, 1);
			TestEqual(TEXT("Heavy gain is limited to one per two seconds"), Saturn.Runtime->GetSkillState().Stacks, 1);
		} },
		{ 1.1f, [this, Uranus, Saturn, Front]()
		{
			Uranus.Runtime->NotifyDefenseSuccess();
			Saturn.Runtime->NotifyHeavyHit(Front.Actor);
			TestEqual(TEXT("Uranus gains after one second"), Uranus.Runtime->GetSkillState().Stacks, 2);
			TestEqual(TEXT("Saturn still observes two-second ICD"), Saturn.Runtime->GetSkillState().Stacks, 1);
		} },
		{ 1.1f, [Uranus, Saturn, Front]()
		{
			Uranus.Runtime->NotifyDefenseSuccess();
			Saturn.Runtime->NotifyHeavyHit(Front.Actor);
		} },
		{ 1.1f, [this, Uranus, Saturn, Neptune]()
		{
			Uranus.Runtime->NotifyDefenseSuccess();
			TestEqual(TEXT("Tich Quang caps at three"), Uranus.Runtime->GetSkillState().Stacks, 3);
			TestTrue(TEXT("Uranus consumes through actual A3"), Uranus.Runtime->RequestSkill(3, Uranus.Actor).IsSuccess());
			TestTrue(TEXT("Saturn applies actual A3"), Saturn.Runtime->RequestSkill(3, Saturn.Actor).IsSuccess());
			TestTrue(TEXT("Neptune mastered shield starts"), Neptune.Runtime->RequestSkill(3).IsSuccess());
		} },
		{ 0.8f, [this, Uranus, Saturn, Neptune]()
		{
			CheckValue(this, TEXT("Uranus shield mastery and three stacks each apply once"), Uranus.Runtime->GetSkillState().Shield, 338.0f);
			TestEqual(TEXT("Shield consumes only Uranus stack source"), Uranus.Runtime->GetSkillState().Stacks, 0);
			CheckValue(this, TEXT("Saturn mastery applies only to shield"), Saturn.Runtime->GetSkillState().Shield, 264.0f);
			TestEqual(TEXT("A3 obeys the same layer gain ICD"), Saturn.Runtime->GetSkillState().Stacks, 2);
			CheckValue(this, TEXT("Neptune mastery scales only its A3 shield"), Neptune.Runtime->GetSkillState().Shield, 260.0f);
			TestTrue(TEXT("Uranus directional buff commits"), Uranus.Runtime->RequestSkill(4).IsSuccess());
			TestTrue(TEXT("Saturn four-layer buff commits"), Saturn.Runtime->RequestSkill(4).IsSuccess());
		} },
		{ 0.8f, [this, Uranus, Saturn, Neptune, Ally, Front, Back]()
		{
			TestEqual(TEXT("Tam Son forces four layers"), Saturn.Runtime->GetSkillState().Stacks, 4);
			CheckValue(this, TEXT("Saturn poise is halved"), Saturn.Runtime->GetPoiseScale(), 0.5f);
			CheckValue(this, TEXT("Four layers reduce incoming damage by twelve percent"), Saturn.Runtime->GetIncomingReduction(Back.Actor), 0.12f);
			FGolemSkillDamagePacket Packet;
			Packet.RawDamage = 100.0f;
			Packet.ActivationId = FGuid::NewGuid();
			UGolemSkillRuntimeComponent::ApplySkillDamage(Front.Actor, Ally.Actor, Packet);
			CheckValue(this, TEXT("Front-source hit behind Uranus receives real directional reduction"), Ally.Attributes->GetHealth(), 925.0f);
			Packet.ActivationId = FGuid::NewGuid();
			UGolemSkillRuntimeComponent::ApplySkillDamage(Back.Actor, Ally.Actor, Packet);
			CheckValue(this, TEXT("Rear-source hit receives no directional reduction"), Ally.Attributes->GetHealth(), 825.0f);
			Packet.ActivationId = FGuid::NewGuid();
			UGolemSkillRuntimeComponent::ApplySkillDamage(Front.Actor, Neptune.Actor, Packet);
			CheckValue(this, TEXT("Neptune mastery does not increase the twenty-five percent delay ratio"), Neptune.Runtime->GetDeferredDamage(), 25.0f);
		} },
		{ 1.1f, [this, Neptune]()
		{
			CheckValue(this, TEXT("Mastered shield cannot absorb deferred ticks"), Neptune.Attributes->GetHealth(), 995.0f);
		} }
	});
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGolemVenusProcTest, "Golem.Skills.Passives.VenusConsumeActualShield",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGolemVenusProcTest::RunTest(const FString& Parameters)
{
	using namespace GolemSpecializationTests;
	const TSharedRef<FWorld> Context = MakeShared<FWorld>();
	const FFixture Caster = Spawn(Context->World.Get(), FVector::ZeroVector, true, TEXT("Venus"));
	const FFixture Ally = Spawn(Context->World.Get(), FVector(100, 0, 0), true);
	const FFixture Other = Spawn(Context->World.Get(), FVector(100, 100, 0), true);
	Caster.ASC->SetNumericAttributeBase(UGolemAttributeSet::GetMasteryBonusAttribute(), 0.30f);
	Ally.ASC->SetNumericAttributeBase(UGolemAttributeSet::GetHealthAttribute(), 100.0f);
	Other.ASC->SetNumericAttributeBase(UGolemAttributeSet::GetHealthAttribute(), 100.0f);
	RunPhases(this, Context,
	{
		{ 0.0f, [this, Caster]()
		{
			TestTrue(TEXT("Venus A4 starts"), Caster.Runtime->RequestSkill(4).IsSuccess());
		} },
		{ 0.8f, [this, Caster, Ally]()
		{
			TestTrue(TEXT("Venus AoE heal starts"), Caster.Runtime->RequestSkill(3, Ally.Actor).IsSuccess());
		} },
		{ 0.8f, [this, Caster, Ally, Other]()
		{
			CheckValue(this, TEXT("A3 heals first ally without A2 mastery"), Ally.Attributes->GetHealth(), 220.0f);
			CheckValue(this, TEXT("A3 heals second ally"), Other.Attributes->GetHealth(), 220.0f);
			TestEqual(TEXT("AoE actual healing adds one stack for the whole cast"), Caster.Runtime->GetSkillState().Stacks, 1);
			CheckValue(this, TEXT("Venus shield uses actual heal per target"), Ally.Runtime->GetSkillState().Shield, 30.0f);
			Ally.ASC->SetNumericAttributeBase(UGolemAttributeSet::GetHealthAttribute(), 100.0f);
			TestTrue(TEXT("Mastered A2 starts"), Caster.Runtime->RequestSkill(2, Ally.Actor).IsSuccess());
		} },
		{ 0.8f, [this, Caster, Ally]()
		{
			CheckValue(this, TEXT("Mastery scales only A2 direct healing"), Ally.Attributes->GetHealth(), 386.0f);
			CheckValue(this, TEXT("Same-source shield refresh keeps maximum instead of adding"), Ally.Runtime->GetSkillState().Shield, 71.5f);
			TestEqual(TEXT("Second direct cast adds second stack"), Caster.Runtime->GetSkillState().Stacks, 2);
		} },
		{ 6.1f, [this, Caster, Ally]()
		{
			Ally.ASC->SetNumericAttributeBase(UGolemAttributeSet::GetHealthAttribute(), 100.0f);
			TestTrue(TEXT("Third actual direct heal starts after cooldown"), Caster.Runtime->RequestSkill(2, Ally.Actor).IsSuccess());
		} },
		{ 0.8f, [this, Caster]()
		{
			TestEqual(TEXT("Venus proc reaches three stacks"), Caster.Runtime->GetSkillState().Stacks, 3);
		} },
		{ 4.8f, [this, Caster, Ally, Other]()
		{
			Ally.ASC->SetNumericAttributeBase(UGolemAttributeSet::GetHealthAttribute(), 100.0f);
			Other.ASC->SetNumericAttributeBase(UGolemAttributeSet::GetHealthAttribute(), 100.0f);
			Caster.ASC->SetNumericAttributeBase(UGolemAttributeSet::GetRuneEnergyAttribute(), 100.0f);
			TestTrue(TEXT("Empowered direct heal starts"), Caster.Runtime->RequestSkill(3, Ally.Actor).IsSuccess());
		} },
		{ 0.8f, [this, Caster, Ally, Other]()
		{
			CheckValue(this, TEXT("Empowered AoE increases the entire cast by thirty percent"), Ally.Attributes->GetHealth(), 256.0f);
			CheckValue(this, TEXT("Empowered bonus also applies to secondary AoE target"), Other.Attributes->GetHealth(), 256.0f);
			CheckValue(this, TEXT("Empowered cast spends half A3 cost"), Caster.Attributes->GetRuneEnergy(), 90.0f);
			TestEqual(TEXT("Consume does not simultaneously gain a new stack"), Caster.Runtime->GetSkillState().Stacks, 0);
		} }
	});
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGolemMercuryPeriodicTest, "Golem.Skills.Passives.MercuryRefreshPropagation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGolemMercuryPeriodicTest::RunTest(const FString& Parameters)
{
	using namespace GolemSpecializationTests;
	const TSharedRef<FWorld> Context = MakeShared<FWorld>();
	const FFixture Caster = Spawn(Context->World.Get(), FVector::ZeroVector, true, TEXT("Mercury"));
	const FFixture Ally = Spawn(Context->World.Get(), FVector(100, 0, 0), true);
	const FFixture Other = Spawn(Context->World.Get(), FVector(100, 100, 0), true);
	FGolemSkillBuild Build = Caster.Runtime->GetBuild();
	Build.SkillRune = TEXT("Skill.Propagation");
	Caster.Runtime->ConfigureBuild(Build);
	Caster.ASC->SetNumericAttributeBase(UGolemAttributeSet::GetMasteryBonusAttribute(), 0.20f);
	Caster.ASC->SetNumericAttributeBase(UGolemAttributeSet::GetCooldownRecoveryAttribute(), 0.30f);
	Ally.ASC->SetNumericAttributeBase(UGolemAttributeSet::GetHealthAttribute(), 100.0f);
	Other.ASC->SetNumericAttributeBase(UGolemAttributeSet::GetHealthAttribute(), 100.0f);
	RunPhases(this, Context,
	{
		{ 0.0f, [this, Caster]()
		{
			TestTrue(TEXT("Mercury active buff starts"), Caster.Runtime->RequestSkill(4).IsSuccess());
		} },
		{ 0.8f, [this, Caster, Ally]()
		{
			TestTrue(TEXT("Primary HoT starts"), Caster.Runtime->RequestSkill(2, Ally.Actor).IsSuccess());
		} },
		{ 1.6f, [this, Caster, Ally, Other]()
		{
			CheckValue(this, TEXT("First HoT tick snapshots one unique target and mastery"), Ally.Attributes->GetHealth(), 143.68f);
			CheckValue(this, TEXT("Buff secondary is thirty percent of pre-clamp tick"), Other.Attributes->GetHealth(), 113.104f);
			TestTrue(TEXT("Propagation direct heal starts"), Caster.Runtime->RequestSkill(3, Ally.Actor).IsSuccess());
		} },
		{ 0.8f, [this, Ally, Other]()
		{
			CheckValue(this, TEXT("Primary direct heal includes one HoT bonus and propagation reduction"), Ally.Attributes->GetHealth(), 302.8f);
			CheckValue(this, TEXT("Propagation secondary does not recurse or gain a HoT"), Other.Attributes->GetHealth(), 160.84f);
		} },
		{ 2.9f, [this, Caster, Ally]()
		{
			TestTrue(TEXT("Same-source HoT refresh starts before expiry"), Caster.Runtime->RequestSkill(2, Ally.Actor).IsSuccess());
		} },
		{ 0.65f, [this, Ally, Other]()
		{
			CheckValue(this, TEXT("Refresh retains one periodic stream"), Ally.Attributes->GetHealth(), 477.52f);
			CheckValue(this, TEXT("Secondary remains nonrecursive"), Other.Attributes->GetHealth(), 213.256f);
		} },
		{ 0.65f, [this, Caster, Ally, Other]()
		{
			CheckValue(this, TEXT("Refresh preserves original one-second cadence"), Ally.Attributes->GetHealth(), 521.2f);
			CheckValue(this, TEXT("Only one extra secondary tick occurs"), Other.Attributes->GetHealth(), 226.36f);
			TestEqual(TEXT("Heals and secondary ticks never create foreign proc stacks"), Caster.Runtime->GetSkillState().Stacks, 0);
		} }
	});
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGolemJupiterProcTest, "Golem.Skills.Passives.JupiterChargesSecondaryMastery",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGolemJupiterProcTest::RunTest(const FString& Parameters)
{
	using namespace GolemSpecializationTests;
	const TSharedRef<FWorld> Context = MakeShared<FWorld>();
	const FFixture Caster = Spawn(Context->World.Get(), FVector::ZeroVector, true, TEXT("Jupiter"));
	const FFixture Main = Spawn(Context->World.Get(), FVector(200, 0, 0), false);
	const FFixture First = Spawn(Context->World.Get(), FVector(200, 50, 0), false);
	const FFixture Second = Spawn(Context->World.Get(), FVector(200, 100, 0), false);
	const FFixture Third = Spawn(Context->World.Get(), FVector(200, 150, 0), false);
	Caster.ASC->SetNumericAttributeBase(UGolemAttributeSet::GetMasteryBonusAttribute(), 0.30f);
	First.ASC->SetNumericAttributeBase(UGolemAttributeSet::GetLightningResistanceAttribute(), 0.20f);
	RunPhases(this, Context,
	{
		{ 0.0f, [this, Caster]()
		{
			TestTrue(TEXT("Jupiter active buff starts"), Caster.Runtime->RequestSkill(4).IsSuccess());
		} },
		{ 0.8f, [this, Caster, Main]()
		{
			TestTrue(TEXT("Jupiter builder starts"), Caster.Runtime->RequestSkill(1, Main.Actor).IsSuccess());
		} },
		{ 0.8f, [this, Caster, Main]()
		{
			TestEqual(TEXT("Buff builder grants exactly two charges"), Caster.Runtime->GetSkillState().Stacks, 2);
			CheckValue(this, TEXT("Mastery does not scale Jupiter builder"), Main.Attributes->GetHealth(), 910.0f);
			TestTrue(TEXT("Jupiter spender starts"), Caster.Runtime->RequestSkill(2, Main.Actor).IsSuccess());
		} },
		{ 1.2f, [this, Caster, Main, First, Second, Third]()
		{
			CheckValue(this, TEXT("A2 mastery and consumed charge multiplier each apply once"), Main.Attributes->GetHealth(), 589.68f);
			CheckValue(this, TEXT("Secondary packet reads its own live resistance"), First.Attributes->GetHealth(), 923.1232f);
			CheckValue(this, TEXT("Second secondary gets thirty percent raw main damage"), Second.Attributes->GetHealth(), 903.904f);
			CheckValue(this, TEXT("Base buff caps secondaries at two"), Third.Attributes->GetHealth(), 1000.0f);
			TestEqual(TEXT("Spender and secondary packets consume without rebuilding charges"), Caster.Runtime->GetSkillState().Stacks, 0);
			FGolemSkillBuild Changed = Caster.Runtime->GetBuild();
			Changed.SpecChoice = TEXT("B");
			TestTrue(TEXT("Talent B updates existing buff without resetting cooldown"), Caster.Runtime->ConfigureBuild(Changed).IsSuccess());
		} },
		{ 6.2f, [this, Caster, Main]()
		{
			TestTrue(TEXT("Second spender uses retained cooldown"), Caster.Runtime->RequestSkill(2, Main.Actor).IsSuccess());
		} },
		{ 0.8f, [this, Caster, Third]()
		{
			CheckValue(this, TEXT("Talent B permits a third real secondary hit"), Third.Attributes->GetHealth(), 914.2f);
			TestEqual(TEXT("Secondary never creates charges"), Caster.Runtime->GetSkillState().Stacks, 0);
		} }
	});
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGolemPlutoReactionTest, "Golem.Skills.Passives.PlutoPhaseSharedShatter",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGolemPlutoReactionTest::RunTest(const FString& Parameters)
{
	using namespace GolemSpecializationTests;
	const TSharedRef<FWorld> Context = MakeShared<FWorld>();
	const FFixture Caster = Spawn(Context->World.Get(), FVector::ZeroVector, true, TEXT("Pluto"));
	const FFixture Other = Spawn(Context->World.Get(), FVector(0, 100, 0), true, TEXT("Pluto"));
	const FFixture Enemy = Spawn(Context->World.Get(), FVector(200, 0, 0), false);
	Caster.ASC->SetNumericAttributeBase(UGolemAttributeSet::GetMasteryBonusAttribute(), 0.30f);
	Enemy.ASC->SetNumericAttributeBase(UGolemAttributeSet::GetFrostResistanceAttribute(), 0.30f);
	RunPhases(this, Context,
	{
		{ 0.0f, [this, Caster]()
		{
			TestTrue(TEXT("Pluto active buff starts"), Caster.Runtime->RequestSkill(4).IsSuccess());
		} },
		{ 0.8f, [this, Caster, Enemy]()
		{
			TestTrue(TEXT("Ice AoE starts"), Caster.Runtime->RequestSkill(3, Enemy.Actor).IsSuccess());
		} },
		{ 0.8f, [this, Caster, Enemy]()
		{
			CheckValue(this, TEXT("Ice direct hit penetrates only twenty positive resistance points"), Enemy.Attributes->GetHealth(), 910.0f);
			TestEqual(TEXT("Ice AoE adds two chilled without shattering"), Enemy.Runtime->GetSkillState().Chilled, 2);
			TestTrue(TEXT("Water phase starts"), Caster.Runtime->RequestSkill(1, Enemy.Actor).IsSuccess());
		} },
		{ 0.8f, [this, Context, Caster, Enemy]()
		{
			CheckValue(this, TEXT("Water family does not inherit Ice penetration or mastery"), Enemy.Attributes->GetHealth(), 840.0f);
			TestEqual(TEXT("Shared target reaches three chilled"), Enemy.Runtime->GetSkillState().Chilled, 3);
			TestTrue(TEXT("Water readies the next Ice attack"), Caster.Runtime->GetSkillState().IceReadyEnd > Context->World->GetTimeSeconds());
			TestTrue(TEXT("CanShatter spender starts"), Caster.Runtime->RequestSkill(2, Enemy.Actor).IsSuccess());
		} },
		{ 0.8f, [this, Caster, Other, Enemy]()
		{
			CheckValue(this, TEXT("Empowered ice plus mastered buffed Shatter preserve separate damage factors"), Enemy.Attributes->GetHealth(), 534.15f);
			TestEqual(TEXT("Shatter consumes shared chilled"), Enemy.Runtime->GetSkillState().Chilled, 0);
			TestTrue(TEXT("Other Pluto builds chilled through the same GAS action"), Other.Runtime->RequestSkill(3, Enemy.Actor).IsSuccess());
		} },
		{ 0.8f, [this, Other, Enemy]()
		{
			TestTrue(TEXT("Other Pluto water hit starts"), Other.Runtime->RequestSkill(1, Enemy.Actor).IsSuccess());
		} },
		{ 0.8f, [this, Other, Enemy]()
		{
			TestTrue(TEXT("Other Pluto spender starts inside shared reaction ICD"), Other.Runtime->RequestSkill(2, Enemy.Actor).IsSuccess());
		} },
		{ 0.8f, [this, Enemy]()
		{
			CheckValue(this, TEXT("Shared four-second ICD prevents a second party Shatter"), Enemy.Attributes->GetHealth(), 209.35f);
			TestEqual(TEXT("Blocked reaction keeps three shared chilled stacks"), Enemy.Runtime->GetSkillState().Chilled, 3);
		} }
	});
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGolemMarsPeriodicTest, "Golem.Skills.Passives.MarsConvergenceBurnRefresh",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGolemMarsPeriodicTest::RunTest(const FString& Parameters)
{
	using namespace GolemSpecializationTests;
	const TSharedRef<FWorld> Context = MakeShared<FWorld>();
	const FFixture Caster = Spawn(Context->World.Get(), FVector::ZeroVector, true, TEXT("Mars"));
	const FFixture Enemy = Spawn(Context->World.Get(), FVector(200, 0, 0), false);
	const FFixture Other = Spawn(Context->World.Get(), FVector(200, 50, 0), false);
	FGolemSkillBuild Build = Caster.Runtime->GetBuild();
	Build.SkillRune = TEXT("Skill.Convergence");
	Caster.Runtime->ConfigureBuild(Build);
	Caster.ASC->SetNumericAttributeBase(UGolemAttributeSet::GetMasteryBonusAttribute(), 0.30f);
	Caster.ASC->SetNumericAttributeBase(UGolemAttributeSet::GetCooldownRecoveryAttribute(), 0.30f);
	Enemy.ASC->SetNumericAttributeBase(UGolemAttributeSet::GetMaxHealthAttribute(), 10000.0f);
	Enemy.ASC->SetNumericAttributeBase(UGolemAttributeSet::GetHealthAttribute(), 10000.0f);
	RunPhases(this, Context,
	{
		{ 0.0f, [this, Caster]()
		{
			TestTrue(TEXT("Mars active buff starts"), Caster.Runtime->RequestSkill(4).IsSuccess());
		} },
		{ 0.8f, [this, Caster, Enemy]()
		{
			TestTrue(TEXT("EarthStone builder starts"), Caster.Runtime->RequestSkill(1, Enemy.Actor).IsSuccess());
		} },
		{ 0.8f, [this, Caster, Enemy]()
		{
			TestEqual(TEXT("Buff grants exactly two Heat per builder cast"), Caster.Runtime->GetSkillState().Stacks, 2);
			TestTrue(TEXT("Converged spender starts"), Caster.Runtime->RequestSkill(2, Enemy.Actor).IsSuccess());
		} },
		{ 0.8f, [this, Caster, Enemy, Other]()
		{
			CheckValue(this, TEXT("Convergence modifies raw direct damage once"), Enemy.Attributes->GetHealth(), 9636.0f);
			CheckValue(this, TEXT("Convergence removes real cone secondary targeting"), Other.Attributes->GetHealth(), 1000.0f);
			TestEqual(TEXT("Fire consumes two Heat then refunds two"), Caster.Runtime->GetSkillState().Stacks, 2);
		} },
		{ 1.2f, [this, Caster, Enemy]()
		{
			CheckValue(this, TEXT("Burn snapshots converged raw base and mastery exactly once"), Enemy.Attributes->GetHealth(), 9626.848f);
			TestEqual(TEXT("Burn ticks cannot generate Heat"), Caster.Runtime->GetSkillState().Stacks, 2);
			TestTrue(TEXT("Next builder starts after its cooldown"), Caster.Runtime->RequestSkill(1, Enemy.Actor).IsSuccess());
		} },
		{ 0.8f, [this, Caster]()
		{
			TestEqual(TEXT("Next builder raises Heat to four"), Caster.Runtime->GetSkillState().Stacks, 4);
		} },
		{ 2.5f, [this, Caster, Enemy]()
		{
			TestTrue(TEXT("Refresh spender starts before old Burn expires"), Caster.Runtime->RequestSkill(2, Enemy.Actor).IsSuccess());
		} },
		{ 0.8f, [this, Caster, Enemy]()
		{
			CheckValue(this, TEXT("Refresh leaves five old ticks and two direct hits"), Enemy.Attributes->GetHealth(), 9226.24f);
			TestEqual(TEXT("Refresh refunds only two consumed Heat"), Caster.Runtime->GetSkillState().Stacks, 2);
		} },
		{ 0.5f, [this, Caster, Enemy, Other]()
		{
			CheckValue(this, TEXT("Refreshed Burn preserves cadence with the new four-stack snapshot"), Enemy.Attributes->GetHealth(), 9207.936f);
			CheckValue(this, TEXT("Single-target Burn never reaches nearby nonprimary"), Other.Attributes->GetHealth(), 1000.0f);
			TestEqual(TEXT("No recursive proc from refreshed tick"), Caster.Runtime->GetSkillState().Stacks, 2);
		} }
	});
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGolemTerraPeriodicTest, "Golem.Skills.Passives.TerraEmberMobileOverheal",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGolemTerraPeriodicTest::RunTest(const FString& Parameters)
{
	using namespace GolemSpecializationTests;
	const TSharedRef<FWorld> Context = MakeShared<FWorld>();
	const FFixture Caster = Spawn(Context->World.Get(), FVector::ZeroVector, true, TEXT("Terra"));
	const FFixture Ally = Spawn(Context->World.Get(), FVector(100, 0, 0), true);
	const FFixture Other = Spawn(Context->World.Get(), FVector(100, 100, 0), true);
	FGolemSkillBuild Build = Caster.Runtime->GetBuild();
	Build.SkillRune = TEXT("Skill.Mobile");
	Caster.Runtime->ConfigureBuild(Build);
	Caster.ASC->SetNumericAttributeBase(UGolemAttributeSet::GetMasteryBonusAttribute(), 0.20f);
	Caster.ASC->SetNumericAttributeBase(UGolemAttributeSet::GetCooldownRecoveryAttribute(), 0.30f);
	Ally.ASC->SetNumericAttributeBase(UGolemAttributeSet::GetHealthAttribute(), 100.0f);
	Other.ASC->SetNumericAttributeBase(UGolemAttributeSet::GetHealthAttribute(), 100.0f);
	RunPhases(this, Context,
	{
		{ 0.0f, [this, Caster, Ally]()
		{
			TestTrue(TEXT("First Ember cast starts"), Caster.Runtime->RequestSkill(3, Ally.Actor).IsSuccess());
		} },
		{ 0.8f, [this, Caster, Ally]()
		{
			CheckValue(this, TEXT("A2 mastery never scales Terra direct A3"), Ally.Attributes->GetHealth(), 300.0f);
			TestEqual(TEXT("First actual direct cast gains one Ember"), Caster.Runtime->GetSkillState().Stacks, 1);
		} },
		{ 9.0f, [this, Caster, Ally]()
		{
			TestTrue(TEXT("Second Ember cast starts"), Caster.Runtime->RequestSkill(3, Ally.Actor).IsSuccess());
		} },
		{ 0.8f, [this, Caster]()
		{
			TestEqual(TEXT("Second actual direct cast gains second Ember"), Caster.Runtime->GetSkillState().Stacks, 2);
		} },
		{ 9.0f, [this, Caster, Ally]()
		{
			TestTrue(TEXT("Third Ember cast starts"), Caster.Runtime->RequestSkill(3, Ally.Actor).IsSuccess());
		} },
		{ 0.8f, [this, Caster]()
		{
			TestEqual(TEXT("Three direct casts reach three Ember"), Caster.Runtime->GetSkillState().Stacks, 3);
			TestTrue(TEXT("Terra active buff starts"), Caster.Runtime->RequestSkill(4).IsSuccess());
		} },
		{ 0.8f, [this, Caster, Ally]()
		{
			TestTrue(TEXT("Mobile ground heal starts"), Caster.Runtime->RequestSkill(2, Ally.Actor).IsSuccess());
		} },
		{ 0.8f, [this, Caster, Ally, Other]()
		{
			CheckValue(this, TEXT("Mobile placement does not immediately heal"), Ally.Attributes->GetHealth(), 700.0f);
			Ally.Actor->SetActorLocation(FVector(700, 0, 0));
			Other.Actor->SetActorLocation(FVector(700, 100, 0));
			Ally.ASC->SetNumericAttributeBase(UGolemAttributeSet::GetHealthAttribute(), 950.0f);
			TestEqual(TEXT("Ground placement does not consume or gain Ember"), Caster.Runtime->GetSkillState().Stacks, 3);
		} },
		{ 3.9f, [this, Caster, Ally, Other]()
		{
			CheckValue(this, TEXT("Mobile heal follows selected ally for four mastered ticks"), Ally.Attributes->GetHealth(), 1000.0f);
			CheckValue(this, TEXT("Low HP bonus uses live target HP before each tick"), Other.Attributes->GetHealth(), 310.0f);
			CheckValue(this, TEXT("Overheal converts once and refreshes same-source maximum"), Ally.Runtime->GetSkillState().Shield, 12.6f);
			TestEqual(TEXT("Ground ticks never create Ember"), Caster.Runtime->GetSkillState().Stacks, 3);
		} },
		{ 3.3f, [this, Caster, Ally, Other]()
		{
			CheckValue(this, TEXT("Mobile expires after four seconds without a fifth tick"), Other.Attributes->GetHealth(), 310.0f);
			Ally.ASC->SetNumericAttributeBase(UGolemAttributeSet::GetHealthAttribute(), 100.0f);
			TestTrue(TEXT("Three-Ember consumer starts within retained proc lifetime"), Caster.Runtime->RequestSkill(3, Ally.Actor).IsSuccess());
		} },
		{ 0.8f, [this, Caster, Ally]()
		{
			CheckValue(this, TEXT("Buffed low-health direct heal is two hundred fifty"), Ally.Attributes->GetHealth(), 350.0f);
			TestEqual(TEXT("Ember consumer does not regain a stack"), Caster.Runtime->GetSkillState().Stacks, 0);
		} },
		{ 1.6f, [this, Ally]()
		{
			CheckValue(this, TEXT("Ember HoT has one live buffed tick then one normal tick"), Ally.Attributes->GetHealth(), 372.5f);
		} },
		{ 4.1f, [this, Caster, Ally]()
		{
			CheckValue(this, TEXT("Six Ember ticks derive from unbuffed base heal and do not gain A2 mastery"), Ally.Attributes->GetHealth(), 412.5f);
			TestEqual(TEXT("Passive HoT never regenerates Ember"), Caster.Runtime->GetSkillState().Stacks, 0);
		} }
	});
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGolemAftershockTest, "Golem.Skills.Runes.AftershockCapCooldownSnapshot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGolemAftershockTest::RunTest(const FString& Parameters)
{
	using namespace GolemSpecializationTests;
	const TSharedRef<FWorld> Context = MakeShared<FWorld>();
	const FFixture Caster = Spawn(Context->World.Get(), FVector::ZeroVector, true, TEXT("Saturn"));
	const FFixture Restored = Spawn(Context->World.Get(), FVector(0, 100, 0), true, TEXT("Saturn"));
	TArray<FFixture> Enemies;
	for (int32 Index = 0; Index < 4; ++Index)
	{
		Enemies.Add(Spawn(Context->World.Get(), FVector(200 + Index * 40, 0, 0), false));
	}
	FGolemSkillBuild Build = Caster.Runtime->GetBuild();
	Build.WeaponRune = TEXT("Weapon.Aftershock");
	Caster.Runtime->ConfigureBuild(Build);
	Restored.Runtime->ConfigureBuild(Build);
	Caster.ASC->SetNumericAttributeBase(UGolemAttributeSet::GetVersatilityAttribute(), 0.20f);
	Restored.ASC->SetNumericAttributeBase(UGolemAttributeSet::GetVersatilityAttribute(), 0.20f);
	RunPhases(this, Context,
	{
		{ 0.0f, [this, Caster, Enemies]()
		{
			Caster.Runtime->NotifyHeavyHit(Enemies[0].Actor);
			Caster.Runtime->NotifyHeavyHit(Enemies[0].Actor);
			for (int32 Index = 0; Index < 3; ++Index)
			{
				CheckValue(this, TEXT("Aftershock applies outgoing Versatility once to at most three enemies"), Enemies[Index].Attributes->GetHealth(), 964.0f);
			}
			CheckValue(this, TEXT("Fourth enemy is excluded"), Enemies[3].Attributes->GetHealth(), 1000.0f);
		} },
		{ 3.8f, [this, Caster, Enemies]()
		{
			Caster.Runtime->NotifyHeavyHit(Enemies[0].Actor);
			CheckValue(this, TEXT("Heavy hit inside four-second ICD cannot repeat Aftershock"), Enemies[0].Attributes->GetHealth(), 964.0f);
		} },
		{ 0.4f, [this, Caster, Restored, Enemies]()
		{
			Caster.Runtime->NotifyHeavyHit(Enemies[0].Actor);
			CheckValue(this, TEXT("Aftershock resumes after four-second ICD"), Enemies[0].Attributes->GetHealth(), 928.0f);
			const FGolemSkillRuntimeSnapshot Saved = Caster.Runtime->CaptureSnapshot();
			TestTrue(TEXT("Snapshot retains the rune proc cooldown"), Saved.AftershockRemaining > 3.9f);
			TestTrue(TEXT("Reconstructed runtime restores the valid fragment"), Restored.Runtime->RestoreSnapshot(Saved).IsSuccess());
			Restored.Runtime->NotifyHeavyHit(Enemies[0].Actor);
			CheckValue(this, TEXT("Reconstruction cannot reset the Aftershock ICD"), Enemies[0].Attributes->GetHealth(), 928.0f);
		} },
		{ 4.1f, [this, Caster, Enemies]()
		{
			FGolemSkillBuild Detached = Caster.Runtime->GetBuild();
			Detached.WeaponRune = NAME_None;
			TestTrue(TEXT("Exact weapon rune source detaches"), Caster.Runtime->ConfigureBuild(Detached).IsSuccess());
			Caster.Runtime->NotifyHeavyHit(Enemies[0].Actor);
			CheckValue(this, TEXT("Detached rune no longer produces AoE after cooldown is available"), Enemies[0].Attributes->GetHealth(), 928.0f);
		} }
	});
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGolemSkillCommitLifecycleTest, "Golem.Skills.Lifecycle.CancelCommitAndBuildFence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGolemSkillCommitLifecycleTest::RunTest(const FString& Parameters)
{
	using namespace GolemSpecializationTests;
	const TSharedRef<FWorld> Context = MakeShared<FWorld>();
	const FFixture Caster = Spawn(Context->World.Get(), FVector::ZeroVector, true, TEXT("Jupiter"));
	const FFixture Enemy = Spawn(Context->World.Get(), FVector(1000, 0, 0), false);
	RunPhases(this, Context,
	{
		{ 0.0f, [this, Caster, Enemy]()
		{
			TestTrue(TEXT("A2 starts its actual GAS windup"), Caster.Runtime->RequestSkill(2, Enemy.Actor).IsSuccess());
			TestTrue(TEXT("Runtime exposes the active skill during windup"), Caster.Runtime->HasActiveSkill());
			TestTrue(TEXT("In-flight activation prevents incomplete snapshot capture"), Caster.Runtime->HasUnsnapshottedEffects());
			FGolemSkillBuild Changed = Caster.Runtime->GetBuild();
			Changed.SpecId = TEXT("Mars");
			TestFalse(TEXT("A sanctuary build cannot replace an active skill during windup"), Caster.Runtime->ConfigureBuild(Changed).IsSuccess());
			Caster.ASC->CancelAbilityHandle(Caster.Runtime->GetSkillHandle(2));
			CheckValue(this, TEXT("Cancel before commit does not spend resource"), Caster.Attributes->GetRuneEnergy(), 100.0f);
			TestTrue(TEXT("Cancel before commit starts no cooldown"), Caster.Runtime->IsSkillAvailable(2));
			TestFalse(TEXT("Cancelled skill cleans its active state"), Caster.Runtime->HasActiveSkill());
			TestTrue(TEXT("Same GAS spec can activate after cancellation"), Caster.Runtime->RequestSkill(2, Enemy.Actor).IsSuccess());
		} },
		{ 0.55f, [this, Caster]()
		{
			CheckValue(this, TEXT("Actual commit spends the exact A2 cost"), Caster.Attributes->GetRuneEnergy(), 75.0f);
			TestFalse(TEXT("Actual commit starts the stable cooldown"), Caster.Runtime->IsSkillAvailable(2));
			Caster.ASC->CancelAbilityHandle(Caster.Runtime->GetSkillHandle(2));
			CheckValue(this, TEXT("Cancel after commit never refunds resource"), Caster.Attributes->GetRuneEnergy(), 75.0f);
			FGolemSkillBuild Changed = Caster.Runtime->GetBuild();
			Changed.SpecId = TEXT("Mars");
			TestFalse(TEXT("Pending committed projectile prevents build replacement"), Caster.Runtime->ConfigureBuild(Changed).IsSuccess());
		} },
		{ 0.7f, [this, Caster, Enemy]()
		{
			CheckValue(this, TEXT("Committed projectile resolves after the ability recovery is cancelled"), Enemy.Attributes->GetHealth(), 780.0f);
			TestFalse(TEXT("Projectile resolves its unfinished state"), Caster.Runtime->HasUnsnapshottedEffects());
			const float Remaining = Caster.Runtime->CaptureSnapshot().CooldownRemaining[1];
			FGolemSkillBuild Changed = Caster.Runtime->GetBuild();
			Changed.SpecId = TEXT("Mars");
			TestTrue(TEXT("Build can update once the committed projectile resolves"), Caster.Runtime->ConfigureBuild(Changed).IsSuccess());
			CheckValue(this, TEXT("Build replacement preserves committed cooldown"), Caster.Runtime->CaptureSnapshot().CooldownRemaining[1], Remaining);
		} }
	});
	return true;
}

namespace GolemSpecializationTests
{
	bool InstallEquipment(FAutomationTestBase* Test, const FFixture& Fixture, FName Family = NAME_None,
		FGuid ItemId = FGuid(), bool bHasHammer = true)
	{
		FGolemEquipmentCombatBuild Build;
		Build.bEnabled = true;
		Build.bHasHammer = bHasHammer;
		Build.RelicEffectId = Family;
		Build.RelicInstanceId = ItemId;
		if (!Test->TestTrue(TEXT("Actual runtime prepares a valid equipment source"),
			Fixture.Runtime->PrepareEquipmentBuild(Build).IsSuccess()))
		{
			return false;
		}
		Fixture.Runtime->CommitPreparedEquipmentBuild();
		Fixture.Runtime->PublishPreparedBuild();
		return true;
	}

	bool InstallStats(FAutomationTestBase* Test, const FFixture& Fixture, int32 Preset)
	{
		FGolemGASProjectionPlan Plan;
		Plan.SourceKey = GolemProjection::RPGSourceKey;
		Plan.DefinitionId = FPrimaryAssetId(TEXT("GolemHero"), TEXT("Training_Red"));
		Plan.SkillBuild = Fixture.Runtime->GetBuild();
		Plan.EquipmentCombatBuild.bEnabled = true;
		Plan.EquipmentCombatBuild.bHasHammer = true;
		if (Preset == 0)
		{
			Plan.MaxHealthBonus = 140.0f;
			Plan.AttackPowerBonus = 10.0f;
			Plan.DefenseBonus = 36.0f;
		}
		else
		{
			Plan.MaxHealthBonus = Preset == 1 ? 380.0f : 310.0f;
			Plan.AttackPowerBonus = Preset == 1 ? 28.0f : Preset == 2 ? 32.0f : 36.0f;
			Plan.DefenseBonus = Preset == 1 ? 97.0f : 72.0f;
			Plan.MaxStaminaBonus = 10.0f;
			Plan.MaxRuneEnergyBonus = Preset == 2 ? 10.0f : 0.0f;
			Plan.CritChanceBonus = Preset == 3 ? 0.09f : 0.02f;
			Plan.CooldownRecoveryBonus = Preset == 1 ? 0.02f : 0.05f;
			Plan.MasteryBonus = Preset == 2 ? 0.09f : 0.04f;
			Plan.VersatilityBonus = Preset == 1 ? 0.12f : 0.07f;
			Plan.MaxPoiseBonus = 10.0f;
		}
		FGolemMutationFence Fence;
		FGolemProjectionCapability Capability;
		if (!Test->TestTrue(TEXT("Equipment owns an actual ASC transaction fence"),
			Fixture.ASC->BeginMutationFence(FGuid::NewGuid(), Fence).IsSuccess()))
		{
			return false;
		}
		const bool bApplied = Fixture.ASC->PrepareSourceProjection(Fence, Plan,
			Fixture.ASC->GetSourceProjection(Plan.SourceKey).Revision, Capability).IsSuccess()
			&& Fixture.ASC->StageSourceProjection(Fence, Capability).IsSuccess()
			&& Fixture.ASC->CommitSourceProjection(Fence, Capability).IsSuccess()
			&& Fixture.ASC->PublishSourceProjection(Fence, Capability).IsSuccess();
		if (!bApplied && Capability.IsValid())
		{
			Fixture.ASC->AbortSourceProjection(Fence, Capability);
		}
		Test->TestTrue(TEXT("Equipment aggregate commits and publishes through real source projection"), bApplied);
		Test->TestTrue(TEXT("Equipment transaction releases only its fence"), Fixture.ASC->EndMutationFence(Fence).IsSuccess());
		return bApplied;
	}

	bool UseConsumable(FAutomationTestBase* Test, const FFixture& Fixture, FName Effect)
	{
		FGolemMutationFence Fence;
		if (!Test->TestTrue(TEXT("Consumable holds a real ASC transaction fence"),
			Fixture.ASC->BeginMutationFence(FGuid::NewGuid(), Fence).IsSuccess()))
		{
			return false;
		}
		const bool bPrepared = Fixture.Runtime->PrepareConsumable(Effect, Fence).IsSuccess();
		Test->TestTrue(TEXT("Consumable prebuilds its actual native instant effect"), bPrepared);
		if (bPrepared)
		{
			Fixture.Runtime->CommitPreparedConsumable();
			Fixture.Runtime->PublishPreparedConsumable();
		}
		else
		{
			Fixture.Runtime->AbortPreparedConsumable();
		}
		Fixture.ASC->EndMutationFence(Fence);
		return bPrepared;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGolemEquipmentAggregationTest, "Golem.Equipment.Runtime.AggregationTwentyReplacements",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGolemEquipmentAggregationTest::RunTest(const FString& Parameters)
{
	using namespace GolemSpecializationTests;
	const TSharedRef<FWorld> Context = MakeShared<FWorld>();
	const FFixture Caster = Spawn(Context->World.Get(), FVector::ZeroVector, true, TEXT("Jupiter"), 100.0f, 100.0f);
	const FFixture Enemy = Spawn(Context->World.Get(), FVector(200, 0, 0), false, NAME_None, 100.0f, 100.0f);
	Caster.ASC->SetNumericAttributeBase(UGolemAttributeSet::GetCritChanceAttribute(), 0.10f);
	Caster.ASC->SetNumericAttributeBase(UGolemAttributeSet::GetHealthAttribute(), 700.0f);
	Caster.ASC->SetNumericAttributeBase(UGolemAttributeSet::GetStaminaAttribute(), 80.0f);
	Caster.ASC->SetNumericAttributeBase(UGolemAttributeSet::GetRuneEnergyAttribute(), 70.0f);
	const FGameplayAbilitySpecHandle Original = Caster.Runtime->GetSkillHandle(1);
	for (int32 Index = 0; Index < 20; ++Index)
	{
		const int32 Preset = Index % 4;
		if (!InstallStats(this, Caster, Preset))
		{
			return false;
		}
		CheckValue(this, TEXT("Final maximum HP contains one equipment source"), Caster.Attributes->GetMaxHealth(),
			Preset == 0 ? 1140.0f : Preset == 1 ? 1380.0f : 1310.0f);
		CheckValue(this, TEXT("Power aggregates weapon once"), Caster.Attributes->GetAttackPower(),
			Preset == 0 ? 110.0f : Preset == 1 ? 128.0f : Preset == 2 ? 132.0f : 136.0f);
		CheckValue(this, TEXT("Armor aggregates the complete preset"), Caster.Attributes->GetDefense(),
			Preset == 0 ? 136.0f : Preset == 1 ? 197.0f : 172.0f);
		CheckValue(this, TEXT("Secondary Crit uses percentage units"), Caster.Attributes->GetCritChance(),
			Preset == 0 ? 0.10f : Preset == 3 ? 0.19f : 0.12f);
		CheckValue(this, TEXT("CDR uses one source"), Caster.Attributes->GetCooldownRecovery(),
			Preset == 0 ? 0.0f : Preset == 1 ? 0.02f : 0.05f);
		CheckValue(this, TEXT("Mastery uses one source"), Caster.Attributes->GetMasteryBonus(),
			Preset == 0 ? 0.0f : Preset == 2 ? 0.09f : 0.04f);
		CheckValue(this, TEXT("Versatility uses one source"), Caster.Attributes->GetVersatility(),
			Preset == 0 ? 0.0f : Preset == 1 ? 0.12f : 0.07f);
		CheckValue(this, TEXT("Stamina maximum aggregates without recovery"), Caster.Attributes->GetMaxStamina(),
			Preset == 0 ? 100.0f : 110.0f);
		CheckValue(this, TEXT("Resource maximum aggregates without recovery"), Caster.Attributes->GetMaxRuneEnergy(),
			Preset == 2 ? 110.0f : 100.0f);
		CheckValue(this, TEXT("Poise bonus is additive to the combat-owned baseline"), Caster.Attributes->GetPoiseBonus(),
			Preset == 0 ? 0.0f : 10.0f);
		CheckValue(this, TEXT("Equip keeps absolute current HP"), Caster.Attributes->GetHealth(), 700.0f);
		CheckValue(this, TEXT("Equip never refills stamina"), Caster.Attributes->GetStamina(), 80.0f);
		CheckValue(this, TEXT("Equip never refills resource"), Caster.Attributes->GetRuneEnergy(), 70.0f);
		TestTrue(TEXT("Replacing gear retains the exact specialization grant"), Caster.Runtime->GetSkillHandle(1) == Original);
	}
	FGolemSkillDamagePacket Packet;
	Packet.RawDamage = 2.0f * Caster.Attributes->GetAttackPower() * (1.0f + Caster.Attributes->GetVersatility());
	Packet.bCanDefend = false;
	Packet.ActivationId = FGuid::NewGuid();
	TestTrue(TEXT("Strike coefficient packet runs actual GAS damage execution"),
		UGolemSkillRuntimeComponent::ApplySkillDamage(Caster.Actor, Enemy.Actor, Packet).IsSuccess());
	CheckValue(this, TEXT("Strike example applies 232.832 physical HP damage"), 1000.0f - Enemy.Attributes->GetHealth(), 232.832f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGolemRelicLifeseedTest, "Golem.Equipment.Runtime.LifeseedCadenceExactSourceCleanup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGolemRelicLifeseedTest::RunTest(const FString& Parameters)
{
	using namespace GolemSpecializationTests;
	const TSharedRef<FWorld> Context = MakeShared<FWorld>();
	const FFixture Caster = Spawn(Context->World.Get(), FVector::ZeroVector, true, TEXT("Mercury"));
	const FFixture Ally = Spawn(Context->World.Get(), FVector(100, 0, 0), true);
	const FFixture FullCaster = Spawn(Context->World.Get(), FVector(3000, 0, 0), true, TEXT("Mercury"));
	Caster.ASC->SetNumericAttributeBase(UGolemAttributeSet::GetVersatilityAttribute(), 0.20f);
	Caster.ASC->SetNumericAttributeBase(UGolemAttributeSet::GetMasteryBonusAttribute(), 0.30f);
	Caster.ASC->SetNumericAttributeBase(UGolemAttributeSet::GetCritChanceAttribute(), 0.40f);
	Ally.ASC->SetNumericAttributeBase(UGolemAttributeSet::GetHealthAttribute(), 100.0f);
	InstallEquipment(this, Caster, TEXT("Lifeseed"), FGuid::NewGuid());
	InstallEquipment(this, FullCaster, TEXT("Lifeseed"), FGuid::NewGuid());
	const TSharedRef<float> DirectHealth = MakeShared<float>(0.0f);
	const TSharedRef<FDelegateHandle> WithdrawalHandle = MakeShared<FDelegateHandle>();
	const TSharedRef<bool> bWithdrawn = MakeShared<bool>(false);
	RunPhases(this, Context,
	{
		{ 0.0f, [this, Caster, Ally, FullCaster]()
		{
			TestTrue(TEXT("Real healer GAS primary starts"), Caster.Runtime->RequestSkill(3, Ally.Actor).IsSuccess());
			TestTrue(TEXT("Real full-HP primary cast remains a legal ability"), FullCaster.Runtime->RequestSkill(3, FullCaster.Actor).IsSuccess());
		} },
		{ 0.9f, [this, Caster, Ally, FullCaster, DirectHealth]()
		{
			*DirectHealth = Ally.Attributes->GetHealth();
			CheckValue(this, TEXT("Mercury primary receives Versatility but no item tick at commit"), *DirectHealth, 316.0f);
			TestTrue(TEXT("Confirmed actual primary heal starts the ten-second family ICD"),
				Caster.Runtime->CaptureSnapshot().LifeseedRemaining > 9.0f);
			CheckValue(this, TEXT("Full-HP primary cannot farm a relic ICD or HoT"), FullCaster.Runtime->CaptureSnapshot().LifeseedRemaining, 0.0f);
			TestFalse(TEXT("Full-HP primary creates no item stream"), FullCaster.Runtime->HasUnsnapshottedEffects());
		} },
		{ 0.6f, [this, Ally, DirectHealth]()
		{
			CheckValue(this, TEXT("Lifeseed first tick occurs at plus one second with snapshot V once"), Ally.Attributes->GetHealth(), *DirectHealth + 18.0f);
		} },
		{ 1.0f, [this, Ally, DirectHealth]()
		{
			CheckValue(this, TEXT("Second item tick gains neither mastery nor crit"), Ally.Attributes->GetHealth(), *DirectHealth + 36.0f);
		} },
		{ 1.0f, [this, Caster, Ally, DirectHealth]()
		{
			CheckValue(this, TEXT("Exactly three item ticks complete"), Ally.Attributes->GetHealth(), *DirectHealth + 54.0f);
			TestEqual(TEXT("Item ticks never generate healer stacks"), Caster.Runtime->GetSkillState().Stacks, 0);
		} },
		{ 1.0f, [this, Caster, Ally, DirectHealth]()
		{
			CheckValue(this, TEXT("No fourth item tick exists"), Ally.Attributes->GetHealth(), *DirectHealth + 54.0f);
			const float ICD = Caster.Runtime->CaptureSnapshot().LifeseedRemaining;
			InstallEquipment(this, Caster);
			InstallEquipment(this, Caster, TEXT("Lifeseed"), FGuid::NewGuid());
			CheckValue(this, TEXT("Re-equip a different instance cannot reset family ICD"), Caster.Runtime->CaptureSnapshot().LifeseedRemaining, ICD);
		} },
		{ 8.5f, [this, Caster, Ally]()
		{
			TestTrue(TEXT("New source can start a real cast after family ICD"), Caster.Runtime->RequestSkill(3, Ally.Actor).IsSuccess());
		} },
		{ 0.9f, [this, Caster, Ally, DirectHealth, WithdrawalHandle, bWithdrawn]()
		{
			*DirectHealth = Ally.Attributes->GetHealth();
			*WithdrawalHandle = Ally.ASC->GetGameplayAttributeValueChangeDelegate(UGolemAttributeSet::GetHealthAttribute())
				.AddLambda([this, Caster, Ally, WithdrawalHandle, bWithdrawn](const FOnAttributeChangeData& Change)
				{
					if (!*bWithdrawn && Change.NewValue > Change.OldValue)
					{
						*bWithdrawn = true;
						Ally.ASC->GetGameplayAttributeValueChangeDelegate(UGolemAttributeSet::GetHealthAttribute()).Remove(*WithdrawalHandle);
						InstallEquipment(this, Caster);
					}
				});
		} },
		{ 3.3f, [this, Caster, Ally, DirectHealth, WithdrawalHandle, bWithdrawn]()
		{
			Ally.ASC->GetGameplayAttributeValueChangeDelegate(UGolemAttributeSet::GetHealthAttribute()).Remove(*WithdrawalHandle);
			TestTrue(TEXT("Actual item-heal callback can withdraw its exact source"), *bWithdrawn);
			CheckValue(this, TEXT("Generation guard cancels all later ticks after callback withdrawal"), Ally.Attributes->GetHealth(), *DirectHealth + 18.0f);
			TestTrue(TEXT("Withdrawal leaves no pending item stream"), !Caster.Runtime->HasUnsnapshottedEffects());
		} }
	});
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGolemRelicStarfallTest, "Golem.Equipment.Runtime.StarfallActivationSchoolSnapshot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGolemRelicStarfallTest::RunTest(const FString& Parameters)
{
	using namespace GolemSpecializationTests;
	const TSharedRef<FWorld> Context = MakeShared<FWorld>();
	const FFixture Caster = Spawn(Context->World.Get(), FVector::ZeroVector, true, TEXT("Mars"));
	const FFixture Enemy = Spawn(Context->World.Get(), FVector(200, 0, 0), false, NAME_None, 100.0f, 100.0f);
	const FFixture Other = Spawn(Context->World.Get(), FVector(200, 100, 0), false);
	const FFixture Restored = Spawn(Context->World.Get(), FVector(0, 100, 0), true, TEXT("Mars"));
	const FFixture ShieldTarget = Spawn(Context->World.Get(), FVector(400, 0, 0), false, TEXT("Uranus"));
	Caster.ASC->SetNumericAttributeBase(UGolemAttributeSet::GetVersatilityAttribute(), 0.20f);
	Caster.ASC->SetNumericAttributeBase(UGolemAttributeSet::GetMasteryBonusAttribute(), 0.30f);
	Caster.ASC->SetNumericAttributeBase(UGolemAttributeSet::GetCritChanceAttribute(), 0.40f);
	Restored.ASC->SetNumericAttributeBase(UGolemAttributeSet::GetVersatilityAttribute(), 0.20f);
	InstallEquipment(this, Caster, TEXT("Starfall"), FGuid::NewGuid());
	InstallEquipment(this, Restored, TEXT("Starfall"), FGuid::NewGuid());
	FGolemSkillDamagePacket Packet;
	Packet.RawDamage = 20.0f;
	Packet.bEligibleItemActivation = true;
	Packet.bCanDefend = false;
	Packet.PrimaryTarget = Enemy.Actor;
	Packet.ActivationId = FGuid::NewGuid();
	UGolemSkillRuntimeComponent::ApplySkillDamage(Caster.Actor, Enemy.Actor, Packet);
	Packet.HitIndex = 1;
	UGolemSkillRuntimeComponent::ApplySkillDamage(Caster.Actor, Other.Actor, Packet);
	TestEqual(TEXT("AoE child shares one activation counter"), Caster.Runtime->GetSkillState().StarfallCount, 1);
	TestTrue(TEXT("Snapshot stores a pending item counter with its remaining ten-second window"),
		Restored.Runtime->RestoreSnapshot(Caster.Runtime->CaptureSnapshot()).IsSuccess());
	TestEqual(TEXT("Reconstruction preserves the pending counter"), Restored.Runtime->GetSkillState().StarfallCount, 1);
	Packet.ActivationId = FGuid::NewGuid();
	Packet.bDirect = false;
	UGolemSkillRuntimeComponent::ApplySkillDamage(Caster.Actor, Enemy.Actor, Packet);
	TestEqual(TEXT("Periodic or secondary packet cannot increment counter"), Caster.Runtime->GetSkillState().StarfallCount, 1);
	Packet.bDirect = true;
	Packet.ActivationId = FGuid::NewGuid();
	UGolemSkillRuntimeComponent::ApplySkillDamage(Caster.Actor, Enemy.Actor, Packet);
	const float Before = Enemy.Attributes->GetHealth();
	Packet.ActivationId = FGuid::NewGuid();
	Packet.bPrimaryTarget = false;
	UGolemSkillRuntimeComponent::ApplySkillDamage(Caster.Actor, Other.Actor, Packet);
	CheckValue(this, TEXT("Positive child can trigger Starfall onto the explicitly selected primary"),
		Before - Enemy.Attributes->GetHealth(), 38.4f);
	TestEqual(TEXT("Third activation consumes the counter without secondary recursion"), Caster.Runtime->GetSkillState().StarfallCount, 0);
	TestEqual(TEXT("Relic proc does not generate Mars Heat"), Caster.Runtime->GetSkillState().Stacks, 0);
	const FGolemSkillRuntimeSnapshot Saved = Caster.Runtime->CaptureSnapshot();
	CheckValue(this, TEXT("Starfall stores its eight-second family ICD"), Saved.StarfallRemaining, 8.0f);
	TestTrue(TEXT("Snapshot reconstructs the exact remaining ICD"), Restored.Runtime->RestoreSnapshot(Saved).IsSuccess());
	Packet.ActivationId = FGuid::NewGuid();
	UGolemSkillRuntimeComponent::ApplySkillDamage(Restored.Actor, Enemy.Actor, Packet);
	TestEqual(TEXT("Reconstruction cannot count an activation during ICD"), Restored.Runtime->GetSkillState().StarfallCount, 0);
	InstallEquipment(this, Caster);
	InstallEquipment(this, Caster, TEXT("Starfall"), FGuid::NewGuid());
	CheckValue(this, TEXT("Source replacement preserves owner-family ICD"), Caster.Runtime->CaptureSnapshot().StarfallRemaining, 8.0f);
	RunPhases(this, Context,
	{
		{ 8.1f, [this, Caster, Enemy, ShieldTarget]()
		{
			TestTrue(TEXT("Actual target tank A3 begins a real absorption pool"), ShieldTarget.Runtime->RequestSkill(3, ShieldTarget.Actor).IsSuccess());
			FGolemSkillDamagePacket Frost;
			Frost.RawDamage = 20.0f;
			Frost.School = EGolemSkillSchool::Frost;
			Frost.bEligibleItemActivation = true;
			Frost.bCanDefend = false;
			Frost.PrimaryTarget = Enemy.Actor;
			const float Start = Enemy.Attributes->GetHealth();
			for (int32 Index = 0; Index < 3; ++Index)
			{
				Frost.ActivationId = FGuid::NewGuid();
				UGolemSkillRuntimeComponent::ApplySkillDamage(Caster.Actor, Enemy.Actor, Frost);
			}
			CheckValue(this, TEXT("Item proc keeps magical school and ignores physical Armor"), Start - Enemy.Attributes->GetHealth(), 108.0f);
		} },
		{ 0.8f, [this, Restored, ShieldTarget]()
		{
			CheckValue(this, TEXT("Actual target shield exists before direct packets"), ShieldTarget.Runtime->GetSkillState().Shield, 200.0f);
			FGolemSkillDamagePacket Hit;
			Hit.RawDamage = 20.0f;
			Hit.School = EGolemSkillSchool::Frost;
			Hit.bEligibleItemActivation = true;
			Hit.bCanDefend = false;
			Hit.PrimaryTarget = ShieldTarget.Actor;
			for (int32 Index = 0; Index < 3; ++Index)
			{
				Hit.ActivationId = FGuid::NewGuid();
				UGolemSkillRuntimeComponent::ApplySkillDamage(Restored.Actor, ShieldTarget.Actor, Hit);
			}
			CheckValue(this, TEXT("Post-mitigation positive packets count before shields absorb all HP damage"), ShieldTarget.Runtime->GetSkillState().Shield, 92.0f);
			CheckValue(this, TEXT("Shielded primary retains full health while third activation really procs"), ShieldTarget.Attributes->GetHealth(), 1000.0f);
			TestEqual(TEXT("Shield absorption cannot suppress eligible activation counting"), Restored.Runtime->GetSkillState().StarfallCount, 0);
			TestTrue(TEXT("Shielded third activation starts item ICD"), Restored.Runtime->CaptureSnapshot().StarfallRemaining > 7.9f);
		} },
		{ 8.1f, [this, Caster, Enemy]()
		{
			FGolemSkillDamagePacket Hit;
			Hit.RawDamage = 20.0f;
			Hit.bEligibleItemActivation = true;
			Hit.bCanDefend = false;
			Hit.ActivationId = FGuid::NewGuid();
			UGolemSkillRuntimeComponent::ApplySkillDamage(Caster.Actor, Enemy.Actor, Hit);
			TestEqual(TEXT("First activation after ICD starts a fresh counter"), Caster.Runtime->GetSkillState().StarfallCount, 1);
		} },
		{ 10.2f, [this, Caster]()
		{
			TestEqual(TEXT("Uncompleted counter expires after ten seconds"), Caster.Runtime->GetSkillState().StarfallCount, 0);
		} }
	});
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGolemConsumableRuntimeTest, "Golem.Equipment.Runtime.ConsumableAtomicSharedCooldown",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGolemConsumableRuntimeTest::RunTest(const FString& Parameters)
{
	using namespace GolemSpecializationTests;
	const TSharedRef<FWorld> Context = MakeShared<FWorld>();
	const FFixture Caster = Spawn(Context->World.Get(), FVector::ZeroVector, true, TEXT("Venus"));
	const FFixture Restored = Spawn(Context->World.Get(), FVector(0, 100, 0), true, TEXT("Venus"));
	InstallEquipment(this, Caster);
	InstallEquipment(this, Restored);
	Caster.ASC->SetNumericAttributeBase(UGolemAttributeSet::GetHealthAttribute(), 800.0f);
	Caster.ASC->SetNumericAttributeBase(UGolemAttributeSet::GetVersatilityAttribute(), 0.20f);
	Caster.ASC->SetNumericAttributeBase(UGolemAttributeSet::GetMasteryBonusAttribute(), 0.30f);
	FGolemSkillRuntimeSnapshot Fragment = Caster.Runtime->CaptureSnapshot();
	Fragment.Resource = 50.0f;
	Fragment.Stacks = 2;
	Fragment.StackRemaining = 15.0f;
	Fragment.BuffRemaining = 10.0f;
	Fragment.CooldownRemaining[3] = 60.0f;
	Fragment.ResourceRegenDelayRemaining = 0.8f;
	Caster.Runtime->RestoreSnapshot(Fragment);
	FGolemMutationFence Fence;
	TestTrue(TEXT("Actual transaction owns mutation fence"), Caster.ASC->BeginMutationFence(FGuid::NewGuid(), Fence).IsSuccess());
	TestTrue(TEXT("Health potion native effect prepares under owning fence"), Caster.Runtime->PrepareConsumable(TEXT("Health"), Fence).IsSuccess());
	Caster.Runtime->CommitPreparedConsumable();
	CheckValue(this, TEXT("Logical CD commit remains callback-free before effect publication"), Caster.Attributes->GetHealth(), 800.0f);
	CheckValue(this, TEXT("Logical commit starts the shared thirty-second CD"), Caster.Runtime->CaptureSnapshot().ConsumableRemaining, 30.0f);
	Caster.Runtime->PublishPreparedConsumable();
	Caster.ASC->EndMutationFence(Fence);
	CheckValue(this, TEXT("Health potion heals exactly twenty percent MaxHP"), Caster.Attributes->GetHealth(), 1000.0f);
	CheckValue(this, TEXT("Potion does not trigger Venus shield"), Caster.Runtime->GetSkillState().Shield, 0.0f);
	TestEqual(TEXT("Potion does not change healer stacks"), Caster.Runtime->GetSkillState().Stacks, 2);
	CheckValue(this, TEXT("Potion cannot reset A4 cooldown"), Caster.Runtime->CaptureSnapshot().CooldownRemaining[3], 60.0f);
	CheckValue(this, TEXT("Potion does not reset resource regeneration pause"), Caster.Runtime->CaptureSnapshot().ResourceRegenDelayRemaining, 0.8f);
	TestFalse(TEXT("Resource potion shares the same Golem cooldown"), Caster.Runtime->ValidateConsumable(TEXT("Resource")).IsSuccess());
	const FGolemSkillRuntimeSnapshot Saved = Caster.Runtime->CaptureSnapshot();
	TestTrue(TEXT("Runtime reconstruction accepts the complete consumable fragment"), Restored.Runtime->RestoreSnapshot(Saved).IsSuccess());
	TestFalse(TEXT("Reconstruction cannot reset the potion cooldown"), Restored.Runtime->ValidateConsumable(TEXT("Resource")).IsSuccess());
	RunPhases(this, Context,
	{
		{ 30.1f, [this, Caster]()
		{
			TestFalse(TEXT("Full HP rejects health potion without starting another cooldown"), Caster.Runtime->ValidateConsumable(TEXT("Health")).IsSuccess());
			Caster.ASC->SetNumericAttributeBase(UGolemAttributeSet::GetRuneEnergyAttribute(), 60.0f);
			Caster.ASC->SetLooseGameplayTagCount(GolemGameplayTags::State_Combat_Staggered, 1);
			TestFalse(TEXT("Hard stun rejects potion"), Caster.Runtime->ValidateConsumable(TEXT("Resource")).IsSuccess());
			Caster.ASC->SetLooseGameplayTagCount(GolemGameplayTags::State_Combat_Staggered, 0);
			const float MajorCD = Caster.Runtime->CaptureSnapshot().CooldownRemaining[3];
			UseConsumable(this, Caster, TEXT("Resource"));
			CheckValue(this, TEXT("Resource potion restores exactly thirty resource"), Caster.Attributes->GetRuneEnergy(), 90.0f);
			CheckValue(this, TEXT("Resource potion preserves existing major cooldown"), Caster.Runtime->CaptureSnapshot().CooldownRemaining[3], MajorCD);
			Caster.Runtime->ResetForCheckpoint();
			CheckValue(this, TEXT("Checkpoint retry preserves the shared consumable cooldown"), Caster.Runtime->CaptureSnapshot().ConsumableRemaining, 30.0f);
		} }
	});
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGolemConsumableApplicationApprovalTest,
	"Golem.Equipment.Runtime.ConsumableApplicationApproval",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGolemConsumableApplicationApprovalTest::RunTest(const FString& Parameters)
{
	using namespace GolemSpecializationTests;
	const TSharedRef<FWorld> Context = MakeShared<FWorld>();
	const FFixture Caster = Spawn(Context->World.Get(), FVector::ZeroVector, true, TEXT("Venus"));
	InstallEquipment(this, Caster);
	Caster.ASC->SetNumericAttributeBase(UGolemAttributeSet::GetHealthAttribute(), 700.0f);
	FGolemSkillRuntimeSnapshot Initial = Caster.Runtime->CaptureSnapshot();
	Initial.Resource = 50.0f;
	Initial.Stacks = 2;
	Initial.StackRemaining = 15.0f;
	Initial.ResourceRegenDelayRemaining = 0.8f;
	Initial.CooldownRemaining[3] = 60.0f;
	Caster.Runtime->RestoreSnapshot(Initial);
	int32 Publications = 0;
	const FDelegateHandle StateHandle = Caster.Runtime->OnSkillStateChanged.AddLambda(
		[&Publications](const FGolemSkillRuntimeState&)
		{
			++Publications;
		});
	int32 Applications = 0;
	const FDelegateHandle AppliedHandle = Caster.ASC->OnGameplayEffectAppliedDelegateToSelf.AddLambda(
		[&Applications, Caster](UAbilitySystemComponent*, const FGameplayEffectSpec& Spec, FActiveGameplayEffectHandle)
		{
			if (Spec.Def == GetDefault<UGolemConsumableEffect>())
			{
				++Applications;
				// The staged spec is retired before callbacks, so a reentrant publication cannot execute twice.
				Caster.Runtime->PublishPreparedConsumable();
			}
		});
	int32 Rejections = 0;
	FGameplayEffectApplicationQuery Reject = FGameplayEffectApplicationQuery::CreateLambda(
		[&Rejections](const FActiveGameplayEffectsContainer&, const FGameplayEffectSpec& Spec)
		{
			if (Spec.Def == GetDefault<UGolemConsumableEffect>())
			{
				++Rejections;
				return false;
			}
			return true;
		});
	const FDelegateHandle RejectHandle = Reject.GetHandle();
	Caster.ASC->GameplayEffectApplicationQueries.Add(MoveTemp(Reject));
	FGolemMutationFence Fence;
	TestTrue(TEXT("Veto test holds the actual owning transaction fence"),
		Caster.ASC->BeginMutationFence(FGuid::NewGuid(), Fence).IsSuccess());
	TestEqual(TEXT("Real Health GE veto rejects preflight"),
		Caster.Runtime->PrepareConsumable(TEXT("Health"), Fence).Error, EGolemError::InvalidState);
	TestEqual(TEXT("Real Resource GE veto rejects preflight"),
		Caster.Runtime->PrepareConsumable(TEXT("Resource"), Fence).Error, EGolemError::InvalidState);
	TestEqual(TEXT("Both native recovery specs reached the real registered query"), Rejections, 2);
	Caster.Runtime->CommitPreparedConsumable();
	Caster.Runtime->PublishPreparedConsumable();
	Caster.Runtime->AbortPreparedConsumable();
	CheckValue(this, TEXT("Rejected recovery leaves actual HP unchanged"), Caster.Attributes->GetHealth(), 700.0f);
	CheckValue(this, TEXT("Rejected recovery leaves actual class resource unchanged"), Caster.Attributes->GetRuneEnergy(), 50.0f);
	CheckValue(this, TEXT("Rejected recovery starts no shared cooldown"), Caster.Runtime->CaptureSnapshot().ConsumableRemaining, 0.0f);
	CheckValue(this, TEXT("Rejected recovery retains existing major cooldown"), Caster.Runtime->CaptureSnapshot().CooldownRemaining[3], 60.0f);
	CheckValue(this, TEXT("Rejected recovery retains regeneration pause"), Caster.Runtime->CaptureSnapshot().ResourceRegenDelayRemaining, 0.8f);
	TestEqual(TEXT("Rejected spec causes no actual GAS applied callback"), Applications, 0);
	TestEqual(TEXT("Rejected spec causes no confirmed runtime publication"), Publications, 0);
	TestTrue(TEXT("Vetoed transaction releases its own fence"), Caster.ASC->EndMutationFence(Fence).IsSuccess());
	Caster.ASC->GameplayEffectApplicationQueries.RemoveAll([RejectHandle](const FGameplayEffectApplicationQuery& Query)
	{
		return Query.GetHandle() == RejectHandle;
	});

	int32 Approvals = 0;
	FGameplayEffectApplicationQuery ApproveOnce = FGameplayEffectApplicationQuery::CreateLambda(
		[&Approvals](const FActiveGameplayEffectsContainer&, const FGameplayEffectSpec& Spec)
		{
			return Spec.Def != GetDefault<UGolemConsumableEffect>() || ++Approvals == 1;
		});
	const FDelegateHandle ApprovalHandle = ApproveOnce.GetHandle();
	Caster.ASC->GameplayEffectApplicationQueries.Add(MoveTemp(ApproveOnce));
	TestTrue(TEXT("Fresh recovery obtains a new owning fence"),
		Caster.ASC->BeginMutationFence(FGuid::NewGuid(), Fence).IsSuccess());
	TestTrue(TEXT("Stateful query approves the exact native instant spec before commit"),
		Caster.Runtime->PrepareConsumable(TEXT("Health"), Fence).IsSuccess());
	TestEqual(TEXT("Approval runs once during preflight"), Approvals, 1);
	CheckValue(this, TEXT("Preflight cannot recover HP"), Caster.Attributes->GetHealth(), 700.0f);
	Caster.Runtime->CommitPreparedConsumable();
	CheckValue(this, TEXT("Logical commit preserves HP until confirmed publication"), Caster.Attributes->GetHealth(), 700.0f);
	TestEqual(TEXT("Logical commit invokes no runtime subscribers"), Publications, 0);
	Caster.Runtime->PublishPreparedConsumable();
	Caster.Runtime->PublishPreparedConsumable();
	TestEqual(TEXT("Committed execution does not rerun the stateful approval query"), Approvals, 1);
	CheckValue(this, TEXT("Approved native GE actually heals twenty percent MaxHP once"), Caster.Attributes->GetHealth(), 900.0f);
	CheckValue(this, TEXT("Approved native GE retains class resource"), Caster.Attributes->GetRuneEnergy(), 50.0f);
	CheckValue(this, TEXT("Approved native GE retains the shared thirty-second cooldown"), Caster.Runtime->CaptureSnapshot().ConsumableRemaining, 30.0f);
	CheckValue(this, TEXT("Approved potion creates no healer shield proc"), Caster.Runtime->GetSkillState().Shield, 0.0f);
	TestEqual(TEXT("Approved potion retains healer stacks"), Caster.Runtime->GetSkillState().Stacks, 2);
	TestEqual(TEXT("Actual native execution emits one standard GAS applied callback"), Applications, 1);
	TestEqual(TEXT("Actual native execution emits one confirmed runtime publication"), Publications, 1);
	TestTrue(TEXT("Approved recovery releases its exact transaction fence"), Caster.ASC->EndMutationFence(Fence).IsSuccess());
	Caster.ASC->GameplayEffectApplicationQueries.RemoveAll([ApprovalHandle](const FGameplayEffectApplicationQuery& Query)
	{
		return Query.GetHandle() == ApprovalHandle;
	});
	Caster.ASC->OnGameplayEffectAppliedDelegateToSelf.Remove(AppliedHandle);
	Caster.Runtime->OnSkillStateChanged.Remove(StateHandle);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGolemNineEquippedSpecializationsTest, "Golem.Equipment.Runtime.NineSpecializationsAndHammerRequirement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGolemNineEquippedSpecializationsTest::RunTest(const FString& Parameters)
{
	using namespace GolemSpecializationTests;
	const TSharedRef<FWorld> Context = MakeShared<FWorld>();
	TArray<FFixture> Casters;
	TArray<FFixture> Targets;
	for (FName Spec : FGolemSkillCatalog::AllSpecs())
	{
		const FVector Origin(Casters.Num() * 3000.0f, 0, 0);
		const FFixture Caster = Spawn(Context->World.Get(), Origin, true, Spec, 100.0f, 100.0f);
		const bool bHealer = FGolemSkillCatalog::Role(Spec) == EGolemSkillRole::Healer;
		const FFixture Target = Spawn(Context->World.Get(), Origin + FVector(200, 0, 0), false);
		Target.ASC->SetNumericAttributeBase(UGolemAttributeSet::GetHealthAttribute(), 100.0f);
		Caster.ASC->SetNumericAttributeBase(UGolemAttributeSet::GetCritChanceAttribute(), 0.10f);
		InstallStats(this, Caster, bHealer ? 2 : FGolemSkillCatalog::Role(Spec) == EGolemSkillRole::Tank ? 1 : 3);
		TestTrue(FString::Printf(TEXT("%s uses its real equipped GAS A1"), *Spec.ToString()), Caster.Runtime->RequestSkill(1, Target.Actor).IsSuccess());
		Casters.Add(Caster);
		Targets.Add(Target);
	}
	RunPhases(this, Context,
	{
		{ 1.4f, [this, Casters, Targets]()
		{
			for (int32 Index = 0; Index < Casters.Num(); ++Index)
			{
				const FFixture Caster = Casters[Index];
				const FGolemSpecActionDefinition Action = FGolemSkillCatalog::GetSkill(Caster.Runtime->GetBuild(), 1);
				TestTrue(FString::Printf(TEXT("%s equipped A1 actually damages its selected enemy"), *Action.SpecId.ToString()),
					Targets[Index].Attributes->GetHealth() < 100.0f);
				InstallEquipment(this, Caster, NAME_None, FGuid(), false);
				const float Resource = Caster.Attributes->GetRuneEnergy();
				const FGolemSkillRuntimeSnapshot Before = Caster.Runtime->CaptureSnapshot();
				for (int32 Slot = 1; Slot <= 4; ++Slot)
				{
					TestEqual(TEXT("Empty weapon rejects every real specialization action with RequiresHammer"),
						Caster.Runtime->ValidateSkill(Slot, Targets[Index].Actor).Context, FName(TEXT("RequiresHammer")));
					TestFalse(TEXT("Empty weapon cannot spend through GAS activation"), Caster.Runtime->RequestSkill(Slot, Targets[Index].Actor).IsSuccess());
				}
				CheckValue(this, TEXT("Rejected unarmed activation spends no resource"), Caster.Attributes->GetRuneEnergy(), Resource);
				TestTrue(TEXT("Rejected unarmed activation preserves all cooldowns"),
					Caster.Runtime->CaptureSnapshot().CooldownRemaining == Before.CooldownRemaining);
				FGolemEquipmentCombatBuild Legacy;
				TestTrue(TEXT("Legacy M5.5 build opt-out remains available"), Caster.Runtime->PrepareEquipmentBuild(Legacy).IsSuccess());
				Caster.Runtime->CommitPreparedEquipmentBuild();
				TestFalse(TEXT("Legacy M5.5 fixtures retain existing hammer behavior"), Caster.Runtime->RequiresHammer());
			}
		} }
	});
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGolemEquipmentShieldRecapTest, "Golem.Equipment.Runtime.MaxHealthReclampsExistingShield",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGolemEquipmentShieldRecapTest::RunTest(const FString& Parameters)
{
	using namespace GolemSpecializationTests;
	const TSharedRef<FWorld> Context = MakeShared<FWorld>();
	const FFixture Caster = Spawn(Context->World.Get(), FVector::ZeroVector, true, TEXT("Uranus"), 1000.0f);
	InstallEquipment(this, Caster);
	RunPhases(this, Context,
	{
		{ 0.0f, [this, Caster]()
		{
			TestTrue(TEXT("Actual tank A3 creates its owned shield"), Caster.Runtime->RequestSkill(3, Caster.Actor).IsSuccess());
		} },
		{ 0.8f, [this, Caster]()
		{
			CheckValue(this, TEXT("Large real shield obeys the thirty-five percent cap"), Caster.Runtime->GetSkillState().Shield, 350.0f);
			Caster.ASC->SetNumericAttributeBase(UGolemAttributeSet::GetMaxHealthAttribute(), 100.0f);
			CheckValue(this, TEXT("MaxHP change immediately reclamps an existing shield without a timer tick"), Caster.Runtime->GetSkillState().Shield, 35.0f);
			CheckValue(this, TEXT("Decreasing MaxHP clamps absolute health"), Caster.Attributes->GetHealth(), 100.0f);
			Caster.ASC->SetNumericAttributeBase(UGolemAttributeSet::GetMaxHealthAttribute(), 1000.0f);
			CheckValue(this, TEXT("Increasing MaxHP never recreates discarded shield capacity"), Caster.Runtime->GetSkillState().Shield, 35.0f);
			CheckValue(this, TEXT("Increasing MaxHP never heals current health"), Caster.Attributes->GetHealth(), 100.0f);
		} }
	});
	return true;
}

#endif
