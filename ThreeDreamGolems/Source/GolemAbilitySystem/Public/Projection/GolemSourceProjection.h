#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Types/GolemResult.h"
#include "Types/GolemStableIds.h"
#include "Skills/GolemSkillTypes.h"
#include "Skills/GolemEquipmentCombatTypes.h"
#include "GolemSourceProjection.generated.h"

namespace GolemProjection
{
	GOLEMABILITYSYSTEM_API extern const FName RPGSourceKey;
}

/** Multipliers tune an existing exact GAS action; identity values preserve its authored behavior. */
USTRUCT(BlueprintType)
struct GOLEMABILITYSYSTEM_API FGolemActionModifier
{
	GENERATED_BODY()

	UPROPERTY()
	FGameplayTag AbilityTag;
	UPROPERTY()
	float DamageScale = 1.0f;
	UPROPERTY()
	float CostScale = 1.0f;
	UPROPERTY()
	float DurationScale = 1.0f;

	FGolemResult Validate() const;
	bool operator==(const FGolemActionModifier& Other) const;
};

/** An Items coordinator supplies one aggregate RPG source. This model contains no Items state. */
USTRUCT(BlueprintType)
struct GOLEMABILITYSYSTEM_API FGolemGASProjectionPlan
{
	GENERATED_BODY()

	UPROPERTY()
	FName SourceKey;
	UPROPERTY()
	FPrimaryAssetId DefinitionId;
	UPROPERTY()
	float MaxHealthBonus = 0.0f;
	UPROPERTY()
	float MaxStaminaBonus = 0.0f;
	UPROPERTY()
	float MaxRuneEnergyBonus = 0.0f;
	UPROPERTY()
	float AttackPowerBonus = 0.0f;
	UPROPERTY()
	float DefenseBonus = 0.0f;
	UPROPERTY()
	float CritChanceBonus = 0.0f;
	UPROPERTY()
	float HasteBonus = 0.0f;
	UPROPERTY()
	float CooldownRecoveryBonus = 0.0f;
	UPROPERTY()
	float MasteryBonus = 0.0f;
	UPROPERTY()
	float VersatilityBonus = 0.0f;
	UPROPERTY()
	float MaxPoiseBonus = 0.0f;
	UPROPERTY()
	TArray<FGolemActionModifier> ActionModifiers;
	UPROPERTY()
	FPrimaryAssetId OptionalAbilitySetId;
	UPROPERTY()
	TArray<FPrimaryAssetId> OptionalAbilitySetIds;
	UPROPERTY()
	FGolemSkillBuild SkillBuild;
	UPROPERTY()
	FGolemEquipmentCombatBuild EquipmentCombatBuild;

	FGolemResult Validate() const;
	bool HasStatBonuses() const;
	bool operator==(const FGolemGASProjectionPlan& Other) const;
};

USTRUCT(BlueprintType)
struct GOLEMABILITYSYSTEM_API FGolemSourceProjection
{
	GENERATED_BODY()

	UPROPERTY()
	FGolemEntityId EntityId;
	UPROPERTY()
	int64 Revision = 0;
	UPROPERTY()
	FGolemGASProjectionPlan Plan;
};

/** Transient synchronous transaction capability. It is never a replication or save identity. */
USTRUCT()
struct GOLEMABILITYSYSTEM_API FGolemMutationFence
{
	GENERATED_BODY()

	UPROPERTY()
	FGuid Token;
	UPROPERTY()
	FGolemEntityId EntityId;
	UPROPERTY()
	FGuid RequestId;

	bool IsValid() const;
	bool operator==(const FGolemMutationFence& Other) const;
};

USTRUCT()
struct GOLEMABILITYSYSTEM_API FGolemProjectionCapability
{
	GENERATED_BODY()

	UPROPERTY()
	FGuid Token;
	UPROPERTY()
	FGolemEntityId EntityId;
	UPROPERTY()
	FName SourceKey;
	UPROPERTY()
	int64 ExpectedRevision = 0;

	bool IsValid() const;
	bool operator==(const FGolemProjectionCapability& Other) const;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FOnGolemSourceProjectionChanged, const FGolemSourceProjection&);
