#include "RPGLoop/KnightRPGGearPresentation.h"

#include "Assets/KnightAssetManager.h"
#include "Attributes/KnightAttributeSet.h"
#include "Catalog/KnightGearCatalog.h"
#include "Character/KnightCharacter.h"
#include "Components/KnightAbilitySystemComponent.h"
#include "Components/KnightCombatComponent.h"
#include "Components/KnightLifeComponent.h"
#include "Definitions/KnightCombatProfileDefinition.h"
#include "Definitions/KnightItemDefinition.h"
#include "Definitions/KnightRuneDefinition.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/GameStateBase.h"
#include "Engine/World.h"
#include "GearLab/KnightGearLabState.h"
#include "Progression/KnightRPGComponent.h"
#include "Skills/KnightSkillRuntimeComponent.h"
#include "Tags/KnightGameplayTags.h"

namespace KnightRPGGearPresentation
{
	namespace
	{
		const UKnightItemDefinition* Resolve(const FPrimaryAssetId& Id)
		{
			UKnightDefinition* Definition = nullptr;
			UKnightAssetManager* Manager = UKnightAssetManager::GetKnightAssetManager();
			return Manager != nullptr && Manager->ResolveDefinition(Id, UKnightItemDefinition::StaticClass(), Definition).IsSuccess()
				? Cast<UKnightItemDefinition>(Definition) : nullptr;
		}

		FText Name(const FPrimaryAssetId& Id)
		{
			const UKnightItemDefinition* Definition = Resolve(Id);
			return Definition != nullptr ? Definition->GetDisplayName() : FText::FromName(Id.PrimaryAssetName);
		}

		FText KnightName(const FPrimaryAssetId& Id)
		{
			if (Id.PrimaryAssetName == TEXT("Training_Red"))
			{
				return NSLOCTEXT("KnightGearUI", "Sky", "Sky");
			}
			if (Id.PrimaryAssetName == TEXT("Training_Blue"))
			{
				return NSLOCTEXT("KnightGearUI", "Sea", "Sea");
			}
			if (Id.PrimaryAssetName == TEXT("Training_Green"))
			{
				return NSLOCTEXT("KnightGearUI", "Earth", "Earth");
			}
			return FText::FromName(Id.PrimaryAssetName);
		}

		FText Stats(const FKnightItemStatBonuses& Bonus)
		{
			FString Text;
			auto Add = [&Text](const TCHAR* Label, float Value, bool bPercent = false)
			{
				if (Value > 0.0f)
				{
					Text += FString::Printf(TEXT("%s +%g%s\n"), Label, bPercent ? Value * 100.0f : Value,
						bPercent ? TEXT(" percentage points") : TEXT(""));
				}
			};
			Add(TEXT("Power"), Bonus.AttackPower);
			Add(TEXT("Max HP"), Bonus.MaxHealth);
			Add(TEXT("Armor"), Bonus.Defense);
			Add(TEXT("Max Stamina"), Bonus.MaxStamina);
			Add(TEXT("Max class resource"), Bonus.MaxRuneEnergy);
			Add(TEXT("Poise"), Bonus.Poise);
			Add(TEXT("Crit chance (direct damage only)"), Bonus.CritChance, true);
			Add(TEXT("Haste (windup only)"), Bonus.Haste, true);
			Add(TEXT("Cooldown recovery (new cooldowns)"), Bonus.CooldownRecovery, true);
			Add(TEXT("Mastery"), Bonus.MasteryBonus, true);
			Add(TEXT("Versatility"), Bonus.Versatility, true);
			return FText::FromString(Text);
		}

		FText Effect(FName EffectId)
		{
			if (EffectId == TEXT("Bulwark"))
			{
				return NSLOCTEXT("KnightGearUI", "Bulwark", "Equip: eligible hostile direct block/parry grants self shield 0.60×Power for5s; ICD12s. No Mastery/Versatility/crit. Family ICD survives re-equip and possession.");
			}
			if (EffectId == TEXT("Lifeseed"))
			{
				return NSLOCTEXT("KnightGearUI", "Lifeseed", "Equip: primary direct ability heal with actual healing>0 applies0.15×Power HoT/s for3s, first tick+1s; ICD10s. Snapshots outgoing Versatility; no Mastery/crit/secondary/potion trigger.");
			}
			if (EffectId == TEXT("Starfall"))
			{
				return NSLOCTEXT("KnightGearUI", "Starfall", "Equip: third eligible hostile direct activation deals0.40×Power secondary damage to primary target, original school; ICD8s, counter expires after10s. No crit/proc/reaction/poise/durability. Target defense still applies.");
			}
			return NSLOCTEXT("KnightGearUI", "NoEquipEffect", "Equip effect: None");
		}

		FLinearColor Color(EKnightItemRarity Rarity)
		{
			switch (Rarity)
			{
			case EKnightItemRarity::Poor: return FLinearColor(0.65f, 0.65f, 0.65f);
			case EKnightItemRarity::Uncommon: return FLinearColor(0.3f, 0.95f, 0.35f);
			case EKnightItemRarity::Rare: return FLinearColor(0.35f, 0.65f, 1.0f);
			case EKnightItemRarity::Epic: return FLinearColor(0.8f, 0.5f, 1.0f);
			default: return FLinearColor::White;
			}
		}

