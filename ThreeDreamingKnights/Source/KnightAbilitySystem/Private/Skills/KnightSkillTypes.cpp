#include "Skills/KnightSkillTypes.h"

namespace
{
	FText SkillDisplayName(FName SpecId, int32 Slot)
	{
		if (SpecId == TEXT("Uranus"))
		{
			const FText Names[]
			{
				NSLOCTEXT("KnightSkillNames", "UranusA1", "Quang Kích"),
				NSLOCTEXT("KnightSkillNames", "UranusA2", "Quang Phạt"),
				NSLOCTEXT("KnightSkillNames", "UranusA3", "Quang Thuẫn"),
				NSLOCTEXT("KnightSkillNames", "UranusA4", "Thiên Quang Trận")
			};
			return Slot >= 1 && Slot <= 4 ? Names[Slot - 1] : FText::GetEmpty();
		}
		else if (SpecId == TEXT("Venus"))
		{
			const FText Names[]
			{
				NSLOCTEXT("KnightSkillNames", "VenusA1", "Quang Châm"),
				NSLOCTEXT("KnightSkillNames", "VenusA2", "Hồi Quang"),
				NSLOCTEXT("KnightSkillNames", "VenusA3", "Liên Quang"),
				NSLOCTEXT("KnightSkillNames", "VenusA4", "Khải Quang")
			};
			return Slot >= 1 && Slot <= 4 ? Names[Slot - 1] : FText::GetEmpty();
		}
		else if (SpecId == TEXT("Jupiter"))
		{
			const FText Names[]
			{
				NSLOCTEXT("KnightSkillNames", "JupiterA1", "Điện Kích"),
				NSLOCTEXT("KnightSkillNames", "JupiterA2", "Lôi Thương"),
				NSLOCTEXT("KnightSkillNames", "JupiterA3", "Lướt Sét"),
				NSLOCTEXT("KnightSkillNames", "JupiterA4", "Lôi Vân")
			};
			return Slot >= 1 && Slot <= 4 ? Names[Slot - 1] : FText::GetEmpty();
		}
		else if (SpecId == TEXT("Neptune"))
		{
			const FText Names[]
			{
				NSLOCTEXT("KnightSkillNames", "NeptuneA1", "Thủy Kích"),
				NSLOCTEXT("KnightSkillNames", "NeptuneA2", "Sóng Áp"),
				NSLOCTEXT("KnightSkillNames", "NeptuneA3", "Thủy Giáp"),
				NSLOCTEXT("KnightSkillNames", "NeptuneA4", "Tĩnh Thủy")
			};
			return Slot >= 1 && Slot <= 4 ? Names[Slot - 1] : FText::GetEmpty();
		}
		else if (SpecId == TEXT("Mercury"))
		{
			const FText Names[]
			{
				NSLOCTEXT("KnightSkillNames", "MercuryA1", "Thủy Tiễn"),
				NSLOCTEXT("KnightSkillNames", "MercuryA2", "Mạch Nước"),
				NSLOCTEXT("KnightSkillNames", "MercuryA3", "Thủy Liệu"),
				NSLOCTEXT("KnightSkillNames", "MercuryA4", "Đại Triều")
			};
			return Slot >= 1 && Slot <= 4 ? Names[Slot - 1] : FText::GetEmpty();
		}
		else if (SpecId == TEXT("Pluto"))
		{
			const FText Names[]
			{
				NSLOCTEXT("KnightSkillNames", "PlutoA1", "Thủy Châm"),
				NSLOCTEXT("KnightSkillNames", "PlutoA2", "Băng Thương"),
				NSLOCTEXT("KnightSkillNames", "PlutoA3", "Băng Hoàn"),
				NSLOCTEXT("KnightSkillNames", "PlutoA4", "Tuyệt Hàn")
			};
			return Slot >= 1 && Slot <= 4 ? Names[Slot - 1] : FText::GetEmpty();
		}
		else if (SpecId == TEXT("Saturn"))
		{
			const FText Names[]
			{
				NSLOCTEXT("KnightSkillNames", "SaturnA1", "Thạch Kích"),
				NSLOCTEXT("KnightSkillNames", "SaturnA2", "Địa Chấn"),
				NSLOCTEXT("KnightSkillNames", "SaturnA3", "Thạch Giáp"),
				NSLOCTEXT("KnightSkillNames", "SaturnA4", "Tâm Sơn")
			};
			return Slot >= 1 && Slot <= 4 ? Names[Slot - 1] : FText::GetEmpty();
		}
		else if (SpecId == TEXT("Terra"))
		{
			const FText Names[]
			{
				NSLOCTEXT("KnightSkillNames", "TerraA1", "Hỏa Châm"),
				NSLOCTEXT("KnightSkillNames", "TerraA2", "Trụ Sinh Lực"),
				NSLOCTEXT("KnightSkillNames", "TerraA3", "Tái Tạo"),
				NSLOCTEXT("KnightSkillNames", "TerraA4", "Lửa Bền Sinh")
			};
			return Slot >= 1 && Slot <= 4 ? Names[Slot - 1] : FText::GetEmpty();
		}
		else if (SpecId == TEXT("Mars"))
		{
			const FText Names[]
			{
				NSLOCTEXT("KnightSkillNames", "MarsA1", "Thạch Chùy"),
				NSLOCTEXT("KnightSkillNames", "MarsA2", "Hỏa Chùy"),
				NSLOCTEXT("KnightSkillNames", "MarsA3", "Nứt Đất"),
				NSLOCTEXT("KnightSkillNames", "MarsA4", "Lò Rèn Sống")
			};
			return Slot >= 1 && Slot <= 4 ? Names[Slot - 1] : FText::GetEmpty();
		}
		return FText::GetEmpty();
	}
}
bool FKnightSkillBuild::operator==(const FKnightSkillBuild& Other) const
{
	return SpecId == Other.SpecId && ClassChoice == Other.ClassChoice && SpecChoice == Other.SpecChoice
		&& WeaponRune == Other.WeaponRune && SkillRune == Other.SkillRune;
}

