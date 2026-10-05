#include "Components/KnightAIDecisionSchedulerComponent.h"

#include "AIController.h"
#include "AbilitySystemGlobals.h"
#include "Components/KnightAbilitySystemComponent.h"
#include "Components/KnightLifeComponent.h"
#include "Components/StateTreeAIComponent.h"
#include "Context/KnightAIContextComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Tags/KnightGameplayTags.h"
#include "TimerManager.h"

UKnightAIDecisionSchedulerComponent::UKnightAIDecisionSchedulerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

FKnightResult UKnightAIDecisionSchedulerComponent::StartScheduling(
	UStateTreeAIComponent* InTree, UKnightAIContextComponent* InContext)
{
	AAIController* Controller = Cast<AAIController>(GetOwner());
	if (Controller == nullptr || !Controller->HasAuthority())
	{
		return FKnightResult::Failure(EKnightError::NotAuthority, TEXT("ScheduleAIDecision"),
			NSLOCTEXT("KnightAI", "ScheduleAuthority", "AI decisions run on the authoritative controller."));
	}
	if (bEndingPlay || GetWorld() == nullptr || GetWorld()->bIsTearingDown || Controller->GetPawn() == nullptr
		|| !IsValid(InTree) || !IsValid(InContext) || InTree->GetOwner() != Controller
		|| InContext->GetOwner() != Controller || InTree->PrimaryComponentTick.bCanEverTick)
	{
		return FKnightResult::Failure(EKnightError::InvalidState, TEXT("ScheduleAIDecision"),
			NSLOCTEXT("KnightAI", "ScheduleContext", "Scheduled AI requires its own initialized context and manual StateTree tick."));
	}
	StopScheduling();
	Tree = InTree;
	Context = InContext;
	PreviousDecisionAt = GetWorld()->GetTimeSeconds();
	DecisionLOD = CalculateLOD();
	const float LODInterval = GetIntervalForLOD(DecisionLOD);
	DecisionInterval = FMath::Max(LODInterval, InContext->GetBaseDecisionInterval());
	const FKnightResult Result = InContext->SetScheduledDecisionInterval(LODInterval);
	if (!Result.IsSuccess())
	{
		StopScheduling();
		return Result;
	}
	InTree->SetComponentTickEnabled(false);
	ScheduleNextDecision(0.01f);
	return FKnightResult::Success();
}

void UKnightAIDecisionSchedulerComponent::StopScheduling()
{
	++ScheduleGeneration;
	if (GetWorld() != nullptr)
	{
		GetWorld()->GetTimerManager().ClearTimer(DecisionTimer);
	}
	if (Tree.IsValid())
	{
		Tree->SetComponentTickEnabled(false);
	}
	DecisionTimer.Invalidate();
	Tree.Reset();
	Context.Reset();
}

void UKnightAIDecisionSchedulerComponent::RequestDecisionWake()
{
	if (!bEndingPlay && Tree.IsValid() && Context.IsValid())
	{
		ScheduleNextDecision(0.01f);
	}
}

EKnightAILODTier UKnightAIDecisionSchedulerComponent::GetDecisionLOD() const
{
	return DecisionLOD;
}

float UKnightAIDecisionSchedulerComponent::GetDecisionInterval() const
{
	return DecisionInterval;
}

int64 UKnightAIDecisionSchedulerComponent::GetDecisionTickCount() const
{
	return DecisionTickCount;
}

bool UKnightAIDecisionSchedulerComponent::HasDecisionTimer() const
{
	return GetWorld() != nullptr && GetWorld()->GetTimerManager().IsTimerActive(DecisionTimer);
}

void UKnightAIDecisionSchedulerComponent::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	bEndingPlay = true;
	StopScheduling();
	OnDecisionScheduled.Clear();
	Super::EndPlay(EndPlayReason);
}

