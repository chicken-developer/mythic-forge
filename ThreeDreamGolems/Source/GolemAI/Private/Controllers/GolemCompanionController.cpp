#include "Controllers/GolemCompanionController.h"

#include "Character/GolemCharacter.h"
#include "Definitions/GolemHeroDefinition.h"

#include "Components/StateTreeAIComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "NavigationSystem.h"
#include "Navigation/PathFollowingComponent.h"
#include "StateTree.h"

DEFINE_LOG_CATEGORY_STATIC(LogGolemCompanion, Log, All);

AGolemCompanionController::AGolemCompanionController()
{
	FollowComponent = CreateDefaultSubobject<UStateTreeAIComponent>(TEXT("FollowStateTree"));
	FollowComponent->SetStartLogicAutomatically(false);
	BrainComponent = FollowComponent;
	bStartAILogicOnPossess = false;
	FollowTree = TSoftObjectPtr<UStateTree>(
		FSoftObjectPath(TEXT("/Game/Knight/AI/ST_Knight_Follow.ST_Knight_Follow")));
}

UStateTreeAIComponent* AGolemCompanionController::GetFollowComponent() const
{
	return FollowComponent;
}

bool AGolemCompanionController::IsFollowing() const
{
	return FollowComponent != nullptr && FollowComponent->IsRunning();
}

bool AGolemCompanionController::TryGetFormationGoal(FVector& OutGoal, float& OutAcceptanceRadius) const
{
	const AGolemCharacter* Golem = Cast<AGolemCharacter>(GetPawn());
	const UWorld* World = GetWorld();
	if (!HasAuthority() || World == nullptr || Golem == nullptr || !Golem->CanTransferControl())
	{
		return false;
	}
	const UGolemHeroDefinition* Definition = Golem->GetHeroDefinition();
	if (Definition == nullptr)
	{
		return false;
	}

	const AGolemCharacter* Leader = nullptr;
	double NearestDistanceSquared = TNumericLimits<double>::Max();
	for (FConstPlayerControllerIterator Iterator = World->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		const APlayerController* HumanController = Iterator->Get();
		const AGolemCharacter* Candidate = HumanController != nullptr
			? Cast<AGolemCharacter>(HumanController->GetPawn()) : nullptr;
		if (Candidate == nullptr || Candidate == Golem || !Candidate->CanTransferControl())
		{
			continue;
		}
		const double DistanceSquared = FVector::DistSquared(Golem->GetActorLocation(), Candidate->GetActorLocation());
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

bool AGolemCompanionController::TryMoveToFormation(const FVector& Goal, const float AcceptanceRadius)
{
	AGolemCharacter* Golem = Cast<AGolemCharacter>(GetPawn());
	UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!HasAuthority() || Golem == nullptr || !Golem->CanTransferControl() || Navigation == nullptr)
	{
		return false;
	}

	FNavLocation ProjectedGoal;
	const FVector ProjectionExtent(150.0, 150.0, 250.0);
	if (!Navigation->ProjectPointToNavigation(Goal, ProjectedGoal, ProjectionExtent,
		&Golem->GetCharacterMovement()->GetNavAgentPropertiesRef()))
	{
		StopMovement();
		return false;
	}

	const EPathFollowingRequestResult::Type Result = MoveToLocation(ProjectedGoal.Location, AcceptanceRadius,
		true, true, false, true, nullptr, false);
	return Result != EPathFollowingRequestResult::Failed;
}

void AGolemCompanionController::StopFollowing(const FString& Reason)
{
	FollowComponent->StopLogic(Reason);
	StopMovement();
}

void AGolemCompanionController::SetFollowTree(UStateTree* InFollowTree)
{
	StopFollowing(TEXT("Follow tree replaced"));
	FollowTree = InFollowTree;
	if (HasActorBegunPlay() && GetPawn() != nullptr)
	{
		StartFollowing();
	}
}

void AGolemCompanionController::BeginPlay()
{
	Super::BeginPlay();
	StartFollowing();
}

void AGolemCompanionController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	if (HasActorBegunPlay())
	{
		StartFollowing();
	}
}

void AGolemCompanionController::OnUnPossess()
{
	StopFollowing(TEXT("Golem controller ownership changed"));
	Super::OnUnPossess();
}

void AGolemCompanionController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopFollowing(TEXT("Companion controller ended"));
	Super::EndPlay(EndPlayReason);
}

void AGolemCompanionController::StartFollowing()
{
	const AGolemCharacter* Golem = Cast<AGolemCharacter>(GetPawn());
	if (!HasAuthority() || Golem == nullptr || FollowComponent->IsRunning())
	{
		return;
	}
	UStateTree* Tree = FollowTree.LoadSynchronous();
	if (Tree == nullptr || !Tree->IsReadyToRun())
	{
		UE_LOG(LogGolemCompanion, Error, TEXT("Follow StateTree is missing or uncompiled: %s."),
			*FollowTree.ToSoftObjectPath().ToString());
		return;
	}
	FollowComponent->SetStateTree(Tree);
	// A reclaimed downed Golem still needs its tree configured for the eventual life-state restart.
	if (!Golem->CanTransferControl())
	{
		return;
	}
	FollowComponent->StartLogic();
	if (!FollowComponent->IsRunning())
	{
		UE_LOG(LogGolemCompanion, Error, TEXT("Follow StateTree did not start for %s."), *GetName());
	}
}
