#pragma once
#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
class UWorld;
int32 ThanhACalibration(const TSharedPtr<FJsonObject>& Scene,const FString& Directory,bool CaptureOnly,
    TFunctionRef<bool(UWorld*,const TSharedPtr<FJsonObject>&,const FString&)> Capture);
