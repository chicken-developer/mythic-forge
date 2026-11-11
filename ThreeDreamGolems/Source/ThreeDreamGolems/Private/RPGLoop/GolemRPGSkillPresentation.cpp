#include "RPGLoop/GolemRPGSkillPresentation.h"

#include "Assets/GolemAssetManager.h"
#include "Attributes/GolemAttributeSet.h"
#include "Character/GolemCharacter.h"
#include "Components/GolemAbilitySystemComponent.h"
#include "Components/GolemCombatComponent.h"
#include "Definitions/GolemAbilitySetDefinition.h"
#include "Definitions/GolemCombatProfileDefinition.h"
#include "Definitions/GolemItemDefinition.h"
#include "Definitions/GolemRuneDefinition.h"
#include "Definitions/GolemSkillDefinition.h"
#include "Effects/GolemSourceProjectionEffect.h"
#include "Engine/World.h"
#include "GameplayEffect.h"
#include "Skills/GolemSkillTypes.h"
#include "Tags/GolemGameplayTags.h"

namespace GolemRPGSkillPresentation
{
	namespace
	{
		FText HeroDisplayName(const FPrimaryAssetId& HeroId)
		{
			const FName Id = HeroId.PrimaryAssetName;
			if (Id == TEXT("Training_Red"))
			{
				return NSLOCTEXT("GolemSkillsUI", "Sky", "Sky");
			}
			if (Id == TEXT("Training_Blue"))
			{
				return NSLOCTEXT("GolemSkillsUI", "Sea", "Sea");
			}
			if (Id == TEXT("Training_Green"))
			{
				return NSLOCTEXT("GolemSkillsUI", "Earth", "Earth");
			}
			return FText::FromName(Id);
		}

		float StatBonus(const FGolemItemStatBonuses& Bonuses, const FGameplayAttribute& Attribute)
		{
			if (Attribute == UGolemAttributeSet::GetMaxHealthAttribute())
			{
				return Bonuses.MaxHealth;
			}
			if (Attribute == UGolemAttributeSet::GetMaxStaminaAttribute())
			{
				return Bonuses.MaxStamina;
			}
			if (Attribute == UGolemAttributeSet::GetMaxRuneEnergyAttribute())
			{
				return Bonuses.MaxRuneEnergy;
			}
			if (Attribute == UGolemAttributeSet::GetAttackPowerAttribute())
			{
				return Bonuses.AttackPower;
			}
			if (Attribute == UGolemAttributeSet::GetCritChanceAttribute())
			{
				return Bonuses.CritChance;
			}
			if (Attribute == UGolemAttributeSet::GetHasteAttribute())
			{
				return Bonuses.Haste;
			}
			if (Attribute == UGolemAttributeSet::GetCooldownRecoveryAttribute())
			{
				return Bonuses.CooldownRecovery;
			}
			if (Attribute == UGolemAttributeSet::GetMasteryBonusAttribute())
			{
				return Bonuses.MasteryBonus;
			}
			if (Attribute == UGolemAttributeSet::GetVersatilityAttribute())
			{
				return Bonuses.Versatility;
			}
			return Attribute == UGolemAttributeSet::GetDefenseAttribute() ? Bonuses.Defense : 0.0f;
		}

		struct FGolemStatSource
		{
			FText Name;
			FGolemItemStatBonuses Bonuses;
			TArray<TSubclassOf<UGameplayEffect>> PassiveEffects;
		};

