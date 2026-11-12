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
	float PoiseDamage = 20.0f;
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
	float MaxPoise = 50.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Combat|Defense")
	float StaggerDuration = 2.5f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Combat|Defense")
	float BlockFacingCosine = 0.35f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Combat|Defense")
	float BlockDamageFraction = 0.1f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Combat|Defense")
	float ParryWindow = 0.28f;
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
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Combat|Targeting")
	float LockRange = 1200.0f;

	FGolemResult Validate() const;
};

/**
 * What differs between the three Golems inside the shared move set (DEV_SPEC section 4): how long
 * the heavy attack winds up, and whether it is a swing or a straight projectile.
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
	UAnimMontage* GetStaggerMontage() const;
	UAnimMontage* GetTraversalMontage() const;
	UAnimMontage* GetFinisherSourceMontage() const;
	UAnimMontage* GetFinisherTargetMontage() const;
	const TArray<FGolemAttackDefinition>& GetAttacks() const;
	const FGolemCombatTuning& GetTuning() const;
	FPrimaryAssetId GetCombatAbilitySetId() const;
	FGolemResult ValidateCombatProfile() const;
	FGolemResult TrySetPresentation(USkeletalMesh* Mesh, UAnimSequence* Idle, UAnimSequence* Walk);
	FGolemResult TrySetAttacks(const TArray<FGolemAttackDefinition>& InAttacks);
	FGolemResult TrySetActionMontages(UAnimMontage* Dodge, UAnimMontage* Stagger,
		UAnimMontage* Traversal, UAnimMontage* FinisherSource, UAnimMontage* FinisherTarget);
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
	UPROPERTY(EditDefaultsOnly, Category = "Golem|Combat|Animation")
	TSoftObjectPtr<UAnimMontage> StaggerMontage;
	UPROPERTY(EditDefaultsOnly, Category = "Golem|Combat|Animation")
	TSoftObjectPtr<UAnimMontage> TraversalMontage;
	UPROPERTY(EditDefaultsOnly, Category = "Golem|Combat|Animation")
	TSoftObjectPtr<UAnimMontage> FinisherSourceMontage;
	UPROPERTY(EditDefaultsOnly, Category = "Golem|Combat|Animation")
	TSoftObjectPtr<UAnimMontage> FinisherTargetMontage;
	UPROPERTY(EditDefaultsOnly, Category = "Golem|Combat|Tuning")
	FGolemCombatTuning Tuning;
	UPROPERTY(EditDefaultsOnly, Category = "Golem|Combat|Abilities")
	FPrimaryAssetId CombatAbilitySetId = FPrimaryAssetId(TEXT("GolemAbilitySet"), TEXT("FoundationCombat"));
};
