#include "Tasks/GolemFollowTask.h"

#include "Character/GolemCharacter.h"
#include "Controllers/GolemCompanionController.h"

#include "StateTreeExecutionContext.h"

const UStruct* FGolemFollowTask::GetInstanceDataType() const
{
	return FInstanceDataType::StaticStruct();
}

EStateTreeRunStatus FGolemFollowTask::EnterState(FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.ElapsedSinceRequest = RepathInterval;
	return Tick(Context, 0.0f);
}

void FGolemFollowTask::ExitState(FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	UObject* Owner = Context.GetOwner();
	if (AGolemCompanionController* Controller = Cast<AGolemCompanionController>(Owner))
	{
		Controller->StopMovement();
	}
}

EStateTreeRunStatus FGolemFollowTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	UObject* Owner = Context.GetOwner();
	AGolemCompanionController* Controller = Cast<AGolemCompanionController>(Owner);
	const AGolemCharacter* Golem = Controller != nullptr ? Cast<AGolemCharacter>(Controller->GetPawn()) : nullptr;
	if (Golem == nullptr || !Golem->CanTransferControl())
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
	if (FVector::DistSquared2D(Golem->GetActorLocation(), Goal) <= FMath::Square(AcceptanceRadius))
	{
		Controller->StopMovement();
	}
	else
	{
		Controller->TryMoveToFormation(Goal, AcceptanceRadius);
	}
	return EStateTreeRunStatus::Running;
}