		FGolemRPGSpecRow Spec(const FName Id)
		{
			FGolemRPGSpecRow Row;
			Row.SpecId = Id;
			Row.Name = FText::FromName(Id);
			const EGolemSkillRole Role = FGolemSkillCatalog::Role(Id);
			Row.Role = Role == EGolemSkillRole::Tank ? NSLOCTEXT("GolemSkillsUI", "Tank", "Tank")
				: (Role == EGolemSkillRole::Healer ? NSLOCTEXT("GolemSkillsUI", "Healer", "Healer")
				: NSLOCTEXT("GolemSkillsUI", "Damage", "DPS"));
			FGolemSkillBuild Build;
			Build.SpecId = Id;
			Row.MajorBuff = FGolemSkillCatalog::GetSkill(Build, 4).DisplayName;
			if (Id == TEXT("Uranus"))
			{
				Row.PassiveName = NSLOCTEXT("GolemSkillsUI", "UranusPassive", "Tich Quang / Stored Light");
				Row.PassiveTooltip = NSLOCTEXT("GolemSkillsUI", "UranusLoop", "Block/parry gains up to3 light stacks (1/s,8s). A3 consumes all for +10% shield each. A4 protects the rear60 degree cone against frontal sources,25% DR.");
				Row.ChoiceA = NSLOCTEXT("GolemSkillsUI", "UranusA", "A: A3 shield absorption x1.15");
				Row.ChoiceB = NSLOCTEXT("GolemSkillsUI", "UranusB", "B: A4 formation duration 8s -> 10s");
			}
			else if (Id == TEXT("Venus"))
			{
				Row.PassiveName = NSLOCTEXT("GolemSkillsUI", "VenusPassive", "Du Quang / Afterglow");
				Row.PassiveTooltip = NSLOCTEXT("GolemSkillsUI", "VenusLoop", "Actual direct heal gains1 stack/cast, max3/10s. Next direct cast consumes3 for +30% heal and half cost. A4 gives25% actual heal as shield,5s, cap15% recipient HP.");
				Row.ChoiceA = NSLOCTEXT("GolemSkillsUI", "VenusA", "A: A2 direct healing x1.15");
				Row.ChoiceB = NSLOCTEXT("GolemSkillsUI", "VenusB", "B: A3 healing radius 5m -> 7m");
			}
			else if (Id == TEXT("Jupiter"))
			{
				Row.PassiveName = NSLOCTEXT("GolemSkillsUI", "JupiterPassive", "Tinh Dien / Static Charge");
				Row.PassiveTooltip = NSLOCTEXT("GolemSkillsUI", "JupiterLoop", "A1 hit gains charge, max5/10s. A2 consumes for +6% direct damage/stack. A4 doubles builder gain and adds30% pre-defense lightning to2 other enemies; secondary cannot proc.");
				Row.ChoiceA = NSLOCTEXT("GolemSkillsUI", "JupiterA", "A: A2 raw direct coefficient x1.15");
				Row.ChoiceB = NSLOCTEXT("GolemSkillsUI", "JupiterB", "B: A4 secondary targets 2 -> 3");
			}
			else if (Id == TEXT("Neptune"))
			{
				Row.PassiveName = NSLOCTEXT("GolemSkillsUI", "NeptunePassive", "Du Ap / Deferred Pressure");
				Row.PassiveTooltip = NSLOCTEXT("GolemSkillsUI", "NeptuneLoop", "25% eligible damage after mitigation/block becomes five HP-only ticks, before shield. A4 clears50% outstanding debt capped20% own MaxHP, then15% DR6s. Cannot switch while debt remains.");
				Row.ChoiceA = NSLOCTEXT("GolemSkillsUI", "NeptuneA", "A: A3 shield absorption x1.15");
				Row.ChoiceB = NSLOCTEXT("GolemSkillsUI", "NeptuneB", "B: A4 damage reduction duration 6s -> 8s");
			}
			else if (Id == TEXT("Mercury"))
			{
				Row.PassiveName = NSLOCTEXT("GolemSkillsUI", "MercuryPassive", "Dong Sinh Luc / Life Current");
				Row.PassiveTooltip = NSLOCTEXT("GolemSkillsUI", "MercuryLoop", "Each distinct Golem with own main HoT grants4% healing, cap12%. Snapshot on apply. A4 main HoT ticks add30% pre-clamp heal to the lowest HP other ally within6m; no recursion.");
				Row.ChoiceA = NSLOCTEXT("GolemSkillsUI", "MercuryA", "A: A2 HoT tick coefficient x1.15");
				Row.ChoiceB = NSLOCTEXT("GolemSkillsUI", "MercuryB", "B: A4 secondary healing radius 6m -> 8m");
			}
			else if (Id == TEXT("Pluto"))
			{
				Row.PassiveName = NSLOCTEXT("GolemSkillsUI", "PlutoPassive", "Chuyen Pha / Phase Shift");
				Row.PassiveTooltip = NSLOCTEXT("GolemSkillsUI", "PlutoLoop", "Water hit empowers next Ice +20%; Ice hit discounts next Water25%,6s. A1 adds Chilled; A3 adds2. A2 at3 consumes for Shatter0.6Power, shared target ICD4s. A4 Ice penetrates20% positive Frost resistance and Shatter+25%.");
				Row.ChoiceA = NSLOCTEXT("GolemSkillsUI", "PlutoA", "A: A2 raw direct coefficient x1.15");
				Row.ChoiceB = NSLOCTEXT("GolemSkillsUI", "PlutoB", "B: A3 ice area radius 4m -> 5m");
			}
			else if (Id == TEXT("Saturn"))
			{
				Row.PassiveName = NSLOCTEXT("GolemSkillsUI", "SaturnPassive", "Thach Tang / Stone Layers");
				Row.PassiveTooltip = NSLOCTEXT("GolemSkillsUI", "SaturnLoop", "Heavy hit or successful A3 grants1 layer/2s, max4; common8s refresh,3% DR each. A4 holds4 layers8s and halves poise damage. No forced mechanic immunity.");
				Row.ChoiceA = NSLOCTEXT("GolemSkillsUI", "SaturnA", "A: A3 shield absorption x1.15");
				Row.ChoiceB = NSLOCTEXT("GolemSkillsUI", "SaturnB", "B: A4 Mountain Heart duration 8s -> 10s");
			}
			else if (Id == TEXT("Terra"))
			{
				Row.PassiveName = NSLOCTEXT("GolemSkillsUI", "TerraPassive", "Than Hong Sinh Menh / Life Embers");
				Row.PassiveTooltip = NSLOCTEXT("GolemSkillsUI", "TerraLoop", "Actual A3 heal gains1 ember/cast, max3/10s. Next consumes3 for30% base heal as HoT6s. A4 heal below40% HP gains25%;30% overheal becomes shield5s, cap15% recipient HP. Pillar ticks do not build embers.");
				Row.ChoiceA = NSLOCTEXT("GolemSkillsUI", "TerraA", "A: A2 pillar healing tick coefficient x1.15");
				Row.ChoiceB = NSLOCTEXT("GolemSkillsUI", "TerraB", "B: A2 pillar radius 5m -> 7m; Mobile Rune retains its 3m radius");
			}
			else
			{
				Row.PassiveName = NSLOCTEXT("GolemSkillsUI", "MarsPassive", "Nhiet Lo / Furnace Heat");
				Row.PassiveTooltip = NSLOCTEXT("GolemSkillsUI", "MarsLoop", "Earth/Stone direct hit gains Heat once/cast, max5/10s. Fire A2 consumes all for Burn:8% raw direct/stack over6s. A4 doubles builder gain and refunds2 Heat after actual consume. Ticks cannot proc.");
				Row.ChoiceA = NSLOCTEXT("GolemSkillsUI", "MarsA", "A: A2 raw direct coefficient x1.15");
				Row.ChoiceB = NSLOCTEXT("GolemSkillsUI", "MarsB", "B: A3 ground strip width 2m -> 3m");
			}
			Row.Summary = Row.PassiveTooltip;
			return Row;
		}

