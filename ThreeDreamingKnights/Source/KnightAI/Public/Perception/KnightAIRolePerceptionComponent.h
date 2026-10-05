#pragma once

#include "CoreMinimal.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AIPerceptionTypes.h"
#include "Types/KnightResult.h"
#include "KnightAIRolePerceptionComponent.generated.h"

class APawn;
class UAISenseConfig_Sight;
class UKnightLifeComponent;
struct FKnightAIArchetypeTuning;

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnKnightAIRolePerceptionChanged, AActor*, bool);

/** Genuine sight stimuli are filtered by stable GAS identity, role, life and world after affiliation detection. */
UCLASS(ClassGroup = Knight)
class KNIGHTAI_API UKnightAIRolePerceptionComponent : public UAIPerceptionComponent
{
	GENERATED_BODY()

public:
	UKnightAIRolePerceptionComponent();
	FKnightResult InitializeRoleSense(APawn* InPawn);
	FKnightResult ConfigureRoleSense(const FKnightAIArchetypeTuning& Tuning);
	void ClearRoleSense();
	int32 GetEligibleSensedCount() const;
	int64 GetStimulusUpdateCount() const;
	FOnKnightAIRolePerceptionChanged OnRolePerceptionChanged;

protected:
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;

private:
	struct FKnightSensedActor
	{
		TWeakObjectPtr<UKnightLifeComponent> Life;
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
	UPROPERTY(VisibleAnywhere, Category = "Knight|AI|Perception")
	TObjectPtr<UAISenseConfig_Sight> SightConfig;
	TWeakObjectPtr<APawn> ControlledPawn;
	TMap<TWeakObjectPtr<AActor>, FKnightSensedActor> SensedActors;
	int64 StimulusUpdateCount = 0;
	bool bEndingPlay = false;
};
