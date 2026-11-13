#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTagContainer.h"
#include "Types/GolemResult.h"
#include "Types/GolemStableIds.h"
#include "GolemPlayerController.generated.h"

class AGolemCharacter;
class UEnhancedInputComponent;
class UEnhancedInputLocalPlayerSubsystem;
class UGolemAbilitySystemComponent;
class UGolemCombatComponent;
class UGolemCombatProfileDefinition;
class UGolemInputConfig;
struct FInputActionValue;

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnGolemControlledGolemChanged, AGolemCharacter*, AGolemCharacter*);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnGolemCommandResult, const FGolemResult&);

/** Human input follows possession; it never owns the canonical Golem identity. */
UCLASS()
class GOLEMFRAMEWORK_API AGolemPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AGolemPlayerController();
	AGolemCharacter* GetControlledGolem() const;
	UGolemInputConfig* GetGolemInputConfig() const;
	int32 GetGameplayMappingPriority() const;
	const FGolemResult& GetLastCommandResult() const;
	void RequestSwitchGolem(int32 DesiredSlot);
	void RequestRestartDrill();
	void RequestAbilityInput(const FGameplayTag& InputTag, bool bPressed);
	void RequestInteract();
	void ResetControlInput();
	void SetLocalInputPreferences(float Sensitivity, bool bInvertY, bool bToggleBlock);
	virtual void SetPawn(APawn* InPawn) override;
	virtual void SetPlayer(UPlayer* InPlayer) override;

	FOnGolemControlledGolemChanged OnControlledGolemChanged;
	FOnGolemCommandResult OnCommandResult;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void SetupInputComponent() override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;
	virtual void OnRep_Pawn() override;
	virtual void AcknowledgePossession(APawn* InPawn) override;
	virtual void PawnLeavingGame() override;

private:
	UFUNCTION(Server, Reliable)
	void ServerRequestSwitchGolem(int32 DesiredSlot);

	UFUNCTION(Server, Reliable)
	void ServerRequestRestartDrill();

	UFUNCTION(Server, Reliable)
	void ServerRequestAbilityInput(FGameplayTag InputTag, bool bPressed, FGolemEntityId ExpectedGolemId);

	UFUNCTION(Server, Reliable)
	void ServerRequestInteract(FGolemEntityId ExpectedGolemId);

	UFUNCTION(Client, Reliable)
	void ClientReceiveCommandResult(const FGolemResult& Result);

	UFUNCTION(Client, Reliable)
	void ClientResetControlInput();

	void InitializeInputConfig();
	void UpdateControlledGolem();
	void UpdateLocalMapping();
	void RemoveLocalMapping();
	void UpdateCombatInputSource();
	void RemoveCombatInputSource();
	void UpdateCombatMapping();
	void HandleCombatProfileChanged(UGolemCombatProfileDefinition* Profile);
	void RemoveOwnedInputBindings();
	void BindAbilityInput(const FGameplayTag& InputTag);
	void UpdateAbilityResultSource();
	void RemoveAbilityResultSource();
	void HandleAbilityResult(const FGolemResult& Result);
	FGolemResult ValidateGolemCommand(const FGolemEntityId& ExpectedGolemId, AGolemCharacter*& OutGolem) const;
	void HandleMove(const FInputActionValue& Value);
	void HandleLook(const FInputActionValue& Value);
	void HandleJumpStarted();
	void HandleJumpEnded();
	void HandleSelectGolem(int32 DesiredSlot);
	void HandleRestartDrill();
	void HandleAbilityPressed(FGameplayTag InputTag);
	void HandleAbilityReleased(FGameplayTag InputTag);
	void HandleInteract();

	UPROPERTY(Transient)
	TObjectPtr<UGolemInputConfig> InputConfig;

	UPROPERTY(Transient)
	TWeakObjectPtr<AGolemCharacter> ControlledGolem;

	UPROPERTY(Transient)
	TWeakObjectPtr<UEnhancedInputComponent> BoundInputComponent;

	UPROPERTY(Transient)
	TWeakObjectPtr<UEnhancedInputLocalPlayerSubsystem> MappingSubsystem;

	UPROPERTY(Transient)
	TWeakObjectPtr<UGolemAbilitySystemComponent> AbilityResultSource;
	UPROPERTY(Transient)
	TWeakObjectPtr<UGolemCombatComponent> CombatInputSource;

	UPROPERTY(Transient)
	FGolemEntityId AbilityResultGolemId;

	UPROPERTY(Transient)
	FGolemResult LastCommandResult;

	UPROPERTY(EditDefaultsOnly, Category = "Golem|Input", meta = (ClampMin = "0"))
	int32 GameplayMappingPriority = 0;

	UPROPERTY(EditDefaultsOnly, Category = "Golem|Input", meta = (ClampMin = "0.01"))
	float LookSensitivity = 1.0f;
	bool bInvertLookY = false;
	bool bToggleBlockInput = false;

	TArray<uint32> InputBindingHandles;
	FDelegateHandle AbilityResultHandle;
	FDelegateHandle CombatProfileHandle;
	bool bCombatMappingAdded = false;
	bool bProcessingAbilityInput = false;
	bool bEndingPlay = false;
};
