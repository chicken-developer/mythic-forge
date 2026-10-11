#include "RPGLoop/KnightRPGSkillPresentation.h"

#include "Assets/KnightAssetManager.h"
#include "Attributes/KnightAttributeSet.h"
#include "Character/KnightCharacter.h"
#include "Components/KnightAbilitySystemComponent.h"
#include "Components/KnightCombatComponent.h"
#include "Definitions/KnightAbilitySetDefinition.h"
#include "Definitions/KnightCombatProfileDefinition.h"
#include "Definitions/KnightItemDefinition.h"
#include "Definitions/KnightRuneDefinition.h"
#include "Definitions/KnightSkillDefinition.h"
#include "Effects/KnightSourceProjectionEffect.h"
#include "Engine/World.h"
#include "GameplayEffect.h"
#include "Skills/KnightSkillTypes.h"
#include "Tags/KnightGameplayTags.h"

namespace KnightRPGSkillPresentation
{
	namespace
	{
		FText HeroDisplayName(const FPrimaryAssetId& HeroId)
		{
			const FName Id = HeroId.PrimaryAssetName;
			if (Id == TEXT("Training_Red"))
			{
				return NSLOCTEXT("KnightSkillsUI", "Sky", "Sky");
			}
			if (Id == TEXT("Training_Blue"))
			{
				return NSLOCTEXT("KnightSkillsUI", "Sea", "Sea");
			}
			if (Id == TEXT("Training_Green"))
			{
				return NSLOCTEXT("KnightSkillsUI", "Earth", "Earth");
			}
			return FText::FromName(Id);
		}

		float StatBonus(const FKnightItemStatBonuses& Bonuses, const FGameplayAttribute& Attribute)
		{
			if (Attribute == UKnightAttributeSet::GetMaxHealthAttribute())
			{
				return Bonuses.MaxHealth;
			}
			if (Attribute == UKnightAttributeSet::GetMaxStaminaAttribute())
			{
				return Bonuses.MaxStamina;
			}
			if (Attribute == UKnightAttributeSet::GetMaxRuneEnergyAttribute())
			{
				return Bonuses.MaxRuneEnergy;
			}
			if (Attribute == UKnightAttributeSet::GetAttackPowerAttribute())
			{
				return Bonuses.AttackPower;
			}
			if (Attribute == UKnightAttributeSet::GetCritChanceAttribute())
			{
				return Bonuses.CritChance;
			}
			if (Attribute == UKnightAttributeSet::GetHasteAttribute())
			{
				return Bonuses.Haste;
			}
			if (Attribute == UKnightAttributeSet::GetCooldownRecoveryAttribute())
			{
				return Bonuses.CooldownRecovery;
			}
			if (Attribute == UKnightAttributeSet::GetMasteryBonusAttribute())
			{
				return Bonuses.MasteryBonus;
			}
			if (Attribute == UKnightAttributeSet::GetVersatilityAttribute())
			{
				return Bonuses.Versatility;
			}
			return Attribute == UKnightAttributeSet::GetDefenseAttribute() ? Bonuses.Defense : 0.0f;
		}

		struct FKnightStatSource
		{
			FText Name;
			FKnightItemStatBonuses Bonuses;
			TArray<TSubclassOf<UGameplayEffect>> PassiveEffects;
		};

