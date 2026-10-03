#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Types/KnightResult.h"
#include "KnightTraversalComponent.generated.h"

class AController;
class UAnimMontage;
class UGameplayAbility;

USTRUCT()
struct KNIGHTCOMBAT_API FKnightTraversalSnapshot
{
	GENERATED_BODY()
	UPROPERTY()
	FGuid Handle;
	UPROPERTY()
	FTransform Landing;
	UPROPERTY()
	FName WarpTargetName;
};

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnKnightTraversalEnded, FGuid, bool);

/** Server validates obstacle and landing; CharacterMovement consumes a real Motion Warping montage. */
UCLASS(ClassGroup = Knight, meta = (BlueprintSpawnableComponent))
class KNIGHTCOMBAT_API UKnightTraversalComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UKnightTraversalComponent();
	FKnightResult TryBeginTraversal(UAnimMontage* Montage, FName WarpTargetName, FGuid& OutHandle,
		UGameplayAbility* SourceAbility = nullptr);
	void EndTraversal(const FGuid& Handle, bool bAborted);
	void PrepareForEndPlay();
	bool HasActiveTraversal() const;
	float GetActiveDuration() const;
	const FKnightTraversalSnapshot& GetSnapshot() const;
	FOnKnightTraversalEnded OnTraversalEnded;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

private:
	UFUNCTION()
	void OnRep_Snapshot();
	FKnightResult FindValidatedLanding(FTransform& OutLanding) const;
	UPROPERTY(ReplicatedUsing = OnRep_Snapshot)
	FKnightTraversalSnapshot Snapshot;
	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> ActiveMontage;
	UPROPERTY(Transient)
	TWeakObjectPtr<AController> StartingController;
	UPROPERTY(EditDefaultsOnly, Category = "Knight|Traversal", meta = (ClampMin = "100", Units = "cm"))
	float ProbeDistance = 240.0f;
	UPROPERTY(EditDefaultsOnly, Category = "Knight|Traversal", meta = (ClampMin = "50", Units = "cm"))
	float MaxObstacleHeight = 130.0f;
	UPROPERTY(EditDefaultsOnly, Category = "Knight|Traversal", meta = (ClampMin = "150", Units = "cm"))
	float MaxTravelDistance = 480.0f;
	UPROPERTY(EditDefaultsOnly, Category = "Knight|Traversal", meta = (ClampMin = "10", Units = "cm"))
	float CompletionTolerance = 100.0f;
	float StartedAt = 0.0f;
	float ActiveDuration = 0.0f;
	uint8 PreviousMovementMode = 0;
	uint8 PreviousCustomMode = 0;
	FName AppliedWarpTarget;
	FGuid CameraHandle;
	bool bPreviousOrientToMovement = true;
	bool bOwnTraversingTag = false;
	bool bEndingPlay = false;
};
