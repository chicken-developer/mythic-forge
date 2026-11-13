#pragma once

#include "CoreMinimal.h"
#include "Assets/GolemDefinition.h"
#include "GameplayTagContainer.h"
#include "GolemCombatProfileDefinition.generated.h"

class UAnimMontage;
class UAnimSequence;
class USkeletalMesh;

USTRUCT(BlueprintType)
struct GOLEMCOMBAT_API FGolemAttackDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Combat|Attack")
	FGameplayTag ActionTag;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Combat|Attack")
	TSoftObjectPtr<UAnimMontage> Montage;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Combat|Attack")
	FName SectionName;
	/** Damage as a multiple of the attacker's Power: 1.0 for a light hit, 2.5 for a heavy one. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Combat|Attack")
	float PowerScale = 1.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Combat|Attack")
	float TraceRadius = 35.0f;

	/** True for hits that place the attacker's element on a void (heavy attacks and some skill books). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Combat|Attack")
	bool bAppliesColor = false;
};

USTRUCT(BlueprintType)
struct GOLEMCOMBAT_API FGolemCombatTuning
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Combat|Defense")
	float BlockFacingCosine = 0.35f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Combat|Defense")
	float DodgeDuration = 0.65f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Combat|Defense")
	float DodgeInvulnerabilityStart = 0.05f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Combat|Defense")
	float DodgeInvulnerabilityEnd = 0.4f;
	/** Seconds before the next dodge at Agility 100; higher Agility shortens it in proportion. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Combat|Defense")
	float DodgeCooldownSeconds = 1.5f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Combat|Defense")
	float DodgeSpeed = 600.0f;
	/** Furthest an attack target may be before the Golem stops turning towards it. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Combat|Targeting", meta = (Units = "cm"))
	float TargetRange = 1200.0f;

	FGolemResult Validate() const;
};

/** The one move that belongs to a class (DEV_SPEC section 4). All three sit on the same button. */
UENUM(BlueprintType)
enum class EGolemClassMove : uint8
{
	/** Warrior: hold to take a fraction of damage from the front, for a limited time. */
	Guard,
	/** Hunter: leap backwards and loose one shot that places no colour. */
	BackflipShot,
	/** Mage: every teammate nearby ignores the next hit, for a short time. */
	Barrier
};

/**
 * What differs between the three Golems inside the shared move set (DEV_SPEC section 4): how long
 * the heavy attack winds up, whether it is a swing or a straight projectile, and the class move.
 */
USTRUCT(BlueprintType)
struct GOLEMCOMBAT_API FGolemHeroMoveTuning
{
	GENERATED_BODY()

	/** Seconds from the button press to the moment the heavy attack lands or fires; 0 keeps the animation's own timing. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Combat|Heavy", meta = (ClampMin = "0.0", Units = "s"))
	float HeavyChargeSeconds = 0.0f;

	/** Bow and staff: the heavy attack is a projectile that must touch the void itself. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Combat|Heavy")
	bool bHeavyIsProjectile = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Combat|Heavy", meta = (ClampMin = "100.0", Units = "cm"))
	float ProjectileRange = 2500.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Combat|Heavy", meta = (ClampMin = "100.0", Units = "cm/s"))
	float ProjectileSpeed = 3000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Combat|Heavy", meta = (ClampMin = "1.0", Units = "cm"))
	float ProjectileRadius = 18.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Combat|ClassMove")
	EGolemClassMove ClassMove = EGolemClassMove::Guard;

	/** Seconds before the class move can be used again; a Guard counts from the moment it ends. 0 for none. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Combat|ClassMove", meta = (ClampMin = "0.0", Units = "s"))
	float ClassMoveCooldownSeconds = 0.0f;

	/** Guard: share of frontal damage that still gets through; 0.2 is an 80% reduction. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Combat|ClassMove", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float GuardDamageFraction = 0.2f;

	/** Guard: longest time it can be held; 0 for as long as the button is down. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Combat|ClassMove", meta = (ClampMin = "0.0", Units = "s"))
	float GuardMaxSeconds = 0.0f;

	/** Backflip shot: distance covered backwards. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Combat|ClassMove", meta = (ClampMin = "0.0", Units = "cm"))
	float BackflipDistance = 500.0f;

	/** Backflip shot: damage of the shot as a multiple of Power. It never places colour. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Combat|ClassMove", meta = (ClampMin = "0.0"))
	float BackflipShotPowerScale = 1.5f;

	/** Barrier: teammates within this distance are covered. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Combat|ClassMove", meta = (ClampMin = "0.0", Units = "cm"))
	float BarrierRadius = 400.0f;

	/** Barrier: seconds it waits for a hit to stop. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Combat|ClassMove", meta = (ClampMin = "0.1", Units = "s"))
	float BarrierSeconds = 2.0f;

	FGolemResult Validate() const;
};

/** Authored identity and animation contracts are validated before any combat grants become active. */
UCLASS(BlueprintType)
class GOLEMCOMBAT_API UGolemCombatProfileDefinition : public UGolemDefinition
{
	GENERATED_BODY()

public:
	UGolemCombatProfileDefinition();
	USkeletalMesh* GetSkeletalMesh() const;
	UAnimSequence* GetIdleAnimation() const;
	UAnimSequence* GetWalkAnimation() const;
	UAnimMontage* GetDodgeMontage() const;
	const TArray<FGolemAttackDefinition>& GetAttacks() const;
	const FGolemCombatTuning& GetTuning() const;
	FPrimaryAssetId GetCombatAbilitySetId() const;
	FGolemResult ValidateCombatProfile() const;
	FGolemResult TrySetPresentation(USkeletalMesh* Mesh, UAnimSequence* Idle, UAnimSequence* Walk);
	FGolemResult TrySetAttacks(const TArray<FGolemAttackDefinition>& InAttacks);
	FGolemResult TrySetDodgeMontage(UAnimMontage* Dodge);
	FGolemResult TrySetTuning(const FGolemCombatTuning& InTuning);
	FGolemResult TrySetCombatAbilitySetId(const FPrimaryAssetId& InAbilitySetId);
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

private:
	FGolemResult ValidateAttacks(const TArray<FGolemAttackDefinition>& InAttacks) const;
	UPROPERTY(EditDefaultsOnly, Category = "Golem|Combat|Animation")
	TSoftObjectPtr<USkeletalMesh> SkeletalMesh;
	UPROPERTY(EditDefaultsOnly, Category = "Golem|Combat|Animation")
	TSoftObjectPtr<UAnimSequence> IdleAnimation;
	UPROPERTY(EditDefaultsOnly, Category = "Golem|Combat|Animation")
	TSoftObjectPtr<UAnimSequence> WalkAnimation;
	UPROPERTY(EditDefaultsOnly, Category = "Golem|Combat|Attack")
	TArray<FGolemAttackDefinition> Attacks;
	UPROPERTY(EditDefaultsOnly, Category = "Golem|Combat|Animation")
	TSoftObjectPtr<UAnimMontage> DodgeMontage;
	UPROPERTY(EditDefaultsOnly, Category = "Golem|Combat|Tuning")
	FGolemCombatTuning Tuning;
	UPROPERTY(EditDefaultsOnly, Category = "Golem|Combat|Abilities")
	FPrimaryAssetId CombatAbilitySetId = FPrimaryAssetId(TEXT("GolemAbilitySet"), TEXT("FoundationCombat"));
};
