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
 * One attack of a Xenon. Every attack shows where it will land for TelegraphSeconds first, never
 * less than 0.6 s, so players can read it and step out (DEV_SPEC section 6).
 */
USTRUCT(BlueprintType)
struct GOLEMCOMBAT_API FGolemXenonMove
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Xenon|Move")
	FName MoveId;

	/** The Xenon starts this attack when a Golem in front of it is at most this far away. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Xenon|Move", meta = (ClampMin = "50.0", Units = "cm"))
	float StartRange = 290.0f;

	/** Seconds the landing zone is shown before the hit. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Xenon|Move", meta = (ClampMin = "0.6", Units = "s"))
	float TelegraphSeconds = 1.0f;

	/** Seconds the Xenon stands still after the hit. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Xenon|Move", meta = (ClampMin = "0.0", Units = "s"))
	float RecoverSeconds = 1.4f;

	/** Reach of the hit, measured from the centre of the Xenon. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Xenon|Move", meta = (ClampMin = "50.0", Units = "cm"))
	float Range = 340.0f;

	/** Half of the arc the hit covers in front of the Xenon: wide for a sweep, narrow for a shove. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Xenon|Move", meta = (ClampMin = "5.0", ClampMax = "180.0", Units = "deg"))
	float HalfAngleDegrees = 80.0f;

	/** Multiplies the Xenon's damage per hit. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Xenon|Move", meta = (ClampMin = "0.0"))
	float DamageScale = 1.0f;

	/** Speed a Golem is thrown back with; 0 for no push. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Xenon|Move", meta = (ClampMin = "0.0", Units = "cm/s"))
	float Knockback = 0.0f;

	FGolemResult Validate() const;
};

/** How a Xenon moves and what it attacks with. */
USTRUCT(BlueprintType)
struct GOLEMCOMBAT_API FGolemXenonBehavior
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Xenon|Behavior", meta = (ClampMin = "0.0", Units = "cm/s"))
	float MoveSpeed = 200.0f;

	/** How fast it turns to face its target: slow enough that a Golem can get behind it. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Xenon|Behavior", meta = (ClampMin = "1.0", Units = "deg"))
	float TurnRateDegrees = 110.0f;

	/** Golems farther away than this are ignored. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Xenon|Behavior", meta = (ClampMin = "100.0", Units = "cm"))
	float AggroRange = 3000.0f;

	/** Tried in order; the first whose start range is met is used. A Xenon with no moves only walks. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Xenon|Behavior")
	TArray<FGolemXenonMove> Moves;

	FGolemResult Validate() const;
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
	const FGolemXenonBehavior& GetBehavior() const { return Behavior; }
	FGolemResult TrySetBehavior(const FGolemXenonBehavior& InBehavior);

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

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Xenon", meta = (AllowPrivateAccess = "true"))
	FGolemXenonBehavior Behavior;

	/** Share of voids that need two elements; 0.3 at difficulty 1. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Xenon", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", ClampMax = "1.0"))
	float MixedVoidChance = 0.3f;

	/** The first void always needs a single element, so new players get one easy step. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Xenon", meta = (AllowPrivateAccess = "true"))
	bool bFirstVoidBasic = false;
};
