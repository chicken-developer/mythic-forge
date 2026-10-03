#pragma once

#include "CoreMinimal.h"
#include <atomic>

struct FKnightRequestGeneration
{
	std::atomic<uint64> Value { 0 };
};

/** Validate on the game thread before applying an async result to its weak UObject owner. */
class KNIGHTCORE_API FKnightCancellationToken
{
public:
	bool IsCurrent() const;
	uint64 GetGeneration() const;

private:
	friend class FKnightAsyncRequestScope;
	TWeakObjectPtr<UObject> Owner;
	TWeakPtr<FKnightRequestGeneration, ESPMode::ThreadSafe> State;
	uint64 Generation = 0;
};

/** New requests supersede old callbacks; teardown/travel calls Cancel, and destruction cancels implicitly. */
class KNIGHTCORE_API FKnightAsyncRequestScope : public FNoncopyable
{
public:
	FKnightAsyncRequestScope();
	~FKnightAsyncRequestScope();
	FKnightCancellationToken BeginRequest(UObject* Owner);
	void Cancel();

private:
	TSharedRef<FKnightRequestGeneration, ESPMode::ThreadSafe> State;
};
