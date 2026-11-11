#pragma once

#include "CoreMinimal.h"
#include "Context/GolemAIContextComponent.h"
#include "Conditions/StateTreeAIConditionBase.h"
#include "StateTreeAsyncExecutionContext.h"
#include "StateTreeEvaluatorBase.h"
#include "Tasks/StateTreeAITask.h"
#include "AITypes.h"
#include "GolemAIStateTreeNodes.generated.h"

struct FGolemAIActionCompletionState;

USTRUCT()
struct GOLEMAI_API FGolemAIDecisionEvaluatorInstanceData
{
	GENERATED_BODY()
	UPROPERTY(VisibleAnywhere, Category = "Output")
	FGolemAIDecisionSnapshot Snapshot;
};

USTRUCT(meta = (DisplayName = "Golem AI Decision Snapshot", Category = "Golem|AI"))
struct GOLEMAI_API FGolemAIDecisionEvaluator : public FStateTreeEvaluatorCommonBase
{
	GENERATED_BODY()
	using FInstanceDataType = FGolemAIDecisionEvaluatorInstanceData;
	virtual const UStruct* GetInstanceDataType() const override;
	virtual void TreeStart(FStateTreeExecutionContext& Context) const override;
	virtual void TreeStop(FStateTreeExecutionContext& Context) const override;
	virtual void Tick(FStateTreeExecutionContext& Context, float DeltaTime) const override;
};

UENUM()
enum class EGolemAIContextConditionKind : uint8
{
	Alive,
	HasTarget,
	FriendlyDownedTarget,
	CanRevive,
	NeedsRegroup,
	CanSelectAction,
	HoldPosition,
	Staggered,
	HasFormationGoal
};

/** The compiler requires exact reflected instance storage even for a stateless condition. */
USTRUCT()
struct GOLEMAI_API FGolemAIContextConditionInstanceData
{
	GENERATED_BODY()
};

USTRUCT(meta = (DisplayName = "Knight AI Context Condition", Category = "Golem|AI"))
struct GOLEMAI_API FGolemAIContextCondition : public FStateTreeAIConditionBase
{
	GENERATED_BODY()
	using FInstanceDataType = FGolemAIContextConditionInstanceData;
	virtual const UStruct* GetInstanceDataType() const override;
	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;
	UPROPERTY(EditAnywhere, Category = "Condition")
	EGolemAIContextConditionKind Kind = EGolemAIContextConditionKind::Alive;
	UPROPERTY(EditAnywhere, Category = "Condition")
	bool bDesiredValue = true;
};

USTRUCT()
struct GOLEMAI_API FGolemAISelectTargetInstanceData
{
	GENERATED_BODY()
	UPROPERTY(VisibleAnywhere, Category = "Output")
	FGolemEntityId SelectedTargetId;
};

USTRUCT(meta = (DisplayName = "Knight AI Acquire Target", Category = "Golem|AI"))
struct GOLEMAI_API FGolemAISelectTargetTask : public FStateTreeAITaskBase
{
	GENERATED_BODY()
	using FInstanceDataType = FGolemAISelectTargetInstanceData;
	virtual const UStruct* GetInstanceDataType() const override;
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context,
		const FStateTreeTransitionResult& Transition) const override;
};

USTRUCT()
struct GOLEMAI_API FGolemAIExecuteActionInstanceData
{
	GENERATED_BODY()
	UPROPERTY()
	TWeakObjectPtr<UGolemAIContextComponent> DecisionContext;
	FGuid ActionToken;
	FDelegateHandle CompletionHandle;
	EGolemAIActionStatus CompletionStatus = EGolemAIActionStatus::Unknown;
	bool bEntering = false;
	TSharedPtr<FGolemAIActionCompletionState> CompletionState;
};

