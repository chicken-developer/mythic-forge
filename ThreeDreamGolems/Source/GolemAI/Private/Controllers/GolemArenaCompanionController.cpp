#include "Controllers/GolemArenaCompanionController.h"

#include "AbilitySystemGlobals.h"
#include "Assets/GolemAssetManager.h"
#include "Character/GolemCharacter.h"
#include "ColorAssist/GolemColorAssistComponent.h"
#include "Components/GolemAbilitySystemComponent.h"
#include "Components/GolemAIDecisionSchedulerComponent.h"
#include "Components/GolemLifeComponent.h"
#include "Components/StateTreeAIComponent.h"
#include "Context/GolemAIContextComponent.h"
#include "Definitions/GolemAIArchetypeDefinition.h"
#include "Definitions/GolemHeroDefinition.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Perception/GolemAIRolePerceptionComponent.h"
#include "StateTree.h"
#include "Tags/GolemGameplayTags.h"
#include "Threat/GolemThreatComponent.h"
#include "Components/GolemCombatComponent.h"
#include "Xenon/GolemXenonBrainComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogGolemArenaCompanion, Log, All);

AGolemArenaCompanionController::AGolemArenaCompanionController()
{
	AIContext = CreateDefaultSubobject<UGolemAIContextComponent>(TEXT("ArenaAIContext"));
	RolePerception = CreateDefaultSubobject<UGolemAIRolePerceptionComponent>(TEXT("ArenaRolePerception"));
	DecisionScheduler = CreateDefaultSubobject<UGolemAIDecisionSchedulerComponent>(TEXT("ArenaDecisionScheduler"));
	ColorAssist = CreateDefaultSubobject<UGolemColorAssistComponent>(TEXT("ArenaColorAssist"));
	SetPerceptionComponent(*RolePerception);
	GetFollowComponent()->PrimaryComponentTick.bCanEverTick = false;
}

void AGolemArenaCompanionController::SetPawn(APawn* InPawn)
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
			UE_LOG(LogGolemArenaCompanion, Error, TEXT("Companion context initialization failed [%s]: %s."),
				*ArenaInitializationResult.Context.ToString(), *ArenaInitializationResult.Message.ToString());
		}
	}
}

FGolemResult AGolemArenaCompanionController::RefreshArenaConfiguration()
{
	if (!HasAuthority())
	{
		return FGolemResult::Failure(EGolemError::NotAuthority, TEXT("RefreshArenaCompanion"),
			NSLOCTEXT("GolemAI", "ArenaRefreshAuthority", "Companion configuration belongs to the server."));
	}
	if (bEndingPlay || GetWorld() == nullptr || GetWorld()->bIsTearingDown || GetPawn() == nullptr)
	{
		return FGolemResult::Failure(EGolemError::InvalidState, TEXT("RefreshArenaCompanion"),
			NSLOCTEXT("GolemAI", "ArenaRefreshState", "Companion configuration requires its live owned Golem."));
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
				ArenaInitializationResult = FGolemResult::Failure(EGolemError::InvalidState, TEXT("CompanionStateTree"),
					NSLOCTEXT("GolemAI", "ArenaTreeStart", "The configured companion StateTree did not enter a running state."));
			}
			else
			{
				ArenaInitializationResult = DecisionScheduler->StartScheduling(GetFollowComponent(), AIContext);
			}
		}
	}
	return ArenaInitializationResult;
}

