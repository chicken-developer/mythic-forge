#pragma once

#include "CoreMinimal.h"
#include <atomic>

struct FGolemRequestGeneration
{
	std::atomic<uint64> Value { 0 };
};

/** Validate on the game thread before applying an async result to its weak UObject owner. */
class GOLEMCORE_API FGolemCancellationToken
{
public:
	bool IsCurrent() const;
	uint64 GetGeneration() const;

private:
	friend class FGolemAsyncRequestScope;
	TWeakObjectPtr<UObject> Owner;
	TWeakPtr<FGolemRequestGeneration, ESPMode::ThreadSafe> State;
	uint64 Generation = 0;
};

/** New requests supersede old callbacks; teardown/travel calls Cancel, and destruction cancels implicitly. */
class GOLEMCORE_API FGolemAsyncRequestScope : public FNoncopyable
{
public:
	FGolemAsyncRequestScope();
	~FGolemAsyncRequestScope();
	FGolemCancellationToken BeginRequest(UObject* Owner);
	void Cancel();

private:
	TSharedRef<FGolemRequestGeneration, ESPMode::ThreadSafe> State;
};
