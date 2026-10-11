#include "StateTree/KnightAIStateTreeNodes.h"

#include "AIController.h"
#include "AbilitySystemGlobals.h"
#include "Components/KnightAbilitySystemComponent.h"
#include "Components/KnightLifeComponent.h"
#include "Engine/World.h"
#include "EnvironmentQuery/EnvQuery.h"
#include "EnvironmentQuery/EnvQueryManager.h"
#include "GameFramework/Pawn.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationSystem.h"
#include "StateTreeExecutionContext.h"
#include "TimerManager.h"

/** Completion is deferred out of the tree's write scope, including instant GAS end during EnterState. */
struct FKnightAIActionCompletionState
{
	FStateTreeWeakExecutionContext WeakContext;
	TWeakObjectPtr<UWorld> World;
	FTimerHandle DeferredCompletion;
	bool bCancelled = false;
};

namespace KnightAIStateTree
{
	UKnightAIContextComponent* ResolveContext(const FStateTreeExecutionContext& Context)
	{
		AActor* Owner = Cast<AActor>(Context.GetOwner());
		return Owner != nullptr ? Owner->FindComponentByClass<UKnightAIContextComponent>() : nullptr;
	}

	EStateTreeRunStatus ToRunStatus(EKnightAIActionStatus Status)
	{
		return Status == EKnightAIActionStatus::Succeeded ? EStateTreeRunStatus::Succeeded
			: Status == EKnightAIActionStatus::Running ? EStateTreeRunStatus::Running : EStateTreeRunStatus::Failed;
	}

	bool CanRevive(const FKnightAIDecisionSnapshot& Snapshot, const UKnightAIContextComponent* Decision)
	{
		UKnightAbilitySystemComponent* TargetASC = Snapshot.bTargetFriendlyDowned && Snapshot.Target.IsValid()
			? Cast<UKnightAbilitySystemComponent>(UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(
				Snapshot.Target.Get(), true)) : nullptr;
		return Decision != nullptr && Snapshot.bAlive && !Snapshot.bStaggered && TargetASC != nullptr &&
			TargetASC->GetLifeComponent() != nullptr && TargetASC->GetLifeComponent()->
				ValidateReviveSource(Decision->GetControlledPawn()).IsSuccess();
	}

	void CompleteReposition(FStateTreeStrongExecutionContext& Strong, FKnightAIRepositionInstanceData& Data,
		bool bSucceeded)
	{
		Data.bCompleted = true;
		Data.bSucceeded = bSucceeded;
		Strong.FinishTask(bSucceeded ? EStateTreeFinishTaskType::Succeeded : EStateTreeFinishTaskType::Failed);
	}

