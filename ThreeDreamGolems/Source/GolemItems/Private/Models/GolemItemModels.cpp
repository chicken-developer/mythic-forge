#include "Models/GolemItemModels.h"

#include "Types/GolemStableIds.h"
#include "Models/GolemGearModels.h"

#define LOCTEXT_NAMESPACE "GolemItemModels"

FGolemResult FGolemItemStatBonuses::Validate() const
{
	const float Values[] { MaxHealth, MaxStamina, MaxRuneEnergy, AttackPower, Defense, Poise,
		CritChance, Haste, CooldownRecovery, MasteryBonus, Versatility };
	for (float Value : Values)
	{
		if (!FMath::IsFinite(Value) || Value < 0.0f || Value > GolemItems::MaximumStatBonus)
		{
			return FGolemResult::Failure(EGolemError::InvalidData, TEXT("ItemStatBonuses"),
				LOCTEXT("InvalidStatBonus", "Item stat bonuses must be finite and within the supported positive range."));
		}
	}
	return FGolemResult::Success();
}

void FGolemItemStatBonuses::Add(const FGolemItemStatBonuses& Other)
{
	MaxHealth += Other.MaxHealth;
	MaxStamina += Other.MaxStamina;
	MaxRuneEnergy += Other.MaxRuneEnergy;
	AttackPower += Other.AttackPower;
	Defense += Other.Defense;
	Poise += Other.Poise;
	CritChance += Other.CritChance;
	Haste += Other.Haste;
	CooldownRecovery += Other.CooldownRecovery;
	MasteryBonus += Other.MasteryBonus;
	Versatility += Other.Versatility;
}

bool FGolemItemStatBonuses::IsEmpty() const
{
	return MaxHealth == 0.0f && MaxStamina == 0.0f && MaxRuneEnergy == 0.0f
		&& AttackPower == 0.0f && Defense == 0.0f && Poise == 0.0f && CritChance == 0.0f && Haste == 0.0f
		&& CooldownRecovery == 0.0f && MasteryBonus == 0.0f && Versatility == 0.0f;
}

bool FGolemItemStatBonuses::operator==(const FGolemItemStatBonuses& Other) const
{
	return MaxHealth == Other.MaxHealth && MaxStamina == Other.MaxStamina
		&& MaxRuneEnergy == Other.MaxRuneEnergy && AttackPower == Other.AttackPower && Defense == Other.Defense
		&& Poise == Other.Poise && CritChance == Other.CritChance && Haste == Other.Haste
		&& CooldownRecovery == Other.CooldownRecovery && MasteryBonus == Other.MasteryBonus && Versatility == Other.Versatility;
}

FGolemResult FGolemItemInstance::ValidateIdentityAndCount() const
{
	if (!InstanceId.IsValid() || !GolemIdentity::IsValidDefinitionId(DefinitionId))
	{
		return FGolemResult::Failure(EGolemError::InvalidId, TEXT("ItemInstance"),
			LOCTEXT("InvalidItemIdentity", "An item instance requires a persistent GUID and a valid stable definition ID."));
	}
	if (DefinitionId.PrimaryAssetType != FPrimaryAssetType(TEXT("GolemItem")))
	{
		return FGolemResult::Failure(EGolemError::TypeMismatch, FName(*DefinitionId.ToString()),
			LOCTEXT("WrongItemType", "An item instance must reference a GolemItem definition."));
	}
	if (Count <= 0 || Count > GolemItems::MaximumStackCount || ItemRevision < 1 || ItemRevision >= MAX_int64
		|| UpgradeRank < 0 || UpgradeRank > 1)
	{
		return FGolemResult::Failure(EGolemError::InvalidData, TEXT("ItemInstance"),
			LOCTEXT("InvalidItemCount", "An item count must be positive and within the supported stack range."));
	}
	const bool bBag = LocationId == GolemGear::InventoryLocation;
	const bool bEquipment = LocationId == GolemGear::EquipmentLocation;
	const bool bWeaponSocket = LocationId == GolemGear::WeaponSocketLocation;
	const bool bSkillSocket = LocationId == GolemGear::SkillSocketLocation;
	if ((!bBag && !bEquipment && !bWeaponSocket && !bSkillSocket)
		|| (bBag && (AssignedGolemId.IsValid() || SlotTag.IsValid() || ParentWeaponId.IsValid()))
		|| (bEquipment && (!AssignedGolemId.IsValid() || !SlotTag.IsValid() || ParentWeaponId.IsValid() || Count != 1))
		|| (bWeaponSocket && (!ParentWeaponId.IsValid() || Count != 1 || RuneAttachmentId.IsValid()))
		|| (bSkillSocket && (!AssignedGolemId.IsValid() || ParentWeaponId.IsValid() || SlotTag.IsValid()
			|| Count != 1 || RuneAttachmentId.IsValid())))
	{
		return FGolemResult::Failure(EGolemError::InvalidData, TEXT("ItemLocation"),
			LOCTEXT("Location", "An item has exactly one valid bag, equipment or attachment location."));
	}
	return FGolemResult::Success();
}

