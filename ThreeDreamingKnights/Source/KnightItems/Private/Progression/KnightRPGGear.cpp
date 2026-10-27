#include "Progression/KnightRPGComponent.h"

#include "Catalog/KnightGearCatalog.h"
#include "Definitions/KnightItemDefinition.h"
#include "Definitions/KnightRuneDefinition.h"
#include "Inventory/KnightInventoryComponent.h"
#include "Tags/KnightGameplayTags.h"

#define LOCTEXT_NAMESPACE "KnightRPGGear"

namespace KnightRPGGear
{
	TArray<FKnightItemInstance> ApplyValues(const FKnightInventoryView& Inventory, const FKnightInventoryRequest& Request)
	{
		TArray<FKnightItemInstance> Result = Inventory.Items;
		for (const FKnightInventoryMutation& Mutation : Request.Mutations)
		{
			FKnightItemInstance* Item = Result.FindByPredicate([&Mutation](const FKnightItemInstance& Entry)
			{
				return Entry.InstanceId == Mutation.Item.InstanceId;
			});
			if (Mutation.Operation == EKnightInventoryOperation::Update)
			{
				if (Item != nullptr)
				{
					*Item = Mutation.Item;
				}
			}
			else if (Mutation.Operation == EKnightInventoryOperation::Add)
			{
				if (Item == nullptr)
				{
					Result.Add(Mutation.Item);
				}
				else
				{
					Item->Count += Mutation.Item.Count;
					Item->ItemRevision += Mutation.ExpectedItemRevision > 0 ? 1 : 0;
				}
			}
			else if (Item != nullptr)
			{
				Item->Count -= Mutation.Item.Count;
				Item->ItemRevision += Mutation.ExpectedItemRevision > 0 ? 1 : 0;
				if (Item->Count == 0)
				{
					Result.RemoveAll([&Mutation](const FKnightItemInstance& Entry)
					{
						return Entry.InstanceId == Mutation.Item.InstanceId;
					});
				}
			}
		}
		Result.Sort([](const FKnightItemInstance& Left, const FKnightItemInstance& Right)
		{
			return Left.InstanceId < Right.InstanceId;
		});
		return Result;
	}

	void SetValue(const FKnightInventoryView& Inventory, FKnightInventoryRequest& Request, FKnightItemInstance Value)
	{
		FKnightInventoryMutation* Pending = Request.Mutations.FindByPredicate([&Value](const FKnightInventoryMutation& Entry)
		{
			return Entry.Item.InstanceId == Value.InstanceId;
		});
		const FKnightItemInstance* Original = Inventory.Items.FindByPredicate([&Value](const FKnightItemInstance& Entry)
		{
			return Entry.InstanceId == Value.InstanceId;
		});
		if (Pending != nullptr && Pending->Operation == EKnightInventoryOperation::Add && Original == nullptr)
		{
			Value.ItemRevision = 1;
			Pending->Item = Value;
			return;
		}
		if (Original == nullptr)
		{
			return;
		}
		Value.ItemRevision = Original->ItemRevision;
		if (Value == *Original)
		{
			return;
		}
		Value.ItemRevision = Original->ItemRevision + 1;
		if (Pending == nullptr)
		{
			Pending = &Request.Mutations.AddDefaulted_GetRef();
		}
		Pending->Operation = EKnightInventoryOperation::Update;
		Pending->ExpectedItemRevision = Original->ItemRevision;
		Pending->Item = Value;
	}
}

FKnightResult UKnightRPGComponent::ConfigureGearDemo(const FGuid& DemoRunId, int64 InitialCurrency,
	int32 InitialLevel, bool bEquipStarter)
{
	const FKnightResult Authority = ValidateAuthority();
	if (!Authority.IsSuccess())
	{
		return Authority;
	}
	if (IsInitialized() || bTransactionInProgress || GearDemoRunId.IsValid() || !DemoRunId.IsValid()
		|| InitialCurrency < 0 || InitialCurrency > KnightGear::MaximumCurrency
		|| (InitialLevel != 1 && InitialLevel != 8))
	{
		return FKnightResult::Failure(EKnightError::InvalidData, TEXT("GearDemoConfiguration"),
			LOCTEXT("Configure", "Configure a fresh isolated progression or level-eight catalog demo once."));
	}
	GearDemoRunId = DemoRunId;
	GearInitialCurrency = InitialCurrency;
	GearInitialLevel = InitialLevel;
	bGearEquipStarter = bEquipStarter;
	return FKnightResult::Success();
}

FKnightResult UKnightRPGComponent::RegisterRewardBundle(const FKnightGearRewardBundle& Bundle)
{
	const FKnightResult Authority = ValidateAuthority();
	if (!Authority.IsSuccess())
	{
		return Authority;
	}
	if (!PublishedView.bGearDemo || bTransactionInProgress || !Bundle.RewardId.IsValid()
		|| Bundle.DemoRunId != PublishedView.DemoRunId || Bundle.Currency < 0
		|| Bundle.Currency > KnightGear::MaximumCurrency || (Bundle.Items.IsEmpty() && Bundle.BoundItems.IsEmpty())
		|| Bundle.Items.Num() > KnightItems::MaximumBatchOperations || Bundle.BoundItems.Num() > 3
		|| Bundle.Level < 1 || Bundle.Level > 8)
	{
		return FKnightResult::Failure(EKnightError::InvalidData, TEXT("GearReward"),
			LOCTEXT("Reward", "An authoritative reward belongs to this isolated demo generation and has a complete bounded bundle."));
	}
	if (const FKnightGearRewardBundle* Existing = RewardOffers.FindByPredicate([&Bundle](const FKnightGearRewardBundle& Entry)
	{
		return Entry.RewardId == Bundle.RewardId;
	}))
	{
		return *Existing == Bundle ? FKnightResult::Success()
			: FKnightResult::Failure(EKnightError::Conflict, TEXT("GearReward"), LOCTEXT("OfferReplay", "A reward identity cannot change its authoritative bundle."));
	}
	TSet<FPrimaryAssetId> Definitions;
	TSet<FGuid> Bound;
	const FKnightInventoryView BoundInventory = Inventory->GetView();
	for (const FGuid Id : Bundle.BoundItems)
	{
		const FKnightItemInstance* Item = BoundInventory.Items.FindByPredicate([Id](const FKnightItemInstance& Row)
		{
			return Row.InstanceId == Id;
		});
		const UKnightDefinition* Definition = nullptr;
		if (!Id.IsValid() || Bound.Contains(Id) || Item == nullptr
			|| !ResolveDefinition(Item->DefinitionId, UKnightItemDefinition::StaticClass(), Definition).IsSuccess()
			|| CastChecked<UKnightItemDefinition>(Definition)->GetMetadata().Subtype != EKnightEquipmentSubtype::Hammer)
		{
			return FKnightResult::Failure(EKnightError::InvalidData, TEXT("BoundRewardHammer"), FText::GetEmpty());
		}
		Bound.Add(Id);
	}
	for (const FKnightGearRewardItem& Item : Bundle.Items)
	{
		const UKnightDefinition* Definition = nullptr;
		const FKnightResult Resolution = ResolveDefinition(Item.DefinitionId, UKnightItemDefinition::StaticClass(), Definition);
		if (!Resolution.IsSuccess())
		{
			return Resolution;
		}
		if (Item.Count < 1 || Item.Count > KnightItems::MaximumStackCount || Definitions.Contains(Item.DefinitionId))
		{
			return FKnightResult::Failure(EKnightError::InvalidData, TEXT("GearReward"), LOCTEXT("OfferCount", "Reward quantities are positive and each definition occurs once."));
		}
		Definitions.Add(Item.DefinitionId);
	}
	RewardOffers.Add(Bundle);
	return FKnightResult::Success();
}

