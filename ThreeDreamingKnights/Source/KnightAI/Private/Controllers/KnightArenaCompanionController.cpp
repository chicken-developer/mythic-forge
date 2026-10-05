#include "Controllers/KnightArenaCompanionController.h"

#include "AbilitySystemGlobals.h"
#include "Assets/KnightAssetManager.h"
#include "Character/KnightCharacter.h"
#include "Components/KnightAbilitySystemComponent.h"
#include "Components/KnightAIDecisionSchedulerComponent.h"
#include "Components/KnightLifeComponent.h"
#include "Components/StateTreeAIComponent.h"
#include "Context/KnightAIContextComponent.h"
#include "Definitions/KnightAIArchetypeDefinition.h"
#include "Definitions/KnightHeroDefinition.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Perception/KnightAIRolePerceptionComponent.h"
#include "StateTree.h"
#include "Tags/KnightGameplayTags.h"
#include "Threat/KnightThreatComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogKnightArenaCompanion, Log, All);

AKnightArenaCompanionController::AKnightArenaCompanionController()
{
	AIContext = CreateDefaultSubobject<UKnightAIContextComponent>(TEXT("ArenaAIContext"));
	RolePerception = CreateDefaultSubobject<UKnightAIRolePerceptionComponent>(TEXT("ArenaRolePerception"));
	DecisionScheduler = CreateDefaultSubobject<UKnightAIDecisionSchedulerComponent>(TEXT("ArenaDecisionScheduler"));
	SetPerceptionComponent(*RolePerception);
	GetFollowComponent()->PrimaryComponentTick.bCanEverTick = false;
}

void AKnightArenaCompanionController::SetPawn(APawn* InPawn)
{
	if (GetPawn() != InPawn)
	{
		StopArenaDecisions();
	}
	Super::SetPawn(InPawn);
	if (HasAuthority() && InPawn != nullptr && !bEndingPlay)
	{
		ArenaInitializationResult = AIContext->InitializeControlledPawn(InPawn);
		if (ArenaInitializationResult.IsSuccess() && GetWorld() != nullptr && GetWorld()->HasBegunPlay())
		{
			ArenaInitializationResult = InitializeArenaPawn(InPawn);
		}
		if (!ArenaInitializationResult.IsSuccess())
		{
			UE_LOG(LogKnightArenaCompanion, Error, TEXT("Companion context initialization failed [%s]: %s."),
				*ArenaInitializationResult.Context.ToString(), *ArenaInitializationResult.Message.ToString());
		}
	}
}

FKnightResult AKnightArenaCompanionController::RefreshArenaConfiguration()
{
	if (!HasAuthority())
	{
		return FKnightResult::Failure(EKnightError::NotAuthority, TEXT("RefreshArenaCompanion"),
			NSLOCTEXT("KnightAI", "ArenaRefreshAuthority", "Companion configuration belongs to the server."));
	}
	if (bEndingPlay || GetWorld() == nullptr || GetWorld()->bIsTearingDown || GetPawn() == nullptr)
	{
		return FKnightResult::Failure(EKnightError::InvalidState, TEXT("RefreshArenaCompanion"),
			NSLOCTEXT("KnightAI", "ArenaRefreshState", "Companion configuration requires its live owned Knight."));
	}
	DecisionScheduler->StopScheduling();
	StopFollowing(TEXT("Arena companion configuration refreshed"));
	ArenaInitializationResult = InitializeArenaPawn(GetPawn());
	if (ArenaInitializationResult.IsSuccess())
	{
		SetFollowTree(Archetype->GetStateTree());
		if (HasActorBegunPlay())
		{
			if (AIContext->GetDecisionSnapshot().bAlive && !GetFollowComponent()->IsRunning())
			{
				ArenaInitializationResult = FKnightResult::Failure(EKnightError::InvalidState, TEXT("CompanionStateTree"),
					NSLOCTEXT("KnightAI", "ArenaTreeStart", "The configured companion StateTree did not enter a running state."));
			}
			else
			{
				ArenaInitializationResult = DecisionScheduler->StartScheduling(GetFollowComponent(), AIContext);
			}
		}
	}
	return ArenaInitializationResult;
}

