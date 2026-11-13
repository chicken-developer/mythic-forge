#pragma once

#include "CoreMinimal.h"
#include "Types/GolemElement.h"
#include "Types/GolemResult.h"
#include "GolemVoidTypes.generated.h"

/** DEV_SPEC section 5: the life of one glowing void on a Xenon. */
UENUM(BlueprintType)
enum class EGolemVoidState : uint8
{
	/** Not revealed; cannot be hit. */
	Hidden,
	/** Waiting for colour. */
	Open,
	/** Holds at least one correct colour and is counting down. */
	Primed,
	/** Just took a wrong colour; locked for a short time. */
	Dulled,
	/** Complete; never changes again. */
	Filled
};

/** What one colour-applying hit did to a void. */
UENUM(BlueprintType)
enum class EGolemVoidHit : uint8
{
	/** The void was Hidden, Dulled or Filled. */
	Ignored,
	/** A new correct colour was recorded and more are still needed. */
	Primed,
	/** That colour was already there; nothing changed and the timer was not refreshed. */
	Repeated,
	/** The last missing colour arrived. */
	Filled,
	/** Wrong colour: recorded colours were cleared and the void locked. */
	Dulled
};

/** Starting values from DEV_SPEC section 5. Designers override them in Xenon data. */
USTRUCT(BlueprintType)
struct GOLEMCOMBAT_API FGolemVoidTuning
{
	GENERATED_BODY()

	/** Seconds the team has to add the remaining colours once the first one lands. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Combat|Voids", meta = (ClampMin = "0.1", Units = "s"))
	float MixWindowSeconds = 4.0f;

	/** Same window for a void that needs all three elements (the white core). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Combat|Voids", meta = (ClampMin = "0.1", Units = "s"))
	float WhiteCoreWindowSeconds = 5.0f;

	/** Seconds a void stays locked after a wrong colour. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Combat|Voids", meta = (ClampMin = "0.0", Units = "s"))
	float DulledSeconds = 3.0f;

	/** Seconds a player's signal marker stays on a void (DEV_SPEC section 12). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Combat|Voids", meta = (ClampMin = "0.1", Units = "s"))
	float SignalSeconds = 6.0f;

	FGolemResult Validate() const;
};

/**
 * The rules of one void with no UObject, world or clock of its own: every call takes the current
 * host time, so each rule can be tested with exact numbers.
 */
struct GOLEMCOMBAT_API FGolemVoidRuntime
{
	EGolemVoidState State = EGolemVoidState::Hidden;

	/** Elements this void needs (GolemElements mask). */
	uint8 RequiredMask = 0;

	/** Correct elements placed so far. */
	uint8 AppliedMask = 0;

	/** Host time when a Primed window closes or a Dulled lock ends; 0 otherwise. */
	double TimerEnd = 0.0;

	/** Sets what the void needs and returns it to Hidden or Open with nothing applied. */
	bool Configure(uint8 InRequiredMask, bool bStartHidden);

	/** Hidden to Open. */
	bool Reveal();

	/** Open, Primed or Dulled back to Hidden, clearing applied colours. A Filled void stays Filled. */
	bool Hide();

	/** Runs timers up to Now. True when the state changed. */
	bool Update(double Now);

	/** Applies one colour-carrying hit. ColorHoldSeconds counts only for the hit that opens the window. */
	EGolemVoidHit Apply(EGolemElement Element, float ColorHoldSeconds, double Now, const FGolemVoidTuning& Tuning);

	uint8 MissingMask() const
	{
		return RequiredMask & ~AppliedMask;
	}

	bool AcceptsHits() const
	{
		return State == EGolemVoidState::Open || State == EGolemVoidState::Primed;
	}
};
