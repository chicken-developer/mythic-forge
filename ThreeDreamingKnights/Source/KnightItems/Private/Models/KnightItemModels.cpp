#include "Models/KnightItemModels.h"

#include "Types/KnightStableIds.h"

#define LOCTEXT_NAMESPACE "KnightItemModels"

FKnightResult FKnightItemStatBonuses::Validate() const
{
	const float Values[] { MaxHealth, MaxStamina, MaxRuneEnergy, AttackPower, Defense };
	for (float Value : Values)
	{
		if (!FMath::IsFinite(Value) || Value < 0.0f || Value > KnightItems::MaximumStatBonus)
		{
			return FKnightResult::Failure(EKnightError::InvalidData, TEXT("ItemStatBonuses"),
				LOCTEXT("InvalidStatBonus", "Item stat bonuses must be finite and within the supported positive range."));
		}
	}
	return FKnightResult::Success();
}

bool FKnightItemStatBonuses::IsEmpty() const
{
	return MaxHealth == 0.0f && MaxStamina == 0.0f && MaxRuneEnergy == 0.0f
		&& AttackPower == 0.0f && Defense == 0.0f;
}

bool FKnightItemStatBonuses::operator==(const FKnightItemStatBonuses& Other) const
{
	return MaxHealth == Other.MaxHealth && MaxStamina == Other.MaxStamina
		&& MaxRuneEnergy == Other.MaxRuneEnergy && AttackPower == Other.AttackPower && Defense == Other.Defense;
}

FKnightResult FKnightItemInstance::ValidateIdentityAndCount() const
{
	if (!InstanceId.IsValid() || !KnightIdentity::IsValidDefinitionId(DefinitionId))
	{
		return FKnightResult::Failure(EKnightError::InvalidId, TEXT("ItemInstance"),
			LOCTEXT("InvalidItemIdentity", "An item instance requires a persistent GUID and a valid stable definition ID."));
	}
	if (DefinitionId.PrimaryAssetType != FPrimaryAssetType(TEXT("KnightItem")))
	{
		return FKnightResult::Failure(EKnightError::TypeMismatch, FName(*DefinitionId.ToString()),
			LOCTEXT("WrongItemType", "An item instance must reference a KnightItem definition."));
	}
	if (Count <= 0 || Count > KnightItems::MaximumStackCount)
	{
		return FKnightResult::Failure(EKnightError::InvalidData, TEXT("ItemInstance"),
			LOCTEXT("InvalidItemCount", "An item count must be positive and within the supported stack range."));
	}
	return FKnightResult::Success();
}

bool FKnightItemInstance::operator==(const FKnightItemInstance& Other) const
{
	return InstanceId == Other.InstanceId && DefinitionId == Other.DefinitionId && Count == Other.Count;
}

bool FKnightInventoryMutation::operator==(const FKnightInventoryMutation& Other) const
{
	return Operation == Other.Operation && Item == Other.Item;
}

FKnightResult FKnightInventoryRequest::ValidateShape() const
{
	if (!RequestId.IsValid())
	{
		return FKnightResult::Failure(EKnightError::InvalidId, TEXT("InventoryRequest"),
			LOCTEXT("InvalidRequestId", "An inventory transaction requires a valid request GUID."));
	}
	if (ExpectedRevision < 1 || ExpectedRevision >= MAX_int64)
	{
		return FKnightResult::Failure(EKnightError::InvalidRevision, TEXT("InventoryRequest"),
			LOCTEXT("InvalidExpectedRevision", "The expected inventory revision is outside the supported range."));
	}
	if (Mutations.IsEmpty() || Mutations.Num() > KnightItems::MaximumBatchOperations)
	{
		return FKnightResult::Failure(EKnightError::InvalidData, TEXT("InventoryRequest"),
			LOCTEXT("InvalidBatchSize", "An inventory transaction requires a bounded, nonempty mutation batch."));
	}
	TSet<FGuid> InstanceIds;
	for (const FKnightInventoryMutation& Mutation : Mutations)
	{
		if (Mutation.Operation != EKnightInventoryOperation::Add
			&& Mutation.Operation != EKnightInventoryOperation::Remove)
		{
			return FKnightResult::Failure(EKnightError::InvalidData, TEXT("InventoryRequest"),
				LOCTEXT("InvalidOperation", "The inventory transaction contains an unsupported operation."));
		}
		const FKnightResult ItemResult = Mutation.Item.ValidateIdentityAndCount();
		if (!ItemResult.IsSuccess())
		{
			return ItemResult;
		}
		if (InstanceIds.Contains(Mutation.Item.InstanceId))
		{
			return FKnightResult::Failure(EKnightError::DuplicateId, TEXT("InventoryRequest"),
				LOCTEXT("DuplicateBatchInstance", "Each instance may appear only once in an atomic mutation batch."));
		}
		InstanceIds.Add(Mutation.Item.InstanceId);
	}
	return FKnightResult::Success();
}

bool FKnightInventoryRequest::operator==(const FKnightInventoryRequest& Other) const
{
	return RequestId == Other.RequestId && ExpectedRevision == Other.ExpectedRevision && Mutations == Other.Mutations;
}

bool FKnightInventoryReceipt::operator==(const FKnightInventoryReceipt& Other) const
{
	return Request == Other.Request && CommittedRevision == Other.CommittedRevision;
}

bool FKnightInventoryView::IsInitialized() const
{
	return InventoryId.IsValid() && Revision >= 1 && Capacity >= 1
		&& Capacity <= KnightItems::MaximumInventoryCapacity;
}

bool FKnightInventoryView::operator==(const FKnightInventoryView& Other) const
{
	return InventoryId == Other.InventoryId && Capacity == Other.Capacity
		&& Revision == Other.Revision && Items == Other.Items;
}

bool FKnightInventorySnapshot::operator==(const FKnightInventorySnapshot& Other) const
{
	return SchemaVersion == Other.SchemaVersion && InventoryId == Other.InventoryId
		&& Capacity == Other.Capacity && Revision == Other.Revision && Items == Other.Items && Receipts == Other.Receipts;
}

#undef LOCTEXT_NAMESPACE
