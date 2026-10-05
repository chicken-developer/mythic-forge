#include "RPGLoop/KnightRPGPlayerController.h"

#include "RPGLoop/KnightRPGLoopGameMode.h"

#include "Engine/World.h"

AKnightRPGPlayerController::AKnightRPGPlayerController()
{
	LastRPGResult.Result = FKnightResult::Failure(EKnightError::InvalidState, TEXT("RPGCommand"),
		NSLOCTEXT("KnightRPG", "CommandNotRequested", "No RPG command has completed."));
	LastRPGLoopResult = FKnightResult::Failure(EKnightError::InvalidState, TEXT("RPGContinue"),
		NSLOCTEXT("KnightRPG", "ContinueNotRequested", "No continue request has completed."));
}

void AKnightRPGPlayerController::RequestRPGCommand(const FKnightRPGCommand& Request)
{
	if (bEndingRPGPlay || !IsLocalController() || GetWorld() == nullptr || GetWorld()->bIsTearingDown)
	{
		return;
	}
	ServerRequestRPGCommand(Request);
}

const FKnightRPGCommandResult& AKnightRPGPlayerController::GetLastRPGResult() const
{
	return LastRPGResult;
}

void AKnightRPGPlayerController::RequestContinueRPGLoop()
{
	if (bEndingRPGPlay || !IsLocalController() || GetWorld() == nullptr || GetWorld()->bIsTearingDown)
	{
		return;
	}
	ServerRequestContinueRPGLoop();
}

const FKnightResult& AKnightRPGPlayerController::GetLastRPGLoopResult() const
{
	return LastRPGLoopResult;
}

void AKnightRPGPlayerController::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	bEndingRPGPlay = true;
	OnRPGCommandResult.Clear();
	OnRPGLoopResult.Clear();
	Super::EndPlay(EndPlayReason);
}

void AKnightRPGPlayerController::ServerRequestRPGCommand_Implementation(FKnightRPGCommand Request)
{
	FKnightRPGCommandResult Outcome;
	Outcome.RequestId = Request.RequestId;
	Outcome.KnightId = Request.ExpectedKnightId;
	Outcome.InventoryId = Request.InventoryId;
	Outcome.Operation = Request.Operation;
	UWorld* World = GetWorld();
	AKnightRPGLoopGameMode* Mode = World != nullptr ? World->GetAuthGameMode<AKnightRPGLoopGameMode>() : nullptr;
	if (bEndingRPGPlay || World == nullptr || World->bIsTearingDown)
	{
		Outcome.Result = FKnightResult::Failure(EKnightError::ShuttingDown, TEXT("RPGCommand"),
			NSLOCTEXT("KnightRPG", "CommandWorldEnding", "The RPG world is shutting down."));
	}
	else if (Mode == nullptr)
	{
		Outcome.Result = FKnightResult::Failure(EKnightError::InvalidState, TEXT("RPGCommand"),
			NSLOCTEXT("KnightRPG", "CommandModeUnavailable", "The current world does not support RPG commands."));
	}
	else
	{
		Outcome = Mode->ExecuteRPGCommand(this, Request);
	}
	ClientReceiveRPGCommandResult(Outcome);
}

void AKnightRPGPlayerController::ClientReceiveRPGCommandResult_Implementation(FKnightRPGCommandResult Outcome)
{
	if (bEndingRPGPlay)
	{
		return;
	}
	// Preserve the committed source ID. A listener decides whether it still represents its displayed Knight.
	LastRPGResult = MoveTemp(Outcome);
	OnRPGCommandResult.Broadcast(LastRPGResult);
}

void AKnightRPGPlayerController::ServerRequestContinueRPGLoop_Implementation()
{
	UWorld* World = GetWorld();
	AKnightRPGLoopGameMode* Mode = World != nullptr ? World->GetAuthGameMode<AKnightRPGLoopGameMode>() : nullptr;
	FKnightResult Result;
	if (bEndingRPGPlay || World == nullptr || World->bIsTearingDown)
	{
		Result = FKnightResult::Failure(EKnightError::ShuttingDown, TEXT("RPGContinue"),
			NSLOCTEXT("KnightRPG", "ContinueWorldEnding", "The RPG world is shutting down."));
	}
	else if (Mode == nullptr)
	{
		Result = FKnightResult::Failure(EKnightError::InvalidState, TEXT("RPGContinue"),
			NSLOCTEXT("KnightRPG", "ContinueModeUnavailable", "The current world does not support this RPG loop."));
	}
	else
	{
		Result = Mode->TryContinueRPGLoop(this);
	}
	ClientReceiveRPGLoopResult(Result);
}

void AKnightRPGPlayerController::ClientReceiveRPGLoopResult_Implementation(FKnightResult Result)
{
	if (bEndingRPGPlay)
	{
		return;
	}
	LastRPGLoopResult = MoveTemp(Result);
	OnRPGLoopResult.Broadcast(LastRPGLoopResult);
}
