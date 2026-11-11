#include "Commands/GolemRPGCommand.h"

#define LOCTEXT_NAMESPACE "GolemRPGCommand"

FGolemResult FGolemRPGCommand::ValidateShape() const
{
	if (!RequestId.IsValid() || !ExpectedGolemId.IsValid() || !InventoryId.IsValid())
	{
		return FGolemResult::Failure(EGolemError::InvalidId, TEXT("RPGCommand"),
			LOCTEXT("Identity", "An RPG command requires exact request, Golem and inventory identities."));
	}
	if (ExpectedInventoryRevision < 1 || ExpectedInventoryRevision >= MAX_int64
		|| ExpectedGolemRevision < 1 || ExpectedGolemRevision >= MAX_int64)
	{
		return FGolemResult::Failure(EGolemError::InvalidRevision, TEXT("RPGCommand"),
			LOCTEXT("Revision", "Expected RPG revisions must be finite positive values."));
	}
	bool bValidPayload = false;
	if (Operation >= EGolemRPGOperation::TransferEquipment)
	{
		FGolemRPGCommand Allowed;
		Allowed.Operation = Operation;
		Allowed.RequestId = RequestId;
		Allowed.ExpectedGolemId = ExpectedGolemId;
		Allowed.InventoryId = InventoryId;
		Allowed.ExpectedInventoryRevision = ExpectedInventoryRevision;
		Allowed.ExpectedGolemRevision = ExpectedGolemRevision;
		switch (Operation)
		{
		case EGolemRPGOperation::TransferEquipment:
			Allowed.ItemInstanceId = ItemInstanceId;
			Allowed.ExpectedItemRevision = ExpectedItemRevision;
			Allowed.SlotTag = SlotTag;
			Allowed.TargetGolemId = TargetGolemId;
			Allowed.ExpectedTargetGolemRevision = ExpectedTargetGolemRevision;
			bValidPayload = ItemInstanceId.IsValid() && ExpectedItemRevision > 0 && SlotTag.IsValid()
				&& TargetGolemId.IsValid() && TargetGolemId != ExpectedGolemId && ExpectedTargetGolemRevision > 0;
			break;
		case EGolemRPGOperation::SwapRings:
			bValidPayload = true;
			break;
		case EGolemRPGOperation::AttachWeaponRune:
		case EGolemRPGOperation::DetachWeaponRune:
			Allowed.ItemInstanceId = ItemInstanceId;
			Allowed.ExpectedItemRevision = ExpectedItemRevision;
			Allowed.WeaponRuneInstanceId = WeaponRuneInstanceId;
			bValidPayload = ItemInstanceId.IsValid() && ExpectedItemRevision > 0
				&& (WeaponRuneInstanceId.IsValid() == (Operation == EGolemRPGOperation::AttachWeaponRune));
			break;
		case EGolemRPGOperation::UseConsumable:
		case EGolemRPGOperation::BindConsumable:
			Allowed.ItemDefinitionId = ItemDefinitionId;
			Allowed.QuickSlot = QuickSlot;
			bValidPayload = QuickSlot >= 0 && QuickSlot < 2
				&& (Operation == EGolemRPGOperation::UseConsumable && !ItemDefinitionId.IsValid()
					|| GolemIdentity::IsValidDefinitionId(ItemDefinitionId));
			break;
		case EGolemRPGOperation::BuyItem:
			Allowed.ItemDefinitionId = ItemDefinitionId;
			Allowed.Quantity = Quantity;
			bValidPayload = GolemIdentity::IsValidDefinitionId(ItemDefinitionId) && Quantity > 0 && Quantity <= 20;
			break;
		case EGolemRPGOperation::SellItem:
			Allowed.Quantity = Quantity;
			Allowed.bConfirmEpicSale = bConfirmEpicSale;
			[[fallthrough]];
		case EGolemRPGOperation::UpgradeItem:
			Allowed.ItemInstanceId = ItemInstanceId;
			Allowed.ExpectedItemRevision = ExpectedItemRevision;
			bValidPayload = ItemInstanceId.IsValid() && ExpectedItemRevision > 0 && Quantity > 0
				&& Quantity <= 10000;
			break;
		case EGolemRPGOperation::DeliverQuestItem:
			Allowed.ItemInstanceId = ItemInstanceId;
			Allowed.ExpectedItemRevision = ExpectedItemRevision;
			Allowed.ItemDefinitionId = ItemDefinitionId;
			Allowed.Quantity = Quantity;
			bValidPayload = ItemInstanceId.IsValid() && ExpectedItemRevision > 0
				&& GolemIdentity::IsValidDefinitionId(ItemDefinitionId) && Quantity > 0 && Quantity <= 10000;
			break;
		case EGolemRPGOperation::ClaimReward:
			Allowed.RewardId = RewardId;
			bValidPayload = RewardId.IsValid();
			break;
		case EGolemRPGOperation::ApplyGearPreset:
			Allowed.PresetId = PresetId;
			bValidPayload = PresetId == TEXT("Guard") || PresetId == TEXT("Mend") || PresetId == TEXT("Strike");
			break;
		default: break;
		}
		return bValidPayload && *this == Allowed ? FGolemResult::Success()
			: FGolemResult::Failure(EGolemError::InvalidData, TEXT("GearCommand"),
				LOCTEXT("GearPayload", "Gear commands require exact identities/revisions and only fields belonging to the selected operation."));
	}
	const bool bLegacyPayload = ProposedBuild == FGolemSkillBuild() && !WeaponRuneInstanceId.IsValid()
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
	case EGolemRPGOperation::Pickup:
		bValidPayload = PickupId.IsValid() && !ItemInstanceId.IsValid() && !SlotTag.IsValid()
			&& SkillDefinitionId == FPrimaryAssetId();
		break;
	case EGolemRPGOperation::Equip:
	case EGolemRPGOperation::Rune:
	case EGolemRPGOperation::Unequip:
		bValidPayload = !PickupId.IsValid() && ItemInstanceId.IsValid() && SlotTag.IsValid()
			&& SkillDefinitionId == FPrimaryAssetId();
		break;
	case EGolemRPGOperation::PurchaseSkill:
		bValidPayload = !PickupId.IsValid() && !ItemInstanceId.IsValid() && !SlotTag.IsValid()
			&& GolemIdentity::IsValidDefinitionId(SkillDefinitionId)
			&& SkillDefinitionId.PrimaryAssetType == FPrimaryAssetType(TEXT("KnightSkill"));
		break;
	case EGolemRPGOperation::Respec:
		bValidPayload = !PickupId.IsValid() && !ItemInstanceId.IsValid() && !SlotTag.IsValid()
			&& SkillDefinitionId == FPrimaryAssetId();
		break;
	case EGolemRPGOperation::ApplyBuild:
		bValidPayload = bCleanDomainPayload && !ProposedBuild.SpecId.IsNone()
			&& !TalentLoadoutId.IsValid() && TalentLoadoutName.IsEmpty()
			&& (ProposedBuild.WeaponRune.IsNone() == !WeaponRuneInstanceId.IsValid())
			&& (ProposedBuild.SkillRune.IsNone() == !SkillRuneInstanceId.IsValid());
		break;
	case EGolemRPGOperation::NewTalentLoadout:
		bValidPayload = bCleanDomainPayload && !ProposedBuild.SpecId.IsNone()
			&& ProposedBuild.WeaponRune.IsNone() && ProposedBuild.SkillRune.IsNone()
			&& !WeaponRuneInstanceId.IsValid() && !SkillRuneInstanceId.IsValid()
			&& !TalentLoadoutId.IsValid() && bValidName;
		break;
	case EGolemRPGOperation::RenameTalentLoadout:
	case EGolemRPGOperation::DuplicateTalentLoadout:
		bValidPayload = bCleanDomainPayload && ProposedBuild == FGolemSkillBuild()
			&& !WeaponRuneInstanceId.IsValid() && !SkillRuneInstanceId.IsValid()
			&& TalentLoadoutId.IsValid() && bValidName;
		break;
	case EGolemRPGOperation::DeleteTalentLoadout:
	case EGolemRPGOperation::ApplyTalentLoadout:
		bValidPayload = bCleanDomainPayload && ProposedBuild == FGolemSkillBuild()
			&& !WeaponRuneInstanceId.IsValid() && !SkillRuneInstanceId.IsValid()
			&& TalentLoadoutId.IsValid() && TalentLoadoutName.IsEmpty();
		break;
	default:
		break;
	}
	if (Operation <= EGolemRPGOperation::Respec)
	{
		bValidPayload = bValidPayload && bLegacyPayload;
	}
	const bool bItemRevisionAllowed = Operation == EGolemRPGOperation::Equip || Operation == EGolemRPGOperation::Unequip;
	bValidPayload = bValidPayload && !TargetGolemId.IsValid() && ExpectedTargetGolemRevision == 0
		&& !ItemDefinitionId.IsValid() && (ExpectedItemRevision == 0 || bItemRevisionAllowed && ExpectedItemRevision > 0)
		&& Quantity == 1 && QuickSlot == 0 && !RewardId.IsValid() && PresetId.IsNone() && !bConfirmEpicSale;
	return bValidPayload ? FGolemResult::Success()
		: FGolemResult::Failure(EGolemError::InvalidData, TEXT("RPGCommand"),
			LOCTEXT("Payload", "The command contains missing or unrelated payload fields for its operation."));
}