void UKnightAIDecisionSchedulerComponent::RunDecision()
{
	AAIController* Controller = Cast<AAIController>(GetOwner());
	UWorld* World = GetWorld();
	if (bEndingPlay || World == nullptr || World->bIsTearingDown || Controller == nullptr
		|| !Controller->HasAuthority() || Controller->GetPawn() == nullptr || !Tree.IsValid() || !Context.IsValid())
	{
		StopScheduling();
		return;
	}
	DecisionLOD = CalculateLOD();
	const float LODInterval = GetIntervalForLOD(DecisionLOD);
	DecisionInterval = FMath::Max(LODInterval, Context->GetBaseDecisionInterval());
	const FKnightResult Result = Context->SetScheduledDecisionInterval(LODInterval);
	if (!Result.IsSuccess())
	{
		StopScheduling();
		return;
	}
	const double Now = World->GetTimeSeconds();
	const uint64 AcceptedGeneration = ScheduleGeneration;
	const TWeakObjectPtr<UStateTreeAIComponent> AcceptedTree = Tree;
	if (Tree->IsRunning() && !Tree->IsPaused())
	{
		// Stock StateTree schedules its next component tick internally; this timer owns the outer LOD cadence.
		Tree->TickComponent(FMath::Max(static_cast<float>(Now - PreviousDecisionAt), 0.001f),
			LEVELTICK_All, &Tree->PrimaryComponentTick);
		if (bEndingPlay || AcceptedGeneration != ScheduleGeneration || !AcceptedTree.IsValid()
			|| Tree != AcceptedTree || !Context.IsValid() || Controller->GetPawn() == nullptr)
		{
			return;
		}
		Tree->SetComponentTickEnabled(false);
		++DecisionTickCount;
		OnDecisionScheduled.Broadcast(DecisionLOD, DecisionInterval, DecisionTickCount);
	}
	if (bEndingPlay || AcceptedGeneration != ScheduleGeneration || !AcceptedTree.IsValid()
		|| Tree != AcceptedTree || !Context.IsValid() || Controller->GetPawn() == nullptr)
	{
		return;
	}
	PreviousDecisionAt = Now;
	ScheduleNextDecision(DecisionInterval);
}

void UKnightAIDecisionSchedulerComponent::ScheduleNextDecision(float Delay)
{
	if (!bEndingPlay && Tree.IsValid() && Context.IsValid()
		&& GetWorld() != nullptr && !GetWorld()->bIsTearingDown)
	{
		GetWorld()->GetTimerManager().SetTimer(DecisionTimer, this, &ThisClass::RunDecision, Delay, false);
	}
}

EKnightAILODTier UKnightAIDecisionSchedulerComponent::CalculateLOD() const
{
	const AAIController* Controller = Cast<AAIController>(GetOwner());
	const APawn* Pawn = Controller != nullptr ? Controller->GetPawn() : nullptr;
	const UWorld* World = GetWorld();
	if (Pawn == nullptr || World == nullptr)
	{
		return EKnightAILODTier::Dormant;
	}
	double DistanceSquared = TNumericLimits<double>::Max();
	for (FConstPlayerControllerIterator Iterator = World->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		const APlayerController* Human = Iterator->Get();
		const APawn* HumanPawn = Human != nullptr ? Human->GetPawn() : nullptr;
		const UKnightAbilitySystemComponent* ASC = HumanPawn != nullptr
			? Cast<UKnightAbilitySystemComponent>(UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(HumanPawn)) : nullptr;
		if (ASC != nullptr && ASC->HasMatchingGameplayTag(KnightGameplayTags::Role_Party_Knight)
			&& ASC->GetLifeComponent() != nullptr && !ASC->GetLifeComponent()->IsDead())
		{
			DistanceSquared = FMath::Min(DistanceSquared,
				FVector::DistSquared(Pawn->GetActorLocation(), HumanPawn->GetActorLocation()));
		}
	}
	if (DistanceSquared <= FMath::Square(1200.0))
	{
		return EKnightAILODTier::Near;
	}
	return DistanceSquared <= FMath::Square(3000.0) ? EKnightAILODTier::Far : EKnightAILODTier::Dormant;
}

float UKnightAIDecisionSchedulerComponent::GetIntervalForLOD(EKnightAILODTier Tier)
{
	switch (Tier)
	{
	case EKnightAILODTier::Near: return 0.1f;
	case EKnightAILODTier::Far: return 0.35f;
	case EKnightAILODTier::Dormant: return 1.0f;
	default: return 1.0f;
	}
}
