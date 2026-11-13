#pragma once

#include "CoreMinimal.h"
#include "Commands/GolemCompanionCommand.h"
#include "Player/GolemPlayerController.h"
#include "GolemArenaPlayerController.generated.h"

class UInputAction;
class UInputMappingContext;
class UEnhancedInputLocalPlayerSubsystem;
class UEnhancedInputComponent;
class UGolemColorVoidComponent;

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

	/**
	 * Signal button (DEV_SPEC section 12): marks the void the camera points at for the whole team.
	 * Aim is forgiving: the void nearest to the centre of the screen is taken.
	 */
	void RequestSignal();

	/** Host-side validation and placement of one signal; the RPC and tests both go through it. */
	FGolemResult ApplySignal(UGolemColorVoidComponent* Void);
	const FGolemResult& GetLastSignalResult() const { return LastSignalResult; }

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
	UFUNCTION(Server, Reliable)
	void ServerSignalVoid(UGolemColorVoidComponent* Void);
	UFUNCTION(Client, Reliable)
	void ClientReceiveSignalResult(FGolemResult Result);

	/** A void farther than this from the Golem cannot be signalled. */
	UPROPERTY(EditDefaultsOnly, Category = "Golem|Arena|Signal", meta = (ClampMin = "100.0", Units = "cm"))
	float SignalRange = 4000.0f;

	/** How far from the centre of the screen a void may be and still be picked. */
	UPROPERTY(EditDefaultsOnly, Category = "Golem|Arena|Signal", meta = (ClampMin = "0.0", ClampMax = "89.0", Units = "deg"))
	float SignalAimDegrees = 14.0f;
	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> CommandMappings;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UInputAction>> CommandActions;
	TWeakObjectPtr<UEnhancedInputLocalPlayerSubsystem> MappingSubsystem;
	TWeakObjectPtr<UEnhancedInputComponent> BoundCommandInput;
	TArray<uint32> BindingHandles;
	FGolemResult LastCompanionCommandResult;
	FGolemResult LastSignalResult;
	EGolemCompanionCommand LastCompanionCommand = EGolemCompanionCommand::Follow;
};
