#include "Progression/KnightRPGComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Assets/KnightAssetManager.h"
#include "Attributes/KnightAttributeSet.h"
#include "Catalog/KnightGearCatalog.h"
#include "Components/KnightAbilitySystemComponent.h"
#include "Components/KnightLifeComponent.h"
#include "Components/SceneComponent.h"
#include "Definitions/KnightItemDefinition.h"
#include "Definitions/KnightAbilitySetDefinition.h"
#include "Definitions/KnightRuneDefinition.h"
#include "Definitions/KnightSkillDefinition.h"
#include "Definitions/KnightSkillTreeDefinition.h"
#include "Effects/KnightSourceProjectionEffect.h"
#include "Effects/KnightArenaResourceRecoveryEffect.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/WorldSettings.h"
#include "Inventory/KnightInventoryComponent.h"
#include "Loot/KnightItemPickup.h"
#include "GameplayEffect.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "Skills/KnightSkillRuntimeComponent.h"
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
			UKnightItemDefinition* ExtraItem = nullptr, bool bSkillLab = false, bool bGear = false) const
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
			if (bGear)
			{
				for (const FKnightGearSeed& Seed : FKnightGearCatalog::GetSeeds())
				{
					UKnightItemDefinition* Definition = Seed.Metadata.Category == EKnightItemCategory::AttachRune
						? NewObject<UKnightRuneDefinition>(Owner) : NewObject<UKnightItemDefinition>(Owner);
					Result.InitializationResult = FKnightGearCatalog::ConfigureDefinition(Definition, Seed);
					if (!Result.InitializationResult.IsSuccess())
					{
						return Result;
					}
					Items.Add(Definition);
				}
			}
			if (ExtraItem != nullptr)
			{
				Items.Add(ExtraItem);
			}
			Result.Inventory->InitializeFixtureCatalog(Items);
			TArray<UKnightDefinition*> Definitions { Ore.Get(), Hammer.Get(), Rune.Get(), Skill.Get(), BrokenSkill.Get(), Tree.Get() };
			if (bGear)
			{
				for (UKnightItemDefinition* Definition : Items)
				{
					if (!Definitions.Contains(Definition))
					{
						Definitions.Add(Definition);
					}
				}
			}
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
				if (bSkillLab)
				{
					UKnightSkillRuntimeComponent* Runtime = NewObject<UKnightSkillRuntimeComponent>(Pawn);
					Pawn->AddInstanceComponent(Runtime);
					Runtime->RegisterComponent();
				}
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

	FKnightResult ConfigureLab(const FKnightDomains& Domains)
	{
		TMap<FPrimaryAssetId, FKnightSkillBuild> Builds;
		const FName Specs[] { TEXT("Uranus"), TEXT("Pluto"), TEXT("Terra") };
		for (int32 Index = 0; Index < 3; ++Index)
		{
			FKnightSkillBuild Build;
			Build.SpecId = Specs[Index];
			Builds.Add(Domains.Registrations[Index].HeroDefinitionId, Build);
		}
		return Domains.RPG->ConfigureSkillLabBuilds(Builds);
	}

	TArray<FKnightItemInstance> CatalogItems()
	{
		TArray<FKnightItemInstance> Items;
		for (const FKnightGearSeed& Definition : FKnightGearCatalog::GetSeeds())
		{
			const int32 Copies = Definition.Metadata.Category == EKnightItemCategory::Equipment
				|| Definition.Metadata.Category == EKnightItemCategory::AttachRune ? 3 : 1;
			for (int32 Index = 0; Index < Copies; ++Index)
			{
				FKnightItemInstance Value;
				Value.InstanceId = FGuid::NewGuid();
				Value.DefinitionId = Definition.DefinitionId;
				Value.Count = Definition.Metadata.Category == EKnightItemCategory::Material ? 20
					: Definition.Metadata.Category == EKnightItemCategory::Consumable ? 2 : 1;
				Items.Add(Value);
			}
		}
		return Items;
	}

	FKnightItemInstance InventoryItem(const FKnightDomains& Domains, const FGuid& Id)
	{
		const FKnightInventoryView View = Domains.Inventory->GetView();
		const FKnightItemInstance* Found = View.Items.FindByPredicate([&Id](const FKnightItemInstance& Value)
		{
			return Value.InstanceId == Id;
		});
		return Found == nullptr ? FKnightItemInstance() : *Found;
	}

	FKnightItemInstance BagItem(const FKnightDomains& Domains, FName CatalogId)
	{
		const FPrimaryAssetId Id = FKnightGearCatalog::DefinitionId(CatalogId);
		const FKnightInventoryView View = Domains.Inventory->GetView();
		const FKnightItemInstance* Found = View.Items.FindByPredicate([&Id](const FKnightItemInstance& Value)
		{
			return Value.DefinitionId == Id && Value.LocationId == KnightGear::InventoryLocation;
		});
		return Found == nullptr ? FKnightItemInstance() : *Found;
	}

	bool EquipItem(FAutomationTestBase& Test, const FKnightDomains& Domains, const FKnightRPGCallerContext& Owner,
		int32 KnightIndex, const FKnightItemInstance& Value, FGameplayTag Slot)
	{
		FKnightRPGCommand Equip = Command(Domains, KnightIndex, EKnightRPGOperation::Equip);
		Equip.ItemInstanceId = Value.InstanceId;
		Equip.ExpectedItemRevision = Value.ItemRevision;
		Equip.SlotTag = Slot;
		const FKnightResult Result = Domains.RPG->ExecuteCommand(Owner, Equip).Result;
		return Test.TestTrue(FString::Printf(TEXT("Actual equip %s at%s: %s/%d %s"),
			*Value.DefinitionId.ToString(), *Slot.ToString(), *Result.Context.ToString(),
			static_cast<int32>(Result.Error), *Result.Message.ToString()), Result.IsSuccess());
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKnightSkillBuildTransactionTest, "Knight.Items.M55.BuildAndTalentLoadouts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FKnightSkillBuildTransactionTest::RunTest(const FString& Parameters)
{
	using namespace KnightRPGTests;
	FKnightWorld Fixture;
	FKnightDomains Domains = Fixture.MakeDomains(nullptr, nullptr, true);
	if (!TestTrue(TEXT("Real skill lab runtime and catalog initialize"), Domains.InitializationResult.IsSuccess()
		&& Domains.Inventory->InitializeInventory(8).IsSuccess() && ConfigureLab(Domains).IsSuccess()
		&& Domains.RPG->InitializeRPG(Domains.Inventory, Fixture.Tree->GetPrimaryAssetId(), Domains.Registrations).IsSuccess()))
	{
		return false;
	}
	const FKnightRPGCallerContext Owner = Caller(Domains, FGuid::NewGuid());
	UKnightAbilitySystemComponent* ASC = Owner.AbilitySystem.Get();
	UKnightSkillRuntimeComponent* Runtime = Owner.Pawn->FindComponentByClass<UKnightSkillRuntimeComponent>();
	TestTrue(TEXT("Initial confirmed source configures the actual runtime build"),
		Runtime->GetSkillState().Build == Domains.RPG->GetKnightView(Owner.KnightId).Build);
	for (int32 Index = 0; Index < 3; ++Index)
	{
		const FKnightRPGCallerContext KnightOwner = Caller(Domains, Owner.HumanId, Index);
		UKnightAbilitySystemComponent* KnightASC = KnightOwner.AbilitySystem.Get();
		const int32 OriginalAbilities = KnightASC->GetActivatableAbilities().Num();
		const TArray<FName> Specs = FKnightSkillCatalog::SpecsForHero(KnightOwner.HeroDefinitionId);
		TestEqual(TEXT("Canonical Golem exposes exactly three specs"), Specs.Num(), 3);
		for (FName SpecId : Specs)
		{
			FKnightRPGCommand Switch = Command(Domains, Index, EKnightRPGOperation::ApplyBuild);
			Switch.ProposedBuild.SpecId = SpecId;
			TestTrue(*FString::Printf(TEXT("%s switches through actual atomic projection"), *SpecId.ToString()),
				Domains.RPG->ExecuteCommand(KnightOwner, Switch).Result.IsSuccess());
			TestEqual(TEXT("Spec switch preserves the base grants and exactly four spec actions"),
				KnightASC->GetActivatableAbilities().Num(), OriginalAbilities);
			TestTrue(TEXT("GAS runtime selected spec agrees with confirmed domain identity"),
				KnightOwner.Pawn->FindComponentByClass<UKnightSkillRuntimeComponent>()->GetSkillState().Build.SpecId == SpecId);
		}
		FKnightRPGCommand Reset = Command(Domains, Index, EKnightRPGOperation::ApplyBuild);
		const FName SeedSpecs[] { TEXT("Uranus"), TEXT("Pluto"), TEXT("Terra") };
		Reset.ProposedBuild.SpecId = SeedSpecs[Index];
		TestTrue(TEXT("Fixture restores its explicit representative initial spec without a resource reset"),
			Domains.RPG->ExecuteCommand(KnightOwner, Reset).Result.IsSuccess());
	}
	FKnightSkillRuntimeSnapshot Timed;
	FKnightRPGSnapshot ActiveSnapshot;
	TestTrue(TEXT("Real GAS major ability enters its windup before commit"), Runtime->RequestSkill(4, nullptr).IsSuccess());
	const FKnightRPGView BeforeActiveCapture = Domains.RPG->GetView();
	const FKnightInventoryView BeforeActiveInventory = Domains.Inventory->GetView();
	TestFalse(TEXT("Sanctuary fragment rejects an active GAS action instead of losing its windup"),
		Domains.RPG->CaptureSnapshot(ActiveSnapshot).IsSuccess());
	TestTrue(TEXT("Rejected active-action capture preserves both confirmed domains"),
		Domains.RPG->GetView() == BeforeActiveCapture && Domains.Inventory->GetView() == BeforeActiveInventory);
	ASC->CancelAbilityHandle(Runtime->GetSkillHandle(4));
	Timed.CooldownRemaining = { 0.0f, 0.0f, 0.0f, 45.0f };
	Timed.Resource = 42.0f;
	Timed.AftershockRemaining = 3.0f;
	Timed.ResourceRegenDelayRemaining = 0.75f;
	TestTrue(TEXT("Runtime fixture owns real retained cooldown and spent resource"), Runtime->RestoreSnapshot(Timed).IsSuccess());
	const float OriginalStamina = ASC->GetKnightAttributes()->GetMaxStamina();
	const float OriginalHealth = ASC->GetKnightAttributes()->GetHealth();
	FKnightRPGCommand Build = Command(Domains, 0, EKnightRPGOperation::ApplyBuild);
	Build.ProposedBuild = Domains.RPG->GetKnightView(Owner.KnightId).Build;
	Build.ProposedBuild.ClassChoice = TEXT("Endurance");
	Build.ProposedBuild.SpecChoice = TEXT("A");
	const FKnightRPGCommand Stale = Build;
	TestTrue(TEXT("Both real choices atomically spend the shared two point seed"),
		Domains.RPG->ExecuteCommand(Owner, Build).Result.IsSuccess());
	TestEqual(TEXT("Class and spec cannot spend separate copies of points"), Domains.RPG->GetKnightView(Owner.KnightId).SkillPoints, 0);
	TestEqual(TEXT("Endurance source adds exactly ten stamina"), ASC->GetKnightAttributes()->GetMaxStamina(), OriginalStamina + 10.0f);
	TestEqual(TEXT("Applying a build never heals"), ASC->GetKnightAttributes()->GetHealth(), OriginalHealth);
	TestEqual(TEXT("Talent change never refills spent resource"), ASC->GetKnightAttributes()->GetRuneEnergy(), 42.0f);
	TestTrue(TEXT("Exact replay leaves one grant and unchanged points"), Domains.RPG->ExecuteCommand(Owner, Build).bWasReplay);
	FKnightRPGCommand Competing = Stale;
	Competing.RequestId = FGuid::NewGuid();
	TestEqual(TEXT("Second command against old revision rejects"),
		Domains.RPG->ExecuteCommand(Owner, Competing).Result.Error, EKnightError::InvalidRevision);
	const FKnightRPGView BeforeInvalid = Domains.RPG->GetView();
	FKnightRPGCommand WrongHero = Command(Domains, 0, EKnightRPGOperation::ApplyBuild);
	WrongHero.ProposedBuild.SpecId = TEXT("Terra");
	TestFalse(TEXT("Canonical sky Knight rejects earth spec"), Domains.RPG->ExecuteCommand(Owner, WrongHero).Result.IsSuccess());
	TestTrue(TEXT("Invalid identity leaves complete state untouched"), Domains.RPG->GetView() == BeforeInvalid);
	FKnightRPGCallerContext CombatOwner = Owner;
	CombatOwner.bInCombat = true;
	FKnightRPGCommand Refund = Command(Domains, 0, EKnightRPGOperation::ApplyBuild);
	Refund.ProposedBuild.SpecId = TEXT("Uranus");
	TestEqual(TEXT("Trusted encounter lock rejects build change"),
		Domains.RPG->ExecuteCommand(CombatOwner, Refund).Result.Error, EKnightError::InvalidState);
	FKnightRPGCommand Save = Command(Domains, 0, EKnightRPGOperation::NewTalentLoadout);
	Save.ProposedBuild = Build.ProposedBuild;
	Save.TalentLoadoutName = TEXT("Guard");
	TestTrue(TEXT("Save draft creates a real rune-free preset"), Domains.RPG->ExecuteCommand(Owner, Save).Result.IsSuccess());
	const FGuid PresetId = Save.RequestId;
	FKnightRPGCommand Duplicate = Command(Domains, 0, EKnightRPGOperation::DuplicateTalentLoadout);
	Duplicate.TalentLoadoutId = PresetId;
	Duplicate.TalentLoadoutName = TEXT("Guard Copy");
	TestTrue(TEXT("Duplicate preserves choice descriptor using a new stable ID"),
		Domains.RPG->ExecuteCommand(Owner, Duplicate).Result.IsSuccess());
	FKnightRPGCommand Rename = Command(Domains, 0, EKnightRPGOperation::RenameTalentLoadout);
	Rename.TalentLoadoutId = Duplicate.RequestId;
	Rename.TalentLoadoutName = TEXT("guard");
	TestEqual(TEXT("Case-insensitive duplicate name rejected"), Domains.RPG->ExecuteCommand(Owner, Rename).Result.Error, EKnightError::Conflict);
	Rename.RequestId = FGuid::NewGuid();
	Rename.TalentLoadoutName = TEXT("Burst");
	TestTrue(TEXT("Rename updates the saved preset version"), Domains.RPG->ExecuteCommand(Owner, Rename).Result.IsSuccess());
	Refund = Command(Domains, 0, EKnightRPGOperation::ApplyBuild);
	Refund.ProposedBuild.SpecId = TEXT("Venus");
	TestTrue(TEXT("Spec swap with both choices refunded is atomic"), Domains.RPG->ExecuteCommand(Owner, Refund).Result.IsSuccess());
	TestEqual(TEXT("Refund releases both real shared points"), Domains.RPG->GetKnightView(Owner.KnightId).SkillPoints, 2);
	TestEqual(TEXT("Source cleanup removes only Endurance"), ASC->GetKnightAttributes()->GetMaxStamina(), OriginalStamina);
	TestEqual(TEXT("New spec inherits the existing SpecMajor cooldown"), Runtime->CaptureSnapshot().CooldownRemaining[3], 45.0f);
	TestEqual(TEXT("Spec swap never refunds resource"), ASC->GetKnightAttributes()->GetRuneEnergy(), 42.0f);
	FKnightRPGCommand Apply = Command(Domains, 0, EKnightRPGOperation::ApplyTalentLoadout);
	Apply.TalentLoadoutId = PresetId;
	TestTrue(TEXT("Saved talent loadout reapplies actual spec and choices"), Domains.RPG->ExecuteCommand(Owner, Apply).Result.IsSuccess());
	FKnightRPGCommand Delete = Command(Domains, 0, EKnightRPGOperation::DeleteTalentLoadout);
	Delete.TalentLoadoutId = PresetId;
	const FKnightSkillBuild Effective = Domains.RPG->GetKnightView(Owner.KnightId).Build;
	TestTrue(TEXT("Delete selected preset preserves the effective build"), Domains.RPG->ExecuteCommand(Owner, Delete).Result.IsSuccess()
		&& Domains.RPG->GetKnightView(Owner.KnightId).Build == Effective
		&& !Domains.RPG->GetKnightView(Owner.KnightId).ActiveTalentLoadoutId.IsValid());
	FKnightRPGSnapshot Snapshot;
	TestTrue(TEXT("Actual domain journal includes CRUD and runtime fragment"), Domains.RPG->CaptureSnapshot(Snapshot).IsSuccess());
	TArray<uint8> Bytes;
	{
		FMemoryWriter Writer(Bytes);
		FObjectAndNameAsStringProxyArchive Archive(Writer, false);
		Archive.ArIsSaveGame = true;
		FKnightRPGSnapshot::StaticStruct()->SerializeItem(Archive, &Snapshot, nullptr);
		TestFalse(TEXT("Schema two reflected values serialize without actor references"), Archive.IsError());
	}
	FKnightRPGSnapshot Decoded;
	{
		FMemoryReader Reader(Bytes);
		FObjectAndNameAsStringProxyArchive Archive(Reader, false);
		Archive.ArIsSaveGame = true;
		FKnightRPGSnapshot::StaticStruct()->SerializeItem(Archive, &Decoded, nullptr);
		TestFalse(TEXT("Schema two reflected values decode"), Archive.IsError());
	}
	TestTrue(TEXT("Archive preserves preset IDs, choices, cooldown groups and spent resource"), Decoded == Snapshot);
	FKnightRPGSnapshot ChangedDelay = Decoded;
	ChangedDelay.CombatStates.FindChecked(Owner.KnightId).Skills.ResourceRegenDelayRemaining = 0.25f;
	TestFalse(TEXT("Snapshot equality includes the resource regeneration delay"), ChangedDelay == Snapshot);
	FKnightRPGSnapshot ZeroHealth = Decoded;
	ZeroHealth.CombatStates.FindChecked(Owner.KnightId).Health = 0.0f;
	FKnightDomains Rejected = Fixture.MakeDomains(&Domains.Registrations, nullptr, true);
	const FKnightRPGView RejectedBefore = Rejected.RPG->GetView();
	const FKnightInventoryView RejectedInventoryBefore = Rejected.Inventory->GetView();
	TestFalse(TEXT("Zero-health fragment rejects before life callbacks can invalidate prepared projections"),
		Rejected.RPG->TryRestoreSnapshot(Rejected.Inventory, ZeroHealth, Rejected.Registrations).IsSuccess());
	TestTrue(TEXT("Rejected zero-health fragment changes neither domain"),
		Rejected.RPG->GetView() == RejectedBefore && Rejected.Inventory->GetView() == RejectedInventoryBefore);
	for (const FKnightRPGKnightRegistration& Registration : Rejected.Registrations)
	{
		TestTrue(TEXT("Rejected health preflight leaves every participant alive, unfenced and without a new source"),
			Registration.AbilitySystem->GetLifeComponent()->IsAlive()
			&& !Registration.AbilitySystem->IsMutationFenced()
			&& Registration.AbilitySystem->GetSourceProjection(KnightRPG::ProjectionSource).Revision == 0);
	}
	FKnightDomains Restored = Fixture.MakeDomains(&Domains.Registrations, nullptr, true);
	TestTrue(TEXT("Full talent journal reconstructs without changing equipment or points"),
		Restored.RPG->TryRestoreSnapshot(Restored.Inventory, Decoded, Restored.Registrations).IsSuccess()
		&& Restored.RPG->GetView() == Domains.RPG->GetView());
	TestEqual(TEXT("Reconstruction preserves current resource rather than refilling it"),
		Restored.Registrations[0].AbilitySystem->GetKnightAttributes()->GetRuneEnergy(), 42.0f);
	TestEqual(TEXT("Reconstruction keeps the stable major cooldown group"),
		Restored.Registrations[0].Pawn->FindComponentByClass<UKnightSkillRuntimeComponent>()->CaptureSnapshot().CooldownRemaining[3], 45.0f);
	TestEqual(TEXT("Reconstruction retains the weapon proc ICD"),
		Restored.Registrations[0].Pawn->FindComponentByClass<UKnightSkillRuntimeComponent>()->CaptureSnapshot().AftershockRemaining, 3.0f);
	TestEqual(TEXT("Reconstruction retains the resource regeneration pause"),
		Restored.Registrations[0].Pawn->FindComponentByClass<UKnightSkillRuntimeComponent>()
			->CaptureSnapshot().ResourceRegenDelayRemaining, 0.75f);
	Restored.Registrations[0].AbilitySystem->SetNumericAttributeBase(UKnightAttributeSet::GetHealthAttribute(), 0.0f);
	FKnightRPGSnapshot Nonliving;
	TestFalse(TEXT("Capture rejects a nonliving Knight because the sanctuary fragment has no LifeState or bleedout"),
		Restored.RPG->CaptureSnapshot(Nonliving).IsSuccess());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKnightSkillPassiveSnapshotTest, "Knight.Items.M55.PassiveSnapshotCapacity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FKnightSkillPassiveSnapshotTest::RunTest(const FString& Parameters)
{
	using namespace KnightRPGTests;
	UKnightAssetManager* Manager = UKnightAssetManager::GetKnightAssetManager();
	UKnightDefinition* Resolved = nullptr;
	const FPrimaryAssetId SetId(TEXT("KnightAbilitySet"), TEXT("Foundation"));
	if (!TestTrue(TEXT("Actual authored typed ability set resolves for the passive fixture"), Manager != nullptr
		&& Manager->ResolveDefinition(SetId, UKnightAbilitySetDefinition::StaticClass(), Resolved).IsSuccess()))
	{
		return false;
	}
	UKnightAbilitySetDefinition* Set = CastChecked<UKnightAbilitySetDefinition>(Resolved);
	const TArray<FKnightAbilitySetEntry> OriginalAbilities = Set->GetAbilities();
	const TArray<TSubclassOf<UGameplayEffect>> OriginalEffects = Set->GetEffects();
	UKnightArenaResourceRecoveryEffect* Passive = GetMutableDefault<UKnightArenaResourceRecoveryEffect>();
	const TArray<FGameplayModifierInfo> OriginalModifiers = Passive->Modifiers;
	const FScalableFloat OriginalPeriod = Passive->Period;
	// Scope restoration also runs after early failure. No modified package or CDO is saved to disk.
	ON_SCOPE_EXIT
	{
		Set->TrySetAbilities(OriginalAbilities);
		Set->TrySetEffects(OriginalEffects);
		Passive->Modifiers = OriginalModifiers;
		Passive->Period = OriginalPeriod;
	};
	const FKnightAbilitySetEntry* Pulse = OriginalAbilities.FindByPredicate([](const FKnightAbilitySetEntry& Entry)
	{
		return Entry.InputTag == KnightGameplayTags::Input_Ability_Test;
	});
	if (!TestNotNull(TEXT("Authored set contains its distinct native Pulse action"), Pulse))
	{
		return false;
	}
	Passive->Modifiers.Reset();
	Passive->Period = FScalableFloat(0.0f);
	const FGameplayAttribute Maximums[] { UKnightAttributeSet::GetMaxHealthAttribute(),
		UKnightAttributeSet::GetMaxStaminaAttribute(), UKnightAttributeSet::GetMaxRuneEnergyAttribute() };
	const float Magnitudes[] { 25.0f, 15.0f, 20.0f };
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Maximums); ++Index)
	{
		FGameplayModifierInfo Modifier;
		Modifier.Attribute = Maximums[Index];
		Modifier.ModifierOp = EGameplayModOp::Additive;
		Modifier.ModifierMagnitude = FScalableFloat(Magnitudes[Index]);
		Passive->Modifiers.Add(Modifier);
	}
	if (!TestTrue(TEXT("Scoped authored passive conforms to real source grant validation"),
		Set->TrySetAbilities({ *Pulse }).IsSuccess()
		&& Set->TrySetEffects({ UKnightArenaResourceRecoveryEffect::StaticClass() }).IsSuccess()
		&& Set->ValidateAbilitySet().IsSuccess()))
	{
		return false;
	}
	FKnightWorld Fixture;
	if (!TestTrue(TEXT("Equipment flat stats and optional passive are independent authored sources"),
		Fixture.Hammer->TrySetItemTuning(EKnightItemKind::Equipment, 1, SetId,
			Fixture.Hammer->GetStatBonuses()).IsSuccess()))
	{
		return false;
	}
	FKnightDomains Domains = Fixture.MakeDomains(nullptr, nullptr, true);
	if (!TestTrue(TEXT("Passive snapshot fixture initializes the genuine source coordinator"),
		Domains.InitializationResult.IsSuccess() && Domains.Inventory->InitializeInventory(8).IsSuccess()
		&& ConfigureLab(Domains).IsSuccess()))
	{
		return false;
	}
	const FKnightItemInstance Hammer = Item(Fixture.Hammer.Get());
	if (!TestTrue(TEXT("Passive hammer owns a real shared inventory GUID"),
		Domains.Inventory->TryApplyTransaction(Seed(Domains.Inventory, { Hammer })).Result.IsSuccess()
		&& Domains.RPG->InitializeRPG(Domains.Inventory, Fixture.Tree->GetPrimaryAssetId(),
			Domains.Registrations).IsSuccess()))
	{
		return false;
	}
	const FKnightRPGCallerContext Owner = Caller(Domains, FGuid::NewGuid());
	FKnightRPGCommand Equip = Command(Domains, 0, EKnightRPGOperation::Equip);
	Equip.ItemInstanceId = Hammer.InstanceId;
	Equip.SlotTag = KnightGameplayTags::Slot_Equipment_Weapon;
	if (!TestTrue(TEXT("GAS grants the actual authored passive and flat item effect"),
		Domains.RPG->ExecuteCommand(Owner, Equip).Result.IsSuccess()))
	{
		return false;
	}
	UKnightAbilitySystemComponent* ASC = Owner.AbilitySystem.Get();
	TestEqual(TEXT("Max HP contains base100 + flat20 + optional passive25 exactly once"),
		ASC->GetKnightAttributes()->GetMaxHealth(), 145.0f);
	TestEqual(TEXT("Max Stamina contains the additive passive15"), ASC->GetKnightAttributes()->GetMaxStamina(), 115.0f);
	TestEqual(TEXT("Max resource contains the additive passive20"), ASC->GetKnightAttributes()->GetMaxRuneEnergy(), 120.0f);
	ASC->SetNumericAttributeBase(UKnightAttributeSet::GetHealthAttribute(), 145.0f);
	ASC->SetNumericAttributeBase(UKnightAttributeSet::GetStaminaAttribute(), 115.0f);
	ASC->SetNumericAttributeBase(UKnightAttributeSet::GetRuneEnergyAttribute(), 120.0f);
	FKnightRPGSnapshot Snapshot;
	if (!TestTrue(TEXT("Full current pools from the authored passive capture without a recovery reset"),
		Domains.RPG->CaptureSnapshot(Snapshot).IsSuccess()))
	{
		return false;
	}
	FKnightDomains Fresh = Fixture.MakeDomains(&Domains.Registrations, nullptr, true);
	if (!TestTrue(TEXT("Preflight accounts for optional passive capacity before staging"),
		Fresh.RPG->TryRestoreSnapshot(Fresh.Inventory, Snapshot, Fresh.Registrations).IsSuccess()))
	{
		return false;
	}
	UKnightAbilitySystemComponent* RestoredASC = Fresh.Registrations[0].AbilitySystem.Get();
	TestEqual(TEXT("Reconstruction retains absolute HP at passive capacity"), RestoredASC->GetKnightAttributes()->GetHealth(), 145.0f);
	TestEqual(TEXT("Reconstruction retains absolute stamina at passive capacity"), RestoredASC->GetKnightAttributes()->GetStamina(), 115.0f);
	TestEqual(TEXT("Reconstruction retains absolute resource at passive capacity"), RestoredASC->GetKnightAttributes()->GetRuneEnergy(), 120.0f);
	FKnightRPGSnapshot Overflow = Snapshot;
	Overflow.CombatStates.FindChecked(Owner.KnightId).Health = 146.0f;
	FKnightDomains Rejected = Fixture.MakeDomains(&Domains.Registrations, nullptr, true);
	const FKnightRPGView RPGBefore = Rejected.RPG->GetView();
	const FKnightInventoryView InventoryBefore = Rejected.Inventory->GetView();
	TestFalse(TEXT("One HP above actual passive capacity rejects before a source can stage"),
		Rejected.RPG->TryRestoreSnapshot(Rejected.Inventory, Overflow, Rejected.Registrations).IsSuccess());
	TestTrue(TEXT("Invalid passive capacity leaves both domains untouched"),
		Rejected.RPG->GetView() == RPGBefore && Rejected.Inventory->GetView() == InventoryBefore);
	for (const FKnightRPGKnightRegistration& Registration : Rejected.Registrations)
	{
		TestTrue(TEXT("Rejected passive capacity neither fences nor grants any new participant source"),
			!Registration.AbilitySystem->IsMutationFenced()
			&& Registration.AbilitySystem->GetSourceProjection(KnightRPG::ProjectionSource).Revision == 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKnightSkillRuneAttachmentTest, "Knight.Items.M55.InventoryBackedAttachments",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FKnightSkillRuneAttachmentTest::RunTest(const FString& Parameters)
{
	using namespace KnightRPGTests;
	FKnightWorld Fixture;
	TStrongObjectPtr<UKnightRuneDefinition> Convergence(NewObject<UKnightRuneDefinition>());
	Convergence->TrySetIdentity(TEXT("DomainConvergence"), TEXT("KnightItem"));
	TestTrue(TEXT("Actual attachment rune definition validates"), Convergence->TrySetAttachmentRune(TEXT("Skill.Convergence")).IsSuccess());
	FKnightDomains Domains = Fixture.MakeDomains(nullptr, Convergence.Get(), true);
	Domains.Inventory->InitializeInventory(8);
	const FKnightItemInstance Rune = Item(Convergence.Get());
	const FKnightItemInstance Hammer = Item(Fixture.Hammer.Get());
	Domains.Inventory->TryApplyTransaction(Seed(Domains.Inventory, { Rune, Hammer }));
	if (!TestTrue(TEXT("Attachment fixture initializes actual shared inventory and sources"),
		Domains.InitializationResult.IsSuccess() && ConfigureLab(Domains).IsSuccess()
		&& Domains.RPG->InitializeRPG(Domains.Inventory, Fixture.Tree->GetPrimaryAssetId(), Domains.Registrations).IsSuccess()))
	{
		return false;
	}
	const FKnightRPGCallerContext Owner = Caller(Domains, FGuid::NewGuid());
	FKnightRPGCommand Attach = Command(Domains, 0, EKnightRPGOperation::ApplyBuild);
	Attach.ProposedBuild.SpecId = TEXT("Uranus");
	Attach.ProposedBuild.SkillRune = TEXT("Skill.Convergence");
	Attach.SkillRuneInstanceId = FGuid::NewGuid();
	TestFalse(TEXT("Fake source inventory instance rejects"), Domains.RPG->ExecuteCommand(Owner, Attach).Result.IsSuccess());
	Attach.SkillRuneInstanceId = Rune.InstanceId;
	FKnightRPGCommand Race = Command(Domains, 2, EKnightRPGOperation::ApplyBuild);
	Race.ProposedBuild.SpecId = TEXT("Mars");
	Race.ProposedBuild.SkillRune = TEXT("Skill.Convergence");
	Race.SkillRuneInstanceId = Rune.InstanceId;
	TestTrue(TEXT("Compatible actual inventory instance attaches"), Domains.RPG->ExecuteCommand(Owner, Attach).Result.IsSuccess());
	TestEqual(TEXT("Concurrent second Knight cannot claim the same rune instance"),
		Domains.RPG->ExecuteCommand(Caller(Domains, FGuid::NewGuid(), 2), Race).Result.Error, EKnightError::Conflict);
	TestTrue(TEXT("Attachment exact replay does not duplicate its grant"), Domains.RPG->ExecuteCommand(Owner, Attach).bWasReplay);
	FKnightRPGCommand Incompatible = Command(Domains, 0, EKnightRPGOperation::ApplyBuild);
	Incompatible.ProposedBuild = Attach.ProposedBuild;
	Incompatible.ProposedBuild.SpecId = TEXT("Venus");
	Incompatible.SkillRuneInstanceId = Rune.InstanceId;
	const FKnightRPGView Before = Domains.RPG->GetView();
	TestFalse(TEXT("Spec switch rejects retained incompatible rune"), Domains.RPG->ExecuteCommand(Owner, Incompatible).Result.IsSuccess());
	TestTrue(TEXT("Incompatible rune rollback retains full effective state"), Domains.RPG->GetView() == Before);
	FKnightInventoryRequest Removal;
	Removal.RequestId = FGuid::NewGuid();
	Removal.ExpectedRevision = Domains.Inventory->GetView().Revision;
	FKnightInventoryMutation Remove;
	Remove.Operation = EKnightInventoryOperation::Remove;
	Remove.Item = Rune;
	Removal.Mutations.Add(Remove);
	TestEqual(TEXT("Attached rune cannot be consumed by direct inventory mutation"),
		Domains.Inventory->TryApplyTransaction(Removal).Result.Error, EKnightError::NotOwned);
	FKnightRPGCommand Detach = Command(Domains, 0, EKnightRPGOperation::ApplyBuild);
	Detach.ProposedBuild.SpecId = TEXT("Venus");
	TestTrue(TEXT("Detach and compatible spec change share one transaction"), Domains.RPG->ExecuteCommand(Owner, Detach).Result.IsSuccess());
	TestEqual(TEXT("Rune remains in shared inventory after detach"), Domains.Inventory->GetView().Items.Num(), 2);
	TestTrue(TEXT("Removing attachment leaves independent weapon inventory value"),
		Domains.Inventory->GetView().Items.ContainsByPredicate([&Hammer](const FKnightItemInstance& Entry)
			{
				return Entry == Hammer;
			}));
	TStrongObjectPtr<UKnightRuneDefinition> Aftershock(NewObject<UKnightRuneDefinition>());
	Aftershock->TrySetIdentity(TEXT("DomainAftershock"), TEXT("KnightItem"));
	Aftershock->TrySetAttachmentRune(TEXT("Weapon.Aftershock"));
	FKnightDomains WeaponDomains = Fixture.MakeDomains(nullptr, Aftershock.Get(), true);
	WeaponDomains.Inventory->InitializeInventory(8);
	const FKnightItemInstance WeaponRune = Item(Aftershock.Get());
	const FKnightItemInstance Weapon = Item(Fixture.Hammer.Get());
	WeaponDomains.Inventory->TryApplyTransaction(Seed(WeaponDomains.Inventory, { WeaponRune, Weapon }));
	ConfigureLab(WeaponDomains);
	WeaponDomains.RPG->InitializeRPG(WeaponDomains.Inventory, Fixture.Tree->GetPrimaryAssetId(), WeaponDomains.Registrations);
	const FKnightRPGCallerContext WeaponOwner = Caller(WeaponDomains, FGuid::NewGuid());
	FKnightRPGCommand WeaponAttach = Command(WeaponDomains, 0, EKnightRPGOperation::ApplyBuild);
	WeaponAttach.ProposedBuild.SpecId = TEXT("Uranus");
	WeaponAttach.ProposedBuild.WeaponRune = TEXT("Weapon.Aftershock");
	WeaponAttach.WeaponRuneInstanceId = WeaponRune.InstanceId;
	TestEqual(TEXT("Aftershock cannot attach to an empty weapon socket"),
		WeaponDomains.RPG->ExecuteCommand(WeaponOwner, WeaponAttach).Result.Error, EKnightError::InvalidState);
	FKnightRPGCommand Equip = Command(WeaponDomains, 0, EKnightRPGOperation::Equip);
	Equip.ItemInstanceId = Weapon.InstanceId;
	Equip.SlotTag = KnightGameplayTags::Slot_Equipment_Weapon;
	TestTrue(TEXT("Real M5 hammer equips independently"), WeaponDomains.RPG->ExecuteCommand(WeaponOwner, Equip).Result.IsSuccess());
	WeaponAttach = Command(WeaponDomains, 0, EKnightRPGOperation::ApplyBuild);
	WeaponAttach.ProposedBuild.SpecId = TEXT("Uranus");
	WeaponAttach.ProposedBuild.WeaponRune = TEXT("Weapon.Aftershock");
	WeaponAttach.WeaponRuneInstanceId = WeaponRune.InstanceId;
	TestTrue(TEXT("Aftershock compatible heavy hammer attachment commits"),
		WeaponDomains.RPG->ExecuteCommand(WeaponOwner, WeaponAttach).Result.IsSuccess());
	FKnightRPGCommand Unequip = Command(WeaponDomains, 0, EKnightRPGOperation::Unequip);
	Unequip.ItemInstanceId = Weapon.InstanceId;
	Unequip.SlotTag = KnightGameplayTags::Slot_Equipment_Weapon;
	TestEqual(TEXT("Weapon removal cannot leave an active incompatible Aftershock source"),
		WeaponDomains.RPG->ExecuteCommand(WeaponOwner, Unequip).Result.Error, EKnightError::InvalidState);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKnightM5SnapshotMigrationTest, "Knight.Items.M55.LegacySnapshotMigration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FKnightM5SnapshotMigrationTest::RunTest(const FString& Parameters)
{
	using namespace KnightRPGTests;
	FKnightWorld Fixture;
	FKnightDomains Domains = Fixture.MakeDomains();
	Domains.Inventory->InitializeInventory(4);
	const FKnightItemInstance Hammer = Item(Fixture.Hammer.Get());
	Domains.Inventory->TryApplyTransaction(Seed(Domains.Inventory, { Hammer }));
	Domains.RPG->InitializeRPG(Domains.Inventory, Fixture.Tree->GetPrimaryAssetId(), Domains.Registrations);
	const FKnightRPGCallerContext Owner = Caller(Domains, FGuid::NewGuid());
	FKnightRPGCommand Equip = Command(Domains, 0, EKnightRPGOperation::Equip);
	Equip.ItemInstanceId = Hammer.InstanceId;
	Equip.SlotTag = KnightGameplayTags::Slot_Equipment_Weapon;
	TestTrue(TEXT("Old M5 equipment transaction commits"), Domains.RPG->ExecuteCommand(Owner, Equip).Result.IsSuccess());
	FKnightRPGSnapshot Snapshot;
	TestTrue(TEXT("Legacy source snapshot captures"), Domains.RPG->CaptureSnapshot(Snapshot).IsSuccess());
	Snapshot.SchemaVersion = 1;
	FKnightDomains Restored = Fixture.MakeDomains(&Domains.Registrations);
	TestTrue(TEXT("Schema one migrates using unchanged inventory and journal IDs"),
		Restored.RPG->TryRestoreSnapshot(Restored.Inventory, Snapshot, Restored.Registrations).IsSuccess()
		&& Restored.RPG->GetView() == Domains.RPG->GetView()
		&& Restored.Inventory->GetView() == Domains.Inventory->GetView());
	TestTrue(TEXT("Migration retains legacy kit until campaign explicitly unlocks new spec catalog"),
		Restored.RPG->GetKnightView(Owner.KnightId).Build.SpecId.IsNone());
	FKnightRPGSnapshot Upgraded;
	TestTrue(TEXT("Subsequent capture uses the current schema"), Restored.RPG->CaptureSnapshot(Upgraded).IsSuccess()
		&& Upgraded.SchemaVersion == KnightRPG::SnapshotVersion);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKnightGearCatalogTest, "Knight.Items.M56.CatalogAndUpgradeMath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FKnightGearCatalogTest::RunTest(const FString& Parameters)
{
	TArray<TStrongObjectPtr<UKnightItemDefinition>> Owners;
	TArray<UKnightItemDefinition*> Definitions;
	TSet<EKnightEquipmentSubtype> Subtypes;
	for (const FKnightGearSeed& Seed : FKnightGearCatalog::GetSeeds())
	{
		UKnightItemDefinition* Definition = Seed.Metadata.Category == EKnightItemCategory::AttachRune
			? NewObject<UKnightRuneDefinition>() : NewObject<UKnightItemDefinition>();
		Owners.Emplace(Definition);
		Definitions.Add(Definition);
		TestTrue(*Seed.CatalogId.ToString(), FKnightGearCatalog::ConfigureDefinition(Definition, Seed).IsSuccess());
		if (Seed.Metadata.Category == EKnightItemCategory::Equipment)
		{
			Subtypes.Add(Seed.Metadata.Subtype);
		}
		if (Seed.CatalogId == TEXT("W02"))
		{
			const FKnightItemStatBonuses Rank = Definition->GetEffectiveStats(1);
			TestEqual(TEXT("Power 18 rounds half-up to 20"), Rank.AttackPower, 20.0f);
			TestEqual(TEXT("Armor 25 rounds half-up to 28"), Rank.Defense, 28.0f);
			TestEqual(TEXT("Percentage points are not upgraded"), Rank.Versatility, 0.02f);
			TestEqual(TEXT("Rank one item level is 22"), Definition->GetEffectiveItemLevel(1), 22);
			TestEqual(TEXT("Upgrade does not lower requirement"), Definition->GetMetadata().RequiredLevel, 5);
		}
	}
	TestTrue(TEXT("All 41 actual definitions validate against the manifest"), FKnightGearCatalog::ValidateCatalog(Definitions).IsSuccess());
	TestEqual(TEXT("Eleven equipment subtypes"), Subtypes.Num(), 11);
	TestEqual(TEXT("Twelve equipment slots"), KnightGear::EquipmentSlots().Num(), 12);
	TestEqual(TEXT("Rune manifest reuses canonical existing identity"),
		FKnightGearCatalog::DefinitionId(TEXT("R01")), FPrimaryAssetId(TEXT("KnightItem"), TEXT("Aftershock")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKnightGearTransactionTest, "Knight.Items.M56.AtomicGearEconomyAndSnapshot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FKnightGearTransactionTest::RunTest(const FString& Parameters)
{
	using namespace KnightRPGTests;
	FKnightWorld Fixture;
	FKnightDomains Domains = Fixture.MakeDomains(nullptr, nullptr, true, true);
	if (!TestTrue(TEXT("Actual gear definitions and ASCs initialize"), Domains.InitializationResult.IsSuccess()
		&& Domains.Inventory->InitializeInventory(128).IsSuccess() && ConfigureLab(Domains).IsSuccess()
		&& Domains.RPG->ConfigureGearDemo(FGuid::NewGuid(), 500, 8, false).IsSuccess()))
	{
		return false;
	}
	TArray<FKnightItemInstance> Items;
	for (const FKnightGearSeed& SeedDefinition : FKnightGearCatalog::GetSeeds())
	{
		const int32 Copies = SeedDefinition.Metadata.Category == EKnightItemCategory::Equipment
			|| SeedDefinition.Metadata.Category == EKnightItemCategory::AttachRune ? 3 : 1;
		for (int32 Index = 0; Index < Copies; ++Index)
		{
			FKnightItemInstance Value;
			Value.InstanceId = FGuid::NewGuid();
			Value.DefinitionId = SeedDefinition.DefinitionId;
			Value.Count = SeedDefinition.Metadata.Category == EKnightItemCategory::Material ? 20
				: SeedDefinition.Metadata.Category == EKnightItemCategory::Consumable ? 6 : 1;
			Items.Add(Value);
		}
	}
	if (!TestTrue(TEXT("102 real equipment/rune GUIDs and seven stacks fit 128 bag cells"),
		Domains.Inventory->TryApplyTransaction(Seed(Domains.Inventory, Items)).Result.IsSuccess()
		&& Domains.Inventory->GetView().GetBagCount() == 109
		&& Domains.RPG->InitializeRPG(Domains.Inventory, Fixture.Tree->GetPrimaryAssetId(), Domains.Registrations, 5).IsSuccess()))
	{
		return false;
	}
	FKnightRPGCallerContext Owner = Caller(Domains, FGuid::NewGuid());
	for (const FKnightRPGKnightRegistration& Registration : Domains.Registrations)
	{
		Owner.EditableKnights.Add(Registration.KnightId);
	}
	FKnightRPGCommand Preset = Command(Domains, 0, EKnightRPGOperation::ApplyGearPreset);
	Preset.PresetId = TEXT("Guard");
	if (!TestTrue(TEXT("Guard preset equips all twelve exact instances atomically"),
		Domains.RPG->ExecuteCommand(Owner, Preset).Result.IsSuccess()))
	{
		return false;
	}
	const FKnightRPGKnightView Guard = Domains.RPG->GetKnightView(Owner.KnightId);
	TestEqual(TEXT("Guard manifest power contribution"), Guard.StatBonuses.AttackPower, 28.0f);
	TestEqual(TEXT("Guard manifest health contribution"), Guard.StatBonuses.MaxHealth, 380.0f);
	TestEqual(TEXT("Guard manifest armor contribution"), Guard.StatBonuses.Defense, 97.0f);
	TestTrue(TEXT("Guard manifest versatility contribution"), FMath::IsNearlyEqual(Guard.StatBonuses.Versatility, 0.12f));
	TestEqual(TEXT("All twelve real equipment slots are assigned"), Guard.Assignments.Num(), 12);
	TestEqual(TEXT("Equipped items do not occupy bag cells"), Domains.Inventory->GetView().GetBagCount(), 97);
	FKnightRPGCommand DuplicateRing = Command(Domains, 0, EKnightRPGOperation::Equip);
	DuplicateRing.ItemInstanceId = Domains.Inventory->GetView().Items.FindByPredicate([](const FKnightItemInstance& Entry)
	{
		return Entry.DefinitionId == FKnightGearCatalog::DefinitionId(TEXT("F01"))
			&& Entry.LocationId == KnightGear::InventoryLocation;
	})->InstanceId;
	DuplicateRing.ExpectedItemRevision = 1;
	DuplicateRing.SlotTag = KnightGameplayTags::Slot_Equipment_Ring2;
	const FKnightInventoryView BeforeRing = Domains.Inventory->GetView();
	const FKnightRPGView BeforeRingRPG = Domains.RPG->GetView();
	TestEqual(TEXT("Two copies of one ring definition cannot occupy both ring slots"),
		Domains.RPG->ExecuteCommand(Owner, DuplicateRing).Result.Error, EKnightError::Conflict);
	TestTrue(TEXT("Ring rejection leaves every location and revision unchanged"), Domains.Inventory->GetView() == BeforeRing
		&& Domains.RPG->GetView() == BeforeRingRPG);
	const FKnightEquipmentAssignment* Weapon = Guard.FindAssignment(KnightGameplayTags::Slot_Equipment_Weapon);
	const FGuid WeaponId = Weapon->ItemInstanceId;
	auto FindItem = [&Domains](const FGuid& Id)
	{
		const FKnightInventoryView View = Domains.Inventory->GetView();
		const FKnightItemInstance* Value = View.Items.FindByPredicate([&Id](const FKnightItemInstance& Entry)
		{
			return Entry.InstanceId == Id;
		});
		return Value == nullptr ? FKnightItemInstance() : *Value;
	};
	const FGuid RuneId = Items.FindByPredicate([](const FKnightItemInstance& Entry)
	{
		return Entry.DefinitionId == FKnightGearCatalog::DefinitionId(TEXT("R01"));
	})->InstanceId;
	FKnightRPGCommand Attach = Command(Domains, 0, EKnightRPGOperation::AttachWeaponRune);
	Attach.ItemInstanceId = WeaponId;
	Attach.ExpectedItemRevision = FindItem(WeaponId).ItemRevision;
	Attach.WeaponRuneInstanceId = RuneId;
	TestTrue(TEXT("Aftershock attaches to the exact hammer parent"), Domains.RPG->ExecuteCommand(Owner, Attach).Result.IsSuccess());
	TestTrue(TEXT("Rune has one explicit parent and no bag cell"), FindItem(RuneId).ParentWeaponId == WeaponId
		&& FindItem(RuneId).LocationId == KnightGear::WeaponSocketLocation);
	const int64 CurrencyBefore = Domains.RPG->GetView().Currency;
	UKnightSkillRuntimeComponent* Runtime = Owner.Pawn->FindComponentByClass<UKnightSkillRuntimeComponent>();
	if (!TestNotNull(TEXT("The actual upgraded Knight owns its skill and equipment runtime"), Runtime))
	{
		return false;
	}
	FKnightSkillRuntimeSnapshot UpgradeTimers = Runtime->CaptureSnapshot();
	if (!TestTrue(TEXT("The initialized runtime exposes all four real action cooldowns"),
		UpgradeTimers.CooldownRemaining.Num() == 4))
	{
		return false;
	}
	UpgradeTimers.CooldownRemaining[3] = 20.0f;
	UpgradeTimers.AftershockRemaining = 3.0f;
	if (!TestTrue(TEXT("Upgrade fixture retains a spent major cooldown and Aftershock family ICD"),
		Runtime->RestoreSnapshot(UpgradeTimers).IsSuccess()))
	{
		return false;
	}
	const auto MaterialCount = [&Domains](FName CatalogId)
	{
		const FPrimaryAssetId DefinitionId = FKnightGearCatalog::DefinitionId(CatalogId);
		const FKnightInventoryView CurrentInventory = Domains.Inventory->GetView();
		int32 Count = 0;
		for (const FKnightItemInstance& Value : CurrentInventory.Items)
		{
			if (Value.DefinitionId == DefinitionId)
			{
				Count += Value.Count;
			}
		}
		return Count;
	};
	const int32 MaterialCountsBefore[] { MaterialCount(TEXT("M01")), MaterialCount(TEXT("M02")),
		MaterialCount(TEXT("M03")) };
	FKnightRPGCommand Upgrade = Command(Domains, 0, EKnightRPGOperation::UpgradeItem);
	Upgrade.ItemInstanceId = WeaponId;
	Upgrade.ExpectedItemRevision = FindItem(WeaponId).ItemRevision;
	TestTrue(TEXT("Fixed-cost rank one upgrade commits"), Domains.RPG->ExecuteCommand(Owner, Upgrade).Result.IsSuccess());
	TestTrue(TEXT("Upgrade preserves exact GUID, rune and costs twenty currency"), FindItem(WeaponId).UpgradeRank == 1
		&& FindItem(WeaponId).RuneAttachmentId == RuneId && Domains.RPG->GetView().Currency == CurrencyBefore - 20);
	const int32 MaterialCosts[] { 4, 2, 1 };
	for (int32 Index = 0; Index < 3; ++Index)
	{
		const FName CatalogId(*FString::Printf(TEXT("M%02d"), Index + 1));
		TestEqual(FString::Printf(TEXT("Upgrade consumes exactly%d of actual %s"), MaterialCosts[Index],
			*CatalogId.ToString()), MaterialCount(CatalogId), MaterialCountsBefore[Index] - MaterialCosts[Index]);
	}
	TestTrue(TEXT("Successful upgrade retains both major cooldown and Aftershock ICD"),
		Runtime->CaptureSnapshot().CooldownRemaining[3] == 20.0f
		&& Runtime->CaptureSnapshot().AftershockRemaining == 3.0f);
	const FKnightInventoryView Upgraded = Domains.Inventory->GetView();
	FKnightRPGSnapshot UpgradedSnapshot;
	if (!TestTrue(TEXT("The real upgraded domains capture exact pools cooldowns costs and receipt"),
		Domains.RPG->CaptureSnapshot(UpgradedSnapshot).IsSuccess()))
	{
		return false;
	}
	TestTrue(TEXT("Exact successful upgrade replay does not spend twice"), Domains.RPG->ExecuteCommand(Owner, Upgrade).bWasReplay
		&& Domains.Inventory->GetView() == Upgraded);
	FKnightRPGCommand StaleUpgrade = Command(Domains, 0, EKnightRPGOperation::UpgradeItem);
	StaleUpgrade.ItemInstanceId = WeaponId;
	StaleUpgrade.ExpectedItemRevision = Upgrade.ExpectedItemRevision;
	TestEqual(TEXT("Fresh upgrade with the old exact item revision rejects"),
		Domains.RPG->ExecuteCommand(Owner, StaleUpgrade).Result.Error, EKnightError::InvalidRevision);
	FKnightRPGCommand SecondUpgrade = Command(Domains, 0, EKnightRPGOperation::UpgradeItem);
	SecondUpgrade.ItemInstanceId = WeaponId;
	SecondUpgrade.ExpectedItemRevision = FindItem(WeaponId).ItemRevision;
	TestEqual(TEXT("Fresh rank-one second upgrade rejects with all remaining costs available"),
		Domains.RPG->ExecuteCommand(Owner, SecondUpgrade).Result.Error, EKnightError::InsufficientResources);
	FKnightRPGSnapshot AfterRejectedUpgrades;
	TestTrue(TEXT("Upgrade replay stale-item and rank-cap rejection retain every count revision receipt pool and cooldown"),
		Domains.RPG->CaptureSnapshot(AfterRejectedUpgrades).IsSuccess() && AfterRejectedUpgrades == UpgradedSnapshot
		&& !Owner.AbilitySystem->IsMutationFenced()
		&& !Domains.Inventory->IsReserved());
	FKnightRPGCommand Transfer = Command(Domains, 0, EKnightRPGOperation::TransferEquipment);
	Transfer.ItemInstanceId = WeaponId;
	Transfer.ExpectedItemRevision = FindItem(WeaponId).ItemRevision;
	Transfer.TargetKnightId = Domains.Registrations[1].KnightId;
	Transfer.ExpectedTargetKnightRevision = Domains.RPG->GetKnightView(Transfer.TargetKnightId).Revision;
	Transfer.SlotTag = KnightGameplayTags::Slot_Equipment_Weapon;
	FKnightItemStatBonuses SourcePreview;
	FKnightItemStatBonuses TargetPreview;
	TestTrue(TEXT("Pure transfer compare evaluates both final source and target contributions"),
		Domains.RPG->PreviewTransfer(Owner.KnightId, Transfer.TargetKnightId, WeaponId, Transfer.SlotTag,
			SourcePreview, TargetPreview).IsSuccess() && SourcePreview.AttackPower == 10.0f
		&& TargetPreview.AttackPower == 20.0f && Domains.Inventory->GetView() == Upgraded);
	FKnightRPGCallerContext Denied = Owner;
	Denied.EditableKnights.Reset();
	const FKnightRPGView BeforeDenied = Domains.RPG->GetView();
	TestEqual(TEXT("Both Knight permissions are mandatory"), Domains.RPG->ExecuteCommand(Denied, Transfer).Result.Error, EKnightError::NotOwned);
	TestTrue(TEXT("Permission rejection mutates no confirmed state"), Domains.RPG->GetView() == BeforeDenied
		&& Domains.Inventory->GetView() == Upgraded);
	TestTrue(TEXT("Authorized transfer commits both Knight revisions"), Domains.RPG->ExecuteCommand(Owner, Transfer).Result.IsSuccess());
	TestTrue(TEXT("Transfer carries the exact parent rune"), FindItem(RuneId).AssignedKnightId == Transfer.TargetKnightId
		&& FindItem(RuneId).ParentWeaponId == WeaponId && !Domains.RPG->GetKnightView(Owner.KnightId).WeaponRuneInstanceId.IsValid());
	FKnightRPGSnapshot Snapshot;
	if (!TestTrue(TEXT("Complete currency/location/rank/receipt snapshot validates"), Domains.RPG->CaptureSnapshot(Snapshot).IsSuccess()))
	{
		return false;
	}
	FKnightDomains Restored = Fixture.MakeDomains(&Domains.Registrations, nullptr, true, true);
	TestTrue(TEXT("Fresh source owners reconstruct the exact gear fragment"), Restored.InitializationResult.IsSuccess()
		&& Restored.RPG->TryRestoreSnapshot(Restored.Inventory, Snapshot, Restored.Registrations).IsSuccess()
		&& Restored.RPG->GetView() == Domains.RPG->GetView() && Restored.Inventory->GetView() == Domains.Inventory->GetView());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKnightGearRejectionTest, "Knight.Items.M56.RequirementsConsumablesAndRewardRollback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FKnightGearRejectionTest::RunTest(const FString& Parameters)
{
	using namespace KnightRPGTests;
	FKnightWorld Fixture;
	FKnightDomains Domains = Fixture.MakeDomains(nullptr, nullptr, true, true);
	const FGuid DemoId = FGuid::NewGuid();
	if (!TestTrue(TEXT("Isolated level-one gear domain initializes"), Domains.InitializationResult.IsSuccess()
		&& Domains.Inventory->InitializeInventory(128).IsSuccess() && ConfigureLab(Domains).IsSuccess()
		&& Domains.RPG->ConfigureGearDemo(DemoId, 1000, 1, true).IsSuccess()))
	{
		return false;
	}
	TArray<FKnightItemInstance> Values;
	auto Add = [&Values](FName Id, int32 Count, int32 Copies = 1)
	{
		for (int32 Index = 0; Index < Copies; ++Index)
		{
			FKnightItemInstance Value;
			Value.InstanceId = FGuid::NewGuid();
			Value.DefinitionId = FKnightGearCatalog::DefinitionId(Id);
			Value.Count = Count;
			Values.Add(Value);
		}
	};
	Add(TEXT("W01"), 1, 3);
	for (int32 Index = 1; Index <= 7; ++Index)
	{
		Add(FName(*FString::Printf(TEXT("A%02d"), Index)), 1, 3);
	}
	Add(TEXT("C01"), 6);
	Add(TEXT("C02"), 6);
	Add(TEXT("W02"), 1);
	Add(TEXT("W05"), 1);
	Add(TEXT("F03"), 1);
	Add(TEXT("J01"), 2);
	Add(TEXT("M01"), 1);
	Add(TEXT("Q01"), 1);
	if (!TestTrue(TEXT("Starter initialization assigns 24 genuine item instances before publication"),
		Domains.Inventory->TryApplyTransaction(Seed(Domains.Inventory, Values)).Result.IsSuccess()
		&& Domains.RPG->InitializeRPG(Domains.Inventory, Fixture.Tree->GetPrimaryAssetId(), Domains.Registrations, 5).IsSuccess()))
	{
		return false;
	}
	FKnightRPGSnapshot SeededSnapshot;
	const FKnightResult SeededCapture = Domains.RPG->CaptureSnapshot(SeededSnapshot);
	if (!TestTrue(FString::Printf(TEXT("Initial24-assignment seed is canonical before the first command: %s/%d %s"),
		*SeededCapture.Context.ToString(), static_cast<int32>(SeededCapture.Error), *SeededCapture.Message.ToString()),
		SeededCapture.IsSuccess() && SeededSnapshot.InitialView == Domains.RPG->GetView()
		&& SeededSnapshot.Receipts.IsEmpty()))
	{
		return false;
	}
	FKnightRPGCallerContext Owner = Caller(Domains, FGuid::NewGuid());
	const FKnightRPGKnightView Starter = Domains.RPG->GetKnightView(Owner.KnightId);
	TestTrue(TEXT("Starter aggregate matches P10/H140/A36 and two stable quick bindings"),
		Starter.Assignments.Num() == 8 && Starter.StatBonuses.AttackPower == 10.0f
		&& Starter.StatBonuses.MaxHealth == 140.0f && Starter.StatBonuses.Defense == 36.0f
		&& Starter.ConsumableBindings.Num() == 2);
	const FGuid RareId = Values.FindByPredicate([](const FKnightItemInstance& Item)
	{
		return Item.DefinitionId == FKnightGearCatalog::DefinitionId(TEXT("W02"));
	})->InstanceId;
	FKnightRPGCommand Equip = Command(Domains, 0, EKnightRPGOperation::Equip);
	Equip.ItemInstanceId = RareId;
	Equip.ExpectedItemRevision = 1;
	Equip.SlotTag = KnightGameplayTags::Slot_Equipment_Weapon;
	const FKnightInventoryView BeforeRequirement = Domains.Inventory->GetView();
	TestTrue(TEXT("Level-one Knight cannot equip level-five Rare hammer"),
		!Domains.RPG->ExecuteCommand(Owner, Equip).Result.IsSuccess()
		&& Domains.Inventory->GetView() == BeforeRequirement);
	Equip.RequestId = FGuid::NewGuid();
	Equip.SlotTag = KnightGameplayTags::Slot_Equipment_Head;
	TestTrue(TEXT("Wrong subtype cannot equip a head slot"), !Domains.RPG->ExecuteCommand(Owner, Equip).Result.IsSuccess());
	FKnightRPGCommand Upgrade = Command(Domains, 0, EKnightRPGOperation::UpgradeItem);
	Upgrade.ItemInstanceId = RareId;
	Upgrade.ExpectedItemRevision = 1;
	const FKnightRPGView BeforeUpgrade = Domains.RPG->GetView();
	TestTrue(TEXT("Incomplete material cost refunds all candidate removals"),
		!Domains.RPG->ExecuteCommand(Owner, Upgrade).Result.IsSuccess()
		&& Domains.Inventory->GetView() == BeforeRequirement && Domains.RPG->GetView() == BeforeUpgrade);
	const FGuid EpicId = Values.FindByPredicate([](const FKnightItemInstance& Item)
	{
		return Item.DefinitionId == FKnightGearCatalog::DefinitionId(TEXT("W05"));
	})->InstanceId;
	FKnightRPGCommand EpicSale = Command(Domains, 0, EKnightRPGOperation::SellItem);
	EpicSale.ItemInstanceId = EpicId;
	EpicSale.ExpectedItemRevision = 1;
	TestTrue(TEXT("Epic sale requires explicit confirmation without mutating its exact instance"),
		!Domains.RPG->ExecuteCommand(Owner, EpicSale).Result.IsSuccess()
		&& Domains.Inventory->GetView() == BeforeRequirement);
	UKnightAbilitySystemComponent* ASC = Owner.AbilitySystem.Get();
	ASC->SetNumericAttributeBase(UKnightAttributeSet::GetHealthAttribute(), ASC->GetKnightAttributes()->GetMaxHealth());
	FKnightRPGCommand Use = Command(Domains, 0, EKnightRPGOperation::UseConsumable);
	Use.QuickSlot = 0;
	const FKnightInventoryView BeforeUse = Domains.Inventory->GetView();
	TestTrue(TEXT("Full-health quick use consumes no stack and advances no revision"),
		!Domains.RPG->ExecuteCommand(Owner, Use).Result.IsSuccess() && Domains.Inventory->GetView() == BeforeUse);
	const float MaximumHealth = ASC->GetKnightAttributes()->GetMaxHealth();
	ASC->SetNumericAttributeBase(UKnightAttributeSet::GetHealthAttribute(), MaximumHealth - 100.0f);
	Use = Command(Domains, 0, EKnightRPGOperation::UseConsumable);
	if (!TestTrue(TEXT("Missing-health use commits the real stack and native heal together"),
		Domains.RPG->ExecuteCommand(Owner, Use).Result.IsSuccess()
		&& FMath::IsNearlyEqual(ASC->GetKnightAttributes()->GetHealth(),
			FMath::Min(MaximumHealth, MaximumHealth - 100.0f + MaximumHealth * 0.20f))))
	{
		return false;
	}
	const FKnightInventoryView AfterUse = Domains.Inventory->GetView();
	ASC->SetNumericAttributeBase(UKnightAttributeSet::GetRuneEnergyAttribute(), 20.0f);
	Use = Command(Domains, 0, EKnightRPGOperation::UseConsumable);
	Use.QuickSlot = 1;
	TestTrue(TEXT("Resource vial shares the same Knight cooldown and retains its count on rejection"),
		!Domains.RPG->ExecuteCommand(Owner, Use).Result.IsSuccess() && Domains.Inventory->GetView() == AfterUse
		&& ASC->GetKnightAttributes()->GetRuneEnergy() == 20.0f);
	const FKnightGearRewardBundle Reward = FKnightGearCatalog::Reward(TEXT("A"), DemoId, FGuid::NewGuid());
	TestTrue(TEXT("Authoritative stage A offer registers once"), Domains.RPG->RegisterRewardBundle(Reward).IsSuccess());
	FKnightRPGCommand Claim = Command(Domains, 0, EKnightRPGOperation::ClaimReward);
	Claim.RewardId = Reward.RewardId;
	if (!TestTrue(TEXT("Stage A claim atomically grants all party levels, jewelry, materials and currency"),
		Domains.RPG->ExecuteCommand(Owner, Claim).Result.IsSuccess() && Domains.RPG->GetView().Currency == 1030
		&& Domains.RPG->GetView().Knights.ContainsByPredicate([](const FKnightRPGKnightView& Knight) { return Knight.Level == 3; })))
	{
		return false;
	}
	const FKnightInventoryView AfterReward = Domains.Inventory->GetView();
	TestTrue(TEXT("One reward receipt replays without adding its bundle twice"),
		Domains.RPG->ExecuteCommand(Owner, Claim).bWasReplay && Domains.Inventory->GetView() == AfterReward);
	for (int32 Index = 0; Index < 2; ++Index)
	{
		const FPrimaryAssetId RingId = FKnightGearCatalog::DefinitionId(Index == 0 ? TEXT("F01") : TEXT("F02"));
		const FKnightInventoryView Current = Domains.Inventory->GetView();
		const FKnightItemInstance* Ring = Current.Items.FindByPredicate([&RingId](const FKnightItemInstance& Item)
		{
			return Item.DefinitionId == RingId && Item.LocationId == KnightGear::InventoryLocation;
		});
		Equip = Command(Domains, 0, EKnightRPGOperation::Equip);
		Equip.ItemInstanceId = Ring->InstanceId;
		Equip.ExpectedItemRevision = Ring->ItemRevision;
		Equip.SlotTag = Index == 0 ? FGameplayTag(KnightGameplayTags::Slot_Equipment_Ring1)
			: FGameplayTag(KnightGameplayTags::Slot_Equipment_Ring2);
		TestTrue(TEXT("Level-three reward jewelry meets its real requirement"), Domains.RPG->ExecuteCommand(Owner, Equip).Result.IsSuccess());
	}
	const FGuid BeforeFirstRing = Domains.RPG->GetKnightView(Owner.KnightId).FindAssignment(KnightGameplayTags::Slot_Equipment_Ring1)->ItemInstanceId;
	FKnightRPGCommand RingSwap = Command(Domains, 0, EKnightRPGOperation::SwapRings);
	TestTrue(TEXT("Atomic ring swap preserves identities and exchanges explicit slot locations"),
		Domains.RPG->ExecuteCommand(Owner, RingSwap).Result.IsSuccess()
		&& Domains.RPG->GetKnightView(Owner.KnightId).FindAssignment(KnightGameplayTags::Slot_Equipment_Ring2)->ItemInstanceId == BeforeFirstRing);
	while (Domains.Inventory->GetView().GetBagCount() < 128)
	{
		FKnightRPGCommand Fill = Command(Domains, 0, EKnightRPGOperation::BuyItem);
		Fill.ItemDefinitionId = FKnightGearCatalog::DefinitionId(TEXT("W01"));
		Fill.Quantity = FMath::Min(20, 128 - Domains.Inventory->GetView().GetBagCount());
		if (!TestTrue(TEXT("Real purchases occupy final bag capacity through retained domain receipts"),
			Domains.RPG->ExecuteCommand(Owner, Fill).Result.IsSuccess()))
		{
			return false;
		}
	}
	const FKnightInventoryView Full = Domains.Inventory->GetView();
	const FKnightRPGView FullRPG = Domains.RPG->GetView();
	FKnightRPGCommand Buy = Command(Domains, 0, EKnightRPGOperation::BuyItem);
	Buy.ItemDefinitionId = FKnightGearCatalog::DefinitionId(TEXT("W01"));
	TestTrue(TEXT("Full bag rejects vendor purchase without charging"), !Domains.RPG->ExecuteCommand(Owner, Buy).Result.IsSuccess()
		&& Domains.Inventory->GetView() == Full && Domains.RPG->GetView() == FullRPG);
	const FKnightGearRewardBundle Next = FKnightGearCatalog::Reward(TEXT("B"), DemoId, FGuid::NewGuid());
	Domains.RPG->RegisterRewardBundle(Next);
	Claim = Command(Domains, 0, EKnightRPGOperation::ClaimReward);
	Claim.RewardId = Next.RewardId;
	TestTrue(TEXT("Full bag leaves stage B claimable with no partial level or currency grants"),
		!Domains.RPG->ExecuteCommand(Owner, Claim).Result.IsSuccess() && Domains.Inventory->GetView() == Full
		&& Domains.RPG->GetView() == FullRPG && Domains.RPG->GetRewardOffers().Num() == 2);
	const FKnightGearRewardBundle Last = FKnightGearCatalog::Reward(TEXT("C"), DemoId, FGuid::NewGuid());
	Domains.RPG->RegisterRewardBundle(Last);
	Claim = Command(Domains, 0, EKnightRPGOperation::ClaimReward);
	Claim.RewardId = Last.RewardId;
	TestEqual(TEXT("Duplicate unique quest token rejects the entire bundle"),
		Domains.RPG->ExecuteCommand(Owner, Claim).Result.Error, EKnightError::Conflict);
	TestTrue(TEXT("Unique-owned rejection keeps counts, levels, currency and claims unchanged"),
		Domains.Inventory->GetView() == Full && Domains.RPG->GetView() == FullRPG);
	const FKnightItemInstance* Junk = Full.Items.FindByPredicate([](const FKnightItemInstance& Item)
	{
		return Item.DefinitionId == FKnightGearCatalog::DefinitionId(TEXT("J01"));
	});
	FKnightRPGCommand Sell = Command(Domains, 0, EKnightRPGOperation::SellItem);
	Sell.ItemInstanceId = Junk->InstanceId;
	Sell.ExpectedItemRevision = Junk->ItemRevision;
	Sell.Quantity = Junk->Count;
	TestTrue(TEXT("Selling a full junk stack frees one bag cell and credits its exact scalar value"),
		Domains.RPG->ExecuteCommand(Owner, Sell).Result.IsSuccess() && Domains.Inventory->GetView().GetBagCount() == 127
		&& Domains.RPG->GetView().Currency == FullRPG.Currency + Junk->Count * 2);
	const int64 AfterSaleCurrency = Domains.RPG->GetView().Currency;
	Buy = Command(Domains, 0, EKnightRPGOperation::BuyItem);
	Buy.ItemDefinitionId = FKnightGearCatalog::DefinitionId(TEXT("W01"));
	TestTrue(TEXT("Vendor recovery consumes only the newly freed cell and five currency"),
		Domains.RPG->ExecuteCommand(Owner, Buy).Result.IsSuccess() && Domains.Inventory->GetView().GetBagCount() == 128
		&& Domains.RPG->GetView().Currency == AfterSaleCurrency - 5);
	TestTrue(TEXT("Exact sale replay never credits its retired stack twice"),
		Domains.RPG->ExecuteCommand(Owner, Sell).bWasReplay && Domains.RPG->GetView().Currency == AfterSaleCurrency - 5);
	FKnightRPGSnapshot Snapshot;
	const FKnightResult Capture = Domains.RPG->CaptureSnapshot(Snapshot);
	if (!TestTrue(FString::Printf(TEXT("Snapshot captures complete economy history: %s/%d %s"),
		*Capture.Context.ToString(), static_cast<int32>(Capture.Error), *Capture.Message.ToString()), Capture.IsSuccess()))
	{
		return false;
	}
	const FKnightRPGCombatSnapshot* Fragment = Snapshot.CombatStates.Find(Owner.KnightId);
	if (!TestTrue(TEXT("Snapshot preserves the shared consumable cooldown and three complete offers"),
		Fragment != nullptr && Fragment->Skills.ConsumableRemaining > 29.0f && Snapshot.RewardOffers.Num() == 3
		&& !Snapshot.RewardOffers[0].Items.IsEmpty()))
	{
		return false;
	}
	FKnightDomains Restored = Fixture.MakeDomains(&Domains.Registrations, nullptr, true, true);
	FKnightRPGSnapshot Corrupted = Snapshot;
	Corrupted.RewardOffers[0].Items[0].Count = 0;
	TestTrue(TEXT("Malformed persisted offer rejects before preparing any source or inventory"),
		!Restored.RPG->TryRestoreSnapshot(Restored.Inventory, Corrupted, Restored.Registrations).IsSuccess()
		&& !Restored.Inventory->IsInitialized() && Restored.RPG->GetRewardOffers().IsEmpty());
	TestTrue(TEXT("The same fresh owners can restore valid data after offer rollback"),
		Restored.RPG->TryRestoreSnapshot(Restored.Inventory, Snapshot, Restored.Registrations).IsSuccess()
		&& Restored.RPG->GetView() == Domains.RPG->GetView());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKnightLegacyRuneAdapterTest, "Knight.Items.M56.LegacyRuneParentMigration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FKnightLegacyRuneAdapterTest::RunTest(const FString& Parameters)
{
	using namespace KnightRPGTests;
	FKnightWorld Fixture;
	TStrongObjectPtr<UKnightRuneDefinition> Rune(NewObject<UKnightRuneDefinition>());
	const FKnightGearSeed& RuneSeed = *FKnightGearCatalog::GetSeeds().FindByPredicate([](const FKnightGearSeed& Seed)
	{
		return Seed.CatalogId == TEXT("R01");
	});
	FKnightGearCatalog::ConfigureDefinition(Rune.Get(), RuneSeed);
	FKnightDomains Domains = Fixture.MakeDomains(nullptr, Rune.Get(), true);
	Domains.Inventory->InitializeInventory(8);
	ConfigureLab(Domains);
	const FKnightItemInstance Hammer = Item(Fixture.Hammer.Get());
	const FKnightItemInstance Attachment = Item(Rune.Get());
	Domains.Inventory->TryApplyTransaction(Seed(Domains.Inventory, { Hammer, Attachment }));
	Domains.RPG->InitializeRPG(Domains.Inventory, Fixture.Tree->GetPrimaryAssetId(), Domains.Registrations, 5);
	const FKnightRPGCallerContext Owner = Caller(Domains, FGuid::NewGuid());
	FKnightRPGCommand Equip = Command(Domains, 0, EKnightRPGOperation::Equip);
	Equip.ItemInstanceId = Hammer.InstanceId;
	Equip.SlotTag = KnightGameplayTags::Slot_Equipment_Weapon;
	TestTrue(TEXT("Historical M5.5 hammer assignment commits"), Domains.RPG->ExecuteCommand(Owner, Equip).Result.IsSuccess());
	FKnightRPGCommand Attach = Command(Domains, 0, EKnightRPGOperation::ApplyBuild);
	Attach.ProposedBuild = Domains.RPG->GetKnightView(Owner.KnightId).Build;
	Attach.ProposedBuild.WeaponRune = TEXT("Weapon.Aftershock");
	Attach.WeaponRuneInstanceId = Attachment.InstanceId;
	TestTrue(TEXT("Historical attachment command commits"), Domains.RPG->ExecuteCommand(Owner, Attach).Result.IsSuccess());
	FKnightRPGSnapshot Snapshot;
	if (!TestTrue(TEXT("Legacy capture records the exact derived parent without rewriting item history"),
		Domains.RPG->CaptureSnapshot(Snapshot).IsSuccess()
		&& Snapshot.LegacyWeaponRuneParents.FindChecked(Attachment.InstanceId) == Hammer.InstanceId))
	{
		return false;
	}
	const TArray<FKnightRPGReceipt> OriginalReceipts = Snapshot.Receipts;
	const TArray<FKnightInventoryReceipt> OriginalInventoryReceipts = Snapshot.Inventory.Receipts;
	Snapshot.SchemaVersion = 2;
	Snapshot.InitialInventory.SchemaVersion = 1;
	Snapshot.Inventory.SchemaVersion = 1;
	Snapshot.LegacyWeaponRuneParents.Reset();
	FKnightDomains Restored = Fixture.MakeDomains(&Domains.Registrations, Rune.Get(), true);
	if (!TestTrue(TEXT("M5.5 schema migrates to fresh canonical source owners"),
		Restored.RPG->TryRestoreSnapshot(Restored.Inventory, Snapshot, Restored.Registrations).IsSuccess()))
	{
		return false;
	}
	FKnightRPGSnapshot Adapted;
	TestTrue(TEXT("Parent adapter preserves exact RPG and inventory receipt payloads"),
		Restored.RPG->CaptureSnapshot(Adapted).IsSuccess() && Adapted.Receipts == OriginalReceipts
		&& Adapted.Inventory.Receipts == OriginalInventoryReceipts
		&& Restored.RPG->GetWeaponRuneParentId(Attachment.InstanceId) == Hammer.InstanceId
		&& Adapted.LegacyWeaponRuneParents.FindChecked(Attachment.InstanceId) == Hammer.InstanceId);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKnightLegacyItemizationTest, "Knight.Items.M56.LegacyItemizationBoundary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FKnightLegacyItemizationTest::RunTest(const FString& Parameters)
{
	using namespace KnightRPGTests;
	for (int32 LegacySchema : { 1, 2 })
	{
		FKnightWorld Fixture;
		FKnightItemStatBonuses RuneStats;
		RuneStats.MaxHealth = 50.0f;
		Fixture.Rune->TrySetItemTuning(EKnightItemKind::Rune, 1, FPrimaryAssetId(), RuneStats);
		FKnightDomains Legacy = Fixture.MakeDomains(nullptr, nullptr, true, true);
		const bool bSkillLab = LegacySchema == 2;
		const int32 Capacity = bSkillLab ? 3 : 2;
		if (!TestTrue(TEXT("Historical domains initialize with their original bag capacity"),
			Legacy.InitializationResult.IsSuccess() && Legacy.Inventory->InitializeInventory(Capacity).IsSuccess()
			&& (!bSkillLab || ConfigureLab(Legacy).IsSuccess())))
		{
			return false;
		}
		const FKnightItemInstance Hammer = Item(Fixture.Hammer.Get());
		const FKnightItemInstance OldRune = Item(Fixture.Rune.Get());
		TArray<FKnightItemInstance> Items { Hammer, OldRune };
		FKnightItemInstance Attachment;
		if (bSkillLab)
		{
			Attachment.InstanceId = FGuid::NewGuid();
			Attachment.DefinitionId = FKnightGearCatalog::DefinitionId(TEXT("R01"));
			Attachment.Count = 1;
			Items.Add(Attachment);
		}
		if (!TestTrue(TEXT("Actual old items fill the legacy bag before assignments"),
			Legacy.Inventory->TryApplyTransaction(Seed(Legacy.Inventory, Items)).Result.IsSuccess()
			&& Legacy.RPG->InitializeRPG(Legacy.Inventory, Fixture.Tree->GetPrimaryAssetId(),
				Legacy.Registrations, 5).IsSuccess()))
		{
			return false;
		}
		const FKnightRPGCallerContext Owner = Caller(Legacy, FGuid::NewGuid());
		FKnightRPGCommand Equip = Command(Legacy, 0, EKnightRPGOperation::Equip);
		Equip.ItemInstanceId = Hammer.InstanceId;
		Equip.SlotTag = KnightGameplayTags::Slot_Equipment_Weapon;
		FKnightResult Result = Legacy.RPG->ExecuteCommand(Owner, Equip).Result;
		FKnightRPGCommand Rune = Command(Legacy, 0, EKnightRPGOperation::Rune);
		Rune.ItemInstanceId = OldRune.InstanceId;
		Rune.SlotTag = KnightGameplayTags::Slot_Equipment_Rune;
		if (!TestTrue(TEXT("Valid historical mapped Weapon and unmapped Rune assignments commit"),
			Result.IsSuccess() && Legacy.RPG->ExecuteCommand(Owner, Rune).Result.IsSuccess()))
		{
			return false;
		}
		if (bSkillLab)
		{
			FKnightRPGCommand Attach = Command(Legacy, 0, EKnightRPGOperation::ApplyBuild);
			Attach.ProposedBuild = Legacy.RPG->GetKnightView(Owner.KnightId).Build;
			Attach.ProposedBuild.WeaponRune = TEXT("Weapon.Aftershock");
			Attach.WeaponRuneInstanceId = Attachment.InstanceId;
			if (!TestTrue(TEXT("Historical M5.5 attachment reserves its exact known GUID"),
				Legacy.RPG->ExecuteCommand(Owner, Attach).Result.IsSuccess()))
			{
				return false;
			}
			TestEqual(TEXT("Native legacy fixture maximum includes base100, weapon20 and old Rune50"),
				Owner.AbilitySystem->GetKnightAttributes()->GetMaxHealth(), 170.0f);
			Owner.AbilitySystem->SetNumericAttributeBase(UKnightAttributeSet::GetHealthAttribute(), 160.0f);
			TestEqual(TEXT("Migration retains an actually damaged source pool before withdrawing the old Rune"),
				Owner.AbilitySystem->GetKnightAttributes()->GetHealth(), 160.0f);
		}
		FKnightRPGSnapshot Snapshot;
		if (!TestTrue(TEXT("Actual source captures before the legacy schema fixture is retained"),
			Legacy.RPG->CaptureSnapshot(Snapshot).IsSuccess()))
		{
			return false;
		}
		Snapshot.SchemaVersion = LegacySchema;
		Snapshot.InitialInventory.SchemaVersion = 1;
		Snapshot.Inventory.SchemaVersion = 1;
		Snapshot.LegacyWeaponRuneParents.Reset();
		const FKnightRPGSnapshot Original = Snapshot;
		if (!TestTrue(TEXT("The actual historical equip and Rune commands retained their receipt prefix"),
			!Snapshot.Receipts.IsEmpty()))
		{
			return false;
		}
		FKnightDomains Migrated = Fixture.MakeDomains(&Legacy.Registrations, nullptr, true, true);
		for (int32 FutureField = 0; FutureField < 2; ++FutureField)
		{
			FKnightRPGSnapshot ForgedReceipt = Snapshot;
			if (FutureField == 0)
			{
				ForgedReceipt.Receipts[0].CommittedKnights.Add(ForgedReceipt.Receipts[0].CommittedKnight);
			}
			else
			{
				ForgedReceipt.Receipts[0].CommittedCurrency = 1;
			}
			const FKnightRPGSnapshot ForgedReceiptOriginal = ForgedReceipt;
			Result = Migrated.RPG->TryRestoreItemizationSnapshot(Migrated.Inventory, ForgedReceipt,
				Migrated.Registrations, FGuid::NewGuid());
			TestTrue(FString::Printf(TEXT("Schema%d rejects future receipt field%d before any mutation"),
				LegacySchema, FutureField), Result.Context == TEXT("LegacyGearReceipt")
				&& Result.Error == EKnightError::Conflict && !Migrated.RPG->IsInitialized()
				&& !Migrated.Inventory->IsInitialized() && ForgedReceipt == ForgedReceiptOriginal && Snapshot == Original);
		}
		FKnightRPGSnapshot ForgedLegacy = Snapshot;
		ForgedLegacy.Inventory.Items[0].ItemRevision = 2;
		const FKnightRPGSnapshot ForgedOriginal = ForgedLegacy;
		Result = Migrated.RPG->TryRestoreItemizationSnapshot(Migrated.Inventory, ForgedLegacy,
			Migrated.Registrations, FGuid::NewGuid());
		TestTrue(TEXT("An old schema cannot smuggle new item revisions; reject before any fresh-domain mutation"),
			Result.Context == TEXT("LegacyInventoryFields") && !Result.IsSuccess()
			&& !Migrated.RPG->IsInitialized() && !Migrated.Inventory->IsInitialized()
			&& ForgedLegacy == ForgedOriginal && Snapshot == Original);
		FKnightRPGSnapshot Overcapacity = Snapshot;
		Overcapacity.InitialInventory.Capacity = 1;
		Overcapacity.Inventory.Capacity = 1;
		const FKnightRPGSnapshot OvercapacityOriginal = Overcapacity;
		TestTrue(TEXT("Overcapacity legacy import rejects atomically without editing the source snapshot"),
			!Migrated.RPG->TryRestoreItemizationSnapshot(Migrated.Inventory, Overcapacity,
				Migrated.Registrations, FGuid::NewGuid()).IsSuccess()
			&& !Migrated.RPG->IsInitialized() && !Migrated.Inventory->IsInitialized()
			&& Overcapacity == OvercapacityOriginal && Snapshot == Original);
		const FGuid MigrationId = FGuid::NewGuid();
		Result = Migrated.RPG->TryRestoreItemizationSnapshot(Migrated.Inventory, Snapshot,
			Migrated.Registrations, MigrationId);
		if (!TestTrue(FString::Printf(TEXT("Schema%d imports explicit itemization after unchanged legacy replay"), LegacySchema),
			Result.IsSuccess()))
		{
			AddInfo(Result.Message.ToString());
			return false;
		}
		TestTrue(TEXT("Migration enables twelve slots independently of fixture authority without any grants"),
			Migrated.RPG->GetView().IsItemizationEnabled() && !Migrated.RPG->GetView().bGearDemo
			&& Migrated.RPG->GetView().Currency == 0 && !Migrated.RPG->GetView().DemoRunId.IsValid()
			&& Migrated.Inventory->GetView().Capacity == Capacity && Snapshot == Original);
		const FKnightRPGKnightView Knight = Migrated.RPG->GetKnightView(Owner.KnightId);
		const FKnightEquipmentAssignment* MappedWeapon = Knight.FindAssignment(KnightGameplayTags::Slot_Equipment_Weapon);
		const FKnightInventoryView InventoryView = Migrated.Inventory->GetView();
		const FKnightItemInstance* Returned = InventoryView.Items.FindByPredicate([&OldRune](const FKnightItemInstance& Item)
		{
			return Item.InstanceId == OldRune.InstanceId;
		});
		TestTrue(TEXT("Unmapped old Rune returns to bag with same GUID; existing weapon maps and new slots remain empty"),
			Returned != nullptr && Returned->LocationId == KnightGear::InventoryLocation
			&& Returned->DefinitionId == OldRune.DefinitionId && Returned->Count == 1
			&& !Returned->AssignedKnightId.IsValid() && Knight.Assignments.Num() == 1
			&& MappedWeapon != nullptr && MappedWeapon->ItemInstanceId == Hammer.InstanceId
			&& Knight.FindAssignment(KnightGameplayTags::Slot_Equipment_Rune) == nullptr
			&& Knight.StatBonuses.MaxHealth == 20.0f && Knight.RuneTuning == FKnightRuneTuning()
			&& InventoryView.GetBagCount() == 1);
		if (bSkillLab)
		{
			const FKnightItemInstance* RuneItem = InventoryView.Items.FindByPredicate([&Attachment](const FKnightItemInstance& Item)
			{
				return Item.InstanceId == Attachment.InstanceId;
			});
			if (!TestNotNull(TEXT("M5.5 attachment survives with its exact existing GUID"), RuneItem))
			{
				return false;
			}
			TestEqual(TEXT("M5.5 attachment retains the exact parent weapon GUID"), RuneItem->ParentWeaponId, Hammer.InstanceId);
			TestEqual(TEXT("M5.5 attachment migrates to the physical weapon socket"),
				RuneItem->LocationId, KnightGear::WeaponSocketLocation);
			const UKnightAttributeSet* RestoredAttributes = Migrated.Registrations[0].AbilitySystem->GetKnightAttributes();
			TestEqual(TEXT("Withdrawing the old Rune removes only its50 MaxHP contribution"),
				RestoredAttributes->GetMaxHealth(), 120.0f);
			TestEqual(TEXT("The damaged160 pool clamps to120 without reconstruction refill"),
				RestoredAttributes->GetHealth(), 120.0f);
		}
		FKnightRPGSnapshot Boundary;
		if (!TestTrue(TEXT("Capture preserves immutable receipt prefix and explicit administrative boundary"),
			Migrated.RPG->CaptureSnapshot(Boundary).IsSuccess() && Boundary.Receipts == Original.Receipts
			&& Boundary.ItemizationMigration.MigrationId == MigrationId
			&& Boundary.ItemizationMigration.LegacyReceiptCount == Original.Receipts.Num()
			&& Boundary.Inventory.Receipts.Num() == Original.Inventory.Receipts.Num() + 1
			&& Boundary.Inventory.Receipts[0] == Original.Inventory.Receipts[0]))
		{
			return false;
		}
		FKnightRPGCallerContext NewOwner = Caller(Migrated, Owner.HumanId);
		FKnightRPGCommand Preset = Command(Migrated, 0, EKnightRPGOperation::ApplyGearPreset);
		Preset.PresetId = TEXT("Guard");
		TestEqual(TEXT("Migrated non-demo Knights never receive fixture preset authority"),
			Migrated.RPG->ExecuteCommand(NewOwner, Preset).Result.Error, EKnightError::NotOwned);
		FKnightRPGCommand Unequip = Command(Migrated, 0, EKnightRPGOperation::Unequip);
		Unequip.ItemInstanceId = Hammer.InstanceId;
		Unequip.ExpectedItemRevision = InventoryView.Items.FindByPredicate([&Hammer](const FKnightItemInstance& Item)
		{
			return Item.InstanceId == Hammer.InstanceId;
		})->ItemRevision;
		Unequip.SlotTag = KnightGameplayTags::Slot_Equipment_Weapon;
		TestTrue(TEXT("A new real twelve-slot command commits after the administrative boundary"),
			Migrated.RPG->ExecuteCommand(NewOwner, Unequip).Result.IsSuccess());
		FKnightRPGSnapshot Mixed;
		if (!TestTrue(TEXT("Mixed old and new journal captures with preserved parent ownership"),
			Migrated.RPG->CaptureSnapshot(Mixed).IsSuccess()))
		{
			return false;
		}
		FKnightDomains Reconstructed = Fixture.MakeDomains(&Legacy.Registrations, nullptr, true, true);
		TestTrue(TEXT("Default fresh restore replays legacy prefix, deterministic boundary and new commands exactly"),
			Reconstructed.RPG->TryRestoreSnapshot(Reconstructed.Inventory, Mixed, Reconstructed.Registrations).IsSuccess()
			&& Reconstructed.RPG->GetView() == Migrated.RPG->GetView()
			&& Reconstructed.Inventory->GetView() == Migrated.Inventory->GetView());
		FKnightRPGSnapshot RoundTrip;
		TestTrue(TEXT("Captured migrated snapshot is stable through reconstruction"),
			Reconstructed.RPG->CaptureSnapshot(RoundTrip).IsSuccess() && RoundTrip == Mixed);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKnightCatalogPresetOwnershipTest, "Knight.Items.M56.CatalogPresetsAndExactRingOwnership",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FKnightCatalogPresetOwnershipTest::RunTest(const FString& Parameters)
{
	using namespace KnightRPGTests;
	FKnightWorld Fixture;
	FKnightDomains Domains = Fixture.MakeDomains(nullptr, nullptr, true, true);
	if (!TestTrue(TEXT("Real catalog starts at level8 with distinct starter equipment"),
		Domains.InitializationResult.IsSuccess() && Domains.Inventory->InitializeInventory(128).IsSuccess()
		&& ConfigureLab(Domains).IsSuccess() && Domains.RPG->ConfigureGearDemo(FGuid::NewGuid(), 1000, 8, true).IsSuccess()
		&& Domains.Inventory->TryApplyTransaction(Seed(Domains.Inventory, CatalogItems())).Result.IsSuccess()
		&& Domains.RPG->InitializeRPG(Domains.Inventory, Fixture.Tree->GetPrimaryAssetId(), Domains.Registrations, 5).IsSuccess()))
	{
		return false;
	}
	const FKnightRPGCallerContext Owner = Caller(Domains, FGuid::NewGuid());
	UKnightAbilitySystemComponent* ASC = Owner.AbilitySystem.Get();
	ASC->SetNumericAttributeBase(UKnightAttributeSet::GetHealthAttribute(), 70.0f);
	ASC->SetNumericAttributeBase(UKnightAttributeSet::GetStaminaAttribute(), 80.0f);
	ASC->SetNumericAttributeBase(UKnightAttributeSet::GetRuneEnergyAttribute(), 60.0f);
	const FGameplayAbilitySpecHandle OriginalGrant = Owner.Pawn->FindComponentByClass<UKnightSkillRuntimeComponent>()->GetSkillHandle(1);
	const TCHAR* StatNames[] { TEXT("HP"), TEXT("Power"), TEXT("Armor"), TEXT("Stamina"), TEXT("Resource"),
		TEXT("Poise bonus"), TEXT("Crit"), TEXT("Haste"), TEXT("CD recovery"), TEXT("Mastery"), TEXT("Versatility") };
	// Independent expected contributions from brief section11; values are not generated from catalog stats.
	const float Expected[4][11]
	{
		{ 140.0f, 10.0f, 36.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
		{ 380.0f, 28.0f, 97.0f, 10.0f, 0.0f, 10.0f, 0.02f, 0.0f, 0.02f, 0.04f, 0.12f },
		{ 310.0f, 32.0f, 72.0f, 10.0f, 10.0f, 10.0f, 0.02f, 0.0f, 0.05f, 0.09f, 0.07f },
		{ 310.0f, 36.0f, 72.0f, 10.0f, 0.0f, 10.0f, 0.09f, 0.0f, 0.05f, 0.04f, 0.07f }
	};
	const float NativeBase[11] { 100.0f, 25.0f, 0.0f, 100.0f, 100.0f, 0.0f, 0.10f, 0.0f, 0.0f, 0.0f, 0.0f };
	for (int32 Iteration = 0; Iteration < 20; ++Iteration)
	{
		const int32 PresetIndex = Iteration % 4;
		if (PresetIndex == 0 && Iteration != 0)
		{
			const TArray<FKnightEquipmentAssignment> PreviousAssignments = Domains.RPG->GetKnightView(Owner.KnightId).Assignments;
			for (const FKnightEquipmentAssignment& Assignment : PreviousAssignments)
			{
				FKnightRPGCommand Unequip = Command(Domains, 0, EKnightRPGOperation::Unequip);
				Unequip.ItemInstanceId = Assignment.ItemInstanceId;
				Unequip.ExpectedItemRevision = InventoryItem(Domains, Assignment.ItemInstanceId).ItemRevision;
				Unequip.SlotTag = Assignment.SlotTag;
				if (!TestTrue(TEXT("Actual starter reconstruction withdraws one existing assignment at a time"),
					Domains.RPG->ExecuteCommand(Owner, Unequip).Result.IsSuccess()))
				{
					return false;
				}
			}
			for (int32 SlotIndex = 0; SlotIndex < 8; ++SlotIndex)
			{
				const FName Id = SlotIndex == 0 ? FName(TEXT("W01"))
					: FName(*FString::Printf(TEXT("A%02d"), SlotIndex));
				if (!EquipItem(*this, Domains, Owner, 0, BagItem(Domains, Id), KnightGear::EquipmentSlots()[SlotIndex]))
				{
					return false;
				}
			}
		}
		else if (PresetIndex != 0)
		{
			FKnightRPGCommand Preset = Command(Domains, 0, EKnightRPGOperation::ApplyGearPreset);
			const FName Names[] { TEXT("Guard"), TEXT("Mend"), TEXT("Strike") };
			Preset.PresetId = Names[PresetIndex - 1];
			if (!TestTrue(TEXT("Actual catalog preset commits through RPG inventory and GAS fences"),
				Domains.RPG->ExecuteCommand(Owner, Preset).Result.IsSuccess()))
			{
				return false;
			}
		}
		const FKnightRPGKnightView Knight = Domains.RPG->GetKnightView(Owner.KnightId);
		const FKnightItemStatBonuses& Bonuses = Knight.StatBonuses;
		const float Contributions[11] { Bonuses.MaxHealth, Bonuses.AttackPower, Bonuses.Defense, Bonuses.MaxStamina,
			Bonuses.MaxRuneEnergy, Bonuses.Poise, Bonuses.CritChance, Bonuses.Haste, Bonuses.CooldownRecovery,
			Bonuses.MasteryBonus, Bonuses.Versatility };
		const UKnightAttributeSet* Attributes = ASC->GetKnightAttributes();
		const float Effective[11] { Attributes->GetMaxHealth(), Attributes->GetAttackPower(), Attributes->GetDefense(),
			Attributes->GetMaxStamina(), Attributes->GetMaxRuneEnergy(), Attributes->GetPoiseBonus(),
			Attributes->GetCritChance(), Attributes->GetHaste(), Attributes->GetCooldownRecovery(),
			Attributes->GetMasteryBonus(), Attributes->GetVersatility() };
		for (int32 StatIndex = 0; StatIndex < 11; ++StatIndex)
		{
			TestTrue(FString::Printf(TEXT("Iteration%d catalog-derived %s contribution"), Iteration, StatNames[StatIndex]),
				FMath::IsNearlyEqual(Contributions[StatIndex], Expected[PresetIndex][StatIndex], 0.001f));
			TestTrue(FString::Printf(TEXT("Iteration%d actual GAS %s has one equipment source"), Iteration, StatNames[StatIndex]),
				FMath::IsNearlyEqual(Effective[StatIndex], NativeBase[StatIndex] + Expected[PresetIndex][StatIndex], 0.001f));
		}
		TestEqual(TEXT("Catalog changes retain damaged absolute health"), Attributes->GetHealth(), 70.0f);
		TestEqual(TEXT("Catalog changes retain spent stamina"), Attributes->GetStamina(), 80.0f);
		TestEqual(TEXT("Catalog changes retain spent resource"), Attributes->GetRuneEnergy(), 60.0f);
		TestTrue(TEXT("Catalog replacement retains the exact specialization ability grant"),
			Owner.Pawn->FindComponentByClass<UKnightSkillRuntimeComponent>()->GetSkillHandle(1) == OriginalGrant);
	}
	const FKnightRPGKnightView Strike = Domains.RPG->GetKnightView(Owner.KnightId);
	TestTrue(TEXT("Different F03 and F04 definitions coexist in the two explicit ring slots"),
		Strike.FindAssignment(KnightGameplayTags::Slot_Equipment_Ring1)->DefinitionId == FKnightGearCatalog::DefinitionId(TEXT("F03"))
		&& Strike.FindAssignment(KnightGameplayTags::Slot_Equipment_Ring2)->DefinitionId == FKnightGearCatalog::DefinitionId(TEXT("F04")));
	const FKnightItemInstance SecondRing = BagItem(Domains, TEXT("F03"));
	FKnightRPGCommand Duplicate = Command(Domains, 0, EKnightRPGOperation::Equip);
	Duplicate.ItemInstanceId = SecondRing.InstanceId;
	Duplicate.ExpectedItemRevision = SecondRing.ItemRevision;
	Duplicate.SlotTag = KnightGameplayTags::Slot_Equipment_Ring2;
	const FKnightInventoryView Before = Domains.Inventory->GetView();
	const FKnightRPGView BeforeRPG = Domains.RPG->GetView();
	TestEqual(TEXT("Two distinct F03 GUIDs cannot equip on one Knight"),
		Domains.RPG->ExecuteCommand(Owner, Duplicate).Result.Error, EKnightError::Conflict);
	TestTrue(TEXT("Exact F03 rejection preserves all Knight locations stats and revisions"),
		Domains.Inventory->GetView() == Before && Domains.RPG->GetView() == BeforeRPG);
	const FKnightRPGCallerContext OtherOwner = Caller(Domains, Owner.HumanId, 1);
	if (!EquipItem(*this, Domains, OtherOwner, 1, SecondRing, KnightGameplayTags::Slot_Equipment_Ring1))
	{
		return false;
	}
	TestTrue(TEXT("F03 unique equip is per Knight, allowing another owned GUID on another Knight"),
		Domains.RPG->GetKnightView(OtherOwner.KnightId).FindAssignment(KnightGameplayTags::Slot_Equipment_Ring1)->ItemInstanceId == SecondRing.InstanceId
		&& Domains.RPG->GetKnightView(Owner.KnightId).FindAssignment(KnightGameplayTags::Slot_Equipment_Ring1)->ItemInstanceId != SecondRing.InstanceId);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKnightPhysicalRuneTransitionTest, "Knight.Items.M56.PhysicalRuneSwapDetachAndFullBagSpecRollback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FKnightPhysicalRuneTransitionTest::RunTest(const FString& Parameters)
{
	using namespace KnightRPGTests;
	FKnightWorld Fixture;
	FKnightDomains Domains = Fixture.MakeDomains(nullptr, nullptr, true, true);
	if (!TestTrue(TEXT("Physical attachment test starts with actual catalog and starter GUIDs"),
		Domains.InitializationResult.IsSuccess() && Domains.Inventory->InitializeInventory(128).IsSuccess()
		&& ConfigureLab(Domains).IsSuccess() && Domains.RPG->ConfigureGearDemo(FGuid::NewGuid(), 1000, 8, true).IsSuccess()
		&& Domains.Inventory->TryApplyTransaction(Seed(Domains.Inventory, CatalogItems())).Result.IsSuccess()
		&& Domains.RPG->InitializeRPG(Domains.Inventory, Fixture.Tree->GetPrimaryAssetId(), Domains.Registrations, 5).IsSuccess()))
	{
		return false;
	}
	const FKnightRPGCallerContext Owner = Caller(Domains, FGuid::NewGuid());
	UKnightSkillRuntimeComponent* Runtime = Owner.Pawn->FindComponentByClass<UKnightSkillRuntimeComponent>();
	const FGuid ParentId = Domains.RPG->GetKnightView(Owner.KnightId)
		.FindAssignment(KnightGameplayTags::Slot_Equipment_Weapon)->ItemInstanceId;
	const FGuid RuneId = BagItem(Domains, TEXT("R01")).InstanceId;
	FKnightRPGCommand Attach = Command(Domains, 0, EKnightRPGOperation::AttachWeaponRune);
	Attach.ItemInstanceId = ParentId;
	Attach.ExpectedItemRevision = InventoryItem(Domains, ParentId).ItemRevision;
	Attach.WeaponRuneInstanceId = RuneId;
	if (!TestTrue(TEXT("Aftershock attaches to the actual starter weapon"), Domains.RPG->ExecuteCommand(Owner, Attach).Result.IsSuccess()))
	{
		return false;
	}
	FKnightSkillRuntimeSnapshot Timers = Runtime->CaptureSnapshot();
	Timers.AftershockRemaining = 3.0f;
	Timers.CooldownRemaining[3] = 20.0f;
	TestTrue(TEXT("Retained timer fixture is accepted by the real runtime adapter"), Runtime->RestoreSnapshot(Timers).IsSuccess());
	const FKnightItemInstance Replacement = BagItem(Domains, TEXT("W02"));
	if (!EquipItem(*this, Domains, Owner, 0, Replacement, KnightGameplayTags::Slot_Equipment_Weapon))
	{
		return false;
	}
	TestTrue(TEXT("Swap retains old parent and rune in exclusive locations without implicitly transferring attachment"),
		InventoryItem(Domains, ParentId).LocationId == KnightGear::InventoryLocation
		&& InventoryItem(Domains, ParentId).RuneAttachmentId == RuneId
		&& InventoryItem(Domains, RuneId).LocationId == KnightGear::WeaponSocketLocation
		&& InventoryItem(Domains, RuneId).ParentWeaponId == ParentId
		&& !InventoryItem(Domains, RuneId).AssignedKnightId.IsValid()
		&& !Domains.RPG->GetKnightView(Owner.KnightId).WeaponRuneInstanceId.IsValid()
		&& Domains.RPG->GetKnightView(Owner.KnightId).Build.WeaponRune.IsNone());
	TestEqual(TEXT("Swapping parent cannot reset Aftershock's family ICD"), Runtime->CaptureSnapshot().AftershockRemaining, 3.0f);
	if (!EquipItem(*this, Domains, Owner, 0, InventoryItem(Domains, ParentId), KnightGameplayTags::Slot_Equipment_Weapon))
	{
		return false;
	}
	TestTrue(TEXT("Re-equipping the exact old GUID reactivates its existing Aftershock source"),
		Domains.RPG->GetKnightView(Owner.KnightId).WeaponRuneInstanceId == RuneId
		&& Runtime->GetBuild().WeaponRune == TEXT("Weapon.Aftershock")
		&& InventoryItem(Domains, RuneId).AssignedKnightId == Owner.KnightId
		&& InventoryItem(Domains, Replacement.InstanceId).LocationId == KnightGear::InventoryLocation);
	TestEqual(TEXT("Re-equipping parent retains Aftershock ICD"), Runtime->CaptureSnapshot().AftershockRemaining, 3.0f);
	FKnightRPGCommand Detach = Command(Domains, 0, EKnightRPGOperation::DetachWeaponRune);
	Detach.ItemInstanceId = ParentId;
	Detach.ExpectedItemRevision = InventoryItem(Domains, ParentId).ItemRevision;
	if (!TestTrue(TEXT("Explicit detach commits without consuming its existing rune"),
		Domains.RPG->ExecuteCommand(Owner, Detach).Result.IsSuccess()))
	{
		return false;
	}
	TestTrue(TEXT("Detach returns the same GUID to bag and removes only its parent source"),
		InventoryItem(Domains, RuneId).LocationId == KnightGear::InventoryLocation
		&& !InventoryItem(Domains, RuneId).ParentWeaponId.IsValid()
		&& InventoryItem(Domains, RuneId).Count == 1
		&& !InventoryItem(Domains, ParentId).RuneAttachmentId.IsValid()
		&& Runtime->GetBuild().WeaponRune.IsNone());
	const FGuid SkillRuneId = BagItem(Domains, TEXT("R02")).InstanceId;
	FKnightRPGCommand SkillAttach = Command(Domains, 0, EKnightRPGOperation::ApplyBuild);
	SkillAttach.ProposedBuild = Domains.RPG->GetKnightView(Owner.KnightId).Build;
	SkillAttach.ProposedBuild.SkillRune = TEXT("Skill.Convergence");
	SkillAttach.SkillRuneInstanceId = SkillRuneId;
	if (!TestTrue(TEXT("Compatible Uranus attachment moves its exact instance to the skill socket"),
		Domains.RPG->ExecuteCommand(Owner, SkillAttach).Result.IsSuccess()
		&& InventoryItem(Domains, SkillRuneId).LocationId == KnightGear::SkillSocketLocation))
	{
		return false;
	}
	while (Domains.Inventory->GetView().GetBagCount() < 128)
	{
		FKnightRPGCommand Buy = Command(Domains, 0, EKnightRPGOperation::BuyItem);
		Buy.ItemDefinitionId = FKnightGearCatalog::DefinitionId(TEXT("W01"));
		Buy.Quantity = FMath::Min(20, 128 - Domains.Inventory->GetView().GetBagCount());
		if (!TestTrue(TEXT("Real vendor items fill the final-state bag capacity"), Domains.RPG->ExecuteCommand(Owner, Buy).Result.IsSuccess()))
		{
			return false;
		}
	}
	const FKnightInventoryView Full = Domains.Inventory->GetView();
	const FKnightRPGView FullRPG = Domains.RPG->GetView();
	const FGameplayAbilitySpecHandle OldGrant = Runtime->GetSkillHandle(1);
	FKnightRPGCommand Switch = Command(Domains, 0, EKnightRPGOperation::ApplyBuild);
	Switch.ProposedBuild = Domains.RPG->GetKnightView(Owner.KnightId).Build;
	Switch.ProposedBuild.SpecId = TEXT("Venus");
	Switch.SkillRuneInstanceId = SkillRuneId;
	TestTrue(TEXT("Switch cannot retain incompatible Convergence on Venus"),
		!Domains.RPG->ExecuteCommand(Owner, Switch).Result.IsSuccess()
		&& Domains.Inventory->GetView() == Full && Domains.RPG->GetView() == FullRPG);
	Switch.RequestId = FGuid::NewGuid();
	Switch.ProposedBuild.SkillRune = NAME_None;
	Switch.SkillRuneInstanceId.Invalidate();
	TestEqual(TEXT("Atomic spec plus detach rejects when the return rune has no bag cell"),
		Domains.RPG->ExecuteCommand(Owner, Switch).Result.Error, EKnightError::CapacityExceeded);
	TestTrue(TEXT("Full-bag failure retains exact attachment, grants, revisions and timers"),
		Domains.Inventory->GetView() == Full && Domains.RPG->GetView() == FullRPG
		&& Runtime->GetBuild().SpecId == TEXT("Uranus") && Runtime->GetSkillHandle(1) == OldGrant
		&& Runtime->CaptureSnapshot().AftershockRemaining == 3.0f
		&& Runtime->CaptureSnapshot().CooldownRemaining[3] == 20.0f);
	const FKnightItemInstance Filler = BagItem(Domains, TEXT("W01"));
	FKnightRPGCommand Sale = Command(Domains, 0, EKnightRPGOperation::SellItem);
	Sale.ItemInstanceId = Filler.InstanceId;
	Sale.ExpectedItemRevision = Filler.ItemRevision;
	if (!TestTrue(TEXT("One real sale provides the exact capacity needed for a safe detach"),
		Domains.RPG->ExecuteCommand(Owner, Sale).Result.IsSuccess() && Domains.Inventory->GetView().GetBagCount() == 127))
	{
		return false;
	}
	Switch = Command(Domains, 0, EKnightRPGOperation::ApplyBuild);
	Switch.ProposedBuild = Domains.RPG->GetKnightView(Owner.KnightId).Build;
	Switch.ProposedBuild.SpecId = TEXT("Venus");
	Switch.ProposedBuild.SkillRune = NAME_None;
	TestTrue(TEXT("Recovery commits spec switch and exact Rune return together with no cooldown reset"),
		Domains.RPG->ExecuteCommand(Owner, Switch).Result.IsSuccess()
		&& Domains.Inventory->GetView().GetBagCount() == 128
		&& InventoryItem(Domains, SkillRuneId).LocationId == KnightGear::InventoryLocation
		&& !InventoryItem(Domains, SkillRuneId).AssignedKnightId.IsValid()
		&& InventoryItem(Domains, SkillRuneId).Count == 1
		&& Runtime->GetBuild().SpecId == TEXT("Venus") && Runtime->GetBuild().SkillRune.IsNone()
		&& Runtime->CaptureSnapshot().CooldownRemaining[3] == 20.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKnightConsumableDomainCoverageTest, "Knight.Items.M56.CombatConsumableFallbackAndStateRollback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FKnightConsumableDomainCoverageTest::RunTest(const FString& Parameters)
{
	using namespace KnightRPGTests;
	FKnightWorld Fixture;
	FKnightDomains Domains = Fixture.MakeDomains(nullptr, nullptr, true, true);
	FKnightItemInstance First;
	First.InstanceId = FGuid(1, 0, 0, 1);
	First.DefinitionId = FKnightGearCatalog::DefinitionId(TEXT("C01"));
	First.Count = 1;
	FKnightItemInstance Second = First;
	Second.InstanceId = FGuid(2, 0, 0, 1);
	if (!TestTrue(TEXT("Two actual last-dose stacks and server-confirmed bindings initialize"),
		Domains.InitializationResult.IsSuccess() && Domains.Inventory->InitializeInventory(8).IsSuccess()
		&& ConfigureLab(Domains).IsSuccess() && Domains.RPG->ConfigureGearDemo(FGuid::NewGuid(), 0, 8, false).IsSuccess()
		&& Domains.Inventory->TryApplyTransaction(Seed(Domains.Inventory, { Second, First })).Result.IsSuccess()
		&& Domains.RPG->InitializeRPG(Domains.Inventory, Fixture.Tree->GetPrimaryAssetId(), Domains.Registrations, 5).IsSuccess()))
	{
		return false;
	}
	const FGuid HumanId = FGuid::NewGuid();
	FKnightRPGCallerContext Owner = Caller(Domains, HumanId);
	Owner.bInSafeZone = false;
	Owner.bInCombat = true;
	UKnightAbilitySystemComponent* ASC = Owner.AbilitySystem.Get();
	UKnightSkillRuntimeComponent* Runtime = Owner.Pawn->FindComponentByClass<UKnightSkillRuntimeComponent>();
	ASC->SetNumericAttributeBase(UKnightAttributeSet::GetHealthAttribute(), 10.0f);
	const FKnightInventoryView Initial = Domains.Inventory->GetView();
	const FKnightRPGView InitialRPG = Domains.RPG->GetView();
	const FKnightSkillRuntimeSnapshot InitialRuntime = Runtime->CaptureSnapshot();
	FKnightRPGCallerContext Unauthenticated = Owner;
	Unauthenticated.HumanId.Invalidate();
	FKnightRPGCommand Use = Command(Domains, 0, EKnightRPGOperation::UseConsumable);
	TestEqual(TEXT("Unauthenticated caller cannot use the shared bag without a human identity"),
		Domains.RPG->ExecuteCommand(Unauthenticated, Use).Result.Error, EKnightError::NotOwned);
	const FGameplayTag Hardlocks[] { KnightGameplayTags::State_Combat_Staggered,
		KnightGameplayTags::State_Combat_Attacking, KnightGameplayTags::State_Combat_Traversing,
		KnightGameplayTags::State_Combat_Finishing, KnightGameplayTags::State_Life_Reviving };
	for (FGameplayTag Lock : Hardlocks)
	{
		ASC->SetLooseGameplayTagCount(Lock, 1);
		Use = Command(Domains, 0, EKnightRPGOperation::UseConsumable);
		TestTrue(FString::Printf(TEXT("Actual %s forbids quick use without count effect CD or revision changes"), *Lock.ToString()),
			!Domains.RPG->ExecuteCommand(Owner, Use).Result.IsSuccess()
			&& Domains.Inventory->GetView() == Initial && Domains.RPG->GetView() == InitialRPG
			&& ASC->GetKnightAttributes()->GetHealth() == 10.0f
			&& Runtime->CaptureSnapshot().ConsumableRemaining == InitialRuntime.ConsumableRemaining
			&& !ASC->IsMutationFenced() && !Domains.Inventory->IsReserved());
		ASC->SetLooseGameplayTagCount(Lock, 0);
	}
	FKnightRPGCallerContext Nonliving = Caller(Domains, HumanId, 2);
	Nonliving.bInSafeZone = false;
	Nonliving.bInCombat = true;
	UKnightAbilitySystemComponent* NonlivingASC = Nonliving.AbilitySystem.Get();
	UKnightLifeComponent* Life = NonlivingASC->GetLifeComponent();
	UKnightSkillRuntimeComponent* NonlivingRuntime = Nonliving.Pawn->FindComponentByClass<UKnightSkillRuntimeComponent>();
	NonlivingASC->SetNumericAttributeBase(UKnightAttributeSet::GetHealthAttribute(), 0.0f);
	TestTrue(TEXT("Actual zero-health party Knight enters Downed through the existing life component"), Life->IsDowned());
	for (int32 StateIndex = 0; StateIndex < 2; ++StateIndex)
	{
		if (StateIndex == 1)
		{
			Life->NotifyDamageWhileDowned();
			TestTrue(TEXT("Actual follow-up damage transitions Downed to Dead"), Life->IsDead());
		}
		Use = Command(Domains, 2, EKnightRPGOperation::UseConsumable);
		TestTrue(FString::Printf(TEXT("Nonliving state%d rejects with complete domain and cooldown rollback"), StateIndex),
			!Domains.RPG->ExecuteCommand(Nonliving, Use).Result.IsSuccess()
			&& Domains.Inventory->GetView() == Initial && Domains.RPG->GetView() == InitialRPG
			&& NonlivingASC->GetKnightAttributes()->GetHealth() == 0.0f
			&& NonlivingRuntime->CaptureSnapshot().ConsumableRemaining == 0.0f);
	}
	Use = Command(Domains, 0, EKnightRPGOperation::UseConsumable);
	const FKnightRPGCommandResult FirstUse = Domains.RPG->ExecuteCommand(Owner, Use);
	if (!TestTrue(TEXT("Living idle human can use consumable during combat outside sanctuary"), FirstUse.Result.IsSuccess()))
	{
		AddInfo(FirstUse.Result.Message.ToString());
		return false;
	}
	TestTrue(TEXT("Server stable ordering consumes the lower GUID's last dose and leaves the other stack"),
		FirstUse.AffectedInstanceId == First.InstanceId && !InventoryItem(Domains, First.InstanceId).InstanceId.IsValid()
		&& InventoryItem(Domains, Second.InstanceId).Count == 1
		&& Domains.RPG->GetKnightView(Owner.KnightId).ConsumableBindings[0] == First.DefinitionId);
	TestEqual(TEXT("Combat potion applies exactly twenty percent of actual100 MaxHP"), ASC->GetKnightAttributes()->GetHealth(), 30.0f);
	TestEqual(TEXT("Successful use commits shared thirty-second Knight cooldown"), Runtime->CaptureSnapshot().ConsumableRemaining, 30.0f);
	const FKnightInventoryView AfterFirst = Domains.Inventory->GetView();
	const FKnightRPGView AfterFirstRPG = Domains.RPG->GetView();
	TestTrue(TEXT("Exact successful request replay cannot heal or consume another stack"),
		Domains.RPG->ExecuteCommand(Owner, Use).bWasReplay && Domains.Inventory->GetView() == AfterFirst
		&& Domains.RPG->GetView() == AfterFirstRPG && ASC->GetKnightAttributes()->GetHealth() == 30.0f
		&& Runtime->CaptureSnapshot().ConsumableRemaining == 30.0f);
	FKnightRPGCallerContext NextOwner = Caller(Domains, HumanId, 1);
	NextOwner.bInSafeZone = false;
	NextOwner.bInCombat = true;
	NextOwner.AbilitySystem->SetNumericAttributeBase(UKnightAttributeSet::GetHealthAttribute(), 10.0f);
	Use = Command(Domains, 1, EKnightRPGOperation::UseConsumable);
	const FKnightRPGCommandResult Fallback = Domains.RPG->ExecuteCommand(NextOwner, Use);
	TestTrue(TEXT("Unchanged DefinitionId binding falls back to the remaining same-type GUID for another controlled Knight"),
		Fallback.Result.IsSuccess() && Fallback.AffectedInstanceId == Second.InstanceId
		&& !InventoryItem(Domains, Second.InstanceId).InstanceId.IsValid()
		&& Domains.RPG->GetKnightView(NextOwner.KnightId).ConsumableBindings[0] == First.DefinitionId
		&& NextOwner.AbilitySystem->GetKnightAttributes()->GetHealth() == 30.0f);
	const FKnightInventoryView Empty = Domains.Inventory->GetView();
	const FKnightRPGView EmptyRPG = Domains.RPG->GetView();
	Use = Command(Domains, 0, EKnightRPGOperation::UseConsumable);
	TestEqual(TEXT("Depleted bound definition reports missing stack instead of creating a dose"),
		Domains.RPG->ExecuteCommand(Owner, Use).Result.Error, EKnightError::InsufficientResources);
	TestTrue(TEXT("Missing-stack rejection preserves effect count revision and retained per-Knight cooldown"),
		Domains.Inventory->GetView() == Empty && Domains.RPG->GetView() == EmptyRPG
		&& ASC->GetKnightAttributes()->GetHealth() == 30.0f && Runtime->CaptureSnapshot().ConsumableRemaining == 30.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKnightConsumableVetoTest, "Knight.Items.M56.ConsumableApplicationVetoRollback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FKnightConsumableVetoTest::RunTest(const FString& Parameters)
{
	using namespace KnightRPGTests;
	for (int32 QuickSlot = 0; QuickSlot < 2; ++QuickSlot)
	{
		FKnightWorld Fixture;
		FKnightDomains Domains = Fixture.MakeDomains(nullptr, nullptr, true, true);
		if (!TestTrue(TEXT("Actual catalog inventory and native consumable runtime initialize"),
			Domains.InitializationResult.IsSuccess() && Domains.Inventory->InitializeInventory(128).IsSuccess()
			&& ConfigureLab(Domains).IsSuccess()
			&& Domains.RPG->ConfigureGearDemo(FGuid::NewGuid(), 23, 8, false).IsSuccess()
			&& Domains.Inventory->TryApplyTransaction(Seed(Domains.Inventory, CatalogItems())).Result.IsSuccess()
			&& Domains.RPG->InitializeRPG(Domains.Inventory, Fixture.Tree->GetPrimaryAssetId(),
				Domains.Registrations, 5).IsSuccess()))
		{
			return false;
		}
		FKnightRPGCallerContext Owner = Caller(Domains, FGuid::NewGuid());
		Owner.bInSafeZone = false;
		Owner.bInCombat = true;
		UKnightAbilitySystemComponent* ASC = Owner.AbilitySystem.Get();
		ASC->SetNumericAttributeBase(UKnightAttributeSet::GetHealthAttribute(), 10.0f);
		ASC->SetNumericAttributeBase(UKnightAttributeSet::GetRuneEnergyAttribute(), 20.0f);
		const FKnightItemInstance Dose = BagItem(Domains, QuickSlot == 0 ? TEXT("C01") : TEXT("C02"));
		FKnightRPGCommand Use = Command(Domains, 0, EKnightRPGOperation::UseConsumable);
		Use.QuickSlot = QuickSlot;
		FKnightRPGSnapshot Before;
		if (!TestTrue(TEXT("The damaged domain captures every count receipt currency pool and cooldown"),
			Dose.InstanceId.IsValid() && Domains.RPG->CaptureSnapshot(Before).IsSuccess()))
		{
			return false;
		}
		int32 RejectedApplications = 0;
		FGameplayEffectApplicationQuery Reject = FGameplayEffectApplicationQuery::CreateLambda(
			[&RejectedApplications](const FActiveGameplayEffectsContainer&, const FGameplayEffectSpec& Spec)
			{
				++RejectedApplications;
				return Spec.Def->DurationPolicy != EGameplayEffectDurationType::Instant;
			});
		const FDelegateHandle RejectHandle = Reject.GetHandle();
		ASC->GameplayEffectApplicationQueries.Add(MoveTemp(Reject));
		ON_SCOPE_EXIT
		{
			ASC->GameplayEffectApplicationQueries.RemoveAll([RejectHandle](const FGameplayEffectApplicationQuery& Query)
			{
				return Query.GetHandle() == RejectHandle;
			});
		};
		const FKnightRPGCommandResult Rejected = Domains.RPG->ExecuteCommand(Owner, Use);
		FKnightRPGSnapshot AfterRejected;
		TestTrue(FString::Printf(TEXT("Actual quick slot%d GE veto rejects before any domain effect or receipt commits"),
			QuickSlot), !Rejected.Result.IsSuccess() && RejectedApplications == 1
			&& Domains.RPG->CaptureSnapshot(AfterRejected).IsSuccess() && AfterRejected == Before
			&& !ASC->IsMutationFenced() && !Domains.Inventory->IsReserved()
			&& Domains.RPG->QueryCommittedCommand(Owner, Use).Result.Error == EKnightError::NotFound);
		ASC->GameplayEffectApplicationQueries.RemoveAll([RejectHandle](const FGameplayEffectApplicationQuery& Query)
		{
			return Query.GetHandle() == RejectHandle;
		});
		int32 ApprovedApplications = 0;
		FGameplayEffectApplicationQuery ApproveOnce = FGameplayEffectApplicationQuery::CreateLambda(
			[&ApprovedApplications](const FActiveGameplayEffectsContainer&, const FGameplayEffectSpec&)
			{
				return ++ApprovedApplications == 1;
			});
		const FDelegateHandle ApprovedHandle = ApproveOnce.GetHandle();
		ASC->GameplayEffectApplicationQueries.Add(MoveTemp(ApproveOnce));
		ON_SCOPE_EXIT
		{
			ASC->GameplayEffectApplicationQueries.RemoveAll([ApprovedHandle](const FGameplayEffectApplicationQuery& Query)
			{
				return Query.GetHandle() == ApprovedHandle;
			});
		};
		const FKnightRPGCommandResult Committed = Domains.RPG->ExecuteCommand(Owner, Use);
		if (!TestTrue(TEXT("The same uncommitted request consumes and recovers once after actual approval"),
			Committed.Result.IsSuccess() && !Committed.bWasReplay && ApprovedApplications == 1
			&& Committed.AffectedInstanceId == Dose.InstanceId
			&& InventoryItem(Domains, Dose.InstanceId).Count == Dose.Count - 1
			&& ASC->GetKnightAttributes()->GetHealth() == (QuickSlot == 0 ? 30.0f : 10.0f)
			&& ASC->GetKnightAttributes()->GetRuneEnergy() == (QuickSlot == 1 ? 50.0f : 20.0f)))
		{
			return false;
		}
		FKnightRPGSnapshot AfterCommitted;
		FKnightRPGSnapshot AfterReplay;
		TestTrue(TEXT("Committed quick use retains exact thirty-second CD and one receipt through replay"),
			Domains.RPG->CaptureSnapshot(AfterCommitted).IsSuccess()
			&& AfterCommitted.CombatStates.FindChecked(Owner.KnightId).Skills.ConsumableRemaining == 30.0f
			&& Domains.RPG->ExecuteCommand(Owner, Use).bWasReplay
			&& Domains.RPG->CaptureSnapshot(AfterReplay).IsSuccess() && AfterReplay == AfterCommitted
			&& ApprovedApplications == 1);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKnightCommittedQuerySwitchTest, "Knight.Items.M56.CommittedQueryAfterKnightSwitch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FKnightCommittedQuerySwitchTest::RunTest(const FString& Parameters)
{
	using namespace KnightRPGTests;
	FKnightWorld Fixture;
	FKnightDomains Domains = Fixture.MakeDomains(nullptr, nullptr, true, true);
	if (!TestTrue(TEXT("Committed-history fixture initializes real catalog, inventory and canonical ASCs"),
		Domains.InitializationResult.IsSuccess() && Domains.Inventory->InitializeInventory(128).IsSuccess()
		&& ConfigureLab(Domains).IsSuccess() && Domains.RPG->ConfigureGearDemo(FGuid::NewGuid(), 1000, 8, true).IsSuccess()
		&& Domains.Inventory->TryApplyTransaction(Seed(Domains.Inventory, CatalogItems())).Result.IsSuccess()
		&& Domains.RPG->InitializeRPG(Domains.Inventory, Fixture.Tree->GetPrimaryAssetId(), Domains.Registrations, 5).IsSuccess()))
	{
		return false;
	}
	FKnightRPGCallerContext Original = Caller(Domains, FGuid::NewGuid());
	for (const FKnightRPGKnightRegistration& Registration : Domains.Registrations)
	{
		Original.EditableKnights.Add(Registration.KnightId);
	}
	Original.AbilitySystem->SetNumericAttributeBase(UKnightAttributeSet::GetHealthAttribute(), 10.0f);
	const FKnightRPGCommand Use = Command(Domains, 0, EKnightRPGOperation::UseConsumable);
	const FKnightRPGCommandResult CommittedUse = Domains.RPG->ExecuteCommand(Original, Use);
	if (!TestTrue(TEXT("History begins with a real consumed stack, heal and committed cooldown"), CommittedUse.Result.IsSuccess()))
	{
		return false;
	}
	const FGuid WeaponId = Domains.RPG->GetKnightView(Original.KnightId)
		.FindAssignment(KnightGameplayTags::Slot_Equipment_Weapon)->ItemInstanceId;
	FKnightRPGCommand Transfer = Command(Domains, 0, EKnightRPGOperation::TransferEquipment);
	Transfer.ItemInstanceId = WeaponId;
	Transfer.ExpectedItemRevision = InventoryItem(Domains, WeaponId).ItemRevision;
	Transfer.TargetKnightId = Domains.Registrations[1].KnightId;
	Transfer.ExpectedTargetKnightRevision = Domains.RPG->GetKnightView(Transfer.TargetKnightId).Revision;
	Transfer.SlotTag = KnightGameplayTags::Slot_Equipment_Weapon;
	const FKnightRPGCommandResult CommittedTransfer = Domains.RPG->ExecuteCommand(Original, Transfer);
	if (!TestTrue(TEXT("A real transfer commits both participant identities before control changes"), CommittedTransfer.Result.IsSuccess()))
	{
		return false;
	}
	FKnightRPGCallerContext Switched = Caller(Domains, Original.HumanId, 2);
	Switched.bInSafeZone = false;
	Switched.bInCombat = true;
	Switched.EditableKnights.Reset();
	const FKnightInventoryView Before = Domains.Inventory->GetView();
	const FKnightRPGView BeforeRPG = Domains.RPG->GetView();
	UKnightSkillRuntimeComponent* Runtime = Original.Pawn->FindComponentByClass<UKnightSkillRuntimeComponent>();
	const float BeforeHealth = Original.AbilitySystem->GetKnightAttributes()->GetHealth();
	const float BeforeCooldown = Runtime->CaptureSnapshot().ConsumableRemaining;
	int32 Publications = 0;
	const FDelegateHandle Observer = Domains.RPG->OnSnapshotChanged.AddLambda([&Publications]()
	{
		++Publications;
	});
	const FKnightRPGCommand Commands[] { Use, Transfer };
	const FKnightRPGCommandResult Committed[] { CommittedUse, CommittedTransfer };
	for (int32 Index = 0; Index < 2; ++Index)
	{
		const FKnightRPGCommandResult Query = Domains.RPG->QueryCommittedCommand(Switched, Commands[Index]);
		const FKnightRPGCommandResult Execute = Domains.RPG->ExecuteCommand(Switched, Commands[Index]);
		TestTrue(TEXT("Exact same-human history survives current Knight, edit-permission and sanctuary changes"),
			Query.Result.IsSuccess() && Query.bWasReplay && Execute.Result.IsSuccess() && Execute.bWasReplay
			&& Query.RequestId == Committed[Index].RequestId && Query.KnightId == Committed[Index].KnightId
			&& Query.CommittedInventoryRevision == Committed[Index].CommittedInventoryRevision
			&& Query.CommittedKnightRevision == Committed[Index].CommittedKnightRevision
			&& Query.AffectedInstanceId == Committed[Index].AffectedInstanceId
			&& Execute.CommittedInventoryRevision == Query.CommittedInventoryRevision
			&& Execute.CommittedKnightRevision == Query.CommittedKnightRevision);
	}
	FKnightRPGCommand Altered = Use;
	Altered.QuickSlot = 1;
	TestEqual(TEXT("Read-only receipt query rejects an altered exact payload"),
		Domains.RPG->QueryCommittedCommand(Switched, Altered).Result.Error, EKnightError::Conflict);
	TestEqual(TEXT("Execute shares exactly the same altered-payload receipt policy"),
		Domains.RPG->ExecuteCommand(Switched, Altered).Result.Error, EKnightError::Conflict);
	FKnightRPGCallerContext DifferentHuman = Switched;
	DifferentHuman.HumanId = FGuid::NewGuid();
	TestEqual(TEXT("Another authenticated human cannot read a successful historical request"),
		Domains.RPG->QueryCommittedCommand(DifferentHuman, Use).Result.Error, EKnightError::Conflict);
	TestEqual(TEXT("Execute shares exactly the same historical HumanId restriction"),
		Domains.RPG->ExecuteCommand(DifferentHuman, Use).Result.Error, EKnightError::Conflict);
	FKnightRPGCallerContext InvalidCaller = Switched;
	InvalidCaller.AbilitySystem = Original.AbilitySystem;
	TestEqual(TEXT("Historical query still requires the current canonical caller's matching ASC"),
		Domains.RPG->QueryCommittedCommand(InvalidCaller, Use).Result.Error, EKnightError::NotOwned);
	FKnightRPGCommand NewRequest = Transfer;
	NewRequest.RequestId = FGuid::NewGuid();
	const FKnightRPGCommandResult Absent = Domains.RPG->QueryCommittedCommand(Switched, NewRequest);
	TestTrue(TEXT("Absent receipt returns typed NotFound without claiming success or replay"),
		Absent.Result.Error == EKnightError::NotFound && !Absent.bWasReplay
		&& Absent.CommittedInventoryRevision == 0 && Absent.CommittedKnightRevision == 0);
	TestEqual(TEXT("An absent historical request cannot bypass fresh current-Knight ownership"),
		Domains.RPG->ExecuteCommand(Switched, NewRequest).Result.Error, EKnightError::NotOwned);
	Domains.RPG->OnSnapshotChanged.Remove(Observer);
	TestTrue(TEXT("Every history query, exact replay and rejection preserves current counts, locations, revisions, HP and CD"),
		Domains.Inventory->GetView() == Before && Domains.RPG->GetView() == BeforeRPG && Publications == 0
		&& Original.AbilitySystem->GetKnightAttributes()->GetHealth() == BeforeHealth
		&& Runtime->CaptureSnapshot().ConsumableRemaining == BeforeCooldown);
	int32 ReentryChecks = 0;
	const FDelegateHandle ReentryObserver = Domains.RPG->OnSnapshotChanged.AddLambda(
		[this, &Domains, &Switched, &Use, &ReentryChecks]()
		{
			++ReentryChecks;
			TestEqual(TEXT("Read-only history query retains transaction publication reentry rejection"),
				Domains.RPG->QueryCommittedCommand(Switched, Use).Result.Error, EKnightError::Conflict);
		});
	FKnightRPGCommand Purchase = Command(Domains, 0, EKnightRPGOperation::BuyItem);
	Purchase.ItemDefinitionId = FKnightGearCatalog::DefinitionId(TEXT("W01"));
	const FKnightResult PurchaseResult = Domains.RPG->ExecuteCommand(Original, Purchase).Result;
	Domains.RPG->OnSnapshotChanged.Remove(ReentryObserver);
	TestTrue(TEXT("One genuine new purchase publishes once while query remains callback-free"),
		PurchaseResult.IsSuccess() && ReentryChecks == 1);
	return true;
}

#endif