TArray<FName> FKnightSkillCatalog::AllSpecs()
{
	return { TEXT("Uranus"), TEXT("Venus"), TEXT("Jupiter"), TEXT("Neptune"), TEXT("Mercury"), TEXT("Pluto"),
		TEXT("Saturn"), TEXT("Terra"), TEXT("Mars") };
}

TArray<FName> FKnightSkillCatalog::SpecsForHero(const FPrimaryAssetId& HeroId)
{
	const FName Name = HeroId.PrimaryAssetName;
	if (Name == TEXT("Training_Red") || Name == TEXT("Storm") || Name == TEXT("Sky"))
	{
		return { TEXT("Uranus"), TEXT("Venus"), TEXT("Jupiter") };
	}
	if (Name == TEXT("Training_Blue") || Name == TEXT("Tide") || Name == TEXT("Sea"))
	{
		return { TEXT("Neptune"), TEXT("Mercury"), TEXT("Pluto") };
	}
	if (Name == TEXT("Training_Green") || Name == TEXT("Forge") || Name == TEXT("Earth"))
	{
		return { TEXT("Saturn"), TEXT("Terra"), TEXT("Mars") };
	}
	return {};
}

FName FKnightSkillCatalog::DefaultSpec(const FPrimaryAssetId& HeroId)
{
	const TArray<FName> Specs = SpecsForHero(HeroId);
	return Specs.IsEmpty() ? NAME_None : Specs[0];
}

EKnightSkillRole FKnightSkillCatalog::Role(FName SpecId)
{
	if (SpecId == TEXT("Uranus") || SpecId == TEXT("Neptune") || SpecId == TEXT("Saturn"))
	{
		return EKnightSkillRole::Tank;
	}
	if (SpecId == TEXT("Venus") || SpecId == TEXT("Mercury") || SpecId == TEXT("Terra"))
	{
		return EKnightSkillRole::Healer;
	}
	return EKnightSkillRole::Damage;
}

bool FKnightSkillCatalog::IsKnownSpec(FName SpecId)
{
	return AllSpecs().Contains(SpecId);
}

