#include "Controllers/KnightEnemyController.h"

#include "AbilitySystemGlobals.h"
#include "Assets/KnightAssetManager.h"
#include "Boss/KnightBossPhaseComponent.h"
#include "Components/KnightAbilitySystemComponent.h"
#include "Components/KnightAIDecisionSchedulerComponent.h"
#include "Components/KnightLifeComponent.h"
#include "Components/StateTreeAIComponent.h"
#include "Context/KnightAIContextComponent.h"
#include "Definitions/KnightAIArchetypeDefinition.h"
#include "Engine/World.h"
#include "Perception/KnightAIRolePerceptionComponent.h"
#include "StateTree.h"
#include "Threat/KnightThreatComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogKnightEnemyController, Log, All);

AKnightEnemyController::AKnightEnemyController()
{
	AIContext = CreateDefaultSubobject<UKnightAIContextComponent>(TEXT("EnemyAIContext"));
	DecisionTree = CreateDefaultSubobject<UStateTreeAIComponent>(TEXT("EnemyStateTree"));
	RolePerception = CreateDefaultSubobject<UKnightAIRolePerceptionComponent>(TEXT("EnemyRolePerception"));
	DecisionScheduler = CreateDefaultSubobject<UKnightAIDecisionSchedulerComponent>(TEXT("EnemyDecisionScheduler"));
	DecisionTree->SetStartLogicAutomatically(false);
	DecisionTree->PrimaryComponentTick.bCanEverTick = false;
	BrainComponent = DecisionTree;
	bStartAILogicOnPossess = false;
	SetPerceptionComponent(*RolePerception);
	PerceptionHandle = RolePerception->OnRolePerceptionChanged.AddUObject(this, &ThisClass::HandleRolePerception);
	ContextHandle = AIContext->OnDecisionChanged.AddUObject(this, &ThisClass::HandleContextChanged);
}

FKnightResult AKnightEnemyController::ConfigureArchetype(const FPrimaryAssetId& InArchetypeId)
{
	if (!HasAuthority())
	{
		return FKnightResult::Failure(EKnightError::NotAuthority, TEXT("EnemyArchetype"),
			NSLOCTEXT("KnightAI", "EnemyArchetypeAuthority", "Enemy archetypes are configured by the server."));
	}
	if (bEndingPlay || GetWorld() == nullptr || GetWorld()->bIsTearingDown)
	{
		return FKnightResult::Failure(EKnightError::ShuttingDown, TEXT("EnemyArchetype"),
			NSLOCTEXT("KnightAI", "EnemyArchetypeEnding", "Enemy controller is shutting down."));
	}
	UKnightAssetManager* Manager = UKnightAssetManager::GetKnightAssetManager();
	UKnightDefinition* Definition = nullptr;
	const FKnightResult Result = Manager != nullptr
		? Manager->ResolveDefinition(InArchetypeId, UKnightAIArchetypeDefinition::StaticClass(), Definition)
		: FKnightResult::Failure(EKnightError::InvalidState, TEXT("EnemyArchetype"),
			NSLOCTEXT("KnightAI", "EnemyCatalog", "The enemy definition catalog is unavailable."));
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
	if (ArchetypeId == InArchetypeId && Archetype == Resolved && DecisionTree->IsRunning())
	{
		return FKnightResult::Success();
	}
	DecisionScheduler->StopScheduling();
	DecisionTree->StopLogic(TEXT("Enemy archetype changed"));
	AIContext->CancelOwnedAction();
	StopMovement();
	Archetype = Resolved;
	ArchetypeId = InArchetypeId;
	InitializationResult = GetPawn() != nullptr ? ApplyArchetype() : FKnightResult::Success();
	return InitializationResult;
}

UKnightAIContextComponent* AKnightEnemyController::GetAIContext() const
{
	return AIContext;
}

UKnightAIRolePerceptionComponent* AKnightEnemyController::GetRolePerception() const
{
	return RolePerception;
}

UKnightAIDecisionSchedulerComponent* AKnightEnemyController::GetDecisionScheduler() const
{
	return DecisionScheduler;
}

UStateTreeAIComponent* AKnightEnemyController::GetDecisionTree() const
{
	return DecisionTree;
}

FPrimaryAssetId AKnightEnemyController::GetArchetypeId() const
{
	return ArchetypeId;
}

EKnightAILODTier AKnightEnemyController::GetDecisionLOD() const
{
	return DecisionScheduler->GetDecisionLOD();
}

int64 AKnightEnemyController::GetDecisionTickCount() const
{
	return DecisionScheduler->GetDecisionTickCount();
}

const FKnightResult& AKnightEnemyController::GetInitializationResult() const
{
	return InitializationResult;
}

void AKnightEnemyController::OnPossess(APawn* InPawn)
{
	StopEnemyDecisions();
	Super::OnPossess(InPawn);
	InitializationResult = AIContext->InitializeControlledPawn(InPawn);
	if (InitializationResult.IsSuccess())
	{
		UKnightAbilitySystemComponent* ASC = AIContext->GetAbilitySystem();
		PawnLife = ASC->GetLifeComponent();
		LifeHandle = PawnLife->OnLifeStateChanged.AddUObject(this, &ThisClass::HandlePawnLifeChanged);
		if (Archetype != nullptr)
		{
			InitializationResult = ApplyArchetype();
		}
	}
	if (!InitializationResult.IsSuccess())
	{
		UE_LOG(LogKnightEnemyController, Error, TEXT("Enemy possession initialization failed [%s]: %s."),
			*InitializationResult.Context.ToString(), *InitializationResult.Message.ToString());
	}
}

void AKnightEnemyController::OnUnPossess()
{
	StopEnemyDecisions();
	Super::OnUnPossess();
}

void AKnightEnemyController::EndPlay(EEndPlayReason::Type EndPlayReason)
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

FKnightResult AKnightEnemyController::ApplyArchetype()
{
	FKnightResult Result = AIContext->InitializeControlledPawn(GetPawn());
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
	UKnightBossPhaseComponent* Boss = GetPawn()->FindComponentByClass<UKnightBossPhaseComponent>();
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
			return FKnightResult::Failure(EKnightError::InvalidState, TEXT("EnemyStateTree"),
				NSLOCTEXT("KnightAI", "EnemyTreeStart", "The configured enemy StateTree did not enter a running state."));
		}
	}
	return DecisionScheduler->StartScheduling(DecisionTree, AIContext);
}

void AKnightEnemyController::StopEnemyDecisions()
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

void AKnightEnemyController::HandleRolePerception(AActor* Actor, bool bSensed)
{
	if (bEndingPlay || GetPawn() == nullptr || AIContext->GetControlledPawn() != GetPawn())
	{
		return;
	}
	AIContext->SetPerceivedActor(Actor, bSensed);
	if (UKnightThreatComponent* Threat = GetPawn()->FindComponentByClass<UKnightThreatComponent>())
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

void AKnightEnemyController::HandlePawnLifeChanged(EKnightLifeState Previous, EKnightLifeState Current)
{
	if (bEndingPlay || GetPawn() == nullptr || GetWorld() == nullptr || GetWorld()->bIsTearingDown)
	{
		return;
	}
	if (Current != EKnightLifeState::Alive)
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

void AKnightEnemyController::HandleContextChanged()
{
	OnDecisionChanged.Broadcast();
}
