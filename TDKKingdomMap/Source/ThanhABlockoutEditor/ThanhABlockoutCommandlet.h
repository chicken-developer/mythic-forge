#pragma once
#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "ThanhABlockoutCommandlet.generated.h"

UCLASS()
class UThanhABlockoutCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    UThanhABlockoutCommandlet();
    virtual int32 Main(const FString& Params) override;
};
