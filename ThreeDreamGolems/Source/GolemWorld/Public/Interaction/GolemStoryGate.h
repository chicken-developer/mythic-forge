#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GolemStoryGate.generated.h"

class UGolemStoryWorldComponent;
class UStaticMeshComponent;
class UTextRenderComponent;

/** Physical blocker follows confirmed world facts on authority and replicated clients. */
UCLASS()
class GOLEMWORLD_API AGolemStoryGate : public AActor
{
	GENERATED_BODY()

public:
	AGolemStoryGate();
	bool IsOpen() const;
	UPROPERTY(EditInstanceOnly, Category = "Golem|World|Gate")
	FName RequiredBranchId;
	UPROPERTY(EditInstanceOnly, Category = "Golem|World|Gate")
	FName RequiredChoice = TEXT("Open");

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type Reason) override;

private:
	void BindWorldState();
	void RefreshGate();
	UPROPERTY(VisibleAnywhere, Category = "Golem|World|Gate")
	TObjectPtr<UStaticMeshComponent> Blocker;
	UPROPERTY(VisibleAnywhere, Category = "Golem|World|Gate")
	TObjectPtr<UTextRenderComponent> Label;
	TWeakObjectPtr<UGolemStoryWorldComponent> WorldState;
	FDelegateHandle WorldHandle;
	FTimerHandle ReadinessTimer;
	int32 ReadinessAttempts = 0;
	bool bOpen = false;
};
