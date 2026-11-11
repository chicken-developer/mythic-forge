#include "Events/GolemAsyncRequestScope.h"

bool FGolemCancellationToken::IsCurrent() const
{
	check(IsInGameThread());
	const TSharedPtr<FGolemRequestGeneration, ESPMode::ThreadSafe> CurrentState = State.Pin();
	return Owner.IsValid() && CurrentState.IsValid()
		&& CurrentState->Value.load(std::memory_order_acquire) == Generation;
}

uint64 FGolemCancellationToken::GetGeneration() const
{
	return Generation;
}

FGolemAsyncRequestScope::FGolemAsyncRequestScope()
	: State(MakeShared<FGolemRequestGeneration, ESPMode::ThreadSafe>())
{
}

FGolemAsyncRequestScope::~FGolemAsyncRequestScope()
{
	Cancel();
}

FGolemCancellationToken FGolemAsyncRequestScope::BeginRequest(UObject* Owner)
{
	check(IsInGameThread());
	FGolemCancellationToken Token;
	Token.Owner = Owner;
	Token.State = State;
	Token.Generation = State->Value.fetch_add(1, std::memory_order_acq_rel) + 1;
	return Token;
}

void FGolemAsyncRequestScope::Cancel()
{
	State->Value.fetch_add(1, std::memory_order_acq_rel);
}
