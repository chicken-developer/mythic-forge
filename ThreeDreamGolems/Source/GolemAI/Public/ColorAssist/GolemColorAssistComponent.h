#pragma once

#include "CoreMinimal.h"
#include "ColorAssist/GolemColorAssistPlanner.h"
#include "Components/ActorComponent.h"
#include "GolemColorAssistComponent.generated.h"

class UGolemColorVoidComponent;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnGolemColorAssistJobChanged, bool /*bActive*/);

/**
 * Lives on a companion's AI controller and carries out what FGolemColorAssistPlanner decides:
 * walk to where the void can be reached, face it, and use the Golem's real heavy attack. The
 * attack is aimed at that one void and may colour nothing else. Host only.
 */
UCLASS(ClassGroup = Golem)
class GOLEMAI_API UGolemColorAssistComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGolemColorAssistComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	void Configure(const FGolemColorAssistTuning& InTuning);
	void SetSeed(int32 Seed);

	/** Higher priorities (reviving a teammate) switch colour help off until cleared. */
	void SetBlocked(bool bInBlocked);

	/** One decision step at host time Now. The tick calls it; tests call it with exact times. */
	void Step(double Now);

	/** Drops the current job and forgets what was seen. */
	void ResetAssist();

	bool HasJob() const { return JobVoid.IsValid(); }
	UGolemColorVoidComponent* GetJobVoid() const { return JobVoid.Get(); }
	EGolemColorAssistReason GetJobReason() const { return JobReason; }
	bool IsSwinging() const { return bSwinging; }

	/** True when a job starts, false when it ends. The controller pauses its ordinary decisions in between. */
	FOnGolemColorAssistJobChanged OnJobChanged;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void BeginJob(UGolemColorVoidComponent* Void, EGolemColorAssistReason Reason, double Now);
	void EndJob();
	void DriveJob(double Now);

	UPROPERTY(EditDefaultsOnly, Category = "Golem|AI|Colour")
	FGolemColorAssistTuning Tuning;

	FGolemColorAssistPlanner Planner;
	TWeakObjectPtr<UGolemColorVoidComponent> JobVoid;
	EGolemColorAssistReason JobReason = EGolemColorAssistReason::None;
	double JobStartedAt = 0.0;
	double NextMoveAt = 0.0;
	double SwingStartedAt = 0.0;
	bool bSwinging = false;
	bool bSawAttack = false;
	bool bBlocked = false;
};
