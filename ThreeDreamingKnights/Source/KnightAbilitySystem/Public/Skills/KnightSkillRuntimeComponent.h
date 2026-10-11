#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayAbilitySpec.h"
#include "GameplayEffectTypes.h"
#include "Projection/KnightSourceProjection.h"
#include "Skills/KnightSkillTypes.h"
#include "Skills/KnightEquipmentCombatTypes.h"
#include "KnightSkillRuntimeComponent.generated.h"

class UKnightAbilitySystemComponent;
class UKnightAttributeSet;
class UKnightSpecializationAbility;
enum class EKnightLifeState : uint8;
struct FOnAttributeChangeData;

USTRUCT(BlueprintType)
struct KNIGHTABILITYSYSTEM_API FKnightSkillRuntimeState
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Knight|Skills")
	FKnightSkillBuild Build;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Knight|Skills")
	TArray<double> CooldownEnds;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Knight|Skills")
	int32 Stacks = 0;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Knight|Skills")
	double StackEnd = 0.0;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Knight|Skills")
	double BuffEnd = 0.0;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Knight|Skills")
	float Shield = 0.0f;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Knight|Skills")
	float DeferredDamage = 0.0f;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Knight|Skills")
	int32 Chilled = 0;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Knight|Skills")
	double ChilledEnd = 0.0;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Knight|Skills")
	double WaterReadyEnd = 0.0;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Knight|Skills")
	double IceReadyEnd = 0.0;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Knight|Skills")
	int32 Revision = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Knight|Equipment")
	FKnightEquipmentCombatBuild EquipmentCombatBuild;
	UPROPERTY(BlueprintReadOnly, Category = "Knight|Equipment")
	double BulwarkEnd = 0.0;
	UPROPERTY(BlueprintReadOnly, Category = "Knight|Equipment")
	double LifeseedEnd = 0.0;
	UPROPERTY(BlueprintReadOnly, Category = "Knight|Equipment")
	double StarfallEnd = 0.0;
	UPROPERTY(BlueprintReadOnly, Category = "Knight|Equipment")
	int32 StarfallCount = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Knight|Equipment")
	double StarfallCounterEnd = 0.0;
	UPROPERTY(BlueprintReadOnly, Category = "Knight|Equipment")
	double ConsumableEnd = 0.0;
	UPROPERTY(BlueprintReadOnly, Category = "Knight|Equipment")
	double CrystalEnd = 0.0;
};

/** In-memory reconstruction fragment. Cooldown time is remaining, independent of a world's clock. */
USTRUCT()
struct KNIGHTABILITYSYSTEM_API FKnightSkillRuntimeSnapshot
{
	GENERATED_BODY()
	UPROPERTY(SaveGame)
	TArray<float> CooldownRemaining;
	UPROPERTY(SaveGame)
	float Resource = 100.0f;
	UPROPERTY(SaveGame)
	int32 Stacks = 0;
	UPROPERTY(SaveGame)
	float StackRemaining = 0.0f;
	UPROPERTY(SaveGame)
	float BuffRemaining = 0.0f;
	UPROPERTY(SaveGame)
	float WaterReadyRemaining = 0.0f;
	UPROPERTY(SaveGame)
	float IceReadyRemaining = 0.0f;
	UPROPERTY(SaveGame)
	float AftershockRemaining = 0.0f;
	UPROPERTY(SaveGame)
	float StackGainRemaining = 0.0f;
	UPROPERTY(SaveGame)
	float ResourceRegenDelayRemaining = 0.0f;
	UPROPERTY(SaveGame)
	float BulwarkRemaining = 0.0f;
	UPROPERTY(SaveGame)
	float LifeseedRemaining = 0.0f;
	UPROPERTY(SaveGame)
	float StarfallRemaining = 0.0f;
	UPROPERTY(SaveGame)
	float StarfallCounterRemaining = 0.0f;
	UPROPERTY(SaveGame)
	float ConsumableRemaining = 0.0f;
	UPROPERTY(SaveGame)
	int32 StarfallCount = 0;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FOnKnightSkillStateChanged, const FKnightSkillRuntimeState&);

