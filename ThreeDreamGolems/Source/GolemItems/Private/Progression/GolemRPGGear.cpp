#include "Progression/GolemRPGComponent.h"

#include "Catalog/GolemGearCatalog.h"
#include "Definitions/GolemItemDefinition.h"
#include "Definitions/GolemRuneDefinition.h"
#include "Inventory/GolemInventoryComponent.h"
#include "Tags/GolemGameplayTags.h"

#define LOCTEXT_NAMESPACE "GolemRPGGear"

namespace GolemRPGGear
{
	TArray<FGolemItemInstance> ApplyValues(const FGolemInventoryView& Inventory, const FGolemInventoryRequest& Request)
	{
		TArray<FGolemItemInstance> Result = Inventory.Items;
		for (const FGolemInventoryMutation& Mutation : Request.Mutations)
		{
			FGolemItemInstance* Item = Result.FindByPredicate([&Mutation](const FGolemItemInstance& Entry)
			{
				return Entry.InstanceId == Mutation.Item.InstanceId;
			});
			if (Mutation.Operation == EGolemInventoryOperation::Update)
			{
				if (Item != nullptr)
				{
					*Item = Mutation.Item;
				}
			}
			else if (Mutation.Operation == EGolemInventoryOperation::Add)
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
					Result.RemoveAll([&Mutation](const FGolemItemInstance& Entry)
					{
						return Entry.InstanceId == Mutation.Item.InstanceId;
					});
				}
			}
		}
		Result.Sort([](const FGolemItemInstance& Left, const FGolemItemInstance& Right)
		{
			return Left.InstanceId < Right.InstanceId;
		});
		return Result;
	}

	void SetValue(const FGolemInventoryView& Inventory, FGolemInventoryRequest& Request, FGolemItemInstance Value)
	{
		FGolemInventoryMutation* Pending = Request.Mutations.FindByPredicate([&Value](const FGolemInventoryMutation& Entry)
		{
			return Entry.Item.InstanceId == Value.InstanceId;
		});
		const FGolemItemInstance* Original = Inventory.Items.FindByPredicate([&Value](const FGolemItemInstance& Entry)
		{
			return Entry.InstanceId == Value.InstanceId;
		});
		if (Pending != nullptr && Pending->Operation == EGolemInventoryOperation::Add && Original == nullptr)
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
		Pending->Operation = EGolemInventoryOperation::Update;
		Pending->ExpectedItemRevision = Original->ItemRevision;
		Pending->Item = Value;
	}
}

FGolemResult UGolemRPGComponent::ConfigureGearDemo(const FGuid& DemoRunId, int64 InitialCurrency,
	int32 InitialLevel, bool bEquipStarter)
{
	const FGolemResult Authority = ValidateAuthority();
	if (!Authority.IsSuccess())
	{
		return Authority;
	}
	if (IsInitialized() || bTransactionInProgress || GearDemoRunId.IsValid() || !DemoRunId.IsValid()
		|| InitialCurrency < 0 || InitialCurrency > GolemGear::MaximumCurrency
		|| (InitialLevel != 1 && InitialLevel != 8))
	{
		return FGolemResult::Failure(EGolemError::InvalidData, TEXT("GearDemoConfiguration"),
			LOCTEXT("Configure", "Configure a fresh isolated progression or level-eight catalog demo once."));
	}
	GearDemoRunId = DemoRunId;
	GearInitialCurrency = InitialCurrency;
	GearInitialLevel = InitialLevel;
	bGearEquipStarter = bEquipStarter;
	return FGolemResult::Success();
}

FGolemResult UGolemRPGComponent::RegisterRewardBundle(const FGolemGearRewardBundle& Bundle)
{
	const FGolemResult Authority = ValidateAuthority();
	if (!Authority.IsSuccess())
	{
		return Authority;
	}
	if (!PublishedView.bGearDemo || bTransactionInProgress || !Bundle.RewardId.IsValid()
		|| Bundle.DemoRunId != PublishedView.DemoRunId || Bundle.Currency < 0
		|| Bundle.Currency > GolemGear::MaximumCurrency || (Bundle.Items.IsEmpty() && Bundle.BoundItems.IsEmpty())
		|| Bundle.Items.Num() > GolemItems::MaximumBatchOperations || Bundle.BoundItems.Num() > 3
		|| Bundle.Level < 1 || Bundle.Level > 8)
	{
		return FGolemResult::Failure(EGolemError::InvalidData, TEXT("GearReward"),
			LOCTEXT("Reward", "An authoritative reward belongs to this isolated demo generation and has a complete bounded bundle."));
	}
	if (const FGolemGearRewardBundle* Existing = RewardOffers.FindByPredicate([&Bundle](const FGolemGearRewardBundle& Entry)
	{
		return Entry.RewardId == Bundle.RewardId;
	}))
	{
		return *Existing == Bundle ? FGolemResult::Success()
			: FGolemResult::Failure(EGolemError::Conflict, TEXT("GearReward"), LOCTEXT("OfferReplay", "A reward identity cannot change its authoritative bundle."));
	}
	TSet<FPrimaryAssetId> Definitions;
	TSet<FGuid> Bound;
	const FGolemInventoryView BoundInventory = Inventory->GetView();
	for (const FGuid Id : Bundle.BoundItems)
	{
		const FGolemItemInstance* Item = BoundInventory.Items.FindByPredicate([Id](const FGolemItemInstance& Row)
		{
			return Row.InstanceId == Id;
		});
		const UGolemDefinition* Definition = nullptr;
		if (!Id.IsValid() || Bound.Contains(Id) || Item == nullptr
			|| !ResolveDefinition(Item->DefinitionId, UGolemItemDefinition::StaticClass(), Definition).IsSuccess()
			|| CastChecked<UGolemItemDefinition>(Definition)->GetMetadata().Subtype != EGolemEquipmentSubtype::Hammer)
		{
			return FGolemResult::Failure(EGolemError::InvalidData, TEXT("BoundRewardHammer"), FText::GetEmpty());
		}
		Bound.Add(Id);
	}
	for (const FGolemGearRewardItem& Item : Bundle.Items)
	{
		const UGolemDefinition* Definition = nullptr;
		const FGolemResult Resolution = ResolveDefinition(Item.DefinitionId, UGolemItemDefinition::StaticClass(), Definition);
		if (!Resolution.IsSuccess())
		{
			return Resolution;
		}
		if (Item.Count < 1 || Item.Count > GolemItems::MaximumStackCount || Definitions.Contains(Item.DefinitionId))
		{
			return FGolemResult::Failure(EGolemError::InvalidData, TEXT("GearReward"), LOCTEXT("OfferCount", "Reward quantities are positive and each definition occurs once."));
		}
		Definitions.Add(Item.DefinitionId);
	}
	RewardOffers.Add(Bundle);
	return FGolemResult::Success();
}

