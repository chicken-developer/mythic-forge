#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "Types/KnightResult.h"
#include "KnightPlayerState.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FOnKnightHumanAssignmentChanged, int32);

/** Human connection identity only. Knight combat/progression identity remains on its persistent character. */
UCLASS()
class KNIGHTFRAMEWORK_API AKnightPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	FGuid GetHumanId() const;
	int32 GetAssignedSlot() const;
	FKnightResult InitializeHumanIdentity();
	FKnightResult SetAssignedSlot(int32 InAssignedSlot);
	FOnKnightHumanAssignmentChanged OnAssignmentChanged;

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
