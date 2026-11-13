#pragma once

#include "CoreMinimal.h"
#include "Tasks/StateTreeAITask.h"
#include "GolemFollowTask.generated.h"

USTRUCT()
struct GOLEMAI_API FGolemFollowTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY()
	float ElapsedSinceRequest = 0.0f;
};

/** Keeps an AI-owned Golem in its authored formation without teleporting or replacing the pawn. */
USTRUCT(meta = (DisplayName = "Golem Follow Formation", Category = "Golem|AI"))
struct GOLEMAI_API FGolemFollowTask : public FStateTreeAITaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FGolemFollowTaskInstanceData;

	virtual const UStruct* GetInstanceDataType() const override;
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context,
		const FStateTreeTransitionResult& Transition) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context,
		const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, float DeltaTime) const override;

	UPROPERTY(EditAnywhere, Category = "Golem|AI|Follow", meta = (ClampMin = "0.1", Units = "s"))
	float RepathInterval = 0.35f;
};