		FKnightGearItemRow ItemRow(const FKnightItemInstance& Item, const FKnightRPGView& Party,
			const FKnightInventoryView& Inventory, const FKnightRPGKnightView& Viewed)
		{
			FKnightGearItemRow Row;
			const UKnightItemDefinition* Definition = Resolve(Item.DefinitionId);
			if (Definition == nullptr)
			{
				return Row;
			}
			const FKnightItemMetadata& Metadata = Definition->GetMetadata();
			FString AllowedSlots;
			for (const FGameplayTag& AllowedSlot : Metadata.AllowedSlots)
			{
				AllowedSlots += AllowedSlots.IsEmpty() ? AllowedSlot.ToString() : TEXT(", ") + AllowedSlot.ToString();
			}
			Row.InstanceId = Item.InstanceId;
			Row.DefinitionId = Item.DefinitionId;
			Row.AssignedKnightId = Item.AssignedKnightId;
			Row.Category = FName(*StaticEnum<EKnightItemCategory>()->GetNameStringByValue(static_cast<int64>(Metadata.Category)));
			Row.Subtype = FName(*StaticEnum<EKnightEquipmentSubtype>()->GetNameStringByValue(static_cast<int64>(Metadata.Subtype)));
			Row.Rarity = FName(*StaticEnum<EKnightItemRarity>()->GetNameStringByValue(static_cast<int64>(Metadata.Rarity)));
			Row.LocationId = Item.LocationId;
			Row.SlotId = Item.SlotTag.GetTagName();
			Row.ItemRevision = Item.ItemRevision;
			Row.Count = Item.Count;
			Row.ItemLevel = Definition->GetEffectiveItemLevel(Item.UpgradeRank);
			Row.RequiredLevel = Metadata.RequiredLevel;
			Row.UpgradeRank = Item.UpgradeRank;
			Row.AttachedRuneId = Item.RuneAttachmentId;
			Row.BuyPrice = Metadata.BuyPrice;
			Row.SellPrice = Metadata.SellPrice;
			Row.Name = Definition->GetDisplayName();
			const FKnightGearSeed* Seed = FKnightGearCatalog::GetSeeds().FindByPredicate(
				[&Item](const FKnightGearSeed& Candidate)
				{
					return Candidate.DefinitionId == Item.DefinitionId;
				});
			Row.Badge = FText::FromName(Seed != nullptr ? Seed->CatalogId : Item.DefinitionId.PrimaryAssetName);
			Row.QualityColor = Color(Metadata.Rarity);
			Row.bUsable = Viewed.Level >= Metadata.RequiredLevel;
			Row.bCanUpgrade = Metadata.UpgradeTrack == TEXT("RareOne") && Item.UpgradeRank == 0;
			if (const UKnightRuneDefinition* Rune = Cast<UKnightRuneDefinition>(Definition))
			{
				Row.RuneId = Rune->GetAttachmentRuneId();
			}
			FText Owner = NSLOCTEXT("KnightGearUI", "SharedBag", "Shared inventory");
			for (const FKnightRPGKnightView& Knight : Party.Knights)
			{
				if (Knight.KnightId == Item.AssignedKnightId)
				{
					Owner = KnightName(Knight.HeroDefinitionId);
				}
			}
			Row.Location = FText::Format(NSLOCTEXT("KnightGearUI", "Location", "Location: {0} / {1} / {2}"),
				FText::FromName(Item.LocationId), Owner, FText::FromName(Item.SlotTag.GetTagName()));
			Row.Requirement = Row.bUsable ? NSLOCTEXT("KnightGearUI", "LevelMet", "Level requirement met") :
				FText::Format(NSLOCTEXT("KnightGearUI", "LevelBlocked", "Requires level {0}; selected Golem is level {1}"),
					FText::AsNumber(Metadata.RequiredLevel), FText::AsNumber(Viewed.Level));
			if (Metadata.Category == EKnightItemCategory::AttachRune)
			{
				FText RuneReason;
				if (!FKnightSkillCatalog::IsRuneCompatible(Viewed.Build.SpecId, Row.RuneId))
				{
					RuneReason = NSLOCTEXT("KnightGearUI", "RuneSpecBlocked", "Incompatible with the inspected specialization.");
				}
				if (Row.RuneId == TEXT("Weapon.Aftershock"))
				{
					const FKnightEquipmentAssignment* Weapon = Viewed.FindAssignment(KnightGameplayTags::Slot_Equipment_Weapon);
					const UKnightItemDefinition* WeaponDefinition = Weapon != nullptr ? Resolve(Weapon->DefinitionId) : nullptr;
					if (WeaponDefinition == nullptr || WeaponDefinition->GetMetadata().Subtype != EKnightEquipmentSubtype::Hammer)
					{
						RuneReason = NSLOCTEXT("KnightGearUI", "RuneHammerBlocked", "Requires an equipped Hammer parent; Rune remains on that weapon GUID.");
					}
				}
				if (Item.LocationId != KnightGear::InventoryLocation)
				{
					RuneReason = NSLOCTEXT("KnightGearUI", "RuneAttached", "This exact Rune instance is already attached. Detach it explicitly first.");
				}
				Row.bUsable &= RuneReason.IsEmpty();
				Row.Requirement = FText::Format(NSLOCTEXT("KnightGearUI", "RuneRequirement", "{0}. {1}"),
					Row.Requirement, RuneReason);
			}
			FText Socket = NSLOCTEXT("KnightGearUI", "NoSocket", "Rune socket: None");
			if (Metadata.Category == EKnightItemCategory::Equipment && !Metadata.SocketProfile.IsNone())
			{
				const FKnightItemInstance* Rune = Inventory.Items.FindByPredicate([&Item](const FKnightItemInstance& Candidate)
				{
					return Candidate.InstanceId == Item.RuneAttachmentId;
				});
				Socket = FText::Format(NSLOCTEXT("KnightGearUI", "Socket", "Rune socket: {0}; attached: {1} ({2}). Rune follows this weapon GUID and does not transfer to a replacement weapon."),
					FText::FromName(Metadata.SocketProfile), Rune != nullptr ? Name(Rune->DefinitionId) : NSLOCTEXT("KnightGearUI", "EmptySocket", "Empty"),
					Rune != nullptr ? FText::FromString(Rune->InstanceId.ToString()) : FText());
			}
			const FText Upgrade = Row.bCanUpgrade ? NSLOCTEXT("KnightGearUI", "UpgradeCost", "Rank0→1:20 currency +4 M01 +2 M02 +1 M03. iLvl20→22; positive flat stats round-half-up×1.10, percentage points/passive coefficients unchanged.") :
				NSLOCTEXT("KnightGearUI", "NoUpgrade", "No further upgrade in this demo.");
			Row.Tooltip = FText::Format(NSLOCTEXT("KnightGearUI", "Tooltip",
				"{0} · {1}\nCategory {2}; subtype {3}; slot {4}\niLvl {5}; RequiredLevel {6}; inspected for {7} Lv{8}\n{9}\n{10}\n{11}\n{12}\nUnique equipped group {13}, limit {14} per Knight\nUpgrade rank +{15}. {16}\nSource receipt {17}\nSell value {18} each; equipped/socketed items cannot be sold. No buyback.\nRole hint (not a requirement): {19}\n{20}"),
				Row.Name, FText::FromName(Row.Rarity), FText::FromName(Row.Category), FText::FromName(Row.Subtype),
				FText::FromString(AllowedSlots.IsEmpty() ? TEXT("None") : AllowedSlots), Row.ItemLevel > 0 ? FText::AsNumber(Row.ItemLevel) : NSLOCTEXT("KnightGearUI", "ItemLevelNone", "None"),
				FText::AsNumber(Row.RequiredLevel), KnightName(Viewed.HeroDefinitionId), FText::AsNumber(Viewed.Level),
				Row.Requirement, Stats(Definition->GetEffectiveStats(Item.UpgradeRank)), Effect(Metadata.EquipEffectId), Socket,
				FText::FromName(Metadata.UniqueEquipGroup.PrimaryAssetName), FText::AsNumber(Metadata.UniqueEquipLimit),
				FText::AsNumber(Item.UpgradeRank), Upgrade, Item.RewardReceiptId.IsValid() ? FText::FromString(Item.RewardReceiptId.ToString()) :
				NSLOCTEXT("KnightGearUI", "UnknownSource", "Acquisition receipt unavailable"), Metadata.SellPrice > 0 ?
				FText::AsNumber(Metadata.SellPrice) : NSLOCTEXT("KnightGearUI", "Unsellable", "Unsellable"), Metadata.RoleHint, Metadata.Description);
			FText Use;
			if (Metadata.Category == EKnightItemCategory::Consumable)
			{
				Use = Metadata.SocketProfile == TEXT("Health")
					? NSLOCTEXT("KnightGearUI", "HealthUse", "Use: self heal20% MaxHP while living and injured; no Power/Versatility/crit or healing proc. Shared potion cooldown30s per Knight.")
					: NSLOCTEXT("KnightGearUI", "ResourceUse", "Use: restore30 class resource while living and below maximum. Shared potion cooldown30s per Knight; preserves regen delay and all skill cooldowns.");
			}
			else if (Metadata.Category == EKnightItemCategory::AttachRune)
			{
				Use = Row.RuneId == TEXT("Weapon.Aftershock")
					? NSLOCTEXT("KnightGearUI", "AftershockUse", "Weapon Rune: heavy Hammer hit adds0.3Power Physical in2m, max3 targets, Poise5, ICD4s; no chaining.")
					: Row.RuneId == TEXT("Skill.Convergence")
					? NSLOCTEXT("KnightGearUI", "ConvergenceUse", "Skill Rune: Uranus/Neptune/Saturn/Mars A2 cone becomes primary-only, raw direct damage x1.20; Mars Burn uses that modified raw once.")
					: Row.RuneId == TEXT("Skill.Propagation")
					? NSLOCTEXT("KnightGearUI", "PropagationUse", "Skill Rune: Venus A2 or Mercury/Terra A3 primary heal x0.85; another ally within6m receives30% of primary pre-clamp heal. Secondary cannot proc.")
					: NSLOCTEXT("KnightGearUI", "MobileUse", "Skill Rune: Terra A2 fixed5m/6s area follows selected ally at3m/4s; tick cadence, cost and cooldown preserved.");
				Use = FText::Format(NSLOCTEXT("KnightGearUI", "RuneWorldDistinction", "{0}\nAttach Rune modifies equipped weapon or skill. Runestone is a separate world interaction and is not an inventory item."), Use);
			}
			else if (Metadata.bUniqueOwned)
			{
				Use = NSLOCTEXT("KnightGearUI", "UniqueOwned", "Unique-owned per party. Quest token exposes acquisition facts; it does not complete a quest in this demo.");
			}
			Row.Tooltip = FText::Format(NSLOCTEXT("KnightGearUI", "IdentityTooltip",
				"[{0}] {1}\nInstance GUID: {2}\nParent weapon GUID: {3}\nVendor buy price: {4}\n{5}"), Row.Badge, Row.Tooltip,
				Item.InstanceId.IsValid() ? FText::FromString(Item.InstanceId.ToString()) : NSLOCTEXT("KnightGearUI", "StockDefinition", "Vendor stock definition"),
				Item.ParentWeaponId.IsValid() ? FText::FromString(Item.ParentWeaponId.ToString()) : NSLOCTEXT("KnightGearUI", "NoParent", "None"),
				FText::AsNumber(Metadata.BuyPrice), Use);
			return Row;
		}
		void AppendStats(FKnightGearCompare& Result, const AKnightCharacter* Knight, const FKnightRPGKnightView& Viewed,
			const FKnightItemStatBonuses& Proposed, const FText& Prefix = FText())
		{
			if (Knight == nullptr || Knight->GetKnightAttributeSet() == nullptr || Knight->GetKnightAbilitySystem() == nullptr)
			{
				return;
			}
			const UKnightAttributeSet* Attributes = Knight->GetKnightAttributeSet();
			const UKnightAbilitySystemComponent* ASC = Knight->GetKnightAbilitySystem();
			const UKnightSkillRuntimeComponent* Runtime = Knight->FindComponentByClass<UKnightSkillRuntimeComponent>();
			const float Crystal = Runtime != nullptr ? Runtime->GetPowerCrystalMultiplier() : 1.0f;
			auto Add = [&Result, &Prefix](const FText& NameText, float Before, float After, float RawAfter, float Cap, bool bPercent)
			{
				FKnightGearStatRow Row;
				Row.Name = Prefix.IsEmpty() ? NameText : FText::Format(NSLOCTEXT("KnightGearUI", "StatOwner", "{0}: {1}"), Prefix, NameText);
				Row.Before = bPercent ? FText::AsPercent(Before) : FText::AsNumber(Before);
				Row.After = bPercent ? FText::AsPercent(After) : FText::AsNumber(FMath::Clamp(After, 0.0f, Cap));
				Row.Contribution = FText::Format(NSLOCTEXT("KnightGearUI", "RawCap", "Raw {0}; cap {1}; overflow {2}"),
					bPercent ? FText::AsPercent(RawAfter) : FText::AsNumber(RawAfter), bPercent ? FText::AsPercent(Cap) : FText::AsNumber(Cap),
					bPercent ? FText::AsPercent(FMath::Max(0.0f, RawAfter - Cap)) : FText::AsNumber(FMath::Max(0.0f, RawAfter - Cap)));
				Result.Stats.Add(Row);
			};
			Add(NSLOCTEXT("KnightGearUI", "Health", "Max HP"), Attributes->GetMaxHealth(), Attributes->GetMaxHealth() + Proposed.MaxHealth - Viewed.StatBonuses.MaxHealth,
				Attributes->GetMaxHealth() + Proposed.MaxHealth - Viewed.StatBonuses.MaxHealth, 1000000.0f, false);
			Add(NSLOCTEXT("KnightGearUI", "Power", "Power"), Attributes->GetAttackPower(), Attributes->GetAttackPower() + (Proposed.AttackPower - Viewed.StatBonuses.AttackPower) * Crystal,
				Attributes->GetAttackPower() + (Proposed.AttackPower - Viewed.StatBonuses.AttackPower) * Crystal, 1000000.0f, false);
			Add(NSLOCTEXT("KnightGearUI", "Armor", "Armor"), Attributes->GetDefense(), Attributes->GetDefense() + Proposed.Defense - Viewed.StatBonuses.Defense,
				Attributes->GetDefense() + Proposed.Defense - Viewed.StatBonuses.Defense, 1000000.0f, false);
			Add(NSLOCTEXT("KnightGearUI", "Stamina", "Max Stamina"), Attributes->GetMaxStamina(), Attributes->GetMaxStamina() + Proposed.MaxStamina - Viewed.StatBonuses.MaxStamina,
				Attributes->GetMaxStamina() + Proposed.MaxStamina - Viewed.StatBonuses.MaxStamina, 1000000.0f, false);
			Add(NSLOCTEXT("KnightGearUI", "Resource", "Max class resource"), Attributes->GetMaxRuneEnergy(), Attributes->GetMaxRuneEnergy() + Proposed.MaxRuneEnergy - Viewed.StatBonuses.MaxRuneEnergy,
				Attributes->GetMaxRuneEnergy() + Proposed.MaxRuneEnergy - Viewed.StatBonuses.MaxRuneEnergy, 1000000.0f, false);
			if (const UKnightCombatComponent* Combat = Knight->FindComponentByClass<UKnightCombatComponent>())
			{
				const float Before = Combat->GetMaxPoise();
				const float After = Before + Proposed.Poise - Viewed.StatBonuses.Poise;
				Add(NSLOCTEXT("KnightGearUI", "Poise", "Max Poise"), Before, After, After, 1000000.0f, false);
			}
			auto Secondary = [&Add, ASC](const FText& Label, const FGameplayAttribute& Attribute, float Current,
				float Bonus, float Cap)
			{
				const float Raw = ASC->GetNumericAttributeBase(Attribute) + Bonus;
				Add(Label, Current, FMath::Clamp(Raw, 0.0f, Cap), Raw, Cap, true);
			};
			Secondary(NSLOCTEXT("KnightGearUI", "Crit", "Crit (direct damage)"), UKnightAttributeSet::GetCritChanceAttribute(), Attributes->GetCritChance(), Proposed.CritChance, 0.4f);
			Secondary(NSLOCTEXT("KnightGearUI", "Haste", "Haste"), UKnightAttributeSet::GetHasteAttribute(), Attributes->GetHaste(), Proposed.Haste, 0.3f);
			Secondary(NSLOCTEXT("KnightGearUI", "CDR", "Cooldown recovery"), UKnightAttributeSet::GetCooldownRecoveryAttribute(), Attributes->GetCooldownRecovery(), Proposed.CooldownRecovery, 0.3f);
			Secondary(NSLOCTEXT("KnightGearUI", "Mastery", "Mastery"), UKnightAttributeSet::GetMasteryBonusAttribute(), Attributes->GetMasteryBonus(), Proposed.MasteryBonus, 0.3f);
			Secondary(NSLOCTEXT("KnightGearUI", "Versatility", "Versatility"), UKnightAttributeSet::GetVersatilityAttribute(), Attributes->GetVersatility(), Proposed.Versatility, 0.2f);
		}

	}

