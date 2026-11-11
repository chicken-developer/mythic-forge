#include "Catalog/GolemGearCatalog.h"

#include "Definitions/GolemItemDefinition.h"
#include "Definitions/GolemRuneDefinition.h"
#include "Tags/GolemGameplayTags.h"

#define LOCTEXT_NAMESPACE "GolemGearCatalog"

namespace GolemGearCatalog
{
	FGolemItemStatBonuses Stats(float Power = 0.0f, float Health = 0.0f, float Armor = 0.0f,
		float Stamina = 0.0f, float Resource = 0.0f, float Poise = 0.0f, float Crit = 0.0f,
		float Recovery = 0.0f, float Mastery = 0.0f, float Versatility = 0.0f)
	{
		FGolemItemStatBonuses Result;
		Result.AttackPower = Power;
		Result.MaxHealth = Health;
		Result.Defense = Armor;
		Result.MaxStamina = Stamina;
		Result.MaxRuneEnergy = Resource;
		Result.Poise = Poise;
		Result.CritChance = Crit / 100.0f;
		Result.CooldownRecovery = Recovery / 100.0f;
		Result.MasteryBonus = Mastery / 100.0f;
		Result.Versatility = Versatility / 100.0f;
		return Result;
	}
}

FPrimaryAssetId FGolemGearCatalog::DefinitionId(FName CatalogId)
{
	const TCHAR* RuneIds[] { TEXT("Aftershock"), TEXT("Convergence"), TEXT("Propagation"), TEXT("Mobile") };
	for (int32 Index = 0; Index < 4; ++Index)
	{
		if (CatalogId == FName(*FString::Printf(TEXT("R%02d"), Index + 1)))
		{
			return FPrimaryAssetId(TEXT("GolemItem"), RuneIds[Index]);
		}
	}
	return FPrimaryAssetId(TEXT("GolemItem"), FName(*(TEXT("M056_") + CatalogId.ToString())));
}