		FKnightRPGSpecRow Spec(const FName Id)
		{
			FKnightRPGSpecRow Row;
			Row.SpecId = Id;
			Row.Name = FText::FromName(Id);
			const EKnightSkillRole Role = FKnightSkillCatalog::Role(Id);
			Row.Role = Role == EKnightSkillRole::Tank ? NSLOCTEXT("KnightSkillsUI", "Tank", "Tank")
				: (Role == EKnightSkillRole::Healer ? NSLOCTEXT("KnightSkillsUI", "Healer", "Healer")
				: NSLOCTEXT("KnightSkillsUI", "Damage", "DPS"));
			FKnightSkillBuild Build;
			Build.SpecId = Id;
			Row.MajorBuff = FKnightSkillCatalog::GetSkill(Build, 4).DisplayName;
			if (Id == TEXT("Uranus"))
			{
				Row.PassiveName = NSLOCTEXT("KnightSkillsUI", "UranusPassive", "Tich Quang / Stored Light");
				Row.PassiveTooltip = NSLOCTEXT("KnightSkillsUI", "UranusLoop", "Block/parry gains up to3 light stacks (1/s,8s). A3 consumes all for +10% shield each. A4 protects the rear60 degree cone against frontal sources,25% DR.");
				Row.ChoiceA = NSLOCTEXT("KnightSkillsUI", "UranusA", "A: A3 shield absorption x1.15");
				Row.ChoiceB = NSLOCTEXT("KnightSkillsUI", "UranusB", "B: A4 formation duration 8s -> 10s");
			}
			else if (Id == TEXT("Venus"))
			{
				Row.PassiveName = NSLOCTEXT("KnightSkillsUI", "VenusPassive", "Du Quang / Afterglow");
				Row.PassiveTooltip = NSLOCTEXT("KnightSkillsUI", "VenusLoop", "Actual direct heal gains1 stack/cast, max3/10s. Next direct cast consumes3 for +30% heal and half cost. A4 gives25% actual heal as shield,5s, cap15% recipient HP.");
				Row.ChoiceA = NSLOCTEXT("KnightSkillsUI", "VenusA", "A: A2 direct healing x1.15");
				Row.ChoiceB = NSLOCTEXT("KnightSkillsUI", "VenusB", "B: A3 healing radius 5m -> 7m");
			}
			else if (Id == TEXT("Jupiter"))
			{
				Row.PassiveName = NSLOCTEXT("KnightSkillsUI", "JupiterPassive", "Tinh Dien / Static Charge");
				Row.PassiveTooltip = NSLOCTEXT("KnightSkillsUI", "JupiterLoop", "A1 hit gains charge, max5/10s. A2 consumes for +6% direct damage/stack. A4 doubles builder gain and adds30% pre-defense lightning to2 other enemies; secondary cannot proc.");
				Row.ChoiceA = NSLOCTEXT("KnightSkillsUI", "JupiterA", "A: A2 raw direct coefficient x1.15");
				Row.ChoiceB = NSLOCTEXT("KnightSkillsUI", "JupiterB", "B: A4 secondary targets 2 -> 3");
			}
			else if (Id == TEXT("Neptune"))
			{
				Row.PassiveName = NSLOCTEXT("KnightSkillsUI", "NeptunePassive", "Du Ap / Deferred Pressure");
				Row.PassiveTooltip = NSLOCTEXT("KnightSkillsUI", "NeptuneLoop", "25% eligible damage after mitigation/block becomes five HP-only ticks, before shield. A4 clears50% outstanding debt capped20% own MaxHP, then15% DR6s. Cannot switch while debt remains.");
				Row.ChoiceA = NSLOCTEXT("KnightSkillsUI", "NeptuneA", "A: A3 shield absorption x1.15");
				Row.ChoiceB = NSLOCTEXT("KnightSkillsUI", "NeptuneB", "B: A4 damage reduction duration 6s -> 8s");
			}
			else if (Id == TEXT("Mercury"))
			{
				Row.PassiveName = NSLOCTEXT("KnightSkillsUI", "MercuryPassive", "Dong Sinh Luc / Life Current");
				Row.PassiveTooltip = NSLOCTEXT("KnightSkillsUI", "MercuryLoop", "Each distinct Knight with own main HoT grants4% healing, cap12%. Snapshot on apply. A4 main HoT ticks add30% pre-clamp heal to the lowest HP other ally within6m; no recursion.");
				Row.ChoiceA = NSLOCTEXT("KnightSkillsUI", "MercuryA", "A: A2 HoT tick coefficient x1.15");
				Row.ChoiceB = NSLOCTEXT("KnightSkillsUI", "MercuryB", "B: A4 secondary healing radius 6m -> 8m");
			}
			else if (Id == TEXT("Pluto"))
			{
				Row.PassiveName = NSLOCTEXT("KnightSkillsUI", "PlutoPassive", "Chuyen Pha / Phase Shift");
				Row.PassiveTooltip = NSLOCTEXT("KnightSkillsUI", "PlutoLoop", "Water hit empowers next Ice +20%; Ice hit discounts next Water25%,6s. A1 adds Chilled; A3 adds2. A2 at3 consumes for Shatter0.6Power, shared target ICD4s. A4 Ice penetrates20% positive Frost resistance and Shatter+25%.");
				Row.ChoiceA = NSLOCTEXT("KnightSkillsUI", "PlutoA", "A: A2 raw direct coefficient x1.15");
				Row.ChoiceB = NSLOCTEXT("KnightSkillsUI", "PlutoB", "B: A3 ice area radius 4m -> 5m");
			}
			else if (Id == TEXT("Saturn"))
			{
				Row.PassiveName = NSLOCTEXT("KnightSkillsUI", "SaturnPassive", "Thach Tang / Stone Layers");
				Row.PassiveTooltip = NSLOCTEXT("KnightSkillsUI", "SaturnLoop", "Heavy hit or successful A3 grants1 layer/2s, max4; common8s refresh,3% DR each. A4 holds4 layers8s and halves poise damage. No forced mechanic immunity.");
				Row.ChoiceA = NSLOCTEXT("KnightSkillsUI", "SaturnA", "A: A3 shield absorption x1.15");
				Row.ChoiceB = NSLOCTEXT("KnightSkillsUI", "SaturnB", "B: A4 Mountain Heart duration 8s -> 10s");
			}
			else if (Id == TEXT("Terra"))
			{
				Row.PassiveName = NSLOCTEXT("KnightSkillsUI", "TerraPassive", "Than Hong Sinh Menh / Life Embers");
				Row.PassiveTooltip = NSLOCTEXT("KnightSkillsUI", "TerraLoop", "Actual A3 heal gains1 ember/cast, max3/10s. Next consumes3 for30% base heal as HoT6s. A4 heal below40% HP gains25%;30% overheal becomes shield5s, cap15% recipient HP. Pillar ticks do not build embers.");
				Row.ChoiceA = NSLOCTEXT("KnightSkillsUI", "TerraA", "A: A2 pillar healing tick coefficient x1.15");
				Row.ChoiceB = NSLOCTEXT("KnightSkillsUI", "TerraB", "B: A2 pillar radius 5m -> 7m; Mobile Rune retains its 3m radius");
			}
			else
			{
				Row.PassiveName = NSLOCTEXT("KnightSkillsUI", "MarsPassive", "Nhiet Lo / Furnace Heat");
				Row.PassiveTooltip = NSLOCTEXT("KnightSkillsUI", "MarsLoop", "Earth/Stone direct hit gains Heat once/cast, max5/10s. Fire A2 consumes all for Burn:8% raw direct/stack over6s. A4 doubles builder gain and refunds2 Heat after actual consume. Ticks cannot proc.");
				Row.ChoiceA = NSLOCTEXT("KnightSkillsUI", "MarsA", "A: A2 raw direct coefficient x1.15");
				Row.ChoiceB = NSLOCTEXT("KnightSkillsUI", "MarsB", "B: A3 ground strip width 2m -> 3m");
			}
			Row.Summary = Row.PassiveTooltip;
			return Row;
		}

