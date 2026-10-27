#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "KnightCampaignPreferencesCommandlet.generated.h"

/** Separate-process preference persistence probe with an explicit evidence-only config path. */
UCLASS()
class UKnightCampaignPreferencesCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UKnightCampaignPreferencesCommandlet();
	virtual int32 Main(const FString& Params) override;
};
