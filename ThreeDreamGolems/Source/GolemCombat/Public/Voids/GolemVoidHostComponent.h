#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Damage/GolemDamageResponse.h"
#include "Voids/GolemVoidTypes.h"
#include "GolemVoidHostComponent.generated.h"

class UGolemAbilitySystemComponent;
class UGolemColorVoidComponent;
enum class EGolemLifeState : uint8;

/** Per-Xenon numbers from DEV_SPEC sections 5 and 6. All of them are starting values. */
USTRUCT(BlueprintType)
struct GOLEMCOMBAT_API FGolemVoidHostTuning
{
	GENERATED_BODY()

	/** Ordinary voids that must be filled before the Xenon is restored. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Combat|Voids", meta = (ClampMin = "1"))
	int32 VoidsToFill = 3;

	/** How many of them are revealed at the same time. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Combat|Voids", meta = (ClampMin = "1"))
	int32 MaxOpenVoids = 1;

	/** Guardians and Xeros: a three-element core appears once every ordinary void is filled. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Combat|Voids")
	bool bHasWhiteCore = false;

	/** Seconds the Xenon is stunned after a fill; the next void appears when it ends. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Combat|Voids", meta = (ClampMin = "0.0", Units = "s"))
	float StaggerSeconds = 2.5f;

	/** Share of damage the grey body takes from anything that is not a fill. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Combat|Voids", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float GrayBodyDamageFraction = 0.1f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Combat|Voids")
	FGolemVoidTuning Void;

	int32 TotalFills() const
	{
		return VoidsToFill + (bHasWhiteCore ? 1 : 0);
	}

	FGolemResult Validate() const;
};

DECLARE_MULTICAST_DELEGATE(FOnGolemVoidHostChanged);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnGolemXenonRestored, AActor*);

/**
 * Runs the voids of one Xenon: reveals them one after another, turns each fill into a slice of
 * health and a stagger, scales every other hit down to the grey-body share, and restores the Xenon
 * when the last void is filled or its health reaches zero. Authority only; clients read the state.
 */
UCLASS(ClassGroup = Golem, meta = (BlueprintSpawnableComponent))
class GOLEMCOMBAT_API UGolemVoidHostComponent : public UActorComponent, public IGolemDamageResponse
{
	GENERATED_BODY()

public:
	UGolemVoidHostComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/**
	 * ColorPlan holds one element mask per ordinary fill, in the order they appear; it must have
	 * VoidsToFill entries. Slots are reused: a filled slot becomes available again for a later void.
	 */
	FGolemResult InitializeHost(UGolemAbilitySystemComponent* InAbilitySystem, const FGolemVoidHostTuning& InTuning,
		const TArray<UGolemColorVoidComponent*>& InSlots, const TArray<uint8>& InColorPlan,
		UGolemColorVoidComponent* InCoreSlot = nullptr);

	/** Runs stagger and reveal timers, and the timers of every void, up to Now. */
	void AdvanceTo(double Now);

	virtual void ResolveIncomingDamage(AActor* Source, float InDamage, float PoiseDamage,
		bool bCanDefend, float& OutHealthDamage) override;

	int32 GetFilledCount() const { return FilledCount; }
	int32 GetTotalFills() const { return Tuning.TotalFills(); }
	bool IsRestored() const { return bRestored; }
	bool IsStaggered() const { return StaggerEndServerTime > 0.0; }
	double GetStaggerEndServerTime() const { return StaggerEndServerTime; }
	bool IsWhiteCoreRevealed() const;
	const FGolemVoidHostTuning& GetTuning() const { return Tuning; }
	const TArray<TObjectPtr<UGolemColorVoidComponent>>& GetSlots() const { return Slots; }
	UGolemColorVoidComponent* GetCoreSlot() const { return CoreSlot; }

	/** Voids that can take a hit right now. */
	TArray<UGolemColorVoidComponent*> GetOpenVoids() const;

	FOnGolemVoidHostChanged OnHostChanged;
	FOnGolemXenonRestored OnRestored;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void HandleVoidFilled(UGolemColorVoidComponent* Void, AActor* InstigatorActor);
	void HandleLifeChanged(EGolemLifeState Previous, EGolemLifeState Current);
	void RevealUpToCapacity();
	void Restore(AActor* InstigatorActor);
	void EndStagger();
	int32 CountActiveSlots() const;
	double HostNow() const;

	UFUNCTION()
	void OnRep_HostState();

	UPROPERTY(ReplicatedUsing = OnRep_HostState)
	int32 FilledCount = 0;

	UPROPERTY(ReplicatedUsing = OnRep_HostState)
	bool bRestored = false;

	UPROPERTY(ReplicatedUsing = OnRep_HostState)
	double StaggerEndServerTime = 0.0;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UGolemColorVoidComponent>> Slots;

	UPROPERTY(Transient)
	TObjectPtr<UGolemColorVoidComponent> CoreSlot;

	UPROPERTY(Transient)
	TWeakObjectPtr<UGolemAbilitySystemComponent> AbilitySystem;

	UPROPERTY(EditDefaultsOnly, Category = "Golem|Combat|Voids")
	FGolemVoidHostTuning Tuning;

	TArray<uint8> ColorPlan;
	FDelegateHandle LifeHandle;
	int32 NextPlanIndex = 0;
	int32 LastFilledSlot = INDEX_NONE;
	double LastTime = 0.0;
	bool bInitialized = false;
	bool bRestoring = false;
};
