#pragma once

#include "CoreMinimal.h"
#include "GolemResult.generated.h"

UENUM(BlueprintType)
enum class EGolemError : uint8
{
	None,
	InvalidId,
	NotFound,
	DuplicateId,
	TypeMismatch,
	LoadFailed,
	InvalidData,
	NotAuthority,
	InvalidRevision,
	DuplicateConfirmation,
	ShuttingDown,
	Cancelled,
	Conflict,
	InvalidState,
	OutOfRange,
	NotOwned,
	CapacityExceeded,
	InsufficientResources
};

/** A domain command outcome. Context carries a stable ID or operation name, never localized text. */
USTRUCT(BlueprintType)
struct GOLEMCORE_API FGolemResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Golem|Core|Result")
	EGolemError Error = EGolemError::None;

	UPROPERTY(BlueprintReadOnly, Category = "Golem|Core|Result")
	FName Context;

	UPROPERTY(BlueprintReadOnly, Category = "Golem|Core|Result")
	FText Message;

	bool IsSuccess() const;
	static FGolemResult Success();
	static FGolemResult Failure(EGolemError InError, FName InContext, const FText& InMessage);
};