USTRUCT(meta = (DisplayName = "Knight AI Execute Shared GAS Action", Category = "Golem|AI"))
struct GOLEMAI_API FGolemAIExecuteActionTask : public FStateTreeAIActionTaskBase
{
	GENERATED_BODY()
	using FInstanceDataType = FGolemAIExecuteActionInstanceData;
	FGolemAIExecuteActionTask();
	virtual const UStruct* GetInstanceDataType() const override;
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context,
		const FStateTreeTransitionResult& Transition) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context,
		const FStateTreeTransitionResult& Transition) const override;
	UPROPERTY(EditAnywhere, Category = "Action")
	bool bRevive = false;
	UPROPERTY(EditAnywhere, Category = "Action")
	bool bUseAttackLease = true;
};

USTRUCT()
struct GOLEMAI_API FGolemAIRepositionInstanceData
{
	GENERATED_BODY()
	UPROPERTY()
	TWeakObjectPtr<UGolemAIContextComponent> DecisionContext;
	UPROPERTY()
	TWeakObjectPtr<AAIController> MoveController;
	int32 QueryId = INDEX_NONE;
	FGuid RequestToken;
	FGolemEntityId TargetId;
	FAIRequestID MoveRequest;
	FDelegateHandle MoveHandle;
	FVector Destination = FVector::ZeroVector;
	bool bCompleted = false;
	bool bSucceeded = false;
	double StartedAt = 0.0;
};

USTRUCT(meta = (DisplayName = "Knight AI EQS Reposition", Category = "Golem|AI"))
struct GOLEMAI_API FGolemAIRepositionTask : public FStateTreeAIActionTaskBase
{
	GENERATED_BODY()
	using FInstanceDataType = FGolemAIRepositionInstanceData;
	virtual const UStruct* GetInstanceDataType() const override;
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context,
		const FStateTreeTransitionResult& Transition) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context,
		const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, float DeltaTime) const override;
	UPROPERTY(EditAnywhere, Category = "Movement", meta = (Units = "cm", ClampMin = "1"))
	float AcceptanceRadius = 15.0f;
	/** Revive approaches use the target's authored revive radius instead of a combat-position EQS. */
	UPROPERTY(EditAnywhere, Category = "Movement")
	bool bMoveToReviveTarget = false;
	UPROPERTY(EditAnywhere, Category = "Movement", meta = (Units = "s", ClampMin = "0.1"))
	float Timeout = 6.0f;
};

USTRUCT()
struct GOLEMAI_API FGolemAIFormationInstanceData
{
	GENERATED_BODY()
	float ElapsedSinceRequest = 0.0f;
	FAIRequestID MoveRequest;
};

USTRUCT(meta = (DisplayName = "Knight AI Follow Or Regroup", Category = "Golem|AI"))
struct GOLEMAI_API FGolemAIFormationTask : public FStateTreeAIActionTaskBase
{
	GENERATED_BODY()
	using FInstanceDataType = FGolemAIFormationInstanceData;
	virtual const UStruct* GetInstanceDataType() const override;
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context,
		const FStateTreeTransitionResult& Transition) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context,
		const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, float DeltaTime) const override;
	UPROPERTY(EditAnywhere, Category = "Movement")
	bool bCompleteWhenReached = false;
	UPROPERTY(EditAnywhere, Category = "Movement", meta = (Units = "s", ClampMin = "0.1"))
	float RepathInterval = 0.35f;
};

USTRUCT()
struct GOLEMAI_API FGolemAIWaitInstanceData
{
	GENERATED_BODY()
	float Elapsed = 0.0f;
};

USTRUCT(meta = (DisplayName = "Knight AI Bounded Wait", Category = "Golem|AI"))
struct GOLEMAI_API FGolemAIWaitTask : public FStateTreeAITaskBase
{
	GENERATED_BODY()
	using FInstanceDataType = FGolemAIWaitInstanceData;
	virtual const UStruct* GetInstanceDataType() const override;
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context,
		const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, float DeltaTime) const override;
	UPROPERTY(EditAnywhere, Category = "Wait", meta = (Units = "s", ClampMin = "0.05"))
	float Duration = 0.35f;
};
