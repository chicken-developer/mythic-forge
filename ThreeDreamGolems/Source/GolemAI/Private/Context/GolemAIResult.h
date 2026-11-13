#pragma once

#include "Types/GolemResult.h"

inline FGolemResult GolemAIFailure(EGolemError Error, FName Operation)
{
	FText Message;
	switch (Error)
	{
	case EGolemError::NotAuthority:
		Message = NSLOCTEXT("GolemAI", "AuthorityRequired", "The host must perform this AI command.");
		break;
	case EGolemError::NotFound:
		Message = NSLOCTEXT("GolemAI", "NoEligibleAction", "No eligible AI action or target is available.");
		break;
	case EGolemError::Conflict:
		Message = NSLOCTEXT("GolemAI", "ActionConflict", "Another action currently owns this Golem.");
		break;
	case EGolemError::OutOfRange:
		Message = NSLOCTEXT("GolemAI", "TargetOutOfRange", "The AI target is out of range or out of sight.");
		break;
	case EGolemError::CapacityExceeded:
		Message = NSLOCTEXT("GolemAI", "AttackBudgetFull", "The available attack slots are occupied.");
		break;
	case EGolemError::NotOwned:
		Message = NSLOCTEXT("GolemAI", "OwnershipChanged", "The AI no longer controls this Golem or action.");
		break;
	case EGolemError::ShuttingDown:
		Message = NSLOCTEXT("GolemAI", "ContextEnding", "The AI context is shutting down.");
		break;
	case EGolemError::InvalidId:
	case EGolemError::InvalidData:
	case EGolemError::TypeMismatch:
		Message = NSLOCTEXT("GolemAI", "InvalidData", "The AI definition or command data is invalid.");
		break;
	default:
		Message = NSLOCTEXT("GolemAI", "UnavailableState", "The current state does not allow this AI action.");
		break;
	}
	return FGolemResult::Failure(Error, Operation, Message);
}