const TArray<FGolemGearSeed>& FGolemGearCatalog::GetSeeds()
{
	static const TArray<FGolemGearSeed> Seeds = []()
	{
		using namespace GolemGearCatalog;
		TArray<FGolemGearSeed> Rows;
		auto AddEquipment = [&Rows](const TCHAR* Id, const FText& Name, EGolemEquipmentSubtype Subtype,
			EGolemItemRarity Rarity, int32 ItemLevel, int32 Level, const FGolemItemStatBonuses& Bonuses,
			FName Effect = NAME_None)
		{
			FGolemGearSeed Row;
			Row.CatalogId = Id;
			Row.DefinitionId = FGolemGearCatalog::DefinitionId(Row.CatalogId);
			Row.Name = Name;
			Row.Description = LOCTEXT("StoneEquipment", "Universal stone fitting. Role hints are suggestions; requirements apply equally to every Golem.");
			Row.Stats = Bonuses;
			Row.Metadata.Category = EGolemItemCategory::Equipment;
			Row.Metadata.Subtype = Subtype;
			Row.Metadata.Rarity = Rarity;
			Row.Metadata.ItemLevel = ItemLevel;
			Row.Metadata.RequiredLevel = Level;
			Row.Metadata.Theme = Row.CatalogId;
			Row.Metadata.EquipEffectId = Effect;
			Row.Metadata.UpgradeTrack = Rarity == EGolemItemRarity::Rare ? FName(TEXT("RareOne")) : NAME_None;
			Row.Metadata.AllowedSlots.Add(GolemGear::SlotForSubtype(Subtype));
			Row.Metadata.SellPrice = Rarity == EGolemItemRarity::Epic ? 8 : Rarity == EGolemItemRarity::Rare ? 4 : 1;
			if (Subtype == EGolemEquipmentSubtype::Hammer)
			{
				Row.Metadata.SocketProfile = TEXT("WeaponRune");
			}
			if (Subtype == EGolemEquipmentSubtype::Ring)
			{
				Row.Metadata.AllowedSlots.Add(GolemGameplayTags::Slot_Equipment_Ring2);
				Row.Metadata.UniqueEquipGroup = Row.DefinitionId;
				Row.Metadata.UniqueEquipLimit = 1;
			}
			if (Rarity == EGolemItemRarity::Uncommon && (Subtype == EGolemEquipmentSubtype::Neck
				|| Subtype == EGolemEquipmentSubtype::Ring))
			{
				Row.Metadata.BuyPrice = 8;
				Row.Metadata.SellPrice = 2;
			}
			Rows.Add(MoveTemp(Row));
		};
		const EGolemEquipmentSubtype Hammer = EGolemEquipmentSubtype::Hammer;
		const EGolemItemRarity Common = EGolemItemRarity::Common;
		const EGolemItemRarity Uncommon = EGolemItemRarity::Uncommon;
		const EGolemItemRarity Rare = EGolemItemRarity::Rare;
		const EGolemItemRarity Epic = EGolemItemRarity::Epic;
		AddEquipment(TEXT("W01"), LOCTEXT("W01", "Búa Đá Tập Sự"), Hammer, Common, 10, 1, Stats(10));
		Rows.Last().Metadata.BuyPrice = 5;
		AddEquipment(TEXT("W02"), LOCTEXT("W02", "Búa Thành Lũy"), Hammer, Rare, 20, 5, Stats(18, 0, 25, 0, 0, 0, 0, 0, 0, 2));
		AddEquipment(TEXT("W03"), LOCTEXT("W03", "Búa Mạch Sống"), Hammer, Rare, 20, 5, Stats(18, 0, 0, 0, 0, 0, 0, 2, 3));
		AddEquipment(TEXT("W04"), LOCTEXT("W04", "Búa Sét Nứt"), Hammer, Rare, 20, 5, Stats(18, 0, 0, 0, 0, 0, 3, 2));
		AddEquipment(TEXT("W05"), LOCTEXT("W05", "Trọng Chùy Thiên Thành"), Hammer, Epic, 30, 8, Stats(26, 0, 35, 0, 0, 0, 0, 0, 0, 3));
		AddEquipment(TEXT("W06"), LOCTEXT("W06", "Chùy Sinh Mệnh Cổ"), Hammer, Epic, 30, 8, Stats(26, 0, 0, 0, 0, 0, 0, 3, 4));
		AddEquipment(TEXT("W07"), LOCTEXT("W07", "Chùy Thiên Thạch"), Hammer, Epic, 30, 8, Stats(26, 0, 0, 0, 0, 0, 4, 3));
		AddEquipment(TEXT("A01"), LOCTEXT("A01", "Vòng Trán Đá Thô"), EGolemEquipmentSubtype::Head, Uncommon, 10, 1, Stats(0, 20, 5));
		AddEquipment(TEXT("A02"), LOCTEXT("A02", "Vai Đá Thô"), EGolemEquipmentSubtype::Shoulders, Uncommon, 10, 1, Stats(0, 20, 5));
		AddEquipment(TEXT("A03"), LOCTEXT("A03", "Giáp Ngực Đá Thô"), EGolemEquipmentSubtype::Chest, Uncommon, 10, 1, Stats(0, 40, 10));
		AddEquipment(TEXT("A04"), LOCTEXT("A04", "Vòng Tay Đá Thô"), EGolemEquipmentSubtype::Wrists, Uncommon, 10, 1, Stats(0, 10, 3));
		AddEquipment(TEXT("A05"), LOCTEXT("A05", "Bọc Tay Đá Thô"), EGolemEquipmentSubtype::Hands, Uncommon, 10, 1, Stats(0, 15, 4));
		AddEquipment(TEXT("A06"), LOCTEXT("A06", "Đai Đá Thô"), EGolemEquipmentSubtype::Waist, Uncommon, 10, 1, Stats(0, 15, 4));
		AddEquipment(TEXT("A07"), LOCTEXT("A07", "Bọc Chân Đá Thô"), EGolemEquipmentSubtype::Feet, Uncommon, 10, 1, Stats(0, 20, 5));
		for (int32 Index = 7; Index < 14; ++Index)
		{
			Rows[Index].Metadata.BuyPrice = 4;
		}
		AddEquipment(TEXT("A08"), LOCTEXT("A08", "Vương Miện Thành Lũy"), EGolemEquipmentSubtype::Head, Rare, 20, 5, Stats(0, 40, 10, 0, 0, 0, 0, 0, 0, 2));
		AddEquipment(TEXT("A09"), LOCTEXT("A09", "Vai Hộ Thành"), EGolemEquipmentSubtype::Shoulders, Rare, 20, 5, Stats(0, 40, 10, 0, 0, 10));
		AddEquipment(TEXT("A10"), LOCTEXT("A10", "Giáp Ngực Thạch Vệ"), EGolemEquipmentSubtype::Chest, Rare, 20, 5, Stats(0, 80, 20, 0, 0, 0, 0, 0, 0, 2));
		AddEquipment(TEXT("A11"), LOCTEXT("A11", "Vòng Tay Dẫn Sinh"), EGolemEquipmentSubtype::Wrists, Rare, 20, 5, Stats(0, 20, 6, 0, 0, 0, 0, 0, 2));
		AddEquipment(TEXT("A12"), LOCTEXT("A12", "Găng Tay Phá Trận"), EGolemEquipmentSubtype::Hands, Rare, 20, 5, Stats(0, 30, 8, 0, 0, 0, 2));
		AddEquipment(TEXT("A13"), LOCTEXT("A13", "Đai Dòng Chảy"), EGolemEquipmentSubtype::Waist, Rare, 20, 5, Stats(0, 30, 8, 0, 0, 0, 0, 2));
		AddEquipment(TEXT("A14"), LOCTEXT("A14", "Bọc Chân Lữ Hành"), EGolemEquipmentSubtype::Feet, Rare, 20, 5, Stats(0, 40, 10, 10));
		AddEquipment(TEXT("N01"), LOCTEXT("N01", "Mặt Dây Người Lữ Hành"), EGolemEquipmentSubtype::Neck, Uncommon, 15, 3, Stats(4, 20));
		AddEquipment(TEXT("N02"), LOCTEXT("N02", "Mặt Dây Tam Mạch"), EGolemEquipmentSubtype::Neck, Rare, 20, 5, Stats(6, 30, 0, 0, 0, 0, 0, 0, 0, 2));
		AddEquipment(TEXT("F01"), LOCTEXT("F01", "Nhẫn Đá Bền"), EGolemEquipmentSubtype::Ring, Uncommon, 15, 3, Stats(0, 30, 0, 0, 0, 0, 0, 0, 0, 1));
		AddEquipment(TEXT("F02"), LOCTEXT("F02", "Nhẫn Trữ Năng"), EGolemEquipmentSubtype::Ring, Uncommon, 15, 3, Stats(0, 0, 0, 0, 10, 0, 0, 1));
		AddEquipment(TEXT("F03"), LOCTEXT("F03", "Nhẫn Phá Giáp"), EGolemEquipmentSubtype::Ring, Rare, 20, 5, Stats(4, 0, 0, 0, 0, 0, 2, 1));
		AddEquipment(TEXT("F04"), LOCTEXT("F04", "Nhẫn Cộng Hưởng"), EGolemEquipmentSubtype::Ring, Rare, 20, 5, Stats(4, 0, 0, 0, 0, 0, 0, 0, 2, 1));
		AddEquipment(TEXT("T01"), LOCTEXT("T01", "Mảnh Thành Cổ"), EGolemEquipmentSubtype::Relic, Rare, 20, 5, Stats(0, 40, 0, 0, 0, 0, 0, 0, 0, 2), TEXT("Bulwark"));
		AddEquipment(TEXT("T02"), LOCTEXT("T02", "Hạt Mạch Sống"), EGolemEquipmentSubtype::Relic, Rare, 20, 5, Stats(4, 0, 0, 0, 0, 0, 0, 0, 2), TEXT("Lifeseed"));
		AddEquipment(TEXT("T03"), LOCTEXT("T03", "Mảnh Sao Rơi"), EGolemEquipmentSubtype::Relic, Rare, 20, 5, Stats(4, 0, 0, 0, 0, 0, 2), TEXT("Starfall"));
		const FName RuneKeys[] { TEXT("Weapon.Aftershock"), TEXT("Skill.Convergence"), TEXT("Skill.Propagation"), TEXT("Skill.Mobile") };
		for (int32 Index = 0; Index < 4; ++Index)
		{
			FGolemGearSeed Row;
			Row.CatalogId = FName(*FString::Printf(TEXT("R%02d"), Index + 1));
			Row.DefinitionId = FGolemGearCatalog::DefinitionId(Row.CatalogId);
			Row.Name = FText::FromName(RuneKeys[Index]);
			Row.Metadata.Category = EGolemItemCategory::AttachRune;
			Row.Metadata.Rarity = Rare;
			Row.Metadata.SocketProfile = RuneKeys[Index];
			Rows.Add(MoveTemp(Row));
		}
		auto AddItem = [&Rows](const TCHAR* Id, const FText& Name, EGolemItemCategory Category,
			EGolemItemRarity Rarity, int32 Stack, FName Effect = NAME_None)
		{
			FGolemGearSeed Row;
			Row.CatalogId = Id;
			Row.DefinitionId = FGolemGearCatalog::DefinitionId(Row.CatalogId);
			Row.Name = Name;
			Row.MaximumStack = Stack;
			Row.Metadata.Category = Category;
			Row.Metadata.Rarity = Rarity;
			Row.Metadata.SocketProfile = Effect;
			Row.Metadata.Theme = Id;
			Rows.Add(MoveTemp(Row));
		};
		AddItem(TEXT("C01"), LOCTEXT("C01", "Bình Nhựa Sinh Lực"), EGolemItemCategory::Consumable, Common, 20, TEXT("Health"));
		Rows.Last().Metadata.BuyPrice = 3;
		Rows.Last().Metadata.SellPrice = 1;
		AddItem(TEXT("C02"), LOCTEXT("C02", "Bình Tụ Năng"), EGolemItemCategory::Consumable, Common, 20, TEXT("Resource"));
		Rows.Last().Metadata.BuyPrice = 3;
		Rows.Last().Metadata.SellPrice = 1;
		AddItem(TEXT("M01"), LOCTEXT("M01", "Mảnh Đá Rèn"), EGolemItemCategory::Material, Common, 99);
		AddItem(TEXT("M02"), LOCTEXT("M02", "Bụi Rune"), EGolemItemCategory::Material, Uncommon, 99);
		AddItem(TEXT("M03"), LOCTEXT("M03", "Tinh Thể Cổ"), EGolemItemCategory::Material, Rare, 99);
		AddItem(TEXT("Q01"), LOCTEXT("Q01", "Ấn Cổng Vỡ"), EGolemItemCategory::QuestItem, Common, 1);
		Rows.Last().Metadata.bUniqueOwned = true;
		AddItem(TEXT("J01"), LOCTEXT("J01", "Mảnh Đá Sứt"), EGolemItemCategory::Junk, EGolemItemRarity::Poor, 99);
		Rows.Last().Metadata.SellPrice = 2;
		return Rows;
	}();
	return Seeds;
}