TArray<FKnightGearRewardBundle> UKnightRPGComponent::GetRewardOffers() const
{
	return RewardOffers;
}

FKnightResult UKnightRPGComponent::SeedGearAssignments(FKnightRPGView& Candidate,
	const FKnightInventoryView& InventoryView, FKnightInventoryRequest& Request) const
{
	Request.RequestId = FGuid::NewGuid();
	Request.ExpectedRevision = InventoryView.Revision;
	if (!bGearEquipStarter)
	{
		return FKnightResult::Success();
	}
	TSet<FGuid> Selected;
	TArray<FKnightItemInstance> SortedItems = InventoryView.Items;
	SortedItems.Sort([](const FKnightItemInstance& Left, const FKnightItemInstance& Right)
	{
		return Left.InstanceId < Right.InstanceId;
	});
	for (FKnightRPGKnightView& Knight : Candidate.Knights)
	{
		for (int32 Index = 0; Index < 8; ++Index)
		{
			const FPrimaryAssetId Id = FKnightGearCatalog::DefinitionId(Index == 0 ? FName(TEXT("W01"))
				: FName(*FString::Printf(TEXT("A%02d"), Index)));
			const FKnightItemInstance* Item = SortedItems.FindByPredicate([&Id, &Selected](const FKnightItemInstance& Entry)
			{
				return Entry.DefinitionId == Id && Entry.Count == 1 && !Selected.Contains(Entry.InstanceId)
					&& Entry.LocationId == KnightGear::InventoryLocation;
			});
			if (Item == nullptr)
			{
				return FKnightResult::Failure(EKnightError::NotFound, TEXT("GearStarter"), LOCTEXT("Starter", "Each canonical Knight needs eight distinct real starter item GUIDs."));
			}
			Selected.Add(Item->InstanceId);
			FKnightEquipmentAssignment Assignment;
			Assignment.ItemInstanceId = Item->InstanceId;
			Assignment.DefinitionId = Id;
			Assignment.SlotTag = KnightGear::EquipmentSlots()[Index];
			Knight.Assignments.Add(Assignment);
		}
	}
	return SynchronizeLocations(Candidate, InventoryView, Request);
}

