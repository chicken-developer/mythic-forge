#pragma once
#include "CoreMinimal.h"
bool ThanhAEnvironmentAudit();
bool ThanhAEnvironmentRun(const FString& Mode, TFunctionRef<bool(UWorld*,const TSharedPtr<FJsonObject>&,const FString&)> Capture);