FGolemResult AGolemArenaCompanionController::TrySetCommand(EGolemCompanionCommand Command, AActor* FocusTarget)
{
	AGolemCharacter* Golem = Cast<AGolemCharacter>(GetPawn());
	if (!HasAuthority())
	{
		return FGolemResult::Failure(EGolemError::NotAuthority, TEXT("CompanionCommand"),
			NSLOCTEXT("GolemAI", "CommandAuthority", "Companion commands are validated by the server."));
	}
	if (bEndingPlay || GetWorld() == nullptr || GetWorld()->bIsTearingDown || Golem == nullptr
		|| Golem->GetController() != this || !Golem->GetLifeComponent()->IsAlive()
		|| Archetype == nullptr || !ArenaInitializationResult.IsSuccess()
		|| static_cast<uint8>(Command) > static_cast<uint8>(EGolemCompanionCommand::Regroup)
		|| (Command != EGolemCompanionCommand::Focus && FocusTarget != nullptr))
	{
		return FGolemResult::Failure(EGolemError::InvalidState, TEXT("CompanionCommand"),
			NSLOCTEXT("GolemAI", "CommandState", "Command requires a live AI companion and a valid command type."));
	}
	UGolemAbilitySystemComponent* TargetASC = IsValid(FocusTarget)
		? Cast<UGolemAbilitySystemComponent>(UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(FocusTarget)) : nullptr;
	if (Command == EGolemCompanionCommand::Focus
		&& (!IsValid(FocusTarget) || FocusTarget == Golem || FocusTarget->IsActorBeingDestroyed()
			|| FocusTarget->GetWorld() != GetWorld() || TargetASC == nullptr || !TargetASC->GetEntityId().IsValid()
			|| !TargetASC->HasMatchingGameplayTag(GolemGameplayTags::Role_Enemy)
			|| TargetASC->GetLifeComponent() == nullptr || !TargetASC->GetLifeComponent()->IsAlive()
			|| FVector::DistSquared(Golem->GetActorLocation(), FocusTarget->GetActorLocation())
				> FMath::Square(Archetype->GetTuning().LoseSightRadius)))
	{
		return FGolemResult::Failure(EGolemError::InvalidData, TEXT("CompanionCommand"),
			NSLOCTEXT("GolemAI", "CommandFocus", "Focus requires a living nearby enemy with a stable identity."));
	}
	const FGolemEntityId FocusId = TargetASC != nullptr ? TargetASC->GetEntityId() : FGolemEntityId();
	if (CommandSnapshot.Command == Command && CommandSnapshot.FocusTargetId == FocusId)
	{
		return FGolemResult::Success();
	}
	if (CommandSnapshot.CommandRevision == MAX_int32)
	{
		return FGolemResult::Failure(EGolemError::Conflict, TEXT("CompanionCommand"),
			NSLOCTEXT("GolemAI", "CommandRevision", "Companion command revision is exhausted."));
	}
	const FGolemResult Result = AIContext->SetCompanionCommand(Command, FocusTarget);
	if (!Result.IsSuccess())
	{
		return Result;
	}
	StopMovement();
	CommandSnapshot.Command = Command;
	CommandSnapshot.FocusTargetId = FocusId;
	CommandSnapshot.HoldLocation = Command == EGolemCompanionCommand::Hold
		? Golem->GetActorLocation() : FVector::ZeroVector;
	++CommandSnapshot.CommandRevision;
	DecisionScheduler->RequestDecisionWake();
	OnCommandChanged.Broadcast(CommandSnapshot);
	return FGolemResult::Success();
}

UGolemAIContextComponent* AGolemArenaCompanionController::GetAIContext() const
{
	return AIContext;
}

UGolemAIRolePerceptionComponent* AGolemArenaCompanionController::GetRolePerception() const
{
	return RolePerception;
}

UGolemAIDecisionSchedulerComponent* AGolemArenaCompanionController::GetDecisionScheduler() const
{
	return DecisionScheduler;
}

UGolemColorAssistComponent* AGolemArenaCompanionController::GetColorAssist() const
{
	return ColorAssist;
}

const FGolemCompanionCommandSnapshot& AGolemArenaCompanionController::GetCommandSnapshot() const
{
	return CommandSnapshot;
}

const FGolemResult& AGolemArenaCompanionController::GetArenaInitializationResult() const
{
	return ArenaInitializationResult;
}

