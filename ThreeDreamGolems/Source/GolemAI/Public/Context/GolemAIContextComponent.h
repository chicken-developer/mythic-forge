#pragma once

#include "CoreMinimal.h"
#include "Combat/GolemCombatDirectorSubsystem.h"
#include "Commands/GolemCompanionCommand.h"
#include "Components/ActorComponent.h"
#include "Definitions/GolemAIActionSetDefinition.h"
#include "GameplayAbilitySpec.h"
#include "GolemAIContextComponent.generated.h"

class AAIController;
class APawn;
class UEnvQuery;
class UGolemAbilitySystemComponent;
class UGolemLifeComponent;
class UGolemThreatComponent;
struct FAbilityEndedData;
struct FGolemAIRepositionTask;
enum class EGolemLifeState : uint8;

UENUM(BlueprintType)
enum class EGolemAIActionStatus : uint8
{
	Unknown,
	Running,
	Succeeded,
	Failed,
	Cancelled
};

USTRUCT(BlueprintType)
struct GOLEMAI_API FGolemAIDecisionSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Golem|AI|Decision")
	FGolemEntityId PawnId;
	UPROPERTY(BlueprintReadOnly, Category = "Golem|AI|Decision")
	FGolemEntityId TargetId;
	UPROPERTY(BlueprintReadOnly, Category = "Golem|AI|Decision")
	TWeakObjectPtr<AActor> Target;
	UPROPERTY(BlueprintReadOnly, Category = "Golem|AI|Decision")
	FPrimaryAssetId ActionSetId;
	UPROPERTY(BlueprintReadOnly, Category = "Golem|AI|Decision")
	EGolemCompanionCommand Command = EGolemCompanionCommand::Follow;
	UPROPERTY(BlueprintReadOnly, Category = "Golem|AI|Decision")
	bool bAlive = false;
	UPROPERTY(BlueprintReadOnly, Category = "Golem|AI|Decision")
	bool bHasTarget = false;
	UPROPERTY(BlueprintReadOnly, Category = "Golem|AI|Decision")
	bool bTargetFriendlyDowned = false;
	UPROPERTY(BlueprintReadOnly, Category = "Golem|AI|Decision")
	bool bActionRunning = false;
	UPROPERTY(BlueprintReadOnly, Category = "Golem|AI|Decision")
	bool bHasFormationGoal = false;
	UPROPERTY(BlueprintReadOnly, Category = "Golem|AI|Decision")
	bool bNeedsRegroup = false;
	UPROPERTY(BlueprintReadOnly, Category = "Golem|AI|Decision")
	bool bCanSelectAction = false;
	UPROPERTY(BlueprintReadOnly, Category = "Golem|AI|Decision")
	bool bHoldPosition = false;
	UPROPERTY(BlueprintReadOnly, Category = "Golem|AI|Decision")
	bool bStaggered = false;
	UPROPERTY(BlueprintReadOnly, Category = "Golem|AI|Decision")
	float TargetDistance = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category = "Golem|AI|Decision")
	float Stamina = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category = "Golem|AI|Decision")
	float RuneEnergy = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category = "Golem|AI|Decision")
	FVector FormationGoal = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly, Category = "Golem|AI|Decision")
	float FormationAcceptance = 120.0f;
	UPROPERTY(BlueprintReadOnly, Category = "Golem|AI|Decision")
	float DecisionInterval = 0.1f;
	UPROPERTY(BlueprintReadOnly, Category = "Golem|AI|Decision")
	FGameplayTag ActiveActionTag;
	FGuid ActiveActionToken;
};

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnGolemAIActionFinished, FGuid, EGolemAIActionStatus);
DECLARE_MULTICAST_DELEGATE(FOnGolemAIDecisionChanged);

/** Controller context owns only its decisions, action capability and observers; the pawn owns persistent GAS state. */
UCLASS(ClassGroup = Golem, meta = (BlueprintSpawnableComponent))
class GOLEMAI_API UGolemAIContextComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGolemAIContextComponent();
	FGolemResult InitializeControlledPawn(APawn* InPawn);
	void ClearControlledPawn();
	FGolemResult ValidateActionSet(const FPrimaryAssetId& InActionSetId) const;
	FGolemResult SetActionSet(const FPrimaryAssetId& InActionSetId);
	FPrimaryAssetId GetActionSetId() const;
	FGolemAIDecisionSnapshot GetDecisionSnapshot() const;
	APawn* GetControlledPawn() const;
	UGolemAbilitySystemComponent* GetAbilitySystem() const;
	FGolemResult SetValidatedTarget(AActor* InTarget, bool bFriendlyDowned = false);
	void ClearTarget();
	FGolemResult SetCompanionCommand(EGolemCompanionCommand InCommand, AActor* FocusTarget = nullptr);
	FGolemResult SetFormationLeader(AActor* Leader, const FVector& LocalOffset, float Acceptance);
	FGolemResult SetPerceivedActor(AActor* Actor, bool bSensed);
	FGolemResult SetDecisionInterval(float InInterval);
	float GetBaseDecisionInterval() const;
	FGolemResult SetScheduledDecisionInterval(float InInterval);
	FGolemResult SetAttackLeaseDuration(float InDuration);
	FGolemResult SetRepositionQuery(UEnvQuery* InQuery);
	UEnvQuery* GetRepositionQuery() const;
	/** Explicit sensor update. The returned snapshot and selection queries themselves never mutate context. */
	void RefreshDecisionContext();
	FGolemResult TrySelectAction(FGolemAIActionEntry& OutAction) const;
	FGolemResult TryBeginAction(const FGolemAIActionEntry& Action, bool bUseAttackLease, FGuid& OutActionToken);
	FGolemResult TryBeginRevive(FGuid& OutActionToken);
	bool HasSpecialization() const;
	FGolemResult TryBeginSpecializationAction(bool bUseAttackLease, FGuid& OutActionToken);
	FGolemResult CancelAction(const FGuid& ActionToken);
	void CancelOwnedAction();
	EGolemAIActionStatus GetActionStatus(const FGuid& ActionToken) const;
	FGolemAttackLease GetActiveAttackLease() const;
	TArray<int32> GetActiveEQSRequestIds() const;
	int32 GetCompletedEQSRequestCount() const;
	int32 GetAbortedEQSRequestCount() const;
	FOnGolemAIActionFinished OnActionFinished;
	FOnGolemAIDecisionChanged OnDecisionChanged;

