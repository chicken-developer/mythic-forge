#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTagContainer.h"
#include "Types/KnightResult.h"
#include "Types/KnightStableIds.h"
#include "KnightPlayerController.generated.h"

class AKnightCharacter;
class UEnhancedInputComponent;
class UEnhancedInputLocalPlayerSubsystem;
class UKnightAbilitySystemComponent;
class UKnightCombatComponent;
class UKnightCombatProfileDefinition;
class UKnightInputConfig;
struct FInputActionValue;

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnKnightControlledKnightChanged, AKnightCharacter*, AKnightCharacter*);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnKnightCommandResult, const FKnightResult&);

/** Human input follows possession; it never owns the canonical Knight identity. */
UCLASS()
class KNIGHTFRAMEWORK_API AKnightPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AKnightPlayerController();
	AKnightCharacter* GetControlledKnight() const;
	UKnightInputConfig* GetKnightInputConfig() const;
	const FKnightResult& GetLastCommandResult() const;
	void RequestSwitchKnight(int32 DesiredSlot);
	void RequestRestartDrill();
	void RequestAbilityInput(const FGameplayTag& InputTag, bool bPressed);
	void RequestInteract();
	void ResetControlInput();
	virtual void SetPawn(APawn* InPawn) override;
	virtual void SetPlayer(UPlayer* InPlayer) override;

	FOnKnightControlledKnightChanged OnControlledKnightChanged;
	FOnKnightCommandResult OnCommandResult;

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
	void ServerRequestSwitchKnight(int32 DesiredSlot);

	UFUNCTION(Server, Reliable)
	void ServerRequestRestartDrill();

	UFUNCTION(Server, Reliable)
	void ServerRequestAbilityInput(FGameplayTag InputTag, bool bPressed, FKnightEntityId ExpectedKnightId);

	UFUNCTION(Server, Reliable)
	void ServerRequestInteract(FKnightEntityId ExpectedKnightId);

	UFUNCTION(Client, Reliable)
	void ClientReceiveCommandResult(const FKnightResult& Result);

	UFUNCTION(Client, Reliable)
	void ClientResetControlInput();

	void InitializeInputConfig();
	void UpdateControlledKnight();
	void UpdateLocalMapping();
	void RemoveLocalMapping();
	void UpdateCombatInputSource();
	void RemoveCombatInputSource();
	void UpdateCombatMapping();
	void HandleCombatProfileChanged(UKnightCombatProfileDefinition* Profile);
	void RemoveOwnedInputBindings();
	void BindAbilityInput(const FGameplayTag& InputTag);
	void UpdateAbilityResultSource();
	void RemoveAbilityResultSource();
	void HandleAbilityResult(const FKnightResult& Result);
	FKnightResult ValidateKnightCommand(const FKnightEntityId& ExpectedKnightId, AKnightCharacter*& OutKnight) const;
	void HandleMove(const FInputActionValue& Value);
	void HandleLook(const FInputActionValue& Value);
	void HandleJumpStarted();
	void HandleJumpEnded();
	void HandleSelectKnight(int32 DesiredSlot);
	void HandleRestartDrill();
	void HandleAbilityPressed(FGameplayTag InputTag);
	void HandleAbilityReleased(FGameplayTag InputTag);
	void HandleInteract();

	UPROPERTY(Transient)
	TObjectPtr<UKnightInputConfig> InputConfig;

	UPROPERTY(Transient)
	TWeakObjectPtr<AKnightCharacter> ControlledKnight;

	UPROPERTY(Transient)
	TWeakObjectPtr<UEnhancedInputComponent> BoundInputComponent;

	UPROPERTY(Transient)
	TWeakObjectPtr<UEnhancedInputLocalPlayerSubsystem> MappingSubsystem;

	UPROPERTY(Transient)
	TWeakObjectPtr<UKnightAbilitySystemComponent> AbilityResultSource;
	UPROPERTY(Transient)
	TWeakObjectPtr<UKnightCombatComponent> CombatInputSource;

	UPROPERTY(Transient)
	FKnightEntityId AbilityResultKnightId;

	UPROPERTY(Transient)
	FKnightResult LastCommandResult;

	UPROPERTY(EditDefaultsOnly, Category = "Knight|Input", meta = (ClampMin = "0"))
	int32 GameplayMappingPriority = 0;

	UPROPERTY(EditDefaultsOnly, Category = "Knight|Input", meta = (ClampMin = "0.01"))
	float LookSensitivity = 1.0f;

	TArray<uint32> InputBindingHandles;
	FDelegateHandle AbilityResultHandle;
	FDelegateHandle CombatProfileHandle;
	bool bCombatMappingAdded = false;
	bool bProcessingAbilityInput = false;
	bool bEndingPlay = false;
};
