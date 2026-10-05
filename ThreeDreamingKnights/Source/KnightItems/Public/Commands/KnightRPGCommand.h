#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Types/KnightResult.h"
#include "Types/KnightStableIds.h"
#include "KnightRPGCommand.generated.h"

UENUM(BlueprintType)
enum class EKnightRPGOperation : uint8
{
	Pickup,
	Equip,
	Unequip,
	Rune,
	PurchaseSkill,
	Respec
};

/** Sparse client intent. Costs, counts, definitions and GAS classes are resolved by authority. */
USTRUCT(BlueprintType)
struct KNIGHTITEMS_API FKnightRPGCommand
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	EKnightRPGOperation Operation = EKnightRPGOperation::Pickup;
	UPROPERTY(SaveGame)
	FGuid RequestId;
	UPROPERTY(SaveGame)
	FKnightEntityId ExpectedKnightId;
	UPROPERTY(SaveGame)
	FGuid InventoryId;
	UPROPERTY(SaveGame)
	int64 ExpectedInventoryRevision = 0;
	UPROPERTY(SaveGame)
	int64 ExpectedKnightRevision = 0;
	UPROPERTY(SaveGame)
	FGuid PickupId;
	UPROPERTY(SaveGame)
	FGuid ItemInstanceId;
	UPROPERTY(SaveGame)
	FGameplayTag SlotTag;
	UPROPERTY(SaveGame)
	FPrimaryAssetId SkillDefinitionId;

	FKnightResult ValidateShape() const;
	bool operator==(const FKnightRPGCommand& Other) const;
};

USTRUCT(BlueprintType)
struct KNIGHTITEMS_API FKnightRPGCommandResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Knight|RPG|Transactions")
	FKnightResult Result;
	UPROPERTY(BlueprintReadOnly, Category = "Knight|RPG|Transactions")
	EKnightRPGOperation Operation = EKnightRPGOperation::Pickup;
	UPROPERTY(BlueprintReadOnly, Category = "Knight|RPG|Transactions")
	FGuid RequestId;
	UPROPERTY(BlueprintReadOnly, Category = "Knight|RPG|Transactions")
	FKnightEntityId KnightId;
	UPROPERTY(BlueprintReadOnly, Category = "Knight|RPG|Transactions")
	FGuid InventoryId;
	UPROPERTY(BlueprintReadOnly, Category = "Knight|RPG|Transactions")
	int64 CommittedInventoryRevision = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Knight|RPG|Transactions")
	int64 CommittedKnightRevision = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Knight|RPG|Transactions")
	FGuid AffectedInstanceId;
	UPROPERTY(BlueprintReadOnly, Category = "Knight|RPG|Transactions")
	bool bWasReplay = false;
};