	void BeginRepositionMove(FStateTreeStrongExecutionContext& Strong,
		const FStateTreeWeakExecutionContext& Weak, const FGuid& Token, FVector Goal, float Acceptance)
	{
		FKnightAIRepositionInstanceData* Data = Strong.GetInstanceDataPtr<FKnightAIRepositionInstanceData>();
		if (Data == nullptr || Data->RequestToken != Token || !Data->DecisionContext.IsValid())
		{
			return;
		}
		UKnightAIContextComponent* Decision = Data->DecisionContext.Get();
		APawn* Pawn = Decision->GetControlledPawn();
		AAIController* Controller = Pawn != nullptr ? Cast<AAIController>(Pawn->GetController()) : nullptr;
		const FKnightAIDecisionSnapshot Snapshot = Decision->GetDecisionSnapshot();
		UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(Decision->GetWorld());
		FNavLocation Projected;
		if (!Snapshot.bAlive || !Snapshot.bHasTarget || Snapshot.TargetId != Data->TargetId || Controller == nullptr ||
			Navigation == nullptr || Goal.ContainsNaN() || !Navigation->ProjectPointToNavigation(Goal, Projected,
				FVector(150.0, 150.0, 250.0), &Pawn->GetNavAgentPropertiesRef()))
		{
			CompleteReposition(Strong, *Data, false);
			return;
		}
		Data->Destination = Projected.Location;
		Data->MoveController = Controller;
		FAIMoveRequest Request;
		Request.SetGoalLocation(Projected.Location);
		Request.SetAcceptanceRadius(Acceptance);
		Request.SetReachTestIncludesAgentRadius(false);
		Request.SetUsePathfinding(true);
		Request.SetAllowPartialPath(false);
		Request.SetProjectGoalLocation(false);
		const FPathFollowingRequestResult Result = Controller->MoveTo(Request);
		Data->MoveRequest = Result.MoveId;
		if (Result.Code != EPathFollowingRequestResult::RequestSuccessful)
		{
			CompleteReposition(Strong, *Data, Result.Code == EPathFollowingRequestResult::AlreadyAtGoal);
			return;
		}
		UPathFollowingComponent* Path = Controller->GetPathFollowingComponent();
		Data->MoveHandle = Path->OnRequestFinished.AddLambda([Weak, Token](FAIRequestID RequestId,
			const FPathFollowingResult& MoveResult)
		{
			if (MoveResult.Code == EPathFollowingResult::Aborted)
			{
				return;
			}
			FStateTreeStrongExecutionContext CallbackContext = Weak.MakeStrongExecutionContext();
			FKnightAIRepositionInstanceData* CallbackData =
				CallbackContext.GetInstanceDataPtr<FKnightAIRepositionInstanceData>();
			if (CallbackData == nullptr || CallbackData->RequestToken != Token || CallbackData->MoveRequest != RequestId)
			{
				return;
			}
			const bool bStillOwned = CallbackData->DecisionContext.IsValid() &&
				CallbackData->DecisionContext->GetDecisionSnapshot().bAlive &&
				CallbackData->DecisionContext->GetDecisionSnapshot().TargetId == CallbackData->TargetId;
			CompleteReposition(CallbackContext, *CallbackData, bStillOwned && MoveResult.IsSuccess());
		});
	}
}

const UStruct* FKnightAIDecisionEvaluator::GetInstanceDataType() const
{
	return FInstanceDataType::StaticStruct();
}

void FKnightAIDecisionEvaluator::TreeStart(FStateTreeExecutionContext& Context) const
{
	Tick(Context, 0.0f);
}

void FKnightAIDecisionEvaluator::TreeStop(FStateTreeExecutionContext& Context) const
{
	if (UKnightAIContextComponent* Decision = KnightAIStateTree::ResolveContext(Context))
	{
		Decision->CancelOwnedAction();
	}
}

void FKnightAIDecisionEvaluator::Tick(FStateTreeExecutionContext& Context, float DeltaTime) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	UKnightAIContextComponent* Decision = KnightAIStateTree::ResolveContext(Context);
	if (Decision != nullptr)
	{
		Decision->RefreshDecisionContext();
		Data.Snapshot = Decision->GetDecisionSnapshot();
	}
	else
	{
		Data.Snapshot = FKnightAIDecisionSnapshot();
	}
}

const UStruct* FKnightAIContextCondition::GetInstanceDataType() const
{
	return FInstanceDataType::StaticStruct();
}

bool FKnightAIContextCondition::TestCondition(FStateTreeExecutionContext& Context) const
{
	const UKnightAIContextComponent* Decision = KnightAIStateTree::ResolveContext(Context);
	if (Decision == nullptr)
	{
		return false;
	}
	const FKnightAIDecisionSnapshot Snapshot = Decision->GetDecisionSnapshot();
	bool bValue = false;
	switch (Kind)
	{
	case EKnightAIContextConditionKind::Alive:
		bValue = Snapshot.bAlive;
		break;
	case EKnightAIContextConditionKind::HasTarget:
		bValue = Snapshot.bHasTarget;
		break;
	case EKnightAIContextConditionKind::FriendlyDownedTarget:
		bValue = Snapshot.bTargetFriendlyDowned;
		break;
	case EKnightAIContextConditionKind::CanRevive:
		bValue = KnightAIStateTree::CanRevive(Snapshot, Decision);
		break;
	case EKnightAIContextConditionKind::NeedsRegroup:
		bValue = Snapshot.bNeedsRegroup;
		break;
	case EKnightAIContextConditionKind::CanSelectAction:
		bValue = Snapshot.bCanSelectAction;
		break;
	case EKnightAIContextConditionKind::HoldPosition:
		bValue = Snapshot.bHoldPosition;
		break;
	case EKnightAIContextConditionKind::Staggered:
		bValue = Snapshot.bStaggered;
		break;
	case EKnightAIContextConditionKind::HasFormationGoal:
		bValue = Snapshot.bHasFormationGoal;
		break;
	}
	return bValue == bDesiredValue;
}

