#pragma once

#include "CoreMinimal.h"
#include "CommonButtonBase.h"
#include "KnightRPGActionButton.generated.h"

class UBorder;
class UTextBlock;

UCLASS()
class KNIGHTUI_API UKnightRPGActionButton : public UCommonButtonBase
{
	GENERATED_BODY()

public:
	UKnightRPGActionButton();
	virtual bool Initialize() override;
	void SetCaption(const FText& InCaption);

protected:
	virtual void NativeOnHovered() override;
	virtual void NativeOnUnhovered() override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> Caption;
	UPROPERTY(Transient)
	TObjectPtr<UBorder> Background;
	FText CaptionText;
};
