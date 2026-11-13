#include "ColorAssist/GolemColorAssistPlanner.h"

void FGolemColorAssistPlanner::SetSeed(const int32 Seed)
{
	Random.Initialize(Seed);
}

void FGolemColorAssistPlanner::Reset()
{
	Watches.Reset();
}

FGolemColorAssistChoice FGolemColorAssistPlanner::Choose(const EGolemElement Element,
	const TArray<FGolemColorAssistVoid>& Voids, const double Now, const FGolemColorAssistTuning& Tuning)
{
	const uint8 OwnMask = GolemElements::Mask(Element);
	FGolemColorAssistChoice Best;
	float BestDistance = TNumericLimits<float>::Max();
	TSet<uint32> Seen;
	for (const FGolemColorAssistVoid& Void : Voids)
	{
		Seen.Add(Void.Id);
		FWatch& Watch = Watches.FindOrAdd(Void.Id);
		const bool bSignaled = Void.SignalStart > 0.0 && Void.SignalEnd > Now;
		if (!bSignaled)
		{
			Watch.SignalStart = 0.0;
		}
		else if (Watch.SignalStart != Void.SignalStart)
		{
			// A new marker: pick the reaction time once, so the companion does not look mechanical.
			Watch.SignalStart = Void.SignalStart;
			Watch.RespondAt = Void.SignalStart + FMath::Lerp(Tuning.SignalDelayMin,
				FMath::Max(Tuning.SignalDelayMin, Tuning.SignalDelayMax), Random.FRand());
		}
		if (Void.bPrimed && !Watch.bPrimed)
		{
			Watch.PrimedSince = Now;
		}
		Watch.bPrimed = Void.bPrimed;

		// Never a colour the void does not ask for, and never a void that cannot take a hit.
		if (!Void.bAcceptsHits || (Void.MissingMask & OwnMask) == 0 || Void.Distance > Tuning.MaxRange)
		{
			continue;
		}
		EGolemColorAssistReason Reason = EGolemColorAssistReason::None;
		if (bSignaled && Now >= Watch.RespondAt)
		{
			Reason = EGolemColorAssistReason::Signal;
		}
		else if (Void.bPrimed && Now >= Watch.PrimedSince + Tuning.FollowUpDelay)
		{
			// Without a signal the companion only adds to a window somebody else opened.
			Reason = EGolemColorAssistReason::FollowUp;
		}
		if (Reason == EGolemColorAssistReason::None)
		{
			continue;
		}
		// A signal outranks a follow-up; among equals the nearest void wins.
		const bool bBetterReason = Reason == EGolemColorAssistReason::Signal && Best.Reason != EGolemColorAssistReason::Signal;
		if (!Best.IsValid() || bBetterReason || (Reason == Best.Reason && Void.Distance < BestDistance))
		{
			Best.Id = Void.Id;
			Best.Reason = Reason;
			BestDistance = Void.Distance;
		}
	}
	for (auto It = Watches.CreateIterator(); It; ++It)
	{
		if (!Seen.Contains(It.Key()))
		{
			It.RemoveCurrent();
		}
	}
	return Best;
}
