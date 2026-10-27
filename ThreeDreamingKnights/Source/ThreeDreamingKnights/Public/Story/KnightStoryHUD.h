#pragma once

#include "CoreMinimal.h"
#include "PartyArena/KnightPartyArenaHUD.h"
#include "KnightStoryHUD.generated.h"

UCLASS()
class THREEDREAMINGKNIGHTS_API AKnightStoryHUD : public AKnightPartyArenaHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

protected:
	virtual FText GetArenaTitle() const override;
	virtual FText GetCombatHelp() const override;
};
