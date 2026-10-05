#include "Progression/KnightRPGComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Assets/KnightAssetManager.h"
#include "Attributes/KnightAttributeSet.h"
#include "Components/KnightAbilitySystemComponent.h"
#include "Components/KnightLifeComponent.h"
#include "Components/SceneComponent.h"
#include "Definitions/KnightItemDefinition.h"
#include "Definitions/KnightAbilitySetDefinition.h"
#include "Definitions/KnightRuneDefinition.h"
#include "Definitions/KnightSkillDefinition.h"
#include "Definitions/KnightSkillTreeDefinition.h"
#include "Effects/KnightSourceProjectionEffect.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/WorldSettings.h"
#include "Inventory/KnightInventoryComponent.h"
#include "Loot/KnightItemPickup.h"
#include "Misc/AutomationTest.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "Tags/KnightGameplayTags.h"
#include "UObject/Class.h"
#include "UObject/StrongObjectPtr.h"
#include <limits>

namespace KnightRPGTests
{
	struct FKnightDomains
	{
		UKnightInventoryComponent* Inventory = nullptr;
		UKnightRPGComponent* RPG = nullptr;
		TArray<FKnightRPGKnightRegistration> Registrations;
		FKnightResult InitializationResult = FKnightResult::Success();
	};

	class FKnightWorld final : public FNoncopyable
	{
	public:
		FKnightWorld()
			: GameInstance(NewObject<UGameInstance>(GEngine)), Ore(NewObject<UKnightItemDefinition>()),
			Hammer(NewObject<UKnightItemDefinition>()), Rune(NewObject<UKnightRuneDefinition>()),
			Skill(NewObject<UKnightSkillDefinition>()), BrokenSkill(NewObject<UKnightSkillDefinition>()),
			Tree(NewObject<UKnightSkillTreeDefinition>())
		{
			GameInstance->InitializeStandalone();
			World = GameInstance->GetWorld();
			World->SetShouldTick(false);
			World->GetWorldSettings()->DefaultGameMode = AGameModeBase::StaticClass();
			FURL URL;
			URL.Map = TEXT("KnightRPGDomainTest");
			URL.AddOption(TEXT("game=/Script/Engine.GameModeBase"));
			World->SetGameMode(URL);
			World->InitializeActorsForPlay(URL);
			World->BeginPlay();
			Ore->TrySetIdentity(TEXT("DomainForgeShard"), TEXT("KnightItem"));
			Ore->TrySetItemTuning(EKnightItemKind::Material, 10);
			Hammer->TrySetIdentity(TEXT("DomainHammer"), TEXT("KnightItem"));
			FKnightItemStatBonuses HammerStats;
			HammerStats.AttackPower = 10.0f;
			HammerStats.MaxHealth = 20.0f;
			Hammer->TrySetItemTuning(EKnightItemKind::Equipment, 1, FPrimaryAssetId(), HammerStats);
			Rune->TrySetIdentity(TEXT("DomainRune"), TEXT("KnightItem"));
			FKnightRuneTuning RuneTuning;
			RuneTuning.DamageScale = 1.2f;
			RuneTuning.CostScale = 0.5f;
			RuneTuning.DurationScale = 1.5f;
			Rune->TrySetRuneTuning(RuneTuning);
			Skill->TrySetIdentity(TEXT("DomainHammerForce"), TEXT("KnightSkill"));
			BrokenSkill->TrySetIdentity(TEXT("DomainMissingGrant"), TEXT("KnightSkill"));
			FKnightMaterialCost Cost;
			Cost.DefinitionId = Ore->GetPrimaryAssetId();
			Cost.Count = 2;
			FKnightItemStatBonuses SkillStats;
			SkillStats.AttackPower = 5.0f;
			Skill->TrySetSkillTuning(1, { Cost }, {}, SkillStats);
			BrokenSkill->TrySetSkillTuning(1, { Cost }, {}, SkillStats, FKnightRuneTuning(),
				FPrimaryAssetId(TEXT("KnightAbilitySet"), TEXT("MissingDomainGrant")));
			Tree->TrySetIdentity(TEXT("DomainTree"), TEXT("KnightSkillTree"));
			Tree->TrySetSkills({ Skill->GetPrimaryAssetId(), BrokenSkill->GetPrimaryAssetId() });
		}

		~FKnightWorld()
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

		FKnightDomains MakeDomains(const TArray<FKnightRPGKnightRegistration>* ExistingIds = nullptr,
			UKnightItemDefinition* ExtraItem = nullptr) const
		{
			FKnightDomains Result;
			AActor* Owner = World->SpawnActor<AActor>();
			Owner->SetReplicates(true);
			Result.Inventory = NewObject<UKnightInventoryComponent>(Owner);
			Owner->AddInstanceComponent(Result.Inventory);
			Result.Inventory->RegisterComponent();
			Result.RPG = NewObject<UKnightRPGComponent>(Owner);
			Owner->AddInstanceComponent(Result.RPG);
			Result.RPG->RegisterComponent();
			TArray<UKnightItemDefinition*> Items { Ore.Get(), Hammer.Get(), Rune.Get() };
			if (ExtraItem != nullptr)
			{
				Items.Add(ExtraItem);
			}
			Result.Inventory->InitializeFixtureCatalog(Items);
			TArray<UKnightDefinition*> Definitions { Ore.Get(), Hammer.Get(), Rune.Get(), Skill.Get(), BrokenSkill.Get(), Tree.Get() };
			if (ExtraItem != nullptr)
			{
				Definitions.Add(ExtraItem);
			}
			Result.RPG->InitializeFixtureCatalog(Definitions);
			UKnightAssetManager* Manager = UKnightAssetManager::GetKnightAssetManager();
			UKnightDefinition* AbilityDefinition = nullptr;
			Result.InitializationResult = Manager == nullptr
				? FKnightResult::Failure(EKnightError::LoadFailed, TEXT("RPGTestCombatCatalog"), FText::GetEmpty())
				: Manager->ResolveDefinition(FPrimaryAssetId(TEXT("KnightAbilitySet"), TEXT("FoundationCombat")),
					UKnightAbilitySetDefinition::StaticClass(), AbilityDefinition);
			if (!Result.InitializationResult.IsSuccess())
			{
				return Result;
			}
			const UKnightAbilitySetDefinition* CombatSet = Cast<UKnightAbilitySetDefinition>(AbilityDefinition);
			for (int32 Index = 0; Index < 3; ++Index)
			{
				AActor* Pawn = World->SpawnActor<AActor>();
				USceneComponent* Root = NewObject<USceneComponent>(Pawn);
				Pawn->AddInstanceComponent(Root);
				Pawn->SetRootComponent(Root);
				Root->RegisterComponent();
				UKnightAbilitySystemComponent* ASC = NewObject<UKnightAbilitySystemComponent>(Pawn);
				Pawn->AddInstanceComponent(ASC);
				ASC->RegisterComponent();
				UKnightAttributeSet* Attributes = NewObject<UKnightAttributeSet>(Pawn);
				UKnightLifeComponent* Life = NewObject<UKnightLifeComponent>(Pawn);
				Pawn->AddInstanceComponent(Life);
				Life->RegisterComponent();
				FKnightRPGKnightRegistration Registration;
				Registration.KnightId = ExistingIds == nullptr ? FKnightEntityId::Create() : (*ExistingIds)[Index].KnightId;
				const FName HeroNames[] { TEXT("Training_Red"), TEXT("Training_Blue"), TEXT("Training_Green") };
				Registration.HeroDefinitionId = FPrimaryAssetId(TEXT("KnightHero"), HeroNames[Index]);
				Registration.Pawn = Pawn;
				Registration.AbilitySystem = ASC;
				Result.InitializationResult = ASC->InitializeKnightActor(Pawn, Attributes, Life, Registration.KnightId);
				FKnightAbilitySetHandle CombatGrant;
				if (Result.InitializationResult.IsSuccess())
				{
					Result.InitializationResult = ASC->GrantAbilitySet(CombatSet, TEXT("RPGTestCombat"), CombatGrant);
				}
				if (!Result.InitializationResult.IsSuccess())
				{
					return Result;
				}
				Result.Registrations.Add(Registration);
			}
			return Result;
		}

