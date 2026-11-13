#pragma once

#include "CoreMinimal.h"
#include "Definitions/GolemAIArchetypeDefinition.h"
#include "Math/RandomStream.h"
#include "Types/GolemElement.h"

/** What a companion can see of one void at one moment. Mirrors EGolemVoidState without depending on GolemCombat. */
struct GOLEMAI_API FGolemColorAssistVoid
{
	/** Any key that stays the same for one void between calls. */
	uint32 Id = 0;

	/** True for Open and Primed: the void can take a colour right now. */
	bool bAcceptsHits = false;

	/** True while another colour is already waiting on this void. */
	bool bPrimed = false;

	/** Elements still missing (GolemElements mask). */
	uint8 MissingMask = 0;

	/** Host times of the signal marker; both 0 when there is none. */
	double SignalStart = 0.0;
	double SignalEnd = 0.0;

	/** Centimetres from the companion. */
	float Distance = 0.0f;
};

enum class EGolemColorAssistReason : uint8
{
	None,
	/** A player signalled the void and it needs this Golem's colour. */
	Signal,
	/** The void is Primed by somebody else and still missing this Golem's colour. */
	FollowUp
};

struct GOLEMAI_API FGolemColorAssistChoice
{
	uint32 Id = 0;
	EGolemColorAssistReason Reason = EGolemColorAssistReason::None;

	bool IsValid() const
	{
		return Reason != EGolemColorAssistReason::None;
	}
};

/**
 * Decides when a companion places its colour (DEV_SPEC section 12), with no world of its own so
 * each rule can be tested with exact times:
 *  - a signalled void that needs its colour gets a heavy attack 0.5 to 1.0 s after the signal;
 *  - a Primed void still missing its colour gets one after 1.5 s;
 *  - it never colours a void that does not need its colour;
 *  - it never opens a void by itself: without a signal it only adds to a Primed one.
 */
struct GOLEMAI_API FGolemColorAssistPlanner
{
	void SetSeed(int32 Seed);
	void Reset();

	/** Updates what was seen at Now and returns the one void to colour, if any. */
	FGolemColorAssistChoice Choose(EGolemElement Element, const TArray<FGolemColorAssistVoid>& Voids, double Now,
		const FGolemColorAssistTuning& Tuning);

private:
	struct FWatch
	{
		double SignalStart = 0.0;
		double RespondAt = 0.0;
		double PrimedSince = 0.0;
		bool bPrimed = false;
	};

	TMap<uint32, FWatch> Watches;
	FRandomStream Random;
};
