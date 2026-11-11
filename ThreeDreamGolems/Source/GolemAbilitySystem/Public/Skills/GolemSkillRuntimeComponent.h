#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayAbilitySpec.h"
#include "GameplayEffectTypes.h"
#include "Projection/GolemSourceProjection.h"
#include "Skills/GolemSkillTypes.h"
#include "Skills/GolemEquipmentCombatTypes.h"
#include "GolemSkillRuntimeComponent.generated.h"

class UGolemAbilitySystemComponent;
class UGolemAttributeSet;
class UGolemSpecializationAbility;
enum class EGolemLifeState : uint8;
struct FOnAttributeChangeData;

USTRUCT(BlueprintType)
struct GOLEMABILITYSYSTEM_API FGolemSkillRuntimeState
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Golem|Skills")
	FGolemSkillBuild Build;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Golem|Skills")
	TArray<double> CooldownEnds;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Golem|Skills")
	int32 Stacks = 0;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Golem|Skills")
	double StackEnd = 0.0;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Golem|Skills")
	double BuffEnd = 0.0;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Golem|Skills")
	float Shield = 0.0f;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Golem|Skills")
	float DeferredDamage = 0.0f;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Golem|Skills")
	int32 Chilled = 0;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Golem|Skills")
	double ChilledEnd = 0.0;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Golem|Skills")
	double WaterReadyEnd = 0.0;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Golem|Skills")
	double IceReadyEnd = 0.0;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Golem|Skills")
	int32 Revision = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Golem|Equipment")
	FGolemEquipmentCombatBuild EquipmentCombatBuild;
	UPROPERTY(BlueprintReadOnly, Category = "Golem|Equipment")
	double BulwarkEnd = 0.0;
	UPROPERTY(BlueprintReadOnly, Category = "Golem|Equipment")
	double LifeseedEnd = 0.0;
	UPROPERTY(BlueprintReadOnly, Category = "Golem|Equipment")
	double StarfallEnd = 0.0;
	UPROPERTY(BlueprintReadOnly, Category = "Golem|Equipment")
	int32 StarfallCount = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Golem|Equipment")
	double StarfallCounterEnd = 0.0;
	UPROPERTY(BlueprintReadOnly, Category = "Golem|Equipment")
	double ConsumableEnd = 0.0;
	UPROPERTY(BlueprintReadOnly, Category = "Golem|Equipment")
	double CrystalEnd = 0.0;
};

/** In-memory reconstruction fragment. Cooldown time is remaining, independent of a world's clock. */
USTRUCT()
struct GOLEMABILITYSYSTEM_API FGolemSkillRuntimeSnapshot
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

DECLARE_MULTICAST_DELEGATE_OneParam(FOnGolemSkillStateChanged, const FGolemSkillRuntimeState&);