TArray<FGolemGearRewardBundle> UGolemRPGComponent::GetRewardOffers() const
{
	return RewardOffers;
}

FGolemResult UGolemRPGComponent::SeedGearAssignments(FGolemRPGView& Candidate,
	const FGolemInventoryView& InventoryView, FGolemInventoryRequest& Request) const
{
	Request.RequestId = FGuid::NewGuid();
	Request.ExpectedRevision = InventoryView.Revision;
	if (!bGearEquipStarter)
	{
		return FGolemResult::Success();
	}
	TSet<FGuid> Selected;
	TArray<FGolemItemInstance> SortedItems = InventoryView.Items;
	SortedItems.Sort([](const FGolemItemInstance& Left, const FGolemItemInstance& Right)
	{
		return Left.InstanceId < Right.InstanceId;
	});
	for (FGolemRPGGolemView& Golem : Candidate.Golems)
	{
		for (int32 Index = 0; Index < 8; ++Index)
		{
			const FPrimaryAssetId Id = FGolemGearCatalog::DefinitionId(Index == 0 ? FName(TEXT("W01"))
				: FName(*FString::Printf(TEXT("A%02d"), Index)));
			const FGolemItemInstance* Item = SortedItems.FindByPredicate([&Id, &Selected](const FGolemItemInstance& Entry)
			{
				return Entry.DefinitionId == Id && Entry.Count == 1 && !Selected.Contains(Entry.InstanceId)
					&& Entry.LocationId == GolemGear::InventoryLocation;
			});
			if (Item == nullptr)
			{
				return FGolemResult::Failure(EGolemError::NotFound, TEXT("GearStarter"), LOCTEXT("Starter", "Each canonical Golem needs eight distinct real starter item GUIDs."));
			}
			Selected.Add(Item->InstanceId);
			FGolemEquipmentAssignment Assignment;
			Assignment.ItemInstanceId = Item->InstanceId;
			Assignment.DefinitionId = Id;
			Assignment.SlotTag = GolemGear::EquipmentSlots()[Index];
			Golem.Assignments.Add(Assignment);
		}
	}
	return SynchronizeLocations(Candidate, InventoryView, Request);
}

