#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Damage/GolemDamageResponse.h"
#include "Definitions/GolemCombatProfileDefinition.h"
#include "Definitions/GolemAbilitySetDefinition.h"
#include "Events/GolemConfirmedEvent.h"
#include "GolemCombatComponent.generated.h"

class UGolemAbilitySystemComponent;
class UGolemGameplayAbility;
class UStaticMeshComponent;
class UPrimitiveComponent;
class UAnimSequenceBase;
struct FOnAttributeChangeData;

UENUM(BlueprintType, meta = (ScriptName = "GolemCombatOutcomeType"))
enum class EGolemCombatOutcome : uint8
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
struct GOLEMCOMBAT_API FGolemCombatOutcome : public FGolemConfirmedEvent
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "Golem|Combat")
	EGolemCombatOutcome Outcome = EGolemCombatOutcome::LightHit;
	UPROPERTY(BlueprintReadOnly, Category = "Golem|Combat")
	FGolemEntityId TargetId;
	UPROPERTY(BlueprintReadOnly, Category = "Golem|Combat")
	FGameplayTag ActionTag;
	UPROPERTY(BlueprintReadOnly, Category = "Golem|Combat")
	float Amount = 0.0f;
};

USTRUCT(BlueprintType)
struct GOLEMCOMBAT_API FGolemCombatSnapshot
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "Golem|Combat")
	float Poise = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category = "Golem|Combat")
	int32 ComboIndex = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Golem|Combat")
	TObjectPtr<AActor> LockedTarget;
	UPROPERTY(BlueprintReadOnly, Category = "Golem|Combat")
	bool bAttacking = false;
	/** Authoritative montage notify phase: 0 idle, 1 windup, 2 active trace, 3 recovery. */
	UPROPERTY(BlueprintReadOnly, Category = "Golem|Combat")
	uint8 AttackPhase = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Golem|Combat")
	bool bBlocking = false;
	UPROPERTY(BlueprintReadOnly, Category = "Golem|Combat")
	bool bParrying = false;
	UPROPERTY(BlueprintReadOnly, Category = "Golem|Combat")
	bool bDodging = false;
	UPROPERTY(BlueprintReadOnly, Category = "Golem|Combat")
	bool bStaggered = false;
	UPROPERTY(BlueprintReadOnly, Category = "Golem|Combat")
	TArray<int32> OutcomeCounts;
	UPROPERTY(BlueprintReadOnly, Category = "Golem|Combat")
	double LastOutcomeServerTime = 0.0;
	UPROPERTY(BlueprintReadOnly, Category = "Golem|Combat")
	int32 Revision = 0;
	int32 GetOutcomeCount(EGolemCombatOutcome Outcome) const;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FOnGolemCombatSnapshotChanged, const FGolemCombatSnapshot&);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnGolemCombatOutcome, const FGolemCombatOutcome&);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnGolemCombatProfileChanged, UGolemCombatProfileDefinition*);