FKnightResult UKnightRPGComponent::BuildGearCandidate(FKnightRPGView& Candidate,
	const FKnightInventoryView& InventoryView, const FKnightRPGCommand& Command,
	const TArray<FKnightInventoryMutation>* HistoricalMutations, FKnightInventoryRequest& Request, FGuid& OutAffected) const
{
	using namespace KnightRPGGear;
	FKnightRPGKnightView* Knight = Candidate.Knights.FindByPredicate([&Command](const FKnightRPGKnightView& Entry)
	{
		return Entry.KnightId == Command.ExpectedKnightId;
	});
	if (!Candidate.IsItemizationEnabled() || Knight == nullptr || !Knight->bGearEnabled)
	{
		return FKnightResult::Failure(EKnightError::InvalidState, TEXT("GearCatalogUnlock"),
			LOCTEXT("Enabled", "Gear commands require explicitly enabled twelve-slot itemization."));
	}
	if (!Candidate.bGearDemo && (Command.Operation == EKnightRPGOperation::ClaimReward
		|| Command.Operation == EKnightRPGOperation::ApplyGearPreset))
	{
		return FKnightResult::Failure(EKnightError::NotOwned, TEXT("GearFixtureAuthority"),
			LOCTEXT("FixtureOnly", "Demo rewards and fixture presets remain restricted to isolated GearLab generations."));
	}
	const FKnightItemInstance* Item = InventoryView.Items.FindByPredicate([&Command](const FKnightItemInstance& Entry)
	{
		return Entry.InstanceId == Command.ItemInstanceId;
	});
	const UKnightItemDefinition* Definition = nullptr;
	if (Command.ItemInstanceId.IsValid())
	{
		if (Command.ExpectedItemRevision < 1 || Item == nullptr || Item->ItemRevision != Command.ExpectedItemRevision)
		{
			return FKnightResult::Failure(EKnightError::InvalidRevision, TEXT("GearItemRevision"), LOCTEXT("Item", "The exact selected item GUID and revision must still exist."));
		}
		const UKnightDefinition* Resolved = nullptr;
		const FKnightResult Result = ResolveDefinition(Item->DefinitionId, UKnightItemDefinition::StaticClass(), Resolved);
		if (!Result.IsSuccess())
		{
			return Result;
		}
		Definition = CastChecked<UKnightItemDefinition>(Resolved);
	}
	auto AddItems = [this, &InventoryView, &Request, &Command, HistoricalMutations](const FPrimaryAssetId& Id,
		int32 Count, const FGuid& RewardReceipt) -> FKnightResult
	{
		const UKnightDefinition* Resolved = nullptr;
		const FKnightResult Resolution = ResolveDefinition(Id, UKnightItemDefinition::StaticClass(), Resolved);
		if (!Resolution.IsSuccess())
		{
			return Resolution;
		}
		const UKnightItemDefinition* ItemDefinition = CastChecked<UKnightItemDefinition>(Resolved);
		TArray<FKnightItemInstance> Values = ApplyValues(InventoryView, Request);
		if (ItemDefinition->GetMetadata().bUniqueOwned && Values.ContainsByPredicate([&Id](const FKnightItemInstance& Entry)
		{
			return Entry.DefinitionId == Id;
		}))
		{
			return FKnightResult::Failure(EKnightError::Conflict, TEXT("AlreadyOwned"), LOCTEXT("UniqueOwned", "The party already owns this unique quest token; no part of the reward was claimed."));
		}
		for (const FKnightItemInstance& Existing : Values)
		{
			if (Count <= 0)
			{
				break;
			}
			const int32 Room = ItemDefinition->GetMaximumStack() - Existing.Count;
			if (Existing.DefinitionId != Id || Existing.LocationId != KnightGear::InventoryLocation || Room <= 0)
			{
				continue;
			}
			FKnightInventoryMutation* Mutation = Request.Mutations.FindByPredicate([&Existing](const FKnightInventoryMutation& Entry)
			{
				return Entry.Item.InstanceId == Existing.InstanceId;
			});
			const int32 Added = FMath::Min(Room, Count);
			Count -= Added;
			if (Mutation != nullptr && Mutation->Operation == EKnightInventoryOperation::Add)
			{
				Mutation->Item.Count += Added;
			}
			else
			{
				FKnightInventoryMutation Addition;
				Addition.Item = Existing;
				Addition.Item.Count = Added;
				Addition.ExpectedItemRevision = Existing.ItemRevision;
				Request.Mutations.Add(Addition);
			}
		}
		while (Count > 0)
		{
			FKnightInventoryMutation Addition;
			Addition.Item.DefinitionId = Id;
			Addition.Item.Count = FMath::Min(Count, ItemDefinition->GetMaximumStack());
			Addition.Item.InstanceId = FGuid(Command.RequestId.A ^ 0x56000000u, Command.RequestId.B,
				Command.RequestId.C, Command.RequestId.D ^ (Request.Mutations.Num() + 1));
			Addition.Item.RewardReceiptId = RewardReceipt;
			if (HistoricalMutations != nullptr && HistoricalMutations->IsValidIndex(Request.Mutations.Num()))
			{
				Addition.Item.InstanceId = (*HistoricalMutations)[Request.Mutations.Num()].Item.InstanceId;
			}
			Count -= Addition.Item.Count;
			Request.Mutations.Add(Addition);
		}
		return FKnightResult::Success();
	};
	auto RemoveItems = [&InventoryView, &Request](const FPrimaryAssetId& Id, int32 Count) -> FKnightResult
	{
		for (const FKnightItemInstance& Value : ApplyValues(InventoryView, Request))
		{
			if (Count <= 0)
			{
				break;
			}
			if (Value.DefinitionId != Id || Value.LocationId != KnightGear::InventoryLocation)
			{
				continue;
			}
			FKnightInventoryMutation Mutation;
			Mutation.Operation = EKnightInventoryOperation::Remove;
			Mutation.Item = Value;
			Mutation.Item.Count = FMath::Min(Count, Value.Count);
			Mutation.ExpectedItemRevision = Value.ItemRevision;
			Count -= Mutation.Item.Count;
			Request.Mutations.Add(Mutation);
		}
		return Count == 0 ? FKnightResult::Success()
			: FKnightResult::Failure(EKnightError::InsufficientResources, TEXT("GearMaterials"), LOCTEXT("Materials", "The complete exact upgrade material cost must be available in the shared bag."));
	};
	auto Assign = [&Candidate, Knight, &InventoryView](FKnightRPGKnightView& Target,
		const FKnightItemInstance& Value, FGameplayTag Slot) -> FKnightResult
	{
		for (const FKnightRPGKnightView& Owner : Candidate.Knights)
		{
			if (Owner.Assignments.ContainsByPredicate([&Value](const FKnightEquipmentAssignment& Entry)
			{
				return Entry.ItemInstanceId == Value.InstanceId;
			}))
			{
				return FKnightResult::Failure(EKnightError::NotOwned, TEXT("EquipmentLocation"), LOCTEXT("Assigned", "An existing equipment assignment must move through an explicit transfer or ring swap."));
			}
		}
		Target.Assignments.RemoveAll([Slot](const FKnightEquipmentAssignment& Entry)
		{
			return Entry.SlotTag == Slot;
		});
		FKnightEquipmentAssignment Assignment;
		Assignment.ItemInstanceId = Value.InstanceId;
		Assignment.DefinitionId = Value.DefinitionId;
		Assignment.SlotTag = Slot;
		Target.Assignments.Add(Assignment);
		if (Slot == KnightGameplayTags::Slot_Equipment_Weapon)
		{
			Target.WeaponRuneInstanceId = Value.RuneAttachmentId;
			Target.Build.WeaponRune = Value.RuneAttachmentId.IsValid() ? FName(TEXT("Weapon.Aftershock")) : NAME_None;
		}
		return FKnightResult::Success();
	};
	FKnightResult Result = FKnightResult::Success();
	if (Command.Operation == EKnightRPGOperation::Equip)
	{
		if (Item->LocationId != KnightGear::InventoryLocation || Definition->GetMetadata().Category != EKnightItemCategory::Equipment
			|| !Definition->GetMetadata().AllowedSlots.Contains(Command.SlotTag) || Knight->Level < Definition->GetMetadata().RequiredLevel)
		{
			return FKnightResult::Failure(EKnightError::InvalidState, TEXT("EquipmentRequirement"), LOCTEXT("Equip", "Choose an available bag item, compatible explicit slot and sufficient character level."));
		}
		Result = Assign(*Knight, *Item, Command.SlotTag);
		OutAffected = Item->InstanceId;
	}
	else if (Command.Operation == EKnightRPGOperation::Unequip || Command.Operation == EKnightRPGOperation::TransferEquipment)
	{
		const FKnightEquipmentAssignment* Current = Knight->Assignments.FindByPredicate([Item](const FKnightEquipmentAssignment& Entry)
		{
			return Entry.ItemInstanceId == Item->InstanceId;
		});
		if (Current == nullptr || (Command.Operation == EKnightRPGOperation::Unequip && Current->SlotTag != Command.SlotTag))
		{
			return FKnightResult::Failure(EKnightError::NotOwned, TEXT("EquipmentOwner"), LOCTEXT("Owner", "Only the exact source Knight may unequip or transfer its currently assigned item."));
		}
		const FGameplayTag PreviousSlot = Current->SlotTag;
		Knight->Assignments.RemoveAll([Item](const FKnightEquipmentAssignment& Entry)
		{
			return Entry.ItemInstanceId == Item->InstanceId;
		});
		if (PreviousSlot == KnightGameplayTags::Slot_Equipment_Weapon)
		{
			Knight->WeaponRuneInstanceId.Invalidate();
			Knight->Build.WeaponRune = NAME_None;
		}
		if (Command.Operation == EKnightRPGOperation::TransferEquipment)
		{
			FKnightRPGKnightView* Target = Candidate.Knights.FindByPredicate([&Command](const FKnightRPGKnightView& Entry)
			{
				return Entry.KnightId == Command.TargetKnightId;
			});
			if (Target == nullptr || Target->Revision != Command.ExpectedTargetKnightRevision || Target->Revision >= MAX_int64
				|| Target->Level < Definition->GetMetadata().RequiredLevel || !Definition->GetMetadata().AllowedSlots.Contains(Command.SlotTag))
			{
				return FKnightResult::Failure(EKnightError::InvalidRevision, TEXT("EquipmentTarget"), LOCTEXT("Target", "The target Knight's exact revision, item level requirement and destination slot must be valid."));
			}
			Result = Assign(*Target, *Item, Command.SlotTag);
			++Target->Revision;
		}
		OutAffected = Item->InstanceId;
	}
	else if (Command.Operation == EKnightRPGOperation::SwapRings)
	{
		FKnightEquipmentAssignment* First = Knight->Assignments.FindByPredicate([](const FKnightEquipmentAssignment& Entry)
		{
			return Entry.SlotTag == KnightGameplayTags::Slot_Equipment_Ring1;
		});
		FKnightEquipmentAssignment* Second = Knight->Assignments.FindByPredicate([](const FKnightEquipmentAssignment& Entry)
		{
			return Entry.SlotTag == KnightGameplayTags::Slot_Equipment_Ring2;
		});
		if (First == nullptr || Second == nullptr)
		{
			return FKnightResult::Failure(EKnightError::InvalidState, TEXT("RingSwap"), LOCTEXT("Rings", "Both ring slots must be occupied for an atomic ring swap."));
		}
		Swap(First->SlotTag, Second->SlotTag);
		OutAffected = First->ItemInstanceId;
	}
	else if (Command.Operation == EKnightRPGOperation::AttachWeaponRune || Command.Operation == EKnightRPGOperation::DetachWeaponRune)
	{
		if (Definition->GetMetadata().Subtype != EKnightEquipmentSubtype::Hammer
			|| (Item->LocationId != KnightGear::InventoryLocation && Item->AssignedKnightId != Knight->KnightId))
		{
			return FKnightResult::Failure(EKnightError::NotOwned, TEXT("WeaponRuneParent"), LOCTEXT("Parent", "The exact hammer parent must be in the bag or equipped by the editable Knight."));
		}
		if (Command.Operation == EKnightRPGOperation::AttachWeaponRune)
		{
			const FKnightItemInstance* Rune = InventoryView.Items.FindByPredicate([&Command](const FKnightItemInstance& Entry)
			{
				return Entry.InstanceId == Command.WeaponRuneInstanceId && Entry.LocationId == KnightGear::InventoryLocation;
			});
			const UKnightDefinition* Resolved = nullptr;
			if (Rune == nullptr)
			{
				return FKnightResult::Failure(EKnightError::NotOwned, TEXT("WeaponRune"), LOCTEXT("RuneBag", "Attach an exact available rune instance from shared inventory."));
			}
			Result = ResolveDefinition(Rune->DefinitionId, UKnightRuneDefinition::StaticClass(), Resolved);
			if (!Result.IsSuccess())
			{
				return Result;
			}
			if (CastChecked<UKnightRuneDefinition>(Resolved)->GetAttachmentRuneId() != TEXT("Weapon.Aftershock"))
			{
				return FKnightResult::Failure(EKnightError::TypeMismatch, TEXT("WeaponRune"), LOCTEXT("RuneType", "Only Aftershock is compatible with the heavy-hammer weapon socket."));
			}
		}
		else if (!Item->RuneAttachmentId.IsValid())
		{
			return FKnightResult::Failure(EKnightError::NotFound, TEXT("WeaponRune"), LOCTEXT("NoRune", "This exact hammer has no attached rune to detach."));
		}
		FKnightItemInstance Changed = *Item;
		Changed.RuneAttachmentId = Command.WeaponRuneInstanceId;
		SetValue(InventoryView, Request, Changed);
		if (Item->LocationId == KnightGear::EquipmentLocation)
		{
			Knight->WeaponRuneInstanceId = Changed.RuneAttachmentId;
			Knight->Build.WeaponRune = Changed.RuneAttachmentId.IsValid() ? FName(TEXT("Weapon.Aftershock")) : NAME_None;
		}
		OutAffected = Item->InstanceId;
	}
	else if (Command.Operation == EKnightRPGOperation::UpgradeItem)
	{
		if (Definition->GetMetadata().UpgradeTrack != TEXT("RareOne") || Item->UpgradeRank != 0
			|| (Item->LocationId != KnightGear::InventoryLocation && Item->AssignedKnightId != Knight->KnightId)
			|| Candidate.Currency < 20)
		{
			return FKnightResult::Failure(EKnightError::InsufficientResources, TEXT("Upgrade"), LOCTEXT("Upgrade", "A real rank-zero Rare item and the complete fixed upgrade cost are required."));
		}
		const int32 Counts[] { 4, 2, 1 };
		for (int32 Index = 0; Index < 3; ++Index)
		{
			Result = RemoveItems(FKnightGearCatalog::DefinitionId(FName(*FString::Printf(TEXT("M%02d"), Index + 1))), Counts[Index]);
			if (!Result.IsSuccess())
			{
				return Result;
			}
		}
		Candidate.Currency -= 20;
		FKnightItemInstance Changed = *Item;
		Changed.UpgradeRank = 1;
		SetValue(InventoryView, Request, Changed);
		OutAffected = Item->InstanceId;
	}
	else if (Command.Operation == EKnightRPGOperation::BuyItem)
	{
		const UKnightDefinition* Resolved = nullptr;
		Result = ResolveDefinition(Command.ItemDefinitionId, UKnightItemDefinition::StaticClass(), Resolved);
		if (!Result.IsSuccess())
		{
			return Result;
		}
		const UKnightItemDefinition* Stock = CastChecked<UKnightItemDefinition>(Resolved);
		const int64 Cost = static_cast<int64>(Stock->GetMetadata().BuyPrice) * Command.Quantity;
		if (Stock->GetMetadata().BuyPrice <= 0 || Candidate.Currency < Cost)
		{
			return FKnightResult::Failure(EKnightError::InsufficientResources, TEXT("VendorBuy"), LOCTEXT("Buy", "The item must be actual vendor stock and its entire purchase price available."));
		}
		Result = AddItems(Command.ItemDefinitionId, Command.Quantity, FGuid());
		Candidate.Currency -= Cost;
		OutAffected = Request.Mutations.IsEmpty() ? FGuid() : Request.Mutations[0].Item.InstanceId;
	}
	else if (Command.Operation == EKnightRPGOperation::SellItem)
	{
		const FKnightItemMetadata& Metadata = Definition->GetMetadata();
		if (Item->LocationId != KnightGear::InventoryLocation || Item->RuneAttachmentId.IsValid()
			|| Metadata.SellPrice <= 0 || Command.Quantity > Item->Count
			|| Metadata.Rarity == EKnightItemRarity::Epic && !Command.bConfirmEpicSale)
		{
			return FKnightResult::Failure(EKnightError::InvalidState, TEXT("VendorSell"), LOCTEXT("Sell", "Sell only unassigned unsocketed sellable bag quantities, with explicit confirmation for Epic equipment."));
		}
		FKnightInventoryMutation Removal;
		Removal.Operation = EKnightInventoryOperation::Remove;
		Removal.Item = *Item;
		Removal.Item.Count = Command.Quantity;
		Removal.ExpectedItemRevision = Item->ItemRevision;
		Request.Mutations.Add(Removal);
		Candidate.Currency += static_cast<int64>(Metadata.SellPrice) * Command.Quantity;
		OutAffected = Item->InstanceId;
	}
	else if (Command.Operation == EKnightRPGOperation::ClaimReward)
	{
		const FKnightGearRewardBundle* Reward = RewardOffers.FindByPredicate([&Command](const FKnightGearRewardBundle& Entry)
		{
			return Entry.RewardId == Command.RewardId;
		});
		if (Reward == nullptr || Reward->DemoRunId != Candidate.DemoRunId || Candidate.ClaimedRewards.Contains(Command.RewardId))
		{
			return FKnightResult::Failure(EKnightError::Conflict, TEXT("RewardClaim"), LOCTEXT("Claim", "This authoritative reward must belong to the current run and remain unclaimed."));
		}
		for (const FKnightGearRewardItem& RewardItem : Reward->Items)
		{
			Result = AddItems(RewardItem.DefinitionId, RewardItem.Count, Reward->RewardId);
			if (!Result.IsSuccess())
			{
				return Result;
			}
		}
		for (const FGuid Id : Reward->BoundItems)
		{
			if (!InventoryView.Items.ContainsByPredicate([Id](const FKnightItemInstance& Item)
			{
				return Item.InstanceId == Id;
			}))
			{
				return FKnightResult::Failure(EKnightError::NotFound, TEXT("BoundRewardHammer"), FText::GetEmpty());
			}
		}
		Candidate.Currency += Reward->Currency;
		Candidate.ClaimedRewards.Add(Reward->RewardId);
		for (FKnightRPGKnightView& Entry : Candidate.Knights)
		{
			if (Entry.Level < Reward->Level)
			{
				Entry.Level = Reward->Level;
				if (Entry.KnightId != Knight->KnightId)
				{
					if (Entry.Revision >= MAX_int64)
					{
						return FKnightResult::Failure(EKnightError::CapacityExceeded, TEXT("RewardKnightRevision"),
							LOCTEXT("RewardKnightRevision", "A party reward cannot overflow a canonical Knight revision."));
					}
					++Entry.Revision;
				}
			}
		}
		OutAffected = Reward->RewardId;
	}
	else if (Command.Operation == EKnightRPGOperation::DeliverQuestItem)
	{
		const FKnightItemInstance* DeliveryItem = InventoryView.Items.FindByPredicate([&Command](const FKnightItemInstance& Row)
		{
			return Row.InstanceId == Command.ItemInstanceId;
		});
		const UKnightDefinition* DeliveryDefinition = nullptr;
		if (DeliveryItem == nullptr || DeliveryItem->DefinitionId != Command.ItemDefinitionId
			|| DeliveryItem->ItemRevision != Command.ExpectedItemRevision
			|| DeliveryItem->LocationId != KnightGear::InventoryLocation || DeliveryItem->Count < Command.Quantity
			|| !ResolveDefinition(DeliveryItem->DefinitionId,
				UKnightItemDefinition::StaticClass(), DeliveryDefinition).IsSuccess())
		{
			return FKnightResult::Failure(EKnightError::InvalidData, TEXT("QuestDeliveryItem"), FText::GetEmpty());
		}
		const EKnightItemCategory Category = CastChecked<UKnightItemDefinition>(DeliveryDefinition)->GetMetadata().Category;
		if (Category != EKnightItemCategory::Material && Category != EKnightItemCategory::QuestItem)
		{
			return FKnightResult::Failure(EKnightError::TypeMismatch, TEXT("QuestDeliveryCategory"), FText::GetEmpty());
		}
		FKnightInventoryMutation& Removal = Request.Mutations.AddDefaulted_GetRef();
		Removal.Operation = EKnightInventoryOperation::Remove;
		Removal.Item = *DeliveryItem;
		Removal.Item.Count = Command.Quantity;
		Removal.ExpectedItemRevision = DeliveryItem->ItemRevision;
		OutAffected = DeliveryItem->InstanceId;
	}
	else if (Command.Operation == EKnightRPGOperation::BindConsumable || Command.Operation == EKnightRPGOperation::UseConsumable)
	{
		const FPrimaryAssetId Id = Command.Operation == EKnightRPGOperation::BindConsumable
			? Command.ItemDefinitionId : Knight->ConsumableBindings[Command.QuickSlot];
		const UKnightDefinition* Resolved = nullptr;
		Result = ResolveDefinition(Id, UKnightItemDefinition::StaticClass(), Resolved);
		if (!Result.IsSuccess())
		{
			return Result;
		}
		if (CastChecked<UKnightItemDefinition>(Resolved)->GetMetadata().Category != EKnightItemCategory::Consumable
			|| Command.ItemDefinitionId.IsValid() && Command.ItemDefinitionId != Id)
		{
			return FKnightResult::Failure(EKnightError::TypeMismatch, TEXT("ConsumableBinding"), LOCTEXT("Binding", "Quick-use references a server-confirmed consumable definition, never a client count or arbitrary stack."));
		}
		if (Command.Operation == EKnightRPGOperation::BindConsumable)
		{
			Knight->ConsumableBindings[Command.QuickSlot] = Id;
		}
		else
		{
			TArray<FKnightItemInstance> Sorted = InventoryView.Items;
			Sorted.Sort([](const FKnightItemInstance& Left, const FKnightItemInstance& Right)
			{
				return Left.InstanceId < Right.InstanceId;
			});
			const FKnightItemInstance* Stack = Sorted.FindByPredicate([&Id](const FKnightItemInstance& Entry)
			{
				return Entry.DefinitionId == Id && Entry.LocationId == KnightGear::InventoryLocation;
			});
			if (Stack == nullptr)
			{
				return FKnightResult::Failure(EKnightError::InsufficientResources, TEXT("ConsumableStack"), LOCTEXT("Empty", "No stack of this bound consumable remains in the shared inventory."));
			}
			FKnightInventoryMutation Removal;
			Removal.Operation = EKnightInventoryOperation::Remove;
			Removal.Item = *Stack;
			Removal.Item.Count = 1;
			Removal.ExpectedItemRevision = Stack->ItemRevision;
			Request.Mutations.Add(Removal);
			OutAffected = Stack->InstanceId;
		}
	}
	else if (Command.Operation == EKnightRPGOperation::ApplyGearPreset)
	{
		const bool bGuard = Command.PresetId == TEXT("Guard");
		const bool bMend = Command.PresetId == TEXT("Mend");
		TArray<FName> Ids { bGuard ? FName(TEXT("W02")) : bMend ? FName(TEXT("W03")) : FName(TEXT("W04")) };
		for (int32 Index = 8; Index <= 14; ++Index)
		{
			Ids.Add(FName(*FString::Printf(TEXT("A%02d"), Index)));
		}
		Ids.Add(TEXT("N02"));
		Ids.Add(bGuard ? TEXT("F01") : bMend ? TEXT("F02") : TEXT("F03"));
		Ids.Add(TEXT("F04"));
		Ids.Add(bGuard ? TEXT("T01") : bMend ? TEXT("T02") : TEXT("T03"));
		Knight->Assignments.Reset();
		Knight->WeaponRuneInstanceId.Invalidate();
		Knight->Build.WeaponRune = NAME_None;
		TArray<FKnightItemInstance> Sorted = InventoryView.Items;
		Sorted.Sort([](const FKnightItemInstance& Left, const FKnightItemInstance& Right)
		{
			return Left.InstanceId < Right.InstanceId;
		});
		for (int32 Index = 0; Index < Ids.Num(); ++Index)
		{
			const FPrimaryAssetId Id = FKnightGearCatalog::DefinitionId(Ids[Index]);
			const FKnightItemInstance* Selected = Sorted.FindByPredicate([Knight, &Id](const FKnightItemInstance& Entry)
			{
				return Entry.DefinitionId == Id && Entry.UpgradeRank == 0 && (Entry.LocationId == KnightGear::InventoryLocation
					|| Entry.LocationId == KnightGear::EquipmentLocation && Entry.AssignedKnightId == Knight->KnightId);
			});
			if (Selected == nullptr)
			{
				return FKnightResult::Failure(EKnightError::NotFound, TEXT("GearPreset"), LOCTEXT("Preset", "The explicit demo preset requires all twelve real available rank-zero equipment instances."));
			}
			Result = Assign(*Knight, *Selected, KnightGear::EquipmentSlots()[Index]);
			if (!Result.IsSuccess())
			{
				return Result;
			}
		}
	}
	else
	{
		return FKnightResult::Failure(EKnightError::InvalidData, TEXT("GearOperation"), LOCTEXT("Operation", "The gear operation is unsupported."));
	}
	if (!Result.IsSuccess())
	{
		return Result;
	}
	if (Candidate.Currency < 0 || Candidate.Currency > KnightGear::MaximumCurrency)
	{
		return FKnightResult::Failure(EKnightError::CapacityExceeded, TEXT("Currency"), LOCTEXT("Currency", "The shared currency scalar cannot exceed its supported balance."));
	}
	return FKnightResult::Success();
}