FKnightResult AKnightArenaCompanionController::TrySetCommand(EKnightCompanionCommand Command, AActor* FocusTarget)
{
	AKnightCharacter* Knight = Cast<AKnightCharacter>(GetPawn());
	if (!HasAuthority())
	{
		return FKnightResult::Failure(EKnightError::NotAuthority, TEXT("CompanionCommand"),
			NSLOCTEXT("KnightAI", "CommandAuthority", "Companion commands are validated by the server."));
	}
	if (bEndingPlay || GetWorld() == nullptr || GetWorld()->bIsTearingDown || Knight == nullptr
		|| Knight->GetController() != this || !Knight->GetLifeComponent()->IsAlive()
		|| Archetype == nullptr || !ArenaInitializationResult.IsSuccess()
		|| static_cast<uint8>(Command) > static_cast<uint8>(EKnightCompanionCommand::Regroup)
		|| (Command != EKnightCompanionCommand::Focus && FocusTarget != nullptr))
	{
		return FKnightResult::Failure(EKnightError::InvalidState, TEXT("CompanionCommand"),
			NSLOCTEXT("KnightAI", "CommandState", "Command requires a live AI companion and a valid command type."));
	}
	UKnightAbilitySystemComponent* TargetASC = IsValid(FocusTarget)
		? Cast<UKnightAbilitySystemComponent>(UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(FocusTarget)) : nullptr;
	if (Command == EKnightCompanionCommand::Focus
		&& (!IsValid(FocusTarget) || FocusTarget == Knight || FocusTarget->IsActorBeingDestroyed()
			|| FocusTarget->GetWorld() != GetWorld() || TargetASC == nullptr || !TargetASC->GetEntityId().IsValid()
			|| !TargetASC->HasMatchingGameplayTag(KnightGameplayTags::Role_Enemy)
			|| TargetASC->GetLifeComponent() == nullptr || !TargetASC->GetLifeComponent()->IsAlive()
			|| FVector::DistSquared(Knight->GetActorLocation(), FocusTarget->GetActorLocation())
				> FMath::Square(Archetype->GetTuning().LoseSightRadius)))
	{
		return FKnightResult::Failure(EKnightError::InvalidData, TEXT("CompanionCommand"),
			NSLOCTEXT("KnightAI", "CommandFocus", "Focus requires a living nearby enemy with a stable identity."));
	}
	const FKnightEntityId FocusId = TargetASC != nullptr ? TargetASC->GetEntityId() : FKnightEntityId();
	if (CommandSnapshot.Command == Command && CommandSnapshot.FocusTargetId == FocusId)
	{
		return FKnightResult::Success();
	}
	if (CommandSnapshot.CommandRevision == MAX_int32)
	{
		return FKnightResult::Failure(EKnightError::Conflict, TEXT("CompanionCommand"),
			NSLOCTEXT("KnightAI", "CommandRevision", "Companion command revision is exhausted."));
	}
	const FKnightResult Result = AIContext->SetCompanionCommand(Command, FocusTarget);
	if (!Result.IsSuccess())
	{
		return Result;
	}
	StopMovement();
	CommandSnapshot.Command = Command;
	CommandSnapshot.FocusTargetId = FocusId;
	CommandSnapshot.HoldLocation = Command == EKnightCompanionCommand::Hold
		? Knight->GetActorLocation() : FVector::ZeroVector;
	++CommandSnapshot.CommandRevision;
	DecisionScheduler->RequestDecisionWake();
	OnCommandChanged.Broadcast(CommandSnapshot);
	return FKnightResult::Success();
}

UKnightAIContextComponent* AKnightArenaCompanionController::GetAIContext() const
{
	return AIContext;
}

UKnightAIRolePerceptionComponent* AKnightArenaCompanionController::GetRolePerception() const
{
	return RolePerception;
}

UKnightAIDecisionSchedulerComponent* AKnightArenaCompanionController::GetDecisionScheduler() const
{
	return DecisionScheduler;
}

const FKnightCompanionCommandSnapshot& AKnightArenaCompanionController::GetCommandSnapshot() const
{
	return CommandSnapshot;
}

