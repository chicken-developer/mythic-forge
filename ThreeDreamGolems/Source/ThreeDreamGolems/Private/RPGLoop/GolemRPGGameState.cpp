#include "RPGLoop/GolemRPGGameState.h"

#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "RPGLoop/GolemRPGStateActor.h"

void AGolemRPGGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AGolemRPGGameState, RPGState);
}

AGolemRPGStateActor* AGolemRPGGameState::GetRPGState() const
{
	return RPGState;
}

FGolemResult AGolemRPGGameState::SetRPGState(AGolemRPGStateActor* InState)
{
	if (!HasAuthority() || !IsValid(InState) || InState->GetWorld() != GetWorld()
		|| (RPGState != nullptr && RPGState != InState))
	{
		return FGolemResult::Failure(EGolemError::Conflict, TEXT("RPGStateOwner"), FText::GetEmpty());
	}
	if (RPGState != InState)
	{
		RPGState = InState;
		ForceNetUpdate();
		OnRPGStateChanged.Broadcast(RPGState);
	}
	return FGolemResult::Success();
}

void AGolemRPGGameState::OnRep_RPGState()
{
	OnRPGStateChanged.Broadcast(RPGState);
}