FKnightResult UKnightRPGComponent::SynchronizeLocations(const FKnightRPGView& Candidate,
	const FKnightInventoryView& InventoryView, FKnightInventoryRequest& Request) const
{
	TArray<FKnightItemInstance> Values = KnightRPGGear::ApplyValues(InventoryView, Request);
	const TArray<FKnightItemInstance> PendingValues = Values;
	TMap<FGuid, FKnightEquipmentAssignment> Equipment;
	TMap<FGuid, FKnightEntityId> Owners;
	TMap<FGuid, FKnightEntityId> SkillOwners;
	for (const FKnightRPGKnightView& Knight : Candidate.Knights)
	{
		for (const FKnightEquipmentAssignment& Assignment : Knight.Assignments)
		{
			if (Equipment.Contains(Assignment.ItemInstanceId))
			{
				return FKnightResult::Failure(EKnightError::Conflict, TEXT("GearLocation"),
					LOCTEXT("DuplicateLocation", "An exact equipment instance has one canonical owner."));
			}
			Equipment.Add(Assignment.ItemInstanceId, Assignment);
			Owners.Add(Assignment.ItemInstanceId, Knight.KnightId);
		}
		const FKnightEquipmentAssignment* Weapon = Knight.FindAssignment(KnightGameplayTags::Slot_Equipment_Weapon);
		if (Weapon != nullptr)
		{
			FKnightItemInstance* Parent = Values.FindByPredicate([Weapon](const FKnightItemInstance& Item)
			{
				return Item.InstanceId == Weapon->ItemInstanceId;
			});
			if (Parent != nullptr)
			{
				Parent->RuneAttachmentId = Knight.WeaponRuneInstanceId;
			}
		}
		if (Knight.SkillRuneInstanceId.IsValid())
		{
			if (SkillOwners.Contains(Knight.SkillRuneInstanceId))
			{
				return FKnightResult::Failure(EKnightError::Conflict, TEXT("SkillSocket"),
					LOCTEXT("SkillSocketOwner", "Each skill rune has one canonical socket owner."));
			}
			SkillOwners.Add(Knight.SkillRuneInstanceId, Knight.KnightId);
		}
	}
	TMap<FGuid, FGuid> RuneParents;
	for (const FKnightItemInstance& Parent : Values)
	{
		if (Parent.RuneAttachmentId.IsValid())
		{
			if (RuneParents.Contains(Parent.RuneAttachmentId) || SkillOwners.Contains(Parent.RuneAttachmentId))
			{
				return FKnightResult::Failure(EKnightError::Conflict, TEXT("WeaponSocket"),
					LOCTEXT("WeaponSocketOwner", "A rune cannot belong to multiple weapon or skill sockets."));
			}
			RuneParents.Add(Parent.RuneAttachmentId, Parent.InstanceId);
		}
	}
	for (FKnightItemInstance& Item : Values)
	{
		Item.LocationId = KnightGear::InventoryLocation;
		Item.AssignedKnightId = FKnightEntityId();
		Item.SlotTag = FGameplayTag();
		Item.ParentWeaponId.Invalidate();
		if (const FKnightEquipmentAssignment* Assignment = Equipment.Find(Item.InstanceId))
		{
			Item.LocationId = KnightGear::EquipmentLocation;
			Item.AssignedKnightId = Owners.FindChecked(Item.InstanceId);
			Item.SlotTag = Assignment->SlotTag;
		}
		else if (const FGuid* Parent = RuneParents.Find(Item.InstanceId))
		{
			Item.LocationId = KnightGear::WeaponSocketLocation;
			Item.ParentWeaponId = *Parent;
			if (const FKnightEntityId* Owner = Owners.Find(*Parent))
			{
				Item.AssignedKnightId = *Owner;
			}
		}
		else if (const FKnightEntityId* Owner = SkillOwners.Find(Item.InstanceId))
		{
			Item.LocationId = KnightGear::SkillSocketLocation;
			Item.AssignedKnightId = *Owner;
		}
		const FKnightItemInstance* Pending = PendingValues.FindByPredicate([&Item](const FKnightItemInstance& Entry)
		{
			return Entry.InstanceId == Item.InstanceId;
		});
		if (Pending != nullptr && !(Item == *Pending))
		{
			KnightRPGGear::SetValue(InventoryView, Request, Item);
		}
	}
	return ValidateGearLocations(Candidate, Values);
}