/** Per-character runtime. GAS owns activation; this component owns source pools and timed discrete state. */
UCLASS(ClassGroup = Knight, meta = (BlueprintSpawnableComponent))
class KNIGHTABILITYSYSTEM_API UKnightSkillRuntimeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UKnightSkillRuntimeComponent();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	FKnightResult ConfigureBuild(const FKnightSkillBuild& Build);
	FKnightResult PrepareBuild(const FKnightSkillBuild& Build);
	void CommitPreparedBuild();
	void AbortPreparedBuild();
	void PublishPreparedBuild(bool bNotifySubscribers = true);
	FKnightResult PrepareEquipmentBuild(const FKnightEquipmentCombatBuild& Build);
	void CommitPreparedEquipmentBuild();
	void AbortPreparedEquipmentBuild();
	bool RequiresHammer() const;
	float GetPowerCrystalMultiplier() const;
	FKnightResult ValidateConsumable(FName EffectId) const;
	FKnightResult PrepareConsumable(FName EffectId, const FKnightMutationFence& Fence);
	void CommitPreparedConsumable();
	void AbortPreparedConsumable();
	void PublishPreparedConsumable();
	uint64 GetItemGeneration() const;
	void NotifyItemDefenseSuccess(AActor* Source, FGuid ActivationId, bool bDirect, uint64 ExpectedGeneration);
	void NotifyConfirmedDirectDamage(AActor* Target, FGuid ActivationId, int32 HitIndex,
		EKnightSkillSchool School, float Damage, AActor* PrimaryTarget, uint64 ExpectedGeneration);
	bool HasUnsnapshottedEffects() const;
	bool HasActiveSkill() const;
	FKnightResult RequestSkill(int32 Slot, AActor* Target = nullptr);
	FKnightResult ValidateSkill(int32 Slot, AActor* Target) const;
	FKnightSkillRuntimeState GetSkillState() const;
	FKnightSkillBuild GetBuild() const;
	FKnightSkillRuntimeSnapshot CaptureSnapshot() const;
	static FKnightResult ValidateSnapshot(const FKnightSkillRuntimeSnapshot& Snapshot);
	FKnightResult RestoreSnapshot(const FKnightSkillRuntimeSnapshot& Snapshot, bool bNotifySubscribers = true);
	float GetDeferredDamage() const;
	bool HasDeferredDamage() const;
	bool IsSkillAvailable(int32 Slot) const;
	FGameplayAbilitySpecHandle GetSkillHandle(int32 Slot) const;
	AActor* SelectLowestHealthAlly(float Radius, AActor* Around = nullptr, AActor* Exclude = nullptr) const;
	void SetEncounterId(FGuid InEncounterId);
	FKnightResult AddPowerCrystalBuff();
	FKnightResult ApplyExternalHeal(float Magnitude);
	void NotifyDefenseSuccess();
	void NotifyHeavyHit(AActor* Target);
	float GetIncomingReduction(AActor* Source) const;
	float GetPoiseScale() const;
	float ResolveDamagePools(float Damage, bool bEligibleDeferred);
	void ResetForCheckpoint();
	static FKnightResult ApplySkillDamage(AActor* Source, AActor* Target, const FKnightSkillDamagePacket& Packet,
		const FHitResult* Hit = nullptr);
	FOnKnightSkillStateChanged OnSkillStateChanged;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;

