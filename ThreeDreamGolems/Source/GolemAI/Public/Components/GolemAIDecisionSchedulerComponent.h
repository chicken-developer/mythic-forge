#pragma once

#include "CoreMinimal.h"
#include "Commands/GolemCompanionCommand.h"
#include "Components/ActorComponent.h"
#include "Engine/TimerHandle.h"
#include "Types/GolemResult.h"
#include "GolemAIDecisionSchedulerComponent.generated.h"

class UGolemAIContextComponent;
class UStateTreeAIComponent;

DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnGolemAIDecisionScheduled, EGolemAILODTier, float, int64);

/** Owns decision cadence only. CharacterMovement, montages and GAS tasks retain their normal runtime cadence. */
UCLASS(ClassGroup = Golem)
class GOLEMAI_API UGolemAIDecisionSchedulerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGolemAIDecisionSchedulerComponent();
	FGolemResult StartScheduling(UStateTreeAIComponent* InTree, UGolemAIContextComponent* InContext);
	void StopScheduling();
	void RequestDecisionWake();
	EGolemAILODTier GetDecisionLOD() const;
	float GetDecisionInterval() const;
	int64 GetDecisionTickCount() const;
	bool HasDecisionTimer() const;
	FOnGolemAIDecisionScheduled OnDecisionScheduled;

protected:
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;

private:
	void RunDecision();
	void ScheduleNextDecision(float Delay);
	EGolemAILODTier CalculateLOD() const;
	static float GetIntervalForLOD(EGolemAILODTier Tier);
	TWeakObjectPtr<UStateTreeAIComponent> Tree;
	TWeakObjectPtr<UGolemAIContextComponent> Context;
	FTimerHandle DecisionTimer;
	EGolemAILODTier DecisionLOD = EGolemAILODTier::Near;
	float DecisionInterval = 0.1f;
	double PreviousDecisionAt = 0.0;
	int64 DecisionTickCount = 0;
	uint64 ScheduleGeneration = 0;
	bool bEndingPlay = false;
};
