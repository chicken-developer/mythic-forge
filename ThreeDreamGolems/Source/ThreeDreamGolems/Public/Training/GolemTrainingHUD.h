#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "GolemTrainingHUD.generated.h"

class AGolemCharacter;
class AGameStateBase;
class AGolemGameState;
class AGolemPartyState;
class AGolemPlayerController;
struct FGolemResult;
struct FGolemTrainingProgress;

/** Minimal training presentation; authoritative progress belongs to the game state. */
UCLASS()
class THREEDREAMGOLEMS_API AGolemTrainingHUD : public AHUD
{
	GENERATED_BODY()

public:
	FText GetDisplayedGolemName() const;
	bool IsDisplayingCompletion() const;
	virtual void DrawHUD() override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void HandleGameStateSet(AGameStateBase* InGameState);
	void HandlePartyStateChanged(AGolemPartyState* InPartyState);
	void HandleRosterChanged();
	void HandleControlledGolemChanged(AGolemCharacter* PreviousGolem, AGolemCharacter* CurrentGolem);
	void HandleIdentityChanged();
	void HandleTrainingProgress(const FGolemTrainingProgress& Progress);
	void HandleCommandResult(const FGolemResult& Result);
	void UnbindSources();

	UPROPERTY(Transient)
	TWeakObjectPtr<AGolemGameState> TrainingSource;

	UPROPERTY(Transient)
	TWeakObjectPtr<AGolemPartyState> PartySource;

	UPROPERTY(Transient)
	TWeakObjectPtr<AGolemPlayerController> ControlSource;

	UPROPERTY(Transient)
	TWeakObjectPtr<AGolemCharacter> GolemSource;

	UPROPERTY(Transient)
	FText GolemName;

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
