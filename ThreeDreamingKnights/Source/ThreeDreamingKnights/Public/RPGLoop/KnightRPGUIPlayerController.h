#pragma once

#include "CoreMinimal.h"
#include "RPGLoop/KnightRPGPlayerController.h"
#include "KnightRPGUIPlayerController.generated.h"

class AKnightCharacter;
class AKnightRPGGameState;
class AKnightRPGStateActor;
class UEnhancedInputComponent;
class UEnhancedInputLocalPlayerSubsystem;
class UInputAction;
class UInputMappingContext;
class UKnightAbilitySystemComponent;
class UKnightRPGPanel;
enum class EKnightRPGPanelAction : uint8;
enum class EKnightLifeState : uint8;
struct FKnightSourceProjection;
struct FKnightRPGPanelSnapshot;

DECLARE_MULTICAST_DELEGATE(FOnKnightRPGPresentationChanged);

/** Native local presentation forwards intent to the inherited authoritative RPC facade. */
UCLASS()
class THREEDREAMINGKNIGHTS_API AKnightRPGUIPlayerController : public AKnightRPGPlayerController
{
	GENERATED_BODY()

public:
	virtual void SetPlayer(UPlayer* InPlayer) override;
	void RequestPickup();
	void ToggleRPGPanel();
	UKnightRPGPanel* GetRPGPanel() const;
	FText GetRPGFeedback() const;
	FOnKnightRPGPresentationChanged OnRPGPresentationChanged;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;
	virtual void SetupInputComponent() override;

private:
	void InstallRPGMappings();
	void RemoveRPGMappings();
	void BindRPGSources();
	void UnbindRPGSources();
	void HandleRPGStateReady(AKnightRPGStateActor* InState);
	void HandleControlledKnight(AKnightCharacter* Previous, AKnightCharacter* Current);
	void UnbindDisplayedKnight();
	void HandleLifeChanged(EKnightLifeState Previous, EKnightLifeState Current);
	void HandleProjectionChanged(const FKnightSourceProjection& Projection);
	void HandlePanelAction(EKnightRPGPanelAction Action);
	void HandleRPGResult(const FKnightRPGCommandResult& Outcome);
	void HandleLoopResult(const FKnightResult& Result);
	void RefreshRPGPresentation();
	void CloseRPGPanel();
	FKnightRPGCommand CreateCurrentCommand(EKnightRPGOperation Operation) const;
	FKnightRPGPanelSnapshot BuildPanelSnapshot() const;

	UPROPERTY(Transient)
	TObjectPtr<UKnightRPGPanel> RPGPanel;
	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> RPGMappings;
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> PanelAction;
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> PickupAction;

	TWeakObjectPtr<UEnhancedInputComponent> BoundRPGInput;
	TWeakObjectPtr<UEnhancedInputLocalPlayerSubsystem> MappingSubsystem;
	TWeakObjectPtr<AKnightRPGGameState> RPGGameState;
	TWeakObjectPtr<AKnightRPGStateActor> RPGState;
	TWeakObjectPtr<AKnightCharacter> DisplayedKnight;
	TWeakObjectPtr<UKnightAbilitySystemComponent> DisplayedASC;
	TArray<uint32> RPGInputHandles;
	FDelegateHandle StateReadyHandle;
	FDelegateHandle InventoryHandle;
	FDelegateHandle RPGHandle;
	FDelegateHandle LoopHandle;
	FDelegateHandle ControlHandle;
	FDelegateHandle ResultHandle;
	FDelegateHandle LoopResultHandle;
	FDelegateHandle LifeHandle;
	FDelegateHandle ProjectionHandle;
	FDelegateHandle PanelHandle;
	FTimerHandle SourceReadinessTimer;
	FText Feedback;
	int32 BindingAttempts = 0;
	bool bEndingUIPlay = false;
};
