#pragma once

#include "CoreMinimal.h"
#include "Context/KnightAIContextComponent.h"
#include "Conditions/StateTreeAIConditionBase.h"
#include "StateTreeAsyncExecutionContext.h"
#include "StateTreeEvaluatorBase.h"
#include "Tasks/StateTreeAITask.h"
#include "AITypes.h"
#include "KnightAIStateTreeNodes.generated.h"

struct FKnightAIActionCompletionState;

USTRUCT()
struct KNIGHTAI_API FKnightAIDecisionEvaluatorInstanceData
{
	GENERATED_BODY()
	UPROPERTY(VisibleAnywhere, Category = "Output")
	FKnightAIDecisionSnapshot Snapshot;
};

USTRUCT(meta = (DisplayName = "Knight AI Decision Snapshot", Category = "Knight|AI"))
struct KNIGHTAI_API FKnightAIDecisionEvaluator : public FStateTreeEvaluatorCommonBase
{
	GENERATED_BODY()
	using FInstanceDataType = FKnightAIDecisionEvaluatorInstanceData;
	virtual const UStruct* GetInstanceDataType() const override;
	virtual void TreeStart(FStateTreeExecutionContext& Context) const override;
	virtual void TreeStop(FStateTreeExecutionContext& Context) const override;
	virtual void Tick(FStateTreeExecutionContext& Context, float DeltaTime) const override;
};

UENUM()
enum class EKnightAIContextConditionKind : uint8
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
struct KNIGHTAI_API FKnightAIContextConditionInstanceData
{
	GENERATED_BODY()
};

USTRUCT(meta = (DisplayName = "Knight AI Context Condition", Category = "Knight|AI"))
struct KNIGHTAI_API FKnightAIContextCondition : public FStateTreeAIConditionBase
{
	GENERATED_BODY()
	using FInstanceDataType = FKnightAIContextConditionInstanceData;
	virtual const UStruct* GetInstanceDataType() const override;
	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;
	UPROPERTY(EditAnywhere, Category = "Condition")
	EKnightAIContextConditionKind Kind = EKnightAIContextConditionKind::Alive;
	UPROPERTY(EditAnywhere, Category = "Condition")
	bool bDesiredValue = true;
};

USTRUCT()
struct KNIGHTAI_API FKnightAISelectTargetInstanceData
{
	GENERATED_BODY()
	UPROPERTY(VisibleAnywhere, Category = "Output")
	FKnightEntityId SelectedTargetId;
};

USTRUCT(meta = (DisplayName = "Knight AI Acquire Target", Category = "Knight|AI"))
struct KNIGHTAI_API FKnightAISelectTargetTask : public FStateTreeAITaskBase
{
	GENERATED_BODY()
	using FInstanceDataType = FKnightAISelectTargetInstanceData;
	virtual const UStruct* GetInstanceDataType() const override;
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context,
		const FStateTreeTransitionResult& Transition) const override;
};

USTRUCT()
struct KNIGHTAI_API FKnightAIExecuteActionInstanceData
{
	GENERATED_BODY()
	UPROPERTY()
	TWeakObjectPtr<UKnightAIContextComponent> DecisionContext;
	FGuid ActionToken;
	FDelegateHandle CompletionHandle;
	EKnightAIActionStatus CompletionStatus = EKnightAIActionStatus::Unknown;
	bool bEntering = false;
	TSharedPtr<FKnightAIActionCompletionState> CompletionState;
};

USTRUCT(meta = (DisplayName = "Knight AI Execute Shared GAS Action", Category = "Knight|AI"))
struct KNIGHTAI_API FKnightAIExecuteActionTask : public FStateTreeAIActionTaskBase
{
	GENERATED_BODY()
	using FInstanceDataType = FKnightAIExecuteActionInstanceData;
	FKnightAIExecuteActionTask();
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
struct KNIGHTAI_API FKnightAIRepositionInstanceData
{
	GENERATED_BODY()
	UPROPERTY()
	TWeakObjectPtr<UKnightAIContextComponent> DecisionContext;
	UPROPERTY()
	TWeakObjectPtr<AAIController> MoveController;
	int32 QueryId = INDEX_NONE;
	FGuid RequestToken;
	FKnightEntityId TargetId;
	FAIRequestID MoveRequest;
	FDelegateHandle MoveHandle;
	FVector Destination = FVector::ZeroVector;
	bool bCompleted = false;
	bool bSucceeded = false;
	double StartedAt = 0.0;
};

USTRUCT(meta = (DisplayName = "Knight AI EQS Reposition", Category = "Knight|AI"))
struct KNIGHTAI_API FKnightAIRepositionTask : public FStateTreeAIActionTaskBase
{
	GENERATED_BODY()
	using FInstanceDataType = FKnightAIRepositionInstanceData;
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
struct KNIGHTAI_API FKnightAIFormationInstanceData
{
	GENERATED_BODY()
	float ElapsedSinceRequest = 0.0f;
	FAIRequestID MoveRequest;
};

USTRUCT(meta = (DisplayName = "Knight AI Follow Or Regroup", Category = "Knight|AI"))
struct KNIGHTAI_API FKnightAIFormationTask : public FStateTreeAIActionTaskBase
{
	GENERATED_BODY()
	using FInstanceDataType = FKnightAIFormationInstanceData;
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
struct KNIGHTAI_API FKnightAIWaitInstanceData
{
	GENERATED_BODY()
	float Elapsed = 0.0f;
};

USTRUCT(meta = (DisplayName = "Knight AI Bounded Wait", Category = "Knight|AI"))
struct KNIGHTAI_API FKnightAIWaitTask : public FStateTreeAITaskBase
{
	GENERATED_BODY()
	using FInstanceDataType = FKnightAIWaitInstanceData;
	virtual const UStruct* GetInstanceDataType() const override;
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context,
		const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, float DeltaTime) const override;
	UPROPERTY(EditAnywhere, Category = "Wait", meta = (Units = "s", ClampMin = "0.05"))
	float Duration = 0.35f;
};
