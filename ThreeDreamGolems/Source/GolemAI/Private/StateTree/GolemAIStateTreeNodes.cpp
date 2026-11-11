#include "StateTree/GolemAIStateTreeNodes.h"

#include "AIController.h"
#include "AbilitySystemGlobals.h"
#include "Components/GolemAbilitySystemComponent.h"
#include "Components/GolemLifeComponent.h"
#include "Engine/World.h"
#include "EnvironmentQuery/EnvQuery.h"
#include "EnvironmentQuery/EnvQueryManager.h"
#include "GameFramework/Pawn.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationSystem.h"
#include "StateTreeExecutionContext.h"
#include "TimerManager.h"

/** Completion is deferred out of the tree's write scope, including instant GAS end during EnterState. */
struct FGolemAIActionCompletionState
{
	FStateTreeWeakExecutionContext WeakContext;
	TWeakObjectPtr<UWorld> World;
	FTimerHandle DeferredCompletion;
	bool bCancelled = false;
};

namespace GolemAIStateTree
{
	UGolemAIContextComponent* ResolveContext(const FStateTreeExecutionContext& Context)
	{
		AActor* Owner = Cast<AActor>(Context.GetOwner());
		return Owner != nullptr ? Owner->FindComponentByClass<UGolemAIContextComponent>() : nullptr;
	}

	EStateTreeRunStatus ToRunStatus(EGolemAIActionStatus Status)
	{
		return Status == EGolemAIActionStatus::Succeeded ? EStateTreeRunStatus::Succeeded
			: Status == EGolemAIActionStatus::Running ? EStateTreeRunStatus::Running : EStateTreeRunStatus::Failed;
	}

	bool CanRevive(const FGolemAIDecisionSnapshot& Snapshot, const UGolemAIContextComponent* Decision)
	{
		UGolemAbilitySystemComponent* TargetASC = Snapshot.bTargetFriendlyDowned && Snapshot.Target.IsValid()
			? Cast<UGolemAbilitySystemComponent>(UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(
				Snapshot.Target.Get(), true)) : nullptr;
		return Decision != nullptr && Snapshot.bAlive && !Snapshot.bStaggered && TargetASC != nullptr &&
			TargetASC->GetLifeComponent() != nullptr && TargetASC->GetLifeComponent()->
				ValidateReviveSource(Decision->GetControlledPawn()).IsSuccess();
	}

	void CompleteReposition(FStateTreeStrongExecutionContext& Strong, FGolemAIRepositionInstanceData& Data,
		bool bSucceeded)
	{
		Data.bCompleted = true;
		Data.bSucceeded = bSucceeded;
		Strong.FinishTask(bSucceeded ? EStateTreeFinishTaskType::Succeeded : EStateTreeFinishTaskType::Failed);
	}

