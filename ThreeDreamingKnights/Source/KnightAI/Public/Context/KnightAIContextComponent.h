#pragma once

#include "CoreMinimal.h"
#include "Combat/KnightCombatDirectorSubsystem.h"
#include "Commands/KnightCompanionCommand.h"
#include "Components/ActorComponent.h"
#include "Definitions/KnightAIActionSetDefinition.h"
#include "GameplayAbilitySpec.h"
#include "KnightAIContextComponent.generated.h"

class AAIController;
class APawn;
class UEnvQuery;
class UKnightAbilitySystemComponent;
class UKnightLifeComponent;
class UKnightThreatComponent;
struct FAbilityEndedData;
struct FKnightAIRepositionTask;
enum class EKnightLifeState : uint8;

UENUM(BlueprintType)
enum class EKnightAIActionStatus : uint8
{
	Unknown,
	Running,
	Succeeded,
	Failed,
	Cancelled
};

USTRUCT(BlueprintType)
struct KNIGHTAI_API FKnightAIDecisionSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Knight|AI|Decision")
	FKnightEntityId PawnId;
	UPROPERTY(BlueprintReadOnly, Category = "Knight|AI|Decision")
	FKnightEntityId TargetId;
	UPROPERTY(BlueprintReadOnly, Category = "Knight|AI|Decision")
	TWeakObjectPtr<AActor> Target;
	UPROPERTY(BlueprintReadOnly, Category = "Knight|AI|Decision")
	FPrimaryAssetId ActionSetId;
	UPROPERTY(BlueprintReadOnly, Category = "Knight|AI|Decision")
	EKnightCompanionCommand Command = EKnightCompanionCommand::Follow;
	UPROPERTY(BlueprintReadOnly, Category = "Knight|AI|Decision")
	bool bAlive = false;
	UPROPERTY(BlueprintReadOnly, Category = "Knight|AI|Decision")
	bool bHasTarget = false;
	UPROPERTY(BlueprintReadOnly, Category = "Knight|AI|Decision")
	bool bTargetFriendlyDowned = false;
	UPROPERTY(BlueprintReadOnly, Category = "Knight|AI|Decision")
	bool bActionRunning = false;
	UPROPERTY(BlueprintReadOnly, Category = "Knight|AI|Decision")
	bool bHasFormationGoal = false;
	UPROPERTY(BlueprintReadOnly, Category = "Knight|AI|Decision")
	bool bNeedsRegroup = false;
	UPROPERTY(BlueprintReadOnly, Category = "Knight|AI|Decision")
	bool bCanSelectAction = false;
	UPROPERTY(BlueprintReadOnly, Category = "Knight|AI|Decision")
	bool bHoldPosition = false;
	UPROPERTY(BlueprintReadOnly, Category = "Knight|AI|Decision")
	bool bStaggered = false;
	UPROPERTY(BlueprintReadOnly, Category = "Knight|AI|Decision")
	float TargetDistance = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category = "Knight|AI|Decision")
	float Stamina = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category = "Knight|AI|Decision")
	float RuneEnergy = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category = "Knight|AI|Decision")
	FVector FormationGoal = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly, Category = "Knight|AI|Decision")
	float FormationAcceptance = 120.0f;
	UPROPERTY(BlueprintReadOnly, Category = "Knight|AI|Decision")
	float DecisionInterval = 0.1f;
	UPROPERTY(BlueprintReadOnly, Category = "Knight|AI|Decision")
	FGameplayTag ActiveActionTag;
	FGuid ActiveActionToken;
};

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnKnightAIActionFinished, FGuid, EKnightAIActionStatus);
DECLARE_MULTICAST_DELEGATE(FOnKnightAIDecisionChanged);

