#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "UObject/PrimaryAssetId.h"
#include "GolemSettings.generated.h"

UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Golem Foundation"))
class GOLEMCORE_API UGolemSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UGolemSettings();
	virtual FName GetCategoryName() const override;

	/** Package roots for native definition discovery and duplicate-ID validation. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Golem|Core|Assets")
	TArray<FString> DefinitionRoots;

	UPROPERTY(Config, EditDefaultsOnly, Category = "Golem|Core|Assets")
	FPrimaryAssetId FoundationDefinition;

	/** The canonical party count is an architecture invariant, not a gameplay tuning variable. */
	static constexpr int32 CanonicalPartySize = 3;
};
