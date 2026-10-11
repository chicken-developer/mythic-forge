#pragma once

#include "CoreMinimal.h"
#include "RPGLoop/KnightRPGUIPlayerController.h"
#include "KnightSkillLabPlayerController.generated.h"

class UInputMappingContext;
class UInputAction;
class UEnhancedInputComponent;
class UEnhancedInputLocalPlayerSubsystem;

/** Four semantic skill slots route to the same GAS actions used by companion StateTree. */
UCLASS()
class THREEDREAMINGKNIGHTS_API AKnightSkillLabPlayerController : public AKnightRPGUIPlayerController
{
	GENERATED_BODY()

public:
	void RequestSkillSlot(int32 Slot);

protected:
	virtual void SetupInputComponent() override;
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;

private:
	UFUNCTION(Server, Reliable)
	void ServerRequestSkillSlot(int32 Slot, FKnightEntityId ExpectedKnightId);
	void RequestHoldCommand();
	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> SkillMappings;
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> SkillModifier;
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> HoldCommand;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UInputAction>> SkillActions;
	TWeakObjectPtr<UEnhancedInputComponent> BoundSkillInput;
	TWeakObjectPtr<UEnhancedInputLocalPlayerSubsystem> SkillSubsystem;
	TArray<uint32> BindingHandles;
};
