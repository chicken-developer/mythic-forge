#pragma once

#include "CoreMinimal.h"
#include "KnightResult.generated.h"

UENUM(BlueprintType)
enum class EKnightError : uint8
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
struct KNIGHTCORE_API FKnightResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Knight|Core|Result")
	EKnightError Error = EKnightError::None;

	UPROPERTY(BlueprintReadOnly, Category = "Knight|Core|Result")
	FName Context;

	UPROPERTY(BlueprintReadOnly, Category = "Knight|Core|Result")
	FText Message;

	bool IsSuccess() const;
	static FKnightResult Success();
	static FKnightResult Failure(EKnightError InError, FName InContext, const FText& InMessage);
};