	TArray<FKnightGearQuickUseRow> QuickUses(const FKnightRPGKnightView& Knight,
		const FKnightInventoryView& Inventory, const AKnightCharacter* Actor)
	{
		TArray<FKnightGearQuickUseRow> Rows;
		const UKnightSkillRuntimeComponent* Runtime = Actor != nullptr ? Actor->FindComponentByClass<UKnightSkillRuntimeComponent>() : nullptr;
		for (int32 Index = 0; Index < 2; ++Index)
		{
			FKnightGearQuickUseRow Row;
			if (Knight.ConsumableBindings.IsValidIndex(Index))
			{
				Row.DefinitionId = Knight.ConsumableBindings[Index];
			}
			Row.Name = Row.DefinitionId.IsValid() ? Name(Row.DefinitionId) : NSLOCTEXT("KnightGearUI", "Unbound", "Unbound");
			for (const FKnightItemInstance& Item : Inventory.Items)
			{
				if (Item.DefinitionId == Row.DefinitionId && Item.LocationId == KnightGear::InventoryLocation)
				{
					Row.Count += Item.Count;
				}
			}
			Row.CooldownEndTime = Runtime != nullptr ? static_cast<float>(Runtime->GetSkillState().ConsumableEnd) : 0.0f;
			Rows.Add(Row);
		}
		return Rows;
	}

