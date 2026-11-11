#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Assets/GolemAssetManager.h"
#include "Attributes/GolemAttributeSet.h"
#include "Character/GolemCharacter.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Components/GolemAbilitySystemComponent.h"
#include "Components/GolemCombatComponent.h"
#include "Components/GolemLifeComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "CoreGlobals.h"
#include "Definitions/GolemHeroDefinition.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "HAL/PlatformTime.h"
#include "PartyArena/GolemArenaEnemy.h"
#include "RPGLoop/GolemRPGCharacter.h"
#include "SkillLab/GolemBossArmorComponent.h"
#include "SkillLab/GolemFrozenSurface.h"
#include "SkillLab/GolemRunestone.h"
#include "SkillLab/GolemRunestonePickup.h"
#include "Skills/GolemSkillRuntimeComponent.h"
#include "Skills/GolemSkillTypes.h"
#include "UObject/StrongObjectPtr.h"

namespace GolemSkillLabWorldTests
{
	class FGolemWorld final : public FNoncopyable
	{
	public:
		FGolemWorld()
			: Instance(NewObject<UGameInstance>(GEngine))
		{
			Instance->InitializeStandalone();
			World = Instance->GetWorld();
			World->SetShouldTick(false);
			World->GetWorldSettings()->DefaultGameMode = AGameModeBase::StaticClass();
			FURL URL;
			URL.Map = TEXT("GolemSkillLabWorldTest");
			URL.AddOption(TEXT("game=/Script/Engine.GameModeBase"));
			World->SetGameMode(URL);
			World->InitializeActorsForPlay(URL);
			World->BeginPlay();
		}
		~FGolemWorld()
		{
			if (World.IsValid() && World->HasBegunPlay())
			{
				World->EndPlay(EEndPlayReason::Quit);
			}
			UWorld* OwnedWorld = World.Get();
			Instance->Shutdown();
			if (OwnedWorld != nullptr)
			{
				OwnedWorld->DestroyWorld(false);
				GEngine->DestroyWorldContext(OwnedWorld);
			}
		}
		bool TickOnce()
		{
			if (!World.IsValid() || LastTickedFrame == GFrameCounter)
			{
				return false;
			}
			LastTickedFrame = GFrameCounter;
			World->Tick(LEVELTICK_All, 0.05f);
			return true;
		}
		AGolemRPGCharacter* SpawnGolem(const FVector& Location, FName HeroName = TEXT("Training_Red"))
		{
			UGolemDefinition* Definition = nullptr;
			UGolemAssetManager* Manager = UGolemAssetManager::GetGolemAssetManager();
			if (Manager == nullptr || !Manager->ResolveDefinition(
				FPrimaryAssetId(TEXT("GolemHero"), HeroName), UGolemHeroDefinition::StaticClass(), Definition).IsSuccess()
				|| Cast<UGolemHeroDefinition>(Definition) == nullptr)
			{
				return nullptr;
			}
			FActorSpawnParameters Parameters;
			Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			AGolemRPGCharacter* Golem = World->SpawnActor<AGolemRPGCharacter>(
				Location, FRotator::ZeroRotator, Parameters);
			if (Golem == nullptr || !Golem->InitializeGolem(FGolemEntityId::Create(),
				Cast<UGolemHeroDefinition>(Definition)).IsSuccess())
			{
				return nullptr;
			}
			APlayerController* Controller = World->SpawnActor<APlayerController>();
			if (Controller == nullptr)
			{
				return nullptr;
			}
			Controller->Possess(Golem);
			// This deliberately floorless fixture tests timers and explicit range changes, without gravity drift.
			Golem->GetCharacterMovement()->DisableMovement();
			UGolemAbilitySystemComponent* ASC = Golem->GetGolemAbilitySystem();
			ASC->SetNumericAttributeBase(UGolemAttributeSet::GetMaxHealthAttribute(), 1000.0f);
			ASC->SetNumericAttributeBase(UGolemAttributeSet::GetHealthAttribute(), 1000.0f);
			ASC->SetNumericAttributeBase(UGolemAttributeSet::GetAttackPowerAttribute(), 100.0f);
			ASC->SetNumericAttributeBase(UGolemAttributeSet::GetDefenseAttribute(), 100.0f);
			return Golem;
		}
		TWeakObjectPtr<UWorld> World;
	private:
		TStrongObjectPtr<UGameInstance> Instance;
		uint64 LastTickedFrame = MAX_uint64;
	};

