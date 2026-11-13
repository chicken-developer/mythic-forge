#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "GolemBootstrapCommandlet.generated.h"

/** Authors the foundation fixtures using the installed editor's package serialization. */
UCLASS()
class GOLEMDEVELOPER_API UGolemBootstrapCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UGolemBootstrapCommandlet();

	virtual int32 Main(const FString& Params) override;
};
