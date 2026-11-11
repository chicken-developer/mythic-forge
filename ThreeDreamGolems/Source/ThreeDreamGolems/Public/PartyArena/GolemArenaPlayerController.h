#pragma once

#include "CoreMinimal.h"
#include "Commands/GolemCompanionCommand.h"
#include "Player/GolemPlayerController.h"
#include "GolemArenaPlayerController.generated.h"

class UInputAction;
class UInputMappingContext;
class UEnhancedInputLocalPlayerSubsystem;
class UEnhancedInputComponent;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnGolemArenaCommandResult, const FGolemResult&);

UCLASS()
class THREEDREAMGOLEMS_API AGolemArenaPlayerController : public AGolemPlayerController
{
	GENERATED_BODY()

public:
	virtual void SetPlayer(UPlayer* InPlayer) override;
	void RequestCompanionCommand(EGolemCompanionCommand Command);
	const FGolemResult& GetLastCompanionCommandResult() const;
	EGolemCompanionCommand GetLastCompanionCommand() const;
	FOnGolemArenaCommandResult OnCompanionCommandResult;

protected:
	virtual void SetupInputComponent() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;

private:
	void HandleCommand(EGolemCompanionCommand Command);
	void HandleCycleCommand();
	void InstallCommandMappings();
	void RemoveCommandMappings();
	FGolemResult ApplyCompanionCommand(EGolemCompanionCommand Command,
		const FGolemEntityId& ExpectedGolemId, const FGolemEntityId& FocusId);
	UFUNCTION(Server, Reliable)
	void ServerRequestCompanionCommand(EGolemCompanionCommand Command,
		FGolemEntityId ExpectedGolemId, FGolemEntityId FocusId);
	UFUNCTION(Client, Reliable)
	void ClientReceiveCompanionCommandResult(EGolemCompanionCommand Command, FGolemResult Result);
	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> CommandMappings;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UInputAction>> CommandActions;
	TWeakObjectPtr<UEnhancedInputLocalPlayerSubsystem> MappingSubsystem;
	TWeakObjectPtr<UEnhancedInputComponent> BoundCommandInput;
	TArray<uint32> BindingHandles;
	FGolemResult LastCompanionCommandResult;
	EGolemCompanionCommand LastCompanionCommand = EGolemCompanionCommand::Follow;
};