	void BeginRepositionMove(FStateTreeStrongExecutionContext& Strong,
		const FStateTreeWeakExecutionContext& Weak, const FGuid& Token, FVector Goal, float Acceptance)
	{
		FGolemAIRepositionInstanceData* Data = Strong.GetInstanceDataPtr<FGolemAIRepositionInstanceData>();
		if (Data == nullptr || Data->RequestToken != Token || !Data->DecisionContext.IsValid())
		{
			return;
		}
		UGolemAIContextComponent* Decision = Data->DecisionContext.Get();
		APawn* Pawn = Decision->GetControlledPawn();
		AAIController* Controller = Pawn != nullptr ? Cast<AAIController>(Pawn->GetController()) : nullptr;
		const FGolemAIDecisionSnapshot Snapshot = Decision->GetDecisionSnapshot();
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
			FGolemAIRepositionInstanceData* CallbackData =
				CallbackContext.GetInstanceDataPtr<FGolemAIRepositionInstanceData>();
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

const UStruct* FGolemAIDecisionEvaluator::GetInstanceDataType() const
{
	return FInstanceDataType::StaticStruct();
}

void FGolemAIDecisionEvaluator::TreeStart(FStateTreeExecutionContext& Context) const
{
	Tick(Context, 0.0f);
}

void FGolemAIDecisionEvaluator::TreeStop(FStateTreeExecutionContext& Context) const
{
	if (UGolemAIContextComponent* Decision = GolemAIStateTree::ResolveContext(Context))
	{
		Decision->CancelOwnedAction();
	}
}

void FGolemAIDecisionEvaluator::Tick(FStateTreeExecutionContext& Context, float DeltaTime) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	UGolemAIContextComponent* Decision = GolemAIStateTree::ResolveContext(Context);
	if (Decision != nullptr)
	{
		Decision->RefreshDecisionContext();
		Data.Snapshot = Decision->GetDecisionSnapshot();
	}
	else
	{
		Data.Snapshot = FGolemAIDecisionSnapshot();
	}
}

const UStruct* FGolemAIContextCondition::GetInstanceDataType() const
{
	return FInstanceDataType::StaticStruct();
}

bool FGolemAIContextCondition::TestCondition(FStateTreeExecutionContext& Context) const
{
	const UGolemAIContextComponent* Decision = GolemAIStateTree::ResolveContext(Context);
	if (Decision == nullptr)
	{
		return false;
	}
	const FGolemAIDecisionSnapshot Snapshot = Decision->GetDecisionSnapshot();
	bool bValue = false;
	switch (Kind)
	{
	case EGolemAIContextConditionKind::Alive:
		bValue = Snapshot.bAlive;
		break;
	case EGolemAIContextConditionKind::HasTarget:
		bValue = Snapshot.bHasTarget;
		break;
	case EGolemAIContextConditionKind::FriendlyDownedTarget:
		bValue = Snapshot.bTargetFriendlyDowned;
		break;
	case EGolemAIContextConditionKind::CanRevive:
		bValue = GolemAIStateTree::CanRevive(Snapshot, Decision);
		break;
	case EGolemAIContextConditionKind::NeedsRegroup:
		bValue = Snapshot.bNeedsRegroup;
		break;
	case EGolemAIContextConditionKind::CanSelectAction:
		bValue = Snapshot.bCanSelectAction;
		break;
	case EGolemAIContextConditionKind::HoldPosition:
		bValue = Snapshot.bHoldPosition;
		break;
	case EGolemAIContextConditionKind::Staggered:
		bValue = Snapshot.bStaggered;
		break;
	case EGolemAIContextConditionKind::HasFormationGoal:
		bValue = Snapshot.bHasFormationGoal;
		break;
	}
	return bValue == bDesiredValue;
}

const UStruct* FGolemAISelectTargetTask::GetInstanceDataType() const
{
	return FInstanceDataType::StaticStruct();
}

EStateTreeRunStatus FGolemAISelectTargetTask::EnterState(FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	Data.SelectedTargetId = FGolemEntityId();
	UGolemAIContextComponent* Decision = GolemAIStateTree::ResolveContext(Context);
	if (Decision == nullptr)
	{
		return EStateTreeRunStatus::Failed;
	}
	Decision->RefreshDecisionContext();
	const FGolemAIDecisionSnapshot Snapshot = Decision->GetDecisionSnapshot();
	Data.SelectedTargetId = Snapshot.TargetId;
	return Snapshot.bHasTarget ? EStateTreeRunStatus::Succeeded : EStateTreeRunStatus::Failed;
}

FGolemAIExecuteActionTask::FGolemAIExecuteActionTask()
{
	bShouldCallTick = false;
	// Exact GAS completion wakes a sleeping task; no per-frame ability-state polling is required.
}

const UStruct* FGolemAIExecuteActionTask::GetInstanceDataType() const
{
	return FInstanceDataType::StaticStruct();
}

EStateTreeRunStatus FGolemAIExecuteActionTask::EnterState(FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	Data.DecisionContext = GolemAIStateTree::ResolveContext(Context);
	Data.ActionToken.Invalidate();
	Data.CompletionStatus = EGolemAIActionStatus::Unknown;
	if (!Data.DecisionContext.IsValid())
	{
		return EStateTreeRunStatus::Failed;
	}
	Data.CompletionState = MakeShared<FGolemAIActionCompletionState>();
	Data.CompletionState->WeakContext = Context.MakeWeakExecutionContext();
	Data.CompletionState->World = Data.DecisionContext->GetWorld();
	const TSharedRef<FGolemAIActionCompletionState> Completion = Data.CompletionState.ToSharedRef();
	Data.CompletionHandle = Data.DecisionContext->OnActionFinished.AddLambda(
		[Completion](FGuid Token, EGolemAIActionStatus Status)
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
			Strong.FinishTask(Status == EGolemAIActionStatus::Succeeded
				? EStateTreeFinishTaskType::Succeeded : EStateTreeFinishTaskType::Failed);
		}));
	});
	FGolemResult Result;
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
			FGolemAIActionEntry Action;
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
	const EGolemAIActionStatus Status = Data.DecisionContext->GetActionStatus(Data.ActionToken);
	return GolemAIStateTree::ToRunStatus(Status);
}

