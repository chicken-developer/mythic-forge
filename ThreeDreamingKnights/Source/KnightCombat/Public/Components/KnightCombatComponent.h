#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Damage/KnightDamageResponse.h"
#include "Definitions/KnightCombatProfileDefinition.h"
#include "Definitions/KnightAbilitySetDefinition.h"
#include "Events/KnightConfirmedEvent.h"
#include "KnightCombatComponent.generated.h"

class UKnightAbilitySystemComponent;
class UKnightGameplayAbility;
class UStaticMeshComponent;
class UAnimSequenceBase;

UENUM(BlueprintType, meta = (ScriptName = "KnightCombatOutcomeType"))
enum class EKnightCombatOutcome : uint8
{
	LightHit,
	HeavyHit,
	Combo,
	Blocked,
	Parried,
	Dodged,
	TargetLocked,
	Staggered,
	RuneHit,
	Finisher,
	Traversal
};

USTRUCT(BlueprintType)
struct KNIGHTCOMBAT_API FKnightCombatOutcome : public FKnightConfirmedEvent
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "Knight|Combat")
	EKnightCombatOutcome Outcome = EKnightCombatOutcome::LightHit;
	UPROPERTY(BlueprintReadOnly, Category = "Knight|Combat")
	FKnightEntityId TargetId;
	UPROPERTY(BlueprintReadOnly, Category = "Knight|Combat")
	FGameplayTag ActionTag;
	UPROPERTY(BlueprintReadOnly, Category = "Knight|Combat")
	float Amount = 0.0f;
};

USTRUCT(BlueprintType)
struct KNIGHTCOMBAT_API FKnightCombatSnapshot
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "Knight|Combat")
	float Poise = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category = "Knight|Combat")
	int32 ComboIndex = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Knight|Combat")
	TObjectPtr<AActor> LockedTarget;
	UPROPERTY(BlueprintReadOnly, Category = "Knight|Combat")
	bool bAttacking = false;
	UPROPERTY(BlueprintReadOnly, Category = "Knight|Combat")
	bool bBlocking = false;
	UPROPERTY(BlueprintReadOnly, Category = "Knight|Combat")
	bool bParrying = false;
	UPROPERTY(BlueprintReadOnly, Category = "Knight|Combat")
	bool bDodging = false;
	UPROPERTY(BlueprintReadOnly, Category = "Knight|Combat")
	bool bStaggered = false;
	UPROPERTY(BlueprintReadOnly, Category = "Knight|Combat")
	TArray<int32> OutcomeCounts;
	UPROPERTY(BlueprintReadOnly, Category = "Knight|Combat")
	int32 Revision = 0;
	int32 GetOutcomeCount(EKnightCombatOutcome Outcome) const;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FOnKnightCombatSnapshotChanged, const FKnightCombatSnapshot&);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnKnightCombatOutcome, const FKnightCombatOutcome&);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnKnightCombatProfileChanged, UKnightCombatProfileDefinition*);

