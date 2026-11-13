#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Commands/GolemCompanionCommand.h"
#include "Types/GolemResult.h"
#include "UObject/PrimaryAssetId.h"
#include "GolemEnemyController.generated.h"

class UStateTreeAIComponent;
class UGolemAIArchetypeDefinition;
class UGolemAIContextComponent;
class UGolemAIDecisionSchedulerComponent;
class UGolemAIRolePerceptionComponent;
class UGolemLifeComponent;
enum class EGolemLifeState : uint8;

DECLARE_MULTICAST_DELEGATE(FOnGolemEnemyDecisionChanged);

/** Server perception and StateTree own enemy intent. Persistent combat identity and GAS remain on the pawn. */
UCLASS()
class GOLEMAI_API AGolemEnemyController : public AAIController
{
	GENERATED_BODY()

public:
	AGolemEnemyController();
	FGolemResult ConfigureArchetype(const FPrimaryAssetId& InArchetypeId);
	UGolemAIContextComponent* GetAIContext() const;
	UGolemAIRolePerceptionComponent* GetRolePerception() const;
	UGolemAIDecisionSchedulerComponent* GetDecisionScheduler() const;
	UStateTreeAIComponent* GetDecisionTree() const;
	FPrimaryAssetId GetArchetypeId() const;
	EGolemAILODTier GetDecisionLOD() const;
	int64 GetDecisionTickCount() const;
	const FGolemResult& GetInitializationResult() const;
	FOnGolemEnemyDecisionChanged OnDecisionChanged;

protected:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;

private:
	FGolemResult ApplyArchetype();
	void StopEnemyDecisions();
	void HandleRolePerception(AActor* Actor, bool bSensed);
	void HandlePawnLifeChanged(EGolemLifeState Previous, EGolemLifeState Current);
	void HandleContextChanged();
	UPROPERTY(VisibleAnywhere, Category = "Golem|AI|Context")
	TObjectPtr<UGolemAIContextComponent> AIContext;
	UPROPERTY(VisibleAnywhere, Category = "Golem|AI|StateTree")
	TObjectPtr<UStateTreeAIComponent> DecisionTree;
	UPROPERTY(VisibleAnywhere, Category = "Golem|AI|Perception")
	TObjectPtr<UGolemAIRolePerceptionComponent> RolePerception;
	UPROPERTY(VisibleAnywhere, Category = "Golem|AI|LOD")
	TObjectPtr<UGolemAIDecisionSchedulerComponent> DecisionScheduler;
	UPROPERTY(Transient)
	TObjectPtr<UGolemAIArchetypeDefinition> Archetype;
	FPrimaryAssetId ArchetypeId;
	FGolemResult InitializationResult;
	TWeakObjectPtr<UGolemLifeComponent> PawnLife;
	FDelegateHandle LifeHandle;
	FDelegateHandle PerceptionHandle;
	FDelegateHandle ContextHandle;
	bool bEndingPlay = false;
};