FGolemResult UGolemRPGComponent::BuildGearCandidate(FGolemRPGView& Candidate,
	const FGolemInventoryView& InventoryView, const FGolemRPGCommand& Command,
	const TArray<FGolemInventoryMutation>* HistoricalMutations, FGolemInventoryRequest& Request, FGuid& OutAffected) const
{
	using namespace GolemRPGGear;
	FGolemRPGGolemView* Golem = Candidate.Golems.FindByPredicate([&Command](const FGolemRPGGolemView& Entry)
	{
		return Entry.GolemId == Command.ExpectedGolemId;
	});
	if (!Candidate.IsItemizationEnabled() || Golem == nullptr || !Golem->bGearEnabled)
	{
		return FGolemResult::Failure(EGolemError::InvalidState, TEXT("GearCatalogUnlock"),
			LOCTEXT("Enabled", "Gear commands require explicitly enabled twelve-slot itemization."));
	}
	if (!Candidate.bGearDemo && (Command.Operation == EGolemRPGOperation::ClaimReward
		|| Command.Operation == EGolemRPGOperation::ApplyGearPreset))
	{
		return FGolemResult::Failure(EGolemError::NotOwned, TEXT("GearFixtureAuthority"),
			LOCTEXT("FixtureOnly", "Demo rewards and fixture presets remain restricted to isolated GearLab generations."));
	}
	const FGolemItemInstance* Item = InventoryView.Items.FindByPredicate([&Command](const FGolemItemInstance& Entry)
	{
		return Entry.InstanceId == Command.ItemInstanceId;
	});
	const UGolemItemDefinition* Definition = nullptr;
	if (Command.ItemInstanceId.IsValid())
	{
		if (Command.ExpectedItemRevision < 1 || Item == nullptr || Item->ItemRevision != Command.ExpectedItemRevision)
		{
			return FGolemResult::Failure(EGolemError::InvalidRevision, TEXT("GearItemRevision"), LOCTEXT("Item", "The exact selected item GUID and revision must still exist."));
		}
		const UGolemDefinition* Resolved = nullptr;
		const FGolemResult Result = ResolveDefinition(Item->DefinitionId, UGolemItemDefinition::StaticClass(), Resolved);
		if (!Result.IsSuccess())
		{
			return Result;
		}
		Definition = CastChecked<UGolemItemDefinition>(Resolved);
	}
	auto AddItems = [this, &InventoryView, &Request, &Command, HistoricalMutations](const FPrimaryAssetId& Id,
		int32 Count, const FGuid& RewardReceipt) -> FGolemResult
	{
		const UGolemDefinition* Resolved = nullptr;
		const FGolemResult Resolution = ResolveDefinition(Id, UGolemItemDefinition::StaticClass(), Resolved);
		if (!Resolution.IsSuccess())
		{
			return Resolution;
		}
		const UGolemItemDefinition* ItemDefinition = CastChecked<UGolemItemDefinition>(Resolved);
		TArray<FGolemItemInstance> Values = ApplyValues(InventoryView, Request);
		if (ItemDefinition->GetMetadata().bUniqueOwned && Values.ContainsByPredicate([&Id](const FGolemItemInstance& Entry)
		{
			return Entry.DefinitionId == Id;
		}))
		{
			return FGolemResult::Failure(EGolemError::Conflict, TEXT("AlreadyOwned"), LOCTEXT("UniqueOwned", "The party already owns this unique quest token; no part of the reward was claimed."));
		}
		for (const FGolemItemInstance& Existing : Values)
		{
			if (Count <= 0)
			{
				break;
			}
			const int32 Room = ItemDefinition->GetMaximumStack() - Existing.Count;
			if (Existing.DefinitionId != Id || Existing.LocationId != GolemGear::InventoryLocation || Room <= 0)
			{
				continue;
			}
			FGolemInventoryMutation* Mutation = Request.Mutations.FindByPredicate([&Existing](const FGolemInventoryMutation& Entry)
			{
				return Entry.Item.InstanceId == Existing.InstanceId;
			});
			const int32 Added = FMath::Min(Room, Count);
			Count -= Added;
			if (Mutation != nullptr && Mutation->Operation == EGolemInventoryOperation::Add)
			{
				Mutation->Item.Count += Added;
			}
			else
			{
				FGolemInventoryMutation Addition;
				Addition.Item = Existing;
				Addition.Item.Count = Added;
				Addition.ExpectedItemRevision = Existing.ItemRevision;
				Request.Mutations.Add(Addition);
			}
		}
		while (Count > 0)
		{
			FGolemInventoryMutation Addition;
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
		return FGolemResult::Success();
	};
	auto RemoveItems = [&InventoryView, &Request](const FPrimaryAssetId& Id, int32 Count) -> FGolemResult
	{
		for (const FGolemItemInstance& Value : ApplyValues(InventoryView, Request))
		{
			if (Count <= 0)
			{
				break;
			}
			if (Value.DefinitionId != Id || Value.LocationId != GolemGear::InventoryLocation)
			{
				continue;
			}
			FGolemInventoryMutation Mutation;
			Mutation.Operation = EGolemInventoryOperation::Remove;
			Mutation.Item = Value;
			Mutation.Item.Count = FMath::Min(Count, Value.Count);
			Mutation.ExpectedItemRevision = Value.ItemRevision;
			Count -= Mutation.Item.Count;
			Request.Mutations.Add(Mutation);
		}
		return Count == 0 ? FGolemResult::Success()
			: FGolemResult::Failure(EGolemError::InsufficientResources, TEXT("GearMaterials"), LOCTEXT("Materials", "The complete exact upgrade material cost must be available in the shared bag."));
	};
	auto Assign = [&Candidate, Golem, &InventoryView](FGolemRPGGolemView& Target,
		const FGolemItemInstance& Value, FGameplayTag Slot) -> FGolemResult
	{
		for (const FGolemRPGGolemView& Owner : Candidate.Golems)
		{
			if (Owner.Assignments.ContainsByPredicate([&Value](const FGolemEquipmentAssignment& Entry)
			{
				return Entry.ItemInstanceId == Value.InstanceId;
			}))
			{
				return FGolemResult::Failure(EGolemError::NotOwned, TEXT("EquipmentLocation"), LOCTEXT("Assigned", "An existing equipment assignment must move through an explicit transfer or ring swap."));
			}
		}
		Target.Assignments.RemoveAll([Slot](const FGolemEquipmentAssignment& Entry)
		{
			return Entry.SlotTag == Slot;
		});
		FGolemEquipmentAssignment Assignment;
		Assignment.ItemInstanceId = Value.InstanceId;
		Assignment.DefinitionId = Value.DefinitionId;
		Assignment.SlotTag = Slot;
		Target.Assignments.Add(Assignment);
		if (Slot == GolemGameplayTags::Slot_Equipment_Weapon)
		{
			Target.WeaponRuneInstanceId = Value.RuneAttachmentId;
			Target.Build.WeaponRune = Value.RuneAttachmentId.IsValid() ? FName(TEXT("Weapon.Aftershock")) : NAME_None;
		}
		return FGolemResult::Success();
	};
	FGolemResult Result = FGolemResult::Success();
	if (Command.Operation == EGolemRPGOperation::Equip)
	{
		if (Item->LocationId != GolemGear::InventoryLocation || Definition->GetMetadata().Category != EGolemItemCategory::Equipment
			|| !Definition->GetMetadata().AllowedSlots.Contains(Command.SlotTag) || Golem->Level < Definition->GetMetadata().RequiredLevel)
		{
			return FGolemResult::Failure(EGolemError::InvalidState, TEXT("EquipmentRequirement"), LOCTEXT("Equip", "Choose an available bag item, compatible explicit slot and sufficient character level."));
		}
		Result = Assign(*Golem, *Item, Command.SlotTag);
		OutAffected = Item->InstanceId;
	}
	else if (Command.Operation == EGolemRPGOperation::Unequip || Command.Operation == EGolemRPGOperation::TransferEquipment)
	{
		const FGolemEquipmentAssignment* Current = Golem->Assignments.FindByPredicate([Item](const FGolemEquipmentAssignment& Entry)
		{
			return Entry.ItemInstanceId == Item->InstanceId;
		});
		if (Current == nullptr || (Command.Operation == EGolemRPGOperation::Unequip && Current->SlotTag != Command.SlotTag))
		{
			return FGolemResult::Failure(EGolemError::NotOwned, TEXT("EquipmentOwner"), LOCTEXT("Owner", "Only the exact source Golem may unequip or transfer its currently assigned item."));
		}
		const FGameplayTag PreviousSlot = Current->SlotTag;
		Golem->Assignments.RemoveAll([Item](const FGolemEquipmentAssignment& Entry)
		{
			return Entry.ItemInstanceId == Item->InstanceId;
		});
		if (PreviousSlot == GolemGameplayTags::Slot_Equipment_Weapon)
		{
			Golem->WeaponRuneInstanceId.Invalidate();
			Golem->Build.WeaponRune = NAME_None;
		}
		if (Command.Operation == EGolemRPGOperation::TransferEquipment)
		{
			FGolemRPGGolemView* Target = Candidate.Golems.FindByPredicate([&Command](const FGolemRPGGolemView& Entry)
			{
				return Entry.GolemId == Command.TargetGolemId;
			});
			if (Target == nullptr || Target->Revision != Command.ExpectedTargetGolemRevision || Target->Revision >= MAX_int64
				|| Target->Level < Definition->GetMetadata().RequiredLevel || !Definition->GetMetadata().AllowedSlots.Contains(Command.SlotTag))
			{
				return FGolemResult::Failure(EGolemError::InvalidRevision, TEXT("EquipmentTarget"), LOCTEXT("Target", "The target Golem's exact revision, item level requirement and destination slot must be valid."));
			}
			Result = Assign(*Target, *Item, Command.SlotTag);
			++Target->Revision;
		}
		OutAffected = Item->InstanceId;
	}
	else if (Command.Operation == EGolemRPGOperation::SwapRings)
	{
		FGolemEquipmentAssignment* First = Golem->Assignments.FindByPredicate([](const FGolemEquipmentAssignment& Entry)
		{
			return Entry.SlotTag == GolemGameplayTags::Slot_Equipment_Ring1;
		});
		FGolemEquipmentAssignment* Second = Golem->Assignments.FindByPredicate([](const FGolemEquipmentAssignment& Entry)
		{
			return Entry.SlotTag == GolemGameplayTags::Slot_Equipment_Ring2;
		});
		if (First == nullptr || Second == nullptr)
		{
			return FGolemResult::Failure(EGolemError::InvalidState, TEXT("RingSwap"), LOCTEXT("Rings", "Both ring slots must be occupied for an atomic ring swap."));
		}
		Swap(First->SlotTag, Second->SlotTag);
		OutAffected = First->ItemInstanceId;
	}
	else if (Command.Operation == EGolemRPGOperation::AttachWeaponRune || Command.Operation == EGolemRPGOperation::DetachWeaponRune)
	{
		if (Definition->GetMetadata().Subtype != EGolemEquipmentSubtype::Hammer
			|| (Item->LocationId != GolemGear::InventoryLocation && Item->AssignedGolemId != Golem->GolemId))
		{
			return FGolemResult::Failure(EGolemError::NotOwned, TEXT("WeaponRuneParent"), LOCTEXT("Parent", "The exact hammer parent must be in the bag or equipped by the editable Golem."));
		}
		if (Command.Operation == EGolemRPGOperation::AttachWeaponRune)
		{
			const FGolemItemInstance* Rune = InventoryView.Items.FindByPredicate([&Command](const FGolemItemInstance& Entry)
			{
				return Entry.InstanceId == Command.WeaponRuneInstanceId && Entry.LocationId == GolemGear::InventoryLocation;
			});
			const UGolemDefinition* Resolved = nullptr;
			if (Rune == nullptr)
			{
				return FGolemResult::Failure(EGolemError::NotOwned, TEXT("WeaponRune"), LOCTEXT("RuneBag", "Attach an exact available rune instance from shared inventory."));
			}
			Result = ResolveDefinition(Rune->DefinitionId, UGolemRuneDefinition::StaticClass(), Resolved);
			if (!Result.IsSuccess())
			{
				return Result;
			}
			if (CastChecked<UGolemRuneDefinition>(Resolved)->GetAttachmentRuneId() != TEXT("Weapon.Aftershock"))
			{
				return FGolemResult::Failure(EGolemError::TypeMismatch, TEXT("WeaponRune"), LOCTEXT("RuneType", "Only Aftershock is compatible with the heavy-hammer weapon socket."));
			}
		}
		else if (!Item->RuneAttachmentId.IsValid())
		{
			return FGolemResult::Failure(EGolemError::NotFound, TEXT("WeaponRune"), LOCTEXT("NoRune", "This exact hammer has no attached rune to detach."));
		}
		FGolemItemInstance Changed = *Item;
		Changed.RuneAttachmentId = Command.WeaponRuneInstanceId;
		SetValue(InventoryView, Request, Changed);
		if (Item->LocationId == GolemGear::EquipmentLocation)
		{
			Golem->WeaponRuneInstanceId = Changed.RuneAttachmentId;
			Golem->Build.WeaponRune = Changed.RuneAttachmentId.IsValid() ? FName(TEXT("Weapon.Aftershock")) : NAME_None;
		}
		OutAffected = Item->InstanceId;
	}
	else if (Command.Operation == EGolemRPGOperation::UpgradeItem)
	{
		if (Definition->GetMetadata().UpgradeTrack != TEXT("RareOne") || Item->UpgradeRank != 0
			|| (Item->LocationId != GolemGear::InventoryLocation && Item->AssignedGolemId != Golem->GolemId)
			|| Candidate.Currency < 20)
		{
			return FGolemResult::Failure(EGolemError::InsufficientResources, TEXT("Upgrade"), LOCTEXT("Upgrade", "A real rank-zero Rare item and the complete fixed upgrade cost are required."));
		}
		const int32 Counts[] { 4, 2, 1 };
		for (int32 Index = 0; Index < 3; ++Index)
		{
			Result = RemoveItems(FGolemGearCatalog::DefinitionId(FName(*FString::Printf(TEXT("M%02d"), Index + 1))), Counts[Index]);
			if (!Result.IsSuccess())
			{
				return Result;
			}
		}
		Candidate.Currency -= 20;
		FGolemItemInstance Changed = *Item;
		Changed.UpgradeRank = 1;
		SetValue(InventoryView, Request, Changed);
		OutAffected = Item->InstanceId;
	}
	else if (Command.Operation == EGolemRPGOperation::BuyItem)
	{
		const UGolemDefinition* Resolved = nullptr;
		Result = ResolveDefinition(Command.ItemDefinitionId, UGolemItemDefinition::StaticClass(), Resolved);
		if (!Result.IsSuccess())
		{
			return Result;
		}
		const UGolemItemDefinition* Stock = CastChecked<UGolemItemDefinition>(Resolved);
		const int64 Cost = static_cast<int64>(Stock->GetMetadata().BuyPrice) * Command.Quantity;
		if (Stock->GetMetadata().BuyPrice <= 0 || Candidate.Currency < Cost)
		{
			return FGolemResult::Failure(EGolemError::InsufficientResources, TEXT("VendorBuy"), LOCTEXT("Buy", "The item must be actual vendor stock and its entire purchase price available."));
		}
		Result = AddItems(Command.ItemDefinitionId, Command.Quantity, FGuid());
		Candidate.Currency -= Cost;
		OutAffected = Request.Mutations.IsEmpty() ? FGuid() : Request.Mutations[0].Item.InstanceId;
	}
	else if (Command.Operation == EGolemRPGOperation::SellItem)
	{
		const FGolemItemMetadata& Metadata = Definition->GetMetadata();
		if (Item->LocationId != GolemGear::InventoryLocation || Item->RuneAttachmentId.IsValid()
			|| Metadata.SellPrice <= 0 || Command.Quantity > Item->Count
			|| Metadata.Rarity == EGolemItemRarity::Epic && !Command.bConfirmEpicSale)
		{
			return FGolemResult::Failure(EGolemError::InvalidState, TEXT("VendorSell"), LOCTEXT("Sell", "Sell only unassigned unsocketed sellable bag quantities, with explicit confirmation for Epic equipment."));
		}
		FGolemInventoryMutation Removal;
		Removal.Operation = EGolemInventoryOperation::Remove;
		Removal.Item = *Item;
		Removal.Item.Count = Command.Quantity;
		Removal.ExpectedItemRevision = Item->ItemRevision;
		Request.Mutations.Add(Removal);
		Candidate.Currency += static_cast<int64>(Metadata.SellPrice) * Command.Quantity;
		OutAffected = Item->InstanceId;
	}
	else if (Command.Operation == EGolemRPGOperation::ClaimReward)
	{
		const FGolemGearRewardBundle* Reward = RewardOffers.FindByPredicate([&Command](const FGolemGearRewardBundle& Entry)
		{
			return Entry.RewardId == Command.RewardId;
		});
		if (Reward == nullptr || Reward->DemoRunId != Candidate.DemoRunId || Candidate.ClaimedRewards.Contains(Command.RewardId))
		{
			return FGolemResult::Failure(EGolemError::Conflict, TEXT("RewardClaim"), LOCTEXT("Claim", "This authoritative reward must belong to the current run and remain unclaimed."));
		}
		for (const FGolemGearRewardItem& RewardItem : Reward->Items)
		{
			Result = AddItems(RewardItem.DefinitionId, RewardItem.Count, Reward->RewardId);
			if (!Result.IsSuccess())
			{
				return Result;
			}
		}
		for (const FGuid Id : Reward->BoundItems)
		{
			if (!InventoryView.Items.ContainsByPredicate([Id](const FGolemItemInstance& Item)
			{
				return Item.InstanceId == Id;
			}))
			{
				return FGolemResult::Failure(EGolemError::NotFound, TEXT("BoundRewardHammer"), FText::GetEmpty());
			}
		}
		Candidate.Currency += Reward->Currency;
		Candidate.ClaimedRewards.Add(Reward->RewardId);
		for (FGolemRPGGolemView& Entry : Candidate.Golems)
		{
			if (Entry.Level < Reward->Level)
			{
				Entry.Level = Reward->Level;
				if (Entry.GolemId != Golem->GolemId)
				{
					if (Entry.Revision >= MAX_int64)
					{
						return FGolemResult::Failure(EGolemError::CapacityExceeded, TEXT("RewardGolemRevision"),
							LOCTEXT("RewardGolemRevision", "A party reward cannot overflow a canonical Golem revision."));
					}
					++Entry.Revision;
				}
			}
		}
		OutAffected = Reward->RewardId;
	}
	else if (Command.Operation == EGolemRPGOperation::DeliverQuestItem)
	{
		const FGolemItemInstance* DeliveryItem = InventoryView.Items.FindByPredicate([&Command](const FGolemItemInstance& Row)
		{
			return Row.InstanceId == Command.ItemInstanceId;
		});
		const UGolemDefinition* DeliveryDefinition = nullptr;
		if (DeliveryItem == nullptr || DeliveryItem->DefinitionId != Command.ItemDefinitionId
			|| DeliveryItem->ItemRevision != Command.ExpectedItemRevision
			|| DeliveryItem->LocationId != GolemGear::InventoryLocation || DeliveryItem->Count < Command.Quantity
			|| !ResolveDefinition(DeliveryItem->DefinitionId,
				UGolemItemDefinition::StaticClass(), DeliveryDefinition).IsSuccess())
		{
			return FGolemResult::Failure(EGolemError::InvalidData, TEXT("QuestDeliveryItem"), FText::GetEmpty());
		}
		const EGolemItemCategory Category = CastChecked<UGolemItemDefinition>(DeliveryDefinition)->GetMetadata().Category;
		if (Category != EGolemItemCategory::Material && Category != EGolemItemCategory::QuestItem)
		{
			return FGolemResult::Failure(EGolemError::TypeMismatch, TEXT("QuestDeliveryCategory"), FText::GetEmpty());
		}
		FGolemInventoryMutation& Removal = Request.Mutations.AddDefaulted_GetRef();
		Removal.Operation = EGolemInventoryOperation::Remove;
		Removal.Item = *DeliveryItem;
		Removal.Item.Count = Command.Quantity;
		Removal.ExpectedItemRevision = DeliveryItem->ItemRevision;
		OutAffected = DeliveryItem->InstanceId;
	}
	else if (Command.Operation == EGolemRPGOperation::BindConsumable || Command.Operation == EGolemRPGOperation::UseConsumable)
	{
		const FPrimaryAssetId Id = Command.Operation == EGolemRPGOperation::BindConsumable
			? Command.ItemDefinitionId : Golem->ConsumableBindings[Command.QuickSlot];
		const UGolemDefinition* Resolved = nullptr;
		Result = ResolveDefinition(Id, UGolemItemDefinition::StaticClass(), Resolved);
		if (!Result.IsSuccess())
		{
			return Result;
		}
		if (CastChecked<UGolemItemDefinition>(Resolved)->GetMetadata().Category != EGolemItemCategory::Consumable
			|| Command.ItemDefinitionId.IsValid() && Command.ItemDefinitionId != Id)
		{
			return FGolemResult::Failure(EGolemError::TypeMismatch, TEXT("ConsumableBinding"), LOCTEXT("Binding", "Quick-use references a server-confirmed consumable definition, never a client count or arbitrary stack."));
		}
		if (Command.Operation == EGolemRPGOperation::BindConsumable)
		{
			Golem->ConsumableBindings[Command.QuickSlot] = Id;
		}
		else
		{
			TArray<FGolemItemInstance> Sorted = InventoryView.Items;
			Sorted.Sort([](const FGolemItemInstance& Left, const FGolemItemInstance& Right)
			{
				return Left.InstanceId < Right.InstanceId;
			});
			const FGolemItemInstance* Stack = Sorted.FindByPredicate([&Id](const FGolemItemInstance& Entry)
			{
				return Entry.DefinitionId == Id && Entry.LocationId == GolemGear::InventoryLocation;
			});
			if (Stack == nullptr)
			{
				return FGolemResult::Failure(EGolemError::InsufficientResources, TEXT("ConsumableStack"), LOCTEXT("Empty", "No stack of this bound consumable remains in the shared inventory."));
			}
			FGolemInventoryMutation Removal;
			Removal.Operation = EGolemInventoryOperation::Remove;
			Removal.Item = *Stack;
			Removal.Item.Count = 1;
			Removal.ExpectedItemRevision = Stack->ItemRevision;
			Request.Mutations.Add(Removal);
			OutAffected = Stack->InstanceId;
		}
	}
	else if (Command.Operation == EGolemRPGOperation::ApplyGearPreset)
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
		Golem->Assignments.Reset();
		Golem->WeaponRuneInstanceId.Invalidate();
		Golem->Build.WeaponRune = NAME_None;
		TArray<FGolemItemInstance> Sorted = InventoryView.Items;
		Sorted.Sort([](const FGolemItemInstance& Left, const FGolemItemInstance& Right)
		{
			return Left.InstanceId < Right.InstanceId;
		});
		for (int32 Index = 0; Index < Ids.Num(); ++Index)
		{
			const FPrimaryAssetId Id = FGolemGearCatalog::DefinitionId(Ids[Index]);
			const FGolemItemInstance* Selected = Sorted.FindByPredicate([Golem, &Id](const FGolemItemInstance& Entry)
			{
				return Entry.DefinitionId == Id && Entry.UpgradeRank == 0 && (Entry.LocationId == GolemGear::InventoryLocation
					|| Entry.LocationId == GolemGear::EquipmentLocation && Entry.AssignedGolemId == Golem->GolemId);
			});
			if (Selected == nullptr)
			{
				return FGolemResult::Failure(EGolemError::NotFound, TEXT("GearPreset"), LOCTEXT("Preset", "The explicit demo preset requires all twelve real available rank-zero equipment instances."));
			}
			Result = Assign(*Golem, *Selected, GolemGear::EquipmentSlots()[Index]);
			if (!Result.IsSuccess())
			{
				return Result;
			}
		}
	}
	else
	{
		return FGolemResult::Failure(EGolemError::InvalidData, TEXT("GearOperation"), LOCTEXT("Operation", "The gear operation is unsupported."));
	}
	if (!Result.IsSuccess())
	{
		return Result;
	}
	if (Candidate.Currency < 0 || Candidate.Currency > GolemGear::MaximumCurrency)
	{
		return FGolemResult::Failure(EGolemError::CapacityExceeded, TEXT("Currency"), LOCTEXT("Currency", "The shared currency scalar cannot exceed its supported balance."));
	}
	return FGolemResult::Success();
}

