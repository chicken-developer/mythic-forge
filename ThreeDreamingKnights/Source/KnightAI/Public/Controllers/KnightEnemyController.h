#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Commands/KnightCompanionCommand.h"
#include "Types/KnightResult.h"
#include "UObject/PrimaryAssetId.h"
#include "KnightEnemyController.generated.h"

class UStateTreeAIComponent;
class UKnightAIArchetypeDefinition;
class UKnightAIContextComponent;
class UKnightAIDecisionSchedulerComponent;
class UKnightAIRolePerceptionComponent;
class UKnightLifeComponent;
enum class EKnightLifeState : uint8;

DECLARE_MULTICAST_DELEGATE(FOnKnightEnemyDecisionChanged);

/** Server perception and StateTree own enemy intent. Persistent combat identity and GAS remain on the pawn. */
UCLASS()
class KNIGHTAI_API AKnightEnemyController : public AAIController
{
	GENERATED_BODY()

public:
	AKnightEnemyController();
	FKnightResult ConfigureArchetype(const FPrimaryAssetId& InArchetypeId);
	UKnightAIContextComponent* GetAIContext() const;
	UKnightAIRolePerceptionComponent* GetRolePerception() const;
	UKnightAIDecisionSchedulerComponent* GetDecisionScheduler() const;
	UStateTreeAIComponent* GetDecisionTree() const;
	FPrimaryAssetId GetArchetypeId() const;
	EKnightAILODTier GetDecisionLOD() const;
	int64 GetDecisionTickCount() const;
	const FKnightResult& GetInitializationResult() const;
	FOnKnightEnemyDecisionChanged OnDecisionChanged;

protected:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;

private:
	FKnightResult ApplyArchetype();
	void StopEnemyDecisions();
	void HandleRolePerception(AActor* Actor, bool bSensed);
	void HandlePawnLifeChanged(EKnightLifeState Previous, EKnightLifeState Current);
	void HandleContextChanged();
	UPROPERTY(VisibleAnywhere, Category = "Knight|AI|Context")
	TObjectPtr<UKnightAIContextComponent> AIContext;
	UPROPERTY(VisibleAnywhere, Category = "Knight|AI|StateTree")
	TObjectPtr<UStateTreeAIComponent> DecisionTree;
	UPROPERTY(VisibleAnywhere, Category = "Knight|AI|Perception")
	TObjectPtr<UKnightAIRolePerceptionComponent> RolePerception;
	UPROPERTY(VisibleAnywhere, Category = "Knight|AI|LOD")
	TObjectPtr<UKnightAIDecisionSchedulerComponent> DecisionScheduler;
	UPROPERTY(Transient)
	TObjectPtr<UKnightAIArchetypeDefinition> Archetype;
	FPrimaryAssetId ArchetypeId;
	FKnightResult InitializationResult;
	TWeakObjectPtr<UKnightLifeComponent> PawnLife;
	FDelegateHandle LifeHandle;
	FDelegateHandle PerceptionHandle;
	FDelegateHandle ContextHandle;
	bool bEndingPlay = false;
};
