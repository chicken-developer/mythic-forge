#pragma once

#include "CoreMinimal.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AIPerceptionTypes.h"
#include "Types/GolemResult.h"
#include "GolemAIRolePerceptionComponent.generated.h"

class APawn;
class UAISenseConfig_Sight;
class UGolemLifeComponent;
struct FGolemAIArchetypeTuning;

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnGolemAIRolePerceptionChanged, AActor*, bool);

/** Genuine sight stimuli are filtered by stable GAS identity, role, life and world after affiliation detection. */
UCLASS(ClassGroup = Golem)
class GOLEMAI_API UGolemAIRolePerceptionComponent : public UAIPerceptionComponent
{
	GENERATED_BODY()

public:
	UGolemAIRolePerceptionComponent();
	FGolemResult InitializeRoleSense(APawn* InPawn);
	FGolemResult ConfigureRoleSense(const FGolemAIArchetypeTuning& Tuning);
	void ClearRoleSense();
	int32 GetEligibleSensedCount() const;
	int64 GetStimulusUpdateCount() const;
	FOnGolemAIRolePerceptionChanged OnRolePerceptionChanged;

protected:
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;

private:
	struct FGolemSensedActor
	{
		TWeakObjectPtr<UGolemLifeComponent> Life;
		FDelegateHandle LifeHandle;
		bool bEligible = false;
	};
	bool IsEligibleActor(AActor* Actor) const;
	void RefreshTrackedActor(AActor* Actor);
	void RemoveTrackedActor(AActor* Actor, bool bNotify);
	UFUNCTION()
	void HandleTargetPerception(AActor* Actor, FAIStimulus Stimulus);
	UFUNCTION()
	void HandleTargetEndPlay(AActor* Actor, EEndPlayReason::Type Reason);
	UPROPERTY(VisibleAnywhere, Category = "Golem|AI|Perception")
	TObjectPtr<UAISenseConfig_Sight> SightConfig;
	TWeakObjectPtr<APawn> ControlledPawn;
	TMap<TWeakObjectPtr<AActor>, FGolemSensedActor> SensedActors;
	int64 StimulusUpdateCount = 0;
	bool bEndingPlay = false;
};
