#include "RPGLoop/GolemRPGPlayerController.h"

#include "RPGLoop/GolemRPGLoopGameMode.h"

#include "Engine/World.h"

AGolemRPGPlayerController::AGolemRPGPlayerController()
{
	LastRPGResult.Result = FGolemResult::Failure(EGolemError::InvalidState, TEXT("RPGCommand"),
		NSLOCTEXT("KnightRPG", "CommandNotRequested", "No RPG command has completed."));
	LastRPGLoopResult = FGolemResult::Failure(EGolemError::InvalidState, TEXT("RPGContinue"),
		NSLOCTEXT("KnightRPG", "ContinueNotRequested", "No continue request has completed."));
}

void AGolemRPGPlayerController::RequestRPGCommand(const FGolemRPGCommand& Request)
{
	if (bEndingRPGPlay || !IsLocalController() || GetWorld() == nullptr || GetWorld()->bIsTearingDown)
	{
		return;
	}
	ServerRequestRPGCommand(Request);
}

const FGolemRPGCommandResult& AGolemRPGPlayerController::GetLastRPGResult() const
{
	return LastRPGResult;
}

void AGolemRPGPlayerController::RequestContinueRPGLoop()
{
	if (bEndingRPGPlay || !IsLocalController() || GetWorld() == nullptr || GetWorld()->bIsTearingDown)
	{
		return;
	}
	ServerRequestContinueRPGLoop();
}

const FGolemResult& AGolemRPGPlayerController::GetLastRPGLoopResult() const
{
	return LastRPGLoopResult;
}

void AGolemRPGPlayerController::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	bEndingRPGPlay = true;
	OnRPGCommandResult.Clear();
	OnRPGLoopResult.Clear();
	Super::EndPlay(EndPlayReason);
}

void AGolemRPGPlayerController::ServerRequestRPGCommand_Implementation(FGolemRPGCommand Request)
{
	FGolemRPGCommandResult Outcome;
	Outcome.RequestId = Request.RequestId;
	Outcome.GolemId = Request.ExpectedGolemId;
	Outcome.InventoryId = Request.InventoryId;
	Outcome.Operation = Request.Operation;
	UWorld* World = GetWorld();
	AGolemRPGLoopGameMode* Mode = World != nullptr ? World->GetAuthGameMode<AGolemRPGLoopGameMode>() : nullptr;
	if (bEndingRPGPlay || World == nullptr || World->bIsTearingDown)
	{
		Outcome.Result = FGolemResult::Failure(EGolemError::ShuttingDown, TEXT("RPGCommand"),
			NSLOCTEXT("KnightRPG", "CommandWorldEnding", "The RPG world is shutting down."));
	}
	else if (Mode == nullptr)
	{
		Outcome.Result = FGolemResult::Failure(EGolemError::InvalidState, TEXT("RPGCommand"),
			NSLOCTEXT("KnightRPG", "CommandModeUnavailable", "The current world does not support RPG commands."));
	}
	else
	{
		Outcome = Mode->ExecuteRPGCommand(this, Request);
	}
	ClientReceiveRPGCommandResult(Outcome);
}

void AGolemRPGPlayerController::ClientReceiveRPGCommandResult_Implementation(FGolemRPGCommandResult Outcome)
{
	if (bEndingRPGPlay)
	{
		return;
	}
	// Preserve the committed source ID. A listener decides whether it still represents its displayed Golem.
	LastRPGResult = MoveTemp(Outcome);
	OnRPGCommandResult.Broadcast(LastRPGResult);
}

void AGolemRPGPlayerController::ServerRequestContinueRPGLoop_Implementation()
{
	UWorld* World = GetWorld();
	AGolemRPGLoopGameMode* Mode = World != nullptr ? World->GetAuthGameMode<AGolemRPGLoopGameMode>() : nullptr;
	FGolemResult Result;
	if (bEndingRPGPlay || World == nullptr || World->bIsTearingDown)
	{
		Result = FGolemResult::Failure(EGolemError::ShuttingDown, TEXT("RPGContinue"),
			NSLOCTEXT("KnightRPG", "ContinueWorldEnding", "The RPG world is shutting down."));
	}
	else if (Mode == nullptr)
	{
		Result = FGolemResult::Failure(EGolemError::InvalidState, TEXT("RPGContinue"),
			NSLOCTEXT("KnightRPG", "ContinueModeUnavailable", "The current world does not support this RPG loop."));
	}
	else
	{
		Result = Mode->TryContinueRPGLoop(this);
	}
	ClientReceiveRPGLoopResult(Result);
}

void AGolemRPGPlayerController::ClientReceiveRPGLoopResult_Implementation(FGolemResult Result)
{
	if (bEndingRPGPlay)
	{
		return;
	}
	LastRPGLoopResult = MoveTemp(Result);
	OnRPGLoopResult.Broadcast(LastRPGLoopResult);
}