private:
	friend class UKnightSpecializationAbility;
	struct FShieldPool
	{
		FName Source;
		TWeakObjectPtr<AActor> Caster;
		float Amount = 0.0f;
		float CapFraction = 0.35f;
		double End = 0.0;
		FGuid ItemSource;
	};
	struct FDebtTranche
	{
		float PerTick = 0.0f;
		int32 Ticks = 5;
		double NextTick = 0.0;
	};
	struct FPeriodicStream
	{
		FName Key;
		TWeakObjectPtr<AActor> Target;
		FVector Ground = FVector::ZeroVector;
		float Magnitude = 0.0f;
		float Radius = 0.0f;
		double End = 0.0;
		double NextTick = 0.0;
		bool bDamage = false;
		bool bGround = false;
		bool bMobile = false;
		bool bMercury = false;
		FGuid ItemSource;
		uint64 ItemGeneration = 0;
	};
	UKnightAbilitySystemComponent* ResolveASC() const;
	UKnightAttributeSet* ResolveAttributes() const;
	bool IsAlive() const;
	bool IsFriendly(AActor* Target) const;
	bool HasLineOfSight(AActor* Target) const;
	double ServerTime() const;
	void EnsureInitialized();
	void AdvanceRuntime();
	void PublishState(bool bNotify = true);
	void AddStack(int32 Count, int32 Cap, float Duration, float Interval = 0.0f);
	void AddShield(AActor* Caster, FName Source, float Amount, float Duration, float SourceCap = 0.35f,
		FGuid ItemSource = FGuid());
	void AddPeriodic(FName Key, AActor* Target, float Magnitude, float Duration,
		bool bDamage = false, bool bMercury = false);
	void AddGroundPeriodic(AActor* Target, const FKnightSpecActionDefinition& Definition, float Magnitude);
	float ApplyHeal(AActor* Target, float Magnitude, bool bDirect, bool bCanProcShield = true);
	void CommitSkill(int32 Slot, AActor* Target, FGuid ActivationId);
	void ExecuteSkill(int32 Slot, AActor* Target, FGuid ActivationId, float PowerSnapshot,
		float VersatilitySnapshot, float CritChanceSnapshot, float CritMultiplierSnapshot, float MasterySnapshot,
		int32 ConsumedStacks, bool bEmpoweredHeal, bool bEmpoweredIce, bool bBuffSnapshot);
	TArray<AActor*> FindTargets(const FVector& Center, float Radius, bool bFriendly, AActor* Exclude = nullptr) const;
	void HandleLifeChanged(EKnightLifeState Previous, EKnightLifeState Current);
	void HandleMaxHealthChanged(const FOnAttributeChangeData& Change);
	void NotifyPrimaryDirectHeal(AActor* Target, float ActualHealing, FGuid ExpectedSource, uint64 ExpectedGeneration);
	bool CanTriggerRelic(FName Family, FGuid Source, uint64 Generation) const;
	void RemoveRelicEffects(FGuid Source);
	UFUNCTION()
	void OnRep_State();
	UPROPERTY(ReplicatedUsing = OnRep_State)
	FKnightSkillRuntimeState State;
	UPROPERTY(Transient)
	TArray<FGameplayAbilitySpecHandle> AbilityHandles;
	TArray<FShieldPool> Shields;
	TArray<FDebtTranche> Debts;
	TArray<FPeriodicStream> Periodics;
	TArray<FTimerHandle> ProjectileTimers;
	FTimerHandle RuntimeTimer;
	FDelegateHandle LifeHandle;
	FDelegateHandle MaxHealthHandle;
	FActiveGameplayEffectHandle CrystalEffect;
	FGuid EncounterId;
	double LastSpend = -100.0;
	double LastStack = -100.0;
	double AftershockEnd = 0.0;
	double ShatterEnd = 0.0;
	double CrystalEnd = 0.0;
	bool bInitialized = false;
	FKnightSkillBuild PreparedBuild;
	bool bBuildPrepared = false;
	bool bPreparedCreatedGrants = false;
	TArray<TWeakObjectPtr<UKnightSkillRuntimeComponent>> ChangedShieldOwners;
	FKnightEquipmentCombatBuild PreparedEquipmentBuild;
	bool bEquipmentBuildPrepared = false;
	uint64 ItemGeneration = 0;
	TMap<FGuid, double> SeenItemActivations;
	FGameplayEffectSpecHandle PreparedConsumableSpec;
	FKnightMutationFence PreparedConsumableFence;
	bool bConsumablePreparing = false;
	bool bConsumableCommitted = false;
};
