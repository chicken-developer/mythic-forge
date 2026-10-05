#pragma once

#include "CoreMinimal.h"
#include "Assets/KnightDefinition.h"
#include "Models/KnightItemModels.h"
#include "KnightItemDefinition.generated.h"

/** Definition identity is authored independently of the asset package. This class contains no runtime item count. */
UCLASS(BlueprintType)
class KNIGHTITEMS_API UKnightItemDefinition : public UKnightDefinition
{
	GENERATED_BODY()

public:
	UKnightItemDefinition();
	EKnightItemKind GetItemKind() const;
	int32 GetMaximumStack() const;
	FPrimaryAssetId GetAbilitySetId() const;
	const FKnightItemStatBonuses& GetStatBonuses() const;
	virtual FKnightResult ValidateItem() const;

	/** Equipment/runes are individual instances; only their stable ability-set ID and positive stat bonuses are stored. */
	FKnightResult TrySetItemTuning(EKnightItemKind InKind, int32 InMaximumStack,
		const FPrimaryAssetId& InAbilitySetId = FPrimaryAssetId(),
		const FKnightItemStatBonuses& InStatBonuses = FKnightItemStatBonuses());

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

private:
	static FKnightResult ValidateTuning(EKnightItemKind InKind, int32 InMaximumStack,
		const FPrimaryAssetId& InAbilitySetId, const FKnightItemStatBonuses& InStatBonuses);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Knight|Items|Definition", meta = (AllowPrivateAccess = "true"))
	EKnightItemKind ItemKind = EKnightItemKind::Material;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Knight|Items|Definition",
		meta = (AllowPrivateAccess = "true", ClampMin = "1", ClampMax = "10000"))
	int32 MaximumStack = 100;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Knight|Items|Definition", meta = (AllowPrivateAccess = "true"))
	FPrimaryAssetId AbilitySetId;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Knight|Items|Definition", meta = (AllowPrivateAccess = "true"))
	FKnightItemStatBonuses StatBonuses;
};
