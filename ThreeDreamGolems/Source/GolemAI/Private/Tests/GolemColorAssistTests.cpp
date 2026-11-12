#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "ColorAssist/GolemColorAssistPlanner.h"

namespace GolemColorAssistTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
	constexpr uint8 Red = GolemElements::Mask(EGolemElement::Earth);
	constexpr uint8 Yellow = GolemElements::Mask(EGolemElement::Sky);
	constexpr uint8 Blue = GolemElements::Mask(EGolemElement::Sea);

	FGolemColorAssistVoid MakeVoid(const uint32 Id, const uint8 Missing, const bool bPrimed, const float Distance = 500.0f)
	{
		FGolemColorAssistVoid Void;
		Void.Id = Id;
		Void.bAcceptsHits = true;
		Void.bPrimed = bPrimed;
		Void.MissingMask = Missing;
		Void.Distance = Distance;
		return Void;
	}

	FGolemColorAssistVoid Signalled(FGolemColorAssistVoid Void, const double Start, const double Seconds = 6.0)
	{
		Void.SignalStart = Start;
		Void.SignalEnd = Start + Seconds;
		return Void;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGolemColorAssistSignalTest, "Golem.Color.Assist.AnswersASignalAfterHalfToOneSecond",
	GolemColorAssistTests::Flags)

