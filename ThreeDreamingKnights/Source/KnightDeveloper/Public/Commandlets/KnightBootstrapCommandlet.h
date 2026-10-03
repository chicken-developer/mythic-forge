#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "KnightBootstrapCommandlet.generated.h"

/** Authors the foundation fixtures using the installed editor's package serialization. */
UCLASS()
class KNIGHTDEVELOPER_API UKnightBootstrapCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UKnightBootstrapCommandlet();

	virtual int32 Main(const FString& Params) override;
};