FGolemResult UGolemRPGComponent::SynchronizeLocations(const FGolemRPGView& Candidate,
	const FGolemInventoryView& InventoryView, FGolemInventoryRequest& Request) const
{
	TArray<FGolemItemInstance> Values = GolemRPGGear::ApplyValues(InventoryView, Request);
	const TArray<FGolemItemInstance> PendingValues = Values;
	TMap<FGuid, FGolemEquipmentAssignment> Equipment;
	TMap<FGuid, FGolemEntityId> Owners;
	TMap<FGuid, FGolemEntityId> SkillOwners;
	for (const FGolemRPGGolemView& Golem : Candidate.Golems)
	{
		for (const FGolemEquipmentAssignment& Assignment : Golem.Assignments)
		{
			if (Equipment.Contains(Assignment.ItemInstanceId))
			{
				return FGolemResult::Failure(EGolemError::Conflict, TEXT("GearLocation"),
					LOCTEXT("DuplicateLocation", "An exact equipment instance has one canonical owner."));
			}
			Equipment.Add(Assignment.ItemInstanceId, Assignment);
			Owners.Add(Assignment.ItemInstanceId, Golem.GolemId);
		}
		const FGolemEquipmentAssignment* Weapon = Golem.FindAssignment(GolemGameplayTags::Slot_Equipment_Weapon);
		if (Weapon != nullptr)
		{
			FGolemItemInstance* Parent = Values.FindByPredicate([Weapon](const FGolemItemInstance& Item)
			{
				return Item.InstanceId == Weapon->ItemInstanceId;
			});
			if (Parent != nullptr)
			{
				Parent->RuneAttachmentId = Golem.WeaponRuneInstanceId;
			}
		}
		if (Golem.SkillRuneInstanceId.IsValid())
		{
			if (SkillOwners.Contains(Golem.SkillRuneInstanceId))
			{
				return FGolemResult::Failure(EGolemError::Conflict, TEXT("SkillSocket"),
					LOCTEXT("SkillSocketOwner", "Each skill rune has one canonical socket owner."));
			}
			SkillOwners.Add(Golem.SkillRuneInstanceId, Golem.GolemId);
		}
	}
	TMap<FGuid, FGuid> RuneParents;
	for (const FGolemItemInstance& Parent : Values)
	{
		if (Parent.RuneAttachmentId.IsValid())
		{
			if (RuneParents.Contains(Parent.RuneAttachmentId) || SkillOwners.Contains(Parent.RuneAttachmentId))
			{
				return FGolemResult::Failure(EGolemError::Conflict, TEXT("WeaponSocket"),
					LOCTEXT("WeaponSocketOwner", "A rune cannot belong to multiple weapon or skill sockets."));
			}
			RuneParents.Add(Parent.RuneAttachmentId, Parent.InstanceId);
		}
	}
	for (FGolemItemInstance& Item : Values)
	{
		Item.LocationId = GolemGear::InventoryLocation;
		Item.AssignedGolemId = FGolemEntityId();
		Item.SlotTag = FGameplayTag();
		Item.ParentWeaponId.Invalidate();
		if (const FGolemEquipmentAssignment* Assignment = Equipment.Find(Item.InstanceId))
		{
			Item.LocationId = GolemGear::EquipmentLocation;
			Item.AssignedGolemId = Owners.FindChecked(Item.InstanceId);
			Item.SlotTag = Assignment->SlotTag;
		}
		else if (const FGuid* Parent = RuneParents.Find(Item.InstanceId))
		{
			Item.LocationId = GolemGear::WeaponSocketLocation;
			Item.ParentWeaponId = *Parent;
			if (const FGolemEntityId* Owner = Owners.Find(*Parent))
			{
				Item.AssignedGolemId = *Owner;
			}
		}
		else if (const FGolemEntityId* Owner = SkillOwners.Find(Item.InstanceId))
		{
			Item.LocationId = GolemGear::SkillSocketLocation;
			Item.AssignedGolemId = *Owner;
		}
		const FGolemItemInstance* Pending = PendingValues.FindByPredicate([&Item](const FGolemItemInstance& Entry)
		{
			return Entry.InstanceId == Item.InstanceId;
		});
		if (Pending != nullptr && !(Item == *Pending))
		{
			GolemRPGGear::SetValue(InventoryView, Request, Item);
		}
	}
	return ValidateGearLocations(Candidate, Values);
}