FKnightResult UKnightRPGComponent::ValidateGearLocations(const FKnightRPGView& View,
	const TArray<FKnightItemInstance>& Items) const
{
	if (!View.IsItemizationEnabled())
	{
		return FKnightResult::Success();
	}
	TSet<FGuid> Located;
	for (const FKnightRPGKnightView& Knight : View.Knights)
	{
		for (const FKnightEquipmentAssignment& Assignment : Knight.Assignments)
		{
			const FKnightItemInstance* Item = Items.FindByPredicate([&Assignment](const FKnightItemInstance& Entry)
			{
				return Entry.InstanceId == Assignment.ItemInstanceId && Entry.DefinitionId == Assignment.DefinitionId;
			});
			if (!KnightGear::IsEquipmentSlot(Assignment.SlotTag) || Item == nullptr || Located.Contains(Item->InstanceId)
				|| Item->LocationId != KnightGear::EquipmentLocation || Item->AssignedKnightId != Knight.KnightId
				|| Item->SlotTag != Assignment.SlotTag)
			{
				return FKnightResult::Failure(EKnightError::Conflict, TEXT("EquipmentLocation"),
					LOCTEXT("EquipmentLocation", "Equipment assignments and exact instance locations must agree."));
			}
			Located.Add(Item->InstanceId);
		}
		const FKnightEquipmentAssignment* Weapon = Knight.FindAssignment(KnightGameplayTags::Slot_Equipment_Weapon);
		const FKnightItemInstance* Parent = Weapon == nullptr ? nullptr : Items.FindByPredicate(
			[Weapon](const FKnightItemInstance& Item) { return Item.InstanceId == Weapon->ItemInstanceId; });
		if ((Parent == nullptr && Knight.WeaponRuneInstanceId.IsValid())
			|| (Parent != nullptr && Parent->RuneAttachmentId != Knight.WeaponRuneInstanceId))
		{
			return FKnightResult::Failure(EKnightError::Conflict, TEXT("ActiveWeaponRune"),
				LOCTEXT("ActiveWeaponRune", "The active weapon rune must belong to the equipped exact parent hammer."));
		}
		if (Knight.SkillRuneInstanceId.IsValid())
		{
			const FKnightItemInstance* Rune = Items.FindByPredicate([&Knight](const FKnightItemInstance& Item)
			{
				return Item.InstanceId == Knight.SkillRuneInstanceId;
			});
			if (Rune == nullptr || Located.Contains(Rune->InstanceId) || Rune->LocationId != KnightGear::SkillSocketLocation
				|| Rune->AssignedKnightId != Knight.KnightId)
			{
				return FKnightResult::Failure(EKnightError::Conflict, TEXT("SkillRuneLocation"),
					LOCTEXT("SkillRuneLocation", "Skill runes retain exactly one Knight socket location."));
			}
			Located.Add(Rune->InstanceId);
		}
	}
	for (const FKnightItemInstance& Parent : Items)
	{
		if (!Parent.RuneAttachmentId.IsValid())
		{
			continue;
		}
		const FKnightItemInstance* Rune = Items.FindByPredicate([&Parent](const FKnightItemInstance& Item)
		{
			return Item.InstanceId == Parent.RuneAttachmentId;
		});
		const UKnightDefinition* ParentDefinition = nullptr;
		const UKnightDefinition* RuneDefinition = nullptr;
		if (Rune == nullptr || Located.Contains(Parent.RuneAttachmentId)
			|| !ResolveDefinition(Parent.DefinitionId, UKnightItemDefinition::StaticClass(), ParentDefinition).IsSuccess()
			|| !ResolveDefinition(Rune->DefinitionId, UKnightRuneDefinition::StaticClass(), RuneDefinition).IsSuccess()
			|| CastChecked<UKnightItemDefinition>(ParentDefinition)->GetMetadata().Subtype != EKnightEquipmentSubtype::Hammer
			|| CastChecked<UKnightRuneDefinition>(RuneDefinition)->GetAttachmentRuneId() != TEXT("Weapon.Aftershock")
			|| Rune->LocationId != KnightGear::WeaponSocketLocation || Rune->ParentWeaponId != Parent.InstanceId
			|| Rune->AssignedKnightId != Parent.AssignedKnightId)
		{
			return FKnightResult::Failure(EKnightError::Conflict, TEXT("WeaponRuneParent"),
				LOCTEXT("WeaponRuneParent", "Every attached rune retains its exact compatible hammer parent and location."));
		}
		Located.Add(Rune->InstanceId);
	}
	for (const FKnightItemInstance& Item : Items)
	{
		if (!Item.ValidateIdentityAndCount().IsSuccess()
			|| (Item.LocationId != KnightGear::InventoryLocation && !Located.Contains(Item.InstanceId)))
		{
			return FKnightResult::Failure(EKnightError::Conflict, TEXT("ExclusiveLocation"),
				LOCTEXT("ExclusiveLocation", "Every owned instance has one valid bag, equipment or socket location."));
		}
	}
	return FKnightResult::Success();
}

