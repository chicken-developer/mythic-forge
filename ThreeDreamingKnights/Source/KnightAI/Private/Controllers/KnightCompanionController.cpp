#include "Controllers/KnightCompanionController.h"

#include "Character/KnightCharacter.h"
#include "Definitions/KnightHeroDefinition.h"

#include "Components/StateTreeAIComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "NavigationSystem.h"
#include "Navigation/PathFollowingComponent.h"
#include "StateTree.h"

DEFINE_LOG_CATEGORY_STATIC(LogKnightCompanion, Log, All);

AKnightCompanionController::AKnightCompanionController()
{
	FollowComponent = CreateDefaultSubobject<UStateTreeAIComponent>(TEXT("FollowStateTree"));
	FollowComponent->SetStartLogicAutomatically(false);
	BrainComponent = FollowComponent;
	bStartAILogicOnPossess = false;
	FollowTree = TSoftObjectPtr<UStateTree>(
		FSoftObjectPath(TEXT("/Game/Knight/AI/ST_Knight_Follow.ST_Knight_Follow")));
}

UStateTreeAIComponent* AKnightCompanionController::GetFollowComponent() const
{
	return FollowComponent;
}

bool AKnightCompanionController::IsFollowing() const
{
	return FollowComponent != nullptr && FollowComponent->IsRunning();
}

bool AKnightCompanionController::TryGetFormationGoal(FVector& OutGoal, float& OutAcceptanceRadius) const
{
	const AKnightCharacter* Knight = Cast<AKnightCharacter>(GetPawn());
	const UWorld* World = GetWorld();
	if (!HasAuthority() || World == nullptr || Knight == nullptr || !Knight->CanTransferControl())
	{
		return false;
	}
	const UKnightHeroDefinition* Definition = Knight->GetHeroDefinition();
	if (Definition == nullptr)
	{
		return false;
	}

	const AKnightCharacter* Leader = nullptr;
	double NearestDistanceSquared = TNumericLimits<double>::Max();
	for (FConstPlayerControllerIterator Iterator = World->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		const APlayerController* HumanController = Iterator->Get();
		const AKnightCharacter* Candidate = HumanController != nullptr
			? Cast<AKnightCharacter>(HumanController->GetPawn()) : nullptr;
		if (Candidate == nullptr || Candidate == Knight || !Candidate->CanTransferControl())
		{
			continue;
		}
		const double DistanceSquared = FVector::DistSquared(Knight->GetActorLocation(), Candidate->GetActorLocation());
		if (DistanceSquared < NearestDistanceSquared)
		{
			NearestDistanceSquared = DistanceSquared;
			Leader = Candidate;
		}
	}
	if (Leader == nullptr)
	{
		return false;
	}

	OutGoal = Leader->GetActorLocation()
		+ FRotator(0.0, Leader->GetActorRotation().Yaw, 0.0).RotateVector(Definition->GetFormationOffset());
	OutAcceptanceRadius = Definition->GetFollowDistance();
	return true;
}

bool AKnightCompanionController::TryMoveToFormation(const FVector& Goal, const float AcceptanceRadius)
{
	AKnightCharacter* Knight = Cast<AKnightCharacter>(GetPawn());
	UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!HasAuthority() || Knight == nullptr || !Knight->CanTransferControl() || Navigation == nullptr)
	{
		return false;
	}

	FNavLocation ProjectedGoal;
	const FVector ProjectionExtent(150.0, 150.0, 250.0);
	if (!Navigation->ProjectPointToNavigation(Goal, ProjectedGoal, ProjectionExtent,
		&Knight->GetCharacterMovement()->GetNavAgentPropertiesRef()))
	{
		StopMovement();
		return false;
	}

	const EPathFollowingRequestResult::Type Result = MoveToLocation(ProjectedGoal.Location, AcceptanceRadius,
		true, true, false, true, nullptr, false);
	return Result != EPathFollowingRequestResult::Failed;
}

void AKnightCompanionController::StopFollowing(const FString& Reason)
{
	FollowComponent->StopLogic(Reason);
	StopMovement();
}

void AKnightCompanionController::SetFollowTree(UStateTree* InFollowTree)
{
	StopFollowing(TEXT("Follow tree replaced"));
	FollowTree = InFollowTree;
	if (HasActorBegunPlay() && GetPawn() != nullptr)
	{
		StartFollowing();
	}
}

void AKnightCompanionController::BeginPlay()
{
	Super::BeginPlay();
	StartFollowing();
}

void AKnightCompanionController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	if (HasActorBegunPlay())
	{
		StartFollowing();
	}
}

void AKnightCompanionController::OnUnPossess()
{
	StopFollowing(TEXT("Knight controller ownership changed"));
	Super::OnUnPossess();
}

void AKnightCompanionController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopFollowing(TEXT("Companion controller ended"));
	Super::EndPlay(EndPlayReason);
}

void AKnightCompanionController::StartFollowing()
{
	const AKnightCharacter* Knight = Cast<AKnightCharacter>(GetPawn());
	if (!HasAuthority() || Knight == nullptr || FollowComponent->IsRunning())
	{
		return;
	}
	UStateTree* Tree = FollowTree.LoadSynchronous();
	if (Tree == nullptr || !Tree->IsReadyToRun())
	{
		UE_LOG(LogKnightCompanion, Error, TEXT("Follow StateTree is missing or uncompiled: %s."),
			*FollowTree.ToSoftObjectPath().ToString());
		return;
	}
	FollowComponent->SetStateTree(Tree);
	// A reclaimed downed Knight still needs its tree configured for the eventual life-state restart.
	if (!Knight->CanTransferControl())
	{
		return;
	}
	FollowComponent->StartLogic();
	if (!FollowComponent->IsRunning())
	{
		UE_LOG(LogKnightCompanion, Error, TEXT("Follow StateTree did not start for %s."), *GetName());
	}
}
