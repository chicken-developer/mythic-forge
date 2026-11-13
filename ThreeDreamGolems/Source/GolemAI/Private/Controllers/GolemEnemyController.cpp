#include "Controllers/GolemEnemyController.h"

#include "AbilitySystemGlobals.h"
#include "Assets/GolemAssetManager.h"
#include "Boss/GolemBossPhaseComponent.h"
#include "Components/GolemAbilitySystemComponent.h"
#include "Components/GolemAIDecisionSchedulerComponent.h"
#include "Components/GolemLifeComponent.h"
#include "Components/StateTreeAIComponent.h"
#include "Context/GolemAIContextComponent.h"
#include "Definitions/GolemAIArchetypeDefinition.h"
#include "Engine/World.h"
#include "Perception/GolemAIRolePerceptionComponent.h"
#include "StateTree.h"
#include "Threat/GolemThreatComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogGolemEnemyController, Log, All);

AGolemEnemyController::AGolemEnemyController()
{
	AIContext = CreateDefaultSubobject<UGolemAIContextComponent>(TEXT("EnemyAIContext"));
	DecisionTree = CreateDefaultSubobject<UStateTreeAIComponent>(TEXT("EnemyStateTree"));
	RolePerception = CreateDefaultSubobject<UGolemAIRolePerceptionComponent>(TEXT("EnemyRolePerception"));
	DecisionScheduler = CreateDefaultSubobject<UGolemAIDecisionSchedulerComponent>(TEXT("EnemyDecisionScheduler"));
	DecisionTree->SetStartLogicAutomatically(false);
	DecisionTree->PrimaryComponentTick.bCanEverTick = false;
	BrainComponent = DecisionTree;
	bStartAILogicOnPossess = false;
	SetPerceptionComponent(*RolePerception);
	PerceptionHandle = RolePerception->OnRolePerceptionChanged.AddUObject(this, &ThisClass::HandleRolePerception);
	ContextHandle = AIContext->OnDecisionChanged.AddUObject(this, &ThisClass::HandleContextChanged);
}

FGolemResult AGolemEnemyController::ConfigureArchetype(const FPrimaryAssetId& InArchetypeId)
{
	if (!HasAuthority())
	{
		return FGolemResult::Failure(EGolemError::NotAuthority, TEXT("EnemyArchetype"),
			NSLOCTEXT("GolemAI", "EnemyArchetypeAuthority", "Enemy archetypes are configured by the server."));
	}
	if (bEndingPlay || GetWorld() == nullptr || GetWorld()->bIsTearingDown)
	{
		return FGolemResult::Failure(EGolemError::ShuttingDown, TEXT("EnemyArchetype"),
			NSLOCTEXT("GolemAI", "EnemyArchetypeEnding", "Enemy controller is shutting down."));
	}
	UGolemAssetManager* Manager = UGolemAssetManager::GetGolemAssetManager();
	UGolemDefinition* Definition = nullptr;
	const FGolemResult Result = Manager != nullptr
		? Manager->ResolveDefinition(InArchetypeId, UGolemAIArchetypeDefinition::StaticClass(), Definition)
		: FGolemResult::Failure(EGolemError::InvalidState, TEXT("EnemyArchetype"),
			NSLOCTEXT("GolemAI", "EnemyCatalog", "The enemy definition catalog is unavailable."));
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
	if (ArchetypeId == InArchetypeId && Archetype == Resolved && DecisionTree->IsRunning())
	{
		return FGolemResult::Success();
	}
	DecisionScheduler->StopScheduling();
	DecisionTree->StopLogic(TEXT("Enemy archetype changed"));
	AIContext->CancelOwnedAction();
	StopMovement();
	Archetype = Resolved;
	ArchetypeId = InArchetypeId;
	InitializationResult = GetPawn() != nullptr ? ApplyArchetype() : FGolemResult::Success();
	return InitializationResult;
}

UGolemAIContextComponent* AGolemEnemyController::GetAIContext() const
{
	return AIContext;
}

UGolemAIRolePerceptionComponent* AGolemEnemyController::GetRolePerception() const
{
	return RolePerception;
}

UGolemAIDecisionSchedulerComponent* AGolemEnemyController::GetDecisionScheduler() const
{
	return DecisionScheduler;
}

UStateTreeAIComponent* AGolemEnemyController::GetDecisionTree() const
{
	return DecisionTree;
}

FPrimaryAssetId AGolemEnemyController::GetArchetypeId() const
{
	return ArchetypeId;
}

EGolemAILODTier AGolemEnemyController::GetDecisionLOD() const
{
	return DecisionScheduler->GetDecisionLOD();
}

int64 AGolemEnemyController::GetDecisionTickCount() const
{
	return DecisionScheduler->GetDecisionTickCount();
}

const FGolemResult& AGolemEnemyController::GetInitializationResult() const
{
	return InitializationResult;
}

