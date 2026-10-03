#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "UObject/PrimaryAssetId.h"
#include "KnightSettings.generated.h"

UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Knight Foundation"))
class KNIGHTCORE_API UKnightSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UKnightSettings();
	virtual FName GetCategoryName() const override;

	/** Package roots for native definition discovery and duplicate-ID validation. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Knight|Core|Assets")
	TArray<FString> DefinitionRoots;

	UPROPERTY(Config, EditDefaultsOnly, Category = "Knight|Core|Assets")
	FPrimaryAssetId FoundationDefinition;

	/** The canonical party count is an architecture invariant, not a gameplay tuning variable. */
	static constexpr int32 CanonicalPartySize = 3;
};
