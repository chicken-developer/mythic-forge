#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "KnightTrainingHUD.generated.h"

class AKnightCharacter;
class AGameStateBase;
class AKnightGameState;
class AKnightPartyState;
class AKnightPlayerController;
struct FKnightResult;
struct FKnightTrainingProgress;

/** Minimal training presentation; authoritative progress belongs to the game state. */
UCLASS()
class THREEDREAMINGKNIGHTS_API AKnightTrainingHUD : public AHUD
{
	GENERATED_BODY()

public:
	FText GetDisplayedKnightName() const;
	bool IsDisplayingCompletion() const;
	virtual void DrawHUD() override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void HandleGameStateSet(AGameStateBase* InGameState);
	void HandlePartyStateChanged(AKnightPartyState* InPartyState);
	void HandleRosterChanged();
	void HandleControlledKnightChanged(AKnightCharacter* PreviousKnight, AKnightCharacter* CurrentKnight);
	void HandleIdentityChanged();
	void HandleTrainingProgress(const FKnightTrainingProgress& Progress);
	void HandleCommandResult(const FKnightResult& Result);
	void UnbindSources();

	UPROPERTY(Transient)
	TWeakObjectPtr<AKnightGameState> TrainingSource;

	UPROPERTY(Transient)
	TWeakObjectPtr<AKnightPartyState> PartySource;

	UPROPERTY(Transient)
	TWeakObjectPtr<AKnightPlayerController> ControlSource;

	UPROPERTY(Transient)
	TWeakObjectPtr<AKnightCharacter> KnightSource;

	UPROPERTY(Transient)
	FText KnightName;

	UPROPERTY(Transient)
	FText PartyStatus;

	UPROPERTY(Transient)
	FText ProgressStatus;

	UPROPERTY(Transient)
	FText CommandStatus;

	FDelegateHandle PartySourceHandle;
	FDelegateHandle GameStateHandle;
	FDelegateHandle RosterHandle;
	FDelegateHandle ControlHandle;
	FDelegateHandle IdentityHandle;
	FDelegateHandle ProgressHandle;
	FDelegateHandle CommandHandle;
	bool bCompleted = false;
};