/** Controller context owns only its decisions, action capability and observers; the pawn owns persistent GAS state. */
UCLASS(ClassGroup = Knight, meta = (BlueprintSpawnableComponent))
class KNIGHTAI_API UKnightAIContextComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UKnightAIContextComponent();
	FKnightResult InitializeControlledPawn(APawn* InPawn);
	void ClearControlledPawn();
	FKnightResult ValidateActionSet(const FPrimaryAssetId& InActionSetId) const;
	FKnightResult SetActionSet(const FPrimaryAssetId& InActionSetId);
	FPrimaryAssetId GetActionSetId() const;
	FKnightAIDecisionSnapshot GetDecisionSnapshot() const;
	APawn* GetControlledPawn() const;
	UKnightAbilitySystemComponent* GetAbilitySystem() const;
	FKnightResult SetValidatedTarget(AActor* InTarget, bool bFriendlyDowned = false);
	void ClearTarget();
	FKnightResult SetCompanionCommand(EKnightCompanionCommand InCommand, AActor* FocusTarget = nullptr);
	FKnightResult SetFormationLeader(AActor* Leader, const FVector& LocalOffset, float Acceptance);
	FKnightResult SetPerceivedActor(AActor* Actor, bool bSensed);
	FKnightResult SetDecisionInterval(float InInterval);
	float GetBaseDecisionInterval() const;
	FKnightResult SetScheduledDecisionInterval(float InInterval);
	FKnightResult SetAttackLeaseDuration(float InDuration);
	FKnightResult SetRepositionQuery(UEnvQuery* InQuery);
	UEnvQuery* GetRepositionQuery() const;
	/** Explicit sensor update. The returned snapshot and selection queries themselves never mutate context. */
	void RefreshDecisionContext();
	FKnightResult TrySelectAction(FKnightAIActionEntry& OutAction) const;
	FKnightResult TryBeginAction(const FKnightAIActionEntry& Action, bool bUseAttackLease, FGuid& OutActionToken);
	FKnightResult TryBeginRevive(FGuid& OutActionToken);
	bool HasSpecialization() const;
	FKnightResult TryBeginSpecializationAction(bool bUseAttackLease, FGuid& OutActionToken);
	FKnightResult CancelAction(const FGuid& ActionToken);
	void CancelOwnedAction();
	EKnightAIActionStatus GetActionStatus(const FGuid& ActionToken) const;
	FKnightAttackLease GetActiveAttackLease() const;
	TArray<int32> GetActiveEQSRequestIds() const;
	int32 GetCompletedEQSRequestCount() const;
	int32 GetAbortedEQSRequestCount() const;
	FOnKnightAIActionFinished OnActionFinished;
	FOnKnightAIDecisionChanged OnDecisionChanged;

protected:
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;

private:
	friend struct FKnightAIRepositionTask;
	void TrackEQSRequest(int32 QueryId);
	void FinishEQSRequest(int32 QueryId, bool bAborted);
	FKnightResult ValidateContext() const;
	FKnightResult ValidateTarget(AActor* InTarget, bool bFriendlyDowned) const;
	FKnightAIActionSelectionContext MakeSelectionContext() const;
	FKnightResult SelectSpecializationAction(int32& OutSlot, AActor*& OutTarget) const;
	FKnightResult ResolveActionSpec(FGameplayTag ActionTag, FGameplayAbilitySpecHandle& OutSpec,
		FGameplayTag& OutInput) const;
	FKnightResult BeginActionSpec(FGameplayTag ActionTag, FName ActionKey, float Cooldown,
		bool bUseAttackLease, FGuid& OutActionToken);
	void FinishOwnedAction(EKnightAIActionStatus Status, bool bCancelSpec);
	void HandleAbilityEnded(const FAbilityEndedData& Data);
	void HandlePawnLifeChanged(EKnightLifeState Previous, EKnightLifeState Current);
	void HandleLeaseReleased(const FKnightAttackLease& Lease, EKnightAttackLeaseReleaseReason Reason);
	UFUNCTION()
	void HandlePawnControllerChanged(APawn* Pawn, AController* Previous, AController* Current);
	UFUNCTION()
	void HandleObservedEndPlay(AActor* Actor, EEndPlayReason::Type Reason);

	UPROPERTY(Transient)
	TObjectPtr<UKnightAIActionSetDefinition> ActionSet;
	UPROPERTY(Transient)
	TObjectPtr<UEnvQuery> RepositionQuery;
	TWeakObjectPtr<APawn> ControlledPawn;
	TWeakObjectPtr<UKnightAbilitySystemComponent> AbilitySystem;
	TWeakObjectPtr<UKnightLifeComponent> Life;
	TWeakObjectPtr<UKnightThreatComponent> Threat;
	TWeakObjectPtr<UKnightCombatDirectorSubsystem> Director;
	TWeakObjectPtr<AActor> Target;
	TWeakObjectPtr<AActor> FocusTarget;
	TWeakObjectPtr<AActor> FormationLeader;
	TMap<FKnightEntityId, TWeakObjectPtr<AActor>> PerceivedActors;
	TMap<FName, double> CooldownEnds;
	FKnightEntityId PawnId;
	FKnightEntityId TargetId;
	FPrimaryAssetId ActionSetId;
	FVector FormationOffset = FVector::ZeroVector;
	FVector HoldLocation = FVector::ZeroVector;
	float FormationAcceptance = 120.0f;
	float DecisionInterval = 0.1f;
	float ScheduledDecisionInterval = 0.1f;
	float AttackLeaseDuration = 4.0f;
	EKnightCompanionCommand Command = EKnightCompanionCommand::Follow;
	bool bTargetFriendlyDowned = false;
	FGameplayAbilitySpecHandle ActiveSpec;
	FGameplayTag ActiveInput;
	FGameplayTag ActiveActionTag;
	FGuid ActiveActionToken;
	FGuid LastActionToken;
	EKnightAIActionStatus LastActionStatus = EKnightAIActionStatus::Unknown;
	FKnightAttackLease ActiveLease;
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
