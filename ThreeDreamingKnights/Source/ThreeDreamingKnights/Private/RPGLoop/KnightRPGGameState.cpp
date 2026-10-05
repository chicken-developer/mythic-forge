#include "RPGLoop/KnightRPGGameState.h"

#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "RPGLoop/KnightRPGStateActor.h"

void AKnightRPGGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AKnightRPGGameState, RPGState);
}

AKnightRPGStateActor* AKnightRPGGameState::GetRPGState() const
{
	return RPGState;
}

FKnightResult AKnightRPGGameState::SetRPGState(AKnightRPGStateActor* InState)
{
	if (!HasAuthority() || !IsValid(InState) || InState->GetWorld() != GetWorld()
		|| (RPGState != nullptr && RPGState != InState))
	{
		return FKnightResult::Failure(EKnightError::Conflict, TEXT("RPGStateOwner"), FText::GetEmpty());
	}
	if (RPGState != InState)
	{
		RPGState = InState;
		ForceNetUpdate();
		OnRPGStateChanged.Broadcast(RPGState);
	}
	return FKnightResult::Success();
}

void AKnightRPGGameState::OnRep_RPGState()
{
	OnRPGStateChanged.Broadcast(RPGState);
}
