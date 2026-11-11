#pragma once

#include "Definitions/GolemItemDefinition.h"
#include "Models/GolemRPGModels.h"
#include "GolemRuneDefinition.generated.h"

/** A rune is an unstackable GolemItem and tunes the existing GAS rune action. */
UCLASS(BlueprintType)
class GOLEMITEMS_API UGolemRuneDefinition : public UGolemItemDefinition
{
	GENERATED_BODY()

public:
	UGolemRuneDefinition();
	const FGolemRuneTuning& GetRuneTuning() const;
	FGolemResult TrySetRuneTuning(const FGolemRuneTuning& InTuning);
	FName GetAttachmentRuneId() const;
	FGolemResult TrySetAttachmentRune(FName InRuneId);
	virtual FGolemResult ValidateItem() const override;

private:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Items|Rune", meta = (AllowPrivateAccess = "true"))
	FGolemRuneTuning RuneTuning;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Items|Rune", meta = (AllowPrivateAccess = "true"))
	FName AttachmentRuneId;
};
