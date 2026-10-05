#pragma once

#include "CoreMinimal.h"
#include "Commands/KnightCompanionCommand.h"
#include "Components/ActorComponent.h"
#include "Engine/TimerHandle.h"
#include "Types/KnightResult.h"
#include "KnightAIDecisionSchedulerComponent.generated.h"

class UKnightAIContextComponent;
class UStateTreeAIComponent;

DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnKnightAIDecisionScheduled, EKnightAILODTier, float, int64);

/** Owns decision cadence only. CharacterMovement, montages and GAS tasks retain their normal runtime cadence. */
UCLASS(ClassGroup = Knight)
class KNIGHTAI_API UKnightAIDecisionSchedulerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UKnightAIDecisionSchedulerComponent();
	FKnightResult StartScheduling(UStateTreeAIComponent* InTree, UKnightAIContextComponent* InContext);
	void StopScheduling();
	void RequestDecisionWake();
	EKnightAILODTier GetDecisionLOD() const;
	float GetDecisionInterval() const;
	int64 GetDecisionTickCount() const;
	bool HasDecisionTimer() const;
	FOnKnightAIDecisionScheduled OnDecisionScheduled;

protected:
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;

private:
	void RunDecision();
	void ScheduleNextDecision(float Delay);
	EKnightAILODTier CalculateLOD() const;
	static float GetIntervalForLOD(EKnightAILODTier Tier);
	TWeakObjectPtr<UStateTreeAIComponent> Tree;
	TWeakObjectPtr<UKnightAIContextComponent> Context;
	FTimerHandle DecisionTimer;
	EKnightAILODTier DecisionLOD = EKnightAILODTier::Near;
	float DecisionInterval = 0.1f;
	double PreviousDecisionAt = 0.0;
	int64 DecisionTickCount = 0;
	uint64 ScheduleGeneration = 0;
	bool bEndingPlay = false;
};