const FKnightResult& AKnightArenaCompanionController::GetArenaInitializationResult() const
{
	return ArenaInitializationResult;
}

void AKnightArenaCompanionController::BeginPlay()
{
	PerceptionHandle = RolePerception->OnRolePerceptionChanged.AddUObject(this, &ThisClass::HandleRolePerception);
	ScheduleHandle = DecisionScheduler->OnDecisionScheduled.AddUObject(this, &ThisClass::HandleDecisionScheduled);
	ArenaInitializationResult = GetPawn() != nullptr ? InitializeArenaPawn(GetPawn()) : ResolveArenaArchetype();
	if (ArenaInitializationResult.IsSuccess())
	{
		SetFollowTree(Archetype->GetStateTree());
	}
	Super::BeginPlay();
	if (GetPawn() != nullptr && ArenaInitializationResult.IsSuccess())
	{
		ArenaInitializationResult = DecisionScheduler->StartScheduling(GetFollowComponent(), AIContext);
	}
	else if (!ArenaInitializationResult.IsSuccess())
	{
		StopFollowing(TEXT("Arena configuration unavailable"));
		UE_LOG(LogKnightArenaCompanion, Error, TEXT("Companion BeginPlay initialization failed [%s]: %s."),
			*ArenaInitializationResult.Context.ToString(), *ArenaInitializationResult.Message.ToString());
	}
}

void AKnightArenaCompanionController::OnPossess(APawn* InPawn)
{
	ArenaInitializationResult = ResolveArenaArchetype();
	if (ArenaInitializationResult.IsSuccess())
	{
		SetFollowTree(Archetype->GetStateTree());
	}
	Super::OnPossess(InPawn);
	if (HasActorBegunPlay() && ArenaInitializationResult.IsSuccess())
	{
		ArenaInitializationResult = RefreshArenaConfiguration();
	}
	if (!ArenaInitializationResult.IsSuccess())
	{
		StopFollowing(TEXT("Arena possession configuration unavailable"));
	}
}

void AKnightArenaCompanionController::OnUnPossess()
{
	StopArenaDecisions();
	Super::OnUnPossess();
}

void AKnightArenaCompanionController::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	bEndingPlay = true;
	StopArenaDecisions();
	RolePerception->OnRolePerceptionChanged.Remove(PerceptionHandle);
	DecisionScheduler->OnDecisionScheduled.Remove(ScheduleHandle);
	PerceptionHandle.Reset();
	ScheduleHandle.Reset();
	OnCommandChanged.Clear();
	Super::EndPlay(EndPlayReason);
}

FKnightResult AKnightArenaCompanionController::ResolveArenaArchetype()
{
	if (Archetype != nullptr)
	{
		return FKnightResult::Success();
	}
	UKnightAssetManager* Manager = UKnightAssetManager::GetKnightAssetManager();
	UKnightDefinition* Definition = nullptr;
	const FKnightResult Result = Manager != nullptr
		? Manager->ResolveDefinition(FPrimaryAssetId(TEXT("KnightAIArchetype"), TEXT("ArenaCompanion")),
			UKnightAIArchetypeDefinition::StaticClass(), Definition)
		: FKnightResult::Failure(EKnightError::InvalidState, TEXT("CompanionArchetype"),
			NSLOCTEXT("KnightAI", "CommandManager", "The definition catalog is unavailable."));
	if (!Result.IsSuccess())
	{
		return Result;
	}
	UKnightAIArchetypeDefinition* Resolved = CastChecked<UKnightAIArchetypeDefinition>(Definition);
	const FKnightResult Validation = Resolved->ValidateArchetype();
	if (!Validation.IsSuccess())
	{
		return Validation;
	}
	Archetype = Resolved;
	return FKnightResult::Success();
}