	class FGolemWaitForWorldTime final : public IAutomationLatentCommand
	{
	public:
		FGolemWaitForWorldTime(FAutomationTestBase* InTest, const TSharedRef<FGolemWorld>& InContext,
			float Seconds, TFunction<void()> InCompletion)
			: Test(InTest), Context(InContext), Completion(MoveTemp(InCompletion)),
			TargetTime(InContext->World->GetTimeSeconds() + Seconds), StartedAt(FPlatformTime::Seconds())
		{
		}
		virtual bool Update() override
		{
			if (!Context->World.IsValid() || !Context->World->HasBegunPlay()
				|| FPlatformTime::Seconds() - StartedAt > 60.0)
			{
				Test->AddError(TEXT("SkillLab world fixture lost its world or exceeded its bounded timer wait."));
				return true;
			}
			// TimerManager accepts one tick per real engine frame. Never change the global frame counter.
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
		TSharedRef<FGolemWorld> Context;
		TFunction<void()> Completion;
		double TargetTime;
		double StartedAt;
	};

	void WaitForWorldTime(FAutomationTestBase* Test, const TSharedRef<FGolemWorld>& Context,
		float Seconds, TFunction<void()> Completion)
	{
		FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FGolemWaitForWorldTime>(
			Test, Context, Seconds, MoveTemp(Completion)));
	}

