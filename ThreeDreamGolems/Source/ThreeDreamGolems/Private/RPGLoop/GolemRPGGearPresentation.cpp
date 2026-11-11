#include "RPGLoop/GolemRPGGearPresentation.h"

#include "Assets/GolemAssetManager.h"
#include "Attributes/GolemAttributeSet.h"
#include "Catalog/GolemGearCatalog.h"
#include "Character/GolemCharacter.h"
#include "Components/GolemAbilitySystemComponent.h"
#include "Components/GolemCombatComponent.h"
#include "Components/GolemLifeComponent.h"
#include "Definitions/GolemCombatProfileDefinition.h"
#include "Definitions/GolemItemDefinition.h"
#include "Definitions/GolemRuneDefinition.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/GameStateBase.h"
#include "Engine/World.h"
#include "GearLab/GolemGearLabState.h"
#include "Progression/GolemRPGComponent.h"
#include "Skills/GolemSkillRuntimeComponent.h"
#include "Tags/GolemGameplayTags.h"

namespace GolemRPGGearPresentation
{
	namespace
	{
		const UGolemItemDefinition* Resolve(const FPrimaryAssetId& Id)
		{
			UGolemDefinition* Definition = nullptr;
			UGolemAssetManager* Manager = UGolemAssetManager::GetGolemAssetManager();
			return Manager != nullptr && Manager->ResolveDefinition(Id, UGolemItemDefinition::StaticClass(), Definition).IsSuccess()
				? Cast<UGolemItemDefinition>(Definition) : nullptr;
		}

		FText Name(const FPrimaryAssetId& Id)
		{
			const UGolemItemDefinition* Definition = Resolve(Id);
			return Definition != nullptr ? Definition->GetDisplayName() : FText::FromName(Id.PrimaryAssetName);
		}

		FText GolemName(const FPrimaryAssetId& Id)
		{
			if (Id.PrimaryAssetName == TEXT("Training_Red"))
			{
				return NSLOCTEXT("GolemGearUI", "Sky", "Sky");
			}
			if (Id.PrimaryAssetName == TEXT("Training_Blue"))
			{
				return NSLOCTEXT("GolemGearUI", "Sea", "Sea");
			}
			if (Id.PrimaryAssetName == TEXT("Training_Green"))
			{
				return NSLOCTEXT("GolemGearUI", "Earth", "Earth");
			}
			return FText::FromName(Id.PrimaryAssetName);
		}

