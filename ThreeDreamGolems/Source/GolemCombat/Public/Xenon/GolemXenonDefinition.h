#pragma once

#include "CoreMinimal.h"
#include "Assets/GolemDefinition.h"
#include "Voids/GolemVoidHostComponent.h"
#include "GolemXenonDefinition.generated.h"

/** Where one void can appear on the body, in centimetres relative to the unscaled actor. */
USTRUCT(BlueprintType)
struct GOLEMCOMBAT_API FGolemXenonVoidSlot
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Xenon")
	FVector Offset = FVector(45.0, 0.0, 30.0);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Xenon", meta = (ClampMin = "5.0", Units = "cm"))
	float Radius = 35.0f;
};

/**
 * One kind of dark Xenon (DEV_SPEC section 6, FXenonArchetypeRow). Every number here is a starting
 * value that designers tune in the asset; the difficulty multipliers are applied by the encounter.
 */
UCLASS(BlueprintType)
class GOLEMCOMBAT_API UGolemXenonDefinition : public UGolemDefinition
{
	GENERATED_BODY()

public:
	UGolemXenonDefinition();

	/** At most this many body slots, plus one for a white core. */
	static constexpr int32 MaxVoidSlots = 6;

	float GetMaxHealth() const { return MaxHealth; }
	float GetContactDamage() const { return ContactDamage; }
	float GetBodyScale() const { return BodyScale; }
	float GetMixedVoidChance() const { return MixedVoidChance; }
	bool IsFirstVoidBasic() const { return bFirstVoidBasic; }
	const FGolemVoidHostTuning& GetVoidTuning() const { return Voids; }
	const TArray<FGolemXenonVoidSlot>& GetVoidSlots() const { return VoidSlots; }
	const FGolemXenonVoidSlot& GetCoreSlot() const { return CoreSlot; }

	FGolemResult ValidateXenon() const;

	/**
	 * One element mask per void to fill, chosen from Seed alone so host and clients agree.
	 * MixedChanceOverride lets the encounter apply the difficulty table; negative keeps the asset value.
	 */
	TArray<uint8> BuildColorPlan(int32 Seed, float MixedChanceOverride = -1.0f) const;

	/** Authoring API; rejected values leave the definition unchanged. */
	FGolemResult TrySetXenon(float InMaxHealth, float InContactDamage, float InBodyScale,
		const FGolemVoidHostTuning& InVoids, const TArray<FGolemXenonVoidSlot>& InSlots,
		float InMixedVoidChance, bool bInFirstVoidBasic);

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

private:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Xenon", meta = (AllowPrivateAccess = "true", ClampMin = "1.0"))
	float MaxHealth = 300.0f;

	/** Damage of one hit at difficulty 1. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Xenon", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float ContactDamage = 20.0f;

	/** Xenons stand two to three times as tall as a Golem. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Xenon", meta = (AllowPrivateAccess = "true", ClampMin = "0.5", ClampMax = "6.0"))
	float BodyScale = 2.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Xenon", meta = (AllowPrivateAccess = "true"))
	FGolemVoidHostTuning Voids;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Xenon", meta = (AllowPrivateAccess = "true"))
	TArray<FGolemXenonVoidSlot> VoidSlots;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Xenon", meta = (AllowPrivateAccess = "true"))
	FGolemXenonVoidSlot CoreSlot;

	/** Share of voids that need two elements; 0.3 at difficulty 1. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Xenon", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", ClampMax = "1.0"))
	float MixedVoidChance = 0.3f;

	/** The first void always needs a single element, so new players get one easy step. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Xenon", meta = (AllowPrivateAccess = "true"))
	bool bFirstVoidBasic = false;
};