void AGolemArenaCompanionController::BeginPlay()
{
	PerceptionHandle = RolePerception->OnRolePerceptionChanged.AddUObject(this, &ThisClass::HandleRolePerception);
	ScheduleHandle = DecisionScheduler->OnDecisionScheduled.AddUObject(this, &ThisClass::HandleDecisionScheduled);
	ColorAssistHandle = ColorAssist->OnJobChanged.AddUObject(this, &ThisClass::HandleColorAssistJob);
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
		UE_LOG(LogGolemArenaCompanion, Error, TEXT("Companion BeginPlay initialization failed [%s]: %s."),
			*ArenaInitializationResult.Context.ToString(), *ArenaInitializationResult.Message.ToString());
	}
}

void AGolemArenaCompanionController::OnPossess(APawn* InPawn)
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

void AGolemArenaCompanionController::OnUnPossess()
{
	StopArenaDecisions();
	Super::OnUnPossess();
}

void AGolemArenaCompanionController::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	bEndingPlay = true;
	StopArenaDecisions();
	RolePerception->OnRolePerceptionChanged.Remove(PerceptionHandle);
	DecisionScheduler->OnDecisionScheduled.Remove(ScheduleHandle);
	ColorAssist->OnJobChanged.Remove(ColorAssistHandle);
	ColorAssistHandle.Reset();
	PerceptionHandle.Reset();
	ScheduleHandle.Reset();
	OnCommandChanged.Clear();
	Super::EndPlay(EndPlayReason);
}

FGolemResult AGolemArenaCompanionController::ResolveArenaArchetype()
{
	if (Archetype != nullptr)
	{
		return FGolemResult::Success();
	}
	UGolemAssetManager* Manager = UGolemAssetManager::GetGolemAssetManager();
	UGolemDefinition* Definition = nullptr;
	const FGolemResult Result = Manager != nullptr
		? Manager->ResolveDefinition(FPrimaryAssetId(TEXT("GolemAIArchetype"), TEXT("ArenaCompanion")),
			UGolemAIArchetypeDefinition::StaticClass(), Definition)
		: FGolemResult::Failure(EGolemError::InvalidState, TEXT("CompanionArchetype"),
			NSLOCTEXT("GolemAI", "CommandManager", "The definition catalog is unavailable."));
	if (!Result.IsSuccess())
	{
		return Result;
	}
	UGolemAIArchetypeDefinition* Resolved = CastChecked<UGolemAIArchetypeDefinition>(Definition);
	const FGolemResult Validation = Resolved->ValidateArchetype();
	if (!Validation.IsSuccess())
	{
		return Validation;
	}
	Archetype = Resolved;
	return FGolemResult::Success();
}

FGolemResult AGolemArenaCompanionController::InitializeArenaPawn(APawn* InPawn)
{
	FGolemResult Result = ResolveArenaArchetype();
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
		ColorAssist->Configure(Archetype->GetTuning().ColorAssist);
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
		const FGolemEntityId ControlledId = AIContext->GetDecisionSnapshot().PawnId;
		if (CommandSnapshot.GolemId != ControlledId)
		{
			CommandSnapshot.GolemId = ControlledId;
			CommandSnapshot.Command = EGolemCompanionCommand::Follow;
			CommandSnapshot.FocusTargetId = FGolemEntityId();
			CommandSnapshot.HoldLocation = FVector::ZeroVector;
		}
		RefreshFormationLeader();
	}
	return Result;
}

void AGolemArenaCompanionController::RefreshFormationLeader()
{
	const AGolemCharacter* Golem = Cast<AGolemCharacter>(GetPawn());
	if (bEndingPlay || Golem == nullptr || GetWorld() == nullptr || GetWorld()->bIsTearingDown)
	{
		return;
	}
	const UGolemHeroDefinition* Hero = Golem->GetHeroDefinition();
	if (Hero == nullptr)
	{
		return;
	}
	AActor* Leader = nullptr;
	double DistanceSquared = TNumericLimits<double>::Max();
	for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		APlayerController* Human = Iterator->Get();
		AGolemCharacter* Candidate = Human != nullptr ? Cast<AGolemCharacter>(Human->GetPawn()) : nullptr;
		if (Candidate != nullptr && Candidate != Golem && Candidate->GetLifeComponent()->IsAlive())
		{
			const double CandidateDistance = FVector::DistSquared(Golem->GetActorLocation(), Candidate->GetActorLocation());
			if (CandidateDistance < DistanceSquared)
			{
				DistanceSquared = CandidateDistance;
				Leader = Candidate;
			}
		}
	}
	AIContext->SetFormationLeader(Leader, Hero->GetFormationOffset(), Hero->GetFollowDistance());
}

