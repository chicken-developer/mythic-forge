#pragma once

#include "CoreMinimal.h"
#include "PartyArena/GolemPartyArenaHUD.h"
#include "GolemStoryHUD.generated.h"

UCLASS()
class THREEDREAMGOLEMS_API AGolemStoryHUD : public AGolemPartyArenaHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

protected:
	virtual FText GetArenaTitle() const override;
	virtual FText GetCombatHelp() const override;
};
