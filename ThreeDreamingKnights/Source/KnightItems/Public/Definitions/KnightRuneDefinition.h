#pragma once

#include "Definitions/KnightItemDefinition.h"
#include "Models/KnightRPGModels.h"
#include "KnightRuneDefinition.generated.h"

/** A rune is an unstackable KnightItem and tunes the existing GAS rune action. */
UCLASS(BlueprintType)
class KNIGHTITEMS_API UKnightRuneDefinition : public UKnightItemDefinition
{
	GENERATED_BODY()

public:
	UKnightRuneDefinition();
	const FKnightRuneTuning& GetRuneTuning() const;
	FKnightResult TrySetRuneTuning(const FKnightRuneTuning& InTuning);
	virtual FKnightResult ValidateItem() const override;

private:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Knight|Items|Rune", meta = (AllowPrivateAccess = "true"))
	FKnightRuneTuning RuneTuning;
};