bool FGolemRPGCommand::operator==(const FGolemRPGCommand& Other) const
{
	return Operation == Other.Operation && RequestId == Other.RequestId && ExpectedGolemId == Other.ExpectedGolemId
		&& InventoryId == Other.InventoryId && ExpectedInventoryRevision == Other.ExpectedInventoryRevision
		&& ExpectedGolemRevision == Other.ExpectedGolemRevision && PickupId == Other.PickupId
		&& ItemInstanceId == Other.ItemInstanceId && SlotTag == Other.SlotTag && SkillDefinitionId == Other.SkillDefinitionId
		&& ProposedBuild == Other.ProposedBuild && WeaponRuneInstanceId == Other.WeaponRuneInstanceId
		&& SkillRuneInstanceId == Other.SkillRuneInstanceId && TalentLoadoutId == Other.TalentLoadoutId
		&& TalentLoadoutName == Other.TalentLoadoutName && TargetGolemId == Other.TargetGolemId
		&& ExpectedTargetGolemRevision == Other.ExpectedTargetGolemRevision && ItemDefinitionId == Other.ItemDefinitionId
		&& ExpectedItemRevision == Other.ExpectedItemRevision && Quantity == Other.Quantity && QuickSlot == Other.QuickSlot
		&& RewardId == Other.RewardId && PresetId == Other.PresetId && bConfirmEpicSale == Other.bConfirmEpicSale;
}

#undef LOCTEXT_NAMESPACE