void FGolemAIExecuteActionTask::ExitState(FStateTreeExecutionContext& Context,
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

const UStruct* FGolemAIRepositionTask::GetInstanceDataType() const
{
	return FInstanceDataType::StaticStruct();
}

EStateTreeRunStatus FGolemAIRepositionTask::EnterState(FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	Data = FInstanceDataType();
	Data.DecisionContext = GolemAIStateTree::ResolveContext(Context);
	if (!Data.DecisionContext.IsValid() || !FMath::IsFinite(AcceptanceRadius) || AcceptanceRadius <= 0.0f ||
		!FMath::IsFinite(Timeout) || Timeout < 0.1f || Timeout > 60.0f)
	{
		return EStateTreeRunStatus::Failed;
	}
	const FGolemAIDecisionSnapshot Snapshot = Data.DecisionContext->GetDecisionSnapshot();
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
		UGolemAbilitySystemComponent* TargetASC = Cast<UGolemAbilitySystemComponent>(
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
				GolemAIStateTree::CompleteReposition(Strong, *Callback, Result.IsSuccess());
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
				GolemAIStateTree::CompleteReposition(Strong, *Callback, false);
				return;
			}
			GolemAIStateTree::BeginRepositionMove(Strong, Weak, Token, Result->GetItemAsLocation(0), Acceptance);
		}));
		if (Data.QueryId == INDEX_NONE)
		{
			return EStateTreeRunStatus::Failed;
		}
		Data.DecisionContext->TrackEQSRequest(Data.QueryId);
	}
	return EStateTreeRunStatus::Running;
}

void FGolemAIRepositionTask::ExitState(FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	Data.RequestToken.Invalidate();
	if (Data.DecisionContext.IsValid())
	{
		UGolemAIContextComponent* Decision = Data.DecisionContext.Get();
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

EStateTreeRunStatus FGolemAIRepositionTask::Tick(FStateTreeExecutionContext& Context, float DeltaTime) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	if (!Data.DecisionContext.IsValid())
	{
		return EStateTreeRunStatus::Failed;
	}
	const FGolemAIDecisionSnapshot Snapshot = Data.DecisionContext->GetDecisionSnapshot();
	if (!Snapshot.bAlive || !Snapshot.bHasTarget || Snapshot.TargetId != Data.TargetId ||
		Data.DecisionContext->GetWorld()->GetTimeSeconds() - Data.StartedAt > Timeout)
	{
		return EStateTreeRunStatus::Failed;
	}
	return Data.bCompleted ? (Data.bSucceeded ? EStateTreeRunStatus::Succeeded : EStateTreeRunStatus::Failed)
		: EStateTreeRunStatus::Running;
}

const UStruct* FGolemAIFormationTask::GetInstanceDataType() const
{
	return FInstanceDataType::StaticStruct();
}

EStateTreeRunStatus FGolemAIFormationTask::EnterState(FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	Context.GetInstanceData(*this).ElapsedSinceRequest = RepathInterval;
	return Tick(Context, 0.0f);
}

void FGolemAIFormationTask::ExitState(FStateTreeExecutionContext& Context,
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

EStateTreeRunStatus FGolemAIFormationTask::Tick(FStateTreeExecutionContext& Context, float DeltaTime) const
{
	UGolemAIContextComponent* Decision = GolemAIStateTree::ResolveContext(Context);
	AAIController* Controller = Cast<AAIController>(Context.GetOwner());
	if (Decision == nullptr || Controller == nullptr || !FMath::IsFinite(RepathInterval) || RepathInterval < 0.05f)
	{
		return EStateTreeRunStatus::Failed;
	}
	const FGolemAIDecisionSnapshot Snapshot = Decision->GetDecisionSnapshot();
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

const UStruct* FGolemAIWaitTask::GetInstanceDataType() const
{
	return FInstanceDataType::StaticStruct();
}

EStateTreeRunStatus FGolemAIWaitTask::EnterState(FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	Context.GetInstanceData(*this).Elapsed = 0.0f;
	return FMath::IsFinite(Duration) && Duration >= 0.05f && Duration <= 60.0f
		? EStateTreeRunStatus::Running : EStateTreeRunStatus::Failed;
}

EStateTreeRunStatus FGolemAIWaitTask::Tick(FStateTreeExecutionContext& Context, float DeltaTime) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	Data.Elapsed += DeltaTime;
	return Data.Elapsed >= Duration ? EStateTreeRunStatus::Succeeded : EStateTreeRunStatus::Running;
}