		FText TargetingName(const EKnightSkillTargeting Targeting)
		{
			switch (Targeting)
			{
			case EKnightSkillTargeting::Cone: return NSLOCTEXT("KnightSkillsUI", "Cone", "Cone");
			case EKnightSkillTargeting::Radius: return NSLOCTEXT("KnightSkillsUI", "Radius", "Radius");
			case EKnightSkillTargeting::GroundStrip: return NSLOCTEXT("KnightSkillsUI", "GroundStrip", "Ground strip");
			case EKnightSkillTargeting::GroundArea: return NSLOCTEXT("KnightSkillsUI", "GroundArea", "Fixed ground area");
			case EKnightSkillTargeting::FollowingAlly: return NSLOCTEXT("KnightSkillsUI", "FollowingAlly", "Following chosen ally");
			case EKnightSkillTargeting::Self: return NSLOCTEXT("KnightSkillsUI", "Self", "Self");
			case EKnightSkillTargeting::Dash: return NSLOCTEXT("KnightSkillsUI", "Dash", "Dash");
			default: return NSLOCTEXT("KnightSkillsUI", "Single", "Single primary target");
			}
		}

		TArray<FKnightRPGSpellRow> Spells(const FKnightSkillBuild& Build, const float Power)
		{
			TArray<FKnightRPGSpellRow> Rows;
			if (!FKnightSkillCatalog::IsKnownSpec(Build.SpecId))
			{
				return Rows;
			}
			FKnightSkillBuild BaseBuild;
			BaseBuild.SpecId = Build.SpecId;
			FKnightSkillBuild TalentedBuild = Build;
			TalentedBuild.SkillRune = NAME_None;
			TalentedBuild.WeaponRune = NAME_None;
			for (int32 Slot = 1; Slot <= 4; ++Slot)
			{
				const FKnightSpecActionDefinition Definition = FKnightSkillCatalog::GetSkill(Build, Slot);
				const FKnightSpecActionDefinition Base = FKnightSkillCatalog::GetSkill(BaseBuild, Slot);
				const FKnightSpecActionDefinition Talented = FKnightSkillCatalog::GetSkill(TalentedBuild, Slot);
				FKnightRPGSpellRow Row;
				Row.AbilityId = Definition.AbilityId;
				Row.Name = Definition.DisplayName;
				Row.Category = TEXT("Spec");
				Row.Slot = Slot;
				const FText Targeting = Definition.bPeriodic ? NSLOCTEXT("KnightSkillsUI", "Periodic", "Periodic: first tick at +1s")
					: (Definition.bFriendly ? NSLOCTEXT("KnightSkillsUI", "Friendly", "Ally/self")
					: NSLOCTEXT("KnightSkillsUI", "Enemy", "Enemy, LOS required"));
				const FText Scaling = Definition.Role == EKnightSkillRole::Healer && Slot > 1
					? NSLOCTEXT("KnightSkillsUI", "HealEstimate", "healing; no crit")
					: (Definition.Role == EKnightSkillRole::Tank && Slot == 3
					? NSLOCTEXT("KnightSkillsUI", "ShieldEstimate", "shield absorption; no crit/Versatility")
					: NSLOCTEXT("KnightSkillsUI", "DamageEstimate", "raw direct damage, before target defense/crit"));
				Row.Summary = FText::Format(NSLOCTEXT("KnightSkillsUI", "SkillEstimateTyped", "A{0}: {1} | {2} Power = {3} {4}"),
					FText::AsNumber(Slot), Targeting, FText::AsNumber(Definition.Coefficient),
					FText::AsNumber(Definition.Coefficient * Power), Scaling);
				Row.Tooltip = FText::Format(NSLOCTEXT("KnightSkillsUI", "SkillDetails",
					"{0}\nCost {1}, CD {2}s; range {3}m, radius {4}m; duration {5}s\nWindup {6}s, recovery {7}s; poise {8}. Haste affects windup only. CD Recovery affects new cooldowns only. School {9} (Water=Frost, Light=Arcane, Earth/Stone=Physical).\nRune: {10}"),
					Row.Summary, FText::AsNumber(Definition.Cost), FText::AsNumber(Definition.Cooldown),
					FText::AsNumber(Definition.Range / 100.0f), FText::AsNumber(Definition.Radius / 100.0f),
					FText::AsNumber(Definition.Duration), FText::AsNumber(Definition.Windup),
					FText::AsNumber(Definition.Recovery), FText::AsNumber(Definition.PoiseDamage),
					FText::FromString(StaticEnum<EKnightSkillSchool>()->GetNameStringByValue(static_cast<int64>(Definition.School))),
					FText::FromName(Build.SkillRune));
				Row.Tooltip = FText::Format(NSLOCTEXT("KnightSkillsUI", "SkillComparison",
					"{0}\nTargeting: {1} -> {2}; max {3} primary targets, cone {4} degrees.\nBase / talents / proposed rune: coefficient {5} / {6} / {7}; rune delta {8} Power.\nRadius {9} / {10} / {11}m; duration {12} / {13} / {14}s."),
					Row.Tooltip, TargetingName(Talented.Targeting), TargetingName(Definition.Targeting),
					FText::AsNumber(Definition.MaxTargets), FText::AsNumber(Definition.ConeAngle),
					FText::AsNumber(Base.Coefficient), FText::AsNumber(Talented.Coefficient),
					FText::AsNumber(Definition.Coefficient), FText::AsNumber(Definition.Coefficient - Talented.Coefficient),
					FText::AsNumber(Base.Radius / 100.0f), FText::AsNumber(Talented.Radius / 100.0f),
					FText::AsNumber(Definition.Radius / 100.0f), FText::AsNumber(Base.Duration),
					FText::AsNumber(Talented.Duration), FText::AsNumber(Definition.Duration));
				if (Slot == 4)
				{
					Row.Summary = FText::Format(NSLOCTEXT("KnightSkillsUI", "MajorSummary", "A4: {0}, {1}s; cooldown {2}s"),
						Definition.DisplayName, FText::AsNumber(Definition.Duration), FText::AsNumber(Definition.Cooldown));
					Row.Tooltip = FText::Format(NSLOCTEXT("KnightSkillsUI", "MajorDetails", "{0}\n{1}\nSelf buff. Windup {2}s; recovery {3}s. Uses the shared SpecMajor cooldown; changing specialization preserves remaining time."),
						Row.Summary, Spec(Build.SpecId).PassiveTooltip, FText::AsNumber(Definition.Windup),
						FText::AsNumber(Definition.Recovery));
				}
				if (Definition.Targeting == EKnightSkillTargeting::Dash)
				{
					Row.Summary = FText::Format(NSLOCTEXT("KnightSkillsUI", "DashSummary", "A{0}: Dash {1}m; no invulnerability"),
						FText::AsNumber(Slot), FText::AsNumber(Definition.Range / 100.0f));
					Row.Tooltip = FText::Format(NSLOCTEXT("KnightSkillsUI", "DashDetails", "{0}\nCost {1}; cooldown {2}s. Collision and the M3 dodge/defense policy remain active."),
						Row.Summary, FText::AsNumber(Definition.Cost), FText::AsNumber(Definition.Cooldown));
				}
				Rows.Add(Row);
			}
			return Rows;
		}
	}