bool FGolemColorAssistSignalTest::RunTest(const FString& Parameters)
{
	using namespace GolemColorAssistTests;
	const FGolemColorAssistTuning Tuning;
	TestEqual(TEXT("Shortest reaction"), Tuning.SignalDelayMin, 0.5f);
	TestEqual(TEXT("Longest reaction"), Tuning.SignalDelayMax, 1.0f);
	bool bNeverEarly = true;
	bool bAlwaysByOneSecond = true;
	double Earliest = 10.0;
	double Latest = 0.0;
	for (int32 Seed = 0; Seed < 64; ++Seed)
	{
		FGolemColorAssistPlanner Planner;
		Planner.SetSeed(Seed);
		// An Open red void, signalled at t = 10: Mars (Earth) is asked to open it.
		const TArray<FGolemColorAssistVoid> Voids = { Signalled(MakeVoid(1, Red, false), 10.0) };
		bNeverEarly &= !Planner.Choose(EGolemElement::Earth, Voids, 10.0, Tuning).IsValid();
		bNeverEarly &= !Planner.Choose(EGolemElement::Earth, Voids, 10.49, Tuning).IsValid();
		double RespondedAt = -1.0;
		for (double Now = 10.5; Now <= 11.0001; Now += 0.01)
		{
			const FGolemColorAssistChoice Choice = Planner.Choose(EGolemElement::Earth, Voids, Now, Tuning);
			if (Choice.IsValid())
			{
				bNeverEarly &= Choice.Id == 1 && Choice.Reason == EGolemColorAssistReason::Signal;
				RespondedAt = Now;
				break;
			}
		}
		bAlwaysByOneSecond &= RespondedAt > 0.0;
		Earliest = FMath::Min(Earliest, RespondedAt - 10.0);
		Latest = FMath::Max(Latest, RespondedAt - 10.0);
	}
	TestTrue(TEXT("No answer before half a second"), bNeverEarly);
	TestTrue(TEXT("Always an answer within one second"), bAlwaysByOneSecond);
	TestTrue(TEXT("Reaction times spread across the range"), Earliest < 0.6 && Latest > 0.9);

	// The marker expires: a companion that could not react in time does not act on a stale signal.
	FGolemColorAssistPlanner Late;
	Late.SetSeed(1);
	const TArray<FGolemColorAssistVoid> Expired = { Signalled(MakeVoid(1, Red, false), 10.0) };
	TestFalse(TEXT("An expired marker is ignored"), Late.Choose(EGolemElement::Earth, Expired, 16.5, Tuning).IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGolemColorAssistFollowUpTest, "Golem.Color.Assist.AddsToAPrimedVoidAfterOneAndAHalfSeconds",
	GolemColorAssistTests::Flags)

bool FGolemColorAssistFollowUpTest::RunTest(const FString& Parameters)
{
	using namespace GolemColorAssistTests;
	const FGolemColorAssistTuning Tuning;
	TestEqual(TEXT("Follow-up delay"), Tuning.FollowUpDelay, 1.5f);
	FGolemColorAssistPlanner Planner;
	Planner.SetSeed(7);
	// An orange void: the player (Mars) has placed Earth at t = 20, Sky is still missing.
	const TArray<FGolemColorAssistVoid> Primed = { MakeVoid(3, Yellow, true) };
	TestFalse(TEXT("Nothing at the moment it is primed"), Planner.Choose(EGolemElement::Sky, Primed, 20.0, Tuning).IsValid());
	TestFalse(TEXT("Nothing after 1.4 s"), Planner.Choose(EGolemElement::Sky, Primed, 21.4, Tuning).IsValid());
	const FGolemColorAssistChoice Choice = Planner.Choose(EGolemElement::Sky, Primed, 21.5, Tuning);
	TestTrue(TEXT("Jupiter adds Sky after 1.5 s"), Choice.IsValid() && Choice.Id == 3
		&& Choice.Reason == EGolemColorAssistReason::FollowUp);

	// The window closes and reopens: the delay starts again from the new priming.
	const TArray<FGolemColorAssistVoid> Reopened = { MakeVoid(3, Red | Yellow, false) };
	TestFalse(TEXT("Back to Open: nothing"), Planner.Choose(EGolemElement::Sky, Reopened, 24.5, Tuning).IsValid());
	TestFalse(TEXT("Primed again: the delay restarts"), Planner.Choose(EGolemElement::Sky, Primed, 25.0, Tuning).IsValid());
	TestTrue(TEXT("And runs out again"), Planner.Choose(EGolemElement::Sky, Primed, 26.5, Tuning).IsValid());

	// A signal on a Primed void speeds the companion up, and a signalled void outranks a nearer follow-up.
	FGolemColorAssistPlanner Both;
	Both.SetSeed(3);
	const TArray<FGolemColorAssistVoid> Two =
	{
		MakeVoid(5, Yellow, true, 200.0f),
		Signalled(MakeVoid(6, Yellow, false, 900.0f), 30.0)
	};
	Both.Choose(EGolemElement::Sky, Two, 30.0, Tuning);
	const FGolemColorAssistChoice Preferred = Both.Choose(EGolemElement::Sky, Two, 31.6, Tuning);
	TestTrue(TEXT("The signalled void is chosen over the nearer follow-up"), Preferred.Id == 6
		&& Preferred.Reason == EGolemColorAssistReason::Signal);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGolemColorAssistRestraintTest, "Golem.Color.Assist.NeverWrongColourAndNeverFirst",
	GolemColorAssistTests::Flags)

bool FGolemColorAssistRestraintTest::RunTest(const FString& Parameters)
{
	using namespace GolemColorAssistTests;
	const FGolemColorAssistTuning Tuning;
	FGolemColorAssistPlanner Planner;
	Planner.SetSeed(11);
	bool bEverActed = false;
	// 1. An Open void that needs exactly this Golem's colour, never signalled: the companion must not open it.
	const TArray<FGolemColorAssistVoid> OwnColourOpen = { MakeVoid(1, Blue, false) };
	// 2. A signalled void and a Primed void, neither of which needs this Golem's colour.
	const TArray<FGolemColorAssistVoid> OtherColours =
	{
		Signalled(MakeVoid(2, Red | Yellow, false), 0.0, 600.0),
		MakeVoid(3, Red, true)
	};
	for (double Now = 0.0; Now < 30.0; Now += 0.1)
	{
		bEverActed |= Planner.Choose(EGolemElement::Sea, OwnColourOpen, Now, Tuning).IsValid();
	}
	TestFalse(TEXT("A companion never opens a void by itself, even one of its own colour"), bEverActed);
	bEverActed = false;
	Planner.Reset();
	for (double Now = 0.0; Now < 30.0; Now += 0.1)
	{
		bEverActed |= Planner.Choose(EGolemElement::Sea, OtherColours, Now, Tuning).IsValid();
	}
	TestFalse(TEXT("A companion never colours a void that does not need its colour"), bEverActed);

	// 3. Too far away, or locked: left alone even when signalled.
	FGolemColorAssistVoid Far = Signalled(MakeVoid(4, Blue, false, Tuning.MaxRange + 1.0f), 0.0, 600.0);
	FGolemColorAssistVoid Locked = Signalled(MakeVoid(5, Blue, false), 0.0, 600.0);
	Locked.bAcceptsHits = false;
	Planner.Reset();
	bEverActed = false;
	for (double Now = 0.0; Now < 5.0; Now += 0.1)
	{
		bEverActed |= Planner.Choose(EGolemElement::Sea, { Far, Locked }, Now, Tuning).IsValid();
	}
	TestFalse(TEXT("Out of range and locked voids are left alone"), bEverActed);

	FGolemColorAssistTuning Bad;
	Bad.SignalDelayMax = 0.1f;
	TestFalse(TEXT("The longest reaction cannot be shorter than the shortest"), Bad.Validate().IsSuccess());
	TestTrue(TEXT("Default tuning is valid"), Tuning.Validate().IsSuccess());
	return true;
}

#endif