const UStruct* FKnightAISelectTargetTask::GetInstanceDataType() const
{
	return FInstanceDataType::StaticStruct();
}

EStateTreeRunStatus FKnightAISelectTargetTask::EnterState(FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	Data.SelectedTargetId = FKnightEntityId();
	UKnightAIContextComponent* Decision = KnightAIStateTree::ResolveContext(Context);
	if (Decision == nullptr)
	{
		return EStateTreeRunStatus::Failed;
	}
	Decision->RefreshDecisionContext();
	const FKnightAIDecisionSnapshot Snapshot = Decision->GetDecisionSnapshot();
	Data.SelectedTargetId = Snapshot.TargetId;
	return Snapshot.bHasTarget ? EStateTreeRunStatus::Succeeded : EStateTreeRunStatus::Failed;
}

FKnightAIExecuteActionTask::FKnightAIExecuteActionTask()
{
	bShouldCallTick = false;
	// Exact GAS completion wakes a sleeping task; no per-frame ability-state polling is required.
}

const UStruct* FKnightAIExecuteActionTask::GetInstanceDataType() const
{
	return FInstanceDataType::StaticStruct();
}

EStateTreeRunStatus FKnightAIExecuteActionTask::EnterState(FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	Data.DecisionContext = KnightAIStateTree::ResolveContext(Context);
	Data.ActionToken.Invalidate();
	Data.CompletionStatus = EKnightAIActionStatus::Unknown;
	if (!Data.DecisionContext.IsValid())
	{
		return EStateTreeRunStatus::Failed;
	}
	Data.CompletionState = MakeShared<FKnightAIActionCompletionState>();
	Data.CompletionState->WeakContext = Context.MakeWeakExecutionContext();
	Data.CompletionState->World = Data.DecisionContext->GetWorld();
	const TSharedRef<FKnightAIActionCompletionState> Completion = Data.CompletionState.ToSharedRef();
	Data.CompletionHandle = Data.DecisionContext->OnActionFinished.AddLambda(
		[Completion](FGuid Token, EKnightAIActionStatus Status)
	{
		if (Completion->bCancelled || !Completion->World.IsValid() || Completion->World->bIsTearingDown)
		{
			return;
		}
		Completion->DeferredCompletion = Completion->World->GetTimerManager().SetTimerForNextTick(
			FTimerDelegate::CreateLambda([Completion, Token, Status]()
		{
			if (Completion->bCancelled)
			{
				return;
			}
			FStateTreeStrongExecutionContext Strong = Completion->WeakContext.MakeStrongExecutionContext();
			FInstanceDataType* CallbackData = Strong.GetInstanceDataPtr<FInstanceDataType>();
			if (CallbackData == nullptr || CallbackData->ActionToken != Token)
			{
				return;
			}
			CallbackData->CompletionStatus = Status;
			Strong.FinishTask(Status == EKnightAIActionStatus::Succeeded
				? EStateTreeFinishTaskType::Succeeded : EStateTreeFinishTaskType::Failed);
		}));
	});
	FKnightResult Result;
	if (bRevive)
	{
		Result = Data.DecisionContext->TryBeginRevive(Data.ActionToken);
	}
	else
	{
		if (Data.DecisionContext->HasSpecialization())
		{
			Result = Data.DecisionContext->TryBeginSpecializationAction(bUseAttackLease, Data.ActionToken);
		}
		else
		{
			FKnightAIActionEntry Action;
			Result = Data.DecisionContext->TrySelectAction(Action);
			if (Result.IsSuccess())
			{
				Result = Data.DecisionContext->TryBeginAction(Action, bUseAttackLease, Data.ActionToken);
			}
		}
	}
	if (!Result.IsSuccess())
	{
		return EStateTreeRunStatus::Failed;
	}
	const EKnightAIActionStatus Status = Data.DecisionContext->GetActionStatus(Data.ActionToken);
	return KnightAIStateTree::ToRunStatus(Status);
}