	UGolemBossArmorComponent* SpawnArmor(UWorld* World, const FVector& Location = FVector::ZeroVector)
	{
		AActor* Owner = World->SpawnActor<AActor>(Location, FRotator::ZeroRotator);
		if (Owner == nullptr)
		{
			return nullptr;
		}
		USceneComponent* Root = NewObject<USceneComponent>(Owner);
		Owner->AddInstanceComponent(Root);
		Owner->SetRootComponent(Root);
		Root->RegisterComponent();
		Root->SetWorldLocation(Location);
		UGolemBossArmorComponent* Armor = NewObject<UGolemBossArmorComponent>(Owner);
		Owner->AddInstanceComponent(Armor);
		Armor->RegisterComponent();
		Armor->EnableArmorZone(FGuid::NewGuid());
		return Armor;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGolemGearItemProcArmorTest, "Golem.M56.World.ItemProcDoesNotReactWithArmor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGolemGearItemProcArmorTest::RunTest(const FString& Parameters)
{
	using namespace GolemSkillLabWorldTests;
	TSharedRef<FGolemWorld> Fixture = MakeShared<FGolemWorld>();
	AGolemRPGCharacter* Source = Fixture->SpawnGolem(FVector(-400.0, 0.0, 0.0));
	AGolemRPGCharacter* Target = Fixture->SpawnGolem(FVector::ZeroVector, TEXT("Training_Blue"));
	if (!TestNotNull(TEXT("Actual item proc source"), Source) || !TestNotNull(TEXT("Actual armored target"), Target))
	{
		return false;
	}
	TestTrue(TEXT("Actual target combat profile initializes"), Target->InitializeArenaCombat().IsSuccess());
	FGolemSkillBuild SourceBuild;
	SourceBuild.SpecId = TEXT("Uranus");
	FGolemSkillBuild TargetBuild;
	TargetBuild.SpecId = TEXT("Pluto");
	TestTrue(TEXT("Source shared GAS build"), Source->FindComponentByClass<UGolemSkillRuntimeComponent>()->ConfigureBuild(SourceBuild).IsSuccess());
	TestTrue(TEXT("Target shared GAS build"), Target->FindComponentByClass<UGolemSkillRuntimeComponent>()->ConfigureBuild(TargetBuild).IsSuccess());
	UGolemBossArmorComponent* Armor = NewObject<UGolemBossArmorComponent>(Target);
	Target->AddInstanceComponent(Armor);
	Armor->RegisterComponent();
	Armor->EnableArmorZone(FGuid::NewGuid());
	const float HealthBefore = Target->GetGolemAttributeSet()->GetHealth();
	const float DurabilityBefore = Armor->GetDurability();
	const float PoiseBefore = Target->GetCombatComponent()->GetSnapshot().Poise;
	TestTrue(TEXT("Nonzero combat poise makes the reaction assertion meaningful"), PoiseBefore > 0.0f);
	FHitResult Hit(Target, Armor->GetArmorZone(), Target->GetActorLocation(), FVector::ForwardVector);
	FGolemSkillDamagePacket Packet;
	Packet.RawDamage = 100.0f;
	Packet.PoiseDamage = 50.0f;
	Packet.School = EGolemSkillSchool::Arcane;
	Packet.bDirect = false;
	Packet.bItemProc = true;
	Packet.bHeavy = true;
	Packet.ActivationId = FGuid::NewGuid();
	TestTrue(TEXT("Secondary packet traverses real GAS damage execution"),
		UGolemSkillRuntimeComponent::ApplySkillDamage(Source, Target, Packet, &Hit).IsSuccess());
	TestTrue(TEXT("Item proc damages HP with normal magical defenses rather than armor-zone scalar"),
		FMath::IsNearlyEqual(HealthBefore - Target->GetGolemAttributeSet()->GetHealth(), 100.0f));
	TestEqual(TEXT("Item proc cannot break durability"), Armor->GetDurability(), DurabilityBefore);
	TestEqual(TEXT("Item proc cannot stagger via poise"), Target->GetCombatComponent()->GetSnapshot().Poise, PoiseBefore);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGolemSkillLabArmorTest, "Golem.M55.World.ArmorDurabilityAndDedup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGolemSkillLabArmorTest::RunTest(const FString& Parameters)
{
	using namespace GolemSkillLabWorldTests;
	TSharedRef<FGolemWorld> Fixture = MakeShared<FGolemWorld>();
	UGolemBossArmorComponent* Armor = SpawnArmor(Fixture->World.Get());
	if (!TestNotNull(TEXT("Registered armor fixture"), Armor)
		|| !TestNotNull(TEXT("Real armor primitive"), Armor->GetArmorZone()))
	{
		return false;
	}
	TArray<FHitResult> Hits;
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
	Fixture->World->SweepMultiByObjectType(Hits, FVector(-250.0, 0.0, -35.0), FVector(250.0, 0.0, -35.0),
		FQuat::Identity, Objects, FCollisionShape::MakeSphere(10.0f),
		FCollisionQueryParams(SCENE_QUERY_STAT(GolemSkillLabArmorTest), false));
	const FHitResult* ArmorHit = Hits.FindByPredicate([Armor](const FHitResult& Candidate)
	{
		return Candidate.GetComponent() == Armor->GetArmorZone();
	});
	if (!TestNotNull(TEXT("Actual WorldDynamic object sweep discovers armor collision"), ArmorHit))
	{
		return false;
	}
	const FHitResult Hit = *ArmorHit;
	float Damage = 0.0f;
	const FGuid First = FGuid::NewGuid();
	Armor->ResolveArmorHit(Hit, First, 0, true, 100.0f, Damage);
	TestEqual(TEXT("Intact armored hit reduces HP packet to20%"), Damage, 20.0f);
	TestEqual(TEXT("Heavy durability is20 and separate from HP"), Armor->GetDurability(), 80.0f);
	Armor->ResolveArmorHit(Hit, First, 0, true, 100.0f, Damage);
	TestEqual(TEXT("Duplicate activation/hitIndex applies no HP damage"), Damage, 0.0f);
	TestEqual(TEXT("Duplicate hit does not drain durability"), Armor->GetDurability(), 80.0f);
	TestTrue(TEXT("Frost fixture makes zone brittle"), Armor->MakeBrittle());
	Armor->ResolveArmorHit(Hit, FGuid::NewGuid(), 0, true, 100.0f, Damage);
	TestEqual(TEXT("Brittle hit uses50% HP packet"), Damage, 50.0f);
	TestEqual(TEXT("Brittle doubles heavy durability loss"), Armor->GetDurability(), 40.0f);
	Armor->ResolveArmorHit(Hit, FGuid::NewGuid(), 0, true, 100.0f, Damage);
	TestEqual(TEXT("Breaking hit still uses pre-hit brittle factor"), Damage, 50.0f);
	TestEqual(TEXT("Zone is Broken after breaking hit"), Armor->GetArmorState(), EGolemBossArmorState::Broken);
	Armor->ResolveArmorHit(Hit, FGuid::NewGuid(), 0, false, 100.0f, Damage);
	TestEqual(TEXT("Following hit benefits from exposed armor"), Damage, 120.0f);
	AGolemRPGCharacter* Attacker = Fixture->SpawnGolem(FVector(1500.0, 0.0, 100.0));
	AGolemArenaEnemy* Enemy = Fixture->World->SpawnActor<AGolemArenaEnemy>(
		FVector(1000.0, 0.0, 100.0), FRotator::ZeroRotator);
	if (!TestNotNull(TEXT("Actual packet source Golem"), Attacker)
		|| !TestNotNull(TEXT("Actual packet target enemy"), Enemy)
		|| !TestTrue(TEXT("Actual enemy initializes existing archetype and combat profile"),
			Enemy->InitializeArenaEnemy(FPrimaryAssetId(TEXT("GolemAIArchetype"), TEXT("ArenaEnemy"))).IsSuccess()))
	{
		return false;
	}
	Enemy->GetCharacterMovement()->DisableMovement();
	UGolemBossArmorComponent* EnemyArmor = Enemy->FindComponentByClass<UGolemBossArmorComponent>();
	if (!TestNotNull(TEXT("Enemy owns actual armor adapter"), EnemyArmor))
	{
		return false;
	}
	EnemyArmor->EnableArmorZone(FGuid::NewGuid());
	TArray<FHitResult> EnemyHits;
	Fixture->World->SweepMultiByObjectType(EnemyHits, FVector(750.0, 0.0, 65.0), FVector(1250.0, 0.0, 65.0),
		FQuat::Identity, Objects, FCollisionShape::MakeSphere(10.0f),
		FCollisionQueryParams(SCENE_QUERY_STAT(GolemSkillLabEnemyArmorPacketTest), false));
	const FHitResult* EnemyHit = EnemyHits.FindByPredicate([EnemyArmor](const FHitResult& Candidate)
	{
		return Candidate.GetComponent() == EnemyArmor->GetArmorZone();
	});
	if (!TestNotNull(TEXT("Actual object sweep discovers real enemy armor zone"), EnemyHit))
	{
		return false;
	}
	FGolemSkillDamagePacket Packet;
	Packet.RawDamage = 100.0f;
	Packet.PoiseDamage = 7.0f;
	Packet.bHeavy = true;
	Packet.bCanDefend = false;
	Packet.ActivationId = FGuid::NewGuid();
	Packet.HitIndex = 3;
	const float InitialHP = Enemy->GetGolemAttributeSet()->GetHealth();
	const float InitialPoise = Enemy->GetCombatComponent()->GetSnapshot().Poise;
	TestTrue(TEXT("First actual armored skill packet executes through GAS"),
		UGolemSkillRuntimeComponent::ApplySkillDamage(Attacker, Enemy, Packet, EnemyHit).IsSuccess());
	const float AfterFirstHP = Enemy->GetGolemAttributeSet()->GetHealth();
	const float AfterFirstPoise = Enemy->GetCombatComponent()->GetSnapshot().Poise;
	const float AfterFirstDurability = EnemyArmor->GetDurability();
	TestTrue(TEXT("First packet actually reduces enemy HP and Poise"),
		AfterFirstHP < InitialHP && AfterFirstPoise < InitialPoise);
	TestEqual(TEXT("First packet reduces actual enemy armor durability"), AfterFirstDurability, 80.0f);
	TestTrue(TEXT("Duplicate actual armored skill packet reaches GAS execution"),
		UGolemSkillRuntimeComponent::ApplySkillDamage(Attacker, Enemy, Packet, EnemyHit).IsSuccess());
	TestEqual(TEXT("Duplicate execution preserves actual enemy HP"), Enemy->GetGolemAttributeSet()->GetHealth(), AfterFirstHP);
	TestEqual(TEXT("Duplicate execution preserves actual enemy Poise"), Enemy->GetCombatComponent()->GetSnapshot().Poise, AfterFirstPoise);
	TestEqual(TEXT("Duplicate execution preserves actual enemy durability"), EnemyArmor->GetDurability(), AfterFirstDurability);
	const TWeakObjectPtr<UGolemBossArmorComponent> WeakArmor(Armor);
	WaitForWorldTime(this, Fixture, 10.2f, [this, WeakArmor]()
	{
		if (TestTrue(TEXT("Armor persists until timer completion"), WeakArmor.IsValid()))
		{
			TestEqual(TEXT("Broken expires to Intact"), WeakArmor->GetArmorState(), EGolemBossArmorState::Intact);
			TestEqual(TEXT("Broken expiry restores100 durability"), WeakArmor->GetDurability(), 100.0f);
		}
	});
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGolemSkillLabRunestoneTest, "Golem.M55.World.ChannelPickupRaceAndCrystal",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGolemSkillLabRunestoneTest::RunTest(const FString& Parameters)
{
	using namespace GolemSkillLabWorldTests;
	TSharedRef<FGolemWorld> Fixture = MakeShared<FGolemWorld>();
	AGolemRPGCharacter* Golem = Fixture->SpawnGolem(FVector(0.0, 0.0, 100.0));
	AGolemRPGCharacter* Other = Fixture->SpawnGolem(FVector(-300.0, 300.0, 100.0), TEXT("Training_Blue"));
	if (!TestNotNull(TEXT("Canonical real Golem fixture initializes"), Golem))
	{
		return false;
	}
	if (!TestNotNull(TEXT("Second canonical collector initializes"), Other))
	{
		return false;
	}
	AGolemRunestone* Stone = Fixture->World->SpawnActor<AGolemRunestone>(
		FVector(100.0, 0.0, 60.0), FRotator::ZeroRotator);
	if (!TestNotNull(TEXT("Actual healing Runestone exists"), Stone))
	{
		return false;
	}
	Stone->ConfigureFixture(EGolemRunestoneKind::Healing, FGuid::NewGuid());
	TestTrue(TEXT("First activation reserves channel"), Stone->TryActivate(Golem, FGuid::NewGuid()).IsSuccess());
	TestFalse(TEXT("Concurrent request cannot reserve same stone"), Stone->TryActivate(Golem, FGuid::NewGuid()).IsSuccess());
	const TWeakObjectPtr<AGolemRPGCharacter> WeakGolem(Golem);
	const TWeakObjectPtr<AGolemRPGCharacter> WeakOther(Other);
	const TWeakObjectPtr<AGolemRunestone> WeakStone(Stone);
	WaitForWorldTime(this, Fixture, 2.2f, [this, Fixture, WeakGolem, WeakOther, WeakStone]()
	{
		if (!TestTrue(TEXT("Channel actors persist"), WeakGolem.IsValid() && WeakOther.IsValid() && WeakStone.IsValid()))
		{
			return;
		}
		TestEqual(TEXT("Success starts30s cooldown"), WeakStone->GetState(), EGolemRunestoneState::Cooldown);
		TestEqual(TEXT("Activation itself does not heal or buff party"), WeakGolem->GetGolemAttributeSet()->GetHealth(), 1000.0f);
		TArray<TWeakObjectPtr<AGolemRunestonePickup>> Pickups;
		for (TActorIterator<AGolemRunestonePickup> It(Fixture->World.Get()); It; ++It)
		{
			Pickups.Add(*It);
		}
		TestEqual(TEXT("Healing activation creates2 pickups and1 fire"), Pickups.Num(), 3);
		int32 HealingCount = 0;
		int32 FireCount = 0;
		for (const TWeakObjectPtr<AGolemRunestonePickup>& Pickup : Pickups)
		{
			WeakGolem->GetGolemAbilitySystem()->SetNumericAttributeBase(UGolemAttributeSet::GetHealthAttribute(), 1000.0f);
			WeakGolem->SetActorLocation(Pickup->GetActorLocation() + FVector(0.0, 0.0, 100.0));
			TestFalse(TEXT("Full HP leaves healing charge available"), Pickup->TryCollect(WeakGolem.Get(), FGuid::NewGuid()).IsSuccess());
			const int32 Charges = Pickup->GetRemainingCharges();
			WeakGolem->GetGolemAbilitySystem()->SetNumericAttributeBase(UGolemAttributeSet::GetHealthAttribute(), 500.0f);
			const FGuid Request = FGuid::NewGuid();
			TestTrue(TEXT("Collector receives actual health effect"), Pickup->TryCollect(WeakGolem.Get(), Request).IsSuccess());
			TestEqual(TEXT("Heal scales with recipient MaxHP15%"), WeakGolem->GetGolemAttributeSet()->GetHealth(), 650.0f);
			TestEqual(TEXT("One authoritative charge consumed"), Pickup->GetRemainingCharges(), Charges - 1);
			TestFalse(TEXT("Replay consumes no second charge"), Pickup->TryCollect(WeakGolem.Get(), Request).IsSuccess());
			WeakOther->SetActorLocation(Pickup->GetActorLocation() + FVector(0.0, 100.0, 100.0));
			WeakOther->GetGolemAbilitySystem()->SetNumericAttributeBase(UGolemAttributeSet::GetHealthAttribute(), 500.0f);
			if (Pickup->GetKind() == EGolemRunestonePickupKind::Fire)
			{
				++FireCount;
				TestTrue(TEXT("Fire second charge is available to another Golem"), Pickup->TryCollect(WeakOther.Get(), FGuid::NewGuid()).IsSuccess());
				TestEqual(TEXT("Fire grants recipient's scaled heal"), WeakOther->GetGolemAttributeSet()->GetHealth(), 650.0f);
			}
			else
			{
				++HealingCount;
				TestFalse(TEXT("Second collector loses consumed healing pickup"), Pickup->TryCollect(WeakOther.Get(), FGuid::NewGuid()).IsSuccess());
				TestEqual(TEXT("Losing collector receives no heal"), WeakOther->GetGolemAttributeSet()->GetHealth(), 500.0f);
			}
		}
		TestEqual(TEXT("Actual world result contains two healing pickup types"), HealingCount, 2);
		TestEqual(TEXT("Actual world result contains one two-charge fire"), FireCount, 1);
		WeakOther->SetActorLocation(FVector(-300.0, 300.0, 100.0));
		WeakStone->ResetFixture(FGuid::NewGuid());
		WeakGolem->SetActorLocation(WeakStone->GetActorLocation() + FVector(-120.0, 0.0, 40.0));
		TestTrue(TEXT("Retry permits reservation"), WeakStone->TryActivate(WeakGolem.Get(), FGuid::NewGuid()).IsSuccess());
		WeakGolem->SetActorLocation(FVector(-1000.0, 0.0, 100.0));
		WaitForWorldTime(this, Fixture, 0.2f, [this, Fixture, WeakGolem, WeakOther, WeakStone]()
		{
			TestEqual(TEXT("Leaving range cancels without cooldown"), WeakStone->GetState(), EGolemRunestoneState::Ready);
			WeakStone->ConfigureFixture(EGolemRunestoneKind::Crystal, FGuid::NewGuid());
			WeakGolem->SetActorLocation(WeakStone->GetActorLocation() + FVector(-120.0, 0.0, 40.0));
			TestTrue(TEXT("Crystal Runestone starts real channel"), WeakStone->TryActivate(WeakGolem.Get(), FGuid::NewGuid()).IsSuccess());
			WaitForWorldTime(this, Fixture, 2.2f, [this, Fixture, WeakGolem, WeakOther, WeakStone]()
			{
				TestEqual(TEXT("Crystal channel completes"), WeakStone->GetState(), EGolemRunestoneState::Cooldown);
				TestEqual(TEXT("Crystal activation does not directly buff Golem"), WeakGolem->GetGolemAttributeSet()->GetAttackPower(), 100.0f);
				TWeakObjectPtr<AGolemRunestonePickup> Crystal;
				int32 CrystalCount = 0;
				for (TActorIterator<AGolemRunestonePickup> It(Fixture->World.Get()); It; ++It)
				{
					if (It->GetKind() == EGolemRunestonePickupKind::PowerCrystal)
					{
						Crystal = *It;
						++CrystalCount;
					}
				}
				TestEqual(TEXT("Crystal channel creates exactly one world pickup"), CrystalCount, 1);
				if (!TestTrue(TEXT("Actual crystal pickup exists"), Crystal.IsValid()))
				{
					return;
				}
				WeakGolem->SetActorLocation(Crystal->GetActorLocation() + FVector(0.0, 0.0, 100.0));
				TestTrue(TEXT("Crystal applies collector-owned GE"), Crystal->TryCollect(WeakGolem.Get(), FGuid::NewGuid()).IsSuccess());
				TestEqual(TEXT("Crystal Power multiplier15%"), WeakGolem->GetGolemAttributeSet()->GetAttackPower(), 115.0f);
				WeakOther->SetActorLocation(Crystal->GetActorLocation() + FVector(0.0, 100.0, 100.0));
				TestFalse(TEXT("Different Golem loses crystal race"), Crystal->TryCollect(WeakOther.Get(), FGuid::NewGuid()).IsSuccess());
				TestEqual(TEXT("Crystal loser receives no Power buff"), WeakOther->GetGolemAttributeSet()->GetAttackPower(), 100.0f);
				WaitForWorldTime(this, Fixture, 10.2f, [this, WeakGolem]()
				{
					TestEqual(TEXT("Collector-owned crystal effect expires after10s"), WeakGolem->GetGolemAttributeSet()->GetAttackPower(), 100.0f);
				});
			});
		});
	});
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGolemSkillLabFrozenSurfaceTest, "Golem.M55.World.FrozenWarningAndSafeEjection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGolemSkillLabFrozenSurfaceTest::RunTest(const FString& Parameters)
{
	using namespace GolemSkillLabWorldTests;
	TSharedRef<FGolemWorld> Fixture = MakeShared<FGolemWorld>();
	AGolemFrozenSurface* Surface = Fixture->World->SpawnActor<AGolemFrozenSurface>(
		FVector(0.0, 0.0, 55.0), FRotator::ZeroRotator);
	AGolemRPGCharacter* Golem = Fixture->SpawnGolem(FVector(0.0, 0.0, 150.0));
	UGolemBossArmorComponent* Armor = SpawnArmor(Fixture->World.Get(), FVector(500.0, 0.0, 100.0));
	AGolemRunestone* Stone = Fixture->World->SpawnActor<AGolemRunestone>(
		FVector(-120.0, 0.0, 60.0), FRotator::ZeroRotator);
	if (!TestNotNull(TEXT("Golem available"), Golem) || !TestNotNull(TEXT("Surface available"), Surface)
		|| !TestNotNull(TEXT("Armor available"), Armor) || !TestNotNull(TEXT("Frost Runestone available"), Stone))
	{
		return false;
	}
	Stone->ConfigureFixture(EGolemRunestoneKind::Frost, FGuid::NewGuid());
	TestTrue(TEXT("Frost Runestone channel starts"), Stone->TryActivate(Golem, FGuid::NewGuid()).IsSuccess());
	const TWeakObjectPtr<AGolemFrozenSurface> WeakSurface(Surface);
	const TWeakObjectPtr<AGolemRPGCharacter> WeakGolem(Golem);
	const TWeakObjectPtr<UGolemBossArmorComponent> WeakArmor(Armor);
	const TWeakObjectPtr<AGolemRunestone> WeakStone(Stone);
	WaitForWorldTime(this, Fixture, 2.2f, [this, Fixture, WeakSurface, WeakGolem, WeakArmor, WeakStone]()
	{
		TestEqual(TEXT("Frost activation starts cooldown"), WeakStone->GetState(), EGolemRunestoneState::Cooldown);
		TestTrue(TEXT("Runestone creates actual frozen collision state"), WeakSurface->IsFrozen());
		TestEqual(TEXT("Frost activation makes actual boss armor brittle"), WeakArmor->GetArmorState(), EGolemBossArmorState::Brittle);
		TestEqual(TEXT("Frost activation does not buff party armor"), WeakGolem->GetGolemAttributeSet()->GetDefense(), 100.0f);
		WaitForWorldTime(this, Fixture, 10.1f, [this, Fixture, WeakSurface, WeakGolem, WeakArmor]()
		{
			TestTrue(TEXT("Two-second warning begins before thaw"), WeakSurface->IsThawWarning());
			WeakGolem->SetActorLocation(FVector(0.0, 0.0, 150.0));
			WaitForWorldTime(this, Fixture, 2.1f, [this, WeakSurface, WeakGolem, WeakArmor]()
			{
				TestFalse(TEXT("Surface thaws after12s"), WeakSurface->IsFrozen());
				TestTrue(TEXT("Occupant moved safely outside disappearing collision"), WeakGolem->GetActorLocation().X > 250.0);
				TestEqual(TEXT("Brittle fixture expires to intact"), WeakArmor->GetArmorState(), EGolemBossArmorState::Intact);
			});
		});
	});
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGolemSkillLabChannelOwnershipTest,
	"Golem.M55.World.ChannelLifeOwnershipResetAndReplay",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGolemSkillLabChannelOwnershipTest::RunTest(const FString& Parameters)
{
	using namespace GolemSkillLabWorldTests;
	TSharedRef<FGolemWorld> Fixture = MakeShared<FGolemWorld>();
	AGolemRPGCharacter* Golem = Fixture->SpawnGolem(FVector(0.0, 0.0, 100.0));
	AGolemRunestone* Stone = Fixture->World->SpawnActor<AGolemRunestone>(
		FVector(100.0, 0.0, 60.0), FRotator::ZeroRotator);
	if (!TestNotNull(TEXT("Lifecycle Golem fixture"), Golem) || !TestNotNull(TEXT("Lifecycle Runestone fixture"), Stone))
	{
		return false;
	}
	Stone->ConfigureFixture(EGolemRunestoneKind::Healing, FGuid::NewGuid());
	const TWeakObjectPtr<APlayerController> OriginalController(Cast<APlayerController>(Golem->GetController()));
	if (!TestNotNull(TEXT("Original channel controller"), OriginalController.Get()))
	{
		return false;
	}
	TestTrue(TEXT("Living Golem reserves actual channel"), Stone->TryActivate(Golem, FGuid::NewGuid()).IsSuccess());
	Golem->GetGolemAbilitySystem()->SetNumericAttributeBase(UGolemAttributeSet::GetHealthAttribute(), 0.0f);
	TestTrue(TEXT("Actual health callback transitions canonical Golem to Downed"), Golem->GetLifeComponent()->IsDowned());
	const TWeakObjectPtr<AGolemRPGCharacter> WeakGolem(Golem);
	const TWeakObjectPtr<AGolemRunestone> WeakStone(Stone);
	const FGolemEntityId Identity = Golem->GetGolemEntityId();
	const TWeakObjectPtr<UGolemAbilitySystemComponent> OriginalASC(Golem->GetGolemAbilitySystem());
	WaitForWorldTime(this, Fixture, 0.2f, [this, Fixture, WeakGolem, WeakStone, Identity, OriginalASC, OriginalController]()
	{
		TestEqual(TEXT("Downed state cancels reservation without cooldown"), WeakStone->GetState(), EGolemRunestoneState::Ready);
		TestEqual(TEXT("Canceled downed channel leaves no cooldown time"), WeakStone->GetRemainingTime(), 0.0f);
		TestTrue(TEXT("Explicit checkpoint fixture restores life"), WeakGolem->GetLifeComponent()->ResetForCheckpoint().IsSuccess());
		WeakGolem->GetCharacterMovement()->DisableMovement();
		WeakGolem->SetActorLocation(WeakStone->GetActorLocation() + FVector(-120.0, 0.0, 40.0));
		TestTrue(TEXT("Restored Golem reserves channel"), WeakStone->TryActivate(WeakGolem.Get(), FGuid::NewGuid()).IsSuccess());
		APlayerController* Replacement = Fixture->World->SpawnActor<APlayerController>();
		if (!TestNotNull(TEXT("Actual replacement controller"), Replacement))
		{
			return;
		}
		Replacement->Possess(WeakGolem.Get());
		WeakGolem->GetCharacterMovement()->DisableMovement();
		WaitForWorldTime(this, Fixture, 0.2f, [this, Fixture, WeakGolem, WeakStone, Identity, OriginalASC, OriginalController]()
		{
			TestEqual(TEXT("Changed controller cancels the old controller's reservation"), WeakStone->GetState(), EGolemRunestoneState::Ready);
			TestTrue(TEXT("Possession preserves canonical Golem identity and Character ASC"),
				WeakGolem->GetGolemEntityId() == Identity && WeakGolem->GetGolemAbilitySystem() == OriginalASC.Get());
			OriginalController->Possess(WeakGolem.Get());
			WeakGolem->GetCharacterMovement()->DisableMovement();
			TestTrue(TEXT("Original controller can start a fresh channel"), WeakStone->TryActivate(WeakGolem.Get(), FGuid::NewGuid()).IsSuccess());
			WeakStone->ResetFixture(FGuid::NewGuid());
			WaitForWorldTime(this, Fixture, 2.2f, [this, Fixture, WeakGolem, WeakStone]()
			{
				TestEqual(TEXT("Encounter reset cancels active channel and its commit timer"), WeakStone->GetState(), EGolemRunestoneState::Ready);
				int32 BeforeCount = 0;
				for (TActorIterator<AGolemRunestonePickup> It(Fixture->World.Get()); It; ++It)
				{
					++BeforeCount;
				}
				TestEqual(TEXT("Downed, takeover and reset cancellations spawn no results"), BeforeCount, 0);
				const FGuid CommittedRequest = FGuid::NewGuid();
				TestTrue(TEXT("Fresh retry after cancellation reserves channel"), WeakStone->TryActivate(WeakGolem.Get(), CommittedRequest).IsSuccess());
				WaitForWorldTime(this, Fixture, 2.2f, [this, Fixture, WeakGolem, WeakStone, CommittedRequest]()
				{
					TestEqual(TEXT("Fresh reservation commits actual world result"), WeakStone->GetState(), EGolemRunestoneState::Cooldown);
					TestFalse(TEXT("Committed request replay is rejected during cooldown"), WeakStone->TryActivate(WeakGolem.Get(), CommittedRequest).IsSuccess());
					WaitForWorldTime(this, Fixture, 30.2f, [this, Fixture, WeakGolem, WeakStone, CommittedRequest]()
					{
						TestEqual(TEXT("Actual thirty-second cooldown expires"), WeakStone->GetState(), EGolemRunestoneState::Ready);
						TestFalse(TEXT("Committed request remains rejected after cooldown"), WeakStone->TryActivate(WeakGolem.Get(), CommittedRequest).IsSuccess());
						TestEqual(TEXT("Replay cannot create a new reservation"), WeakStone->GetState(), EGolemRunestoneState::Ready);
						TestTrue(TEXT("New request is allowed after cooldown"), WeakStone->TryActivate(WeakGolem.Get(), FGuid::NewGuid()).IsSuccess());
						WeakStone->ResetFixture(FGuid::NewGuid());
					});
				});
			});
		});
	});
	return true;
}

#endif
