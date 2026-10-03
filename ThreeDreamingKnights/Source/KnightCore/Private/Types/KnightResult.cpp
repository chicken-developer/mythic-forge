#include "Types/KnightResult.h"

bool FKnightResult::IsSuccess() const
{
	return Error == EKnightError::None;
}

FKnightResult FKnightResult::Success()
{
	return FKnightResult();
}

FKnightResult FKnightResult::Failure(EKnightError InError, FName InContext, const FText& InMessage)
{
	FKnightResult Result;
	Result.Error = InError == EKnightError::None ? EKnightError::InvalidData : InError;
	Result.Context = InContext;
	Result.Message = InMessage;
	return Result;
}