	void Enrich(FKnightRPGPanelSnapshot& Snapshot, const FKnightRPGView& Party,
		const FKnightInventoryView& Inventory, const AKnightCharacter* Knight, const APlayerController* Caller,
		bool bAtSanctuary, const FKnightGearDemoSnapshot* Demo)
	{
		if (!Party.IsItemizationEnabled() || Knight == nullptr)
		{
			return;
		}
		const FKnightRPGKnightView* Viewed = Party.Knights.FindByPredicate([Knight](const FKnightRPGKnightView& Entry)
		{
			return Entry.KnightId == Knight->GetKnightEntityId();
		});
		if (Viewed == nullptr || !Viewed->bGearEnabled)
		{
			return;
		}
		FKnightGearPanelSnapshot& Gear = Snapshot.Gear;
		Gear.bEnabled = true;
		Gear.KnightId = Viewed->KnightId;
		Gear.KnightName = KnightName(Viewed->HeroDefinitionId);
		Gear.Level = Viewed->Level;
		Gear.Currency = Party.Currency;
		Gear.bHasDemoState = Party.bGearDemo && Demo != nullptr;
		Gear.InventoryRevision = Inventory.Revision;
		Gear.KnightRevision = Viewed->Revision;
		Gear.SkillRuneInstanceId = Viewed->SkillRuneInstanceId;
		auto RuneName = [&Inventory](const FGuid& InstanceId)
		{
			const FKnightItemInstance* Item = Inventory.Items.FindByPredicate([&InstanceId](const FKnightItemInstance& Entry)
			{
				return Entry.InstanceId == InstanceId;
			});
			return Item != nullptr ? Name(Item->DefinitionId) : NSLOCTEXT("KnightGearUI", "NoAttachment", "Empty");
		};
		Gear.Attachments = FText::Format(NSLOCTEXT("KnightGearUI", "Attachments",
			"Attachments outside12 slots: Weapon {0}; Skill {1}. Inspected specialization {2}."),
			RuneName(Viewed->WeaponRuneInstanceId), RuneName(Viewed->SkillRuneInstanceId), FText::FromName(Viewed->Build.SpecId));
		Gear.bCanEdit = bAtSanctuary && Knight->GetLifeComponent()->IsAlive() &&
			(Knight->GetController() == Caller || Knight->GetController() == nullptr || !Knight->GetController()->IsPlayerController());
		Gear.bCanEdit &= Knight->CanTransferControl();
		if (const UKnightSkillRuntimeComponent* Runtime = Knight->FindComponentByClass<UKnightSkillRuntimeComponent>())
		{
			Gear.bCanEdit &= !Runtime->HasDeferredDamage();
			const FKnightSkillRuntimeState State = Runtime->GetSkillState();
			Gear.RuntimeRevision = State.Revision;
			const AGameStateBase* GameState = Knight->GetWorld()->GetGameState();
			const double Now = GameState != nullptr ? GameState->GetServerWorldTimeSeconds() : Knight->GetWorld()->GetTimeSeconds();
			auto Cooldown = [Now](double End)
			{
				return End > Now ? FText::Format(NSLOCTEXT("KnightGearUI", "ICDRemaining", "{0}s cooldown"),
					FText::AsNumber(FMath::CeilToInt(End - Now))) : NSLOCTEXT("KnightGearUI", "ICDReady", "Ready");
			};
			Gear.RelicStatus = FText::Format(NSLOCTEXT("KnightGearUI", "RelicState",
				"Relic cooldowns: Bulwark {0}; Lifeseed {1}; Starfall {2}. Starfall counter {3}/3 ({4}s remaining). Cooldowns survive unequip/takeover."),
				Cooldown(State.BulwarkEnd), Cooldown(State.LifeseedEnd), Cooldown(State.StarfallEnd),
				FText::AsNumber(State.StarfallCount), FText::AsNumber(FMath::Max(0, FMath::CeilToInt(State.StarfallCounterEnd - Now))));
		}
		Gear.bCanUseVendor = Gear.bCanEdit;
		Gear.bCanUpgrade = Gear.bCanEdit;
		Gear.bCanRunFixtures = Gear.bHasDemoState && bAtSanctuary && Caller != nullptr && Caller->HasAuthority();
		Gear.EditLockReason = Gear.bCanEdit ? FText() : NSLOCTEXT("KnightGearUI", "EditLock", "Sanctuary, living, idle and equipment ownership are required. Other human Knights can be inspected but not edited.");
		for (const FKnightRPGKnightView& Entry : Party.Knights)
		{
			FKnightGearKnightRow Row;
			Row.KnightId = Entry.KnightId;
			Row.Name = KnightName(Entry.HeroDefinitionId);
			Row.Level = Entry.Level;
			Row.Revision = Entry.Revision;
			Gear.Knights.Add(Row);
		}
		for (FGameplayTag Slot : KnightGear::EquipmentSlots())
		{
			FKnightGearSlotRow Row;
			Row.SlotId = Slot.GetTagName();
			Row.Name = FText::FromString(Slot.ToString().Replace(TEXT("Equipment.Slot."), TEXT("")));
			const FKnightEquipmentAssignment* Assignment = Viewed->FindAssignment(Slot);
			if (Assignment != nullptr)
			{
				Row.InstanceId = Assignment->ItemInstanceId;
				Row.ItemName = Name(Assignment->DefinitionId);
				const FKnightItemInstance* Item = Inventory.Items.FindByPredicate([Assignment](const FKnightItemInstance& Candidate)
				{
					return Candidate.InstanceId == Assignment->ItemInstanceId;
				});
				if (Item != nullptr)
				{
					const FKnightGearItemRow ItemView = ItemRow(*Item, Party, Inventory, *Viewed);
					Row.Tooltip = ItemView.Tooltip;
					Row.Badge = ItemView.Badge;
					Row.QualityColor = ItemView.QualityColor;
				}
			}
			else
			{
				Row.Tooltip = FText::Format(NSLOCTEXT("KnightGearUI", "EmptyTooltip", "{0}: Empty. Select an inventory item and this exact slot to compare/equip."), Row.Name);
			}
			Gear.Slots.Add(Row);
		}
		for (const FKnightItemInstance& Item : Inventory.Items)
		{
			FKnightGearItemRow Row = ItemRow(Item, Party, Inventory, *Viewed);
			if (Row.DefinitionId.IsValid())
			{
				Gear.Items.Add(Row);
			}
		}
		for (const FKnightGearSeed& Seed : FKnightGearCatalog::GetSeeds())
		{
			if (!Gear.bHasDemoState || Seed.Metadata.BuyPrice <= 0)
			{
				continue;
			}
			FKnightItemInstance Item;
			Item.DefinitionId = Seed.DefinitionId;
			Item.Count = 1;
			FKnightGearItemRow Row = ItemRow(Item, Party, Inventory, *Viewed);
			Row.bVendorStock = true;
			Gear.VendorStock.Add(Row);
		}
		Gear.QuickUses = QuickUses(*Viewed, Inventory, Knight);
		if (Gear.bHasDemoState)
		{
			Gear.bCatalogLab = Demo->bCatalogLab;
			Gear.ClaimableRewardId = Demo->ClaimableRewardId;
			Gear.FixtureSummary = FText::Format(NSLOCTEXT("KnightGearUI", "FixtureMode", "{0} · {1} · completed encounters {2}/3 · Run {3}"),
				Demo->bCatalogLab ? NSLOCTEXT("KnightGearUI", "CatalogMode", "DEV ONLY Catalog Lab") : NSLOCTEXT("KnightGearUI", "ProgressionMode", "Progression Demo fixture"),
				Demo->StageLabel, FText::AsNumber(Demo->CompletedEncounters), FText::FromString(Demo->DemoRunId.ToString()));
			Snapshot.Objective = Demo->StageLabel;
		}
	}

