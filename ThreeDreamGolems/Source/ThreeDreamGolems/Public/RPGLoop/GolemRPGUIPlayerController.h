#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectTypes.h"
#include "RPGLoop/GolemRPGPlayerController.h"
#include "GolemRPGUIPlayerController.generated.h"

class AGolemCharacter;
class AGolemRPGGameState;
class AGolemRPGStateActor;
class AGolemGearLabState;
class UEnhancedInputComponent;
class UEnhancedInputLocalPlayerSubsystem;
class UInputAction;
class UInputMappingContext;
class UGolemAbilitySystemComponent;
class UGolemCombatComponent;
class UGolemRPGPanel;
class UGolemSkillRuntimeComponent;
enum class EGolemRPGPanelAction : uint8;
enum class EGolemLifeState : uint8;
struct FGolemSourceProjection;
struct FGolemRPGPanelSnapshot;
struct FGolemRPGPanelCommand;
struct FGolemSkillBuild;
struct FGolemGearPanelIntent;
struct FGolemGearQuickUseRow;

DECLARE_MULTICAST_DELEGATE(FOnGolemRPGPresentationChanged);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnGolemGearFixtureResult, const FGuid&, const FGolemResult&);

/** Native local presentation forwards intent to the inherited authoritative RPC facade. */
UCLASS()
class THREEDREAMGOLEMS_API AGolemRPGUIPlayerController : public AGolemRPGPlayerController
{
	GENERATED_BODY()

public:
	virtual void SetPlayer(UPlayer* InPlayer) override;
	void RequestPickup();
	void ToggleRPGPanel();
	UGolemRPGPanel* GetRPGPanel() const;
	FText GetRPGFeedback() const;
	virtual void RequestGearFixture(FName Action, FGuid RequestId);
	TArray<FGolemGearQuickUseRow> GetQuickUseReadModel() const;
	FOnGolemRPGPresentationChanged OnRPGPresentationChanged;
	FOnGolemGearFixtureResult OnGearFixtureResult;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;
	virtual void SetupInputComponent() override;

private:
	friend class FGolemRPGUIClosedReceiptTest;
	void InstallRPGMappings();
	void RemoveRPGMappings();
	void BindRPGSources();
	void UnbindRPGSources();
	void HandleRPGStateReady(AGolemRPGStateActor* InState);
	void HandleControlledGolem(AGolemCharacter* Previous, AGolemCharacter* Current);
	void UnbindDisplayedGolem();
	void HandleLifeChanged(EGolemLifeState Previous, EGolemLifeState Current);
	void HandleProjectionChanged(const FGolemSourceProjection& Projection);
	void HandlePanelAction(EGolemRPGPanelAction Action);
	void HandlePanelCommand(const FGolemRPGPanelCommand& Intent);
	void HandleBuildPreview(const FGolemSkillBuild& Build);
	void HandleGearCommand(const FGolemGearPanelIntent& Intent);
	void HandleGearPreview(const FGolemGearPanelIntent& Intent);
	void HandleGearFixtureResult(const FGuid& RequestId, const FGolemResult& Result);
	void BindGearSources();
	void UnbindGearViewedGolem();
	AGolemCharacter* FindGearGolem() const;
	void HandleRPGResult(const FGolemRPGCommandResult& Outcome);
	void HandleLoopResult(const FGolemResult& Result);
	void RefreshRPGPresentation();
	void ReconcileClosedPanelReceipt(const FGuid& RequestId);
	void CloseRPGPanel();
	FGolemRPGCommand CreateCurrentCommand(EGolemRPGOperation Operation) const;
	FGolemRPGPanelSnapshot BuildPanelSnapshot() const;

	UPROPERTY(Transient)
	TObjectPtr<UGolemRPGPanel> RPGPanel;
	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> RPGMappings;
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> PanelAction;
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> PickupAction;

	TWeakObjectPtr<UEnhancedInputComponent> BoundRPGInput;
	TWeakObjectPtr<UEnhancedInputLocalPlayerSubsystem> MappingSubsystem;
	TWeakObjectPtr<AGolemRPGGameState> RPGGameState;
	TWeakObjectPtr<AGolemRPGStateActor> RPGState;
	TWeakObjectPtr<AGolemGearLabState> GearLabState;
	TWeakObjectPtr<AGolemCharacter> GearViewedGolem;
	TWeakObjectPtr<UGolemAbilitySystemComponent> GearViewedASC;
	TWeakObjectPtr<UGolemSkillRuntimeComponent> GearViewedRuntime;
	TWeakObjectPtr<UGolemCombatComponent> GearViewedCombat;
	TArray<TPair<FGameplayAttribute, FDelegateHandle>> GearAttributeHandles;
	FDelegateHandle GearLifeHandle;
	FDelegateHandle GearRuntimeHandle;
	FDelegateHandle GearCombatHandle;
	FDelegateHandle GearProjectionHandle;
	FTimerHandle GearReadinessTimer;
	int32 GearBindingAttempts = 0;
	TWeakObjectPtr<AGolemCharacter> DisplayedGolem;
	TWeakObjectPtr<UGolemAbilitySystemComponent> DisplayedASC;
	TWeakObjectPtr<UGolemSkillRuntimeComponent> DisplayedRuntime;
	TWeakObjectPtr<UGolemCombatComponent> DisplayedCombat;
	TArray<TPair<FGameplayAttribute, FDelegateHandle>> AttributeHandles;
	FDelegateHandle RuntimeStateHandle;
	FDelegateHandle CombatStateHandle;
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
	FDelegateHandle PanelCommandHandle;
	FDelegateHandle PreviewHandle;
	FDelegateHandle GearCommandHandle;
	FDelegateHandle GearPreviewHandle;
	FDelegateHandle GearStateHandle;
	FDelegateHandle GearFixtureResultHandle;
	FGolemEntityId GearSelectedGolemId;
	FGuid PendingRequestId;
	FGuid LastCompletedRequestId;
	FTimerHandle SourceReadinessTimer;
	FText Feedback;
	int32 BindingAttempts = 0;
	bool bEndingUIPlay = false;
};
