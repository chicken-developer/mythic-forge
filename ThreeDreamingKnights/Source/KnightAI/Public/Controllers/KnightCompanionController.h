#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "KnightCompanionController.generated.h"

class UStateTree;
class UStateTreeAIComponent;

/** Server-owned companion controller. StateTree owns follow decisions and navigation owns motion. */
UCLASS()
class KNIGHTAI_API AKnightCompanionController : public AAIController
{
	GENERATED_BODY()

public:
	AKnightCompanionController();

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

	UPROPERTY(VisibleAnywhere, Category = "Knight|AI|Follow")
	TObjectPtr<UStateTreeAIComponent> FollowComponent;

	UPROPERTY(EditDefaultsOnly, Category = "Knight|AI|Follow")
	TSoftObjectPtr<UStateTree> FollowTree;
};
