#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "GolemCampaignHUD.generated.h"

UCLASS()
class THREEDREAMGOLEMS_API AGolemCampaignHUD : public AHUD
{
	GENERATED_BODY()
public:
	virtual void DrawHUD() override;
private:
	float DrawWrapped(const FText& Text, float X, float Y, float Width, float Scale, FLinearColor Color);
};
