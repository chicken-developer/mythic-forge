#pragma once

#include "CoreMinimal.h"
#include "PartyArena/GolemPartyArenaHUD.h"
#include "GolemColorArenaHUD.generated.h"

class UGolemColorVoidComponent;

/**
 * M0 presentation of the colour rules (DEV_SPEC section 15): on every void the symbols of the
 * elements still missing, the countdown of its mixing window, and the signal marker with an arrow
 * at the screen edge when the void is out of view. There is no Xenon health bar: progress reads as
 * voids filled. Shapes are placeholders until art decides: ^ Earth, O Sky, ~ Sea.
 */
UCLASS()
class THREEDREAMGOLEMS_API AGolemColorArenaHUD : public AGolemPartyArenaHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

	/** What the marker of one void reads right now; empty for a void that shows nothing. Tests read it too. */
	static FString DescribeVoid(const UGolemColorVoidComponent* Void, double HostNow);

protected:
	virtual FText GetArenaTitle() const override;
	virtual FText GetObjectiveText() const override;
	virtual FText GetCombatHelp() const override;
	virtual FText GetPartyHelp() const override;
	virtual FText GetRecoveryHelp() const override;

private:
	void DrawVoid(const UGolemColorVoidComponent* Void, double HostNow);
	void DrawCountdownRing(const FVector2D& Center, float Radius, float Fraction, const FLinearColor& Color);
	void DrawEdgeArrow(const FVector& WorldLocation, const FLinearColor& Color, const FString& Label);
};
