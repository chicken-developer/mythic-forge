#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "Types/GolemResult.h"
#include "GolemPlayerState.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FOnGolemHumanAssignmentChanged, int32);

/** Human connection identity only. Golem combat/progression identity remains on its persistent character. */
UCLASS()
class GOLEMFRAMEWORK_API AGolemPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	FGuid GetHumanId() const;
	int32 GetAssignedSlot() const;
	FGolemResult InitializeHumanIdentity();
	FGolemResult SetAssignedSlot(int32 InAssignedSlot);
	FOnGolemHumanAssignmentChanged OnAssignmentChanged;

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION()
	void OnRep_AssignedSlot();

	UPROPERTY(Replicated)
	FGuid HumanId;

	UPROPERTY(ReplicatedUsing = OnRep_AssignedSlot)
	int32 AssignedSlot = INDEX_NONE;
};