bool FGolemItemInstance::operator==(const FGolemItemInstance& Other) const
{
	return InstanceId == Other.InstanceId && DefinitionId == Other.DefinitionId && Count == Other.Count
		&& ItemRevision == Other.ItemRevision && LocationId == Other.LocationId && AssignedGolemId == Other.AssignedGolemId
		&& SlotTag == Other.SlotTag && UpgradeRank == Other.UpgradeRank && RuneAttachmentId == Other.RuneAttachmentId
		&& ParentWeaponId == Other.ParentWeaponId && RewardReceiptId == Other.RewardReceiptId;
}

bool FGolemInventoryMutation::operator==(const FGolemInventoryMutation& Other) const
{
	return Operation == Other.Operation && Item == Other.Item && ExpectedItemRevision == Other.ExpectedItemRevision;
}

FGolemResult FGolemInventoryRequest::ValidateShape() const
{
	if (!RequestId.IsValid())
	{
		return FGolemResult::Failure(EGolemError::InvalidId, TEXT("InventoryRequest"),
			LOCTEXT("InvalidRequestId", "An inventory transaction requires a valid request GUID."));
	}
	if (ExpectedRevision < 1 || ExpectedRevision >= MAX_int64)
	{
		return FGolemResult::Failure(EGolemError::InvalidRevision, TEXT("InventoryRequest"),
			LOCTEXT("InvalidExpectedRevision", "The expected inventory revision is outside the supported range."));
	}
	if (Mutations.IsEmpty() || Mutations.Num() > GolemItems::MaximumBatchOperations)
	{
		return FGolemResult::Failure(EGolemError::InvalidData, TEXT("InventoryRequest"),
			LOCTEXT("InvalidBatchSize", "An inventory transaction requires a bounded, nonempty mutation batch."));
	}
	TSet<FGuid> InstanceIds;
	for (const FGolemInventoryMutation& Mutation : Mutations)
	{
		if (Mutation.Operation != EGolemInventoryOperation::Add
			&& Mutation.Operation != EGolemInventoryOperation::Remove && Mutation.Operation != EGolemInventoryOperation::Update)
		{
			return FGolemResult::Failure(EGolemError::InvalidData, TEXT("InventoryRequest"),
				LOCTEXT("InvalidOperation", "The inventory transaction contains an unsupported operation."));
		}
		const FGolemResult ItemResult = Mutation.Item.ValidateIdentityAndCount();
		if (!ItemResult.IsSuccess())
		{
			return ItemResult;
		}
		if (Mutation.ExpectedItemRevision < 0 || Mutation.ExpectedItemRevision >= MAX_int64
			|| (Mutation.Operation == EGolemInventoryOperation::Update && (Mutation.ExpectedItemRevision < 1
				|| Mutation.Item.ItemRevision != Mutation.ExpectedItemRevision + 1)))
		{
			return FGolemResult::Failure(EGolemError::InvalidRevision, TEXT("ItemRevision"),
				LOCTEXT("MutationRevision", "An item update requires its exact old revision and one new revision."));
		}
		if (InstanceIds.Contains(Mutation.Item.InstanceId))
		{
			return FGolemResult::Failure(EGolemError::DuplicateId, TEXT("InventoryRequest"),
				LOCTEXT("DuplicateBatchInstance", "Each instance may appear only once in an atomic mutation batch."));
		}
		InstanceIds.Add(Mutation.Item.InstanceId);
	}
	return FGolemResult::Success();
}

bool FGolemInventoryRequest::operator==(const FGolemInventoryRequest& Other) const
{
	return RequestId == Other.RequestId && ExpectedRevision == Other.ExpectedRevision && Mutations == Other.Mutations;
}

bool FGolemInventoryReceipt::operator==(const FGolemInventoryReceipt& Other) const
{
	return Request == Other.Request && CommittedRevision == Other.CommittedRevision;
}

bool FGolemInventoryView::IsInitialized() const
{
	return InventoryId.IsValid() && Revision >= 1 && Capacity >= 1
		&& Capacity <= GolemItems::MaximumInventoryCapacity;
}

int32 FGolemInventoryView::GetBagCount() const
{
	int32 Count = 0;
	for (const FGolemItemInstance& Item : Items)
	{
		Count += Item.LocationId == GolemGear::InventoryLocation ? 1 : 0;
	}
	return Count;
}

bool FGolemInventoryView::operator==(const FGolemInventoryView& Other) const
{
	return InventoryId == Other.InventoryId && Capacity == Other.Capacity
		&& Revision == Other.Revision && Items == Other.Items;
}

bool FGolemInventorySnapshot::operator==(const FGolemInventorySnapshot& Other) const
{
	return SchemaVersion == Other.SchemaVersion && InventoryId == Other.InventoryId
		&& Capacity == Other.Capacity && Revision == Other.Revision && Items == Other.Items && Receipts == Other.Receipts;
}

#undef LOCTEXT_NAMESPACE
