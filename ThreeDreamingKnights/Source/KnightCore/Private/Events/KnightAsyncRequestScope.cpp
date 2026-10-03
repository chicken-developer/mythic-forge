#include "Events/KnightAsyncRequestScope.h"

bool FKnightCancellationToken::IsCurrent() const
{
	check(IsInGameThread());
	const TSharedPtr<FKnightRequestGeneration, ESPMode::ThreadSafe> CurrentState = State.Pin();
	return Owner.IsValid() && CurrentState.IsValid()
		&& CurrentState->Value.load(std::memory_order_acquire) == Generation;
}

uint64 FKnightCancellationToken::GetGeneration() const
{
	return Generation;
}

FKnightAsyncRequestScope::FKnightAsyncRequestScope()
	: State(MakeShared<FKnightRequestGeneration, ESPMode::ThreadSafe>())
{
}

FKnightAsyncRequestScope::~FKnightAsyncRequestScope()
{
	Cancel();
}

FKnightCancellationToken FKnightAsyncRequestScope::BeginRequest(UObject* Owner)
{
	check(IsInGameThread());
	FKnightCancellationToken Token;
	Token.Owner = Owner;
	Token.State = State;
	Token.Generation = State->Value.fetch_add(1, std::memory_order_acq_rel) + 1;
	return Token;
}

void FKnightAsyncRequestScope::Cancel()
{
	State->Value.fetch_add(1, std::memory_order_acq_rel);
}