void AGolemEnemyController::OnPossess(APawn* InPawn)
{
	StopEnemyDecisions();
	Super::OnPossess(InPawn);
	InitializationResult = AIContext->InitializeControlledPawn(InPawn);
	if (InitializationResult.IsSuccess())
	{
		UGolemAbilitySystemComponent* ASC = AIContext->GetAbilitySystem();
		PawnLife = ASC->GetLifeComponent();
		LifeHandle = PawnLife->OnLifeStateChanged.AddUObject(this, &ThisClass::HandlePawnLifeChanged);
		if (Archetype != nullptr)
		{
			InitializationResult = ApplyArchetype();
		}
	}
	if (!InitializationResult.IsSuccess())
	{
		UE_LOG(LogGolemEnemyController, Error, TEXT("Enemy possession initialization failed [%s]: %s."),
			*InitializationResult.Context.ToString(), *InitializationResult.Message.ToString());
	}
}

void AGolemEnemyController::OnUnPossess()
{
	StopEnemyDecisions();
	Super::OnUnPossess();
}

void AGolemEnemyController::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	bEndingPlay = true;
	StopEnemyDecisions();
	RolePerception->OnRolePerceptionChanged.Remove(PerceptionHandle);
	AIContext->OnDecisionChanged.Remove(ContextHandle);
	PerceptionHandle.Reset();
	ContextHandle.Reset();
	OnDecisionChanged.Clear();
	Super::EndPlay(EndPlayReason);
}

FGolemResult AGolemEnemyController::ApplyArchetype()
{
	FGolemResult Result = AIContext->InitializeControlledPawn(GetPawn());
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
		Result = RolePerception->InitializeRoleSense(GetPawn());
	}
	if (!Result.IsSuccess())
	{
		return Result;
	}
	UGolemBossPhaseComponent* Boss = GetPawn()->FindComponentByClass<UGolemBossPhaseComponent>();
	if (Boss != nullptr && Boss->GetDefinitionId().IsValid())
	{
		Result = Boss->BindContext(AIContext);
		if (!Result.IsSuccess())
		{
			return Result;
		}
	}
	DecisionTree->SetStateTree(Archetype->GetStateTree());
	AIContext->RefreshDecisionContext();
	if (AIContext->GetDecisionSnapshot().bAlive)
	{
		DecisionTree->StartLogic();
		if (!DecisionTree->IsRunning())
		{
			return FGolemResult::Failure(EGolemError::InvalidState, TEXT("EnemyStateTree"),
				NSLOCTEXT("GolemAI", "EnemyTreeStart", "The configured enemy StateTree did not enter a running state."));
		}
	}
	return DecisionScheduler->StartScheduling(DecisionTree, AIContext);
}

void AGolemEnemyController::StopEnemyDecisions()
{
	DecisionScheduler->StopScheduling();
	DecisionTree->StopLogic(TEXT("Enemy controller ownership ended"));
	StopMovement();
	if (PawnLife.IsValid())
	{
		PawnLife->OnLifeStateChanged.Remove(LifeHandle);
	}
	LifeHandle.Reset();
	PawnLife.Reset();
	AIContext->ClearControlledPawn();
	RolePerception->ClearRoleSense();
}

void AGolemEnemyController::HandleRolePerception(AActor* Actor, bool bSensed)
{
	if (bEndingPlay || GetPawn() == nullptr || AIContext->GetControlledPawn() != GetPawn())
	{
		return;
	}
	AIContext->SetPerceivedActor(Actor, bSensed);
	if (UGolemThreatComponent* Threat = GetPawn()->FindComponentByClass<UGolemThreatComponent>())
	{
		if (bSensed)
		{
			Threat->ObserveTarget(Actor);
		}
		else
		{
			Threat->ForgetTarget(Actor);
		}
	}
	if (!bSensed && AIContext->GetDecisionSnapshot().Target.Get() == Actor)
	{
		AIContext->ClearTarget();
		StopMovement();
	}
	DecisionScheduler->RequestDecisionWake();
	OnDecisionChanged.Broadcast();
}

void AGolemEnemyController::HandlePawnLifeChanged(EGolemLifeState Previous, EGolemLifeState Current)
{
	if (bEndingPlay || GetPawn() == nullptr || GetWorld() == nullptr || GetWorld()->bIsTearingDown)
	{
		return;
	}
	if (Current != EGolemLifeState::Alive)
	{
		AIContext->CancelOwnedAction();
		DecisionTree->StopLogic(TEXT("Enemy life unavailable"));
		StopMovement();
	}
	else if (Archetype != nullptr && !DecisionTree->IsRunning())
	{
		AIContext->RefreshDecisionContext();
		DecisionTree->RestartLogic();
		DecisionScheduler->RequestDecisionWake();
	}
	OnDecisionChanged.Broadcast();
}

void AGolemEnemyController::HandleContextChanged()
{
	OnDecisionChanged.Broadcast();
}
