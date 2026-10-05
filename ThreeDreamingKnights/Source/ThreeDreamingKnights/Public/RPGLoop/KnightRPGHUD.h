#pragma once

#include "CoreMinimal.h"
#include "PartyArena/KnightPartyArenaHUD.h"
#include "KnightRPGHUD.generated.h"

class AKnightRPGGameState;
class AKnightRPGStateActor;
class AKnightRPGUIPlayerController;
struct FKnightRPGLoopSnapshot;

UCLASS()
class THREEDREAMINGKNIGHTS_API AKnightRPGHUD : public AKnightPartyArenaHUD
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
	void HandleRPGStateReady(AKnightRPGStateActor* InState);
	void RefreshRPGObjective();
	TWeakObjectPtr<AKnightRPGGameState> RPGGameState;
	TWeakObjectPtr<AKnightRPGStateActor> RPGState;
	TWeakObjectPtr<AKnightRPGUIPlayerController> RPGPlayer;
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