bool FKnightSkillCatalog::IsRuneCompatible(FName SpecId, FName RuneId)
{
	if (RuneId.IsNone() || RuneId == TEXT("Weapon.Aftershock"))
	{
		return true;
	}
	if (RuneId == TEXT("Skill.Convergence"))
	{
		return SpecId == TEXT("Uranus") || SpecId == TEXT("Neptune") || SpecId == TEXT("Saturn")
			|| SpecId == TEXT("Mars");
	}
	if (RuneId == TEXT("Skill.Propagation"))
	{
		return SpecId == TEXT("Venus") || SpecId == TEXT("Mercury") || SpecId == TEXT("Terra");
	}
	return RuneId == TEXT("Skill.Mobile") && SpecId == TEXT("Terra");
}

FKnightResult FKnightSkillCatalog::ValidateDescriptor(const FKnightSkillBuild& Build)
{
	const bool bEmpty = Build.SpecId.IsNone() && Build.ClassChoice.IsNone() && Build.SpecChoice.IsNone()
		&& Build.WeaponRune.IsNone() && Build.SkillRune.IsNone();
	if (bEmpty)
	{
		return FKnightResult::Success();
	}
	if (!IsKnownSpec(Build.SpecId)
		|| (!Build.ClassChoice.IsNone() && Build.ClassChoice != TEXT("Endurance") && Build.ClassChoice != TEXT("Reservoir"))
		|| (!Build.SpecChoice.IsNone() && Build.SpecChoice != TEXT("A") && Build.SpecChoice != TEXT("B"))
		|| (!Build.WeaponRune.IsNone() && Build.WeaponRune != TEXT("Weapon.Aftershock"))
		|| Build.SkillRune == TEXT("Weapon.Aftershock") || !IsRuneCompatible(Build.SpecId, Build.SkillRune))
	{
		return FKnightResult::Failure(EKnightError::InvalidData, TEXT("SkillBuild"),
			NSLOCTEXT("KnightSkills", "BuildInvalid", "The specialization, choice or rune is incompatible with this Knight."));
	}
	return FKnightResult::Success();
}

FKnightResult FKnightSkillCatalog::ValidateBuild(const FPrimaryAssetId& HeroId, const FKnightSkillBuild& Build)
{
	const FKnightResult Descriptor = ValidateDescriptor(Build);
	if (!Descriptor.IsSuccess() || Build.SpecId.IsNone())
	{
		return Descriptor;
	}
	if (!SpecsForHero(HeroId).Contains(Build.SpecId))
	{
		return FKnightResult::Failure(EKnightError::InvalidData, TEXT("SkillBuildIdentity"),
			NSLOCTEXT("KnightSkills", "BuildIdentity", "This specialization does not belong to this Knight."));
	}
	return FKnightResult::Success();
}