FGolemResult UGolemRPGComponent::ValidateGearLocations(const FGolemRPGView& View,
	const TArray<FGolemItemInstance>& Items) const
{
	if (!View.IsItemizationEnabled())
	{
		return FGolemResult::Success();
	}
	TSet<FGuid> Located;
	for (const FGolemRPGGolemView& Golem : View.Golems)
	{
		for (const FGolemEquipmentAssignment& Assignment : Golem.Assignments)
		{
			const FGolemItemInstance* Item = Items.FindByPredicate([&Assignment](const FGolemItemInstance& Entry)
			{
				return Entry.InstanceId == Assignment.ItemInstanceId && Entry.DefinitionId == Assignment.DefinitionId;
			});
			if (!GolemGear::IsEquipmentSlot(Assignment.SlotTag) || Item == nullptr || Located.Contains(Item->InstanceId)
				|| Item->LocationId != GolemGear::EquipmentLocation || Item->AssignedGolemId != Golem.GolemId
				|| Item->SlotTag != Assignment.SlotTag)
			{
				return FGolemResult::Failure(EGolemError::Conflict, TEXT("EquipmentLocation"),
					LOCTEXT("EquipmentLocation", "Equipment assignments and exact instance locations must agree."));
			}
			Located.Add(Item->InstanceId);
		}
		const FGolemEquipmentAssignment* Weapon = Golem.FindAssignment(GolemGameplayTags::Slot_Equipment_Weapon);
		const FGolemItemInstance* Parent = Weapon == nullptr ? nullptr : Items.FindByPredicate(
			[Weapon](const FGolemItemInstance& Item) { return Item.InstanceId == Weapon->ItemInstanceId; });
		if ((Parent == nullptr && Golem.WeaponRuneInstanceId.IsValid())
			|| (Parent != nullptr && Parent->RuneAttachmentId != Golem.WeaponRuneInstanceId))
		{
			return FGolemResult::Failure(EGolemError::Conflict, TEXT("ActiveWeaponRune"),
				LOCTEXT("ActiveWeaponRune", "The active weapon rune must belong to the equipped exact parent hammer."));
		}
		if (Golem.SkillRuneInstanceId.IsValid())
		{
			const FGolemItemInstance* Rune = Items.FindByPredicate([&Golem](const FGolemItemInstance& Item)
			{
				return Item.InstanceId == Golem.SkillRuneInstanceId;
			});
			if (Rune == nullptr || Located.Contains(Rune->InstanceId) || Rune->LocationId != GolemGear::SkillSocketLocation
				|| Rune->AssignedGolemId != Golem.GolemId)
			{
				return FGolemResult::Failure(EGolemError::Conflict, TEXT("SkillRuneLocation"),
					LOCTEXT("SkillRuneLocation", "Skill runes retain exactly one Golem socket location."));
			}
			Located.Add(Rune->InstanceId);
		}
	}
	for (const FGolemItemInstance& Parent : Items)
	{
		if (!Parent.RuneAttachmentId.IsValid())
		{
			continue;
		}
		const FGolemItemInstance* Rune = Items.FindByPredicate([&Parent](const FGolemItemInstance& Item)
		{
			return Item.InstanceId == Parent.RuneAttachmentId;
		});
		const UGolemDefinition* ParentDefinition = nullptr;
		const UGolemDefinition* RuneDefinition = nullptr;
		if (Rune == nullptr || Located.Contains(Parent.RuneAttachmentId)
			|| !ResolveDefinition(Parent.DefinitionId, UGolemItemDefinition::StaticClass(), ParentDefinition).IsSuccess()
			|| !ResolveDefinition(Rune->DefinitionId, UGolemRuneDefinition::StaticClass(), RuneDefinition).IsSuccess()
			|| CastChecked<UGolemItemDefinition>(ParentDefinition)->GetMetadata().Subtype != EGolemEquipmentSubtype::Hammer
			|| CastChecked<UGolemRuneDefinition>(RuneDefinition)->GetAttachmentRuneId() != TEXT("Weapon.Aftershock")
			|| Rune->LocationId != GolemGear::WeaponSocketLocation || Rune->ParentWeaponId != Parent.InstanceId
			|| Rune->AssignedGolemId != Parent.AssignedGolemId)
		{
			return FGolemResult::Failure(EGolemError::Conflict, TEXT("WeaponRuneParent"),
				LOCTEXT("WeaponRuneParent", "Every attached rune retains its exact compatible hammer parent and location."));
		}
		Located.Add(Rune->InstanceId);
	}
	for (const FGolemItemInstance& Item : Items)
	{
		if (!Item.ValidateIdentityAndCount().IsSuccess()
			|| (Item.LocationId != GolemGear::InventoryLocation && !Located.Contains(Item.InstanceId)))
		{
			return FGolemResult::Failure(EGolemError::Conflict, TEXT("ExclusiveLocation"),
				LOCTEXT("ExclusiveLocation", "Every owned instance has one valid bag, equipment or socket location."));
		}
	}
	return FGolemResult::Success();
}