/** Optional combat domain. Canonical identity and GAS attributes remain on the owning actor. */
UCLASS(ClassGroup = Golem, meta = (BlueprintSpawnableComponent))
class GOLEMCOMBAT_API UGolemCombatComponent : public UActorComponent, public IGolemDamageResponse
{
	GENERATED_BODY()

public:
	UGolemCombatComponent();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	FGolemResult Configure(const FPrimaryAssetId& InProfileId);
	FGolemResult TryInitializePresentation();
	/** Reapplies a native movement baseline without removing an active replicated rune status. */
	FGolemResult RefreshBaseWalkSpeed(float BaseSpeed);
	UGolemCombatProfileDefinition* GetProfile() const;
	FPrimaryAssetId GetProfileId() const;
	const FGolemCombatSnapshot& GetSnapshot() const;
	bool CanTransferControl() const;
	bool HasOpenComboWindow() const;
	FGolemResult ResetForCheckpoint();
	void ClearTransientActions();
	FGolemResult BeginAttack(FGameplayTag ActionTag, UGolemGameplayAbility* Ability,
		UAnimMontage*& OutMontage, FName& OutSection);
	void BufferAttackInput();
	void EndAttack(UGolemGameplayAbility* Ability);
	void BeginTraceWindow(FName WindowId, UAnimSequenceBase* Animation);
	void TickTraceWindow(FName WindowId, UAnimSequenceBase* Animation);
	void EndTraceWindow(FName WindowId, UAnimSequenceBase* Animation);
	void SetComboWindow(bool bOpen, UAnimSequenceBase* Animation);
	FGolemResult BeginDefense(FGameplayTag ActionTag);
	void EndDefense(FGameplayTag ActionTag);
	FGolemResult BeginDodge();
	void EndDodge();
	FGolemResult CastRune(AActor* Target);
	AActor* FindCombatTarget(float Range = 1200.0f, bool bRequireStagger = false) const;
	FGolemResult ValidateCombatTarget(AActor* Target, float Range) const;
	FGolemResult SetValidatedTarget(AActor* Target);
	FGolemResult TryToggleTargetLock(AActor* Target = nullptr);
	AActor* GetLockedTarget() const;
	FGolemResult SpendStamina(float Amount, FGameplayTag ActionTag = FGameplayTag());
	void PublishOutcome(EGolemCombatOutcome Outcome, FGameplayTag ActionTag,
		AActor* Target = nullptr, float Amount = 0.0f);
	virtual void ResolveIncomingDamage(AActor* Source, float InDamage, float PoiseDamage,
		bool bCanDefend, float& OutHealthDamage) override;
	virtual void ResolveIncomingDamageWithContext(AActor* Source, float InDamage, float PoiseDamage,
		bool bCanDefend, const FGolemIncomingHitContext& Context, float& OutHealthDamage) override;
	float GetMaxPoise() const;
	/** Restores a validated in-memory pool after the owning build projection has committed. */
	FGolemResult RestoreSnapshotPoise(float InPoise);
	FOnGolemCombatSnapshotChanged OnCombatSnapshotChanged;
	FOnGolemCombatOutcome OnCombatOutcome;
	FOnGolemCombatProfileChanged OnProfileChanged;

protected:
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

private:
	void HandlePoiseBonusChanged(const FOnAttributeChangeData& Change);
	UFUNCTION()
	void OnRep_ProfileId();
	UFUNCTION()
	void OnRep_Snapshot();
	FGolemResult ApplyProfile();
	void RefreshSnapshot();
	void PerformWeaponSweep();
	void CommitWeaponHit(const FHitResult& Hit, const FGolemAttackDefinition& Attack, FGameplayTag ActionTag,
		uint64 Generation, FName WindowId, const TWeakObjectPtr<UGolemGameplayAbility>& Ability, int32 Index);
	void FlushDeferredWeaponHits(uint64 Generation, FName WindowId,
		const TWeakObjectPtr<UGolemGameplayAbility>& Ability, int32 Index);
	void ClearDeferredWeaponHits();
	void BeginTraceGeometryDiagnostics();
	void RecordTraceGeometrySweep(const FVector& Hand, const FVector& Head, const FVector& PreviousHead,
		const TArray<FHitResult>& Hits, const TArray<FHitResult>& MovingHits);
	void RecordTraceGeometryDecision(const FHitResult& Hit, FName Decision);
	void EndTraceGeometryDiagnostics(FName Reason);
	bool IsTraceCapabilityCurrent(uint64 Generation, FName WindowId,
		const TWeakObjectPtr<UGolemGameplayAbility>& Ability, int32 Index) const;
	void EnterStagger(AActor* Source);
	void EndStagger();
	void HandleRuneTagChanged(const FGameplayTag Tag, int32 Count);
	void HandleLifeTagChanged(const FGameplayTag Tag, int32 Count);
	void UpdateLockCamera();
	bool IsAuthorityAlive() const;
	float GetProjectedDamage(FGameplayTag ActionTag, float AuthoredDamage) const;
	const FGolemAttackDefinition* GetCurrentAttack() const;
	UPROPERTY(ReplicatedUsing = OnRep_ProfileId)
	FPrimaryAssetId ProfileId;
	UPROPERTY(ReplicatedUsing = OnRep_Snapshot)
	FGolemCombatSnapshot Snapshot;
	UPROPERTY(Transient)
	TObjectPtr<UGolemCombatProfileDefinition> Profile;
	UPROPERTY(Transient)
	TObjectPtr<UGolemAbilitySetDefinition> CombatAbilitySet;
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Hammer;
	UPROPERTY(Transient)
	TWeakObjectPtr<UGolemAbilitySystemComponent> AbilitySystem;
	UPROPERTY(Transient)
	TWeakObjectPtr<UGolemGameplayAbility> AttackAbility;
	FGolemAbilitySetHandle GrantHandle;
	TSet<TWeakObjectPtr<AActor>> WindowHitActors;
	TMap<TWeakObjectPtr<AActor>, FHitResult> DeferredWindowHits;
	TArray<TWeakObjectPtr<AActor>> DeferredWindowHitOrder;
	FName ActiveWindow;
	FVector PreviousHammerHead = FVector::ZeroVector;
	FGameplayTag AttackAction;
	int32 AttackIndex = INDEX_NONE;
	uint64 TraceGeneration = 0;
	bool bComboWindow = false;
	bool bBufferedAttack = false;
	bool bEndingPlay = false;
	bool bSnapshotPublicationPending = false;
	float DodgeStartedAt = 0.0f;
	float ParryStartedAt = 0.0f;
	float LockValidationElapsed = 0.0f;
	float SavedWalkSpeed = 0.0f;
	FDelegateHandle RuneTagHandle;
	FDelegateHandle DownedTagHandle;
	FDelegateHandle DeadTagHandle;
	FDelegateHandle PoiseBonusHandle;
	FTimerHandle StaggerTimer;
	FGuid LockCameraHandle;
	FGuid AttackActivationId;
	int32 WindowHitIndex = 0;
	struct FTraceGeometryDiagnostics
	{
		TWeakObjectPtr<UPrimitiveComponent> ArmorZone;
		bool bActive = false;
		bool bLoggedContact = false;
		bool bLoggedZone = false;
		bool bLoggedAccepted = false;
		bool bLoggedZoneDedup = false;
		int32 Sweeps = 0;
		int32 ArmorCandidates = 0;
		int32 OtherCandidates = 0;
		FName FirstContact;
		FName AcceptedComponent;
		FVector HandMin = FVector::ZeroVector;
		FVector HandMax = FVector::ZeroVector;
		FVector HeadMin = FVector::ZeroVector;
		FVector HeadMax = FVector::ZeroVector;
		float MinSegmentCenterDistance = MAX_flt;
	};
	FTraceGeometryDiagnostics TraceGeometryDiagnostics;
	int32 TraceGeometryLogCount = 0;
};