	FKnightRPGBuildPreview Preview(const FKnightSkillBuild& Build, const float Power)
	{
		FKnightRPGBuildPreview Result;
		Result.Build = Build;
		Result.Spells = Spells(Build, Power);
		Result.Summary = FText::Format(NSLOCTEXT("KnightSkillsUI", "PurePreview", "DRAFT {0}: class {1}, spec choice {2}. Rune {3}. Definition preview only; live HP/resource/cooldowns are unchanged."),
			FText::FromName(Build.SpecId), FText::FromName(Build.ClassChoice), FText::FromName(Build.SpecChoice),
			FText::FromName(Build.SkillRune));
		return Result;
	}

	void Enrich(FKnightRPGPanelSnapshot& Snapshot, const FKnightRPGKnightView& View,
		const FKnightRPGView& PartyView, const FKnightInventoryView& Inventory, const AKnightCharacter* Knight)
	{
		Snapshot.Build = View.Build;
		if (!View.Build.SpecId.IsNone())
		{
			Snapshot.Title = FText::Format(NSLOCTEXT("KnightSkillsUI", "CharacterTitle", "{0} — Character & Build"),
				HeroDisplayName(View.HeroDefinitionId));
		}
		Snapshot.Revision = View.Revision;
		Snapshot.AvailablePoints = View.SkillPoints;
		Snapshot.WeaponRuneInstanceId = View.WeaponRuneInstanceId;
		Snapshot.SkillRuneInstanceId = View.SkillRuneInstanceId;
		Snapshot.ActiveTalentLoadoutId = View.ActiveTalentLoadoutId;
		for (const FKnightRPGKnightView& Entry : PartyView.Knights)
		{
			FKnightRPGKnightChoice Row;
			Row.KnightId = Entry.KnightId;
			Row.Name = HeroDisplayName(Entry.HeroDefinitionId);
			Snapshot.Knights.Add(Row);
		}
		for (const FName Id : FKnightSkillCatalog::SpecsForHero(View.HeroDefinitionId))
		{
			Snapshot.Specializations.Add(Spec(Id));
		}
		for (const FKnightTalentLoadout& Preset : View.TalentLoadouts)
		{
			FKnightRPGTalentLoadoutRow Row;
			Row.LoadoutId = Preset.LoadoutId;
			Row.Name = Preset.Name;
			Row.Build = Preset.Build;
			Row.Version = Preset.Version;
			Snapshot.TalentLoadouts.Add(Row);
		}
		const UKnightAttributeSet* Attributes = Knight->GetKnightAttributeSet();
		Snapshot.Spellbook = Spells(View.Build, Attributes->GetAttackPower());
		if (!View.Build.SpecId.IsNone())
		{
			const FKnightRPGSpecRow Current = Spec(View.Build.SpecId);
			FKnightRPGSpellRow Passive;
			Passive.AbilityId = FName(*(View.Build.SpecId.ToString() + TEXT(".Passive")));
			Passive.Name = Current.PassiveName;
			Passive.Category = TEXT("Passives");
			Passive.bPassive = true;
			Passive.Tooltip = Current.PassiveTooltip;
			Snapshot.Spellbook.Add(Passive);
		}
		for (const FName Id : { FName(TEXT("Light")), FName(TEXT("Heavy")), FName(TEXT("Dodge")),
			FName(TEXT("Block")), FName(TEXT("Parry")), FName(TEXT("Revive")) })
		{
			FKnightRPGSpellRow General;
			General.AbilityId = Id;
			General.Name = FText::FromName(Id);
			General.Category = TEXT("General");
			General.Tooltip = NSLOCTEXT("KnightSkillsUI", "CoreAction", "Core combat action, free. Uses existing M3 input, stamina/cancel policy. Does not occupy A1-A4.");
			Snapshot.Spellbook.Add(General);
		}
		UKnightAssetManager* Manager = UKnightAssetManager::GetKnightAssetManager();
		for (const FKnightItemInstance& Item : Inventory.Items)
		{
			UKnightDefinition* Definition = nullptr;
			if (Manager == nullptr || !Manager->ResolveDefinition(Item.DefinitionId,
				UKnightRuneDefinition::StaticClass(), Definition).IsSuccess())
			{
				continue;
			}
			const UKnightRuneDefinition* Rune = Cast<UKnightRuneDefinition>(Definition);
			if (Rune == nullptr || Rune->GetAttachmentRuneId().IsNone())
			{
				continue;
			}
			FKnightRPGRuneRow Row;
			Row.RuneId = Rune->GetAttachmentRuneId();
			Row.ItemInstanceId = Item.InstanceId;
			Row.Name = Rune->GetDisplayName();
			Row.bWeaponSocket = Row.RuneId == TEXT("Weapon.Aftershock");
			Row.bCompatible = FKnightSkillCatalog::IsRuneCompatible(View.Build.SpecId, Row.RuneId);
			if (View.bGearEnabled && Item.LocationId != TEXT("Inventory") &&
				!(Item.AssignedKnightId == View.KnightId &&
					(Item.InstanceId == View.WeaponRuneInstanceId || Item.InstanceId == View.SkillRuneInstanceId)))
			{
				Row.bCompatible = false;
				Row.IncompatibilityReason = NSLOCTEXT("KnightGearUI", "RuneLocationOccupied", "This exact Rune is already attached to a weapon or another Knight. Detach it first.");
			}
			if (Row.bWeaponSocket)
			{
				const FKnightEquipmentAssignment* Weapon = View.FindAssignment(KnightGameplayTags::Slot_Equipment_Weapon);
				UKnightDefinition* WeaponDefinition = nullptr;
				if (Weapon == nullptr || !Manager->ResolveDefinition(Weapon->DefinitionId,
					UKnightItemDefinition::StaticClass(), WeaponDefinition).IsSuccess()
					|| CastChecked<UKnightItemDefinition>(WeaponDefinition)->GetWeaponFamily() != TEXT("HeavyHammer"))
				{
					Row.bCompatible = false;
					Row.IncompatibilityReason = NSLOCTEXT("KnightSkillsUI", "AftershockWeaponRequired",
						"Equip an M5 HeavyHammer before attaching Aftershock to its weapon socket.");
				}
			}
			if (Item.Count != 1)
			{
				Row.bCompatible = false;
				Row.IncompatibilityReason = NSLOCTEXT("KnightSkillsUI", "UnstackableAttachmentRequired",
					"Attach Runes require one exact unstackable source item.");
			}
			for (const FKnightRPGKnightView& Entry : PartyView.Knights)
			{
				if (Entry.Assignments.ContainsByPredicate([&Item](const FKnightEquipmentAssignment& Assignment)
					{
						return Assignment.ItemInstanceId == Item.InstanceId;
					}))
				{
					Row.bCompatible = false;
					Row.IncompatibilityReason = NSLOCTEXT("KnightSkillsUI", "RuneAlreadyEquipped",
						"This exact rune instance is already assigned to an M5 equipment slot.");
				}
				if (Entry.KnightId != View.KnightId && (Entry.WeaponRuneInstanceId == Item.InstanceId
					|| Entry.SkillRuneInstanceId == Item.InstanceId))
				{
					Row.bCompatible = false;
					Row.IncompatibilityReason = NSLOCTEXT("KnightSkillsUI", "RuneAssigned", "This exact rune instance is attached to another Knight.");
				}
			}
			if (!Row.bCompatible && Row.IncompatibilityReason.IsEmpty())
			{
				Row.IncompatibilityReason = NSLOCTEXT("KnightSkillsUI", "RuneIncompatible", "The selected specialization has no compatible skill for this rune.");
			}
			Row.Description = Row.RuneId == TEXT("Skill.Mobile")
				? NSLOCTEXT("KnightSkillsUI", "Mobile", "Terra A2: ground-fixed5m/6s -> follows chosen ally3m/4s. Tick cadence, cost and cooldown preserved.")
				: (Row.RuneId == TEXT("Skill.Convergence")
				? NSLOCTEXT("KnightSkillsUI", "Convergence", "Tank/Mars A2: cone -> primary only; raw damage x1.20, Mars Burn uses modified raw once.")
				: (Row.RuneId == TEXT("Skill.Propagation")
				? NSLOCTEXT("KnightSkillsUI", "Propagation", "Venus A2/Mercury A3/Terra A3: primary heal x0.85; one other ally within6m gets30% pre-clamp heal. Secondary cannot proc.")
				: NSLOCTEXT("KnightSkillsUI", "Aftershock", "Heavy hammer hit: Aftershock0.3Power Physical, radius2m,max3,poise5,ICD4s; cannot chain.")));
			Snapshot.Runes.Add(Row);
		}
		TArray<FKnightStatSource> Sources;
		TSet<UClass*> OwnedPassiveClasses;
		const auto AddSource = [&Sources, &OwnedPassiveClasses, Manager](const FText& Name,
			const FKnightItemStatBonuses& Bonuses, const FPrimaryAssetId& AbilitySetId)
		{
			FKnightStatSource Source;
			Source.Name = Name;
			Source.Bonuses = Bonuses;
			UKnightDefinition* Resolved = nullptr;
			if (Manager != nullptr && AbilitySetId.IsValid()
				&& Manager->ResolveDefinition(AbilitySetId, UKnightAbilitySetDefinition::StaticClass(), Resolved).IsSuccess())
			{
				Source.PassiveEffects = CastChecked<UKnightAbilitySetDefinition>(Resolved)->GetEffects();
				for (TSubclassOf<UGameplayEffect> EffectClass : Source.PassiveEffects)
				{
					OwnedPassiveClasses.Add(EffectClass.Get());
				}
			}
			Sources.Add(MoveTemp(Source));
		};
		for (const FKnightEquipmentAssignment& Assignment : View.Assignments)
		{
			UKnightDefinition* Resolved = nullptr;
			if (Manager != nullptr && Manager->ResolveDefinition(Assignment.DefinitionId,
				UKnightItemDefinition::StaticClass(), Resolved).IsSuccess())
			{
				const UKnightItemDefinition* Item = CastChecked<UKnightItemDefinition>(Resolved);
				const FText Label = Item->GetItemKind() == EKnightItemKind::Rune
					? NSLOCTEXT("KnightSkillsUI", "RuneSource", "Rune: {0}")
					: NSLOCTEXT("KnightSkillsUI", "EquipmentSource", "Equipment: {0}");
				const FKnightItemInstance* Instance = Inventory.Items.FindByPredicate([&Assignment](const FKnightItemInstance& Candidate)
				{
					return Candidate.InstanceId == Assignment.ItemInstanceId;
				});
				AddSource(FText::Format(Label, Item->GetDisplayName()),
					Item->GetEffectiveStats(Instance != nullptr ? Instance->UpgradeRank : 0), Item->GetAbilitySetId());
			}
		}
		for (const FKnightPurchasedSkill& Purchase : View.PurchasedSkills)
		{
			UKnightDefinition* Resolved = nullptr;
			if (Manager != nullptr && Manager->ResolveDefinition(Purchase.DefinitionId,
				UKnightSkillDefinition::StaticClass(), Resolved).IsSuccess())
			{
				const UKnightSkillDefinition* Skill = CastChecked<UKnightSkillDefinition>(Resolved);
				AddSource(FText::Format(NSLOCTEXT("KnightSkillsUI", "PurchasedTalentSource", "Purchased talent: {0}"),
					Skill->GetDisplayName()), Skill->GetStatBonuses(), Skill->GetAbilitySetId());
			}
		}
		FKnightItemStatBonuses ClassBonuses;
		ClassBonuses.MaxStamina = View.Build.ClassChoice == TEXT("Endurance") ? 10.0f : 0.0f;
		ClassBonuses.MaxRuneEnergy = View.Build.ClassChoice == TEXT("Reservoir") ? 10.0f : 0.0f;
		AddSource(FText::Format(NSLOCTEXT("KnightSkillsUI", "ClassTalentSource", "Class talent: {0}"),
			View.Build.ClassChoice.IsNone() ? NSLOCTEXT("KnightSkillsUI", "NoChoice", "None")
				: FText::FromName(View.Build.ClassChoice)), ClassBonuses, FPrimaryAssetId());
		const UKnightAbilitySystemComponent* ASC = Knight->GetKnightAbilitySystem();
		const TArray<FActiveGameplayEffectHandle> EffectHandles = ASC->GetActiveEffects(FGameplayEffectQuery());
		const FKnightRPGSpecRow CurrentSpec = Spec(View.Build.SpecId);
		const FText SpecChoice = View.Build.SpecChoice == TEXT("A") ? CurrentSpec.ChoiceA
			: View.Build.SpecChoice == TEXT("B") ? CurrentSpec.ChoiceB
			: NSLOCTEXT("KnightSkillsUI", "NoSpecTalent", "None");
		const auto AddStat = [&Snapshot, &Sources, &OwnedPassiveClasses, &EffectHandles, ASC, &SpecChoice, &View](
			const FText& Group, const FText& Name, const FGameplayAttribute& Attribute, const float Value,
			const FText& Policy)
		{
			FKnightRPGStatRow Row;
			Row.Group = Group;
			Row.Name = Name;
			const bool bPercent = Attribute == UKnightAttributeSet::GetCritChanceAttribute()
				|| Attribute == UKnightAttributeSet::GetHasteAttribute()
				|| Attribute == UKnightAttributeSet::GetArcaneResistanceAttribute()
				|| Attribute == UKnightAttributeSet::GetLightningResistanceAttribute()
				|| Attribute == UKnightAttributeSet::GetFrostResistanceAttribute()
				|| Attribute == UKnightAttributeSet::GetFireResistanceAttribute()
				|| Attribute == UKnightAttributeSet::GetMasteryBonusAttribute()
				|| Attribute == UKnightAttributeSet::GetVersatilityAttribute()
				|| Attribute == UKnightAttributeSet::GetCooldownRecoveryAttribute()
				|| Attribute == UKnightAttributeSet::GetDamageReductionAttribute();
			Row.Value = bPercent ? FText::AsPercent(Value) : FText::AsNumber(Value);
			TArray<FText> Lines;
			Lines.Add(FText::Format(NSLOCTEXT("KnightSkillsUI", "BaseAttribute", "Base: {0}"),
				bPercent ? FText::AsPercent(ASC->GetNumericAttributeBase(Attribute)) : FText::AsNumber(ASC->GetNumericAttributeBase(Attribute))));
			for (const FKnightStatSource& Source : Sources)
			{
				float Bonus = StatBonus(Source.Bonuses, Attribute);
				for (TSubclassOf<UGameplayEffect> EffectClass : Source.PassiveEffects)
				{
					const UGameplayEffect* Effect = EffectClass.GetDefaultObject();
					if (Effect == nullptr)
					{
						continue;
					}
					for (const FGameplayModifierInfo& Modifier : Effect->Modifiers)
					{
						float Magnitude = 0.0f;
						if (Modifier.Attribute == Attribute && Modifier.ModifierOp == EGameplayModOp::Additive
							&& Modifier.ModifierMagnitude.GetStaticMagnitudeIfPossible(1.0f, Magnitude))
						{
							Bonus += Magnitude;
						}
					}
				}
				Lines.Add(FText::Format(bPercent ? NSLOCTEXT("KnightGearUI", "PercentContribution", "{0}: +{1} percentage points") :
					NSLOCTEXT("KnightSkillsUI", "NamedStatContribution", "{0}: +{1}"),
					Source.Name, FText::AsNumber(bPercent ? Bonus * 100.0f : Bonus)));
			}
			Lines.Add(FText::Format(NSLOCTEXT("KnightSkillsUI", "SpecStatContribution", "Spec talent: {0}; no flat attribute bonus"), SpecChoice));
			Lines.Add(FText::Format(NSLOCTEXT("KnightSkillsUI", "AttachedRuneStatContribution",
				"Attached weapon rune: {0}; skill rune: {1}. They modify abilities; flat attribute bonus +0."),
				View.Build.WeaponRune.IsNone() ? NSLOCTEXT("KnightSkillsUI", "NoWeaponRune", "None") : FText::FromName(View.Build.WeaponRune),
				View.Build.SkillRune.IsNone() ? NSLOCTEXT("KnightSkillsUI", "NoSkillRune", "None") : FText::FromName(View.Build.SkillRune)));
			bool bHasActiveModifier = false;
			for (FActiveGameplayEffectHandle Handle : EffectHandles)
			{
				const FActiveGameplayEffect* Effect = ASC->GetActiveGameplayEffect(Handle);
				if (Effect == nullptr || Effect->bIsInhibited || Effect->Spec.Def == nullptr
					|| Effect->Spec.Def->IsA<UKnightSourceProjectionEffect>()
					|| OwnedPassiveClasses.Contains(Effect->Spec.Def->GetClass()))
				{
					continue;
				}
				for (int32 Index = 0; Index < Effect->Spec.Def->Modifiers.Num(); ++Index)
				{
					const FGameplayModifierInfo& Modifier = Effect->Spec.Def->Modifiers[Index];
					if (Modifier.Attribute != Attribute)
					{
						continue;
					}
					bHasActiveModifier = true;
					const float Magnitude = Effect->Spec.GetModifierMagnitude(Index);
					const FText Operation = Modifier.ModifierOp == EGameplayModOp::Additive ? NSLOCTEXT("KnightSkillsUI", "AddOp", "+")
						: Modifier.ModifierOp == EGameplayModOp::Multiplicitive ? NSLOCTEXT("KnightSkillsUI", "MultiplyOp", "x")
						: Modifier.ModifierOp == EGameplayModOp::Division ? NSLOCTEXT("KnightSkillsUI", "DivideOp", "/")
						: NSLOCTEXT("KnightSkillsUI", "OverrideOp", "=");
					const FText Duration = Effect->Spec.Def->DurationPolicy == EGameplayEffectDurationType::HasDuration
						? FText::Format(NSLOCTEXT("KnightSkillsUI", "RemainingEffect", "{0}s remaining"),
							FText::AsNumber(FMath::Max(0.0f, Effect->GetTimeRemaining(ASC->GetWorld()->GetTimeSeconds()))))
						: NSLOCTEXT("KnightSkillsUI", "SourceManagedEffect", "active; expires with its source timer");
					Lines.Add(FText::Format(NSLOCTEXT("KnightSkillsUI", "ActiveStatModifier", "Active effect: {0}{1} ({2})"),
						Operation, FText::AsNumber(Magnitude), Duration));
				}
			}
			if (!bHasActiveModifier)
			{
				Lines.Add(NSLOCTEXT("KnightSkillsUI", "NoActiveStatModifiers", "Timed/active effects: +0"));
			}
			Lines.Add(FText::Format(NSLOCTEXT("KnightSkillsUI", "ConfirmedStatValue", "Confirmed effective value after modifiers/caps: {0}"),
				bPercent ? FText::AsPercent(Value) : FText::AsNumber(Value)));
			Lines.Add(Policy);
			Row.Contributions = FText::Join(NSLOCTEXT("KnightSkillsUI", "StatSourceNewline", "\n"), Lines);
			Snapshot.StatRows.Add(Row);
		};
		const FText General = NSLOCTEXT("KnightSkillsUI", "GeneralStats", "General");
		const FText Offense = NSLOCTEXT("KnightSkillsUI", "OffenseStats", "Offense");
		const FText Defense = NSLOCTEXT("KnightSkillsUI", "DefenseStats", "Defense");
		const FText Specialization = NSLOCTEXT("KnightSkillsUI", "SpecializationStats", "Specialization");
		AddStat(General, NSLOCTEXT("KnightSkillsUI", "MaxHP", "Max HP"), UKnightAttributeSet::GetMaxHealthAttribute(), Attributes->GetMaxHealth(), NSLOCTEXT("KnightSkillsUI", "HPPolicy", "Current HP stays absolute on build change; clamp only."));
		AddStat(General, NSLOCTEXT("KnightSkillsUI", "Stamina", "Max Stamina"), UKnightAttributeSet::GetMaxStaminaAttribute(), Attributes->GetMaxStamina(), NSLOCTEXT("KnightSkillsUI", "StaminaPolicy", "M3 movement/defense costs; skill lab regen 25/s."));
		AddStat(General, NSLOCTEXT("KnightSkillsUI", "ClassResource", "Max Class Resource"), UKnightAttributeSet::GetMaxRuneEnergyAttribute(), Attributes->GetMaxRuneEnergy(), NSLOCTEXT("KnightSkillsUI", "ResourcePolicy", "RuneEnergy; regen 10/s with a 1s pause after spending. Build changes never refill it."));
		AddStat(Offense, NSLOCTEXT("KnightSkillsUI", "Power", "Power"), UKnightAttributeSet::GetAttackPowerAttribute(), Attributes->GetAttackPower(), NSLOCTEXT("KnightSkillsUI", "PowerPolicy", "AttackPower; shared damage/heal/shield coefficients."));
		AddStat(Offense, NSLOCTEXT("KnightSkillsUI", "CritChance", "Crit chance"), UKnightAttributeSet::GetCritChanceAttribute(), Attributes->GetCritChance(), NSLOCTEXT("KnightSkillsUI", "CritPolicy", "Cap 40%; direct damage only. Current crit multiplier is shown separately."));
		AddStat(Offense, NSLOCTEXT("KnightSkillsUI", "CritMultiplier", "Crit multiplier"), UKnightAttributeSet::GetCritMultiplierAttribute(), Attributes->GetCritMultiplier(), NSLOCTEXT("KnightSkillsUI", "CritMultiplierPolicy", "Direct critical damage multiplier; heal, shield, periodic damage and reactions do not crit."));
		AddStat(Offense, NSLOCTEXT("KnightSkillsUI", "Haste", "Haste"), UKnightAttributeSet::GetHasteAttribute(), Attributes->GetHaste(), NSLOCTEXT("KnightSkillsUI", "HastePolicy", "Cap 30%; windup/(1+Haste). Does not accelerate melee, periodic ticks or regeneration."));
		AddStat(Defense, NSLOCTEXT("KnightSkillsUI", "Armor", "Armor"), UKnightAttributeSet::GetDefenseAttribute(), Attributes->GetDefense(), NSLOCTEXT("KnightSkillsUI", "ArmorPolicy", "Defense; Physical reduction A/(A+400). Earth/Stone use Physical."));
		AddStat(Defense, NSLOCTEXT("KnightSkillsUI", "DamageReduction", "Damage reduction"), UKnightAttributeSet::GetDamageReductionAttribute(), Attributes->GetDamageReduction(), NSLOCTEXT("KnightSkillsUI", "DamageReductionPolicy", "Global reduction, cap 50%. Conditional formation/layer/major-buff effects are described by their specialization skills."));
		AddStat(Defense, NSLOCTEXT("KnightSkillsUI", "ArcaneResistance", "Arcane resistance"), UKnightAttributeSet::GetArcaneResistanceAttribute(), Attributes->GetArcaneResistance(), NSLOCTEXT("KnightSkillsUI", "ArcanePolicy", "Clamp -25% to 60%; Light maps to Arcane."));
		AddStat(Defense, NSLOCTEXT("KnightSkillsUI", "LightningResistance", "Lightning resistance"), UKnightAttributeSet::GetLightningResistanceAttribute(), Attributes->GetLightningResistance(), NSLOCTEXT("KnightSkillsUI", "LightningPolicy", "Clamp -25% to 60%."));
		AddStat(Defense, NSLOCTEXT("KnightSkillsUI", "FrostResistance", "Frost resistance"), UKnightAttributeSet::GetFrostResistanceAttribute(), Attributes->GetFrostResistance(), NSLOCTEXT("KnightSkillsUI", "FrostPolicy", "Clamp -25% to 60%; Water maps to Frost."));
		AddStat(Defense, NSLOCTEXT("KnightSkillsUI", "FireResistance", "Fire resistance"), UKnightAttributeSet::GetFireResistanceAttribute(), Attributes->GetFireResistance(), NSLOCTEXT("KnightSkillsUI", "FirePolicy", "Clamp -25% to 60%."));
		AddStat(Specialization, NSLOCTEXT("KnightSkillsUI", "Mastery", "Mastery"), UKnightAttributeSet::GetMasteryBonusAttribute(), Attributes->GetMasteryBonus(), NSLOCTEXT("KnightSkillsUI", "MasteryPolicy", "Cap 30%; selected specialization signature hook once only."));
		AddStat(Specialization, NSLOCTEXT("KnightSkillsUI", "Versatility", "Versatility"), UKnightAttributeSet::GetVersatilityAttribute(), Attributes->GetVersatility(), NSLOCTEXT("KnightSkillsUI", "VersatilityPolicy", "Cap 20%; outgoing damage/heal, half strength incoming reduction. Does not scale shields."));
		AddStat(Specialization, NSLOCTEXT("KnightSkillsUI", "CooldownRecovery", "Cooldown Recovery"), UKnightAttributeSet::GetCooldownRecoveryAttribute(), Attributes->GetCooldownRecovery(), NSLOCTEXT("KnightSkillsUI", "CooldownPolicy", "Cap 30%; new cooldown base/(1+value). Running cooldowns remain unchanged."));
		if (const UKnightCombatComponent* Combat = Knight->GetCombatComponent())
		{
			if (const UKnightCombatProfileDefinition* Profile = Combat->GetProfile())
			{
				FKnightRPGStatRow Poise;
				Poise.Group = Defense;
				Poise.Name = NSLOCTEXT("KnightSkillsUI", "Poise", "Poise");
				Poise.Value = FText::Format(NSLOCTEXT("KnightSkillsUI", "PoiseValue", "{0}/{1}"),
					FText::AsNumber(Combat->GetSnapshot().Poise), FText::AsNumber(Combat->GetMaxPoise()));
				Poise.Contributions = FText::Format(NSLOCTEXT("KnightSkillsUI", "PoiseSources",
					"Combat profile base maximum: {0}; equipment contribution: {1}. Damage and regeneration use the M3 Poise pipeline; Saturn A4 reduces incoming Poise damage by 50%, without changing the maximum."),
					FText::AsNumber(Profile->GetTuning().MaxPoise), FText::AsNumber(View.StatBonuses.Poise));
				Snapshot.StatRows.Add(Poise);
			}
		}
	}
}
