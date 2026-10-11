#pragma once
#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "ThanhABlockoutSettings.generated.h"

UCLASS(config=EditorPerProjectUserSettings, meta=(DisplayName="Thanh A Blockout"))
class UThanhABlockoutSettings : public UDeveloperSettings
{
    GENERATED_BODY()
public:
    virtual FName GetContainerName() const override { return TEXT("Editor"); }
    virtual FName GetCategoryName() const override { return TEXT("LevelEditor"); }
    UPROPERTY(EditAnywhere,config,Category="Dimensions") bool ShowDimensions=true;
    UPROPERTY(EditAnywhere,config,Category="Dimensions") bool SelectedOnly=true;
    UPROPERTY(EditAnywhere,config,Category="Dimensions") bool ShowElevation=true;
    UPROPERTY(EditAnywhere,config,Category="Dimensions",meta=(ClampMin=1,ClampMax=256)) int32 MaxLabels=64;
    UPROPERTY(EditAnywhere,config,Category="Grid") bool ShowGrid=true;
    UPROPERTY(EditAnywhere,config,Category="Grid",meta=(ClampMin=1,Units="cm")) float GridSizeCm=100;
    UPROPERTY(EditAnywhere,config,Category="Grid",meta=(ClampMin=1,Units="cm")) float MinorGridSizeCm=10;
};
