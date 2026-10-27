#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "KnightStoryGate.generated.h"

class UKnightStoryWorldComponent;
class UStaticMeshComponent;
class UTextRenderComponent;

/** Physical blocker follows confirmed world facts on authority and replicated clients. */
UCLASS()
class KNIGHTWORLD_API AKnightStoryGate : public AActor
{
	GENERATED_BODY()

public:
	AKnightStoryGate();
	bool IsOpen() const;
	UPROPERTY(EditInstanceOnly, Category = "Knight|World|Gate")
	FName RequiredBranchId;
	UPROPERTY(EditInstanceOnly, Category = "Knight|World|Gate")
	FName RequiredChoice = TEXT("Open");

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type Reason) override;

private:
	void BindWorldState();
	void RefreshGate();
	UPROPERTY(VisibleAnywhere, Category = "Knight|World|Gate")
	TObjectPtr<UStaticMeshComponent> Blocker;
	UPROPERTY(VisibleAnywhere, Category = "Knight|World|Gate")
	TObjectPtr<UTextRenderComponent> Label;
	TWeakObjectPtr<UKnightStoryWorldComponent> WorldState;
	FDelegateHandle WorldHandle;
	FTimerHandle ReadinessTimer;
	int32 ReadinessAttempts = 0;
	bool bOpen = false;
};
