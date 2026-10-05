#pragma once

#include "CoreMinimal.h"
#include "Framework/KnightGameState.h"
#include "KnightRPGGameState.generated.h"

class AKnightRPGStateActor;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnKnightRPGStateReady, AKnightRPGStateActor*);

UCLASS()
class THREEDREAMINGKNIGHTS_API AKnightRPGGameState : public AKnightGameState
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	AKnightRPGStateActor* GetRPGState() const;
	FKnightResult SetRPGState(AKnightRPGStateActor* InState);
	FOnKnightRPGStateReady OnRPGStateChanged;

private:
	UFUNCTION()
	void OnRep_RPGState();

	UPROPERTY(ReplicatedUsing = OnRep_RPGState)
	TObjectPtr<AKnightRPGStateActor> RPGState;
};