		TWeakObjectPtr<UWorld> World;
		TStrongObjectPtr<UGameInstance> GameInstance;
		TStrongObjectPtr<UKnightItemDefinition> Ore;
		TStrongObjectPtr<UKnightItemDefinition> Hammer;
		TStrongObjectPtr<UKnightRuneDefinition> Rune;
		TStrongObjectPtr<UKnightSkillDefinition> Skill;
		TStrongObjectPtr<UKnightSkillDefinition> BrokenSkill;
		TStrongObjectPtr<UKnightSkillTreeDefinition> Tree;
	};

	FKnightRPGCallerContext Caller(const FKnightDomains& Domains, const FGuid& HumanId, int32 Index = 0)
	{
		FKnightRPGCallerContext Result;
		const FKnightRPGKnightRegistration& Registration = Domains.Registrations[Index];
		Result.HumanId = HumanId;
		Result.KnightId = Registration.KnightId;
		Result.HeroDefinitionId = Registration.HeroDefinitionId;
		Result.Pawn = Registration.Pawn;
		Result.AbilitySystem = Registration.AbilitySystem;
		Result.bInSafeZone = true;
		return Result;
	}

	FKnightRPGCommand Command(const FKnightDomains& Domains, int32 Index, EKnightRPGOperation Operation)
	{
		FKnightRPGCommand Result;
		Result.Operation = Operation;
		Result.RequestId = FGuid::NewGuid();
		Result.ExpectedKnightId = Domains.Registrations[Index].KnightId;
		Result.InventoryId = Domains.Inventory->GetView().InventoryId;
		Result.ExpectedInventoryRevision = Domains.Inventory->GetView().Revision;
		Result.ExpectedKnightRevision = Domains.RPG->GetKnightView(Result.ExpectedKnightId).Revision;
		return Result;
	}

	FKnightInventoryRequest Seed(UKnightInventoryComponent* Inventory,
		TArray<FKnightItemInstance> Items)
	{
		FKnightInventoryRequest Request;
		Request.RequestId = FGuid::NewGuid();
		Request.ExpectedRevision = Inventory->GetView().Revision;
		for (const FKnightItemInstance& Item : Items)
		{
			FKnightInventoryMutation Mutation;
			Mutation.Item = Item;
			Request.Mutations.Add(Mutation);
		}
		return Request;
	}

	FKnightItemInstance Item(const UKnightItemDefinition* Definition, int32 Count = 1)
	{
		FKnightItemInstance Result;
		Result.InstanceId = FGuid::NewGuid();
		Result.DefinitionId = Definition->GetPrimaryAssetId();
		Result.Count = Count;
		return Result;
	}