/** Per-character runtime. GAS owns activation; this component owns source pools and timed discrete state. */
UCLASS(ClassGroup = Golem, meta = (BlueprintSpawnableComponent))
class GOLEMABILITYSYSTEM_API UGolemSkillRuntimeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGolemSkillRuntimeComponent();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	FGolemResult ConfigureBuild(const FGolemSkillBuild& Build);
	FGolemResult PrepareBuild(const FGolemSkillBuild& Build);
	void CommitPreparedBuild();
	void AbortPreparedBuild();
	void PublishPreparedBuild(bool bNotifySubscribers = true);
	FGolemResult PrepareEquipmentBuild(const FGolemEquipmentCombatBuild& Build);
	void CommitPreparedEquipmentBuild();
	void AbortPreparedEquipmentBuild();
	bool RequiresHammer() const;
	float GetPowerCrystalMultiplier() const;
	FGolemResult ValidateConsumable(FName EffectId) const;
	FGolemResult PrepareConsumable(FName EffectId, const FGolemMutationFence& Fence);
	void CommitPreparedConsumable();
	void AbortPreparedConsumable();
	void PublishPreparedConsumable();
	uint64 GetItemGeneration() const;
	void NotifyItemDefenseSuccess(AActor* Source, FGuid ActivationId, bool bDirect, uint64 ExpectedGeneration);
	void NotifyConfirmedDirectDamage(AActor* Target, FGuid ActivationId, int32 HitIndex,
		EGolemSkillSchool School, float Damage, AActor* PrimaryTarget, uint64 ExpectedGeneration);
	bool HasUnsnapshottedEffects() const;
	bool HasActiveSkill() const;
	FGolemResult RequestSkill(int32 Slot, AActor* Target = nullptr);
	FGolemResult ValidateSkill(int32 Slot, AActor* Target) const;
	FGolemSkillRuntimeState GetSkillState() const;
	FGolemSkillBuild GetBuild() const;
	FGolemSkillRuntimeSnapshot CaptureSnapshot() const;
	static FGolemResult ValidateSnapshot(const FGolemSkillRuntimeSnapshot& Snapshot);
	FGolemResult RestoreSnapshot(const FGolemSkillRuntimeSnapshot& Snapshot, bool bNotifySubscribers = true);
	float GetDeferredDamage() const;
	bool HasDeferredDamage() const;
	bool IsSkillAvailable(int32 Slot) const;
	FGameplayAbilitySpecHandle GetSkillHandle(int32 Slot) const;
	AActor* SelectLowestHealthAlly(float Radius, AActor* Around = nullptr, AActor* Exclude = nullptr) const;
	void SetEncounterId(FGuid InEncounterId);
	FGolemResult AddPowerCrystalBuff();
	FGolemResult ApplyExternalHeal(float Magnitude);
	void NotifyDefenseSuccess();
	void NotifyHeavyHit(AActor* Target);
	float GetIncomingReduction(AActor* Source) const;
	float GetPoiseScale() const;
	float ResolveDamagePools(float Damage, bool bEligibleDeferred);
	void ResetForCheckpoint();
	static FGolemResult ApplySkillDamage(AActor* Source, AActor* Target, const FGolemSkillDamagePacket& Packet,
		const FHitResult* Hit = nullptr);
	FOnGolemSkillStateChanged OnSkillStateChanged;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;

private:
	friend class UGolemSpecializationAbility;
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
	UGolemAbilitySystemComponent* ResolveASC() const;
	UGolemAttributeSet* ResolveAttributes() const;
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
	void AddGroundPeriodic(AActor* Target, const FGolemSpecActionDefinition& Definition, float Magnitude);
	float ApplyHeal(AActor* Target, float Magnitude, bool bDirect, bool bCanProcShield = true);
	void CommitSkill(int32 Slot, AActor* Target, FGuid ActivationId);
	void ExecuteSkill(int32 Slot, AActor* Target, FGuid ActivationId, float PowerSnapshot,
		float VersatilitySnapshot, float CritChanceSnapshot, float CritMultiplierSnapshot, float MasterySnapshot,
		int32 ConsumedStacks, bool bEmpoweredHeal, bool bEmpoweredIce, bool bBuffSnapshot);
	TArray<AActor*> FindTargets(const FVector& Center, float Radius, bool bFriendly, AActor* Exclude = nullptr) const;
	void HandleLifeChanged(EGolemLifeState Previous, EGolemLifeState Current);
	void HandleMaxHealthChanged(const FOnAttributeChangeData& Change);
	void NotifyPrimaryDirectHeal(AActor* Target, float ActualHealing, FGuid ExpectedSource, uint64 ExpectedGeneration);
	bool CanTriggerRelic(FName Family, FGuid Source, uint64 Generation) const;
	void RemoveRelicEffects(FGuid Source);
	UFUNCTION()
	void OnRep_State();
	UPROPERTY(ReplicatedUsing = OnRep_State)
	FGolemSkillRuntimeState State;
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
	FGolemSkillBuild PreparedBuild;
	bool bBuildPrepared = false;
	bool bPreparedCreatedGrants = false;
	TArray<TWeakObjectPtr<UGolemSkillRuntimeComponent>> ChangedShieldOwners;
	FGolemEquipmentCombatBuild PreparedEquipmentBuild;
	bool bEquipmentBuildPrepared = false;
	uint64 ItemGeneration = 0;
	TMap<FGuid, double> SeenItemActivations;
	FGameplayEffectSpecHandle PreparedConsumableSpec;
	FGolemMutationFence PreparedConsumableFence;
	bool bConsumablePreparing = false;
	bool bConsumableCommitted = false;
};
