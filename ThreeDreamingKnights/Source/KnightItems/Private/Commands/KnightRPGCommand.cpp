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
	if (Operation >= EKnightRPGOperation::TransferEquipment)
	{
		FKnightRPGCommand Allowed;
		Allowed.Operation = Operation;
		Allowed.RequestId = RequestId;
		Allowed.ExpectedKnightId = ExpectedKnightId;
		Allowed.InventoryId = InventoryId;
		Allowed.ExpectedInventoryRevision = ExpectedInventoryRevision;
		Allowed.ExpectedKnightRevision = ExpectedKnightRevision;
		switch (Operation)
		{
		case EKnightRPGOperation::TransferEquipment:
			Allowed.ItemInstanceId = ItemInstanceId;
			Allowed.ExpectedItemRevision = ExpectedItemRevision;
			Allowed.SlotTag = SlotTag;
			Allowed.TargetKnightId = TargetKnightId;
			Allowed.ExpectedTargetKnightRevision = ExpectedTargetKnightRevision;
			bValidPayload = ItemInstanceId.IsValid() && ExpectedItemRevision > 0 && SlotTag.IsValid()
				&& TargetKnightId.IsValid() && TargetKnightId != ExpectedKnightId && ExpectedTargetKnightRevision > 0;
			break;
		case EKnightRPGOperation::SwapRings:
			bValidPayload = true;
			break;
		case EKnightRPGOperation::AttachWeaponRune:
		case EKnightRPGOperation::DetachWeaponRune:
			Allowed.ItemInstanceId = ItemInstanceId;
			Allowed.ExpectedItemRevision = ExpectedItemRevision;
			Allowed.WeaponRuneInstanceId = WeaponRuneInstanceId;
			bValidPayload = ItemInstanceId.IsValid() && ExpectedItemRevision > 0
				&& (WeaponRuneInstanceId.IsValid() == (Operation == EKnightRPGOperation::AttachWeaponRune));
			break;
		case EKnightRPGOperation::UseConsumable:
		case EKnightRPGOperation::BindConsumable:
			Allowed.ItemDefinitionId = ItemDefinitionId;
			Allowed.QuickSlot = QuickSlot;
			bValidPayload = QuickSlot >= 0 && QuickSlot < 2
				&& (Operation == EKnightRPGOperation::UseConsumable && !ItemDefinitionId.IsValid()
					|| KnightIdentity::IsValidDefinitionId(ItemDefinitionId));
			break;
		case EKnightRPGOperation::BuyItem:
			Allowed.ItemDefinitionId = ItemDefinitionId;
			Allowed.Quantity = Quantity;
			bValidPayload = KnightIdentity::IsValidDefinitionId(ItemDefinitionId) && Quantity > 0 && Quantity <= 20;
			break;
		case EKnightRPGOperation::SellItem:
			Allowed.Quantity = Quantity;
			Allowed.bConfirmEpicSale = bConfirmEpicSale;
			[[fallthrough]];
		case EKnightRPGOperation::UpgradeItem:
			Allowed.ItemInstanceId = ItemInstanceId;
			Allowed.ExpectedItemRevision = ExpectedItemRevision;
			bValidPayload = ItemInstanceId.IsValid() && ExpectedItemRevision > 0 && Quantity > 0
				&& Quantity <= 10000;
			break;
		case EKnightRPGOperation::DeliverQuestItem:
			Allowed.ItemInstanceId = ItemInstanceId;
			Allowed.ExpectedItemRevision = ExpectedItemRevision;
			Allowed.ItemDefinitionId = ItemDefinitionId;
			Allowed.Quantity = Quantity;
			bValidPayload = ItemInstanceId.IsValid() && ExpectedItemRevision > 0
				&& KnightIdentity::IsValidDefinitionId(ItemDefinitionId) && Quantity > 0 && Quantity <= 10000;
			break;
		case EKnightRPGOperation::ClaimReward:
			Allowed.RewardId = RewardId;
			bValidPayload = RewardId.IsValid();
			break;
		case EKnightRPGOperation::ApplyGearPreset:
			Allowed.PresetId = PresetId;
			bValidPayload = PresetId == TEXT("Guard") || PresetId == TEXT("Mend") || PresetId == TEXT("Strike");
			break;
		default: break;
		}
		return bValidPayload && *this == Allowed ? FKnightResult::Success()
			: FKnightResult::Failure(EKnightError::InvalidData, TEXT("GearCommand"),
				LOCTEXT("GearPayload", "Gear commands require exact identities/revisions and only fields belonging to the selected operation."));
	}
	const bool bLegacyPayload = ProposedBuild == FKnightSkillBuild() && !WeaponRuneInstanceId.IsValid()
		&& !SkillRuneInstanceId.IsValid() && !TalentLoadoutId.IsValid() && TalentLoadoutName.IsEmpty();
	const bool bCleanDomainPayload = !PickupId.IsValid() && !ItemInstanceId.IsValid() && !SlotTag.IsValid()
		&& SkillDefinitionId == FPrimaryAssetId();
	bool bValidName = !TalentLoadoutName.IsEmpty() && TalentLoadoutName.Len() <= 48
		&& TalentLoadoutName == TalentLoadoutName.TrimStartAndEnd();
	for (TCHAR Character : TalentLoadoutName)
	{
		if (FChar::IsControl(Character))
		{
			bValidName = false;
		}
	}
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
	case EKnightRPGOperation::ApplyBuild:
		bValidPayload = bCleanDomainPayload && !ProposedBuild.SpecId.IsNone()
			&& !TalentLoadoutId.IsValid() && TalentLoadoutName.IsEmpty()
			&& (ProposedBuild.WeaponRune.IsNone() == !WeaponRuneInstanceId.IsValid())
			&& (ProposedBuild.SkillRune.IsNone() == !SkillRuneInstanceId.IsValid());
		break;
	case EKnightRPGOperation::NewTalentLoadout:
		bValidPayload = bCleanDomainPayload && !ProposedBuild.SpecId.IsNone()
			&& ProposedBuild.WeaponRune.IsNone() && ProposedBuild.SkillRune.IsNone()
			&& !WeaponRuneInstanceId.IsValid() && !SkillRuneInstanceId.IsValid()
			&& !TalentLoadoutId.IsValid() && bValidName;
		break;
	case EKnightRPGOperation::RenameTalentLoadout:
	case EKnightRPGOperation::DuplicateTalentLoadout:
		bValidPayload = bCleanDomainPayload && ProposedBuild == FKnightSkillBuild()
			&& !WeaponRuneInstanceId.IsValid() && !SkillRuneInstanceId.IsValid()
			&& TalentLoadoutId.IsValid() && bValidName;
		break;
	case EKnightRPGOperation::DeleteTalentLoadout:
	case EKnightRPGOperation::ApplyTalentLoadout:
		bValidPayload = bCleanDomainPayload && ProposedBuild == FKnightSkillBuild()
			&& !WeaponRuneInstanceId.IsValid() && !SkillRuneInstanceId.IsValid()
			&& TalentLoadoutId.IsValid() && TalentLoadoutName.IsEmpty();
		break;
	default:
		break;
	}
	if (Operation <= EKnightRPGOperation::Respec)
	{
		bValidPayload = bValidPayload && bLegacyPayload;
	}
	const bool bItemRevisionAllowed = Operation == EKnightRPGOperation::Equip || Operation == EKnightRPGOperation::Unequip;
	bValidPayload = bValidPayload && !TargetKnightId.IsValid() && ExpectedTargetKnightRevision == 0
		&& !ItemDefinitionId.IsValid() && (ExpectedItemRevision == 0 || bItemRevisionAllowed && ExpectedItemRevision > 0)
		&& Quantity == 1 && QuickSlot == 0 && !RewardId.IsValid() && PresetId.IsNone() && !bConfirmEpicSale;
	return bValidPayload ? FKnightResult::Success()
		: FKnightResult::Failure(EKnightError::InvalidData, TEXT("RPGCommand"),
			LOCTEXT("Payload", "The command contains missing or unrelated payload fields for its operation."));
}

