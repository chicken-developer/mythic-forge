#pragma once

#include "CoreMinimal.h"
#include "Framework/GolemGameState.h"
#include "GolemRPGGameState.generated.h"

class AGolemRPGStateActor;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnGolemRPGStateReady, AGolemRPGStateActor*);

UCLASS()
class THREEDREAMGOLEMS_API AGolemRPGGameState : public AGolemGameState
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	AGolemRPGStateActor* GetRPGState() const;
	FGolemResult SetRPGState(AGolemRPGStateActor* InState);
	FOnGolemRPGStateReady OnRPGStateChanged;

private:
	UFUNCTION()
	void OnRep_RPGState();

	UPROPERTY(ReplicatedUsing = OnRep_RPGState)
	TObjectPtr<AGolemRPGStateActor> RPGState;
};