	FKnightGearCompare Compare(const UKnightRPGComponent* RPG, const FKnightInventoryView& Inventory,
		const AKnightCharacter* Knight, const FKnightGearPanelIntent& Intent,
		const AKnightCharacter* SourceKnight, const AKnightCharacter* TargetKnight)
	{
		FKnightGearCompare Result;
		Result.InstanceId = Intent.InstanceId;
		Result.KnightId = Intent.KnightId;
		Result.SlotId = Intent.SlotId;
		if (RPG == nullptr || Knight == nullptr)
		{
			return Result;
		}
		const FKnightRPGKnightView Viewed = RPG->GetKnightView(Intent.KnightId);
		const FKnightItemInstance* Item = Inventory.Items.FindByPredicate([&Intent](const FKnightItemInstance& Candidate)
		{
			return Candidate.InstanceId == Intent.InstanceId;
		});
		if (Item == nullptr)
		{
			return Result;
		}
		const UKnightItemDefinition* Definition = Resolve(Item->DefinitionId);
		if (Definition == nullptr)
		{
			return Result;
		}
		FKnightItemStatBonuses Proposed = Viewed.StatBonuses;
		FKnightResult Validation = FKnightResult::Success();
		if (Intent.Action == EKnightGearPanelAction::Transfer)
		{
			FKnightItemStatBonuses SourceBonuses;
			FKnightItemStatBonuses TargetBonuses;
			Validation = RPG->PreviewTransfer(Item->AssignedKnightId, Intent.TargetKnightId, Item->InstanceId,
				FGameplayTag::RequestGameplayTag(Intent.SlotId, false), SourceBonuses, TargetBonuses);
			Result.bValid = Validation.IsSuccess() && SourceKnight != nullptr && TargetKnight != nullptr;
			Result.RejectionReason = Validation.Message;
			if (Result.bValid)
			{
				const FKnightRPGKnightView Source = RPG->GetKnightView(Item->AssignedKnightId);
				const FKnightRPGKnightView Target = RPG->GetKnightView(Intent.TargetKnightId);
				AppendStats(Result, SourceKnight, Source, SourceBonuses, KnightName(Source.HeroDefinitionId));
				AppendStats(Result, TargetKnight, Target, TargetBonuses, KnightName(Target.HeroDefinitionId));
				Result.Summary = FText::Format(NSLOCTEXT("KnightGearUI", "TransferCompare",
					"Atomic transfer {0} → {1}, target {2}. This item's GUID and attached Rune stay together. Displaced destination equipment returns to the shared bag. Both confirmed Knights stay unchanged until Apply."),
					KnightName(Source.HeroDefinitionId), KnightName(Target.HeroDefinitionId), FText::FromName(Intent.SlotId));
			}
			return Result;
		}
		if (Intent.Action == EKnightGearPanelAction::Equip && Definition->GetMetadata().Category == EKnightItemCategory::Equipment)
		{
			Validation = RPG->PreviewEquip(Intent.KnightId, Intent.InstanceId,
				FGameplayTag::RequestGameplayTag(Intent.SlotId, false), Proposed);
		}
		else if (Intent.Action == EKnightGearPanelAction::Unequip && Item->AssignedKnightId == Viewed.KnightId)
		{
			const FKnightItemStatBonuses Removed = Definition->GetEffectiveStats(Item->UpgradeRank);
			Proposed.MaxHealth -= Removed.MaxHealth;
			Proposed.MaxStamina -= Removed.MaxStamina;
			Proposed.MaxRuneEnergy -= Removed.MaxRuneEnergy;
			Proposed.AttackPower -= Removed.AttackPower;
			Proposed.Defense -= Removed.Defense;
			Proposed.Poise -= Removed.Poise;
			Proposed.CritChance -= Removed.CritChance;
			Proposed.Haste -= Removed.Haste;
			Proposed.CooldownRecovery -= Removed.CooldownRecovery;
			Proposed.MasteryBonus -= Removed.MasteryBonus;
			Proposed.Versatility -= Removed.Versatility;
		}
		else if (Intent.Action == EKnightGearPanelAction::Upgrade && Item->AssignedKnightId == Viewed.KnightId)
		{
			const FKnightItemStatBonuses Before = Definition->GetEffectiveStats(Item->UpgradeRank);
			const FKnightItemStatBonuses After = Definition->GetEffectiveStats(Item->UpgradeRank + 1);
			Proposed.MaxHealth += After.MaxHealth - Before.MaxHealth;
			Proposed.MaxStamina += After.MaxStamina - Before.MaxStamina;
			Proposed.MaxRuneEnergy += After.MaxRuneEnergy - Before.MaxRuneEnergy;
			Proposed.AttackPower += After.AttackPower - Before.AttackPower;
			Proposed.Defense += After.Defense - Before.Defense;
			Proposed.Poise += After.Poise - Before.Poise;
		}
		Result.bValid = Validation.IsSuccess();
		Result.RejectionReason = Validation.Message;
		if (!Validation.IsSuccess())
		{
			Proposed = Viewed.StatBonuses;
		}
		AppendStats(Result, Knight, Viewed, Proposed);
		Result.Summary = FText::Format(NSLOCTEXT("KnightGearUI", "CompareSummary",
			"Pure comparison for {0}. Current HP/resource stays unchanged except clamping on confirmation.\n{1}\n{2}"),
			KnightName(Viewed.HeroDefinitionId), Definition->GetMetadata().Subtype == EKnightEquipmentSubtype::Hammer ?
			NSLOCTEXT("KnightGearUI", "RuneRetention", "Weapon replacement: the old weapon's Rune remains attached to its old GUID; it is not moved to the new weapon.") : FText(),
			Effect(Definition->GetMetadata().EquipEffectId));
		if (Intent.Action == EKnightGearPanelAction::Upgrade)
		{
			Result.Summary = FText::Format(NSLOCTEXT("KnightGearUI", "UpgradeCompare",
				"{0}\nITEM rank +{1} → +{2}:\nBefore:\n{3}\nAfter:\n{4}\nCost:20 currency +4 M01 +2 M02 +1 M03. Keeps GUID, rarity, requirements and attached Rune. Unequipped upgrades keep the inspected Knight totals unchanged."),
				Result.Summary, FText::AsNumber(Item->UpgradeRank), FText::AsNumber(Item->UpgradeRank + 1),
				Stats(Definition->GetEffectiveStats(Item->UpgradeRank)), Stats(Definition->GetEffectiveStats(Item->UpgradeRank + 1)));
		}
		const UKnightRuneDefinition* Rune = Cast<UKnightRuneDefinition>(Definition);
		const bool bDetachSkillRune = Intent.Action == EKnightGearPanelAction::DetachSkillRune;
		if (Rune != nullptr && !bDetachSkillRune)
		{
			const FKnightGearItemRow RuneView = ItemRow(*Item, RPG->GetView(), Inventory, Viewed);
			if (!RuneView.bUsable)
			{
				Result.bValid = false;
				Result.RejectionReason = RuneView.Requirement;
			}
		}
		if (bDetachSkillRune || (Rune != nullptr && Rune->GetAttachmentRuneId() != TEXT("Weapon.Aftershock")))
		{
			FKnightSkillBuild ProposedBuild = Viewed.Build;
			ProposedBuild.SkillRune = bDetachSkillRune ? NAME_None : Rune->GetAttachmentRuneId();
			if (bDetachSkillRune)
			{
				const FKnightItemInstance* Attached = Inventory.Items.FindByPredicate([&Viewed](const FKnightItemInstance& Candidate)
				{
					return Candidate.InstanceId == Viewed.SkillRuneInstanceId;
				});
				Result.bValid = Attached != nullptr && !Viewed.Build.SkillRune.IsNone();
				Result.RejectionReason = Result.bValid ? FText() :
					NSLOCTEXT("KnightGearUI", "NoSkillRuneToDetach", "The inspected Golem has no confirmed skill Rune to detach.");
				Result.Summary = FText::Format(NSLOCTEXT("KnightGearUI", "SkillDetachSummary",
					"Pure detachment comparison for {0}: remove {1} (instance {2}). Equipment and the weapon Rune stay unchanged."),
					KnightName(Viewed.HeroDefinitionId), Attached != nullptr ? Name(Attached->DefinitionId) : FText(),
					FText::FromString(Viewed.SkillRuneInstanceId.ToString()));
			}
			FString SkillChanges;
			for (int32 Index = 1; Index <= 4; ++Index)
			{
				const FKnightSpecActionDefinition Before = FKnightSkillCatalog::GetSkill(Viewed.Build, Index);
				const FKnightSpecActionDefinition After = FKnightSkillCatalog::GetSkill(ProposedBuild, Index);
				SkillChanges += FString::Printf(TEXT("A%d: coefficient %g→%g, cost %g→%g, CD %gs→%gs, duration %gs→%gs; %s→%s, targets %d→%d.\n"),
					Index, Before.Coefficient, After.Coefficient, Before.Cost, After.Cost, Before.Cooldown, After.Cooldown,
					Before.Duration, After.Duration, *StaticEnum<EKnightSkillTargeting>()->GetNameStringByValue(static_cast<int64>(Before.Targeting)),
					*StaticEnum<EKnightSkillTargeting>()->GetNameStringByValue(static_cast<int64>(After.Targeting)), Before.MaxTargets, After.MaxTargets);
			}
			Result.Summary = FText::Format(NSLOCTEXT("KnightGearUI", "SkillRuneCompare", "{0}\nSkill Rune comparison:\n{1}"),
				Result.Summary, FText::FromString(SkillChanges));
		}
		return Result;
	}
}