FGolemResult FGolemGearCatalog::ConfigureDefinition(UGolemItemDefinition* Definition, const FGolemGearSeed& Seed)
{
	if (!IsValid(Definition) || !Definition->TrySetIdentity(Seed.DefinitionId.PrimaryAssetName, TEXT("GolemItem")))
	{
		return FGolemResult::Failure(EGolemError::InvalidId, TEXT("GearSeed"), LOCTEXT("SeedIdentity", "Gear authoring needs the exact stable definition identity."));
	}
	Definition->SetDisplayName(Seed.Name);
	EGolemItemKind Kind = EGolemItemKind::Material;
	switch (Seed.Metadata.Category)
	{
	case EGolemItemCategory::Equipment: Kind = EGolemItemKind::Equipment; break;
	case EGolemItemCategory::AttachRune: Kind = EGolemItemKind::Rune; break;
	case EGolemItemCategory::Consumable: Kind = EGolemItemKind::Consumable; break;
	case EGolemItemCategory::QuestItem: Kind = EGolemItemKind::QuestItem; break;
	case EGolemItemCategory::Junk: Kind = EGolemItemKind::Junk; break;
	default: break;
	}
	FGolemResult Result = Definition->TrySetItemTuning(Kind, Seed.MaximumStack, FPrimaryAssetId(), Seed.Stats);
	if (Result.IsSuccess() && Kind == EGolemItemKind::Rune)
	{
		UGolemRuneDefinition* Rune = Cast<UGolemRuneDefinition>(Definition);
		Result = Rune != nullptr ? Rune->TrySetAttachmentRune(Seed.Metadata.SocketProfile)
			: FGolemResult::Failure(EGolemError::TypeMismatch, TEXT("RuneSeed"), LOCTEXT("RuneClass", "Rune seeds reuse actual GolemRuneDefinition assets."));
	}
	if (Result.IsSuccess())
	{
		FGolemItemMetadata Metadata = Seed.Metadata;
		Metadata.Description = Seed.Description;
		Result = Definition->TrySetMetadata(Metadata);
	}
	return Result.IsSuccess() ? Definition->ValidateItem() : Result;
}

