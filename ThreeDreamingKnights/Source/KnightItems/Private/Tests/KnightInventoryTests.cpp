#include "Inventory/KnightInventoryComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Definitions/KnightItemDefinition.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "UObject/Class.h"
#include "UObject/StrongObjectPtr.h"
#include <limits>

namespace KnightInventoryTests
{
	class FKnightTestWorld final : public FNoncopyable
	{
	public:
		FKnightTestWorld()
			: Material(NewObject<UKnightItemDefinition>()), OtherMaterial(NewObject<UKnightItemDefinition>())
		{
			const UWorld::InitializationValues Values = UWorld::InitializationValues()
				.AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
			World.Reset(UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, false,
				ERHIFeatureLevel::Num, &Values));
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World.Get());
			World->SetShouldTick(false);
			Material->TrySetIdentity(TEXT("TestOre"), TEXT("KnightItem"));
			Material->TrySetItemTuning(EKnightItemKind::Material, KnightItems::MaximumStackCount);
			OtherMaterial->TrySetIdentity(TEXT("TestWood"), TEXT("KnightItem"));
			OtherMaterial->TrySetItemTuning(EKnightItemKind::Material, 10);
		}

		~FKnightTestWorld()
		{
			if (World.IsValid())
			{
				World->DestroyWorld(false);
				GEngine->DestroyWorldContext(World.Get());
			}
		}

		UKnightInventoryComponent* MakeInventory() const
		{
			AActor* Owner = World->SpawnActor<AActor>();
			Owner->SetReplicates(true);
			UKnightInventoryComponent* Inventory = NewObject<UKnightInventoryComponent>(Owner);
			Owner->AddInstanceComponent(Inventory);
			Inventory->RegisterComponent();
			return Inventory;
		}

		FKnightResult ConfigureFixtureCatalog(UKnightInventoryComponent* Inventory) const
		{
			TArray<UKnightItemDefinition*> Definitions { Material.Get(), OtherMaterial.Get() };
			return Inventory->InitializeFixtureCatalog(Definitions);
		}

		TStrongObjectPtr<UWorld> World;
		TStrongObjectPtr<UKnightItemDefinition> Material;
		TStrongObjectPtr<UKnightItemDefinition> OtherMaterial;
	};

	FKnightInventoryMutation MakeMutation(EKnightInventoryOperation Operation, const FPrimaryAssetId& DefinitionId,
		const FGuid& InstanceId, int32 Count)
	{
		FKnightInventoryMutation Mutation;
		Mutation.Operation = Operation;
		Mutation.Item.DefinitionId = DefinitionId;
		Mutation.Item.InstanceId = InstanceId;
		Mutation.Item.Count = Count;
		return Mutation;
	}

	FKnightInventoryRequest MakeRequest(const UKnightInventoryComponent* Inventory,
		TArray<FKnightInventoryMutation> Mutations)
	{
		FKnightInventoryRequest Request;
		Request.RequestId = FGuid::NewGuid();
		Request.ExpectedRevision = Inventory->GetView().Revision;
		Request.Mutations = MoveTemp(Mutations);
		return Request;
	}

	int32 ReadCount(const UKnightInventoryComponent* Inventory, const FGuid& InstanceId)
	{
		const FKnightInventoryView View = Inventory->GetView();
		const FKnightItemInstance* Item = View.Items.FindByPredicate([&InstanceId](const FKnightItemInstance& Candidate)
			{
				return Candidate.InstanceId == InstanceId;
			});
		return Item != nullptr ? Item->Count : 0;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKnightItemDefinitionTest, "Knight.Items.DefinitionContracts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FKnightItemDefinitionTest::RunTest(const FString& Parameters)
{
	const TStrongObjectPtr<UKnightItemDefinition> Item(NewObject<UKnightItemDefinition>());
	TestEqual(TEXT("Unassigned item identity is invalid"), Item->ValidateItem().Error, EKnightError::InvalidId);
	TestTrue(TEXT("Native item has an explicit stable identity"),
		Item->TrySetIdentity(TEXT("ContractsOre"), TEXT("KnightItem")));
	TestTrue(TEXT("Default bounded material data validates"), Item->ValidateItem().IsSuccess());
	TestEqual(TEXT("Correct primary type"), Item->GetPrimaryAssetId(),
		FPrimaryAssetId(TEXT("KnightItem"), TEXT("ContractsOre")));
	const int32 OriginalStack = Item->GetMaximumStack();
	TestEqual(TEXT("Zero stack cannot be authored"),
		Item->TrySetItemTuning(EKnightItemKind::Material, 0).Error, EKnightError::InvalidData);
	TestEqual(TEXT("Rejected stack preserves data"), Item->GetMaximumStack(), OriginalStack);
	TestEqual(TEXT("Finite stack bound"),
		Item->TrySetItemTuning(EKnightItemKind::Material, KnightItems::MaximumStackCount + 1).Error,
		EKnightError::InvalidData);
	TestEqual(TEXT("Unknown persisted kind cannot enter data"),
		Item->TrySetItemTuning(static_cast<EKnightItemKind>(255), 1).Error, EKnightError::InvalidData);
	TestEqual(TEXT("Equipment is an individual instance"),
		Item->TrySetItemTuning(EKnightItemKind::Equipment, 2).Error, EKnightError::InvalidData);
	FKnightItemStatBonuses Bonuses;
	Bonuses.AttackPower = 5.0f;
	const FPrimaryAssetId AbilitySet(TEXT("KnightAbilitySet"), TEXT("FoundationCombat"));
	TestTrue(TEXT("Equipment stores stable grant identity and positive bonuses"),
		Item->TrySetItemTuning(EKnightItemKind::Equipment, 1, AbilitySet, Bonuses).IsSuccess());
	TestEqual(TEXT("Stored grant identity"), Item->GetAbilitySetId(), AbilitySet);
	TestEqual(TEXT("Stored bonus"), Item->GetStatBonuses().AttackPower, 5.0f);
	Bonuses.AttackPower = std::numeric_limits<float>::quiet_NaN();
	TestEqual(TEXT("NaN stat data is rejected"),
		Item->TrySetItemTuning(EKnightItemKind::Equipment, 1, AbilitySet, Bonuses).Error, EKnightError::InvalidData);
	TestEqual(TEXT("NaN rejection preserves all previous tuning"), Item->GetStatBonuses().AttackPower, 5.0f);
	Bonuses.AttackPower = -1.0f;
	TestEqual(TEXT("Negative bonus is rejected"), Bonuses.Validate().Error, EKnightError::InvalidData);
	Bonuses.AttackPower = KnightItems::MaximumStatBonus + 1.0f;
	TestEqual(TEXT("Oversized bonus is rejected"), Bonuses.Validate().Error, EKnightError::InvalidData);
	TestEqual(TEXT("Wrong grant definition type is rejected"),
		Item->TrySetItemTuning(EKnightItemKind::Rune, 1,
			FPrimaryAssetId(TEXT("KnightHero"), TEXT("Training_Red"))).Error, EKnightError::InvalidId);
	TestEqual(TEXT("Materials do not store loadout grants"),
		Item->TrySetItemTuning(EKnightItemKind::Material, 10, AbilitySet).Error, EKnightError::InvalidData);
	Item->Rename(TEXT("MovedContractsItem"));
	TestEqual(TEXT("Object rename preserves persisted item identity"), Item->GetPrimaryAssetId(),
		FPrimaryAssetId(TEXT("KnightItem"), TEXT("ContractsOre")));
	Item->TrySetIdentity(TEXT("ContractsOre"), TEXT("KnightHero"));
	TestEqual(TEXT("Definition validator rejects wrong primary type"), Item->ValidateItem().Error,
		EKnightError::TypeMismatch);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKnightInventoryAtomicTest, "Knight.Items.AtomicBatchesAndCatalog",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FKnightInventoryAtomicTest::RunTest(const FString& Parameters)
{
	using namespace KnightInventoryTests;
	FKnightTestWorld Fixture;
	UKnightInventoryComponent* Inventory = Fixture.MakeInventory();
	TestTrue(TEXT("Actual fixture definitions pass typed validation"), Fixture.Material->ValidateItem().IsSuccess());
	TestTrue(TEXT("Explicit immutable typed fixture catalog initializes"),
		Fixture.ConfigureFixtureCatalog(Inventory).IsSuccess());
	TestTrue(TEXT("Finite shared inventory initializes"), Inventory->InitializeInventory(4).IsSuccess());
	const FPrimaryAssetId Ore = Fixture.Material->GetPrimaryAssetId();
	const FGuid OreInstance = FGuid::NewGuid();
	const FKnightInventoryRequest Seed = MakeRequest(Inventory,
		{ MakeMutation(EKnightInventoryOperation::Add, Ore, OreInstance, 7) });
	TestTrue(TEXT("Authoritative seed transaction commits"), Inventory->TryApplyTransaction(Seed).Result.IsSuccess());
	FKnightInventorySnapshot Before;
	TestTrue(TEXT("Authority snapshot captures committed state"), Inventory->CaptureSnapshot(Before).IsSuccess());
	int32 Notifications = 0;
	const FDelegateHandle Handle = Inventory->OnSnapshotChanged.AddLambda([&Notifications]()
		{
			++Notifications;
		});
	const FKnightInventoryRequest InvalidBatch = MakeRequest(Inventory,
		{
			MakeMutation(EKnightInventoryOperation::Remove, Ore, OreInstance, 3),
			MakeMutation(EKnightInventoryOperation::Add,
				FPrimaryAssetId(TEXT("KnightItem"), TEXT("MissingDefinition")), FGuid::NewGuid(), 1)
		});
	TestEqual(TEXT("Missing real typed definition rejects entire batch"),
		Inventory->TryApplyTransaction(InvalidBatch).Result.Error, EKnightError::NotFound);
	FKnightInventorySnapshot After;
	Inventory->CaptureSnapshot(After);
	TestTrue(TEXT("Invalid batch preserves items, revision, identity and receipt history"), Before == After);
	TestEqual(TEXT("Rejected batch sends no snapshot notification"), Notifications, 0);
	FKnightInventoryRequest InvalidCount = MakeRequest(Inventory,
		{ MakeMutation(EKnightInventoryOperation::Remove, Ore, OreInstance, 8) });
	TestEqual(TEXT("Insufficient count never makes a negative balance"),
		Inventory->TryApplyTransaction(InvalidCount).Result.Error, EKnightError::InsufficientResources);
	InvalidCount.Mutations[0].Item.Count = 0;
	TestEqual(TEXT("Zero mutation is rejected"), Inventory->TryApplyTransaction(InvalidCount).Result.Error,
		EKnightError::InvalidData);
	Inventory->CaptureSnapshot(After);
	TestTrue(TEXT("Count failures preserve all authoritative state"), Before == After);
	Fixture.Material->TrySetItemTuning(EKnightItemKind::Material, 1);
	Fixture.Material->TrySetIdentity(TEXT("CallerChangedId"), TEXT("KnightItem"));
	const FKnightInventoryRequest Increment = MakeRequest(Inventory,
		{ MakeMutation(EKnightInventoryOperation::Add, Ore, OreInstance, 1) });
	TestTrue(TEXT("Caller changes cannot mutate the privately duplicated catalog"),
		Inventory->TryApplyTransaction(Increment).Result.IsSuccess());
	TestEqual(TEXT("Actual copied stack definition is used"), ReadCount(Inventory, OreInstance), 8);
	TestEqual(TEXT("Exactly one valid transaction notifies once"), Notifications, 1);
	TestEqual(TEXT("Catalog cannot be replaced after initialization"),
		Fixture.ConfigureFixtureCatalog(Inventory).Error, EKnightError::InvalidState);
	Inventory->OnSnapshotChanged.Remove(Handle);
	UKnightInventoryComponent* ProductionInventory = Fixture.MakeInventory();
	TestTrue(TEXT("Production inventory initializes independently of a fixture catalog"),
		ProductionInventory->InitializeInventory(2).IsSuccess());
	const FKnightInventoryRequest Unregistered = MakeRequest(ProductionInventory,
		{ MakeMutation(EKnightInventoryOperation::Add, Ore, FGuid::NewGuid(), 1) });
	TestEqual(TEXT("Transient fixture definitions do not replace production AssetManager resolution"),
		ProductionInventory->TryApplyTransaction(Unregistered).Result.Error, EKnightError::NotFound);
	UKnightInventoryComponent* InvalidCatalog = Fixture.MakeInventory();
	TArray<UKnightItemDefinition*> Duplicates { Fixture.OtherMaterial.Get(), Fixture.OtherMaterial.Get() };
	TestEqual(TEXT("Duplicate definition IDs quarantine fixture initialization"),
		InvalidCatalog->InitializeFixtureCatalog(Duplicates).Error, EKnightError::DuplicateId);
	TArray<UKnightItemDefinition*> Nulls { nullptr };
	TestEqual(TEXT("Null definition cannot be treated as a successful catalog"),
		InvalidCatalog->InitializeFixtureCatalog(Nulls).Error, EKnightError::InvalidData);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKnightInventoryReplayTest, "Knight.Items.ExactReplayAndRevision",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FKnightInventoryReplayTest::RunTest(const FString& Parameters)
{
	using namespace KnightInventoryTests;
	FKnightTestWorld Fixture;
	UKnightInventoryComponent* Inventory = Fixture.MakeInventory();
	TestTrue(TEXT("Fixture catalog ready"), Fixture.ConfigureFixtureCatalog(Inventory).IsSuccess());
	TestTrue(TEXT("Inventory ready"), Inventory->InitializeInventory(3).IsSuccess());
	const FPrimaryAssetId Ore = Fixture.Material->GetPrimaryAssetId();
	const FGuid OreInstance = FGuid::NewGuid();
	const FKnightInventoryRequest First = MakeRequest(Inventory,
		{ MakeMutation(EKnightInventoryOperation::Add, Ore, OreInstance, 2) });
	const FKnightInventoryTransactionResult FirstCommit = Inventory->TryApplyTransaction(First);
	TestTrue(TEXT("First request commits"), FirstCommit.Result.IsSuccess());
	TestEqual(TEXT("First commit is revision2"), FirstCommit.CommittedRevision, static_cast<int64>(2));
	TestFalse(TEXT("Initial commit is not a replay"), FirstCommit.bWasReplay);
	const FKnightInventoryRequest Second = MakeRequest(Inventory,
		{ MakeMutation(EKnightInventoryOperation::Add, Ore, OreInstance, 3) });
	TestTrue(TEXT("Later request commits"), Inventory->TryApplyTransaction(Second).Result.IsSuccess());
	FKnightInventorySnapshot Before;
	Inventory->CaptureSnapshot(Before);
	int32 Notifications = 0;
	const FDelegateHandle Handle = Inventory->OnSnapshotChanged.AddLambda([&Notifications]()
		{
			++Notifications;
		});
	const FKnightInventoryTransactionResult Replayed = Inventory->TryApplyTransaction(First);
	TestTrue(TEXT("Exact historical payload has idempotent result"), Replayed.Result.IsSuccess() && Replayed.bWasReplay);
	TestEqual(TEXT("Replay reports its original commit revision"), Replayed.CommittedRevision, FirstCommit.CommittedRevision);
	TestEqual(TEXT("Replay does not duplicate items"), ReadCount(Inventory, OreInstance), 5);
	FKnightInventoryRequest Changed = First;
	Changed.Mutations[0].Item.Count = 4;
	TestEqual(TEXT("Same GUID with changed count rejects"),
		Inventory->TryApplyTransaction(Changed).Result.Error, EKnightError::Conflict);
	Changed = First;
	Changed.ExpectedRevision = Inventory->GetView().Revision;
	TestEqual(TEXT("Expected revision is part of exact replay payload"),
		Inventory->TryApplyTransaction(Changed).Result.Error, EKnightError::Conflict);
	Changed = First;
	Changed.Mutations[0].Operation = EKnightInventoryOperation::Remove;
	TestEqual(TEXT("Changed operation cannot masquerade as an own repeat"),
		Inventory->TryApplyTransaction(Changed).Result.Error, EKnightError::Conflict);
	FKnightInventoryRequest Stale = First;
	Stale.RequestId = FGuid::NewGuid();
	TestEqual(TEXT("A new request cannot use a stale revision"),
		Inventory->TryApplyTransaction(Stale).Result.Error, EKnightError::InvalidRevision);
	FKnightInventoryRequest Invalid = MakeRequest(Inventory,
		{ MakeMutation(EKnightInventoryOperation::Add, Ore, FGuid::NewGuid(), 1) });
	Invalid.RequestId.Invalidate();
	TestEqual(TEXT("Invalid request GUID fails"), Inventory->TryApplyTransaction(Invalid).Result.Error,
		EKnightError::InvalidId);
	Invalid.RequestId = FGuid::NewGuid();
	Invalid.Mutations[0].Item.InstanceId.Invalidate();
	TestEqual(TEXT("Invalid persistent instance GUID fails"), Inventory->TryApplyTransaction(Invalid).Result.Error,
		EKnightError::InvalidId);
	FKnightInventorySnapshot After;
	Inventory->CaptureSnapshot(After);
	TestTrue(TEXT("Replay and every rejection preserve the complete state"), Before == After);
	TestEqual(TEXT("No replay/rejection emits another snapshot"), Notifications, 0);
	Inventory->OnSnapshotChanged.Remove(Handle);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKnightInventoryCapacityTest, "Knight.Items.CapacityAndInstanceIdentity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FKnightInventoryCapacityTest::RunTest(const FString& Parameters)
{
	using namespace KnightInventoryTests;
	FKnightTestWorld Fixture;
	UKnightInventoryComponent* Inventory = Fixture.MakeInventory();
	TestTrue(TEXT("Fixture catalog ready"), Fixture.ConfigureFixtureCatalog(Inventory).IsSuccess());
	TestEqual(TEXT("Unbounded capacity rejects"),
		Inventory->InitializeInventory(KnightItems::MaximumInventoryCapacity + 1).Error, EKnightError::CapacityExceeded);
	TestFalse(TEXT("Capacity rejection leaves inventory fresh"), Inventory->IsInitialized());
	TestTrue(TEXT("One-slot capacity initializes"), Inventory->InitializeInventory(1).IsSuccess());
	const FPrimaryAssetId Ore = Fixture.Material->GetPrimaryAssetId();
	const FPrimaryAssetId Wood = Fixture.OtherMaterial->GetPrimaryAssetId();
	const FGuid OldInstance = FGuid::NewGuid();
	TestTrue(TEXT("First instance fills inventory"), Inventory->TryApplyTransaction(MakeRequest(Inventory,
		{ MakeMutation(EKnightInventoryOperation::Add, Ore, OldInstance, 4) })).Result.IsSuccess());
	const FKnightInventoryView Before = Inventory->GetView();
	const FGuid NewInstance = FGuid::NewGuid();
	TestEqual(TEXT("Another instance exceeds complete candidate capacity"), Inventory->TryApplyTransaction(MakeRequest(Inventory,
		{ MakeMutation(EKnightInventoryOperation::Add, Wood, NewInstance, 1) })).Result.Error,
		EKnightError::CapacityExceeded);
	TestTrue(TEXT("Capacity rejection preserves items and revision"), Before == Inventory->GetView());
	TestEqual(TEXT("Duplicate instance within batch rejects before mutation"), Inventory->TryApplyTransaction(MakeRequest(Inventory,
		{
			MakeMutation(EKnightInventoryOperation::Add, Ore, OldInstance, 1),
			MakeMutation(EKnightInventoryOperation::Remove, Ore, OldInstance, 1)
		})).Result.Error, EKnightError::DuplicateId);
	TestEqual(TEXT("Wrong definition for the exact GUID rejects"), Inventory->TryApplyTransaction(MakeRequest(Inventory,
		{ MakeMutation(EKnightInventoryOperation::Remove, Wood, OldInstance, 1) })).Result.Error, EKnightError::Conflict);
	TestEqual(TEXT("Wrong removal GUID rejects"), Inventory->TryApplyTransaction(MakeRequest(Inventory,
		{ MakeMutation(EKnightInventoryOperation::Remove, Ore, FGuid::NewGuid(), 1) })).Result.Error, EKnightError::NotFound);
	TestTrue(TEXT("Final candidate allows atomic replacement at full capacity, independent of operation order"),
		Inventory->TryApplyTransaction(MakeRequest(Inventory,
			{
				MakeMutation(EKnightInventoryOperation::Add, Wood, NewInstance, 9),
				MakeMutation(EKnightInventoryOperation::Remove, Ore, OldInstance, 4)
			})).Result.IsSuccess());
	TestEqual(TEXT("Removed instance no longer exists"), ReadCount(Inventory, OldInstance), 0);
	TestEqual(TEXT("Replacement count is exact"), ReadCount(Inventory, NewInstance), 9);
	TestEqual(TEXT("Authored stack bound rejects additional count"), Inventory->TryApplyTransaction(MakeRequest(Inventory,
		{ MakeMutation(EKnightInventoryOperation::Add, Wood, NewInstance, 2) })).Result.Error,
		EKnightError::CapacityExceeded);
	TestEqual(TEXT("Removed GUID cannot be reused as a new instance"), Inventory->TryApplyTransaction(MakeRequest(Inventory,
		{ MakeMutation(EKnightInventoryOperation::Add, Ore, OldInstance, 1) })).Result.Error, EKnightError::DuplicateId);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKnightInventoryRestoreTest, "Knight.Items.SnapshotReconstruction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FKnightInventoryRestoreTest::RunTest(const FString& Parameters)
{
	using namespace KnightInventoryTests;
	FKnightTestWorld Fixture;
	UKnightInventoryComponent* Inventory = Fixture.MakeInventory();
	TestTrue(TEXT("Fixture catalog ready"), Fixture.ConfigureFixtureCatalog(Inventory).IsSuccess());
	TestTrue(TEXT("Inventory ready"), Inventory->InitializeInventory(3).IsSuccess());
	const FPrimaryAssetId Ore = Fixture.Material->GetPrimaryAssetId();
	const FPrimaryAssetId Wood = Fixture.OtherMaterial->GetPrimaryAssetId();
	const FGuid OreInstance = FGuid::NewGuid();
	const FGuid RetiredInstance = FGuid::NewGuid();
	const FKnightInventoryRequest First = MakeRequest(Inventory,
		{
			MakeMutation(EKnightInventoryOperation::Add, Ore, OreInstance, 6),
			MakeMutation(EKnightInventoryOperation::Add, Wood, RetiredInstance, 2)
		});
	TestTrue(TEXT("Seed batch commits"), Inventory->TryApplyTransaction(First).Result.IsSuccess());
	TestTrue(TEXT("Removal retires identity"), Inventory->TryApplyTransaction(MakeRequest(Inventory,
		{ MakeMutation(EKnightInventoryOperation::Remove, Wood, RetiredInstance, 2) })).Result.IsSuccess());
	TestTrue(TEXT("Partial removal commits"), Inventory->TryApplyTransaction(MakeRequest(Inventory,
		{ MakeMutation(EKnightInventoryOperation::Remove, Ore, OreInstance, 1) })).Result.IsSuccess());
	FKnightInventorySnapshot Snapshot;
	if (!TestTrue(TEXT("Actual authoritative snapshot is available"), Inventory->CaptureSnapshot(Snapshot).IsSuccess())
		|| !TestEqual(TEXT("Reconstruction fixture contains one live instance"), Snapshot.Items.Num(), 1)
		|| !TestEqual(TEXT("Reconstruction fixture contains three real receipts"), Snapshot.Receipts.Num(), 3))
	{
		return false;
	}
	TArray<uint8> Bytes;
	{
		FMemoryWriter Writer(Bytes);
		FObjectAndNameAsStringProxyArchive Archive(Writer, false);
		Archive.ArIsSaveGame = true;
		FKnightInventorySnapshot::StaticStruct()->SerializeItem(Archive, &Snapshot, nullptr);
		TestFalse(TEXT("Native snapshot serialization succeeds"), Archive.IsError());
	}
	FKnightInventorySnapshot Decoded;
	{
		FMemoryReader Reader(Bytes);
		FObjectAndNameAsStringProxyArchive Archive(Reader, false);
		Archive.ArIsSaveGame = true;
		FKnightInventorySnapshot::StaticStruct()->SerializeItem(Archive, &Decoded, nullptr);
		TestFalse(TEXT("Native snapshot deserialization succeeds"), Archive.IsError());
	}
	if (!TestTrue(TEXT("SaveGame reflection roundtrip preserves exact IDs, revision and successful payloads"), Snapshot == Decoded))
	{
		return false;
	}
	UKnightInventoryComponent* Reconstructed = Fixture.MakeInventory();
	TestTrue(TEXT("Fresh reconstruction imports actual typed definitions"),
		Fixture.ConfigureFixtureCatalog(Reconstructed).IsSuccess());
	TestTrue(TEXT("Complete real journal reconstructs inventory"), Reconstructed->TryRestoreSnapshot(Decoded).IsSuccess());
	TestTrue(TEXT("Reconstruction preserves inventory GUID and all item identities/counts"),
		Inventory->GetView() == Reconstructed->GetView());
	FKnightInventorySnapshot ReconstructedSnapshot;
	Reconstructed->CaptureSnapshot(ReconstructedSnapshot);
	TestTrue(TEXT("Reconstruction preserves exact authoritative history"), Snapshot == ReconstructedSnapshot);
	const FKnightInventoryTransactionResult Replayed = Reconstructed->TryApplyTransaction(First);
	TestTrue(TEXT("Successful request replay survives reconstruction"), Replayed.Result.IsSuccess() && Replayed.bWasReplay);
	TestEqual(TEXT("Reconstructed replay preserves original committed revision"), Replayed.CommittedRevision,
		static_cast<int64>(2));
	TestEqual(TEXT("Retired identity cannot resurrect after reconstruction"),
		Reconstructed->TryApplyTransaction(MakeRequest(Reconstructed,
			{ MakeMutation(EKnightInventoryOperation::Add, Wood, RetiredInstance, 1) })).Result.Error,
		EKnightError::DuplicateId);
	TestEqual(TEXT("Snapshot cannot overwrite initialized state"),
		Reconstructed->TryRestoreSnapshot(Snapshot).Error, EKnightError::InvalidState);
	int32 InvalidNotifications = 0;
	UKnightInventoryComponent* InvalidTarget = Fixture.MakeInventory();
	TestTrue(TEXT("Invalid reconstruction target has a real fixture catalog"),
		Fixture.ConfigureFixtureCatalog(InvalidTarget).IsSuccess());
	const FDelegateHandle Handle = InvalidTarget->OnSnapshotChanged.AddLambda([&InvalidNotifications]()
		{
			++InvalidNotifications;
		});
	FKnightInventorySnapshot Invalid = Snapshot;
	Invalid.Items[0].InstanceId = FGuid::NewGuid();
	TestEqual(TEXT("Wrong valid final instance GUID disagrees with journal"),
		InvalidTarget->TryRestoreSnapshot(Invalid).Error, EKnightError::Conflict);
	Invalid = Snapshot;
	const FKnightItemInstance DuplicateInstance = Invalid.Items[0];
	Invalid.Items.Add(DuplicateInstance);
	TestEqual(TEXT("Duplicate final instance is rejected"),
		InvalidTarget->TryRestoreSnapshot(Invalid).Error, EKnightError::DuplicateId);
	Invalid = Snapshot;
	Invalid.Items[0].DefinitionId = FPrimaryAssetId(TEXT("KnightItem"), TEXT("UnregisteredItem"));
	TestEqual(TEXT("Invalid reconstruction definition is rejected"),
		InvalidTarget->TryRestoreSnapshot(Invalid).Error, EKnightError::NotFound);
	Invalid = Snapshot;
	Invalid.Receipts[0].Request.RequestId.Invalidate();
	TestEqual(TEXT("Malformed receipt request GUID is rejected"),
		InvalidTarget->TryRestoreSnapshot(Invalid).Error, EKnightError::InvalidId);
	Invalid = Snapshot;
	Invalid.Receipts[1].Request.RequestId = Invalid.Receipts[0].Request.RequestId;
	TestEqual(TEXT("Duplicate receipt GUID is rejected"),
		InvalidTarget->TryRestoreSnapshot(Invalid).Error, EKnightError::DuplicateId);
	Invalid = Snapshot;
	Invalid.Receipts[1].CommittedRevision += 1;
	TestEqual(TEXT("Noncontiguous receipt revision is rejected"),
		InvalidTarget->TryRestoreSnapshot(Invalid).Error, EKnightError::InvalidRevision);
	Invalid = Snapshot;
	Invalid.Receipts[0].Request.Mutations[0].Item.Count += 1;
	TestEqual(TEXT("Changed receipt payload cannot reconstruct the claimed final counts"),
		InvalidTarget->TryRestoreSnapshot(Invalid).Error, EKnightError::Conflict);
	Invalid = Snapshot;
	Invalid.Revision += 1;
	TestEqual(TEXT("Missing transaction history rejects claimed revision"),
		InvalidTarget->TryRestoreSnapshot(Invalid).Error, EKnightError::InvalidRevision);
	Invalid = Snapshot;
	Invalid.InventoryId.Invalidate();
	TestEqual(TEXT("Invalid inventory GUID is rejected"),
		InvalidTarget->TryRestoreSnapshot(Invalid).Error, EKnightError::InvalidData);
	Invalid = Snapshot;
	Invalid.SchemaVersion += 1;
	TestEqual(TEXT("Unknown snapshot version is rejected"),
		InvalidTarget->TryRestoreSnapshot(Invalid).Error, EKnightError::InvalidData);
	TestFalse(TEXT("Every malformed snapshot leaves target fresh"), InvalidTarget->IsInitialized());
	TestEqual(TEXT("Malformed reconstruction never publishes partial progress"), InvalidNotifications, 0);
	InvalidTarget->OnSnapshotChanged.Remove(Handle);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKnightInventoryReentrantTest, "Knight.Items.ReentrantCommitAndJournalBound",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FKnightInventoryReentrantTest::RunTest(const FString& Parameters)
{
	using namespace KnightInventoryTests;
	FKnightTestWorld Fixture;
	UKnightInventoryComponent* Inventory = Fixture.MakeInventory();
	TestTrue(TEXT("Fixture catalog ready"), Fixture.ConfigureFixtureCatalog(Inventory).IsSuccess());
	TestTrue(TEXT("Inventory ready"), Inventory->InitializeInventory(2).IsSuccess());
	const FPrimaryAssetId Ore = Fixture.Material->GetPrimaryAssetId();
	const FGuid Instance = FGuid::NewGuid();
	const FKnightInventoryRequest OuterRequest = MakeRequest(Inventory,
		{ MakeMutation(EKnightInventoryOperation::Add, Ore, Instance, 1) });
	int32 Notifications = 0;
	bool bNestedIssued = false;
	bool bCompleteReceiptObserved = false;
	FKnightInventoryTransactionResult CallbackReplay;
	FKnightInventoryTransactionResult NestedOutcome;
	const FDelegateHandle Handle = Inventory->OnSnapshotChanged.AddLambda([&]()
		{
			++Notifications;
			if (!bNestedIssued)
			{
				bNestedIssued = true;
				FKnightInventorySnapshot Current;
				const FKnightResult Captured = Inventory->CaptureSnapshot(Current);
				bCompleteReceiptObserved = Captured.IsSuccess() && Current.Revision == 2
					&& Current.Receipts.Num() == 1 && Current.Receipts[0].Request == OuterRequest;
				CallbackReplay = Inventory->TryApplyTransaction(OuterRequest);
				NestedOutcome = Inventory->TryApplyTransaction(MakeRequest(Inventory,
					{ MakeMutation(EKnightInventoryOperation::Add, Ore, Instance, 1) }));
			}
		});
	const FKnightInventoryTransactionResult OuterOutcome = Inventory->TryApplyTransaction(OuterRequest);
	Inventory->OnSnapshotChanged.Remove(Handle);
	TestTrue(TEXT("Subscriber observes finalized receipt and state"), bCompleteReceiptObserved);
	TestTrue(TEXT("Reentrant own replay is idempotent"), CallbackReplay.Result.IsSuccess() && CallbackReplay.bWasReplay);
	TestTrue(TEXT("Reentrant new transaction commits against current revision"), NestedOutcome.Result.IsSuccess());
	TestEqual(TEXT("Outer response still identifies its own commit"), OuterOutcome.CommittedRevision, static_cast<int64>(2));
	TestEqual(TEXT("Nested response identifies subsequent commit"), NestedOutcome.CommittedRevision, static_cast<int64>(3));
	TestEqual(TEXT("Exactly two committed snapshots were published"), Notifications, 2);
	TestEqual(TEXT("Both actual increments apply once"), ReadCount(Inventory, Instance), 2);
	for (int32 Index = 2; Index < KnightItems::MaximumTransactionReceipts; ++Index)
	{
		const FKnightInventoryTransactionResult Result = Inventory->TryApplyTransaction(MakeRequest(Inventory,
			{ MakeMutation(EKnightInventoryOperation::Add, Ore, Instance, 1) }));
		if (!TestTrue(TEXT("Each bounded journal transaction commits"), Result.Result.IsSuccess()))
		{
			return false;
		}
	}
	const FKnightInventoryView BeforeSaturation = Inventory->GetView();
	TestEqual(TEXT("Successful receipt ledger has a finite hard bound"),
		Inventory->TryApplyTransaction(MakeRequest(Inventory,
			{ MakeMutation(EKnightInventoryOperation::Add, Ore, Instance, 1) })).Result.Error,
		EKnightError::CapacityExceeded);
	TestTrue(TEXT("Saturation preserves items and revision"), BeforeSaturation == Inventory->GetView());
	const FKnightInventoryTransactionResult OldestReplay = Inventory->TryApplyTransaction(OuterRequest);
	TestTrue(TEXT("Saturation never evicts oldest replay protection"),
		OldestReplay.Result.IsSuccess() && OldestReplay.bWasReplay && OldestReplay.CommittedRevision == 2);
	Inventory->GetOwner()->Destroy();
	TestEqual(TEXT("Destroyed owner cannot receive later mutations"),
		Inventory->TryApplyTransaction(OuterRequest).Result.Error, EKnightError::ShuttingDown);
	return true;
}

#if WITH_EDITOR
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKnightInventoryClientAuthorityTest, "Knight.Items.ClientAuthorityBoundary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FKnightInventoryClientAuthorityTest::RunTest(const FString& Parameters)
{
	using namespace KnightInventoryTests;
	const UWorld::InitializationValues Values = UWorld::InitializationValues()
		.AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
	const TStrongObjectPtr<UWorld> World(UWorld::CreateWorld(EWorldType::PIE, false, NAME_None, nullptr, false,
		ERHIFeatureLevel::Num, &Values));
	World->SetPlayInEditorInitialNetMode(NM_Client);
	TestEqual(TEXT("Authority fixture is an actual engine client-mode world"), World->GetNetMode(), NM_Client);
	AActor* Owner = World->SpawnActor<AActor>();
	UKnightInventoryComponent* Inventory = NewObject<UKnightInventoryComponent>(Owner);
	Owner->AddInstanceComponent(Inventory);
	Inventory->RegisterComponent();
	TestEqual(TEXT("Local client actor cannot initialize authoritative shared inventory"),
		Inventory->InitializeInventory(2).Error, EKnightError::NotAuthority);
	FKnightInventoryRequest Request;
	TestEqual(TEXT("Client cannot invoke native mutation path"),
		Inventory->TryApplyTransaction(Request).Result.Error, EKnightError::NotAuthority);
	FKnightInventorySnapshot Snapshot;
	TestEqual(TEXT("Client cannot supply persistence fragment"),
		Inventory->CaptureSnapshot(Snapshot).Error, EKnightError::NotAuthority);
	TestEqual(TEXT("Client cannot reconstruct authority"),
		Inventory->TryRestoreSnapshot(Snapshot).Error, EKnightError::NotAuthority);
	World->DestroyWorld(false);
	return true;
}
#endif

#endif
