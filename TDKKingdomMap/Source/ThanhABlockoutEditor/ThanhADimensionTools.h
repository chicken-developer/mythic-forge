#pragma once
#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
class UWorld;
class UThanhADimensionComponent;
struct FThanhAMeasurement
{
    bool Valid=false;
    FVector SizeM=FVector::ZeroVector;
    FVector CenterCm=FVector::ZeroVector;
    double BaseM=0,TopM=0;
    FString Scope,Detail;
    bool Changed=false;
};
FThanhAMeasurement ThanhAMeasure(const UThanhADimensionComponent* Metadata,bool WholeBuilding);
void ThanhAInitializeGroupDimensions(UWorld* World);
TSharedRef<FJsonObject> ThanhAVerifyDimensions(UWorld* World);
FString ThanhADimensionLabel(const UThanhADimensionComponent* Metadata,const FThanhAMeasurement& Measure,bool Elevation);
