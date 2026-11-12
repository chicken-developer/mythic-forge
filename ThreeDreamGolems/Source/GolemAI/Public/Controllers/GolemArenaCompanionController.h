#pragma once

#include "CoreMinimal.h"
#include "Commands/GolemCompanionCommand.h"
#include "Controllers/GolemCompanionController.h"
#include "Types/GolemResult.h"
#include "GolemArenaCompanionController.generated.h"

class UGolemAIArchetypeDefinition;
class UGolemAIContextComponent;
class UGolemAIDecisionSchedulerComponent;
class UGolemAIRolePerceptionComponent;
class UGolemColorAssistComponent;

/** M4 composition adds real combat decisions while the original companion controller remains the M1–M3 baseline. */
UCLASS()
class GOLEMAI_API AGolemArenaCompanionController : public AGolemCompanionController
{
	GENERATED_BODY()

public:
	AGolemArenaCompanionController();
	virtual void SetPawn(APawn* InPawn) override;
	/** Called after native composition grants the combat profile, and after checkpoint reset. */
	FGolemResult RefreshArenaConfiguration();
	FGolemResult TrySetCommand(EGolemCompanionCommand Command, AActor* FocusTarget = nullptr);
	UGolemAIContextComponent* GetAIContext() const;
	UGolemAIRolePerceptionComponent* GetRolePerception() const;
	UGolemAIDecisionSchedulerComponent* GetDecisionScheduler() const;
	UGolemColorAssistComponent* GetColorAssist() const;
	const FGolemCompanionCommandSnapshot& GetCommandSnapshot() const;
	const FGolemResult& GetArenaInitializationResult() const;
	FOnGolemCompanionCommandChanged OnCommandChanged;

protected:
	virtual void BeginPlay() override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;

private:
	FGolemResult ResolveArenaArchetype();
	FGolemResult InitializeArenaPawn(APawn* InPawn);
	void RefreshFormationLeader();
	void StopArenaDecisions();
	void HandleRolePerception(AActor* Actor, bool bSensed);
	void HandleDecisionScheduled(EGolemAILODTier Tier, float Interval, int64 TickCount);
	void HandleColorAssistJob(bool bActive);
	void DodgeTelegraphedAttack();
	UPROPERTY(VisibleAnywhere, Category = "Golem|AI|Context")
	TObjectPtr<UGolemAIContextComponent> AIContext;
	UPROPERTY(VisibleAnywhere, Category = "Golem|AI|Perception")
	TObjectPtr<UGolemAIRolePerceptionComponent> RolePerception;
	UPROPERTY(VisibleAnywhere, Category = "Golem|AI|LOD")
	TObjectPtr<UGolemAIDecisionSchedulerComponent> DecisionScheduler;
	/** Signal response and colour follow-up; pauses the ordinary decisions while it acts. */
	UPROPERTY(VisibleAnywhere, Category = "Golem|AI|Colour")
	TObjectPtr<UGolemColorAssistComponent> ColorAssist;
	UPROPERTY(Transient)
	TObjectPtr<UGolemAIArchetypeDefinition> Archetype;
	FGolemCompanionCommandSnapshot CommandSnapshot;
	FGolemResult ArenaInitializationResult;
	FDelegateHandle PerceptionHandle;
	FDelegateHandle ScheduleHandle;
	FDelegateHandle ColorAssistHandle;
	bool bEndingPlay = false;
};