void FKnightAIExecuteActionTask::ExitState(FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	const FGuid Token = Data.ActionToken;
	Data.ActionToken.Invalidate();
	if (Data.CompletionState.IsValid())
	{
		Data.CompletionState->bCancelled = true;
		if (Data.CompletionState->World.IsValid())
		{
			Data.CompletionState->World->GetTimerManager().ClearTimer(Data.CompletionState->DeferredCompletion);
		}
		Data.CompletionState.Reset();
	}
	if (Data.DecisionContext.IsValid())
	{
		Data.DecisionContext->OnActionFinished.Remove(Data.CompletionHandle);
		if (Token.IsValid())
		{
			Data.DecisionContext->CancelAction(Token);
		}
	}
	Data.CompletionHandle.Reset();
	Data.DecisionContext.Reset();
}

const UStruct* FKnightAIRepositionTask::GetInstanceDataType() const
{
	return FInstanceDataType::StaticStruct();
}

EStateTreeRunStatus FKnightAIRepositionTask::EnterState(FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	Data = FInstanceDataType();
	Data.DecisionContext = KnightAIStateTree::ResolveContext(Context);
	if (!Data.DecisionContext.IsValid() || !FMath::IsFinite(AcceptanceRadius) || AcceptanceRadius <= 0.0f ||
		!FMath::IsFinite(Timeout) || Timeout < 0.1f || Timeout > 60.0f)
	{
		return EStateTreeRunStatus::Failed;
	}
	const FKnightAIDecisionSnapshot Snapshot = Data.DecisionContext->GetDecisionSnapshot();
	APawn* Pawn = Data.DecisionContext->GetControlledPawn();
	if (!Snapshot.bAlive || !Snapshot.bHasTarget || Snapshot.bHoldPosition || Pawn == nullptr)
	{
		return EStateTreeRunStatus::Failed;
	}
	Data.TargetId = Snapshot.TargetId;
	Data.MoveController = Cast<AAIController>(Pawn->GetController());
	Data.RequestToken = FGuid::NewGuid();
	Data.StartedAt = Pawn->GetWorld()->GetTimeSeconds();
	const FStateTreeWeakExecutionContext Weak = Context.MakeWeakExecutionContext();
	const FGuid Token = Data.RequestToken;
	if (bMoveToReviveTarget)
	{
		// Direct navigation approach still uses the real navmesh/path, then GAS validates range/LOS at revive start.
		AAIController* Controller = Cast<AAIController>(Pawn->GetController());
		UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(Pawn->GetWorld());
		FNavLocation Projected;
		UKnightAbilitySystemComponent* TargetASC = Cast<UKnightAbilitySystemComponent>(
			UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Snapshot.Target.Get(), true));
		if (!Snapshot.bTargetFriendlyDowned || Controller == nullptr || Navigation == nullptr || TargetASC == nullptr ||
			TargetASC->GetLifeComponent() == nullptr || !Navigation->ProjectPointToNavigation(
				Snapshot.Target->GetActorLocation(), Projected, FVector(150.0, 150.0, 250.0),
				&Pawn->GetNavAgentPropertiesRef()))
		{
			return EStateTreeRunStatus::Failed;
		}
		FAIMoveRequest Request;
		Request.SetGoalLocation(Projected.Location);
		Request.SetAcceptanceRadius(FMath::Max(1.0f, TargetASC->GetLifeComponent()->GetReviveRange() * 0.65f));
		Request.SetReachTestIncludesAgentRadius(false);
		Request.SetAllowPartialPath(false);
		const FPathFollowingRequestResult Move = Controller->MoveTo(Request);
		Data.MoveRequest = Move.MoveId;
		Data.Destination = Projected.Location;
		if (Move.Code == EPathFollowingRequestResult::AlreadyAtGoal)
		{
			return EStateTreeRunStatus::Succeeded;
		}
		if (Move.Code != EPathFollowingRequestResult::RequestSuccessful)
		{
			return EStateTreeRunStatus::Failed;
		}
		Data.MoveHandle = Controller->GetPathFollowingComponent()->OnRequestFinished.AddLambda(
			[Weak, Token](FAIRequestID MoveId, const FPathFollowingResult& Result)
		{
			if (Result.Code == EPathFollowingResult::Aborted)
			{
				return;
			}
			FStateTreeStrongExecutionContext Strong = Weak.MakeStrongExecutionContext();
			FInstanceDataType* Callback = Strong.GetInstanceDataPtr<FInstanceDataType>();
			if (Callback != nullptr && Callback->RequestToken == Token && Callback->MoveRequest == MoveId)
			{
				KnightAIStateTree::CompleteReposition(Strong, *Callback, Result.IsSuccess());
			}
		});
	}
	else
	{
		UEnvQuery* Query = Data.DecisionContext->GetRepositionQuery();
		if (Query == nullptr)
		{
			return EStateTreeRunStatus::Failed;
		}
		FEnvQueryRequest Request(Query, Pawn);
		Data.QueryId = Request.Execute(EEnvQueryRunMode::SingleResult,
			FQueryFinishedSignature::CreateLambda([Weak, Token, Acceptance = AcceptanceRadius]
				(TSharedPtr<FEnvQueryResult> Result)
		{
			// UE calls the finish delegate synchronously from AbortQuery, including during tree ExitState.
			if (!Result.IsValid() || Result->GetRawStatus() == EEnvQueryStatus::Aborted)
			{
				return;
			}
			FStateTreeStrongExecutionContext Strong = Weak.MakeStrongExecutionContext();
			FInstanceDataType* Callback = Strong.GetInstanceDataPtr<FInstanceDataType>();
			if (Callback == nullptr || Callback->RequestToken != Token || !Result.IsValid() ||
				Callback->QueryId != Result->QueryID)
			{
				return;
			}
			Callback->QueryId = INDEX_NONE;
			if (Callback->DecisionContext.IsValid())
			{
				Callback->DecisionContext->FinishEQSRequest(Result->QueryID, false);
			}
			if (!Result->IsSuccessful() || Result->Items.IsEmpty())
			{
				KnightAIStateTree::CompleteReposition(Strong, *Callback, false);
				return;
			}
			KnightAIStateTree::BeginRepositionMove(Strong, Weak, Token, Result->GetItemAsLocation(0), Acceptance);
		}));
		if (Data.QueryId == INDEX_NONE)
		{
			return EStateTreeRunStatus::Failed;
		}
		Data.DecisionContext->TrackEQSRequest(Data.QueryId);
	}
	return EStateTreeRunStatus::Running;
}

