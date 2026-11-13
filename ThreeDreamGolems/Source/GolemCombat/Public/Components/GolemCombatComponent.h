#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Damage/GolemDamageResponse.h"
#include "Definitions/GolemCombatProfileDefinition.h"
#include "Definitions/GolemAbilitySetDefinition.h"
#include "Events/GolemConfirmedEvent.h"
#include "GolemCombatComponent.generated.h"

class UGolemAbilitySystemComponent;
class UGolemColorVoidComponent;
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
	Dodged
};

/** Number of EGolemCombatOutcome values; the snapshot keeps one counter per outcome. */
constexpr int32 GolemCombatOutcomeCount = static_cast<int32>(EGolemCombatOutcome::Dodged) + 1;

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
	int32 ComboIndex = 0;
	/** Who this Golem turns towards when it attacks. Only AI sets it; a player's Golem attacks the way it faces. */
	UPROPERTY(BlueprintReadOnly, Category = "Golem|Combat")
	TObjectPtr<AActor> AttackTarget;
	UPROPERTY(BlueprintReadOnly, Category = "Golem|Combat")
	bool bAttacking = false;
	/** Authoritative montage notify phase: 0 idle, 1 windup, 2 active trace, 3 recovery. */
	UPROPERTY(BlueprintReadOnly, Category = "Golem|Combat")
	uint8 AttackPhase = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Golem|Combat")
	bool bBlocking = false;
	UPROPERTY(BlueprintReadOnly, Category = "Golem|Combat")
	bool bDodging = false;
	UPROPERTY(BlueprintReadOnly, Category = "Golem|Combat")
	TArray<int32> OutcomeCounts;
	UPROPERTY(BlueprintReadOnly, Category = "Golem|Combat")
	double LastOutcomeServerTime = 0.0;
	/** Host time from which the next dodge is allowed. */
	UPROPERTY(BlueprintReadOnly, Category = "Golem|Combat")
	double DodgeReadyServerTime = 0.0;
	/** Host time from which the class move is allowed again. */
	UPROPERTY(BlueprintReadOnly, Category = "Golem|Combat")
	double ClassMoveReadyServerTime = 0.0;
	/** Host time until which a Mage's barrier stops the next hit; 0 when there is none. */
	UPROPERTY(BlueprintReadOnly, Category = "Golem|Combat")
	double BarrierEndServerTime = 0.0;
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
	/** BaseSpeed is the speed at Agility 100; the owner's Agility scales it. */
	FGolemResult RefreshBaseWalkSpeed(float BaseSpeed);

	/** Per-Golem differences inside the shared move set. Call on the host and on clients alike. */
	FGolemResult SetHeroMoves(const FGolemHeroMoveTuning& InMoves);
	const FGolemHeroMoveTuning& GetHeroMoves() const { return HeroMoves; }

	/**
	 * The void an AI-controlled Golem is about to colour. It is the aim point of its heavy attack
	 * and the only void that attack may colour: an AI never places colour anywhere it was not sent.
	 * Players are not restricted. Pass nullptr to clear. bOnlyIfPrimed is for follow-ups, which add
	 * to a window somebody else opened and must never open one themselves.
	 */
	void SetColorIntent(UGolemColorVoidComponent* Void, bool bOnlyIfPrimed = false);
	UGolemColorVoidComponent* GetColorIntent() const;

	/** Power multiplied by the attack's coefficient (DEV_SPEC section 4). */
	float GetAttackDamage(float PowerScale) const;

	/** Seconds the next dodge is still locked at host time Now. */
	float GetDodgeCooldownRemaining(double Now) const;

	/** True when one is a party Golem and the other an enemy, and both are alive. */
	static bool AreHostile(const AActor* First, const AActor* Second);
	UGolemCombatProfileDefinition* GetProfile() const;
	FPrimaryAssetId GetProfileId() const;
	const FGolemCombatSnapshot& GetSnapshot() const;
	bool CanTransferControl() const;
	bool HasOpenComboWindow() const;
	FGolemResult ResetForCheckpoint();
	void ClearTransientActions();
	/** OutPlayRate slows or speeds the wind-up so a charged heavy attack lands after HeavyChargeSeconds. */
	FGolemResult BeginAttack(FGameplayTag ActionTag, UGolemGameplayAbility* Ability,
		UAnimMontage*& OutMontage, FName& OutSection, float& OutPlayRate);
	void BufferAttackInput();
	void EndAttack(UGolemGameplayAbility* Ability);
	void BeginTraceWindow(FName WindowId, UAnimSequenceBase* Animation);
	void TickTraceWindow(FName WindowId, UAnimSequenceBase* Animation);
	void EndTraceWindow(FName WindowId, UAnimSequenceBase* Animation);
	void SetComboWindow(bool bOpen, UAnimSequenceBase* Animation);
	FGolemResult BeginDodge();

	/**
	 * Starts this Golem's class move. OutMove tells the ability which one it is and OutDuration how
	 * long it runs by itself (0 for a Guard held until the button is released).
	 */
	FGolemResult BeginClassMove(EGolemClassMove& OutMove, float& OutDuration);
	void EndClassMove(bool bWasCancelled);
	float GetClassMoveCooldownRemaining(double Now) const;

	/** Authority only. The next hit before EndTime deals no damage and uses the barrier up. */
	void GrantBarrier(double EndTime);
	bool HasBarrierAt(double Now) const { return Snapshot.BarrierEndServerTime > Now; }

	/** The next dodge goes this way instead of along the movement input. AI uses it to dodge away from a threat. */
	void SetNextDodgeDirection(const FVector& Direction);
	void EndDodge();
	AActor* FindCombatTarget(float Range = 1200.0f) const;
	FGolemResult ValidateCombatTarget(AActor* Target, float Range) const;
	FGolemResult SetValidatedTarget(AActor* Target);
	AActor* GetAttackTarget() const;
	void PublishOutcome(EGolemCombatOutcome Outcome, FGameplayTag ActionTag,
		AActor* Target = nullptr, float Amount = 0.0f);
	virtual void ResolveIncomingDamage(AActor* Source, float InDamage, bool bCanDefend, float& OutHealthDamage) override;
	FOnGolemCombatSnapshotChanged OnCombatSnapshotChanged;
	FOnGolemCombatOutcome OnCombatOutcome;
	FOnGolemCombatProfileChanged OnProfileChanged;

protected:
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

private:
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
	void HandleLifeTagChanged(const FGameplayTag Tag, int32 Count);
	bool IsAuthorityAlive() const;
	bool IsColorHitAllowed(const UGolemColorVoidComponent* Void) const;
	bool PlaceColor(UGolemColorVoidComponent* Void);
	void FireProjectile(float PowerScale, bool bAppliesColor, FGameplayTag ActionTag);
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
	FGolemHeroMoveTuning HeroMoves;
	TWeakObjectPtr<UGolemColorVoidComponent> ColorIntent;
	bool bColorIntentOnlyIfPrimed = false;
	float BaseWalkSpeed = 0.0f;
	FVector NextDodgeDirection = FVector::ZeroVector;
	bool bChargeRateActive = false;
	bool bClassMoveActive = false;
	EGolemClassMove ActiveClassMove = EGolemClassMove::Guard;
	bool bProjectileFired = false;
	float DodgeStartedAt = 0.0f;
	float TargetValidationElapsed = 0.0f;
	FDelegateHandle DownedTagHandle;
	FDelegateHandle DeadTagHandle;
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