bool FKnightRPGCommand::operator==(const FKnightRPGCommand& Other) const
{
	return Operation == Other.Operation && RequestId == Other.RequestId && ExpectedKnightId == Other.ExpectedKnightId
		&& InventoryId == Other.InventoryId && ExpectedInventoryRevision == Other.ExpectedInventoryRevision
		&& ExpectedKnightRevision == Other.ExpectedKnightRevision && PickupId == Other.PickupId
		&& ItemInstanceId == Other.ItemInstanceId && SlotTag == Other.SlotTag && SkillDefinitionId == Other.SkillDefinitionId
		&& ProposedBuild == Other.ProposedBuild && WeaponRuneInstanceId == Other.WeaponRuneInstanceId
		&& SkillRuneInstanceId == Other.SkillRuneInstanceId && TalentLoadoutId == Other.TalentLoadoutId
		&& TalentLoadoutName == Other.TalentLoadoutName && TargetKnightId == Other.TargetKnightId
		&& ExpectedTargetKnightRevision == Other.ExpectedTargetKnightRevision && ItemDefinitionId == Other.ItemDefinitionId
		&& ExpectedItemRevision == Other.ExpectedItemRevision && Quantity == Other.Quantity && QuickSlot == Other.QuickSlot
		&& RewardId == Other.RewardId && PresetId == Other.PresetId && bConfirmEpicSale == Other.bConfirmEpicSale;
}

#undef LOCTEXT_NAMESPACE