/** Optional combat domain. Canonical identity and GAS attributes remain on the owning actor. */
UCLASS(ClassGroup = Knight, meta = (BlueprintSpawnableComponent))
class KNIGHTCOMBAT_API UKnightCombatComponent : public UActorComponent, public IKnightDamageResponse
{
	GENERATED_BODY()

public:
	UKnightCombatComponent();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	FKnightResult Configure(const FPrimaryAssetId& InProfileId);
	FKnightResult TryInitializePresentation();
	/** Reapplies a native movement baseline without removing an active replicated rune status. */
	FKnightResult RefreshBaseWalkSpeed(float BaseSpeed);
	UKnightCombatProfileDefinition* GetProfile() const;
	FPrimaryAssetId GetProfileId() const;
	const FKnightCombatSnapshot& GetSnapshot() const;
	bool CanTransferControl() const;
	bool HasOpenComboWindow() const;
	FKnightResult ResetForCheckpoint();
	void ClearTransientActions();
	FKnightResult BeginAttack(FGameplayTag ActionTag, UKnightGameplayAbility* Ability,
		UAnimMontage*& OutMontage, FName& OutSection);
	void BufferAttackInput();
	void EndAttack(UKnightGameplayAbility* Ability);
	void BeginTraceWindow(FName WindowId, UAnimSequenceBase* Animation);
	void TickTraceWindow(FName WindowId, UAnimSequenceBase* Animation);
	void EndTraceWindow(FName WindowId, UAnimSequenceBase* Animation);
	void SetComboWindow(bool bOpen, UAnimSequenceBase* Animation);
	FKnightResult BeginDefense(FGameplayTag ActionTag);
	void EndDefense(FGameplayTag ActionTag);
	FKnightResult BeginDodge();
	void EndDodge();
	FKnightResult CastRune(AActor* Target);
	AActor* FindCombatTarget(float Range = 1200.0f, bool bRequireStagger = false) const;
	FKnightResult ValidateCombatTarget(AActor* Target, float Range) const;
	FKnightResult SetValidatedTarget(AActor* Target);
	FKnightResult TryToggleTargetLock(AActor* Target = nullptr);
	AActor* GetLockedTarget() const;
	FKnightResult SpendStamina(float Amount, FGameplayTag ActionTag = FGameplayTag());
	void PublishOutcome(EKnightCombatOutcome Outcome, FGameplayTag ActionTag,
		AActor* Target = nullptr, float Amount = 0.0f);
	virtual void ResolveIncomingDamage(AActor* Source, float InDamage, float PoiseDamage,
		bool bCanDefend, float& OutHealthDamage) override;
	FOnKnightCombatSnapshotChanged OnCombatSnapshotChanged;
	FOnKnightCombatOutcome OnCombatOutcome;
	FOnKnightCombatProfileChanged OnProfileChanged;

protected:
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

private:
	UFUNCTION()
	void OnRep_ProfileId();
	UFUNCTION()
	void OnRep_Snapshot();
	FKnightResult ApplyProfile();
	void RefreshSnapshot();
	void PerformWeaponSweep();
	bool IsTraceCapabilityCurrent(uint64 Generation, FName WindowId,
		const TWeakObjectPtr<UKnightGameplayAbility>& Ability, int32 Index) const;
	void EnterStagger(AActor* Source);
	void EndStagger();
	void HandleRuneTagChanged(const FGameplayTag Tag, int32 Count);
	void HandleLifeTagChanged(const FGameplayTag Tag, int32 Count);
	void UpdateLockCamera();
	bool IsAuthorityAlive() const;
	float GetProjectedDamage(FGameplayTag ActionTag, float AuthoredDamage) const;
	const FKnightAttackDefinition* GetCurrentAttack() const;
	UPROPERTY(ReplicatedUsing = OnRep_ProfileId)
	FPrimaryAssetId ProfileId;
	UPROPERTY(ReplicatedUsing = OnRep_Snapshot)
	FKnightCombatSnapshot Snapshot;
	UPROPERTY(Transient)
	TObjectPtr<UKnightCombatProfileDefinition> Profile;
	UPROPERTY(Transient)
	TObjectPtr<UKnightAbilitySetDefinition> CombatAbilitySet;
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Hammer;
	UPROPERTY(Transient)
	TWeakObjectPtr<UKnightAbilitySystemComponent> AbilitySystem;
	UPROPERTY(Transient)
	TWeakObjectPtr<UKnightGameplayAbility> AttackAbility;
	FKnightAbilitySetHandle GrantHandle;
	TSet<TWeakObjectPtr<AActor>> WindowHitActors;
	FName ActiveWindow;
	FVector PreviousHammerHead = FVector::ZeroVector;
	FGameplayTag AttackAction;
	int32 AttackIndex = INDEX_NONE;
	uint64 TraceGeneration = 0;
	bool bComboWindow = false;
	bool bBufferedAttack = false;
	bool bEndingPlay = false;
	float DodgeStartedAt = 0.0f;
	float ParryStartedAt = 0.0f;
	float LockValidationElapsed = 0.0f;
	float SavedWalkSpeed = 0.0f;
	FDelegateHandle RuneTagHandle;
	FDelegateHandle DownedTagHandle;
	FDelegateHandle DeadTagHandle;
	FTimerHandle StaggerTimer;
	FGuid LockCameraHandle;
};
