#include "Tasks/KnightFollowTask.h"

#include "Character/KnightCharacter.h"
#include "Controllers/KnightCompanionController.h"

#include "StateTreeExecutionContext.h"

const UStruct* FKnightFollowTask::GetInstanceDataType() const
{
	return FInstanceDataType::StaticStruct();
}

EStateTreeRunStatus FKnightFollowTask::EnterState(FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.ElapsedSinceRequest = RepathInterval;
	return Tick(Context, 0.0f);
}

void FKnightFollowTask::ExitState(FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	UObject* Owner = Context.GetOwner();
	if (AKnightCompanionController* Controller = Cast<AKnightCompanionController>(Owner))
	{
		Controller->StopMovement();
	}
}

EStateTreeRunStatus FKnightFollowTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	UObject* Owner = Context.GetOwner();
	AKnightCompanionController* Controller = Cast<AKnightCompanionController>(Owner);
	const AKnightCharacter* Knight = Controller != nullptr ? Cast<AKnightCharacter>(Controller->GetPawn()) : nullptr;
	if (Knight == nullptr || !Knight->CanTransferControl())
	{
		return EStateTreeRunStatus::Failed;
	}

	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.ElapsedSinceRequest += DeltaTime;
	if (InstanceData.ElapsedSinceRequest < RepathInterval)
	{
		return EStateTreeRunStatus::Running;
	}
	InstanceData.ElapsedSinceRequest = 0.0f;

	FVector Goal;
	float AcceptanceRadius = 0.0f;
	if (!Controller->TryGetFormationGoal(Goal, AcceptanceRadius))
	{
		Controller->StopMovement();
		return EStateTreeRunStatus::Running;
	}
	if (FVector::DistSquared2D(Knight->GetActorLocation(), Goal) <= FMath::Square(AcceptanceRadius))
	{
		Controller->StopMovement();
	}
	else
	{
		Controller->TryMoveToFormation(Goal, AcceptanceRadius);
	}
	return EStateTreeRunStatus::Running;
}