		FText TargetingName(const EGolemSkillTargeting Targeting)
		{
			switch (Targeting)
			{
			case EGolemSkillTargeting::Cone: return NSLOCTEXT("GolemSkillsUI", "Cone", "Cone");
			case EGolemSkillTargeting::Radius: return NSLOCTEXT("GolemSkillsUI", "Radius", "Radius");
			case EGolemSkillTargeting::GroundStrip: return NSLOCTEXT("GolemSkillsUI", "GroundStrip", "Ground strip");
			case EGolemSkillTargeting::GroundArea: return NSLOCTEXT("GolemSkillsUI", "GroundArea", "Fixed ground area");
			case EGolemSkillTargeting::FollowingAlly: return NSLOCTEXT("GolemSkillsUI", "FollowingAlly", "Following chosen ally");
			case EGolemSkillTargeting::Self: return NSLOCTEXT("GolemSkillsUI", "Self", "Self");
			case EGolemSkillTargeting::Dash: return NSLOCTEXT("GolemSkillsUI", "Dash", "Dash");
			default: return NSLOCTEXT("GolemSkillsUI", "Single", "Single primary target");
			}
		}

		TArray<FGolemRPGSpellRow> Spells(const FGolemSkillBuild& Build, const float Power)
		{
			TArray<FGolemRPGSpellRow> Rows;
			if (!FGolemSkillCatalog::IsKnownSpec(Build.SpecId))
			{
				return Rows;
			}
			FGolemSkillBuild BaseBuild;
			BaseBuild.SpecId = Build.SpecId;
			FGolemSkillBuild TalentedBuild = Build;
			TalentedBuild.SkillRune = NAME_None;
			TalentedBuild.WeaponRune = NAME_None;
			for (int32 Slot = 1; Slot <= 4; ++Slot)
			{
				const FGolemSpecActionDefinition Definition = FGolemSkillCatalog::GetSkill(Build, Slot);
				const FGolemSpecActionDefinition Base = FGolemSkillCatalog::GetSkill(BaseBuild, Slot);
				const FGolemSpecActionDefinition Talented = FGolemSkillCatalog::GetSkill(TalentedBuild, Slot);
				FGolemRPGSpellRow Row;
				Row.AbilityId = Definition.AbilityId;
				Row.Name = Definition.DisplayName;
				Row.Category = TEXT("Spec");
				Row.Slot = Slot;
				const FText Targeting = Definition.bPeriodic ? NSLOCTEXT("GolemSkillsUI", "Periodic", "Periodic: first tick at +1s")
					: (Definition.bFriendly ? NSLOCTEXT("GolemSkillsUI", "Friendly", "Ally/self")
					: NSLOCTEXT("GolemSkillsUI", "Enemy", "Enemy, LOS required"));
				const FText Scaling = Definition.Role == EGolemSkillRole::Healer && Slot > 1
					? NSLOCTEXT("GolemSkillsUI", "HealEstimate", "healing; no crit")
					: (Definition.Role == EGolemSkillRole::Tank && Slot == 3
					? NSLOCTEXT("GolemSkillsUI", "ShieldEstimate", "shield absorption; no crit/Versatility")
					: NSLOCTEXT("GolemSkillsUI", "DamageEstimate", "raw direct damage, before target defense/crit"));
				Row.Summary = FText::Format(NSLOCTEXT("GolemSkillsUI", "SkillEstimateTyped", "A{0}: {1} | {2} Power = {3} {4}"),
					FText::AsNumber(Slot), Targeting, FText::AsNumber(Definition.Coefficient),
					FText::AsNumber(Definition.Coefficient * Power), Scaling);
				Row.Tooltip = FText::Format(NSLOCTEXT("GolemSkillsUI", "SkillDetails",
					"{0}\nCost {1}, CD {2}s; range {3}m, radius {4}m; duration {5}s\nWindup {6}s, recovery {7}s; poise {8}. Haste affects windup only. CD Recovery affects new cooldowns only. School {9} (Water=Frost, Light=Arcane, Earth/Stone=Physical).\nRune: {10}"),
					Row.Summary, FText::AsNumber(Definition.Cost), FText::AsNumber(Definition.Cooldown),
					FText::AsNumber(Definition.Range / 100.0f), FText::AsNumber(Definition.Radius / 100.0f),
					FText::AsNumber(Definition.Duration), FText::AsNumber(Definition.Windup),
					FText::AsNumber(Definition.Recovery), FText::AsNumber(Definition.PoiseDamage),
					FText::FromString(StaticEnum<EGolemSkillSchool>()->GetNameStringByValue(static_cast<int64>(Definition.School))),
					FText::FromName(Build.SkillRune));
				Row.Tooltip = FText::Format(NSLOCTEXT("GolemSkillsUI", "SkillComparison",
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
					Row.Summary = FText::Format(NSLOCTEXT("GolemSkillsUI", "MajorSummary", "A4: {0}, {1}s; cooldown {2}s"),
						Definition.DisplayName, FText::AsNumber(Definition.Duration), FText::AsNumber(Definition.Cooldown));
					Row.Tooltip = FText::Format(NSLOCTEXT("GolemSkillsUI", "MajorDetails", "{0}\n{1}\nSelf buff. Windup {2}s; recovery {3}s. Uses the shared SpecMajor cooldown; changing specialization preserves remaining time."),
						Row.Summary, Spec(Build.SpecId).PassiveTooltip, FText::AsNumber(Definition.Windup),
						FText::AsNumber(Definition.Recovery));
				}
				if (Definition.Targeting == EGolemSkillTargeting::Dash)
				{
					Row.Summary = FText::Format(NSLOCTEXT("GolemSkillsUI", "DashSummary", "A{0}: Dash {1}m; no invulnerability"),
						FText::AsNumber(Slot), FText::AsNumber(Definition.Range / 100.0f));
					Row.Tooltip = FText::Format(NSLOCTEXT("GolemSkillsUI", "DashDetails", "{0}\nCost {1}; cooldown {2}s. Collision and the M3 dodge/defense policy remain active."),
						Row.Summary, FText::AsNumber(Definition.Cost), FText::AsNumber(Definition.Cooldown));
				}
				Rows.Add(Row);
			}
			return Rows;
		}
	}

