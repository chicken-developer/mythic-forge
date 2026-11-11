#pragma once

#include "CoreMinimal.h"
#include "Commands/GolemRPGCommand.h"
#include "PartyArena/GolemArenaPlayerController.h"
#include "GolemRPGPlayerController.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FOnGolemRPGCommandResult, const FGolemRPGCommandResult&);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnGolemRPGLoopResult, const FGolemResult&);

/** Discrete RPG requests retain their original Golem identity across possession changes. */
UCLASS()
class THREEDREAMGOLEMS_API AGolemRPGPlayerController : public AGolemArenaPlayerController
{
	GENERATED_BODY()

public:
	AGolemRPGPlayerController();
	void RequestRPGCommand(const FGolemRPGCommand& Request);
	const FGolemRPGCommandResult& GetLastRPGResult() const;
	void RequestContinueRPGLoop();
	const FGolemResult& GetLastRPGLoopResult() const;
	FOnGolemRPGCommandResult OnRPGCommandResult;
	FOnGolemRPGLoopResult OnRPGLoopResult;

protected:
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;

private:
	UFUNCTION(Server, Reliable)
	void ServerRequestRPGCommand(FGolemRPGCommand Request);

	UFUNCTION(Client, Reliable)
	void ClientReceiveRPGCommandResult(FGolemRPGCommandResult Outcome);

	UFUNCTION(Server, Reliable)
	void ServerRequestContinueRPGLoop();

	UFUNCTION(Client, Reliable)
	void ClientReceiveRPGLoopResult(FGolemResult Result);

	FGolemRPGCommandResult LastRPGResult;
	FGolemResult LastRPGLoopResult;
	bool bEndingRPGPlay = false;
};