void AGolemArenaCompanionController::StopArenaDecisions()
{
	ColorAssist->ResetAssist();
	DecisionScheduler->StopScheduling();
	StopFollowing(TEXT("Arena controller ownership ended"));
	AIContext->ClearControlledPawn();
	RolePerception->ClearRoleSense();
}

void AGolemArenaCompanionController::HandleRolePerception(AActor* Actor, bool bSensed)
{
	if (bEndingPlay || GetPawn() == nullptr || AIContext->GetControlledPawn() != GetPawn())
	{
		return;
	}
	const UGolemAbilitySystemComponent* ASC = IsValid(Actor) ? Cast<UGolemAbilitySystemComponent>(
		UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Actor)) : nullptr;
	if (ASC != nullptr && ASC->HasMatchingGameplayTag(GolemGameplayTags::Role_Enemy))
	{
		AIContext->SetPerceivedActor(Actor, bSensed);
		if (!bSensed)
		{
			if (UGolemThreatComponent* Threat = GetPawn()->FindComponentByClass<UGolemThreatComponent>())
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

void AGolemArenaCompanionController::HandleColorAssistJob(const bool bActive)
{
	if (bEndingPlay || GetPawn() == nullptr)
	{
		return;
	}
	if (bActive)
	{
		// The colour job owns movement and the heavy attack until it ends; ordinary decisions wait.
		AIContext->CancelOwnedAction();
		StopMovement();
		GetFollowComponent()->PauseLogic(TEXT("Colour assist"));
	}
	else
	{
		GetFollowComponent()->ResumeLogic(TEXT("Colour assist"));
		DecisionScheduler->RequestDecisionWake();
	}
}

void AGolemArenaCompanionController::DodgeTelegraphedAttack()
{
	// Ordinary fighting (priority 4): step out of an attack a Xenon is showing. A colour job is not interrupted.
	AGolemCharacter* Golem = Cast<AGolemCharacter>(GetPawn());
	if (Golem == nullptr || ColorAssist->HasJob() || !Golem->GetLifeComponent()->IsAlive())
	{
		return;
	}
	const AActor* Threat = UGolemXenonBrainComponent::FindThreat(GetWorld(), Golem->GetActorLocation());
	UGolemCombatComponent* Combat = Golem->GetCombatComponent();
	if (Threat == nullptr || Combat == nullptr || Combat->GetProfile() == nullptr
		|| Combat->GetDodgeCooldownRemaining(GetWorld()->GetTimeSeconds()) > 0.0f)
	{
		return;
	}
	AIContext->CancelOwnedAction();
	Combat->SetNextDodgeDirection(Golem->GetActorLocation() - Threat->GetActorLocation());
	if (Golem->GetGolemAbilitySystem()->RequestAbilityInput(GolemGameplayTags::Input_Combat_Dodge, true).IsSuccess())
	{
		Golem->GetGolemAbilitySystem()->RequestAbilityInput(GolemGameplayTags::Input_Combat_Dodge, false);
	}
}

void AGolemArenaCompanionController::HandleDecisionScheduled(EGolemAILODTier Tier, float Interval, int64 TickCount)
{
	// Reviving a downed teammate outranks colour help (DEV_SPEC section 12, priority 1).
	ColorAssist->SetBlocked(AIContext->GetDecisionSnapshot().bTargetFriendlyDowned);
	DodgeTelegraphedAttack();
	RefreshFormationLeader();
	CommandSnapshot.DecisionLOD = Tier;
	CommandSnapshot.DecisionInterval = Interval;
	CommandSnapshot.DecisionTickCount = TickCount;
	OnCommandChanged.Broadcast(CommandSnapshot);
}
