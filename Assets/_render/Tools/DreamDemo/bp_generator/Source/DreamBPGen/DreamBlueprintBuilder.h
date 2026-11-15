// One-off editor tool: writes the DreamDemo logic as pure Blueprint assets, so the demo runs
// in a project without any DreamDemo C++ (see Tools/DreamDemo/bp_generator/README.md).

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DreamBlueprintBuilder.generated.h"

UCLASS()
class DREAMBPGEN_API UDreamBlueprintBuilder : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Creates BP_DreamWorldState and BP_DreamInstancedField under PackagePath. Returns an error text, empty on success. */
	UFUNCTION(BlueprintCallable, Category = "DreamDemo")
	static FString BuildDreamBlueprints(const FString& PackagePath);
};