FKnightResult AKnightArenaCompanionController::InitializeArenaPawn(APawn* InPawn)
{
	FKnightResult Result = ResolveArenaArchetype();
	if (Result.IsSuccess())
	{
		Result = AIContext->InitializeControlledPawn(InPawn);
	}
	if (Result.IsSuccess())
	{
		Result = AIContext->SetActionSet(Archetype->GetActionSetId());
	}
	if (Result.IsSuccess())
	{
		Result = AIContext->SetRepositionQuery(Archetype->GetRepositionQuery());
	}
	if (Result.IsSuccess())
	{
		Result = AIContext->SetAttackLeaseDuration(Archetype->GetTuning().AttackLeaseDuration);
	}
	if (Result.IsSuccess())
	{
		Result = RolePerception->ConfigureRoleSense(Archetype->GetTuning());
	}
	if (Result.IsSuccess())
	{
		Result = RolePerception->InitializeRoleSense(InPawn);
	}
	if (Result.IsSuccess())
	{
		const FKnightEntityId ControlledId = AIContext->GetDecisionSnapshot().PawnId;
		if (CommandSnapshot.KnightId != ControlledId)
		{
			CommandSnapshot.KnightId = ControlledId;
			CommandSnapshot.Command = EKnightCompanionCommand::Follow;
			CommandSnapshot.FocusTargetId = FKnightEntityId();
			CommandSnapshot.HoldLocation = FVector::ZeroVector;
		}
		RefreshFormationLeader();
	}
	return Result;
}

void AKnightArenaCompanionController::RefreshFormationLeader()
{
	const AKnightCharacter* Knight = Cast<AKnightCharacter>(GetPawn());
	if (bEndingPlay || Knight == nullptr || GetWorld() == nullptr || GetWorld()->bIsTearingDown)
	{
		return;
	}
	const UKnightHeroDefinition* Hero = Knight->GetHeroDefinition();
	if (Hero == nullptr)
	{
		return;
	}
	AActor* Leader = nullptr;
	double DistanceSquared = TNumericLimits<double>::Max();
	for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		APlayerController* Human = Iterator->Get();
		AKnightCharacter* Candidate = Human != nullptr ? Cast<AKnightCharacter>(Human->GetPawn()) : nullptr;
		if (Candidate != nullptr && Candidate != Knight && Candidate->GetLifeComponent()->IsAlive())
		{
			const double CandidateDistance = FVector::DistSquared(Knight->GetActorLocation(), Candidate->GetActorLocation());
			if (CandidateDistance < DistanceSquared)
			{
				DistanceSquared = CandidateDistance;
				Leader = Candidate;
			}
		}
	}
	AIContext->SetFormationLeader(Leader, Hero->GetFormationOffset(), Hero->GetFollowDistance());
}

void AKnightArenaCompanionController::StopArenaDecisions()
{
	DecisionScheduler->StopScheduling();
	StopFollowing(TEXT("Arena controller ownership ended"));
	AIContext->ClearControlledPawn();
	RolePerception->ClearRoleSense();
}

void AKnightArenaCompanionController::HandleRolePerception(AActor* Actor, bool bSensed)
{
	if (bEndingPlay || GetPawn() == nullptr || AIContext->GetControlledPawn() != GetPawn())
	{
		return;
	}
	const UKnightAbilitySystemComponent* ASC = IsValid(Actor) ? Cast<UKnightAbilitySystemComponent>(
		UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Actor)) : nullptr;
	if (ASC != nullptr && ASC->HasMatchingGameplayTag(KnightGameplayTags::Role_Enemy))
	{
		AIContext->SetPerceivedActor(Actor, bSensed);
		if (!bSensed)
		{
			if (UKnightThreatComponent* Threat = GetPawn()->FindComponentByClass<UKnightThreatComponent>())
			{
				Threat->ForgetTarget(Actor);
			}
		}
	}
	else
	{
		AIContext->RefreshDecisionContext();
	}
	if (!bSensed && AIContext->GetDecisionSnapshot().Target.Get() == Actor)
	{
		AIContext->ClearTarget();
	}
	DecisionScheduler->RequestDecisionWake();
}

void AKnightArenaCompanionController::HandleDecisionScheduled(EKnightAILODTier Tier, float Interval, int64 TickCount)
{
	RefreshFormationLeader();
	CommandSnapshot.DecisionLOD = Tier;
	CommandSnapshot.DecisionInterval = Interval;
	CommandSnapshot.DecisionTickCount = TickCount;
	OnCommandChanged.Broadcast(CommandSnapshot);
}
