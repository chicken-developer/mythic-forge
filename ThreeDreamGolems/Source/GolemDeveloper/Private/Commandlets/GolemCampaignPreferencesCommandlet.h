#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "GolemCampaignPreferencesCommandlet.generated.h"

/** Separate-process preference persistence probe with an explicit evidence-only config path. */
UCLASS()
class UGolemCampaignPreferencesCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UGolemCampaignPreferencesCommandlet();
	virtual int32 Main(const FString& Params) override;
};
