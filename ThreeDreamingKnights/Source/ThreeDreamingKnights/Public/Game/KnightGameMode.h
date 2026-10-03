#pragma once

#include "CoreMinimal.h"
#include "Framework/KnightPartyGameMode.h"
#include "KnightGameMode.generated.h"

/** Project composition root for the canonical party and the cumulative playable scenarios. */
UCLASS()
class THREEDREAMINGKNIGHTS_API AKnightGameMode : public AKnightPartyGameMode
{
	GENERATED_BODY()

public:
	AKnightGameMode();
	virtual FKnightResult TryRestartDrill(APlayerController* Player) override;
};
