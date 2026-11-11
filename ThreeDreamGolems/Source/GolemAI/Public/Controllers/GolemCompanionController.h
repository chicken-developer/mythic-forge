#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "GolemCompanionController.generated.h"

class UStateTree;
class UStateTreeAIComponent;

/** Server-owned companion controller. StateTree owns follow decisions and navigation owns motion. */
UCLASS()
class GOLEMAI_API AGolemCompanionController : public AAIController
{
	GENERATED_BODY()

public:
	AGolemCompanionController();

	UStateTreeAIComponent* GetFollowComponent() const;
	bool IsFollowing() const;
	bool TryGetFormationGoal(FVector& OutGoal, float& OutAcceptanceRadius) const;
	bool TryMoveToFormation(const FVector& Goal, float AcceptanceRadius);
	void StopFollowing(const FString& Reason);
	void SetFollowTree(UStateTree* InFollowTree);

protected:
	virtual void BeginPlay() override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void StartFollowing();

	UPROPERTY(VisibleAnywhere, Category = "Golem|AI|Follow")
	TObjectPtr<UStateTreeAIComponent> FollowComponent;

	UPROPERTY(EditDefaultsOnly, Category = "Golem|AI|Follow")
	TSoftObjectPtr<UStateTree> FollowTree;
};