FGolemResult UGolemRPGComponent::PreviewEquip(const FGolemEntityId& GolemId, const FGuid& InstanceId,
	FGameplayTag SlotTag, FGolemItemStatBonuses& OutBonuses) const
{
	OutBonuses = FGolemItemStatBonuses();
	if (!IsInitialized() || Inventory == nullptr || bTransactionInProgress || !PublishedView.IsItemizationEnabled())
	{
		return FGolemResult::Failure(EGolemError::InvalidState, TEXT("GearPreview"),
			LOCTEXT("PreviewState", "Compare requires an initialized confirmed gear inventory."));
	}
	const FGolemInventoryView InventoryView = Inventory->GetView();
	const FGolemItemInstance* Item = InventoryView.Items.FindByPredicate([&InstanceId](const FGolemItemInstance& Entry)
	{
		return Entry.InstanceId == InstanceId;
	});
	FGolemRPGCommand Command;
	Command.Operation = EGolemRPGOperation::Equip;
	Command.RequestId = FGuid::NewGuid();
	Command.InventoryId = InventoryView.InventoryId;
	Command.ExpectedInventoryRevision = InventoryView.Revision;
	Command.ExpectedGolemId = GolemId;
	Command.ExpectedGolemRevision = GetGolemView(GolemId).Revision;
	Command.ItemInstanceId = InstanceId;
	Command.ExpectedItemRevision = Item == nullptr ? 0 : Item->ItemRevision;
	Command.SlotTag = SlotTag;
	FGolemRPGView Candidate;
	FGolemInventoryRequest Request;
	FGuid Affected;
	FGolemResult Result = BuildCandidate(PublishedView, InventoryView, Command, nullptr, nullptr, Candidate, Request, Affected);
	if (!Result.IsSuccess())
	{
		return Result;
	}
	FGolemRPGGolemView* Golem = Candidate.Golems.FindByPredicate([&GolemId](const FGolemRPGGolemView& Entry)
	{
		return Entry.GolemId == GolemId;
	});
	FGolemGASProjectionPlan Plan;
	Result = BuildProjection(*Golem, GolemRPGGear::ApplyValues(InventoryView, Request), Plan);
	if (Result.IsSuccess())
	{
		OutBonuses = Golem->StatBonuses;
	}
	return Result;
}

