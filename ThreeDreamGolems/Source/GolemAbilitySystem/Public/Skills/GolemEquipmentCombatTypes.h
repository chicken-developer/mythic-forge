#pragma once

#include "CoreMinimal.h"
#include "Types/GolemResult.h"
#include "GolemEquipmentCombatTypes.generated.h"

/** Items supplies confirmed equipment identity; this descriptor contains no inventory ownership. */
USTRUCT(BlueprintType)
struct GOLEMABILITYSYSTEM_API FGolemEquipmentCombatBuild
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Golem|Equipment")
	bool bEnabled = false;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Golem|Equipment")
	bool bHasHammer = false;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Golem|Equipment")
	FGuid RelicInstanceId;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Golem|Equipment")
	FName RelicEffectId;

	FGolemResult Validate() const;
	bool operator==(const FGolemEquipmentCombatBuild& Other) const;
};
