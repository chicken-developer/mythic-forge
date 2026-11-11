#pragma once

#include "CoreMinimal.h"
#include "Assets/GolemDefinition.h"
#include "Models/GolemItemModels.h"
#include "Models/GolemGearModels.h"
#include "GolemItemDefinition.generated.h"

/** Definition identity is authored independently of the asset package. This class contains no runtime item count. */
UCLASS(BlueprintType)
class GOLEMITEMS_API UGolemItemDefinition : public UGolemDefinition
{
	GENERATED_BODY()

public:
	UGolemItemDefinition();
	virtual void PostLoad() override;
	EGolemItemKind GetItemKind() const;
	int32 GetMaximumStack() const;
	FPrimaryAssetId GetAbilitySetId() const;
	const FGolemItemStatBonuses& GetStatBonuses() const;
	const FGolemItemMetadata& GetMetadata() const;
	FGolemResult TrySetMetadata(const FGolemItemMetadata& InMetadata);
	FGolemItemStatBonuses GetEffectiveStats(int32 UpgradeRank) const;
	int32 GetEffectiveItemLevel(int32 UpgradeRank) const;
	FName GetWeaponFamily() const;
	FGolemResult TrySetWeaponFamily(FName InFamily);
	virtual FGolemResult ValidateItem() const;

	/** Equipment/runes are individual instances; only their stable ability-set ID and positive stat bonuses are stored. */
	FGolemResult TrySetItemTuning(EGolemItemKind InKind, int32 InMaximumStack,
		const FPrimaryAssetId& InAbilitySetId = FPrimaryAssetId(),
		const FGolemItemStatBonuses& InStatBonuses = FGolemItemStatBonuses());

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

private:
	static FGolemResult ValidateTuning(EGolemItemKind InKind, int32 InMaximumStack,
		const FPrimaryAssetId& InAbilitySetId, const FGolemItemStatBonuses& InStatBonuses);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Items|Definition", meta = (AllowPrivateAccess = "true"))
	EGolemItemKind ItemKind = EGolemItemKind::Material;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Items|Definition",
		meta = (AllowPrivateAccess = "true", ClampMin = "1", ClampMax = "10000"))
	int32 MaximumStack = 100;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Items|Definition", meta = (AllowPrivateAccess = "true"))
	FPrimaryAssetId AbilitySetId;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Items|Definition", meta = (AllowPrivateAccess = "true"))
	FGolemItemStatBonuses StatBonuses;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Items|Catalog", meta = (AllowPrivateAccess = "true"))
	FGolemItemMetadata Metadata;
	/** Existing M5 equipment is a heavy hammer; future weapon families can opt out of Aftershock compatibility. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Items|Definition", meta = (AllowPrivateAccess = "true"))
	FName WeaponFamily = TEXT("HeavyHammer");
};