void FKnightAIRepositionTask::ExitState(FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	Data.RequestToken.Invalidate();
	if (Data.DecisionContext.IsValid())
	{
		UKnightAIContextComponent* Decision = Data.DecisionContext.Get();
		if (Data.QueryId != INDEX_NONE)
		{
			if (UEnvQueryManager* Queries = UEnvQueryManager::GetCurrent(Decision->GetWorld()))
			{
				Queries->AbortQuery(Data.QueryId);
			}
			Decision->FinishEQSRequest(Data.QueryId, true);
		}
	}
	AAIController* Controller = Data.MoveController.Get();
	if (Controller != nullptr)
	{
		UPathFollowingComponent* Path = Controller->GetPathFollowingComponent();
		Path->OnRequestFinished.Remove(Data.MoveHandle);
		if (Data.MoveRequest.IsValid() && Controller->GetCurrentMoveRequestID() == Data.MoveRequest)
		{
			Path->AbortMove(*Controller, FPathFollowingResultFlags::ForcedScript, Data.MoveRequest);
		}
	}
	Data.QueryId = INDEX_NONE;
	Data.MoveHandle.Reset();
	Data.DecisionContext.Reset();
	Data.MoveController.Reset();
}

EStateTreeRunStatus FKnightAIRepositionTask::Tick(FStateTreeExecutionContext& Context, float DeltaTime) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	if (!Data.DecisionContext.IsValid())
	{
		return EStateTreeRunStatus::Failed;
	}
	const FKnightAIDecisionSnapshot Snapshot = Data.DecisionContext->GetDecisionSnapshot();
	if (!Snapshot.bAlive || !Snapshot.bHasTarget || Snapshot.TargetId != Data.TargetId ||
		Data.DecisionContext->GetWorld()->GetTimeSeconds() - Data.StartedAt > Timeout)
	{
		return EStateTreeRunStatus::Failed;
	}
	return Data.bCompleted ? (Data.bSucceeded ? EStateTreeRunStatus::Succeeded : EStateTreeRunStatus::Failed)
		: EStateTreeRunStatus::Running;
}