	FGolemRPGBuildPreview Preview(const FGolemSkillBuild& Build, const float Power)
	{
		FGolemRPGBuildPreview Result;
		Result.Build = Build;
		Result.Spells = Spells(Build, Power);
		Result.Summary = FText::Format(NSLOCTEXT("GolemSkillsUI", "PurePreview", "DRAFT {0}: class {1}, spec choice {2}. Rune {3}. Definition preview only; live HP/resource/cooldowns are unchanged."),
			FText::FromName(Build.SpecId), FText::FromName(Build.ClassChoice), FText::FromName(Build.SpecChoice),
			FText::FromName(Build.SkillRune));
		return Result;
	}

	void Enrich(FGolemRPGPanelSnapshot& Snapshot, const FGolemRPGGolemView& View,
		const FGolemRPGView& PartyView, const FGolemInventoryView& Inventory, const AGolemCharacter* Golem)
	{
		Snapshot.Build = View.Build;
		if (!View.Build.SpecId.IsNone())
		{
			Snapshot.Title = FText::Format(NSLOCTEXT("GolemSkillsUI", "CharacterTitle", "{0} — Character & Build"),
				HeroDisplayName(View.HeroDefinitionId));
		}
		Snapshot.Revision = View.Revision;
		Snapshot.AvailablePoints = View.SkillPoints;
		Snapshot.WeaponRuneInstanceId = View.WeaponRuneInstanceId;
		Snapshot.SkillRuneInstanceId = View.SkillRuneInstanceId;
		Snapshot.ActiveTalentLoadoutId = View.ActiveTalentLoadoutId;
		for (const FGolemRPGGolemView& Entry : PartyView.Golems)
		{
			FGolemRPGGolemChoice Row;
			Row.GolemId = Entry.GolemId;
			Row.Name = HeroDisplayName(Entry.HeroDefinitionId);
			Snapshot.Golems.Add(Row);
		}
		for (const FName Id : FGolemSkillCatalog::SpecsForHero(View.HeroDefinitionId))
		{
			Snapshot.Specializations.Add(Spec(Id));
		}
		for (const FGolemTalentLoadout& Preset : View.TalentLoadouts)
		{
			FGolemRPGTalentLoadoutRow Row;
			Row.LoadoutId = Preset.LoadoutId;
			Row.Name = Preset.Name;
			Row.Build = Preset.Build;
			Row.Version = Preset.Version;
			Snapshot.TalentLoadouts.Add(Row);
		}
		const UGolemAttributeSet* Attributes = Golem->GetGolemAttributeSet();
		Snapshot.Spellbook = Spells(View.Build, Attributes->GetAttackPower());
		if (!View.Build.SpecId.IsNone())
		{
			const FGolemRPGSpecRow Current = Spec(View.Build.SpecId);
			FGolemRPGSpellRow Passive;
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
			FGolemRPGSpellRow General;
			General.AbilityId = Id;
			General.Name = FText::FromName(Id);
			General.Category = TEXT("General");
			General.Tooltip = NSLOCTEXT("GolemSkillsUI", "CoreAction", "Core combat action, free. Uses existing M3 input, stamina/cancel policy. Does not occupy A1-A4.");
			Snapshot.Spellbook.Add(General);
		}
		UGolemAssetManager* Manager = UGolemAssetManager::GetGolemAssetManager();
		for (const FGolemItemInstance& Item : Inventory.Items)
		{
			UGolemDefinition* Definition = nullptr;
			if (Manager == nullptr || !Manager->ResolveDefinition(Item.DefinitionId,
				UGolemRuneDefinition::StaticClass(), Definition).IsSuccess())
			{
				continue;
			}
			const UGolemRuneDefinition* Rune = Cast<UGolemRuneDefinition>(Definition);
			if (Rune == nullptr || Rune->GetAttachmentRuneId().IsNone())
			{
				continue;
			}
			FGolemRPGRuneRow Row;
			Row.RuneId = Rune->GetAttachmentRuneId();
			Row.ItemInstanceId = Item.InstanceId;
			Row.Name = Rune->GetDisplayName();
			Row.bWeaponSocket = Row.RuneId == TEXT("Weapon.Aftershock");
			Row.bCompatible = FGolemSkillCatalog::IsRuneCompatible(View.Build.SpecId, Row.RuneId);
			if (View.bGearEnabled && Item.LocationId != TEXT("Inventory") &&
				!(Item.AssignedGolemId == View.GolemId &&
					(Item.InstanceId == View.WeaponRuneInstanceId || Item.InstanceId == View.SkillRuneInstanceId)))
			{
				Row.bCompatible = false;
				Row.IncompatibilityReason = NSLOCTEXT("GolemGearUI", "RuneLocationOccupied", "This exact Rune is already attached to a weapon or another Golem. Detach it first.");
			}
			if (Row.bWeaponSocket)
			{
				const FGolemEquipmentAssignment* Weapon = View.FindAssignment(GolemGameplayTags::Slot_Equipment_Weapon);
				UGolemDefinition* WeaponDefinition = nullptr;
				if (Weapon == nullptr || !Manager->ResolveDefinition(Weapon->DefinitionId,
					UGolemItemDefinition::StaticClass(), WeaponDefinition).IsSuccess()
					|| CastChecked<UGolemItemDefinition>(WeaponDefinition)->GetWeaponFamily() != TEXT("HeavyHammer"))
				{
					Row.bCompatible = false;
					Row.IncompatibilityReason = NSLOCTEXT("GolemSkillsUI", "AftershockWeaponRequired",
						"Equip an M5 HeavyHammer before attaching Aftershock to its weapon socket.");
				}
			}
			if (Item.Count != 1)
			{
				Row.bCompatible = false;
				Row.IncompatibilityReason = NSLOCTEXT("GolemSkillsUI", "UnstackableAttachmentRequired",
					"Attach Runes require one exact unstackable source item.");
			}
			for (const FGolemRPGGolemView& Entry : PartyView.Golems)
			{
				if (Entry.Assignments.ContainsByPredicate([&Item](const FGolemEquipmentAssignment& Assignment)
					{
						return Assignment.ItemInstanceId == Item.InstanceId;
					}))
				{
					Row.bCompatible = false;
					Row.IncompatibilityReason = NSLOCTEXT("GolemSkillsUI", "RuneAlreadyEquipped",
						"This exact rune instance is already assigned to an M5 equipment slot.");
				}
				if (Entry.GolemId != View.GolemId && (Entry.WeaponRuneInstanceId == Item.InstanceId
					|| Entry.SkillRuneInstanceId == Item.InstanceId))
				{
					Row.bCompatible = false;
					Row.IncompatibilityReason = NSLOCTEXT("GolemSkillsUI", "RuneAssigned", "This exact rune instance is attached to another Golem.");
				}
			}
			if (!Row.bCompatible && Row.IncompatibilityReason.IsEmpty())
			{
				Row.IncompatibilityReason = NSLOCTEXT("GolemSkillsUI", "RuneIncompatible", "The selected specialization has no compatible skill for this rune.");
			}
			Row.Description = Row.RuneId == TEXT("Skill.Mobile")
				? NSLOCTEXT("GolemSkillsUI", "Mobile", "Terra A2: ground-fixed5m/6s -> follows chosen ally3m/4s. Tick cadence, cost and cooldown preserved.")
				: (Row.RuneId == TEXT("Skill.Convergence")
				? NSLOCTEXT("GolemSkillsUI", "Convergence", "Tank/Mars A2: cone -> primary only; raw damage x1.20, Mars Burn uses modified raw once.")
				: (Row.RuneId == TEXT("Skill.Propagation")
				? NSLOCTEXT("GolemSkillsUI", "Propagation", "Venus A2/Mercury A3/Terra A3: primary heal x0.85; one other ally within6m gets30% pre-clamp heal. Secondary cannot proc.")
				: NSLOCTEXT("GolemSkillsUI", "Aftershock", "Heavy hammer hit: Aftershock0.3Power Physical, radius2m,max3,poise5,ICD4s; cannot chain.")));
			Snapshot.Runes.Add(Row);
		}
		TArray<FGolemStatSource> Sources;
		TSet<UClass*> OwnedPassiveClasses;
		const auto AddSource = [&Sources, &OwnedPassiveClasses, Manager](const FText& Name,
			const FGolemItemStatBonuses& Bonuses, const FPrimaryAssetId& AbilitySetId)
		{
			FGolemStatSource Source;
			Source.Name = Name;
			Source.Bonuses = Bonuses;
			UGolemDefinition* Resolved = nullptr;
			if (Manager != nullptr && AbilitySetId.IsValid()
				&& Manager->ResolveDefinition(AbilitySetId, UGolemAbilitySetDefinition::StaticClass(), Resolved).IsSuccess())
			{
				Source.PassiveEffects = CastChecked<UGolemAbilitySetDefinition>(Resolved)->GetEffects();
				for (TSubclassOf<UGameplayEffect> EffectClass : Source.PassiveEffects)
				{
					OwnedPassiveClasses.Add(EffectClass.Get());
				}
			}
			Sources.Add(MoveTemp(Source));
		};
		for (const FGolemEquipmentAssignment& Assignment : View.Assignments)
		{
			UGolemDefinition* Resolved = nullptr;
			if (Manager != nullptr && Manager->ResolveDefinition(Assignment.DefinitionId,
				UGolemItemDefinition::StaticClass(), Resolved).IsSuccess())
			{
				const UGolemItemDefinition* Item = CastChecked<UGolemItemDefinition>(Resolved);
				const FText Label = Item->GetItemKind() == EGolemItemKind::Rune
					? NSLOCTEXT("GolemSkillsUI", "RuneSource", "Rune: {0}")
					: NSLOCTEXT("GolemSkillsUI", "EquipmentSource", "Equipment: {0}");
				const FGolemItemInstance* Instance = Inventory.Items.FindByPredicate([&Assignment](const FGolemItemInstance& Candidate)
				{
					return Candidate.InstanceId == Assignment.ItemInstanceId;
				});
				AddSource(FText::Format(Label, Item->GetDisplayName()),
					Item->GetEffectiveStats(Instance != nullptr ? Instance->UpgradeRank : 0), Item->GetAbilitySetId());
			}
		}
		for (const FGolemPurchasedSkill& Purchase : View.PurchasedSkills)
		{
			UGolemDefinition* Resolved = nullptr;
			if (Manager != nullptr && Manager->ResolveDefinition(Purchase.DefinitionId,
				UGolemSkillDefinition::StaticClass(), Resolved).IsSuccess())
			{
				const UGolemSkillDefinition* Skill = CastChecked<UGolemSkillDefinition>(Resolved);
				AddSource(FText::Format(NSLOCTEXT("GolemSkillsUI", "PurchasedTalentSource", "Purchased talent: {0}"),
					Skill->GetDisplayName()), Skill->GetStatBonuses(), Skill->GetAbilitySetId());
			}
		}
		FGolemItemStatBonuses ClassBonuses;
		ClassBonuses.MaxStamina = View.Build.ClassChoice == TEXT("Endurance") ? 10.0f : 0.0f;
		ClassBonuses.MaxRuneEnergy = View.Build.ClassChoice == TEXT("Reservoir") ? 10.0f : 0.0f;
		AddSource(FText::Format(NSLOCTEXT("GolemSkillsUI", "ClassTalentSource", "Class talent: {0}"),
			View.Build.ClassChoice.IsNone() ? NSLOCTEXT("GolemSkillsUI", "NoChoice", "None")
				: FText::FromName(View.Build.ClassChoice)), ClassBonuses, FPrimaryAssetId());
		const UGolemAbilitySystemComponent* ASC = Golem->GetGolemAbilitySystem();
		const TArray<FActiveGameplayEffectHandle> EffectHandles = ASC->GetActiveEffects(FGameplayEffectQuery());
		const FGolemRPGSpecRow CurrentSpec = Spec(View.Build.SpecId);
		const FText SpecChoice = View.Build.SpecChoice == TEXT("A") ? CurrentSpec.ChoiceA
			: View.Build.SpecChoice == TEXT("B") ? CurrentSpec.ChoiceB
			: NSLOCTEXT("GolemSkillsUI", "NoSpecTalent", "None");
		const auto AddStat = [&Snapshot, &Sources, &OwnedPassiveClasses, &EffectHandles, ASC, &SpecChoice, &View](
			const FText& Group, const FText& Name, const FGameplayAttribute& Attribute, const float Value,
			const FText& Policy)
		{
			FGolemRPGStatRow Row;
			Row.Group = Group;
			Row.Name = Name;
			const bool bPercent = Attribute == UGolemAttributeSet::GetCritChanceAttribute()
				|| Attribute == UGolemAttributeSet::GetHasteAttribute()
				|| Attribute == UGolemAttributeSet::GetArcaneResistanceAttribute()
				|| Attribute == UGolemAttributeSet::GetLightningResistanceAttribute()
				|| Attribute == UGolemAttributeSet::GetFrostResistanceAttribute()
				|| Attribute == UGolemAttributeSet::GetFireResistanceAttribute()
				|| Attribute == UGolemAttributeSet::GetMasteryBonusAttribute()
				|| Attribute == UGolemAttributeSet::GetVersatilityAttribute()
				|| Attribute == UGolemAttributeSet::GetCooldownRecoveryAttribute()
				|| Attribute == UGolemAttributeSet::GetDamageReductionAttribute();
			Row.Value = bPercent ? FText::AsPercent(Value) : FText::AsNumber(Value);
			TArray<FText> Lines;
			Lines.Add(FText::Format(NSLOCTEXT("GolemSkillsUI", "BaseAttribute", "Base: {0}"),
				bPercent ? FText::AsPercent(ASC->GetNumericAttributeBase(Attribute)) : FText::AsNumber(ASC->GetNumericAttributeBase(Attribute))));
			for (const FGolemStatSource& Source : Sources)
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
				Lines.Add(FText::Format(bPercent ? NSLOCTEXT("GolemGearUI", "PercentContribution", "{0}: +{1} percentage points") :
					NSLOCTEXT("GolemSkillsUI", "NamedStatContribution", "{0}: +{1}"),
					Source.Name, FText::AsNumber(bPercent ? Bonus * 100.0f : Bonus)));
			}
			Lines.Add(FText::Format(NSLOCTEXT("GolemSkillsUI", "SpecStatContribution", "Spec talent: {0}; no flat attribute bonus"), SpecChoice));
			Lines.Add(FText::Format(NSLOCTEXT("GolemSkillsUI", "AttachedRuneStatContribution",
				"Attached weapon rune: {0}; skill rune: {1}. They modify abilities; flat attribute bonus +0."),
				View.Build.WeaponRune.IsNone() ? NSLOCTEXT("GolemSkillsUI", "NoWeaponRune", "None") : FText::FromName(View.Build.WeaponRune),
				View.Build.SkillRune.IsNone() ? NSLOCTEXT("GolemSkillsUI", "NoSkillRune", "None") : FText::FromName(View.Build.SkillRune)));
			bool bHasActiveModifier = false;
			for (FActiveGameplayEffectHandle Handle : EffectHandles)
			{
				const FActiveGameplayEffect* Effect = ASC->GetActiveGameplayEffect(Handle);
				if (Effect == nullptr || Effect->bIsInhibited || Effect->Spec.Def == nullptr
					|| Effect->Spec.Def->IsA<UGolemSourceProjectionEffect>()
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
					const FText Operation = Modifier.ModifierOp == EGameplayModOp::Additive ? NSLOCTEXT("GolemSkillsUI", "AddOp", "+")
						: Modifier.ModifierOp == EGameplayModOp::Multiplicitive ? NSLOCTEXT("GolemSkillsUI", "MultiplyOp", "x")
						: Modifier.ModifierOp == EGameplayModOp::Division ? NSLOCTEXT("GolemSkillsUI", "DivideOp", "/")
						: NSLOCTEXT("GolemSkillsUI", "OverrideOp", "=");
					const FText Duration = Effect->Spec.Def->DurationPolicy == EGameplayEffectDurationType::HasDuration
						? FText::Format(NSLOCTEXT("GolemSkillsUI", "RemainingEffect", "{0}s remaining"),
							FText::AsNumber(FMath::Max(0.0f, Effect->GetTimeRemaining(ASC->GetWorld()->GetTimeSeconds()))))
						: NSLOCTEXT("GolemSkillsUI", "SourceManagedEffect", "active; expires with its source timer");
					Lines.Add(FText::Format(NSLOCTEXT("GolemSkillsUI", "ActiveStatModifier", "Active effect: {0}{1} ({2})"),
						Operation, FText::AsNumber(Magnitude), Duration));
				}
			}
			if (!bHasActiveModifier)
			{
				Lines.Add(NSLOCTEXT("GolemSkillsUI", "NoActiveStatModifiers", "Timed/active effects: +0"));
			}
			Lines.Add(FText::Format(NSLOCTEXT("GolemSkillsUI", "ConfirmedStatValue", "Confirmed effective value after modifiers/caps: {0}"),
				bPercent ? FText::AsPercent(Value) : FText::AsNumber(Value)));
			Lines.Add(Policy);
			Row.Contributions = FText::Join(NSLOCTEXT("GolemSkillsUI", "StatSourceNewline", "\n"), Lines);
			Snapshot.StatRows.Add(Row);
		};
		const FText General = NSLOCTEXT("GolemSkillsUI", "GeneralStats", "General");
		const FText Offense = NSLOCTEXT("GolemSkillsUI", "OffenseStats", "Offense");
		const FText Defense = NSLOCTEXT("GolemSkillsUI", "DefenseStats", "Defense");
		const FText Specialization = NSLOCTEXT("GolemSkillsUI", "SpecializationStats", "Specialization");
		AddStat(General, NSLOCTEXT("GolemSkillsUI", "MaxHP", "Max HP"), UGolemAttributeSet::GetMaxHealthAttribute(), Attributes->GetMaxHealth(), NSLOCTEXT("GolemSkillsUI", "HPPolicy", "Current HP stays absolute on build change; clamp only."));
		AddStat(General, NSLOCTEXT("GolemSkillsUI", "Stamina", "Max Stamina"), UGolemAttributeSet::GetMaxStaminaAttribute(), Attributes->GetMaxStamina(), NSLOCTEXT("GolemSkillsUI", "StaminaPolicy", "M3 movement/defense costs; skill lab regen 25/s."));
		AddStat(General, NSLOCTEXT("GolemSkillsUI", "ClassResource", "Max Class Resource"), UGolemAttributeSet::GetMaxRuneEnergyAttribute(), Attributes->GetMaxRuneEnergy(), NSLOCTEXT("GolemSkillsUI", "ResourcePolicy", "RuneEnergy; regen 10/s with a 1s pause after spending. Build changes never refill it."));
		AddStat(Offense, NSLOCTEXT("GolemSkillsUI", "Power", "Power"), UGolemAttributeSet::GetAttackPowerAttribute(), Attributes->GetAttackPower(), NSLOCTEXT("GolemSkillsUI", "PowerPolicy", "AttackPower; shared damage/heal/shield coefficients."));
		AddStat(Offense, NSLOCTEXT("GolemSkillsUI", "CritChance", "Crit chance"), UGolemAttributeSet::GetCritChanceAttribute(), Attributes->GetCritChance(), NSLOCTEXT("GolemSkillsUI", "CritPolicy", "Cap 40%; direct damage only. Current crit multiplier is shown separately."));
		AddStat(Offense, NSLOCTEXT("GolemSkillsUI", "CritMultiplier", "Crit multiplier"), UGolemAttributeSet::GetCritMultiplierAttribute(), Attributes->GetCritMultiplier(), NSLOCTEXT("GolemSkillsUI", "CritMultiplierPolicy", "Direct critical damage multiplier; heal, shield, periodic damage and reactions do not crit."));
		AddStat(Offense, NSLOCTEXT("GolemSkillsUI", "Haste", "Haste"), UGolemAttributeSet::GetHasteAttribute(), Attributes->GetHaste(), NSLOCTEXT("GolemSkillsUI", "HastePolicy", "Cap 30%; windup/(1+Haste). Does not accelerate melee, periodic ticks or regeneration."));
		AddStat(Defense, NSLOCTEXT("GolemSkillsUI", "Armor", "Armor"), UGolemAttributeSet::GetDefenseAttribute(), Attributes->GetDefense(), NSLOCTEXT("GolemSkillsUI", "ArmorPolicy", "Defense; Physical reduction A/(A+400). Earth/Stone use Physical."));
		AddStat(Defense, NSLOCTEXT("GolemSkillsUI", "DamageReduction", "Damage reduction"), UGolemAttributeSet::GetDamageReductionAttribute(), Attributes->GetDamageReduction(), NSLOCTEXT("GolemSkillsUI", "DamageReductionPolicy", "Global reduction, cap 50%. Conditional formation/layer/major-buff effects are described by their specialization skills."));
		AddStat(Defense, NSLOCTEXT("GolemSkillsUI", "ArcaneResistance", "Arcane resistance"), UGolemAttributeSet::GetArcaneResistanceAttribute(), Attributes->GetArcaneResistance(), NSLOCTEXT("GolemSkillsUI", "ArcanePolicy", "Clamp -25% to 60%; Light maps to Arcane."));
		AddStat(Defense, NSLOCTEXT("GolemSkillsUI", "LightningResistance", "Lightning resistance"), UGolemAttributeSet::GetLightningResistanceAttribute(), Attributes->GetLightningResistance(), NSLOCTEXT("GolemSkillsUI", "LightningPolicy", "Clamp -25% to 60%."));
		AddStat(Defense, NSLOCTEXT("GolemSkillsUI", "FrostResistance", "Frost resistance"), UGolemAttributeSet::GetFrostResistanceAttribute(), Attributes->GetFrostResistance(), NSLOCTEXT("GolemSkillsUI", "FrostPolicy", "Clamp -25% to 60%; Water maps to Frost."));
		AddStat(Defense, NSLOCTEXT("GolemSkillsUI", "FireResistance", "Fire resistance"), UGolemAttributeSet::GetFireResistanceAttribute(), Attributes->GetFireResistance(), NSLOCTEXT("GolemSkillsUI", "FirePolicy", "Clamp -25% to 60%."));
		AddStat(Specialization, NSLOCTEXT("GolemSkillsUI", "Mastery", "Mastery"), UGolemAttributeSet::GetMasteryBonusAttribute(), Attributes->GetMasteryBonus(), NSLOCTEXT("GolemSkillsUI", "MasteryPolicy", "Cap 30%; selected specialization signature hook once only."));
		AddStat(Specialization, NSLOCTEXT("GolemSkillsUI", "Versatility", "Versatility"), UGolemAttributeSet::GetVersatilityAttribute(), Attributes->GetVersatility(), NSLOCTEXT("GolemSkillsUI", "VersatilityPolicy", "Cap 20%; outgoing damage/heal, half strength incoming reduction. Does not scale shields."));
		AddStat(Specialization, NSLOCTEXT("GolemSkillsUI", "CooldownRecovery", "Cooldown Recovery"), UGolemAttributeSet::GetCooldownRecoveryAttribute(), Attributes->GetCooldownRecovery(), NSLOCTEXT("GolemSkillsUI", "CooldownPolicy", "Cap 30%; new cooldown base/(1+value). Running cooldowns remain unchanged."));
		if (const UGolemCombatComponent* Combat = Golem->GetCombatComponent())
		{
			if (const UGolemCombatProfileDefinition* Profile = Combat->GetProfile())
			{
				FGolemRPGStatRow Poise;
				Poise.Group = Defense;
				Poise.Name = NSLOCTEXT("GolemSkillsUI", "Poise", "Poise");
				Poise.Value = FText::Format(NSLOCTEXT("GolemSkillsUI", "PoiseValue", "{0}/{1}"),
					FText::AsNumber(Combat->GetSnapshot().Poise), FText::AsNumber(Combat->GetMaxPoise()));
				Poise.Contributions = FText::Format(NSLOCTEXT("GolemSkillsUI", "PoiseSources",
					"Combat profile base maximum: {0}; equipment contribution: {1}. Damage and regeneration use the M3 Poise pipeline; Saturn A4 reduces incoming Poise damage by 50%, without changing the maximum."),
					FText::AsNumber(Profile->GetTuning().MaxPoise), FText::AsNumber(View.StatBonuses.Poise));
				Snapshot.StatRows.Add(Poise);
			}
		}
	}
}
