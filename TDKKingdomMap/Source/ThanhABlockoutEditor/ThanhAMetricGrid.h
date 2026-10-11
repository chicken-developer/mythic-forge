#pragma once
#include "CoreMinimal.h"
class UMaterialInterface;
class FJsonObject;
UMaterialInterface* ThanhAMetricMaterial(const FString& Root,const FString& Hex,const TSharedPtr<FJsonObject>& Scene);
