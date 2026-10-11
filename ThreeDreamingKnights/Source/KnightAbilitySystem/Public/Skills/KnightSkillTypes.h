#pragma once

#include "CoreMinimal.h"
#include "Types/KnightResult.h"
#include "UObject/PrimaryAssetId.h"
#include "KnightSkillTypes.generated.h"

UENUM(BlueprintType)
enum class EKnightSkillRole : uint8
{
	Tank,
	Healer,
	Damage
};

UENUM(BlueprintType)
enum class EKnightSkillSchool : uint8
{
	Physical,
	Arcane,
	Lightning,
	Frost,
	Fire
};

UENUM(BlueprintType)
enum class EKnightSkillTargeting : uint8
{
	Single,
	Cone,
	Radius,
	GroundStrip,
	GroundArea,
	FollowingAlly,
	Self,
	Dash
};

/** Stable build description projected by Items; empty SpecId preserves the legacy M5 kit. */
USTRUCT(BlueprintType)
struct KNIGHTABILITYSYSTEM_API FKnightSkillBuild
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Knight|Skills")
	FName SpecId;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Knight|Skills")
	FName ClassChoice;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Knight|Skills")
	FName SpecChoice;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Knight|Skills")
	FName WeaponRune;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Knight|Skills")
	FName SkillRune;

	bool operator==(const FKnightSkillBuild& Other) const;
};

USTRUCT(BlueprintType)
struct KNIGHTABILITYSYSTEM_API FKnightSpecActionDefinition
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Knight|Skills")
	FName SpecId;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Knight|Skills")
	FName AbilityId;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Knight|Skills")
	FText DisplayName;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Knight|Skills")
	int32 Slot = 1;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Knight|Skills")
	EKnightSkillRole Role = EKnightSkillRole::Damage;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Knight|Skills")
	EKnightSkillSchool School = EKnightSkillSchool::Physical;
	UPROPERTY(BlueprintReadOnly, Category = "Knight|Skills")
	EKnightSkillTargeting Targeting = EKnightSkillTargeting::Single;
	UPROPERTY(BlueprintReadOnly, Category = "Knight|Skills")
	int32 MaxTargets = 1;
	UPROPERTY(BlueprintReadOnly, Category = "Knight|Skills")
	float ConeAngle = 0.0f;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Knight|Skills")
	float Coefficient = 0.0f;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Knight|Skills")
	float Cost = 0.0f;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Knight|Skills")
	float Cooldown = 2.0f;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Knight|Skills")
	float Range = 300.0f;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Knight|Skills")
	float Radius = 0.0f;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Knight|Skills")
	float Duration = 0.0f;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Knight|Skills")
	float PoiseDamage = 0.0f;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Knight|Skills")
	float Windup = 0.25f;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Knight|Skills")
	float Recovery = 0.35f;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Knight|Skills")
	bool bFriendly = false;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Knight|Skills")
	bool bProjectile = false;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Knight|Skills")
	bool bPeriodic = false;
};

/** Pure queries used by UI preview, authoritative validation and runtime execution. Units are centimeters. */
struct KNIGHTABILITYSYSTEM_API FKnightSkillCatalog
{
	static TArray<FName> AllSpecs();
	static TArray<FName> SpecsForHero(const FPrimaryAssetId& HeroId);
	static FName DefaultSpec(const FPrimaryAssetId& HeroId);
	static EKnightSkillRole Role(FName SpecId);
	static bool IsKnownSpec(FName SpecId);
	static bool IsRuneCompatible(FName SpecId, FName RuneId);
	static FKnightResult ValidateDescriptor(const FKnightSkillBuild& Build);
	static FKnightResult ValidateBuild(const FPrimaryAssetId& HeroId, const FKnightSkillBuild& Build);
	static FKnightSpecActionDefinition GetSkill(const FKnightSkillBuild& Build, int32 Slot);
};

/** Offensive values are snapshotted at commit; target defenses remain live at hit/tick. */
struct KNIGHTABILITYSYSTEM_API FKnightSkillDamagePacket
{
	float RawDamage = 0.0f;
	float PoiseDamage = 0.0f;
	EKnightSkillSchool School = EKnightSkillSchool::Physical;
	float FrostPenetration = 0.0f;
	bool bDirect = true;
	bool bEligibleDeferred = true;
	bool bCanDefend = true;
	bool bHeavy = false;
	bool bItemProc = false;
	bool bPrimaryTarget = true;
	bool bEligibleItemActivation = false;
	TWeakObjectPtr<AActor> PrimaryTarget;
	FGuid ActivationId;
	int32 HitIndex = 0;
};

struct KNIGHTABILITYSYSTEM_API FKnightSkillFormula
{
	static float Mitigate(float RawDamage, EKnightSkillSchool School, float Armor, float Resistance,
		float Reduction, float Versatility, float FrostPenetration = 0.0f);
};