FGolemResult FGolemGearCatalog::ValidateCatalog(TConstArrayView<UGolemItemDefinition*> Definitions)
{
	if (Definitions.Num() != 41 || GetSeeds().Num() != 41 || GolemGear::EquipmentSlots().Num() != 12)
	{
		return FGolemResult::Failure(EGolemError::InvalidData, TEXT("GearCatalog"), LOCTEXT("Count", "The demo manifest requires exactly 41 definitions and 12 equipment slots."));
	}
	TSet<FPrimaryAssetId> Ids;
	TSet<EGolemEquipmentSubtype> Subtypes;
	int32 Counts[6] {};
	for (const UGolemItemDefinition* Definition : Definitions)
	{
		if (Definition == nullptr || !Definition->ValidateItem().IsSuccess() || Ids.Contains(Definition->GetPrimaryAssetId())
			|| !GetSeeds().ContainsByPredicate([Definition](const FGolemGearSeed& Seed)
			{
				return Seed.DefinitionId == Definition->GetPrimaryAssetId();
			}))
		{
			return FGolemResult::Failure(EGolemError::InvalidData, TEXT("GearCatalog"), LOCTEXT("Definitions", "Each manifest definition must resolve once to a validated actual item."));
		}
		Ids.Add(Definition->GetPrimaryAssetId());
		const FGolemGearSeed* Seed = GetSeeds().FindByPredicate([Definition](const FGolemGearSeed& Entry)
		{
			return Entry.DefinitionId == Definition->GetPrimaryAssetId();
		});
		const FGolemItemMetadata& Metadata = Definition->GetMetadata();
		if (!(Definition->GetStatBonuses() == Seed->Stats) || Definition->GetMaximumStack() != Seed->MaximumStack
			|| Metadata.Category != Seed->Metadata.Category || Metadata.Subtype != Seed->Metadata.Subtype
			|| Metadata.Rarity != Seed->Metadata.Rarity || Metadata.ItemLevel != Seed->Metadata.ItemLevel
			|| Metadata.RequiredLevel != Seed->Metadata.RequiredLevel || Metadata.AllowedSlots != Seed->Metadata.AllowedSlots
			|| Metadata.SocketProfile != Seed->Metadata.SocketProfile || Metadata.EquipEffectId != Seed->Metadata.EquipEffectId
			|| Metadata.UpgradeTrack != Seed->Metadata.UpgradeTrack || Metadata.BuyPrice != Seed->Metadata.BuyPrice
			|| Metadata.SellPrice != Seed->Metadata.SellPrice || Metadata.UniqueEquipGroup != Seed->Metadata.UniqueEquipGroup
			|| Metadata.UniqueEquipLimit != Seed->Metadata.UniqueEquipLimit || Metadata.bUniqueOwned != Seed->Metadata.bUniqueOwned)
		{
			return FGolemResult::Failure(EGolemError::Conflict, TEXT("GearManifestValue"),
				LOCTEXT("ManifestValue", "Actual definition stats, requirements, sockets, prices and upgrade policy must match the seed manifest."));
		}
		if (Metadata.Category == EGolemItemCategory::Equipment)
		{
			Subtypes.Add(Metadata.Subtype);
		}
		++Counts[static_cast<int32>(Definition->GetMetadata().Category)];
	}
	const int32 Expected[] { 30, 4, 2, 3, 1, 1 };
	for (int32 Index = 0; Index < 6; ++Index)
	{
		if (Counts[Index] != Expected[Index])
		{
			return FGolemResult::Failure(EGolemError::InvalidData, TEXT("GearCatalog"), LOCTEXT("Categories", "Catalog categories must match the exact authored 30/4/2/3/1/1 definition counts."));
		}
	}
	if (Subtypes.Num() != 11)
	{
		return FGolemResult::Failure(EGolemError::InvalidData, TEXT("GearManifestSubtypes"),
			LOCTEXT("ManifestSubtypes", "The complete catalog requires all eleven actual equipment subtypes."));
	}
	return FGolemResult::Success();
}

