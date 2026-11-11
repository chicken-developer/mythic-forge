#pragma once

#include "CoreMinimal.h"
#include "Types/GolemResult.h"
#include "UObject/PrimaryAssetId.h"
#include "GolemSkillTypes.generated.h"

UENUM(BlueprintType)
enum class EGolemSkillRole : uint8
{
	Tank,
	Healer,
	Damage
};

UENUM(BlueprintType)
enum class EGolemSkillSchool : uint8
{
	Physical,
	Arcane,
	Lightning,
	Frost,
	Fire
};

UENUM(BlueprintType)
enum class EGolemSkillTargeting : uint8
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
struct GOLEMABILITYSYSTEM_API FGolemSkillBuild
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Golem|Skills")
	FName SpecId;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Golem|Skills")
	FName ClassChoice;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Golem|Skills")
	FName SpecChoice;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Golem|Skills")
	FName WeaponRune;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Golem|Skills")
	FName SkillRune;

	bool operator==(const FGolemSkillBuild& Other) const;
};

USTRUCT(BlueprintType)
struct GOLEMABILITYSYSTEM_API FGolemSpecActionDefinition
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Golem|Skills")
	FName SpecId;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Golem|Skills")
	FName AbilityId;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Golem|Skills")
	FText DisplayName;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Golem|Skills")
	int32 Slot = 1;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Golem|Skills")
	EGolemSkillRole Role = EGolemSkillRole::Damage;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Golem|Skills")
	EGolemSkillSchool School = EGolemSkillSchool::Physical;
	UPROPERTY(BlueprintReadOnly, Category = "Golem|Skills")
	EGolemSkillTargeting Targeting = EGolemSkillTargeting::Single;
	UPROPERTY(BlueprintReadOnly, Category = "Golem|Skills")
	int32 MaxTargets = 1;
	UPROPERTY(BlueprintReadOnly, Category = "Golem|Skills")
	float ConeAngle = 0.0f;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Golem|Skills")
	float Coefficient = 0.0f;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Golem|Skills")
	float Cost = 0.0f;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Golem|Skills")
	float Cooldown = 2.0f;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Golem|Skills")
	float Range = 300.0f;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Golem|Skills")
	float Radius = 0.0f;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Golem|Skills")
	float Duration = 0.0f;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Golem|Skills")
	float PoiseDamage = 0.0f;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Golem|Skills")
	float Windup = 0.25f;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Golem|Skills")
	float Recovery = 0.35f;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Golem|Skills")
	bool bFriendly = false;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Golem|Skills")
	bool bProjectile = false;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Golem|Skills")
	bool bPeriodic = false;
};

/** Pure queries used by UI preview, authoritative validation and runtime execution. Units are centimeters. */
struct GOLEMABILITYSYSTEM_API FGolemSkillCatalog
{
	static TArray<FName> AllSpecs();
	static TArray<FName> SpecsForHero(const FPrimaryAssetId& HeroId);
	static FName DefaultSpec(const FPrimaryAssetId& HeroId);
	static EGolemSkillRole Role(FName SpecId);
	static bool IsKnownSpec(FName SpecId);
	static bool IsRuneCompatible(FName SpecId, FName RuneId);
	static FGolemResult ValidateDescriptor(const FGolemSkillBuild& Build);
	static FGolemResult ValidateBuild(const FPrimaryAssetId& HeroId, const FGolemSkillBuild& Build);
	static FGolemSpecActionDefinition GetSkill(const FGolemSkillBuild& Build, int32 Slot);
};

/** Offensive values are snapshotted at commit; target defenses remain live at hit/tick. */
struct GOLEMABILITYSYSTEM_API FGolemSkillDamagePacket
{
	float RawDamage = 0.0f;
	float PoiseDamage = 0.0f;
	EGolemSkillSchool School = EGolemSkillSchool::Physical;
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

struct GOLEMABILITYSYSTEM_API FGolemSkillFormula
{
	static float Mitigate(float RawDamage, EGolemSkillSchool School, float Armor, float Resistance,
		float Reduction, float Versatility, float FrostPenetration = 0.0f);
};
