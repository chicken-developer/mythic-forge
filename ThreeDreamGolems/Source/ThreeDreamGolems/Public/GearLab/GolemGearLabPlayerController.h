#pragma once

#include "CoreMinimal.h"
#include "SkillLab/GolemSkillLabPlayerController.h"
#include "GolemGearLabPlayerController.generated.h"

class UInputAction;
class UInputMappingContext;
class UEnhancedInputComponent;
class UEnhancedInputLocalPlayerSubsystem;

UCLASS()
class THREEDREAMGOLEMS_API AGolemGearLabPlayerController : public AGolemSkillLabPlayerController
{
	GENERATED_BODY()
public:
	virtual void RequestGearFixture(FName Action, FGuid RequestId) override;
	void RequestQuickUse(int32 QuickSlot);
protected:
	virtual void SetupInputComponent() override;
	virtual void EndPlay(EEndPlayReason::Type Reason) override;
private:
	UFUNCTION(Server, Reliable)
	void ServerRequestGearFixture(FName Action, FGuid RequestId, FGolemEntityId ExpectedGolemId);
	UFUNCTION(Client, Reliable)
	void ClientReceiveGearFixture(FGuid RequestId, FGolemResult Result);
	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> QuickMappings;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UInputAction>> QuickActions;
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> QuickModifier;
	TWeakObjectPtr<UEnhancedInputComponent> BoundQuickInput;
	TWeakObjectPtr<UEnhancedInputLocalPlayerSubsystem> QuickSubsystem;
	TArray<uint32> QuickHandles;
};
