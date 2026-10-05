#pragma once

#include "CoreMinimal.h"
#include "Commands/KnightCompanionCommand.h"
#include "Player/KnightPlayerController.h"
#include "KnightArenaPlayerController.generated.h"

class UInputAction;
class UInputMappingContext;
class UEnhancedInputLocalPlayerSubsystem;
class UEnhancedInputComponent;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnKnightArenaCommandResult, const FKnightResult&);

UCLASS()
class THREEDREAMINGKNIGHTS_API AKnightArenaPlayerController : public AKnightPlayerController
{
	GENERATED_BODY()

public:
	virtual void SetPlayer(UPlayer* InPlayer) override;
	void RequestCompanionCommand(EKnightCompanionCommand Command);
	const FKnightResult& GetLastCompanionCommandResult() const;
	EKnightCompanionCommand GetLastCompanionCommand() const;
	FOnKnightArenaCommandResult OnCompanionCommandResult;

protected:
	virtual void SetupInputComponent() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;

private:
	void HandleCommand(EKnightCompanionCommand Command);
	void HandleCycleCommand();
	void InstallCommandMappings();
	void RemoveCommandMappings();
	FKnightResult ApplyCompanionCommand(EKnightCompanionCommand Command,
		const FKnightEntityId& ExpectedKnightId, const FKnightEntityId& FocusId);
	UFUNCTION(Server, Reliable)
	void ServerRequestCompanionCommand(EKnightCompanionCommand Command,
		FKnightEntityId ExpectedKnightId, FKnightEntityId FocusId);
	UFUNCTION(Client, Reliable)
	void ClientReceiveCompanionCommandResult(EKnightCompanionCommand Command, FKnightResult Result);
	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> CommandMappings;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UInputAction>> CommandActions;
	TWeakObjectPtr<UEnhancedInputLocalPlayerSubsystem> MappingSubsystem;
	TWeakObjectPtr<UEnhancedInputComponent> BoundCommandInput;
	TArray<uint32> BindingHandles;
	FKnightResult LastCompanionCommandResult;
	EKnightCompanionCommand LastCompanionCommand = EKnightCompanionCommand::Follow;
};