	TArray<FActiveGameplayEffectHandle> StatHandles(UKnightAbilitySystemComponent* ASC)
	{
		TArray<FActiveGameplayEffectHandle> Result;
		for (FActiveGameplayEffectHandle Handle : ASC->GetActiveEffects(FGameplayEffectQuery()))
		{
			const FActiveGameplayEffect* Effect = ASC->GetActiveGameplayEffect(Handle);
			if (Effect != nullptr && Effect->Spec.Def->IsA<UKnightSourceProjectionEffect>())
			{
				Result.Add(Handle);
			}
		}
		return Result;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKnightInventoryCapabilityTest, "Knight.Items.PreparedCapabilities",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FKnightInventoryCapabilityTest::RunTest(const FString& Parameters)
{
	using namespace KnightRPGTests;
	FKnightWorld Fixture;
	FKnightDomains Domains = Fixture.MakeDomains();
	if (!TestTrue(TEXT("Actual FoundationCombat fixture grants initialize"), Domains.InitializationResult.IsSuccess()))
	{
		return false;
	}
	TestTrue(TEXT("Fresh inventory initializes"), Domains.Inventory->InitializeInventory(4).IsSuccess());
	const FKnightInventoryView Original = Domains.Inventory->GetView();
	FKnightInventoryCapability Read;
	TestTrue(TEXT("Read reserve owns exact revision"), Domains.Inventory->ReserveRead(
		Original.InventoryId, Original.Revision, Read).IsSuccess());
	const FKnightInventoryRequest Request = Seed(Domains.Inventory, { Item(Fixture.Ore.Get(), 2) });
	TestEqual(TEXT("All direct writes reject a read reservation"),
		Domains.Inventory->TryApplyTransaction(Request).Result.Error, EKnightError::Conflict);
	TestEqual(TEXT("Default fabricated capability rejected"),
		Domains.Inventory->CommitPrepared(FKnightInventoryCapability()).Error, EKnightError::NotOwned);
	FKnightDomains Other = Fixture.MakeDomains();
	TestEqual(TEXT("Capability cannot cross inventory owner"), Other.Inventory->AbortPrepared(Read).Error, EKnightError::NotOwned);
	TestTrue(TEXT("Read abort leaves exact committed values unchanged"),
		Domains.Inventory->AbortPrepared(Read).IsSuccess() && Domains.Inventory->GetView() == Original);
	FKnightInventoryCapability Prepared;
	TestTrue(TEXT("Write validates private candidate"), Domains.Inventory->PrepareTransaction(Request, Prepared).IsSuccess());
	TestTrue(TEXT("Preparation never changes public values"), Domains.Inventory->GetView() == Original);
	FKnightInventoryView Candidate;
	TestTrue(TEXT("Exact capability reads private bounded candidate"),
		Domains.Inventory->GetPreparedView(Prepared, Candidate).IsSuccess() && Candidate.Items.Num() == 1);
	TestTrue(TEXT("Commit makes complete state visible before publication"),
		Domains.Inventory->CommitPrepared(Prepared).IsSuccess() && Domains.Inventory->GetView() == Candidate);
	int32 Callbacks = 0;
	const FDelegateHandle Handle = Domains.Inventory->OnSnapshotChanged.AddLambda(
		[this, &Domains, &Callbacks, &Request, &Prepared]()
		{
			++Callbacks;
			TestEqual(TEXT("Publication still rejects direct request reentry"),
				Domains.Inventory->TryApplyTransaction(Request).Result.Error, EKnightError::Conflict);
			TestEqual(TEXT("Retained exact capability cannot recursively publish"),
				Domains.Inventory->PublishPrepared(Prepared, false).Error, EKnightError::Conflict);
		});
	TestTrue(TEXT("Complete reserved state publishes"), Domains.Inventory->PublishPrepared(Prepared).IsSuccess());
	Domains.Inventory->OnSnapshotChanged.Remove(Handle);
	TestEqual(TEXT("One complete publication"), Callbacks, 1);
	TestFalse(TEXT("Reservation released symmetrically"), Domains.Inventory->IsReserved());
	TestEqual(TEXT("Expired token never affects a later operation"),
		Domains.Inventory->AbortPrepared(Prepared).Error, EKnightError::NotOwned);
	TestTrue(TEXT("Retained-publication fixture initializes"), Other.Inventory->InitializeInventory(4).IsSuccess());
	FKnightInventoryCapability Retained;
	TestTrue(TEXT("Retained publication stages and commits a genuine candidate"),
		Other.Inventory->PrepareTransaction(Seed(Other.Inventory, { Item(Fixture.Ore.Get()) }), Retained).IsSuccess()
		&& Other.Inventory->CommitPrepared(Retained).IsSuccess());
	int32 RetainedCallbacks = 0;
	const FDelegateHandle RetainedHandle = Other.Inventory->OnSnapshotChanged.AddLambda([&RetainedCallbacks]()
		{
			++RetainedCallbacks;
		});
	TestTrue(TEXT("Coordinator may retain its reservation through further domain publication"),
		Other.Inventory->PublishPrepared(Retained, false).IsSuccess());
	TestEqual(TEXT("Retained exact capability cannot duplicate publication"),
		Other.Inventory->PublishPrepared(Retained, false).Error, EKnightError::Conflict);
	TestEqual(TEXT("Retained committed state notifies once"), RetainedCallbacks, 1);
	Other.Inventory->OnSnapshotChanged.Remove(RetainedHandle);
	Other.Inventory->GetOwner()->Destroy();
	TestFalse(TEXT("Owner EndPlay releases a retained reservation"), Other.Inventory->IsReserved());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKnightRPGAtomicTest, "Knight.Items.RPG.AtomicProjectionAndReplay",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FKnightRPGAtomicTest::RunTest(const FString& Parameters)
{
	using namespace KnightRPGTests;
	FKnightWorld Fixture;
	FKnightDomains Domains = Fixture.MakeDomains();
	if (!TestTrue(TEXT("Actual FoundationCombat fixture grants initialize"), Domains.InitializationResult.IsSuccess()))
	{
		return false;
	}
	Domains.Inventory->InitializeInventory(4);
	const FKnightItemInstance Hammer = Item(Fixture.Hammer.Get());
	Domains.Inventory->TryApplyTransaction(Seed(Domains.Inventory, { Hammer, Item(Fixture.Ore.Get(), 2) }));
	if (!TestTrue(TEXT("Actual graph and three ASC registrations initialize"), Domains.RPG->InitializeRPG(
		Domains.Inventory, Fixture.Tree->GetPrimaryAssetId(), Domains.Registrations).IsSuccess()))
	{
		return false;
	}
	const FGuid Human = FGuid::NewGuid();
	const FKnightRPGCallerContext Owner = Caller(Domains, Human);
	FKnightRPGCommand Equip = Command(Domains, 0, EKnightRPGOperation::Equip);
	Equip.ItemInstanceId = Hammer.InstanceId;
	Equip.SlotTag = KnightGameplayTags::Slot_Equipment_Weapon;
	TestTrue(TEXT("Real source-owned passive equips"), Domains.RPG->ExecuteCommand(Owner, Equip).Result.IsSuccess());
	UKnightAbilitySystemComponent* ASC = Owner.AbilitySystem.Get();
	TestEqual(TEXT("GAS attack stat changed"), ASC->GetKnightAttributes()->GetAttackPower(), 35.0f);
	TestEqual(TEXT("Sibling Knight retains its own ASC baseline"),
		Domains.Registrations[1].AbilitySystem->GetKnightAttributes()->GetAttackPower(), 25.0f);
	const TArray<FActiveGameplayEffectHandle> OldHandles = StatHandles(ASC);
	if (!TestEqual(TEXT("One actual passive owned handle"), OldHandles.Num(), 1))
	{
		return false;
	}
	const FKnightRPGView Before = Domains.RPG->GetView();
	const FKnightInventoryView InventoryBefore = Domains.Inventory->GetView();
	FKnightRPGCommand Broken = Command(Domains, 0, EKnightRPGOperation::PurchaseSkill);
	Broken.SkillDefinitionId = Fixture.BrokenSkill->GetPrimaryAssetId();
	TestFalse(TEXT("Missing actual GAS grant fails after complete candidate cost validation"),
		Domains.RPG->ExecuteCommand(Owner, Broken).Result.IsSuccess());
	TestTrue(TEXT("Missing grant keeps inventory, points, journal revision and old exact GE handle"),
		Domains.RPG->GetView() == Before && Domains.Inventory->GetView() == InventoryBefore
		&& ASC->GetActiveGameplayEffect(OldHandles[0]) != nullptr && StatHandles(ASC) == OldHandles);
	FKnightRPGCommand Purchase = Command(Domains, 0, EKnightRPGOperation::PurchaseSkill);
	Purchase.SkillDefinitionId = Fixture.Skill->GetPrimaryAssetId();
	FGameplayEffectApplicationQuery Reject = FGameplayEffectApplicationQuery::CreateLambda(
		[](const FActiveGameplayEffectsContainer&, const FGameplayEffectSpec& Spec)
		{
			return !Spec.Def->IsA<UKnightSourceProjectionEffect>();
		});
	const FDelegateHandle RejectHandle = Reject.GetHandle();
	ASC->GameplayEffectApplicationQueries.Add(MoveTemp(Reject));
	TestEqual(TEXT("Actual GE stage rejection reports typed failure"),
		Domains.RPG->ExecuteCommand(Owner, Purchase).Result.Error, EKnightError::InvalidState);
	ASC->GameplayEffectApplicationQueries.RemoveAll([RejectHandle](const FGameplayEffectApplicationQuery& Query)
		{
			return Query.GetHandle() == RejectHandle;
		});
	TestTrue(TEXT("Failed GE stage rolls back both exact domains and retains the same old effect"),
		Domains.RPG->GetView() == Before && Domains.Inventory->GetView() == InventoryBefore
		&& StatHandles(ASC) == OldHandles && ASC->GetKnightAttributes()->GetAttackPower() == 35.0f);
	int32 RawCallbacks = 0;
	int32 Publications = 0;
	const FDelegateHandle InventoryPublished = Domains.Inventory->OnSnapshotChanged.AddLambda(
		[this, &Domains, ASC, &OldHandles]()
		{
			TestEqual(TEXT("Inventory publication sees the promoted GAS projection"),
				ASC->GetSourceProjection(KnightRPG::ProjectionSource).Revision, static_cast<int64>(2));
			TestEqual(TEXT("Inventory subscriber sees final stat after exact retired-effect cleanup"),
				ASC->GetKnightAttributes()->GetAttackPower(), 40.0f);
			TestTrue(TEXT("Old exact passive is removed before inventory publication"),
				ASC->GetActiveGameplayEffect(OldHandles[0]) == nullptr && StatHandles(ASC).Num() == 1);
			TestTrue(TEXT("Cross-domain observers remain protected by both reservations"),
				ASC->IsMutationFenced() && Domains.Inventory->IsReserved());
		});
	const FDelegateHandle Published = Domains.RPG->OnSnapshotChanged.AddLambda(
		[this, &Domains, &Owner, &Purchase, &Publications]()
		{
			++Publications;
			TestEqual(TEXT("Domain subscriber sees committed exact inventory revision"),
				Domains.RPG->GetView().InventoryRevision, Domains.Inventory->GetView().Revision);
			TestEqual(TEXT("RPG subscriber command reentry remains fenced"),
				Domains.RPG->ExecuteCommand(Owner, Purchase).Result.Error, EKnightError::Conflict);
			TestEqual(TEXT("Direct inventory callback mutation remains reserved through RPG publication"),
				Domains.Inventory->TryApplyTransaction(Seed(Domains.Inventory, {})).Result.Error, EKnightError::Conflict);
		});
	const FDelegateHandle Raw = ASC->OnActiveGameplayEffectAddedDelegateToSelf.AddLambda(
		[this, &Domains, &Owner, &Purchase, &RawCallbacks](UAbilitySystemComponent*, const FGameplayEffectSpec& Spec,
			FActiveGameplayEffectHandle)
		{
			if (Spec.Def->IsA<UKnightSourceProjectionEffect>())
			{
				++RawCallbacks;
				TestEqual(TEXT("Raw GAS callback reentry cannot mutate coordinated domains"),
					Domains.RPG->ExecuteCommand(Owner, Purchase).Result.Error, EKnightError::Conflict);
			}
		});
	const FKnightRPGCommandResult Committed = Domains.RPG->ExecuteCommand(Owner, Purchase);
	ASC->OnActiveGameplayEffectAddedDelegateToSelf.Remove(Raw);
	Domains.RPG->OnSnapshotChanged.Remove(Published);
	Domains.Inventory->OnSnapshotChanged.Remove(InventoryPublished);
	TestTrue(TEXT("Real atomic paid skill commits after stage succeeds"), Committed.Result.IsSuccess());
	TestEqual(TEXT("Actual candidate effect callback observed"), RawCallbacks, 1);
	TestEqual(TEXT("One complete domain publication"), Publications, 1);
	TestEqual(TEXT("Result correlates exact committed Knight revision"),
		Committed.CommittedKnightRevision, Domains.RPG->GetKnightView(Owner.KnightId).Revision);
	TestEqual(TEXT("Real aggregate bonus is equipment plus skill"), ASC->GetKnightAttributes()->GetAttackPower(), 40.0f);
	const FKnightRPGView After = Domains.RPG->GetView();
	TestTrue(TEXT("Original human can retrieve exact result after actual context changes Knight"),
		Domains.RPG->ExecuteCommand(Caller(Domains, Human, 1), Purchase).bWasReplay && Domains.RPG->GetView() == After);
	FKnightRPGCommand Altered = Purchase;
	Altered.SkillDefinitionId = Fixture.BrokenSkill->GetPrimaryAssetId();
	TestEqual(TEXT("Changed payload cannot reuse successful GUID"),
		Domains.RPG->ExecuteCommand(Owner, Altered).Result.Error, EKnightError::Conflict);
	TestEqual(TEXT("Another authenticated human cannot retrieve another human receipt"),
		Domains.RPG->ExecuteCommand(Caller(Domains, FGuid::NewGuid()), Purchase).Result.Error, EKnightError::Conflict);
	FKnightRPGCommand Stale = Purchase;
	Stale.RequestId = FGuid::NewGuid();
	TestEqual(TEXT("New stale revision rejects before effects or cost"),
		Domains.RPG->ExecuteCommand(Owner, Stale).Result.Error, EKnightError::InvalidRevision);
	TestFalse(TEXT("All exact reservations and fences cleaned"), ASC->IsMutationFenced() || Domains.Inventory->IsReserved());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKnightRPGSnapshotTest, "Knight.Items.RPG.PaidRefundAndReconstruction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FKnightRPGSnapshotTest::RunTest(const FString& Parameters)
{
	using namespace KnightRPGTests;
	FKnightWorld Fixture;
	FKnightDomains Domains = Fixture.MakeDomains();
	if (!TestTrue(TEXT("Existing real Rune ability is granted once before rune projection"),
		Domains.InitializationResult.IsSuccess()))
	{
		return false;
	}
	Domains.Inventory->InitializeInventory(4);
	const FKnightItemInstance Ore = Item(Fixture.Ore.Get(), 2);
	const FKnightItemInstance Hammer = Item(Fixture.Hammer.Get());
	const FKnightItemInstance Rune = Item(Fixture.Rune.Get());
	Domains.Inventory->TryApplyTransaction(Seed(Domains.Inventory, { Ore, Hammer, Rune }));
	Domains.RPG->InitializeRPG(Domains.Inventory, Fixture.Tree->GetPrimaryAssetId(), Domains.Registrations);
	const FKnightRPGCallerContext Owner = Caller(Domains, FGuid::NewGuid());
	FKnightRPGCommand Equip = Command(Domains, 0, EKnightRPGOperation::Equip);
	Equip.ItemInstanceId = Hammer.InstanceId;
	Equip.SlotTag = KnightGameplayTags::Slot_Equipment_Weapon;
	TestTrue(TEXT("Weapon equips before reconstruction"), Domains.RPG->ExecuteCommand(Owner, Equip).Result.IsSuccess());
	FKnightRPGCommand OtherEquip = Command(Domains, 1, EKnightRPGOperation::Equip);
	OtherEquip.ItemInstanceId = Hammer.InstanceId;
	OtherEquip.SlotTag = KnightGameplayTags::Slot_Equipment_Weapon;
	TestEqual(TEXT("Shared assignment cannot grant the same instance to another Knight"),
		Domains.RPG->ExecuteCommand(Caller(Domains, FGuid::NewGuid(), 1), OtherEquip).Result.Error, EKnightError::Conflict);
	FKnightInventoryRequest RemoveAssigned = Seed(Domains.Inventory, { Hammer });
	RemoveAssigned.Mutations[0].Operation = EKnightInventoryOperation::Remove;
	TestEqual(TEXT("Even internal inventory cost cannot remove assigned equipment"),
		Domains.Inventory->TryApplyTransaction(RemoveAssigned).Result.Error, EKnightError::NotOwned);
	FKnightRPGCommand EquipRune = Command(Domains, 0, EKnightRPGOperation::Rune);
	EquipRune.ItemInstanceId = Rune.InstanceId;
	EquipRune.SlotTag = KnightGameplayTags::Slot_Equipment_Rune;
	TestTrue(TEXT("Rune tunes an existing action through a real projection"),
		Domains.RPG->ExecuteCommand(Owner, EquipRune).Result.IsSuccess());
	TestEqual(TEXT("Rune cost modifier is committed"),
		Owner.AbilitySystem->GetActionModifier(KnightGameplayTags::Ability_Combat_Rune).CostScale, 0.5f);
	FKnightRPGCommand Purchase = Command(Domains, 0, EKnightRPGOperation::PurchaseSkill);
	Purchase.SkillDefinitionId = Fixture.Skill->GetPrimaryAssetId();
	TestTrue(TEXT("Skill consumes an entire exact paid material stack"),
		Domains.RPG->ExecuteCommand(Owner, Purchase).Result.IsSuccess());
	TestFalse(TEXT("Paid stack retires its GUID"), Domains.Inventory->GetView().Items.Contains(Ore));
	FKnightRPGCommand Respec = Command(Domains, 0, EKnightRPGOperation::Respec);
	TestTrue(TEXT("Actual retained point and material costs refunded atomically"),
		Domains.RPG->ExecuteCommand(Owner, Respec).Result.IsSuccess());
	const FKnightInventoryView Refunded = Domains.Inventory->GetView();
	const FKnightItemInstance* RefundedOre = Refunded.Items.FindByPredicate([&Ore](const FKnightItemInstance& Item)
		{
			return Item.DefinitionId == Ore.DefinitionId;
		});
	TestTrue(TEXT("Retired cost GUID is never reused by refund"),
		RefundedOre != nullptr && RefundedOre->Count == Ore.Count && RefundedOre->InstanceId != Ore.InstanceId);
	TestEqual(TEXT("Paid point balance restored"), Domains.RPG->GetKnightView(Owner.KnightId).SkillPoints, 2);
	FKnightRPGSnapshot Snapshot;
	if (!TestTrue(TEXT("Complete identity journal capture validates"), Domains.RPG->CaptureSnapshot(Snapshot).IsSuccess()))
	{
		return false;
	}
	TArray<uint8> Bytes;
	{
		FMemoryWriter Writer(Bytes);
		FObjectAndNameAsStringProxyArchive Archive(Writer, false);
		Archive.ArIsSaveGame = true;
		FKnightRPGSnapshot::StaticStruct()->SerializeItem(Archive, &Snapshot, nullptr);
		TestFalse(TEXT("ID-only snapshot serializes"), Archive.IsError());
	}
	FKnightRPGSnapshot Decoded;
	{
		FMemoryReader Reader(Bytes);
		FObjectAndNameAsStringProxyArchive Archive(Reader, false);
		Archive.ArIsSaveGame = true;
		FKnightRPGSnapshot::StaticStruct()->SerializeItem(Archive, &Decoded, nullptr);
		TestFalse(TEXT("ID-only snapshot deserializes"), Archive.IsError());
	}
	TestTrue(TEXT("Serialization keeps every durable identity and paid receipt"), Decoded == Snapshot);
	FKnightDomains Fresh = Fixture.MakeDomains(&Domains.Registrations);
	if (!TestTrue(TEXT("Fresh domains reconstruct verified journal and stage three new sources once"),
		Fresh.InitializationResult.IsSuccess()
		&& Fresh.RPG->TryRestoreSnapshot(Fresh.Inventory, Decoded, Fresh.Registrations).IsSuccess()))
	{
		return false;
	}
	TestTrue(TEXT("Reconstruction keeps exact inventory IDs and per-Knight values"),
		Fresh.Inventory->GetView() == Refunded && Fresh.RPG->GetView() == Domains.RPG->GetView());
	TestEqual(TEXT("Fresh exact source applied once"), StatHandles(Fresh.Registrations[0].AbilitySystem.Get()).Num(), 1);
	TestEqual(TEXT("Only assigned canonical Knight gets restored stats"),
		Fresh.Registrations[1].AbilitySystem->GetKnightAttributes()->GetAttackPower(), 25.0f);
	TestTrue(TEXT("Persisted successful request replays without requiring its retired pickup or cost stack"),
		Fresh.RPG->ExecuteCommand(Caller(Fresh, Owner.HumanId), Respec).bWasReplay);
	if (!TestTrue(TEXT("Corruption fixtures have genuine committed receipts and live item instances"),
		Snapshot.Receipts.Num() >= 2 && !Snapshot.Inventory.Items.IsEmpty()))
	{
		return false;
	}
	for (int32 Corruption = 0; Corruption < 5; ++Corruption)
	{
		FKnightRPGSnapshot Corrupt = Snapshot;
		if (Corruption == 0)
		{
			++Corrupt.SchemaVersion;
		}
		else if (Corruption == 1)
		{
			Corrupt.Receipts[1].Command.RequestId = Corrupt.Receipts[0].Command.RequestId;
		}
		else if (Corruption == 2)
		{
			FKnightRPGKnightView* Equipped = Corrupt.View.Knights.FindByPredicate([&Owner](const FKnightRPGKnightView& Knight)
				{
					return Knight.KnightId == Owner.KnightId;
				});
			if (!TestTrue(TEXT("Corruption fixture has two actual committed slot assignments"),
				Equipped != nullptr && Equipped->Assignments.Num() == 2))
			{
				return false;
			}
			Equipped->Assignments[1].ItemInstanceId = Equipped->Assignments[0].ItemInstanceId;
		}
		else if (Corruption == 3)
		{
			Corrupt.Receipts.Last().CommittedKnight.SkillPoints += 1;
		}
		else
		{
			Corrupt.Inventory.Items[0].DefinitionId = FPrimaryAssetId(TEXT("KnightItem"), TEXT("MissingItem"));
		}
		FKnightDomains Rejected = Fixture.MakeDomains(&Domains.Registrations);
		if (!TestTrue(TEXT("Corruption fixture retains actual combat prerequisites"),
			Rejected.InitializationResult.IsSuccess()))
		{
			return false;
		}
		TestFalse(TEXT("Corrupt fragment cannot reconstruct"),
			Rejected.RPG->TryRestoreSnapshot(Rejected.Inventory, Corrupt, Rejected.Registrations).IsSuccess());
		TestFalse(TEXT("Corrupt reconstruction leaves both domains fresh"),
			Rejected.Inventory->IsInitialized() || Rejected.RPG->IsInitialized());
		TestTrue(TEXT("Rejected fragment creates no source effects"),
			StatHandles(Rejected.Registrations[0].AbilitySystem.Get()).IsEmpty());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKnightRPGDefinitionTest, "Knight.Items.RPG.RuneAndSkillGraphValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FKnightRPGDefinitionTest::RunTest(const FString& Parameters)
{
	using namespace KnightRPGTests;
	FKnightWorld Fixture;
	FKnightRuneTuning Tuning = Fixture.Rune->GetRuneTuning();
	const FKnightRuneTuning Original = Tuning;
	Tuning.CostScale = std::numeric_limits<float>::quiet_NaN();
	TestEqual(TEXT("NaN rune data rejected"), Fixture.Rune->TrySetRuneTuning(Tuning).Error, EKnightError::InvalidData);
	TestTrue(TEXT("Failed rune authoring preserves exact tuning"), Fixture.Rune->GetRuneTuning() == Original);
	TArray<const UKnightSkillDefinition*> Nodes { Fixture.Skill.Get(), Fixture.BrokenSkill.Get() };
	TestTrue(TEXT("Actual typed complete graph validates"), Fixture.Tree->ValidateGraph(Nodes).IsSuccess());
	Fixture.Skill->TrySetSkillTuning(1, {}, { Fixture.BrokenSkill->GetPrimaryAssetId() }, FKnightItemStatBonuses());
	Fixture.BrokenSkill->TrySetSkillTuning(1, {}, { Fixture.Skill->GetPrimaryAssetId() }, FKnightItemStatBonuses());
	TestEqual(TEXT("Actual two-node dependency cycle rejected"), Fixture.Tree->ValidateGraph(Nodes).Error,
		EKnightError::InvalidData);
	Fixture.Skill->TrySetSkillTuning(1, {}, { FPrimaryAssetId(TEXT("KnightSkill"), TEXT("MissingPrerequisite")) },
		FKnightItemStatBonuses());
	TestEqual(TEXT("Unknown prerequisite cannot masquerade as a valid graph node"), Fixture.Tree->ValidateGraph(Nodes).Error,
		EKnightError::InvalidData);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKnightRPGRestoreStageTest, "Knight.Items.RPG.AllParticipantStageValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FKnightRPGRestoreStageTest::RunTest(const FString& Parameters)
{
	using namespace KnightRPGTests;
	FKnightWorld Fixture;
	FKnightDomains Domains = Fixture.MakeDomains();
	if (!TestTrue(TEXT("Genuine combat grants initialize the three source owners"),
		Domains.InitializationResult.IsSuccess()))
	{
		return false;
	}
	Domains.Inventory->InitializeInventory(4);
	TArray<FKnightItemInstance> Hammers;
	for (int32 Index = 0; Index < 3; ++Index)
	{
		Hammers.Add(Item(Fixture.Hammer.Get()));
	}
	if (!TestTrue(TEXT("Three distinct genuine equipment instances seed the shared domain"),
		Domains.Inventory->TryApplyTransaction(Seed(Domains.Inventory, Hammers)).Result.IsSuccess()
		&& Domains.RPG->InitializeRPG(Domains.Inventory, Fixture.Tree->GetPrimaryAssetId(),
			Domains.Registrations).IsSuccess()))
	{
		return false;
	}
	for (int32 Index = 0; Index < 3; ++Index)
	{
		FKnightRPGCommand Equip = Command(Domains, Index, EKnightRPGOperation::Equip);
		Equip.ItemInstanceId = Hammers[Index].InstanceId;
		Equip.SlotTag = KnightGameplayTags::Slot_Equipment_Weapon;
		if (!TestTrue(TEXT("Each canonical Knight commits its own real stat source"),
			Domains.RPG->ExecuteCommand(Caller(Domains, FGuid::NewGuid(), Index), Equip).Result.IsSuccess()))
		{
			return false;
		}
	}
	FKnightRPGSnapshot Snapshot;
	if (!TestTrue(TEXT("Complete three-source snapshot validates"), Domains.RPG->CaptureSnapshot(Snapshot).IsSuccess()))
	{
		return false;
	}
	FKnightDomains Fresh = Fixture.MakeDomains(&Domains.Registrations);
	if (!TestTrue(TEXT("Fresh reconstruction owners hold the original genuine combat specs"),
		Fresh.InitializationResult.IsSuccess()))
	{
		return false;
	}
	TArray<int32> InitialSpecCounts;
	TArray<int32> InitialGrantCounts;
	for (const FKnightRPGKnightRegistration& Registration : Fresh.Registrations)
	{
		InitialSpecCounts.Add(Registration.AbilitySystem->GetActivatableAbilities().Num());
		InitialGrantCounts.Add(Registration.AbilitySystem->GetOwnedGrantCount());
	}
	UKnightAbilitySystemComponent* FirstASC = Fresh.Registrations[0].AbilitySystem.Get();
	UKnightAbilitySystemComponent* ThirdASC = Fresh.Registrations[2].AbilitySystem.Get();
	int32 Invalidations = 0;
	const FDelegateHandle Fault = ThirdASC->OnActiveGameplayEffectAddedDelegateToSelf.AddLambda(
		[this, FirstASC, &Invalidations](UAbilitySystemComponent*, const FGameplayEffectSpec& Spec,
			FActiveGameplayEffectHandle)
		{
			if (!Spec.Def->IsA<UKnightSourceProjectionEffect>())
			{
				return;
			}
			const TArray<FActiveGameplayEffectHandle> EarlierHandles = StatHandles(FirstASC);
			if (TestEqual(TEXT("Third real Stage observes an earlier staged SDK effect"), EarlierHandles.Num(), 1))
			{
				FirstASC->RemoveActiveGameplayEffect(EarlierHandles[0]);
				++Invalidations;
			}
		});
	const FKnightResult Failed = Fresh.RPG->TryRestoreSnapshot(Fresh.Inventory, Snapshot, Fresh.Registrations);
	ThirdASC->OnActiveGameplayEffectAddedDelegateToSelf.Remove(Fault);
	TestEqual(TEXT("Removed earlier candidate rejects before any participant commits"),
		Failed.Error, EKnightError::InvalidState);
	TestEqual(TEXT("Real third-Stage cross-owner fault was executed once"), Invalidations, 1);
	TestFalse(TEXT("Failed reconstruction never initializes either value domain"),
		Fresh.Inventory->IsInitialized() || Fresh.RPG->IsInitialized());
	for (int32 Index = 0; Index < Fresh.Registrations.Num(); ++Index)
	{
		UKnightAbilitySystemComponent* ASC = Fresh.Registrations[Index].AbilitySystem.Get();
		TestEqual(TEXT("Every source retains its original uncommitted revision"),
			ASC->GetSourceProjection(KnightRPG::ProjectionSource).Revision, static_cast<int64>(0));
		TestTrue(TEXT("Every real staged effect is removed on abort"), StatHandles(ASC).IsEmpty());
		TestEqual(TEXT("Every baseline attack stat is restored"), ASC->GetKnightAttributes()->GetAttackPower(), 25.0f);
		TestEqual(TEXT("Every baseline maximum health is restored"), ASC->GetKnightAttributes()->GetMaxHealth(), 100.0f);
		TestEqual(TEXT("Original spec set remains unchanged"), ASC->GetActivatableAbilities().Num(), InitialSpecCounts[Index]);
		TestEqual(TEXT("Original exact grant count remains unchanged"), ASC->GetOwnedGrantCount(), InitialGrantCounts[Index]);
		TestFalse(TEXT("Every staged fence is released"), ASC->IsMutationFenced());
	}
	TestFalse(TEXT("Inventory and coordinator release their reservations"),
		Fresh.Inventory->IsReserved() || Fresh.RPG->IsTransactionInProgress());
	FKnightDomains Ending = Fixture.MakeDomains(&Domains.Registrations);
	if (!TestTrue(TEXT("Owner-end reconstruction fixture holds genuine combat grants"),
		Ending.InitializationResult.IsSuccess()))
	{
		return false;
	}
	int32 SourcePublications = 0;
	int32 InventoryPublications = 0;
	int32 RPGPublications = 0;
	TArray<FDelegateHandle> SourceObservers;
	for (int32 Index = 0; Index < Ending.Registrations.Num(); ++Index)
	{
		SourceObservers.Add(Ending.Registrations[Index].AbilitySystem->OnSourceProjectionChanged.AddLambda(
			[this, &Ending, Index, &SourcePublications](const FKnightSourceProjection&)
			{
				++SourcePublications;
				TestEqual(TEXT("Only the first participant publishes before shared owner teardown"), Index, 0);
				for (const FKnightRPGKnightRegistration& Registration : Ending.Registrations)
				{
					TestEqual(TEXT("First subscriber sees every participant's already committed value"),
						Registration.AbilitySystem->GetSourceProjection(KnightRPG::ProjectionSource).Revision,
						static_cast<int64>(1));
				}
				TestTrue(TEXT("Subscriber actually ends the shared owner"), Ending.RPG->GetOwner()->Destroy());
			}));
	}
	const FDelegateHandle InventoryObserver = Ending.Inventory->OnSnapshotChanged.AddLambda([&InventoryPublications]()
		{
			++InventoryPublications;
		});
	const FDelegateHandle RPGObserver = Ending.RPG->OnSnapshotChanged.AddLambda([&RPGPublications]()
		{
			++RPGPublications;
		});
	TestTrue(TEXT("Committed reconstruction returns success after shared-owner publication teardown"),
		Ending.RPG->TryRestoreSnapshot(Ending.Inventory, Snapshot, Ending.Registrations).IsSuccess());
	for (int32 Index = 0; Index < Ending.Registrations.Num(); ++Index)
	{
		UKnightAbilitySystemComponent* ASC = Ending.Registrations[Index].AbilitySystem.Get();
		ASC->OnSourceProjectionChanged.Remove(SourceObservers[Index]);
		TestFalse(TEXT("All surviving canonical ASCs silently finish committed fences after owner end"),
			ASC->IsMutationFenced());
		TestEqual(TEXT("Owner end cannot undo an already committed canonical projection"),
			ASC->GetSourceProjection(KnightRPG::ProjectionSource).Revision, static_cast<int64>(1));
		TestEqual(TEXT("Surviving canonical ASC retains one committed stat effect"), StatHandles(ASC).Num(), 1);
	}
	Ending.Inventory->OnSnapshotChanged.Remove(InventoryObserver);
	Ending.RPG->OnSnapshotChanged.Remove(RPGObserver);
	TestEqual(TEXT("No later source subscribers run after shared owner end"), SourcePublications, 1);
	TestEqual(TEXT("No inventory publication runs after shared owner end"), InventoryPublications, 0);
	TestEqual(TEXT("No RPG publication runs after shared owner end"), RPGPublications, 0);
	TestTrue(TEXT("Committed value receipt remains the exact reconstruction result"), Ending.RPG->GetView() == Snapshot.View);
	TestFalse(TEXT("Ended shared owner releases all coordinator reservations"),
		Ending.Inventory->IsReserved() || Ending.RPG->IsTransactionInProgress());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKnightRPGPublicationLifetimeTest, "Knight.Items.RPG.PublicationLifetime",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FKnightRPGPublicationLifetimeTest::RunTest(const FString& Parameters)
{
	using namespace KnightRPGTests;
	FKnightWorld Fixture;
	for (int32 Case = 0; Case < 2; ++Case)
	{
		FKnightDomains Domains = Fixture.MakeDomains();
		if (!TestTrue(TEXT("Lifetime fixture initializes actual combat grants"), Domains.InitializationResult.IsSuccess()))
		{
			return false;
		}
		Domains.Inventory->InitializeInventory(4);
		const FKnightItemInstance Hammer = Item(Fixture.Hammer.Get());
		Domains.Inventory->TryApplyTransaction(Seed(Domains.Inventory, { Hammer, Item(Fixture.Ore.Get(), 2) }));
		if (!TestTrue(TEXT("Lifetime fixture initializes canonical value domains"), Domains.RPG->InitializeRPG(
			Domains.Inventory, Fixture.Tree->GetPrimaryAssetId(), Domains.Registrations).IsSuccess()))
		{
			return false;
		}
		const FKnightRPGCallerContext Owner = Caller(Domains, FGuid::NewGuid());
		UKnightAbilitySystemComponent* ASC = Owner.AbilitySystem.Get();
		FKnightRPGCommand Equip = Command(Domains, 0, EKnightRPGOperation::Equip);
		Equip.ItemInstanceId = Hammer.InstanceId;
		Equip.SlotTag = KnightGameplayTags::Slot_Equipment_Weapon;
		if (!TestTrue(TEXT("Lifetime fixture commits its original source"),
			Domains.RPG->ExecuteCommand(Owner, Equip).Result.IsSuccess()))
		{
			return false;
		}
		const TArray<FActiveGameplayEffectHandle> OldHandles = StatHandles(ASC);
		if (!TestEqual(TEXT("Lifetime fixture has one exact old passive"), OldHandles.Num(), 1))
		{
			return false;
		}
		const FKnightRPGView Before = Domains.RPG->GetView();
		const FKnightInventoryView InventoryBefore = Domains.Inventory->GetView();
		FKnightRPGCommand Purchase = Command(Domains, 0, EKnightRPGOperation::PurchaseSkill);
		Purchase.SkillDefinitionId = Fixture.Skill->GetPrimaryAssetId();
		int32 OwnerEndCallbacks = 0;
		int32 ProjectPublications = 0;
		const FDelegateHandle SourceObserver = ASC->OnSourceProjectionChanged.AddLambda(
			[&ProjectPublications](const FKnightSourceProjection&)
			{
				++ProjectPublications;
			});
		const FDelegateHandle InventoryObserver = Domains.Inventory->OnSnapshotChanged.AddLambda([&ProjectPublications]()
			{
				++ProjectPublications;
			});
		const FDelegateHandle RPGObserver = Domains.RPG->OnSnapshotChanged.AddLambda([&ProjectPublications]()
			{
				++ProjectPublications;
			});
		const FDelegateHandle CommandObserver = Domains.RPG->OnCommandCompleted.AddLambda(
			[&ProjectPublications](const FKnightRPGCommandResult&)
			{
				++ProjectPublications;
			});
		FDelegateHandle Fault;
		if (Case == 0)
		{
			Fault = ASC->OnActiveGameplayEffectAddedDelegateToSelf.AddLambda(
				[this, &Domains, &OwnerEndCallbacks](UAbilitySystemComponent*, const FGameplayEffectSpec& Spec,
					FActiveGameplayEffectHandle)
				{
					if (Spec.Def->IsA<UKnightSourceProjectionEffect>())
					{
						++OwnerEndCallbacks;
						TestTrue(TEXT("Raw staged-effect callback actually destroys the shared owner"),
							Domains.RPG->GetOwner()->Destroy());
					}
				});
		}
		else
		{
			Fault = ASC->OnAnyGameplayEffectRemovedDelegate().AddLambda(
				[this, &Domains, ASC, &OwnerEndCallbacks, Original = OldHandles[0]](const FActiveGameplayEffect& Effect)
				{
					if (Effect.Handle == Original)
					{
						++OwnerEndCallbacks;
						TestEqual(TEXT("Retired-effect callback sees committed source revision"),
							ASC->GetSourceProjection(KnightRPG::ProjectionSource).Revision, static_cast<int64>(2));
						TestTrue(TEXT("Retired-effect callback sees committed skill and inventory cost"),
							Domains.RPG->GetView().InventoryRevision == Domains.Inventory->GetView().Revision
							&& Domains.RPG->GetKnightView(Domains.Registrations[0].KnightId).PurchasedSkills.Num() == 1);
						TestTrue(TEXT("Raw retired-effect callback actually destroys the shared owner"),
							Domains.RPG->GetOwner()->Destroy());
					}
				});
		}
		const FKnightRPGCommandResult Outcome = Domains.RPG->ExecuteCommand(Owner, Purchase);
		if (Case == 0)
		{
			ASC->OnActiveGameplayEffectAddedDelegateToSelf.Remove(Fault);
			TestFalse(TEXT("Ended owner during Stage rejects before commit"), Outcome.Result.IsSuccess());
			TestTrue(TEXT("Stage owner-end rollback preserves exact value state and old effect handle"),
				Domains.RPG->GetView() == Before && Domains.Inventory->GetView() == InventoryBefore
				&& StatHandles(ASC) == OldHandles);
		}
		else
		{
			ASC->OnAnyGameplayEffectRemovedDelegate().Remove(Fault);
			TestTrue(TEXT("Ended owner after commit returns its complete committed receipt"), Outcome.Result.IsSuccess()
				&& Outcome.CommittedKnightRevision == Domains.RPG->GetKnightView(Owner.KnightId).Revision
				&& Outcome.CommittedInventoryRevision == Domains.Inventory->GetView().Revision);
			TestEqual(TEXT("Committed surviving source remains the new revision"),
				ASC->GetSourceProjection(KnightRPG::ProjectionSource).Revision, static_cast<int64>(2));
			TestEqual(TEXT("Committed surviving source keeps final actual stat"), ASC->GetKnightAttributes()->GetAttackPower(), 40.0f);
		}
		ASC->OnSourceProjectionChanged.Remove(SourceObserver);
		Domains.Inventory->OnSnapshotChanged.Remove(InventoryObserver);
		Domains.RPG->OnSnapshotChanged.Remove(RPGObserver);
		Domains.RPG->OnCommandCompleted.Remove(CommandObserver);
		TestEqual(TEXT("Real owner-end callback observed exactly once"), OwnerEndCallbacks, 1);
		TestEqual(TEXT("No project delegate runs after the shared owner ended"), ProjectPublications, 0);
		TestFalse(TEXT("Every owner-end path releases the source and both domain fences"),
			ASC->IsMutationFenced() || Domains.Inventory->IsReserved() || Domains.RPG->IsTransactionInProgress());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKnightRPGPickupRegistryTest, "Knight.Items.RPG.PickupRegistryRollback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FKnightRPGPickupRegistryTest::RunTest(const FString& Parameters)
{
	using namespace KnightRPGTests;
	FKnightWorld Fixture;
	UKnightAssetManager* Manager = UKnightAssetManager::GetKnightAssetManager();
	UKnightDefinition* Resolved = nullptr;
	if (!TestTrue(TEXT("Actual authored ForgeShard resolves through the production catalog"),
		Manager != nullptr && Manager->ResolveDefinition(FPrimaryAssetId(TEXT("KnightItem"), TEXT("ForgeShard")),
			UKnightItemDefinition::StaticClass(), Resolved).IsSuccess()))
	{
		return false;
	}
	UKnightItemDefinition* ActualItem = Cast<UKnightItemDefinition>(Resolved);
	FKnightDomains Domains = Fixture.MakeDomains(nullptr, ActualItem);
	if (!TestTrue(TEXT("Pickup domain initializes with a validated immutable actual-definition catalog"),
		Domains.InitializationResult.IsSuccess() && Domains.Inventory->InitializeInventory(4).IsSuccess()
		&& Domains.RPG->InitializeRPG(Domains.Inventory, Fixture.Tree->GetPrimaryAssetId(),
			Domains.Registrations).IsSuccess()))
	{
		return false;
	}
	const FGuid PickupId = FGuid::NewGuid();
	const FKnightItemInstance PickedItem = Item(ActualItem, 2);
	AKnightItemPickup* Initial = Fixture.World->SpawnActor<AKnightItemPickup>();
	AKnightItemPickup* Impostor = Fixture.World->SpawnActor<AKnightItemPickup>();
	if (!TestTrue(TEXT("Genuine pickup actors initialize from the real typed item"),
		Initial->InitializePickup(PickupId, PickedItem).IsSuccess()
		&& Impostor->InitializePickup(PickupId, PickedItem).IsSuccess()
		&& Domains.RPG->RegisterPickup(Initial).IsSuccess()))
	{
		return false;
	}
	const FKnightRPGView Before = Domains.RPG->GetView();
	const FKnightInventoryView InventoryBefore = Domains.Inventory->GetView();
	TestEqual(TEXT("Actor impersonating the same GUID cannot withdraw the registered actor"),
		Domains.RPG->UnregisterUnclaimedPickup(Impostor).Error, EKnightError::NotOwned);
	Impostor->Destroy();
	TestTrue(TEXT("Exact unclaimed reward actor may be withdrawn before failed-batch cleanup"),
		Domains.RPG->UnregisterUnclaimedPickup(Initial).IsSuccess());
	Initial->Destroy();
	AKnightItemPickup* Replacement = Fixture.World->SpawnActor<AKnightItemPickup>();
	TestTrue(TEXT("Retried reward batch can register its original stable unclaimed pickup identity"),
		Replacement->InitializePickup(PickupId, PickedItem).IsSuccess()
		&& Domains.RPG->RegisterPickup(Replacement).IsSuccess());
	TestTrue(TEXT("Registry rollback never changes either value revision"),
		Domains.RPG->GetView() == Before && Domains.Inventory->GetView() == InventoryBefore);
	const FKnightRPGCallerContext Owner = Caller(Domains, FGuid::NewGuid());
	FKnightRPGCommand Pickup = Command(Domains, 0, EKnightRPGOperation::Pickup);
	Pickup.PickupId = PickupId;
	int32 ClaimCallbacks = 0;
	const FDelegateHandle Callback = Replacement->OnPickupChanged.AddLambda(
		[this, &Domains, Replacement, &ClaimCallbacks]()
		{
			++ClaimCallbacks;
			TestEqual(TEXT("A publication callback cannot withdraw a committing pickup"),
				Domains.RPG->UnregisterUnclaimedPickup(Replacement).Error, EKnightError::Conflict);
		});
	const FKnightRPGCommandResult Claimed = Domains.RPG->ExecuteCommand(Owner, Pickup);
	Replacement->OnPickupChanged.Remove(Callback);
	TestTrue(TEXT("Real range/LOS checked coordinator command consumes the pickup once"),
		Claimed.Result.IsSuccess() && Replacement->IsClaimed());
	TestEqual(TEXT("Actual claim presentation publishes once"), ClaimCallbacks, 1);
	TestEqual(TEXT("A durably claimed actor cannot be withdrawn after publication"),
		Domains.RPG->UnregisterUnclaimedPickup(Replacement).Error, EKnightError::Conflict);
	const FKnightInventoryView After = Domains.Inventory->GetView();
	TestTrue(TEXT("Exact successful pickup replay does not consume or add the item twice"),
		Domains.RPG->ExecuteCommand(Owner, Pickup).bWasReplay && Domains.Inventory->GetView() == After);
	FKnightRPGSnapshot Snapshot;
	if (!TestTrue(TEXT("Durable pickup claim is part of the actual validated journal"),
		Domains.RPG->CaptureSnapshot(Snapshot).IsSuccess()))
	{
		return false;
	}
	FKnightDomains Restored = Fixture.MakeDomains(&Domains.Registrations, ActualItem);
	if (!TestTrue(TEXT("Durable pickup journal reconstructs with real definitions and fresh source owners"),
		Restored.InitializationResult.IsSuccess()
		&& Restored.RPG->TryRestoreSnapshot(Restored.Inventory, Snapshot, Restored.Registrations).IsSuccess()))
	{
		return false;
	}
	AKnightItemPickup* Reissued = Fixture.World->SpawnActor<AKnightItemPickup>();
	TestTrue(TEXT("Claim replay fault creates a genuine fresh actor with the old pickup identity"),
		Reissued->InitializePickup(PickupId, Item(ActualItem, 2)).IsSuccess());
	TestEqual(TEXT("Reconstruction cannot register a new unclaimed actor for a durably claimed pickup"),
		Restored.RPG->RegisterPickup(Reissued).Error, EKnightError::Conflict);
	TestTrue(TEXT("Original human receives the original pickup receipt after reconstruction"),
		Restored.RPG->ExecuteCommand(Caller(Restored, Owner.HumanId), Pickup).bWasReplay
		&& Restored.Inventory->GetView() == After);
	return true;
}

#endif
