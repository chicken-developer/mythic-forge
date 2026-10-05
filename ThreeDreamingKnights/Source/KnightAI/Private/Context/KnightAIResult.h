#pragma once

#include "Types/KnightResult.h"

inline FKnightResult KnightAIFailure(EKnightError Error, FName Operation)
{
	FText Message;
	switch (Error)
	{
	case EKnightError::NotAuthority:
		Message = NSLOCTEXT("KnightAI", "AuthorityRequired", "The host must perform this AI command.");
		break;
	case EKnightError::NotFound:
		Message = NSLOCTEXT("KnightAI", "NoEligibleAction", "No eligible AI action or target is available.");
		break;
	case EKnightError::Conflict:
		Message = NSLOCTEXT("KnightAI", "ActionConflict", "Another action currently owns this Knight.");
		break;
	case EKnightError::OutOfRange:
		Message = NSLOCTEXT("KnightAI", "TargetOutOfRange", "The AI target is out of range or out of sight.");
		break;
	case EKnightError::CapacityExceeded:
		Message = NSLOCTEXT("KnightAI", "AttackBudgetFull", "The available attack slots are occupied.");
		break;
	case EKnightError::NotOwned:
		Message = NSLOCTEXT("KnightAI", "OwnershipChanged", "The AI no longer controls this Knight or action.");
		break;
	case EKnightError::ShuttingDown:
		Message = NSLOCTEXT("KnightAI", "ContextEnding", "The AI context is shutting down.");
		break;
	case EKnightError::InvalidId:
	case EKnightError::InvalidData:
	case EKnightError::TypeMismatch:
		Message = NSLOCTEXT("KnightAI", "InvalidData", "The AI definition or command data is invalid.");
		break;
	default:
		Message = NSLOCTEXT("KnightAI", "UnavailableState", "The current state does not allow this AI action.");
		break;
	}
	return FKnightResult::Failure(Error, Operation, Message);
}