		FText Stats(const FGolemItemStatBonuses& Bonus)
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
				return NSLOCTEXT("GolemGearUI", "Bulwark", "Equip: eligible hostile direct block/parry grants self shield 0.60×Power for5s; ICD12s. No Mastery/Versatility/crit. Family ICD survives re-equip and possession.");
			}
			if (EffectId == TEXT("Lifeseed"))
			{
				return NSLOCTEXT("GolemGearUI", "Lifeseed", "Equip: primary direct ability heal with actual healing>0 applies0.15×Power HoT/s for3s, first tick+1s; ICD10s. Snapshots outgoing Versatility; no Mastery/crit/secondary/potion trigger.");
			}
			if (EffectId == TEXT("Starfall"))
			{
				return NSLOCTEXT("GolemGearUI", "Starfall", "Equip: third eligible hostile direct activation deals0.40×Power secondary damage to primary target, original school; ICD8s, counter expires after10s. No crit/proc/reaction/poise/durability. Target defense still applies.");
			}
			return NSLOCTEXT("GolemGearUI", "NoEquipEffect", "Equip effect: None");
		}

		FLinearColor Color(EGolemItemRarity Rarity)
		{
			switch (Rarity)
			{
			case EGolemItemRarity::Poor: return FLinearColor(0.65f, 0.65f, 0.65f);
			case EGolemItemRarity::Uncommon: return FLinearColor(0.3f, 0.95f, 0.35f);
			case EGolemItemRarity::Rare: return FLinearColor(0.35f, 0.65f, 1.0f);
			case EGolemItemRarity::Epic: return FLinearColor(0.8f, 0.5f, 1.0f);
			default: return FLinearColor::White;
			}
		}

		FGolemGearItemRow ItemRow(const FGolemItemInstance& Item, const FGolemRPGView& Party,
			const FGolemInventoryView& Inventory, const FGolemRPGGolemView& Viewed)
		{
			FGolemGearItemRow Row;
			const UGolemItemDefinition* Definition = Resolve(Item.DefinitionId);
			if (Definition == nullptr)
			{
				return Row;
			}
			const FGolemItemMetadata& Metadata = Definition->GetMetadata();
			FString AllowedSlots;
			for (const FGameplayTag& AllowedSlot : Metadata.AllowedSlots)
			{
				AllowedSlots += AllowedSlots.IsEmpty() ? AllowedSlot.ToString() : TEXT(", ") + AllowedSlot.ToString();
			}
			Row.InstanceId = Item.InstanceId;
			Row.DefinitionId = Item.DefinitionId;
			Row.AssignedGolemId = Item.AssignedGolemId;
			Row.Category = FName(*StaticEnum<EGolemItemCategory>()->GetNameStringByValue(static_cast<int64>(Metadata.Category)));
			Row.Subtype = FName(*StaticEnum<EGolemEquipmentSubtype>()->GetNameStringByValue(static_cast<int64>(Metadata.Subtype)));
			Row.Rarity = FName(*StaticEnum<EGolemItemRarity>()->GetNameStringByValue(static_cast<int64>(Metadata.Rarity)));
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
			const FGolemGearSeed* Seed = FGolemGearCatalog::GetSeeds().FindByPredicate(
				[&Item](const FGolemGearSeed& Candidate)
				{
					return Candidate.DefinitionId == Item.DefinitionId;
				});
			Row.Badge = FText::FromName(Seed != nullptr ? Seed->CatalogId : Item.DefinitionId.PrimaryAssetName);
			Row.QualityColor = Color(Metadata.Rarity);
			Row.bUsable = Viewed.Level >= Metadata.RequiredLevel;
			Row.bCanUpgrade = Metadata.UpgradeTrack == TEXT("RareOne") && Item.UpgradeRank == 0;
			if (const UGolemRuneDefinition* Rune = Cast<UGolemRuneDefinition>(Definition))
			{
				Row.RuneId = Rune->GetAttachmentRuneId();
			}
			FText Owner = NSLOCTEXT("GolemGearUI", "SharedBag", "Shared inventory");
			for (const FGolemRPGGolemView& Golem : Party.Golems)
			{
				if (Golem.GolemId == Item.AssignedGolemId)
				{
					Owner = GolemName(Golem.HeroDefinitionId);
				}
			}
			Row.Location = FText::Format(NSLOCTEXT("GolemGearUI", "Location", "Location: {0} / {1} / {2}"),
				FText::FromName(Item.LocationId), Owner, FText::FromName(Item.SlotTag.GetTagName()));
			Row.Requirement = Row.bUsable ? NSLOCTEXT("GolemGearUI", "LevelMet", "Level requirement met") :
				FText::Format(NSLOCTEXT("GolemGearUI", "LevelBlocked", "Requires level {0}; selected Golem is level {1}"),
					FText::AsNumber(Metadata.RequiredLevel), FText::AsNumber(Viewed.Level));
			if (Metadata.Category == EGolemItemCategory::AttachRune)
			{
				FText RuneReason;
				if (!FGolemSkillCatalog::IsRuneCompatible(Viewed.Build.SpecId, Row.RuneId))
				{
					RuneReason = NSLOCTEXT("GolemGearUI", "RuneSpecBlocked", "Incompatible with the inspected specialization.");
				}
				if (Row.RuneId == TEXT("Weapon.Aftershock"))
				{
					const FGolemEquipmentAssignment* Weapon = Viewed.FindAssignment(GolemGameplayTags::Slot_Equipment_Weapon);
					const UGolemItemDefinition* WeaponDefinition = Weapon != nullptr ? Resolve(Weapon->DefinitionId) : nullptr;
					if (WeaponDefinition == nullptr || WeaponDefinition->GetMetadata().Subtype != EGolemEquipmentSubtype::Hammer)
					{
						RuneReason = NSLOCTEXT("GolemGearUI", "RuneHammerBlocked", "Requires an equipped Hammer parent; Rune remains on that weapon GUID.");
					}
				}
				if (Item.LocationId != GolemGear::InventoryLocation)
				{
					RuneReason = NSLOCTEXT("GolemGearUI", "RuneAttached", "This exact Rune instance is already attached. Detach it explicitly first.");
				}
				Row.bUsable &= RuneReason.IsEmpty();
				Row.Requirement = FText::Format(NSLOCTEXT("GolemGearUI", "RuneRequirement", "{0}. {1}"),
					Row.Requirement, RuneReason);
			}
			FText Socket = NSLOCTEXT("GolemGearUI", "NoSocket", "Rune socket: None");
			if (Metadata.Category == EGolemItemCategory::Equipment && !Metadata.SocketProfile.IsNone())
			{
				const FGolemItemInstance* Rune = Inventory.Items.FindByPredicate([&Item](const FGolemItemInstance& Candidate)
				{
					return Candidate.InstanceId == Item.RuneAttachmentId;
				});
				Socket = FText::Format(NSLOCTEXT("GolemGearUI", "Socket", "Rune socket: {0}; attached: {1} ({2}). Rune follows this weapon GUID and does not transfer to a replacement weapon."),
					FText::FromName(Metadata.SocketProfile), Rune != nullptr ? Name(Rune->DefinitionId) : NSLOCTEXT("GolemGearUI", "EmptySocket", "Empty"),
					Rune != nullptr ? FText::FromString(Rune->InstanceId.ToString()) : FText());
			}
			const FText Upgrade = Row.bCanUpgrade ? NSLOCTEXT("GolemGearUI", "UpgradeCost", "Rank0→1:20 currency +4 M01 +2 M02 +1 M03. iLvl20→22; positive flat stats round-half-up×1.10, percentage points/passive coefficients unchanged.") :
				NSLOCTEXT("GolemGearUI", "NoUpgrade", "No further upgrade in this demo.");
			Row.Tooltip = FText::Format(NSLOCTEXT("GolemGearUI", "Tooltip",
				"{0} · {1}\nCategory {2}; subtype {3}; slot {4}\niLvl {5}; RequiredLevel {6}; inspected for {7} Lv{8}\n{9}\n{10}\n{11}\n{12}\nUnique equipped group {13}, limit {14} per Golem\nUpgrade rank +{15}. {16}\nSource receipt {17}\nSell value {18} each; equipped/socketed items cannot be sold. No buyback.\nRole hint (not a requirement): {19}\n{20}"),
				Row.Name, FText::FromName(Row.Rarity), FText::FromName(Row.Category), FText::FromName(Row.Subtype),
				FText::FromString(AllowedSlots.IsEmpty() ? TEXT("None") : AllowedSlots), Row.ItemLevel > 0 ? FText::AsNumber(Row.ItemLevel) : NSLOCTEXT("GolemGearUI", "ItemLevelNone", "None"),
				FText::AsNumber(Row.RequiredLevel), GolemName(Viewed.HeroDefinitionId), FText::AsNumber(Viewed.Level),
				Row.Requirement, Stats(Definition->GetEffectiveStats(Item.UpgradeRank)), Effect(Metadata.EquipEffectId), Socket,
				FText::FromName(Metadata.UniqueEquipGroup.PrimaryAssetName), FText::AsNumber(Metadata.UniqueEquipLimit),
				FText::AsNumber(Item.UpgradeRank), Upgrade, Item.RewardReceiptId.IsValid() ? FText::FromString(Item.RewardReceiptId.ToString()) :
				NSLOCTEXT("GolemGearUI", "UnknownSource", "Acquisition receipt unavailable"), Metadata.SellPrice > 0 ?
				FText::AsNumber(Metadata.SellPrice) : NSLOCTEXT("GolemGearUI", "Unsellable", "Unsellable"), Metadata.RoleHint, Metadata.Description);
			FText Use;
			if (Metadata.Category == EGolemItemCategory::Consumable)
			{
				Use = Metadata.SocketProfile == TEXT("Health")
					? NSLOCTEXT("GolemGearUI", "HealthUse", "Use: self heal20% MaxHP while living and injured; no Power/Versatility/crit or healing proc. Shared potion cooldown30s per Golem.")
					: NSLOCTEXT("GolemGearUI", "ResourceUse", "Use: restore30 class resource while living and below maximum. Shared potion cooldown30s per Golem; preserves regen delay and all skill cooldowns.");
			}
			else if (Metadata.Category == EGolemItemCategory::AttachRune)
			{
				Use = Row.RuneId == TEXT("Weapon.Aftershock")
					? NSLOCTEXT("GolemGearUI", "AftershockUse", "Weapon Rune: heavy Hammer hit adds0.3Power Physical in2m, max3 targets, Poise5, ICD4s; no chaining.")
					: Row.RuneId == TEXT("Skill.Convergence")
					? NSLOCTEXT("GolemGearUI", "ConvergenceUse", "Skill Rune: Uranus/Neptune/Saturn/Mars A2 cone becomes primary-only, raw direct damage x1.20; Mars Burn uses that modified raw once.")
					: Row.RuneId == TEXT("Skill.Propagation")
					? NSLOCTEXT("GolemGearUI", "PropagationUse", "Skill Rune: Venus A2 or Mercury/Terra A3 primary heal x0.85; another ally within6m receives30% of primary pre-clamp heal. Secondary cannot proc.")
					: NSLOCTEXT("GolemGearUI", "MobileUse", "Skill Rune: Terra A2 fixed5m/6s area follows selected ally at3m/4s; tick cadence, cost and cooldown preserved.");
				Use = FText::Format(NSLOCTEXT("GolemGearUI", "RuneWorldDistinction", "{0}\nAttach Rune modifies equipped weapon or skill. Runestone is a separate world interaction and is not an inventory item."), Use);
			}
			else if (Metadata.bUniqueOwned)
			{
				Use = NSLOCTEXT("GolemGearUI", "UniqueOwned", "Unique-owned per party. Quest token exposes acquisition facts; it does not complete a quest in this demo.");
			}
			Row.Tooltip = FText::Format(NSLOCTEXT("GolemGearUI", "IdentityTooltip",
				"[{0}] {1}\nInstance GUID: {2}\nParent weapon GUID: {3}\nVendor buy price: {4}\n{5}"), Row.Badge, Row.Tooltip,
				Item.InstanceId.IsValid() ? FText::FromString(Item.InstanceId.ToString()) : NSLOCTEXT("GolemGearUI", "StockDefinition", "Vendor stock definition"),
				Item.ParentWeaponId.IsValid() ? FText::FromString(Item.ParentWeaponId.ToString()) : NSLOCTEXT("GolemGearUI", "NoParent", "None"),
				FText::AsNumber(Metadata.BuyPrice), Use);
			return Row;
		}
		void AppendStats(FGolemGearCompare& Result, const AGolemCharacter* Golem, const FGolemRPGGolemView& Viewed,
			const FGolemItemStatBonuses& Proposed, const FText& Prefix = FText())
		{
			if (Golem == nullptr || Golem->GetGolemAttributeSet() == nullptr || Golem->GetGolemAbilitySystem() == nullptr)
			{
				return;
			}
			const UGolemAttributeSet* Attributes = Golem->GetGolemAttributeSet();
			const UGolemAbilitySystemComponent* ASC = Golem->GetGolemAbilitySystem();
			const UGolemSkillRuntimeComponent* Runtime = Golem->FindComponentByClass<UGolemSkillRuntimeComponent>();
			const float Crystal = Runtime != nullptr ? Runtime->GetPowerCrystalMultiplier() : 1.0f;
			auto Add = [&Result, &Prefix](const FText& NameText, float Before, float After, float RawAfter, float Cap, bool bPercent)
			{
				FGolemGearStatRow Row;
				Row.Name = Prefix.IsEmpty() ? NameText : FText::Format(NSLOCTEXT("GolemGearUI", "StatOwner", "{0}: {1}"), Prefix, NameText);
				Row.Before = bPercent ? FText::AsPercent(Before) : FText::AsNumber(Before);
				Row.After = bPercent ? FText::AsPercent(After) : FText::AsNumber(FMath::Clamp(After, 0.0f, Cap));
				Row.Contribution = FText::Format(NSLOCTEXT("GolemGearUI", "RawCap", "Raw {0}; cap {1}; overflow {2}"),
					bPercent ? FText::AsPercent(RawAfter) : FText::AsNumber(RawAfter), bPercent ? FText::AsPercent(Cap) : FText::AsNumber(Cap),
					bPercent ? FText::AsPercent(FMath::Max(0.0f, RawAfter - Cap)) : FText::AsNumber(FMath::Max(0.0f, RawAfter - Cap)));
				Result.Stats.Add(Row);
			};
			Add(NSLOCTEXT("GolemGearUI", "Health", "Max HP"), Attributes->GetMaxHealth(), Attributes->GetMaxHealth() + Proposed.MaxHealth - Viewed.StatBonuses.MaxHealth,
				Attributes->GetMaxHealth() + Proposed.MaxHealth - Viewed.StatBonuses.MaxHealth, 1000000.0f, false);
			Add(NSLOCTEXT("GolemGearUI", "Power", "Power"), Attributes->GetAttackPower(), Attributes->GetAttackPower() + (Proposed.AttackPower - Viewed.StatBonuses.AttackPower) * Crystal,
				Attributes->GetAttackPower() + (Proposed.AttackPower - Viewed.StatBonuses.AttackPower) * Crystal, 1000000.0f, false);
			Add(NSLOCTEXT("GolemGearUI", "Armor", "Armor"), Attributes->GetDefense(), Attributes->GetDefense() + Proposed.Defense - Viewed.StatBonuses.Defense,
				Attributes->GetDefense() + Proposed.Defense - Viewed.StatBonuses.Defense, 1000000.0f, false);
			Add(NSLOCTEXT("GolemGearUI", "Stamina", "Max Stamina"), Attributes->GetMaxStamina(), Attributes->GetMaxStamina() + Proposed.MaxStamina - Viewed.StatBonuses.MaxStamina,
				Attributes->GetMaxStamina() + Proposed.MaxStamina - Viewed.StatBonuses.MaxStamina, 1000000.0f, false);
			Add(NSLOCTEXT("GolemGearUI", "Resource", "Max class resource"), Attributes->GetMaxRuneEnergy(), Attributes->GetMaxRuneEnergy() + Proposed.MaxRuneEnergy - Viewed.StatBonuses.MaxRuneEnergy,
				Attributes->GetMaxRuneEnergy() + Proposed.MaxRuneEnergy - Viewed.StatBonuses.MaxRuneEnergy, 1000000.0f, false);
			if (const UGolemCombatComponent* Combat = Golem->FindComponentByClass<UGolemCombatComponent>())
			{
				const float Before = Combat->GetMaxPoise();
				const float After = Before + Proposed.Poise - Viewed.StatBonuses.Poise;
				Add(NSLOCTEXT("GolemGearUI", "Poise", "Max Poise"), Before, After, After, 1000000.0f, false);
			}
			auto Secondary = [&Add, ASC](const FText& Label, const FGameplayAttribute& Attribute, float Current,
				float Bonus, float Cap)
			{
				const float Raw = ASC->GetNumericAttributeBase(Attribute) + Bonus;
				Add(Label, Current, FMath::Clamp(Raw, 0.0f, Cap), Raw, Cap, true);
			};
			Secondary(NSLOCTEXT("GolemGearUI", "Crit", "Crit (direct damage)"), UGolemAttributeSet::GetCritChanceAttribute(), Attributes->GetCritChance(), Proposed.CritChance, 0.4f);
			Secondary(NSLOCTEXT("GolemGearUI", "Haste", "Haste"), UGolemAttributeSet::GetHasteAttribute(), Attributes->GetHaste(), Proposed.Haste, 0.3f);
			Secondary(NSLOCTEXT("GolemGearUI", "CDR", "Cooldown recovery"), UGolemAttributeSet::GetCooldownRecoveryAttribute(), Attributes->GetCooldownRecovery(), Proposed.CooldownRecovery, 0.3f);
			Secondary(NSLOCTEXT("GolemGearUI", "Mastery", "Mastery"), UGolemAttributeSet::GetMasteryBonusAttribute(), Attributes->GetMasteryBonus(), Proposed.MasteryBonus, 0.3f);
			Secondary(NSLOCTEXT("GolemGearUI", "Versatility", "Versatility"), UGolemAttributeSet::GetVersatilityAttribute(), Attributes->GetVersatility(), Proposed.Versatility, 0.2f);
		}

	}

	TArray<FGolemGearQuickUseRow> QuickUses(const FGolemRPGGolemView& Golem,
		const FGolemInventoryView& Inventory, const AGolemCharacter* Actor)
	{
		TArray<FGolemGearQuickUseRow> Rows;
		const UGolemSkillRuntimeComponent* Runtime = Actor != nullptr ? Actor->FindComponentByClass<UGolemSkillRuntimeComponent>() : nullptr;
		for (int32 Index = 0; Index < 2; ++Index)
		{
			FGolemGearQuickUseRow Row;
			if (Golem.ConsumableBindings.IsValidIndex(Index))
			{
				Row.DefinitionId = Golem.ConsumableBindings[Index];
			}
			Row.Name = Row.DefinitionId.IsValid() ? Name(Row.DefinitionId) : NSLOCTEXT("GolemGearUI", "Unbound", "Unbound");
			for (const FGolemItemInstance& Item : Inventory.Items)
			{
				if (Item.DefinitionId == Row.DefinitionId && Item.LocationId == GolemGear::InventoryLocation)
				{
					Row.Count += Item.Count;
				}
			}
			Row.CooldownEndTime = Runtime != nullptr ? static_cast<float>(Runtime->GetSkillState().ConsumableEnd) : 0.0f;
			Rows.Add(Row);
		}
		return Rows;
	}

	void Enrich(FGolemRPGPanelSnapshot& Snapshot, const FGolemRPGView& Party,
		const FGolemInventoryView& Inventory, const AGolemCharacter* Golem, const APlayerController* Caller,
		bool bAtSanctuary, const FGolemGearDemoSnapshot* Demo)
	{
		if (!Party.IsItemizationEnabled() || Golem == nullptr)
		{
			return;
		}
		const FGolemRPGGolemView* Viewed = Party.Golems.FindByPredicate([Golem](const FGolemRPGGolemView& Entry)
		{
			return Entry.GolemId == Golem->GetGolemEntityId();
		});
		if (Viewed == nullptr || !Viewed->bGearEnabled)
		{
			return;
		}
		FGolemGearPanelSnapshot& Gear = Snapshot.Gear;
		Gear.bEnabled = true;
		Gear.GolemId = Viewed->GolemId;
		Gear.GolemName = GolemName(Viewed->HeroDefinitionId);
		Gear.Level = Viewed->Level;
		Gear.Currency = Party.Currency;
		Gear.bHasDemoState = Party.bGearDemo && Demo != nullptr;
		Gear.InventoryRevision = Inventory.Revision;
		Gear.GolemRevision = Viewed->Revision;
		Gear.SkillRuneInstanceId = Viewed->SkillRuneInstanceId;
		auto RuneName = [&Inventory](const FGuid& InstanceId)
		{
			const FGolemItemInstance* Item = Inventory.Items.FindByPredicate([&InstanceId](const FGolemItemInstance& Entry)
			{
				return Entry.InstanceId == InstanceId;
			});
			return Item != nullptr ? Name(Item->DefinitionId) : NSLOCTEXT("GolemGearUI", "NoAttachment", "Empty");
		};
		Gear.Attachments = FText::Format(NSLOCTEXT("GolemGearUI", "Attachments",
			"Attachments outside12 slots: Weapon {0}; Skill {1}. Inspected specialization {2}."),
			RuneName(Viewed->WeaponRuneInstanceId), RuneName(Viewed->SkillRuneInstanceId), FText::FromName(Viewed->Build.SpecId));
		Gear.bCanEdit = bAtSanctuary && Golem->GetLifeComponent()->IsAlive() &&
			(Golem->GetController() == Caller || Golem->GetController() == nullptr || !Golem->GetController()->IsPlayerController());
		Gear.bCanEdit &= Golem->CanTransferControl();
		if (const UGolemSkillRuntimeComponent* Runtime = Golem->FindComponentByClass<UGolemSkillRuntimeComponent>())
		{
			Gear.bCanEdit &= !Runtime->HasDeferredDamage();
			const FGolemSkillRuntimeState State = Runtime->GetSkillState();
			Gear.RuntimeRevision = State.Revision;
			const AGameStateBase* GameState = Golem->GetWorld()->GetGameState();
			const double Now = GameState != nullptr ? GameState->GetServerWorldTimeSeconds() : Golem->GetWorld()->GetTimeSeconds();
			auto Cooldown = [Now](double End)
			{
				return End > Now ? FText::Format(NSLOCTEXT("GolemGearUI", "ICDRemaining", "{0}s cooldown"),
					FText::AsNumber(FMath::CeilToInt(End - Now))) : NSLOCTEXT("GolemGearUI", "ICDReady", "Ready");
			};
			Gear.RelicStatus = FText::Format(NSLOCTEXT("GolemGearUI", "RelicState",
				"Relic cooldowns: Bulwark {0}; Lifeseed {1}; Starfall {2}. Starfall counter {3}/3 ({4}s remaining). Cooldowns survive unequip/takeover."),
				Cooldown(State.BulwarkEnd), Cooldown(State.LifeseedEnd), Cooldown(State.StarfallEnd),
				FText::AsNumber(State.StarfallCount), FText::AsNumber(FMath::Max(0, FMath::CeilToInt(State.StarfallCounterEnd - Now))));
		}
		Gear.bCanUseVendor = Gear.bCanEdit;
		Gear.bCanUpgrade = Gear.bCanEdit;
		Gear.bCanRunFixtures = Gear.bHasDemoState && bAtSanctuary && Caller != nullptr && Caller->HasAuthority();
		Gear.EditLockReason = Gear.bCanEdit ? FText() : NSLOCTEXT("GolemGearUI", "EditLock", "Sanctuary, living, idle and equipment ownership are required. Other human Golems can be inspected but not edited.");
		for (const FGolemRPGGolemView& Entry : Party.Golems)
		{
			FGolemGearGolemRow Row;
			Row.GolemId = Entry.GolemId;
			Row.Name = GolemName(Entry.HeroDefinitionId);
			Row.Level = Entry.Level;
			Row.Revision = Entry.Revision;
			Gear.Golems.Add(Row);
		}
		for (FGameplayTag Slot : GolemGear::EquipmentSlots())
		{
			FGolemGearSlotRow Row;
			Row.SlotId = Slot.GetTagName();
			Row.Name = FText::FromString(Slot.ToString().Replace(TEXT("Equipment.Slot."), TEXT("")));
			const FGolemEquipmentAssignment* Assignment = Viewed->FindAssignment(Slot);
			if (Assignment != nullptr)
			{
				Row.InstanceId = Assignment->ItemInstanceId;
				Row.ItemName = Name(Assignment->DefinitionId);
				const FGolemItemInstance* Item = Inventory.Items.FindByPredicate([Assignment](const FGolemItemInstance& Candidate)
				{
					return Candidate.InstanceId == Assignment->ItemInstanceId;
				});
				if (Item != nullptr)
				{
					const FGolemGearItemRow ItemView = ItemRow(*Item, Party, Inventory, *Viewed);
					Row.Tooltip = ItemView.Tooltip;
					Row.Badge = ItemView.Badge;
					Row.QualityColor = ItemView.QualityColor;
				}
			}
			else
			{
				Row.Tooltip = FText::Format(NSLOCTEXT("GolemGearUI", "EmptyTooltip", "{0}: Empty. Select an inventory item and this exact slot to compare/equip."), Row.Name);
			}
			Gear.Slots.Add(Row);
		}
		for (const FGolemItemInstance& Item : Inventory.Items)
		{
			FGolemGearItemRow Row = ItemRow(Item, Party, Inventory, *Viewed);
			if (Row.DefinitionId.IsValid())
			{
				Gear.Items.Add(Row);
			}
		}
		for (const FGolemGearSeed& Seed : FGolemGearCatalog::GetSeeds())
		{
			if (!Gear.bHasDemoState || Seed.Metadata.BuyPrice <= 0)
			{
				continue;
			}
			FGolemItemInstance Item;
			Item.DefinitionId = Seed.DefinitionId;
			Item.Count = 1;
			FGolemGearItemRow Row = ItemRow(Item, Party, Inventory, *Viewed);
			Row.bVendorStock = true;
			Gear.VendorStock.Add(Row);
		}
		Gear.QuickUses = QuickUses(*Viewed, Inventory, Golem);
		if (Gear.bHasDemoState)
		{
			Gear.bCatalogLab = Demo->bCatalogLab;
			Gear.ClaimableRewardId = Demo->ClaimableRewardId;
			Gear.FixtureSummary = FText::Format(NSLOCTEXT("GolemGearUI", "FixtureMode", "{0} · {1} · completed encounters {2}/3 · Run {3}"),
				Demo->bCatalogLab ? NSLOCTEXT("GolemGearUI", "CatalogMode", "DEV ONLY Catalog Lab") : NSLOCTEXT("GolemGearUI", "ProgressionMode", "Progression Demo fixture"),
				Demo->StageLabel, FText::AsNumber(Demo->CompletedEncounters), FText::FromString(Demo->DemoRunId.ToString()));
			Snapshot.Objective = Demo->StageLabel;
		}
	}

	FGolemGearCompare Compare(const UGolemRPGComponent* RPG, const FGolemInventoryView& Inventory,
		const AGolemCharacter* Golem, const FGolemGearPanelIntent& Intent,
		const AGolemCharacter* SourceGolem, const AGolemCharacter* TargetGolem)
	{
		FGolemGearCompare Result;
		Result.InstanceId = Intent.InstanceId;
		Result.GolemId = Intent.GolemId;
		Result.SlotId = Intent.SlotId;
		if (RPG == nullptr || Golem == nullptr)
		{
			return Result;
		}
		const FGolemRPGGolemView Viewed = RPG->GetGolemView(Intent.GolemId);
		const FGolemItemInstance* Item = Inventory.Items.FindByPredicate([&Intent](const FGolemItemInstance& Candidate)
		{
			return Candidate.InstanceId == Intent.InstanceId;
		});
		if (Item == nullptr)
		{
			return Result;
		}
		const UGolemItemDefinition* Definition = Resolve(Item->DefinitionId);
		if (Definition == nullptr)
		{
			return Result;
		}
		FGolemItemStatBonuses Proposed = Viewed.StatBonuses;
		FGolemResult Validation = FGolemResult::Success();
		if (Intent.Action == EGolemGearPanelAction::Transfer)
		{
			FGolemItemStatBonuses SourceBonuses;
			FGolemItemStatBonuses TargetBonuses;
			Validation = RPG->PreviewTransfer(Item->AssignedGolemId, Intent.TargetGolemId, Item->InstanceId,
				FGameplayTag::RequestGameplayTag(Intent.SlotId, false), SourceBonuses, TargetBonuses);
			Result.bValid = Validation.IsSuccess() && SourceGolem != nullptr && TargetGolem != nullptr;
			Result.RejectionReason = Validation.Message;
			if (Result.bValid)
			{
				const FGolemRPGGolemView Source = RPG->GetGolemView(Item->AssignedGolemId);
				const FGolemRPGGolemView Target = RPG->GetGolemView(Intent.TargetGolemId);
				AppendStats(Result, SourceGolem, Source, SourceBonuses, GolemName(Source.HeroDefinitionId));
				AppendStats(Result, TargetGolem, Target, TargetBonuses, GolemName(Target.HeroDefinitionId));
				Result.Summary = FText::Format(NSLOCTEXT("GolemGearUI", "TransferCompare",
					"Atomic transfer {0} → {1}, target {2}. This item's GUID and attached Rune stay together. Displaced destination equipment returns to the shared bag. Both confirmed Golems stay unchanged until Apply."),
					GolemName(Source.HeroDefinitionId), GolemName(Target.HeroDefinitionId), FText::FromName(Intent.SlotId));
			}
			return Result;
		}
		if (Intent.Action == EGolemGearPanelAction::Equip && Definition->GetMetadata().Category == EGolemItemCategory::Equipment)
		{
			Validation = RPG->PreviewEquip(Intent.GolemId, Intent.InstanceId,
				FGameplayTag::RequestGameplayTag(Intent.SlotId, false), Proposed);
		}
		else if (Intent.Action == EGolemGearPanelAction::Unequip && Item->AssignedGolemId == Viewed.GolemId)
		{
			const FGolemItemStatBonuses Removed = Definition->GetEffectiveStats(Item->UpgradeRank);
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
		else if (Intent.Action == EGolemGearPanelAction::Upgrade && Item->AssignedGolemId == Viewed.GolemId)
		{
			const FGolemItemStatBonuses Before = Definition->GetEffectiveStats(Item->UpgradeRank);
			const FGolemItemStatBonuses After = Definition->GetEffectiveStats(Item->UpgradeRank + 1);
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
		AppendStats(Result, Golem, Viewed, Proposed);
		Result.Summary = FText::Format(NSLOCTEXT("GolemGearUI", "CompareSummary",
			"Pure comparison for {0}. Current HP/resource stays unchanged except clamping on confirmation.\n{1}\n{2}"),
			GolemName(Viewed.HeroDefinitionId), Definition->GetMetadata().Subtype == EGolemEquipmentSubtype::Hammer ?
			NSLOCTEXT("GolemGearUI", "RuneRetention", "Weapon replacement: the old weapon's Rune remains attached to its old GUID; it is not moved to the new weapon.") : FText(),
			Effect(Definition->GetMetadata().EquipEffectId));
		if (Intent.Action == EGolemGearPanelAction::Upgrade)
		{
			Result.Summary = FText::Format(NSLOCTEXT("GolemGearUI", "UpgradeCompare",
				"{0}\nITEM rank +{1} → +{2}:\nBefore:\n{3}\nAfter:\n{4}\nCost:20 currency +4 M01 +2 M02 +1 M03. Keeps GUID, rarity, requirements and attached Rune. Unequipped upgrades keep the inspected Golem totals unchanged."),
				Result.Summary, FText::AsNumber(Item->UpgradeRank), FText::AsNumber(Item->UpgradeRank + 1),
				Stats(Definition->GetEffectiveStats(Item->UpgradeRank)), Stats(Definition->GetEffectiveStats(Item->UpgradeRank + 1)));
		}
		const UGolemRuneDefinition* Rune = Cast<UGolemRuneDefinition>(Definition);
		const bool bDetachSkillRune = Intent.Action == EGolemGearPanelAction::DetachSkillRune;
		if (Rune != nullptr && !bDetachSkillRune)
		{
			const FGolemGearItemRow RuneView = ItemRow(*Item, RPG->GetView(), Inventory, Viewed);
			if (!RuneView.bUsable)
			{
				Result.bValid = false;
				Result.RejectionReason = RuneView.Requirement;
			}
		}
		if (bDetachSkillRune || (Rune != nullptr && Rune->GetAttachmentRuneId() != TEXT("Weapon.Aftershock")))
		{
			FGolemSkillBuild ProposedBuild = Viewed.Build;
			ProposedBuild.SkillRune = bDetachSkillRune ? NAME_None : Rune->GetAttachmentRuneId();
			if (bDetachSkillRune)
			{
				const FGolemItemInstance* Attached = Inventory.Items.FindByPredicate([&Viewed](const FGolemItemInstance& Candidate)
				{
					return Candidate.InstanceId == Viewed.SkillRuneInstanceId;
				});
				Result.bValid = Attached != nullptr && !Viewed.Build.SkillRune.IsNone();
				Result.RejectionReason = Result.bValid ? FText() :
					NSLOCTEXT("GolemGearUI", "NoSkillRuneToDetach", "The inspected Golem has no confirmed skill Rune to detach.");
				Result.Summary = FText::Format(NSLOCTEXT("GolemGearUI", "SkillDetachSummary",
					"Pure detachment comparison for {0}: remove {1} (instance {2}). Equipment and the weapon Rune stay unchanged."),
					GolemName(Viewed.HeroDefinitionId), Attached != nullptr ? Name(Attached->DefinitionId) : FText(),
					FText::FromString(Viewed.SkillRuneInstanceId.ToString()));
			}
			FString SkillChanges;
			for (int32 Index = 1; Index <= 4; ++Index)
			{
				const FGolemSpecActionDefinition Before = FGolemSkillCatalog::GetSkill(Viewed.Build, Index);
				const FGolemSpecActionDefinition After = FGolemSkillCatalog::GetSkill(ProposedBuild, Index);
				SkillChanges += FString::Printf(TEXT("A%d: coefficient %g→%g, cost %g→%g, CD %gs→%gs, duration %gs→%gs; %s→%s, targets %d→%d.\n"),
					Index, Before.Coefficient, After.Coefficient, Before.Cost, After.Cost, Before.Cooldown, After.Cooldown,
					Before.Duration, After.Duration, *StaticEnum<EGolemSkillTargeting>()->GetNameStringByValue(static_cast<int64>(Before.Targeting)),
					*StaticEnum<EGolemSkillTargeting>()->GetNameStringByValue(static_cast<int64>(After.Targeting)), Before.MaxTargets, After.MaxTargets);
			}
			Result.Summary = FText::Format(NSLOCTEXT("GolemGearUI", "SkillRuneCompare", "{0}\nSkill Rune comparison:\n{1}"),
				Result.Summary, FText::FromString(SkillChanges));
		}
		return Result;
	}
}
