#pragma once

#include "CoreMinimal.h"
#include "Components/SphereComponent.h"
#include "Voids/GolemVoidTypes.h"
#include "GolemColorVoidComponent.generated.h"

class UGolemColorVoidComponent;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnGolemVoidChanged, UGolemColorVoidComponent*);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnGolemVoidFilled, UGolemColorVoidComponent*, AActor*);

/**
 * One glowing void on a Xenon: a collision sphere plus the colour-mixing state. The host decides
 * every transition; clients only receive State, AppliedMask, RequiredMask and the timer end.
 */
UCLASS(ClassGroup = Golem, meta = (BlueprintSpawnableComponent))
class GOLEMCOMBAT_API UGolemColorVoidComponent : public USphereComponent
{
	GENERATED_BODY()

public:
	UGolemColorVoidComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Authority only. RequiredMask is a GolemElements mask. */
	FGolemResult ConfigureVoid(uint8 InRequiredMask, const FGolemVoidTuning& InTuning, bool bStartHidden);

	/** Authority only. Hidden to Open, or back to Hidden. A Filled void ignores both. */
	bool SetRevealed(bool bRevealed);

	/** Authority only. Applies one colour-carrying hit at the current world time. */
	EGolemVoidHit ApplyElement(EGolemElement Element, float ColorHoldSeconds, AActor* InstigatorActor);

	/** Same as ApplyElement with an explicit host time; tests and replays use it. */
	EGolemVoidHit ApplyElementAt(EGolemElement Element, float ColorHoldSeconds, AActor* InstigatorActor, double Now);

	/** Authority only. Runs the window and lock timers up to Now. */
	void AdvanceTo(double Now);

	/**
	 * Authority only. A player points this void out to the team: the marker lasts
	 * Tuning.SignalSeconds and everyone sees which elements are still missing. Only a void that
	 * can take a hit can be signalled.
	 */
	bool Signal(AActor* InstigatorActor);
	bool SignalAt(AActor* InstigatorActor, double Now);

	/** True while a signal marker is on this void at host time Now. */
	bool IsSignaledAt(double Now) const { return SignalEnd > Now; }
	bool IsSignaled() const { return IsSignaledAt(GetHostTimeSeconds()); }
	double GetSignalStartServerTime() const { return SignalStart; }
	double GetSignalEndServerTime() const { return SignalEnd; }

	/** Host time on the host, and the replicated host clock on clients. */
	double GetHostTimeSeconds() const;

	/** Every void in World that can take a hit right now. */
	static TArray<UGolemColorVoidComponent*> GetHittableVoids(const UWorld* World);

	/**
	 * The hittable void a view ray points at: the one the ray touches, otherwise the one closest
	 * to the ray within MaxAngleDegrees. Used by the signal button, where aim is forgiving.
	 */
	static UGolemColorVoidComponent* FindAimedVoid(const UWorld* World, const FVector& ViewLocation,
		const FVector& ViewDirection, float MaxDistance, float MaxAngleDegrees);

	EGolemVoidState GetVoidState() const { return Runtime.State; }
	uint8 GetRequiredMask() const { return Runtime.RequiredMask; }
	uint8 GetAppliedMask() const { return Runtime.AppliedMask; }
	uint8 GetMissingMask() const { return Runtime.MissingMask(); }
	double GetTimerEndServerTime() const { return Runtime.TimerEnd; }
	bool AcceptsHits() const { return Runtime.AcceptsHits(); }
	const FGolemVoidTuning& GetTuning() const { return Tuning; }

	FOnGolemVoidChanged OnVoidChanged;
	FOnGolemVoidFilled OnVoidFilled;

	/** Authority only: the void and the Golem whose player signalled it. */
	FOnGolemVoidFilled OnVoidSignaled;

private:
	void PublishState();
	double HostNow() const;

	UFUNCTION()
	void OnRep_VoidState();

	UPROPERTY(ReplicatedUsing = OnRep_VoidState)
	EGolemVoidState ReplicatedState = EGolemVoidState::Hidden;

	UPROPERTY(ReplicatedUsing = OnRep_VoidState)
	uint8 ReplicatedRequiredMask = 0;

	UPROPERTY(ReplicatedUsing = OnRep_VoidState)
	uint8 ReplicatedAppliedMask = 0;

	UPROPERTY(ReplicatedUsing = OnRep_VoidState)
	double ReplicatedTimerEnd = 0.0;

	UPROPERTY(ReplicatedUsing = OnRep_VoidState)
	double SignalStart = 0.0;

	UPROPERTY(ReplicatedUsing = OnRep_VoidState)
	double SignalEnd = 0.0;

	UPROPERTY(EditDefaultsOnly, Category = "Golem|Combat|Voids")
	FGolemVoidTuning Tuning;

	FGolemVoidRuntime Runtime;
};
