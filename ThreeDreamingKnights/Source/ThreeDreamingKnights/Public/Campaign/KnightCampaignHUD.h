#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "KnightCampaignHUD.generated.h"

UCLASS()
class THREEDREAMINGKNIGHTS_API AKnightCampaignHUD : public AHUD
{
	GENERATED_BODY()
public:
	virtual void DrawHUD() override;
private:
	float DrawWrapped(const FText& Text, float X, float Y, float Width, float Scale, FLinearColor Color);
};
