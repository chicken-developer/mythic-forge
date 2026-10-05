#include "Commands/KnightRPGCommand.h"

#define LOCTEXT_NAMESPACE "KnightRPGCommand"

FKnightResult FKnightRPGCommand::ValidateShape() const
{
	if (!RequestId.IsValid() || !ExpectedKnightId.IsValid() || !InventoryId.IsValid())
	{
		return FKnightResult::Failure(EKnightError::InvalidId, TEXT("RPGCommand"),
			LOCTEXT("Identity", "An RPG command requires exact request, Knight and inventory identities."));
	}
	if (ExpectedInventoryRevision < 1 || ExpectedInventoryRevision >= MAX_int64
		|| ExpectedKnightRevision < 1 || ExpectedKnightRevision >= MAX_int64)
	{
		return FKnightResult::Failure(EKnightError::InvalidRevision, TEXT("RPGCommand"),
			LOCTEXT("Revision", "Expected RPG revisions must be finite positive values."));
	}
	bool bValidPayload = false;
	switch (Operation)
	{
	case EKnightRPGOperation::Pickup:
		bValidPayload = PickupId.IsValid() && !ItemInstanceId.IsValid() && !SlotTag.IsValid()
			&& SkillDefinitionId == FPrimaryAssetId();
		break;
	case EKnightRPGOperation::Equip:
	case EKnightRPGOperation::Rune:
	case EKnightRPGOperation::Unequip:
		bValidPayload = !PickupId.IsValid() && ItemInstanceId.IsValid() && SlotTag.IsValid()
			&& SkillDefinitionId == FPrimaryAssetId();
		break;
	case EKnightRPGOperation::PurchaseSkill:
		bValidPayload = !PickupId.IsValid() && !ItemInstanceId.IsValid() && !SlotTag.IsValid()
			&& KnightIdentity::IsValidDefinitionId(SkillDefinitionId)
			&& SkillDefinitionId.PrimaryAssetType == FPrimaryAssetType(TEXT("KnightSkill"));
		break;
	case EKnightRPGOperation::Respec:
		bValidPayload = !PickupId.IsValid() && !ItemInstanceId.IsValid() && !SlotTag.IsValid()
			&& SkillDefinitionId == FPrimaryAssetId();
		break;
	default:
		break;
	}
	return bValidPayload ? FKnightResult::Success()
		: FKnightResult::Failure(EKnightError::InvalidData, TEXT("RPGCommand"),
			LOCTEXT("Payload", "The command contains missing or unrelated payload fields for its operation."));
}

bool FKnightRPGCommand::operator==(const FKnightRPGCommand& Other) const
{
	return Operation == Other.Operation && RequestId == Other.RequestId && ExpectedKnightId == Other.ExpectedKnightId
		&& InventoryId == Other.InventoryId && ExpectedInventoryRevision == Other.ExpectedInventoryRevision
		&& ExpectedKnightRevision == Other.ExpectedKnightRevision && PickupId == Other.PickupId
		&& ItemInstanceId == Other.ItemInstanceId && SlotTag == Other.SlotTag && SkillDefinitionId == Other.SkillDefinitionId;
}

#undef LOCTEXT_NAMESPACE