protected:
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;

private:
	friend struct FGolemAIRepositionTask;
	void TrackEQSRequest(int32 QueryId);
	void FinishEQSRequest(int32 QueryId, bool bAborted);
	FGolemResult ValidateContext() const;
	FGolemResult ValidateTarget(AActor* InTarget, bool bFriendlyDowned) const;
	FGolemAIActionSelectionContext MakeSelectionContext() const;
	FGolemResult SelectSpecializationAction(int32& OutSlot, AActor*& OutTarget) const;
	FGolemResult ResolveActionSpec(FGameplayTag ActionTag, FGameplayAbilitySpecHandle& OutSpec,
		FGameplayTag& OutInput) const;
	FGolemResult BeginActionSpec(FGameplayTag ActionTag, FName ActionKey, float Cooldown,
		bool bUseAttackLease, FGuid& OutActionToken);
	void FinishOwnedAction(EGolemAIActionStatus Status, bool bCancelSpec);
	void HandleAbilityEnded(const FAbilityEndedData& Data);
	void HandlePawnLifeChanged(EGolemLifeState Previous, EGolemLifeState Current);
	void HandleLeaseReleased(const FGolemAttackLease& Lease, EGolemAttackLeaseReleaseReason Reason);
	UFUNCTION()
	void HandlePawnControllerChanged(APawn* Pawn, AController* Previous, AController* Current);
	UFUNCTION()
	void HandleObservedEndPlay(AActor* Actor, EEndPlayReason::Type Reason);

	UPROPERTY(Transient)
	TObjectPtr<UGolemAIActionSetDefinition> ActionSet;
	UPROPERTY(Transient)
	TObjectPtr<UEnvQuery> RepositionQuery;
	TWeakObjectPtr<APawn> ControlledPawn;
	TWeakObjectPtr<UGolemAbilitySystemComponent> AbilitySystem;
	TWeakObjectPtr<UGolemLifeComponent> Life;
	TWeakObjectPtr<UGolemThreatComponent> Threat;
	TWeakObjectPtr<UGolemCombatDirectorSubsystem> Director;
	TWeakObjectPtr<AActor> Target;
	TWeakObjectPtr<AActor> FocusTarget;
	TWeakObjectPtr<AActor> FormationLeader;
	TMap<FGolemEntityId, TWeakObjectPtr<AActor>> PerceivedActors;
	TMap<FName, double> CooldownEnds;
	FGolemEntityId PawnId;
	FGolemEntityId TargetId;
	FPrimaryAssetId ActionSetId;
	FVector FormationOffset = FVector::ZeroVector;
	FVector HoldLocation = FVector::ZeroVector;
	float FormationAcceptance = 120.0f;
	float DecisionInterval = 0.1f;
	float ScheduledDecisionInterval = 0.1f;
	float AttackLeaseDuration = 4.0f;
	EGolemCompanionCommand Command = EGolemCompanionCommand::Follow;
	bool bTargetFriendlyDowned = false;
	FGameplayAbilitySpecHandle ActiveSpec;
	FGameplayTag ActiveInput;
	FGameplayTag ActiveActionTag;
	FGuid ActiveActionToken;
	FGuid LastActionToken;
	EGolemAIActionStatus LastActionStatus = EGolemAIActionStatus::Unknown;
	FGolemAttackLease ActiveLease;
	FDelegateHandle AbilityEndedHandle;
	FDelegateHandle LifeHandle;
	FDelegateHandle LeaseHandle;
	bool bStartingAction = false;
	bool bEndingPlay = false;
	bool bClearingContext = false;
	uint64 OwnershipGeneration = 0;
	TSet<int32> ActiveEQSRequests;
	int32 CompletedEQSRequests = 0;
	int32 AbortedEQSRequests = 0;
};