FKnightResult UKnightRPGComponent::PreviewEquip(const FKnightEntityId& KnightId, const FGuid& InstanceId,
	FGameplayTag SlotTag, FKnightItemStatBonuses& OutBonuses) const
{
	OutBonuses = FKnightItemStatBonuses();
	if (!IsInitialized() || Inventory == nullptr || bTransactionInProgress || !PublishedView.IsItemizationEnabled())
	{
		return FKnightResult::Failure(EKnightError::InvalidState, TEXT("GearPreview"),
			LOCTEXT("PreviewState", "Compare requires an initialized confirmed gear inventory."));
	}
	const FKnightInventoryView InventoryView = Inventory->GetView();
	const FKnightItemInstance* Item = InventoryView.Items.FindByPredicate([&InstanceId](const FKnightItemInstance& Entry)
	{
		return Entry.InstanceId == InstanceId;
	});
	FKnightRPGCommand Command;
	Command.Operation = EKnightRPGOperation::Equip;
	Command.RequestId = FGuid::NewGuid();
	Command.InventoryId = InventoryView.InventoryId;
	Command.ExpectedInventoryRevision = InventoryView.Revision;
	Command.ExpectedKnightId = KnightId;
	Command.ExpectedKnightRevision = GetKnightView(KnightId).Revision;
	Command.ItemInstanceId = InstanceId;
	Command.ExpectedItemRevision = Item == nullptr ? 0 : Item->ItemRevision;
	Command.SlotTag = SlotTag;
	FKnightRPGView Candidate;
	FKnightInventoryRequest Request;
	FGuid Affected;
	FKnightResult Result = BuildCandidate(PublishedView, InventoryView, Command, nullptr, nullptr, Candidate, Request, Affected);
	if (!Result.IsSuccess())
	{
		return Result;
	}
	FKnightRPGKnightView* Knight = Candidate.Knights.FindByPredicate([&KnightId](const FKnightRPGKnightView& Entry)
	{
		return Entry.KnightId == KnightId;
	});
	FKnightGASProjectionPlan Plan;
	Result = BuildProjection(*Knight, KnightRPGGear::ApplyValues(InventoryView, Request), Plan);
	if (Result.IsSuccess())
	{
		OutBonuses = Knight->StatBonuses;
	}
	return Result;
}