FGolemResult UGolemRPGComponent::PreviewTransfer(const FGolemEntityId& SourceGolemId,
	const FGolemEntityId& TargetGolemId, const FGuid& InstanceId, FGameplayTag SlotTag,
	FGolemItemStatBonuses& OutSource, FGolemItemStatBonuses& OutTarget) const
{
	OutSource = FGolemItemStatBonuses();
	OutTarget = FGolemItemStatBonuses();
	if (!IsInitialized() || Inventory == nullptr || bTransactionInProgress || !PublishedView.IsItemizationEnabled()
		|| SourceGolemId == TargetGolemId)
	{
		return FGolemResult::Failure(EGolemError::InvalidState, TEXT("TransferPreview"),
			LOCTEXT("TransferPreviewState", "Compare a real transfer between two distinct confirmed Golems."));
	}
	const FGolemInventoryView InventoryView = Inventory->GetView();
	const FGolemItemInstance* Item = InventoryView.Items.FindByPredicate([&InstanceId](const FGolemItemInstance& Entry)
	{
		return Entry.InstanceId == InstanceId;
	});
	FGolemRPGCommand Command;
	Command.Operation = EGolemRPGOperation::TransferEquipment;
	Command.RequestId = FGuid::NewGuid();
	Command.InventoryId = InventoryView.InventoryId;
	Command.ExpectedInventoryRevision = InventoryView.Revision;
	Command.ExpectedGolemId = SourceGolemId;
	Command.ExpectedGolemRevision = GetGolemView(SourceGolemId).Revision;
	Command.TargetGolemId = TargetGolemId;
	Command.ExpectedTargetGolemRevision = GetGolemView(TargetGolemId).Revision;
	Command.ItemInstanceId = InstanceId;
	Command.ExpectedItemRevision = Item == nullptr ? 0 : Item->ItemRevision;
	Command.SlotTag = SlotTag;
	FGolemRPGView Candidate;
	FGolemInventoryRequest Request;
	FGuid Affected;
	FGolemResult Result = BuildCandidate(PublishedView, InventoryView, Command, nullptr, nullptr, Candidate, Request, Affected);
	if (!Result.IsSuccess())
	{
		return Result;
	}
	const TArray<FGolemItemInstance> Values = GolemRPGGear::ApplyValues(InventoryView, Request);
	for (FGolemRPGGolemView& Golem : Candidate.Golems)
	{
		if (Golem.GolemId != SourceGolemId && Golem.GolemId != TargetGolemId)
		{
			continue;
		}
		FGolemGASProjectionPlan Plan;
		Result = BuildProjection(Golem, Values, Plan);
		if (!Result.IsSuccess())
		{
			return Result;
		}
		(Golem.GolemId == SourceGolemId ? OutSource : OutTarget) = Golem.StatBonuses;
	}
	return FGolemResult::Success();
}

FGuid UGolemRPGComponent::GetWeaponRuneParentId(const FGuid& RuneId) const
{
	if (Inventory == nullptr || !RuneId.IsValid())
	{
		return FGuid();
	}
	const FGolemInventoryView View = Inventory->GetView();
	const FGolemItemInstance* Rune = View.Items.FindByPredicate([&RuneId](const FGolemItemInstance& Item)
	{
		return Item.InstanceId == RuneId;
	});
	if (Rune != nullptr && Rune->ParentWeaponId.IsValid())
	{
		return Rune->ParentWeaponId;
	}
	for (const FGolemRPGGolemView& Golem : PublishedView.Golems)
	{
		if (Golem.WeaponRuneInstanceId == RuneId)
		{
			const FGolemEquipmentAssignment* Weapon = Golem.FindAssignment(GolemGameplayTags::Slot_Equipment_Weapon);
			return Weapon == nullptr ? FGuid() : Weapon->ItemInstanceId;
		}
	}
	return FGuid();
}

#undef LOCTEXT_NAMESPACE
