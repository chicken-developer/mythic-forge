#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Assets/KnightAssetManager.h"
#include "Attributes/KnightAttributeSet.h"
#include "Character/KnightCharacter.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Components/KnightAbilitySystemComponent.h"
#include "Components/KnightCombatComponent.h"
#include "Components/KnightLifeComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "CoreGlobals.h"
#include "Definitions/KnightHeroDefinition.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "HAL/PlatformTime.h"
#include "PartyArena/KnightArenaEnemy.h"
#include "RPGLoop/KnightRPGCharacter.h"
#include "SkillLab/KnightBossArmorComponent.h"
#include "SkillLab/KnightFrozenSurface.h"
#include "SkillLab/KnightRunestone.h"
#include "SkillLab/KnightRunestonePickup.h"
#include "Skills/KnightSkillRuntimeComponent.h"
#include "Skills/KnightSkillTypes.h"
#include "UObject/StrongObjectPtr.h"

namespace KnightSkillLabWorldTests
{
	class FKnightWorld final : public FNoncopyable
	{
	public:
		FKnightWorld()
			: Instance(NewObject<UGameInstance>(GEngine))
		{
			Instance->InitializeStandalone();
			World = Instance->GetWorld();
			World->SetShouldTick(false);
			World->GetWorldSettings()->DefaultGameMode = AGameModeBase::StaticClass();
			FURL URL;
			URL.Map = TEXT("KnightSkillLabWorldTest");
			URL.AddOption(TEXT("game=/Script/Engine.GameModeBase"));
			World->SetGameMode(URL);
			World->InitializeActorsForPlay(URL);
			World->BeginPlay();
		}
		~FKnightWorld()
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
		AKnightRPGCharacter* SpawnKnight(const FVector& Location, FName HeroName = TEXT("Training_Red"))
		{
			UKnightDefinition* Definition = nullptr;
			UKnightAssetManager* Manager = UKnightAssetManager::GetKnightAssetManager();
			if (Manager == nullptr || !Manager->ResolveDefinition(
				FPrimaryAssetId(TEXT("KnightHero"), HeroName), UKnightHeroDefinition::StaticClass(), Definition).IsSuccess()
				|| Cast<UKnightHeroDefinition>(Definition) == nullptr)
			{
				return nullptr;
			}
			FActorSpawnParameters Parameters;
			Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			AKnightRPGCharacter* Knight = World->SpawnActor<AKnightRPGCharacter>(
				Location, FRotator::ZeroRotator, Parameters);
			if (Knight == nullptr || !Knight->InitializeKnight(FKnightEntityId::Create(),
				Cast<UKnightHeroDefinition>(Definition)).IsSuccess())
			{
				return nullptr;
			}
			APlayerController* Controller = World->SpawnActor<APlayerController>();
			if (Controller == nullptr)
			{
				return nullptr;
			}
			Controller->Possess(Knight);
			// This deliberately floorless fixture tests timers and explicit range changes, without gravity drift.
			Knight->GetCharacterMovement()->DisableMovement();
			UKnightAbilitySystemComponent* ASC = Knight->GetKnightAbilitySystem();
			ASC->SetNumericAttributeBase(UKnightAttributeSet::GetMaxHealthAttribute(), 1000.0f);
			ASC->SetNumericAttributeBase(UKnightAttributeSet::GetHealthAttribute(), 1000.0f);
			ASC->SetNumericAttributeBase(UKnightAttributeSet::GetAttackPowerAttribute(), 100.0f);
			ASC->SetNumericAttributeBase(UKnightAttributeSet::GetDefenseAttribute(), 100.0f);
			return Knight;
		}
		TWeakObjectPtr<UWorld> World;
	private:
		TStrongObjectPtr<UGameInstance> Instance;
		uint64 LastTickedFrame = MAX_uint64;
	};

	class FKnightWaitForWorldTime final : public IAutomationLatentCommand
	{
	public:
		FKnightWaitForWorldTime(FAutomationTestBase* InTest, const TSharedRef<FKnightWorld>& InContext,
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
		TSharedRef<FKnightWorld> Context;
		TFunction<void()> Completion;
		double TargetTime;
		double StartedAt;
	};