FKnightResult UKnightRPGComponent::PreviewTransfer(const FKnightEntityId& SourceKnightId,
	const FKnightEntityId& TargetKnightId, const FGuid& InstanceId, FGameplayTag SlotTag,
	FKnightItemStatBonuses& OutSource, FKnightItemStatBonuses& OutTarget) const
{
	OutSource = FKnightItemStatBonuses();
	OutTarget = FKnightItemStatBonuses();
	if (!IsInitialized() || Inventory == nullptr || bTransactionInProgress || !PublishedView.IsItemizationEnabled()
		|| SourceKnightId == TargetKnightId)
	{
		return FKnightResult::Failure(EKnightError::InvalidState, TEXT("TransferPreview"),
			LOCTEXT("TransferPreviewState", "Compare a real transfer between two distinct confirmed Knights."));
	}
	const FKnightInventoryView InventoryView = Inventory->GetView();
	const FKnightItemInstance* Item = InventoryView.Items.FindByPredicate([&InstanceId](const FKnightItemInstance& Entry)
	{
		return Entry.InstanceId == InstanceId;
	});
	FKnightRPGCommand Command;
	Command.Operation = EKnightRPGOperation::TransferEquipment;
	Command.RequestId = FGuid::NewGuid();
	Command.InventoryId = InventoryView.InventoryId;
	Command.ExpectedInventoryRevision = InventoryView.Revision;
	Command.ExpectedKnightId = SourceKnightId;
	Command.ExpectedKnightRevision = GetKnightView(SourceKnightId).Revision;
	Command.TargetKnightId = TargetKnightId;
	Command.ExpectedTargetKnightRevision = GetKnightView(TargetKnightId).Revision;
	Command.ItemInstanceId = InstanceId;
	Command.ExpectedItemRevision = Item == nullptr ? 0 : Item->ItemRevision;
	Command.SlotTag = SlotTag;
	FKnightRPGView Candidate;
	FKnightInventoryRequest Request;
	FGuid Affected;
	FKnightResult Result = BuildCandidate(PublishedView, InventoryView, Command, nullptr, nullptr, Candidate, Request, Affected);
	if (!Result.IsSuccess())
	{
		return Result;
	}
	const TArray<FKnightItemInstance> Values = KnightRPGGear::ApplyValues(InventoryView, Request);
	for (FKnightRPGKnightView& Knight : Candidate.Knights)
	{
		if (Knight.KnightId != SourceKnightId && Knight.KnightId != TargetKnightId)
		{
			continue;
		}
		FKnightGASProjectionPlan Plan;
		Result = BuildProjection(Knight, Values, Plan);
		if (!Result.IsSuccess())
		{
			return Result;
		}
		(Knight.KnightId == SourceKnightId ? OutSource : OutTarget) = Knight.StatBonuses;
	}
	return FKnightResult::Success();
}

FGuid UKnightRPGComponent::GetWeaponRuneParentId(const FGuid& RuneId) const
{
	if (Inventory == nullptr || !RuneId.IsValid())
	{
		return FGuid();
	}
	const FKnightInventoryView View = Inventory->GetView();
	const FKnightItemInstance* Rune = View.Items.FindByPredicate([&RuneId](const FKnightItemInstance& Item)
	{
		return Item.InstanceId == RuneId;
	});
	if (Rune != nullptr && Rune->ParentWeaponId.IsValid())
	{
		return Rune->ParentWeaponId;
	}
	for (const FKnightRPGKnightView& Knight : PublishedView.Knights)
	{
		if (Knight.WeaponRuneInstanceId == RuneId)
		{
			const FKnightEquipmentAssignment* Weapon = Knight.FindAssignment(KnightGameplayTags::Slot_Equipment_Weapon);
			return Weapon == nullptr ? FGuid() : Weapon->ItemInstanceId;
		}
	}
	return FGuid();
}

#undef LOCTEXT_NAMESPACE
