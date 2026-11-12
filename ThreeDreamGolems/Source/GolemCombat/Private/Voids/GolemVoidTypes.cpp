#include "Voids/GolemVoidTypes.h"

FGolemResult FGolemVoidTuning::Validate() const
{
	if (!FMath::IsFinite(MixWindowSeconds) || MixWindowSeconds < 0.1f || MixWindowSeconds > 600.0f ||
		!FMath::IsFinite(WhiteCoreWindowSeconds) || WhiteCoreWindowSeconds < 0.1f || WhiteCoreWindowSeconds > 600.0f ||
		!FMath::IsFinite(DulledSeconds) || DulledSeconds < 0.0f || DulledSeconds > 600.0f)
	{
		return FGolemResult::Failure(EGolemError::InvalidData, TEXT("VoidTuning"), FText::GetEmpty());
	}
	return FGolemResult::Success();
}

bool FGolemVoidRuntime::Configure(const uint8 InRequiredMask, const bool bStartHidden)
{
	if (!GolemElements::IsValidMask(InRequiredMask))
	{
		return false;
	}
	RequiredMask = InRequiredMask;
	AppliedMask = 0;
	TimerEnd = 0.0;
	State = bStartHidden ? EGolemVoidState::Hidden : EGolemVoidState::Open;
	return true;
}

bool FGolemVoidRuntime::Reveal()
{
	if (State != EGolemVoidState::Hidden || !GolemElements::IsValidMask(RequiredMask))
	{
		return false;
	}
	State = EGolemVoidState::Open;
	return true;
}

bool FGolemVoidRuntime::Hide()
{
	if (State == EGolemVoidState::Hidden || State == EGolemVoidState::Filled)
	{
		return false;
	}
	State = EGolemVoidState::Hidden;
	AppliedMask = 0;
	TimerEnd = 0.0;
	return true;
}

bool FGolemVoidRuntime::Update(const double Now)
{
	if ((State == EGolemVoidState::Primed || State == EGolemVoidState::Dulled) && Now >= TimerEnd)
	{
		// A missed window costs only the colours already placed; a lock simply ends.
		State = EGolemVoidState::Open;
		AppliedMask = 0;
		TimerEnd = 0.0;
		return true;
	}
	return false;
}

EGolemVoidHit FGolemVoidRuntime::Apply(const EGolemElement Element, const float ColorHoldSeconds, const double Now,
	const FGolemVoidTuning& Tuning)
{
	Update(Now);
	if (!AcceptsHits())
	{
		return EGolemVoidHit::Ignored;
	}
	const uint8 ElementMask = GolemElements::Mask(Element);
	if ((RequiredMask & ElementMask) == 0)
	{
		State = EGolemVoidState::Dulled;
		AppliedMask = 0;
		TimerEnd = Now + FMath::Max(0.0f, Tuning.DulledSeconds);
		return EGolemVoidHit::Dulled;
	}
	if ((AppliedMask & ElementMask) != 0)
	{
		return EGolemVoidHit::Repeated;
	}
	const bool bOpensWindow = State == EGolemVoidState::Open;
	AppliedMask |= ElementMask;
	if (AppliedMask == RequiredMask)
	{
		State = EGolemVoidState::Filled;
		TimerEnd = 0.0;
		return EGolemVoidHit::Filled;
	}
	if (bOpensWindow)
	{
		const float Window = GolemElements::Count(RequiredMask) >= 3 ? Tuning.WhiteCoreWindowSeconds : Tuning.MixWindowSeconds;
		const float Hold = FMath::IsFinite(ColorHoldSeconds) ? FMath::Max(0.0f, ColorHoldSeconds) : 0.0f;
		State = EGolemVoidState::Primed;
		TimerEnd = Now + Window + Hold;
	}
	return EGolemVoidHit::Primed;
}
