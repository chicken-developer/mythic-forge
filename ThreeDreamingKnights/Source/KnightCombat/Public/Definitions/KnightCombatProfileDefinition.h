#pragma once

#include "CoreMinimal.h"
#include "Assets/KnightDefinition.h"
#include "GameplayTagContainer.h"
#include "KnightCombatProfileDefinition.generated.h"

class UAnimMontage;
class UAnimSequence;
class USkeletalMesh;

USTRUCT(BlueprintType)
struct KNIGHTCOMBAT_API FKnightAttackDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Knight|Combat|Attack")
	FGameplayTag ActionTag;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Knight|Combat|Attack")
	TSoftObjectPtr<UAnimMontage> Montage;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Knight|Combat|Attack")
	FName SectionName;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Knight|Combat|Attack")
	float Damage = 20.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Knight|Combat|Attack")
	float PoiseDamage = 20.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Knight|Combat|Attack")
	float StaminaCost = 8.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Knight|Combat|Attack")
	float TraceRadius = 35.0f;
};

USTRUCT(BlueprintType)
struct KNIGHTCOMBAT_API FKnightCombatTuning
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Knight|Combat|Defense")
	float MaxPoise = 50.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Knight|Combat|Defense")
	float StaggerDuration = 2.5f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Knight|Combat|Defense")
	float BlockFacingCosine = 0.35f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Knight|Combat|Defense")
	float BlockDamageFraction = 0.1f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Knight|Combat|Defense")
	float BlockStaminaCost = 8.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Knight|Combat|Defense")
	float ParryWindow = 0.28f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Knight|Combat|Defense")
	float ParryStaminaCost = 12.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Knight|Combat|Defense")
	float DodgeDuration = 0.65f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Knight|Combat|Defense")
	float DodgeInvulnerabilityStart = 0.08f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Knight|Combat|Defense")
	float DodgeInvulnerabilityEnd = 0.42f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Knight|Combat|Defense")
	float DodgeStaminaCost = 15.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Knight|Combat|Defense")
	float DodgeSpeed = 600.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Knight|Combat|Targeting")
	float LockRange = 1200.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Knight|Combat|Rune")
	float RuneRange = 900.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Knight|Combat|Rune")
	float RuneDamage = 30.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Knight|Combat|Rune")
	float RunePoiseDamage = 30.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Knight|Combat|Rune")
	float RuneEnergyCost = 20.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Knight|Combat|Rune")
	float RuneSlowDuration = 3.0f;

	FKnightResult Validate() const;
};

/** Authored identity and animation contracts are validated before any combat grants become active. */
UCLASS(BlueprintType)
class KNIGHTCOMBAT_API UKnightCombatProfileDefinition : public UKnightDefinition
{
	GENERATED_BODY()

public:
	UKnightCombatProfileDefinition();
	USkeletalMesh* GetSkeletalMesh() const;
	UAnimSequence* GetIdleAnimation() const;
	UAnimSequence* GetWalkAnimation() const;
	UAnimMontage* GetDodgeMontage() const;
	UAnimMontage* GetRuneMontage() const;
	UAnimMontage* GetStaggerMontage() const;
	UAnimMontage* GetTraversalMontage() const;
	UAnimMontage* GetFinisherSourceMontage() const;
	UAnimMontage* GetFinisherTargetMontage() const;
	const TArray<FKnightAttackDefinition>& GetAttacks() const;
	const FKnightCombatTuning& GetTuning() const;
	FPrimaryAssetId GetCombatAbilitySetId() const;
	FKnightResult ValidateCombatProfile() const;
	FKnightResult TrySetPresentation(USkeletalMesh* Mesh, UAnimSequence* Idle, UAnimSequence* Walk);
	FKnightResult TrySetAttacks(const TArray<FKnightAttackDefinition>& InAttacks);
	FKnightResult TrySetActionMontages(UAnimMontage* Dodge, UAnimMontage* Rune, UAnimMontage* Stagger,
		UAnimMontage* Traversal, UAnimMontage* FinisherSource, UAnimMontage* FinisherTarget);
	FKnightResult TrySetTuning(const FKnightCombatTuning& InTuning);
	FKnightResult TrySetCombatAbilitySetId(const FPrimaryAssetId& InAbilitySetId);
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

private:
	FKnightResult ValidateAttacks(const TArray<FKnightAttackDefinition>& InAttacks) const;
	UPROPERTY(EditDefaultsOnly, Category = "Knight|Combat|Animation")
	TSoftObjectPtr<USkeletalMesh> SkeletalMesh;
	UPROPERTY(EditDefaultsOnly, Category = "Knight|Combat|Animation")
	TSoftObjectPtr<UAnimSequence> IdleAnimation;
	UPROPERTY(EditDefaultsOnly, Category = "Knight|Combat|Animation")
	TSoftObjectPtr<UAnimSequence> WalkAnimation;
	UPROPERTY(EditDefaultsOnly, Category = "Knight|Combat|Attack")
	TArray<FKnightAttackDefinition> Attacks;
	UPROPERTY(EditDefaultsOnly, Category = "Knight|Combat|Animation")
	TSoftObjectPtr<UAnimMontage> DodgeMontage;
	UPROPERTY(EditDefaultsOnly, Category = "Knight|Combat|Animation")
	TSoftObjectPtr<UAnimMontage> RuneMontage;
	UPROPERTY(EditDefaultsOnly, Category = "Knight|Combat|Animation")
	TSoftObjectPtr<UAnimMontage> StaggerMontage;
	UPROPERTY(EditDefaultsOnly, Category = "Knight|Combat|Animation")
	TSoftObjectPtr<UAnimMontage> TraversalMontage;
	UPROPERTY(EditDefaultsOnly, Category = "Knight|Combat|Animation")
	TSoftObjectPtr<UAnimMontage> FinisherSourceMontage;
	UPROPERTY(EditDefaultsOnly, Category = "Knight|Combat|Animation")
	TSoftObjectPtr<UAnimMontage> FinisherTargetMontage;
	UPROPERTY(EditDefaultsOnly, Category = "Knight|Combat|Tuning")
	FKnightCombatTuning Tuning;
	UPROPERTY(EditDefaultsOnly, Category = "Knight|Combat|Abilities")
	FPrimaryAssetId CombatAbilitySetId = FPrimaryAssetId(TEXT("KnightAbilitySet"), TEXT("FoundationCombat"));
};