FKnightSpecActionDefinition FKnightSkillCatalog::GetSkill(const FKnightSkillBuild& Build, int32 Slot)
{
	FKnightSpecActionDefinition D;
	D.SpecId = Build.SpecId;
	D.DisplayName = SkillDisplayName(Build.SpecId, Slot);
	D.Slot = Slot;
	D.Role = Role(Build.SpecId);
	D.AbilityId = FName(*FString::Printf(TEXT("%s.A%d"), *Build.SpecId.ToString(), Slot));
	D.Cost = Slot == 2 ? 25.0f : Slot == 3 ? 20.0f : 0.0f;
	D.Cooldown = Slot == 2 ? 6.0f : Slot == 3 ? 12.0f : 2.0f;
	D.Windup = Slot == 2 ? 0.45f : Slot == 4 ? 0.30f : 0.25f;
	D.Recovery = Slot == 2 ? 0.55f : Slot == 4 ? 0.30f : 0.35f;
	const FName S = Build.SpecId;
	if (Slot == 4)
	{
		D.Duration = S == TEXT("Neptune") ? 6.0f : S == TEXT("Uranus") || S == TEXT("Pluto") || S == TEXT("Saturn") ? 8.0f : 10.0f;
		D.Cooldown = S == TEXT("Neptune") ? 30.0f : S == TEXT("Uranus") || S == TEXT("Saturn") ? 45.0f : 60.0f;
		D.Range = 0.0f;
		D.bFriendly = true;
		D.Targeting = EKnightSkillTargeting::Self;
	}
	else
	{
		D.School = S == TEXT("Uranus") || S == TEXT("Venus") ? EKnightSkillSchool::Arcane : S == TEXT("Jupiter") ? EKnightSkillSchool::Lightning :
			S == TEXT("Neptune") || S == TEXT("Mercury") || S == TEXT("Pluto") ? EKnightSkillSchool::Frost : EKnightSkillSchool::Physical;
		D.bProjectile = Slot == 1 && (D.Role != EKnightSkillRole::Tank && S != TEXT("Mars"));
		D.bProjectile |= Slot == 2 && (S == TEXT("Jupiter") || S == TEXT("Pluto"));
		D.Range = D.bProjectile ? 1500.0f : 300.0f;
		D.Coefficient = Slot == 1 ? 0.9f : 1.7f;
		D.PoiseDamage = Slot == 1 ? 8.0f : 30.0f;
		if (Slot == 1 && (S == TEXT("Neptune") || S == TEXT("Pluto") || S == TEXT("Saturn") || S == TEXT("Mars")))
		{
			D.Coefficient = 1.0f;
			D.PoiseDamage = S == TEXT("Mars") || S == TEXT("Saturn") ? 15.0f : S == TEXT("Pluto") ? 8.0f : 12.0f;
		}
		if (S == TEXT("Uranus") && Slot == 1)
		{
			D.School = EKnightSkillSchool::Physical;
			D.PoiseDamage = 12.0f;
		}
		if (Slot == 2 && (S == TEXT("Jupiter") || S == TEXT("Pluto") || S == TEXT("Mars")))
		{
			D.Coefficient = 2.2f;
			D.PoiseDamage = 25.0f;
		}
		if (S == TEXT("Uranus") && Slot == 2)
		{
			D.Coefficient = 1.8f;
		}
		if (S == TEXT("Saturn") && Slot == 2)
		{
			D.PoiseDamage = 35.0f;
		}
		D.bFriendly = D.Role == EKnightSkillRole::Healer && Slot > 1;
		D.bFriendly |= Slot == 3 && D.Role == EKnightSkillRole::Tank;
		if (D.bFriendly)
		{
			D.Range = 1200.0f;
			D.PoiseDamage = 0.0f;
		}
		if (S == TEXT("Venus"))
		{
			D.Coefficient = Slot == 2 ? 2.2f : Slot == 3 ? 1.2f : 0.9f;
			D.Radius = Slot == 3 ? 500.0f : 0.0f;
		}
		if (S == TEXT("Mercury"))
		{
			D.Coefficient = Slot == 2 ? 0.35f : Slot == 3 ? 1.8f : 0.9f;
			D.Duration = Slot == 2 ? 6.0f : 0.0f;
			D.bPeriodic = Slot == 2;
		}
		if (S == TEXT("Terra"))
		{
			D.School = EKnightSkillSchool::Fire;
			D.Coefficient = Slot == 2 ? 0.35f : Slot == 3 ? 2.0f : 0.9f;
			D.Duration = Slot == 2 ? 6.0f : 0.0f;
			D.Radius = Slot == 2 ? 500.0f : 0.0f;
			D.bPeriodic = Slot == 2;
		}
		if (Slot == 3 && D.Role == EKnightSkillRole::Tank)
		{
			D.Coefficient = S == TEXT("Saturn") ? 2.2f : 2.0f;
			D.Duration = 6.0f;
		}
		if (S == TEXT("Jupiter") && Slot == 3)
		{
			D.Coefficient = 0.0f;
			D.Range = 400.0f;
		}
		if (S == TEXT("Pluto") && Slot == 3)
		{
			D.Coefficient = 1.0f;
			D.Radius = 400.0f;
			D.PoiseDamage = 15.0f;
		}
		if (S == TEXT("Mars") && Slot == 2)
		{
			D.School = EKnightSkillSchool::Fire;
		}
		if (S == TEXT("Mars") && Slot == 3)
		{
			D.Coefficient = 1.3f;
			D.Range = 800.0f;
			D.Radius = 100.0f;
			D.PoiseDamage = 25.0f;
		}
		if (Slot == 2 && (D.Role == EKnightSkillRole::Tank || S == TEXT("Mars")))
		{
			D.Targeting = EKnightSkillTargeting::Cone;
			D.ConeAngle = S == TEXT("Uranus") || S == TEXT("Mars") ? 70.0f : 90.0f;
			D.MaxTargets = 3;
		}
		if (Slot == 3 && (S == TEXT("Venus") || S == TEXT("Pluto")))
		{
			D.Targeting = EKnightSkillTargeting::Radius;
			D.MaxTargets = 3;
		}
		if (S == TEXT("Terra") && Slot == 2)
		{
			D.Targeting = EKnightSkillTargeting::GroundArea;
			D.MaxTargets = 3;
		}
		if (S == TEXT("Mars") && Slot == 3)
		{
			D.Targeting = EKnightSkillTargeting::GroundStrip;
			D.MaxTargets = 3;
		}
		if (S == TEXT("Jupiter") && Slot == 3)
		{
			D.Targeting = EKnightSkillTargeting::Dash;
			D.MaxTargets = 0;
		}
	}
	if (Build.SpecChoice == TEXT("A"))
	{
		const bool bBoost = D.Role == EKnightSkillRole::Tank ? Slot == 3 : Slot == 2;
		if (bBoost)
		{
			D.Coefficient *= 1.15f;
		}
	}
	else if (Build.SpecChoice == TEXT("B"))
	{
		if (Slot == 4 && D.Role == EKnightSkillRole::Tank)
		{
			D.Duration += 2.0f;
		}
		if (Slot == 3 && (S == TEXT("Venus") || S == TEXT("Pluto")))
		{
			D.Radius += S == TEXT("Venus") ? 200.0f : 100.0f;
		}
		if (S == TEXT("Terra") && Slot == 2)
		{
			D.Radius = 700.0f;
		}
		if (S == TEXT("Mars") && Slot == 3)
		{
			D.Radius = 150.0f;
		}
	}
	if (Build.SkillRune == TEXT("Skill.Convergence") && Slot == 2)
	{
		D.Coefficient *= 1.20f;
		D.Radius = 0.0f;
		D.Targeting = EKnightSkillTargeting::Single;
		D.MaxTargets = 1;
		D.ConeAngle = 0.0f;
	}
	if (Build.SkillRune == TEXT("Skill.Propagation") && ((S == TEXT("Venus") && Slot == 2) ||
		((S == TEXT("Mercury") || S == TEXT("Terra")) && Slot == 3)))
	{
		D.Coefficient *= 0.85f;
	}
	if (Build.SkillRune == TEXT("Skill.Mobile") && S == TEXT("Terra") && Slot == 2)
	{
		D.Radius = 300.0f;
		D.Duration = 4.0f;
		D.Targeting = EKnightSkillTargeting::FollowingAlly;
	}
	return D;
}

float FKnightSkillFormula::Mitigate(float RawDamage, EKnightSkillSchool School, float Armor, float Resistance,
	float Reduction, float Versatility, float FrostPenetration)
{
	float Mitigation = 1.0f;
	if (School == EKnightSkillSchool::Physical)
	{
		const float A = FMath::Max(0.0f, Armor);
		Mitigation = 400.0f / (A + 400.0f);
	}
	else
	{
		float R = FMath::Clamp(Resistance, -0.25f, 0.60f);
		if (School == EKnightSkillSchool::Frost && R > 0.0f)
		{
			R = FMath::Max(0.0f, R - FMath::Max(0.0f, FrostPenetration));
		}
		Mitigation = 1.0f - R;
	}
	return FMath::Max(0.0f, RawDamage) * Mitigation * (1.0f - FMath::Clamp(Reduction, 0.0f, 0.5f))
		* (1.0f - FMath::Clamp(Versatility, 0.0f, 0.2f) * 0.5f);
}
