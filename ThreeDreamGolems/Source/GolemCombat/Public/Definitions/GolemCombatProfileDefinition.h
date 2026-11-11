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
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Combat|Attack")
	float Damage = 20.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Combat|Attack")
	float PoiseDamage = 20.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Combat|Attack")
	float StaminaCost = 8.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Combat|Attack")
	float TraceRadius = 35.0f;
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
	float BlockStaminaCost = 8.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Combat|Defense")
	float ParryWindow = 0.28f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Combat|Defense")
	float ParryStaminaCost = 12.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Combat|Defense")
	float DodgeDuration = 0.65f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Combat|Defense")
	float DodgeInvulnerabilityStart = 0.08f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Combat|Defense")
	float DodgeInvulnerabilityEnd = 0.42f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Combat|Defense")
	float DodgeStaminaCost = 15.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Combat|Defense")
	float DodgeSpeed = 600.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Combat|Targeting")
	float LockRange = 1200.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Combat|Rune")
	float RuneRange = 900.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Combat|Rune")
	float RuneDamage = 30.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Combat|Rune")
	float RunePoiseDamage = 30.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Combat|Rune")
	float RuneEnergyCost = 20.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Combat|Rune")
	float RuneSlowDuration = 3.0f;

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
	UAnimMontage* GetRuneMontage() const;
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
	FGolemResult TrySetActionMontages(UAnimMontage* Dodge, UAnimMontage* Rune, UAnimMontage* Stagger,
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
	TSoftObjectPtr<UAnimMontage> RuneMontage;
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
