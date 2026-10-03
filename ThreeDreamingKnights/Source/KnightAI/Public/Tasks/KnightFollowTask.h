#pragma once

#include "CoreMinimal.h"
#include "Tasks/StateTreeAITask.h"
#include "KnightFollowTask.generated.h"

USTRUCT()
struct KNIGHTAI_API FKnightFollowTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY()
	float ElapsedSinceRequest = 0.0f;
};

/** Keeps an AI-owned Knight in its authored formation without teleporting or replacing the pawn. */
USTRUCT(meta = (DisplayName = "Knight Follow Formation", Category = "Knight|AI"))
struct KNIGHTAI_API FKnightFollowTask : public FStateTreeAITaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FKnightFollowTaskInstanceData;

	virtual const UStruct* GetInstanceDataType() const override;
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context,
		const FStateTreeTransitionResult& Transition) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context,
		const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, float DeltaTime) const override;

	UPROPERTY(EditAnywhere, Category = "Knight|AI|Follow", meta = (ClampMin = "0.1", Units = "s"))
	float RepathInterval = 0.35f;
};