const UStruct* FKnightAIFormationTask::GetInstanceDataType() const
{
	return FInstanceDataType::StaticStruct();
}

EStateTreeRunStatus FKnightAIFormationTask::EnterState(FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	Context.GetInstanceData(*this).ElapsedSinceRequest = RepathInterval;
	return Tick(Context, 0.0f);
}

void FKnightAIFormationTask::ExitState(FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	AAIController* Controller = Cast<AAIController>(Context.GetOwner());
	const FAIRequestID RequestId = Context.GetInstanceData(*this).MoveRequest;
	if (Controller != nullptr && RequestId.IsValid() && Controller->GetCurrentMoveRequestID() == RequestId)
	{
		Controller->GetPathFollowingComponent()->AbortMove(*Controller,
			FPathFollowingResultFlags::ForcedScript, RequestId);
	}
}

EStateTreeRunStatus FKnightAIFormationTask::Tick(FStateTreeExecutionContext& Context, float DeltaTime) const
{
	UKnightAIContextComponent* Decision = KnightAIStateTree::ResolveContext(Context);
	AAIController* Controller = Cast<AAIController>(Context.GetOwner());
	if (Decision == nullptr || Controller == nullptr || !FMath::IsFinite(RepathInterval) || RepathInterval < 0.05f)
	{
		return EStateTreeRunStatus::Failed;
	}
	const FKnightAIDecisionSnapshot Snapshot = Decision->GetDecisionSnapshot();
	APawn* Pawn = Decision->GetControlledPawn();
	if (!Snapshot.bAlive || Snapshot.bStaggered || !Snapshot.bHasFormationGoal || Pawn == nullptr)
	{
		return EStateTreeRunStatus::Failed;
	}
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	if (FVector::DistSquared2D(Pawn->GetActorLocation(), Snapshot.FormationGoal) <=
		FMath::Square(Snapshot.FormationAcceptance))
	{
		if (Data.MoveRequest.IsValid() && Controller->GetCurrentMoveRequestID() == Data.MoveRequest)
		{
			Controller->GetPathFollowingComponent()->AbortMove(*Controller,
				FPathFollowingResultFlags::ForcedScript, Data.MoveRequest);
		}
		return bCompleteWhenReached ? EStateTreeRunStatus::Succeeded : EStateTreeRunStatus::Running;
	}
	Data.ElapsedSinceRequest += DeltaTime;
	if (Data.ElapsedSinceRequest < RepathInterval)
	{
		return EStateTreeRunStatus::Running;
	}
	Data.ElapsedSinceRequest = 0.0f;
	UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(Pawn->GetWorld());
	FNavLocation Projected;
	if (Navigation == nullptr || !Navigation->ProjectPointToNavigation(Snapshot.FormationGoal, Projected,
		FVector(150.0, 150.0, 250.0), &Pawn->GetNavAgentPropertiesRef()))
	{
		return EStateTreeRunStatus::Failed;
	}
	FAIMoveRequest Request;
	Request.SetGoalLocation(Projected.Location);
	Request.SetAcceptanceRadius(Snapshot.FormationAcceptance);
	Request.SetAllowPartialPath(false);
	const FPathFollowingRequestResult Result = Controller->MoveTo(Request);
	Data.MoveRequest = Result.MoveId;
	return Result.Code == EPathFollowingRequestResult::Failed ? EStateTreeRunStatus::Failed : EStateTreeRunStatus::Running;
}

const UStruct* FKnightAIWaitTask::GetInstanceDataType() const
{
	return FInstanceDataType::StaticStruct();
}

EStateTreeRunStatus FKnightAIWaitTask::EnterState(FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	Context.GetInstanceData(*this).Elapsed = 0.0f;
	return FMath::IsFinite(Duration) && Duration >= 0.05f && Duration <= 60.0f
		? EStateTreeRunStatus::Running : EStateTreeRunStatus::Failed;
}

EStateTreeRunStatus FKnightAIWaitTask::Tick(FStateTreeExecutionContext& Context, float DeltaTime) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	Data.Elapsed += DeltaTime;
	return Data.Elapsed >= Duration ? EStateTreeRunStatus::Succeeded : EStateTreeRunStatus::Running;
}
