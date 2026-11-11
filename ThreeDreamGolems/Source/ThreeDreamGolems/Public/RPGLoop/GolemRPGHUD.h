#pragma once

#include "CoreMinimal.h"
#include "PartyArena/GolemPartyArenaHUD.h"
#include "GolemRPGHUD.generated.h"

class AGolemRPGGameState;
class AGolemRPGStateActor;
class AGolemRPGUIPlayerController;
struct FGolemRPGLoopSnapshot;

UCLASS()
class THREEDREAMGOLEMS_API AGolemRPGHUD : public AGolemPartyArenaHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;
	FText GetDisplayedRPGObjective() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;

private:
	void BindRPGSources();
	void HandleRPGStateReady(AGolemRPGStateActor* InState);
	void RefreshRPGObjective();
	TWeakObjectPtr<AGolemRPGGameState> RPGGameState;
	TWeakObjectPtr<AGolemRPGStateActor> RPGState;
	TWeakObjectPtr<AGolemRPGUIPlayerController> RPGPlayer;
	FDelegateHandle StateReadyHandle;
	FDelegateHandle LoopHandle;
	FDelegateHandle RPGHandle;
	FDelegateHandle InventoryHandle;
	FDelegateHandle ControlHandle;
	FDelegateHandle PresentationHandle;
	FTimerHandle InitialRPGBindingTimer;
	FText RPGObjective;
	FText ProgressionFeedback;
	FText InteractionFeedback;
	int32 BindingAttempts = 0;
};
