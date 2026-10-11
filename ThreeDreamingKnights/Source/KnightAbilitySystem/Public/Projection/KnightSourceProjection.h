#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Types/KnightResult.h"
#include "Types/KnightStableIds.h"
#include "Skills/KnightSkillTypes.h"
#include "Skills/KnightEquipmentCombatTypes.h"
#include "KnightSourceProjection.generated.h"

namespace KnightProjection
{
	KNIGHTABILITYSYSTEM_API extern const FName RPGSourceKey;
}

/** Multipliers tune an existing exact GAS action; identity values preserve its authored behavior. */
USTRUCT(BlueprintType)
struct KNIGHTABILITYSYSTEM_API FKnightActionModifier
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

	FKnightResult Validate() const;
	bool operator==(const FKnightActionModifier& Other) const;
};

/** An Items coordinator supplies one aggregate RPG source. This model contains no Items state. */
USTRUCT(BlueprintType)
struct KNIGHTABILITYSYSTEM_API FKnightGASProjectionPlan
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
	TArray<FKnightActionModifier> ActionModifiers;
	UPROPERTY()
	FPrimaryAssetId OptionalAbilitySetId;
	UPROPERTY()
	TArray<FPrimaryAssetId> OptionalAbilitySetIds;
	UPROPERTY()
	FKnightSkillBuild SkillBuild;
	UPROPERTY()
	FKnightEquipmentCombatBuild EquipmentCombatBuild;

	FKnightResult Validate() const;
	bool HasStatBonuses() const;
	bool operator==(const FKnightGASProjectionPlan& Other) const;
};

USTRUCT(BlueprintType)
struct KNIGHTABILITYSYSTEM_API FKnightSourceProjection
{
	GENERATED_BODY()

	UPROPERTY()
	FKnightEntityId EntityId;
	UPROPERTY()
	int64 Revision = 0;
	UPROPERTY()
	FKnightGASProjectionPlan Plan;
};

/** Transient synchronous transaction capability. It is never a replication or save identity. */
USTRUCT()
struct KNIGHTABILITYSYSTEM_API FKnightMutationFence
{
	GENERATED_BODY()

	UPROPERTY()
	FGuid Token;
	UPROPERTY()
	FKnightEntityId EntityId;
	UPROPERTY()
	FGuid RequestId;

	bool IsValid() const;
	bool operator==(const FKnightMutationFence& Other) const;
};

USTRUCT()
struct KNIGHTABILITYSYSTEM_API FKnightProjectionCapability
{
	GENERATED_BODY()

	UPROPERTY()
	FGuid Token;
	UPROPERTY()
	FKnightEntityId EntityId;
	UPROPERTY()
	FName SourceKey;
	UPROPERTY()
	int64 ExpectedRevision = 0;

	bool IsValid() const;
	bool operator==(const FKnightProjectionCapability& Other) const;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FOnKnightSourceProjectionChanged, const FKnightSourceProjection&);
