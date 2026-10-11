#pragma once

#include "CoreMinimal.h"
#include "Types/KnightResult.h"
#include "KnightEquipmentCombatTypes.generated.h"

/** Items supplies confirmed equipment identity; this descriptor contains no inventory ownership. */
USTRUCT(BlueprintType)
struct KNIGHTABILITYSYSTEM_API FKnightEquipmentCombatBuild
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Knight|Equipment")
	bool bEnabled = false;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Knight|Equipment")
	bool bHasHammer = false;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Knight|Equipment")
	FGuid RelicInstanceId;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Knight|Equipment")
	FName RelicEffectId;

	FKnightResult Validate() const;
	bool operator==(const FKnightEquipmentCombatBuild& Other) const;
};
