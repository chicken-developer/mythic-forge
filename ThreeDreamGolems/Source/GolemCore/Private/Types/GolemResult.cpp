#include "Types/GolemResult.h"

bool FGolemResult::IsSuccess() const
{
	return Error == EGolemError::None;
}

FGolemResult FGolemResult::Success()
{
	return FGolemResult();
}

FGolemResult FGolemResult::Failure(EGolemError InError, FName InContext, const FText& InMessage)
{
	FGolemResult Result;
	Result.Error = InError == EGolemError::None ? EGolemError::InvalidData : InError;
	Result.Context = InContext;
	Result.Message = InMessage;
	return Result;
}