	void WaitForWorldTime(FAutomationTestBase* Test, const TSharedRef<FKnightWorld>& Context,
		float Seconds, TFunction<void()> Completion)
	{
		FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FKnightWaitForWorldTime>(
			Test, Context, Seconds, MoveTemp(Completion)));
	}

	UKnightBossArmorComponent* SpawnArmor(UWorld* World, const FVector& Location = FVector::ZeroVector)
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
		UKnightBossArmorComponent* Armor = NewObject<UKnightBossArmorComponent>(Owner);
		Owner->AddInstanceComponent(Armor);
		Armor->RegisterComponent();
		Armor->EnableArmorZone(FGuid::NewGuid());
		return Armor;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKnightGearItemProcArmorTest, "Knight.M56.World.ItemProcDoesNotReactWithArmor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FKnightGearItemProcArmorTest::RunTest(const FString& Parameters)
{
	using namespace KnightSkillLabWorldTests;
	TSharedRef<FKnightWorld> Fixture = MakeShared<FKnightWorld>();
	AKnightRPGCharacter* Source = Fixture->SpawnKnight(FVector(-400.0, 0.0, 0.0));
	AKnightRPGCharacter* Target = Fixture->SpawnKnight(FVector::ZeroVector, TEXT("Training_Blue"));
	if (!TestNotNull(TEXT("Actual item proc source"), Source) || !TestNotNull(TEXT("Actual armored target"), Target))
	{
		return false;
	}
	TestTrue(TEXT("Actual target combat profile initializes"), Target->InitializeArenaCombat().IsSuccess());
	FKnightSkillBuild SourceBuild;
	SourceBuild.SpecId = TEXT("Uranus");
	FKnightSkillBuild TargetBuild;
	TargetBuild.SpecId = TEXT("Pluto");
	TestTrue(TEXT("Source shared GAS build"), Source->FindComponentByClass<UKnightSkillRuntimeComponent>()->ConfigureBuild(SourceBuild).IsSuccess());
	TestTrue(TEXT("Target shared GAS build"), Target->FindComponentByClass<UKnightSkillRuntimeComponent>()->ConfigureBuild(TargetBuild).IsSuccess());
	UKnightBossArmorComponent* Armor = NewObject<UKnightBossArmorComponent>(Target);
	Target->AddInstanceComponent(Armor);
	Armor->RegisterComponent();
	Armor->EnableArmorZone(FGuid::NewGuid());
	const float HealthBefore = Target->GetKnightAttributeSet()->GetHealth();
	const float DurabilityBefore = Armor->GetDurability();
	const float PoiseBefore = Target->GetCombatComponent()->GetSnapshot().Poise;
	TestTrue(TEXT("Nonzero combat poise makes the reaction assertion meaningful"), PoiseBefore > 0.0f);
	FHitResult Hit(Target, Armor->GetArmorZone(), Target->GetActorLocation(), FVector::ForwardVector);
	FKnightSkillDamagePacket Packet;
	Packet.RawDamage = 100.0f;
	Packet.PoiseDamage = 50.0f;
	Packet.School = EKnightSkillSchool::Arcane;
	Packet.bDirect = false;
	Packet.bItemProc = true;
	Packet.bHeavy = true;
	Packet.ActivationId = FGuid::NewGuid();
	TestTrue(TEXT("Secondary packet traverses real GAS damage execution"),
		UKnightSkillRuntimeComponent::ApplySkillDamage(Source, Target, Packet, &Hit).IsSuccess());
	TestTrue(TEXT("Item proc damages HP with normal magical defenses rather than armor-zone scalar"),
		FMath::IsNearlyEqual(HealthBefore - Target->GetKnightAttributeSet()->GetHealth(), 100.0f));
	TestEqual(TEXT("Item proc cannot break durability"), Armor->GetDurability(), DurabilityBefore);
	TestEqual(TEXT("Item proc cannot stagger via poise"), Target->GetCombatComponent()->GetSnapshot().Poise, PoiseBefore);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKnightSkillLabArmorTest, "Knight.M55.World.ArmorDurabilityAndDedup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FKnightSkillLabArmorTest::RunTest(const FString& Parameters)
{
	using namespace KnightSkillLabWorldTests;
	TSharedRef<FKnightWorld> Fixture = MakeShared<FKnightWorld>();
	UKnightBossArmorComponent* Armor = SpawnArmor(Fixture->World.Get());
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
		FCollisionQueryParams(SCENE_QUERY_STAT(KnightSkillLabArmorTest), false));
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
	TestEqual(TEXT("Zone is Broken after breaking hit"), Armor->GetArmorState(), EKnightBossArmorState::Broken);
	Armor->ResolveArmorHit(Hit, FGuid::NewGuid(), 0, false, 100.0f, Damage);
	TestEqual(TEXT("Following hit benefits from exposed armor"), Damage, 120.0f);
	AKnightRPGCharacter* Attacker = Fixture->SpawnKnight(FVector(1500.0, 0.0, 100.0));
	AKnightArenaEnemy* Enemy = Fixture->World->SpawnActor<AKnightArenaEnemy>(
		FVector(1000.0, 0.0, 100.0), FRotator::ZeroRotator);
	if (!TestNotNull(TEXT("Actual packet source Knight"), Attacker)
		|| !TestNotNull(TEXT("Actual packet target enemy"), Enemy)
		|| !TestTrue(TEXT("Actual enemy initializes existing archetype and combat profile"),
			Enemy->InitializeArenaEnemy(FPrimaryAssetId(TEXT("KnightAIArchetype"), TEXT("ArenaEnemy"))).IsSuccess()))
	{
		return false;
	}
	Enemy->GetCharacterMovement()->DisableMovement();
	UKnightBossArmorComponent* EnemyArmor = Enemy->FindComponentByClass<UKnightBossArmorComponent>();
	if (!TestNotNull(TEXT("Enemy owns actual armor adapter"), EnemyArmor))
	{
		return false;
	}
	EnemyArmor->EnableArmorZone(FGuid::NewGuid());
	TArray<FHitResult> EnemyHits;
	Fixture->World->SweepMultiByObjectType(EnemyHits, FVector(750.0, 0.0, 65.0), FVector(1250.0, 0.0, 65.0),
		FQuat::Identity, Objects, FCollisionShape::MakeSphere(10.0f),
		FCollisionQueryParams(SCENE_QUERY_STAT(KnightSkillLabEnemyArmorPacketTest), false));
	const FHitResult* EnemyHit = EnemyHits.FindByPredicate([EnemyArmor](const FHitResult& Candidate)
	{
		return Candidate.GetComponent() == EnemyArmor->GetArmorZone();
	});
	if (!TestNotNull(TEXT("Actual object sweep discovers real enemy armor zone"), EnemyHit))
	{
		return false;
	}
	FKnightSkillDamagePacket Packet;
	Packet.RawDamage = 100.0f;
	Packet.PoiseDamage = 7.0f;
	Packet.bHeavy = true;
	Packet.bCanDefend = false;
	Packet.ActivationId = FGuid::NewGuid();
	Packet.HitIndex = 3;
	const float InitialHP = Enemy->GetKnightAttributeSet()->GetHealth();
	const float InitialPoise = Enemy->GetCombatComponent()->GetSnapshot().Poise;
	TestTrue(TEXT("First actual armored skill packet executes through GAS"),
		UKnightSkillRuntimeComponent::ApplySkillDamage(Attacker, Enemy, Packet, EnemyHit).IsSuccess());
	const float AfterFirstHP = Enemy->GetKnightAttributeSet()->GetHealth();
	const float AfterFirstPoise = Enemy->GetCombatComponent()->GetSnapshot().Poise;
	const float AfterFirstDurability = EnemyArmor->GetDurability();
	TestTrue(TEXT("First packet actually reduces enemy HP and Poise"),
		AfterFirstHP < InitialHP && AfterFirstPoise < InitialPoise);
	TestEqual(TEXT("First packet reduces actual enemy armor durability"), AfterFirstDurability, 80.0f);
	TestTrue(TEXT("Duplicate actual armored skill packet reaches GAS execution"),
		UKnightSkillRuntimeComponent::ApplySkillDamage(Attacker, Enemy, Packet, EnemyHit).IsSuccess());
	TestEqual(TEXT("Duplicate execution preserves actual enemy HP"), Enemy->GetKnightAttributeSet()->GetHealth(), AfterFirstHP);
	TestEqual(TEXT("Duplicate execution preserves actual enemy Poise"), Enemy->GetCombatComponent()->GetSnapshot().Poise, AfterFirstPoise);
	TestEqual(TEXT("Duplicate execution preserves actual enemy durability"), EnemyArmor->GetDurability(), AfterFirstDurability);
	const TWeakObjectPtr<UKnightBossArmorComponent> WeakArmor(Armor);
	WaitForWorldTime(this, Fixture, 10.2f, [this, WeakArmor]()
	{
		if (TestTrue(TEXT("Armor persists until timer completion"), WeakArmor.IsValid()))
		{
			TestEqual(TEXT("Broken expires to Intact"), WeakArmor->GetArmorState(), EKnightBossArmorState::Intact);
			TestEqual(TEXT("Broken expiry restores100 durability"), WeakArmor->GetDurability(), 100.0f);
		}
	});
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKnightSkillLabRunestoneTest, "Knight.M55.World.ChannelPickupRaceAndCrystal",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FKnightSkillLabRunestoneTest::RunTest(const FString& Parameters)
{
	using namespace KnightSkillLabWorldTests;
	TSharedRef<FKnightWorld> Fixture = MakeShared<FKnightWorld>();
	AKnightRPGCharacter* Knight = Fixture->SpawnKnight(FVector(0.0, 0.0, 100.0));
	AKnightRPGCharacter* Other = Fixture->SpawnKnight(FVector(-300.0, 300.0, 100.0), TEXT("Training_Blue"));
	if (!TestNotNull(TEXT("Canonical real Knight fixture initializes"), Knight))
	{
		return false;
	}
	if (!TestNotNull(TEXT("Second canonical collector initializes"), Other))
	{
		return false;
	}
	AKnightRunestone* Stone = Fixture->World->SpawnActor<AKnightRunestone>(
		FVector(100.0, 0.0, 60.0), FRotator::ZeroRotator);
	if (!TestNotNull(TEXT("Actual healing Runestone exists"), Stone))
	{
		return false;
	}
	Stone->ConfigureFixture(EKnightRunestoneKind::Healing, FGuid::NewGuid());
	TestTrue(TEXT("First activation reserves channel"), Stone->TryActivate(Knight, FGuid::NewGuid()).IsSuccess());
	TestFalse(TEXT("Concurrent request cannot reserve same stone"), Stone->TryActivate(Knight, FGuid::NewGuid()).IsSuccess());
	const TWeakObjectPtr<AKnightRPGCharacter> WeakKnight(Knight);
	const TWeakObjectPtr<AKnightRPGCharacter> WeakOther(Other);
	const TWeakObjectPtr<AKnightRunestone> WeakStone(Stone);
	WaitForWorldTime(this, Fixture, 2.2f, [this, Fixture, WeakKnight, WeakOther, WeakStone]()
	{
		if (!TestTrue(TEXT("Channel actors persist"), WeakKnight.IsValid() && WeakOther.IsValid() && WeakStone.IsValid()))
		{
			return;
		}
		TestEqual(TEXT("Success starts30s cooldown"), WeakStone->GetState(), EKnightRunestoneState::Cooldown);
		TestEqual(TEXT("Activation itself does not heal or buff party"), WeakKnight->GetKnightAttributeSet()->GetHealth(), 1000.0f);
		TArray<TWeakObjectPtr<AKnightRunestonePickup>> Pickups;
		for (TActorIterator<AKnightRunestonePickup> It(Fixture->World.Get()); It; ++It)
		{
			Pickups.Add(*It);
		}
		TestEqual(TEXT("Healing activation creates2 pickups and1 fire"), Pickups.Num(), 3);
		int32 HealingCount = 0;
		int32 FireCount = 0;
		for (const TWeakObjectPtr<AKnightRunestonePickup>& Pickup : Pickups)
		{
			WeakKnight->GetKnightAbilitySystem()->SetNumericAttributeBase(UKnightAttributeSet::GetHealthAttribute(), 1000.0f);
			WeakKnight->SetActorLocation(Pickup->GetActorLocation() + FVector(0.0, 0.0, 100.0));
			TestFalse(TEXT("Full HP leaves healing charge available"), Pickup->TryCollect(WeakKnight.Get(), FGuid::NewGuid()).IsSuccess());
			const int32 Charges = Pickup->GetRemainingCharges();
			WeakKnight->GetKnightAbilitySystem()->SetNumericAttributeBase(UKnightAttributeSet::GetHealthAttribute(), 500.0f);
			const FGuid Request = FGuid::NewGuid();
			TestTrue(TEXT("Collector receives actual health effect"), Pickup->TryCollect(WeakKnight.Get(), Request).IsSuccess());
			TestEqual(TEXT("Heal scales with recipient MaxHP15%"), WeakKnight->GetKnightAttributeSet()->GetHealth(), 650.0f);
			TestEqual(TEXT("One authoritative charge consumed"), Pickup->GetRemainingCharges(), Charges - 1);
			TestFalse(TEXT("Replay consumes no second charge"), Pickup->TryCollect(WeakKnight.Get(), Request).IsSuccess());
			WeakOther->SetActorLocation(Pickup->GetActorLocation() + FVector(0.0, 100.0, 100.0));
			WeakOther->GetKnightAbilitySystem()->SetNumericAttributeBase(UKnightAttributeSet::GetHealthAttribute(), 500.0f);
			if (Pickup->GetKind() == EKnightRunestonePickupKind::Fire)
			{
				++FireCount;
				TestTrue(TEXT("Fire second charge is available to another Knight"), Pickup->TryCollect(WeakOther.Get(), FGuid::NewGuid()).IsSuccess());
				TestEqual(TEXT("Fire grants recipient's scaled heal"), WeakOther->GetKnightAttributeSet()->GetHealth(), 650.0f);
			}
			else
			{
				++HealingCount;
				TestFalse(TEXT("Second collector loses consumed healing pickup"), Pickup->TryCollect(WeakOther.Get(), FGuid::NewGuid()).IsSuccess());
				TestEqual(TEXT("Losing collector receives no heal"), WeakOther->GetKnightAttributeSet()->GetHealth(), 500.0f);
			}
		}
		TestEqual(TEXT("Actual world result contains two healing pickup types"), HealingCount, 2);
		TestEqual(TEXT("Actual world result contains one two-charge fire"), FireCount, 1);
		WeakOther->SetActorLocation(FVector(-300.0, 300.0, 100.0));
		WeakStone->ResetFixture(FGuid::NewGuid());
		WeakKnight->SetActorLocation(WeakStone->GetActorLocation() + FVector(-120.0, 0.0, 40.0));
		TestTrue(TEXT("Retry permits reservation"), WeakStone->TryActivate(WeakKnight.Get(), FGuid::NewGuid()).IsSuccess());
		WeakKnight->SetActorLocation(FVector(-1000.0, 0.0, 100.0));
		WaitForWorldTime(this, Fixture, 0.2f, [this, Fixture, WeakKnight, WeakOther, WeakStone]()
		{
			TestEqual(TEXT("Leaving range cancels without cooldown"), WeakStone->GetState(), EKnightRunestoneState::Ready);
			WeakStone->ConfigureFixture(EKnightRunestoneKind::Crystal, FGuid::NewGuid());
			WeakKnight->SetActorLocation(WeakStone->GetActorLocation() + FVector(-120.0, 0.0, 40.0));
			TestTrue(TEXT("Crystal Runestone starts real channel"), WeakStone->TryActivate(WeakKnight.Get(), FGuid::NewGuid()).IsSuccess());
			WaitForWorldTime(this, Fixture, 2.2f, [this, Fixture, WeakKnight, WeakOther, WeakStone]()
			{
				TestEqual(TEXT("Crystal channel completes"), WeakStone->GetState(), EKnightRunestoneState::Cooldown);
				TestEqual(TEXT("Crystal activation does not directly buff Knight"), WeakKnight->GetKnightAttributeSet()->GetAttackPower(), 100.0f);
				TWeakObjectPtr<AKnightRunestonePickup> Crystal;
				int32 CrystalCount = 0;
				for (TActorIterator<AKnightRunestonePickup> It(Fixture->World.Get()); It; ++It)
				{
					if (It->GetKind() == EKnightRunestonePickupKind::PowerCrystal)
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
				WeakKnight->SetActorLocation(Crystal->GetActorLocation() + FVector(0.0, 0.0, 100.0));
				TestTrue(TEXT("Crystal applies collector-owned GE"), Crystal->TryCollect(WeakKnight.Get(), FGuid::NewGuid()).IsSuccess());
				TestEqual(TEXT("Crystal Power multiplier15%"), WeakKnight->GetKnightAttributeSet()->GetAttackPower(), 115.0f);
				WeakOther->SetActorLocation(Crystal->GetActorLocation() + FVector(0.0, 100.0, 100.0));
				TestFalse(TEXT("Different Knight loses crystal race"), Crystal->TryCollect(WeakOther.Get(), FGuid::NewGuid()).IsSuccess());
				TestEqual(TEXT("Crystal loser receives no Power buff"), WeakOther->GetKnightAttributeSet()->GetAttackPower(), 100.0f);
				WaitForWorldTime(this, Fixture, 10.2f, [this, WeakKnight]()
				{
					TestEqual(TEXT("Collector-owned crystal effect expires after10s"), WeakKnight->GetKnightAttributeSet()->GetAttackPower(), 100.0f);
				});
			});
		});
	});
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKnightSkillLabFrozenSurfaceTest, "Knight.M55.World.FrozenWarningAndSafeEjection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FKnightSkillLabFrozenSurfaceTest::RunTest(const FString& Parameters)
{
	using namespace KnightSkillLabWorldTests;
	TSharedRef<FKnightWorld> Fixture = MakeShared<FKnightWorld>();
	AKnightFrozenSurface* Surface = Fixture->World->SpawnActor<AKnightFrozenSurface>(
		FVector(0.0, 0.0, 55.0), FRotator::ZeroRotator);
	AKnightRPGCharacter* Knight = Fixture->SpawnKnight(FVector(0.0, 0.0, 150.0));
	UKnightBossArmorComponent* Armor = SpawnArmor(Fixture->World.Get(), FVector(500.0, 0.0, 100.0));
	AKnightRunestone* Stone = Fixture->World->SpawnActor<AKnightRunestone>(
		FVector(-120.0, 0.0, 60.0), FRotator::ZeroRotator);
	if (!TestNotNull(TEXT("Knight available"), Knight) || !TestNotNull(TEXT("Surface available"), Surface)
		|| !TestNotNull(TEXT("Armor available"), Armor) || !TestNotNull(TEXT("Frost Runestone available"), Stone))
	{
		return false;
	}
	Stone->ConfigureFixture(EKnightRunestoneKind::Frost, FGuid::NewGuid());
	TestTrue(TEXT("Frost Runestone channel starts"), Stone->TryActivate(Knight, FGuid::NewGuid()).IsSuccess());
	const TWeakObjectPtr<AKnightFrozenSurface> WeakSurface(Surface);
	const TWeakObjectPtr<AKnightRPGCharacter> WeakKnight(Knight);
	const TWeakObjectPtr<UKnightBossArmorComponent> WeakArmor(Armor);
	const TWeakObjectPtr<AKnightRunestone> WeakStone(Stone);
	WaitForWorldTime(this, Fixture, 2.2f, [this, Fixture, WeakSurface, WeakKnight, WeakArmor, WeakStone]()
	{
		TestEqual(TEXT("Frost activation starts cooldown"), WeakStone->GetState(), EKnightRunestoneState::Cooldown);
		TestTrue(TEXT("Runestone creates actual frozen collision state"), WeakSurface->IsFrozen());
		TestEqual(TEXT("Frost activation makes actual boss armor brittle"), WeakArmor->GetArmorState(), EKnightBossArmorState::Brittle);
		TestEqual(TEXT("Frost activation does not buff party armor"), WeakKnight->GetKnightAttributeSet()->GetDefense(), 100.0f);
		WaitForWorldTime(this, Fixture, 10.1f, [this, Fixture, WeakSurface, WeakKnight, WeakArmor]()
		{
			TestTrue(TEXT("Two-second warning begins before thaw"), WeakSurface->IsThawWarning());
			WeakKnight->SetActorLocation(FVector(0.0, 0.0, 150.0));
			WaitForWorldTime(this, Fixture, 2.1f, [this, WeakSurface, WeakKnight, WeakArmor]()
			{
				TestFalse(TEXT("Surface thaws after12s"), WeakSurface->IsFrozen());
				TestTrue(TEXT("Occupant moved safely outside disappearing collision"), WeakKnight->GetActorLocation().X > 250.0);
				TestEqual(TEXT("Brittle fixture expires to intact"), WeakArmor->GetArmorState(), EKnightBossArmorState::Intact);
			});
		});
	});
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKnightSkillLabChannelOwnershipTest,
	"Knight.M55.World.ChannelLifeOwnershipResetAndReplay",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FKnightSkillLabChannelOwnershipTest::RunTest(const FString& Parameters)
{
	using namespace KnightSkillLabWorldTests;
	TSharedRef<FKnightWorld> Fixture = MakeShared<FKnightWorld>();
	AKnightRPGCharacter* Knight = Fixture->SpawnKnight(FVector(0.0, 0.0, 100.0));
	AKnightRunestone* Stone = Fixture->World->SpawnActor<AKnightRunestone>(
		FVector(100.0, 0.0, 60.0), FRotator::ZeroRotator);
	if (!TestNotNull(TEXT("Lifecycle Knight fixture"), Knight) || !TestNotNull(TEXT("Lifecycle Runestone fixture"), Stone))
	{
		return false;
	}
	Stone->ConfigureFixture(EKnightRunestoneKind::Healing, FGuid::NewGuid());
	const TWeakObjectPtr<APlayerController> OriginalController(Cast<APlayerController>(Knight->GetController()));
	if (!TestNotNull(TEXT("Original channel controller"), OriginalController.Get()))
	{
		return false;
	}
	TestTrue(TEXT("Living Knight reserves actual channel"), Stone->TryActivate(Knight, FGuid::NewGuid()).IsSuccess());
	Knight->GetKnightAbilitySystem()->SetNumericAttributeBase(UKnightAttributeSet::GetHealthAttribute(), 0.0f);
	TestTrue(TEXT("Actual health callback transitions canonical Knight to Downed"), Knight->GetLifeComponent()->IsDowned());
	const TWeakObjectPtr<AKnightRPGCharacter> WeakKnight(Knight);
	const TWeakObjectPtr<AKnightRunestone> WeakStone(Stone);
	const FKnightEntityId Identity = Knight->GetKnightEntityId();
	const TWeakObjectPtr<UKnightAbilitySystemComponent> OriginalASC(Knight->GetKnightAbilitySystem());
	WaitForWorldTime(this, Fixture, 0.2f, [this, Fixture, WeakKnight, WeakStone, Identity, OriginalASC, OriginalController]()
	{
		TestEqual(TEXT("Downed state cancels reservation without cooldown"), WeakStone->GetState(), EKnightRunestoneState::Ready);
		TestEqual(TEXT("Canceled downed channel leaves no cooldown time"), WeakStone->GetRemainingTime(), 0.0f);
		TestTrue(TEXT("Explicit checkpoint fixture restores life"), WeakKnight->GetLifeComponent()->ResetForCheckpoint().IsSuccess());
		WeakKnight->GetCharacterMovement()->DisableMovement();
		WeakKnight->SetActorLocation(WeakStone->GetActorLocation() + FVector(-120.0, 0.0, 40.0));
		TestTrue(TEXT("Restored Knight reserves channel"), WeakStone->TryActivate(WeakKnight.Get(), FGuid::NewGuid()).IsSuccess());
		APlayerController* Replacement = Fixture->World->SpawnActor<APlayerController>();
		if (!TestNotNull(TEXT("Actual replacement controller"), Replacement))
		{
			return;
		}
		Replacement->Possess(WeakKnight.Get());
		WeakKnight->GetCharacterMovement()->DisableMovement();
		WaitForWorldTime(this, Fixture, 0.2f, [this, Fixture, WeakKnight, WeakStone, Identity, OriginalASC, OriginalController]()
		{
			TestEqual(TEXT("Changed controller cancels the old controller's reservation"), WeakStone->GetState(), EKnightRunestoneState::Ready);
			TestTrue(TEXT("Possession preserves canonical Knight identity and Character ASC"),
				WeakKnight->GetKnightEntityId() == Identity && WeakKnight->GetKnightAbilitySystem() == OriginalASC.Get());
			OriginalController->Possess(WeakKnight.Get());
			WeakKnight->GetCharacterMovement()->DisableMovement();
			TestTrue(TEXT("Original controller can start a fresh channel"), WeakStone->TryActivate(WeakKnight.Get(), FGuid::NewGuid()).IsSuccess());
			WeakStone->ResetFixture(FGuid::NewGuid());
			WaitForWorldTime(this, Fixture, 2.2f, [this, Fixture, WeakKnight, WeakStone]()
			{
				TestEqual(TEXT("Encounter reset cancels active channel and its commit timer"), WeakStone->GetState(), EKnightRunestoneState::Ready);
				int32 BeforeCount = 0;
				for (TActorIterator<AKnightRunestonePickup> It(Fixture->World.Get()); It; ++It)
				{
					++BeforeCount;
				}
				TestEqual(TEXT("Downed, takeover and reset cancellations spawn no results"), BeforeCount, 0);
				const FGuid CommittedRequest = FGuid::NewGuid();
				TestTrue(TEXT("Fresh retry after cancellation reserves channel"), WeakStone->TryActivate(WeakKnight.Get(), CommittedRequest).IsSuccess());
				WaitForWorldTime(this, Fixture, 2.2f, [this, Fixture, WeakKnight, WeakStone, CommittedRequest]()
				{
					TestEqual(TEXT("Fresh reservation commits actual world result"), WeakStone->GetState(), EKnightRunestoneState::Cooldown);
					TestFalse(TEXT("Committed request replay is rejected during cooldown"), WeakStone->TryActivate(WeakKnight.Get(), CommittedRequest).IsSuccess());
					WaitForWorldTime(this, Fixture, 30.2f, [this, Fixture, WeakKnight, WeakStone, CommittedRequest]()
					{
						TestEqual(TEXT("Actual thirty-second cooldown expires"), WeakStone->GetState(), EKnightRunestoneState::Ready);
						TestFalse(TEXT("Committed request remains rejected after cooldown"), WeakStone->TryActivate(WeakKnight.Get(), CommittedRequest).IsSuccess());
						TestEqual(TEXT("Replay cannot create a new reservation"), WeakStone->GetState(), EKnightRunestoneState::Ready);
						TestTrue(TEXT("New request is allowed after cooldown"), WeakStone->TryActivate(WeakKnight.Get(), FGuid::NewGuid()).IsSuccess());
						WeakStone->ResetFixture(FGuid::NewGuid());
					});
				});
			});
		});
	});
	return true;
}

#endif