FGolemGearRewardBundle FGolemGearCatalog::Reward(FName Encounter, const FGuid& DemoRunId, const FGuid& EncounterId)
{
	FGolemGearRewardBundle Result;
	const int32 Stage = Encounter == TEXT("A") ? 1 : Encounter == TEXT("B") ? 2 : Encounter == TEXT("C") ? 3 : 0;
	if (Stage == 0 || !DemoRunId.IsValid())
	{
		return Result;
	}
	Result.DemoRunId = DemoRunId;
	Result.EncounterId = EncounterId;
	Result.RewardId = FGuid(DemoRunId.A ^ 0x56300000u, DemoRunId.B, DemoRunId.C, DemoRunId.D ^ Stage);
	Result.Currency = Stage == 1 ? 30 : Stage == 2 ? 40 : 50;
	Result.Level = Stage == 1 ? 3 : Stage == 2 ? 5 : 8;
	auto Add = [&Result](const TCHAR* Id, int32 Count)
	{
		FGolemGearRewardItem Item;
		Item.DefinitionId = FGolemGearCatalog::DefinitionId(Id);
		Item.Count = Count;
		Result.Items.Add(Item);
	};
	if (Stage == 1)
	{
		Add(TEXT("N01"), 3); Add(TEXT("F01"), 3); Add(TEXT("F02"), 3); Add(TEXT("J01"), 3); Add(TEXT("M01"), 4);
	}
	else if (Stage == 2)
	{
		Add(TEXT("W02"), 1); Add(TEXT("W03"), 1); Add(TEXT("W04"), 1);
		for (int32 Index = 8; Index <= 14; ++Index)
		{
			Add(*FString::Printf(TEXT("A%02d"), Index), 3);
		}
		Add(TEXT("N02"), 3); Add(TEXT("F03"), 3); Add(TEXT("F04"), 3);
		Add(TEXT("T01"), 1); Add(TEXT("T02"), 1); Add(TEXT("T03"), 1);
		Add(TEXT("R01"), 3); Add(TEXT("R02"), 1); Add(TEXT("R03"), 1); Add(TEXT("R04"), 1);
		Add(TEXT("M01"), 8); Add(TEXT("M02"), 4); Add(TEXT("M03"), 2);
	}
	else
	{
		Add(TEXT("W05"), 1); Add(TEXT("W06"), 1); Add(TEXT("W07"), 1); Add(TEXT("Q01"), 1); Add(TEXT("M03"), 1);
	}
	return Result;
}

#undef LOCTEXT_NAMESPACE
