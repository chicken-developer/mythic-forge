#pragma once

#include "CoreMinimal.h"
#include "Commands/KnightCompanionCommand.h"
#include "Controllers/KnightCompanionController.h"
#include "Types/KnightResult.h"
#include "KnightArenaCompanionController.generated.h"

class UKnightAIArchetypeDefinition;
class UKnightAIContextComponent;
class UKnightAIDecisionSchedulerComponent;
class UKnightAIRolePerceptionComponent;

/** M4 composition adds real combat decisions while the original companion controller remains the M1–M3 baseline. */
UCLASS()
class KNIGHTAI_API AKnightArenaCompanionController : public AKnightCompanionController
{
	GENERATED_BODY()

public:
	AKnightArenaCompanionController();
	virtual void SetPawn(APawn* InPawn) override;
	/** Called after native composition grants the combat profile, and after checkpoint reset. */
	FKnightResult RefreshArenaConfiguration();
	FKnightResult TrySetCommand(EKnightCompanionCommand Command, AActor* FocusTarget = nullptr);
	UKnightAIContextComponent* GetAIContext() const;
	UKnightAIRolePerceptionComponent* GetRolePerception() const;
	UKnightAIDecisionSchedulerComponent* GetDecisionScheduler() const;
	const FKnightCompanionCommandSnapshot& GetCommandSnapshot() const;
	const FKnightResult& GetArenaInitializationResult() const;
	FOnKnightCompanionCommandChanged OnCommandChanged;

protected:
	virtual void BeginPlay() override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;

private:
	FKnightResult ResolveArenaArchetype();
	FKnightResult InitializeArenaPawn(APawn* InPawn);
	void RefreshFormationLeader();
	void StopArenaDecisions();
	void HandleRolePerception(AActor* Actor, bool bSensed);
	void HandleDecisionScheduled(EKnightAILODTier Tier, float Interval, int64 TickCount);
	UPROPERTY(VisibleAnywhere, Category = "Knight|AI|Context")
	TObjectPtr<UKnightAIContextComponent> AIContext;
	UPROPERTY(VisibleAnywhere, Category = "Knight|AI|Perception")
	TObjectPtr<UKnightAIRolePerceptionComponent> RolePerception;
	UPROPERTY(VisibleAnywhere, Category = "Knight|AI|LOD")
	TObjectPtr<UKnightAIDecisionSchedulerComponent> DecisionScheduler;
	UPROPERTY(Transient)
	TObjectPtr<UKnightAIArchetypeDefinition> Archetype;
	FKnightCompanionCommandSnapshot CommandSnapshot;
	FKnightResult ArenaInitializationResult;
	FDelegateHandle PerceptionHandle;
	FDelegateHandle ScheduleHandle;
	bool bEndingPlay = false;
};
