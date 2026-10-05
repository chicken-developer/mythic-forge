#pragma once

#include "CoreMinimal.h"
#include "Commands/KnightRPGCommand.h"
#include "PartyArena/KnightArenaPlayerController.h"
#include "KnightRPGPlayerController.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FOnKnightRPGCommandResult, const FKnightRPGCommandResult&);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnKnightRPGLoopResult, const FKnightResult&);

/** Discrete RPG requests retain their original Knight identity across possession changes. */
UCLASS()
class THREEDREAMINGKNIGHTS_API AKnightRPGPlayerController : public AKnightArenaPlayerController
{
	GENERATED_BODY()

public:
	AKnightRPGPlayerController();
	void RequestRPGCommand(const FKnightRPGCommand& Request);
	const FKnightRPGCommandResult& GetLastRPGResult() const;
	void RequestContinueRPGLoop();
	const FKnightResult& GetLastRPGLoopResult() const;
	FOnKnightRPGCommandResult OnRPGCommandResult;
	FOnKnightRPGLoopResult OnRPGLoopResult;

protected:
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;

private:
	UFUNCTION(Server, Reliable)
	void ServerRequestRPGCommand(FKnightRPGCommand Request);

	UFUNCTION(Client, Reliable)
	void ClientReceiveRPGCommandResult(FKnightRPGCommandResult Outcome);

	UFUNCTION(Server, Reliable)
	void ServerRequestContinueRPGLoop();

	UFUNCTION(Client, Reliable)
	void ClientReceiveRPGLoopResult(FKnightResult Result);

	FKnightRPGCommandResult LastRPGResult;
	FKnightResult LastRPGLoopResult;
	bool bEndingRPGPlay = false;
};
