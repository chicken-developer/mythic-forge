#pragma once

#include "CoreMinimal.h"
#include "Framework/GolemPartyGameMode.h"
#include "GolemGameMode.generated.h"

/** Project composition root for the canonical party and the cumulative playable scenarios. */
UCLASS()
class THREEDREAMGOLEMS_API AGolemGameMode : public AGolemPartyGameMode
{
	GENERATED_BODY()

public:
	AGolemGameMode();
	virtual FGolemResult TryRestartDrill(APlayerController* Player) override;
};
