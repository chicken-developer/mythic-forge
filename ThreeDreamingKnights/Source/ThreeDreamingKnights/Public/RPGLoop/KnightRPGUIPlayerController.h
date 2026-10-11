#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectTypes.h"
#include "RPGLoop/KnightRPGPlayerController.h"
#include "KnightRPGUIPlayerController.generated.h"

class AKnightCharacter;
class AKnightRPGGameState;
class AKnightRPGStateActor;
class AKnightGearLabState;
class UEnhancedInputComponent;
class UEnhancedInputLocalPlayerSubsystem;
class UInputAction;
class UInputMappingContext;
class UKnightAbilitySystemComponent;
class UKnightCombatComponent;
class UKnightRPGPanel;
class UKnightSkillRuntimeComponent;
enum class EKnightRPGPanelAction : uint8;
enum class EKnightLifeState : uint8;
struct FKnightSourceProjection;
struct FKnightRPGPanelSnapshot;
struct FKnightRPGPanelCommand;
struct FKnightSkillBuild;
struct FKnightGearPanelIntent;
struct FKnightGearQuickUseRow;

DECLARE_MULTICAST_DELEGATE(FOnKnightRPGPresentationChanged);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnKnightGearFixtureResult, const FGuid&, const FKnightResult&);

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
	virtual void RequestGearFixture(FName Action, FGuid RequestId);
	TArray<FKnightGearQuickUseRow> GetQuickUseReadModel() const;
	FOnKnightRPGPresentationChanged OnRPGPresentationChanged;
	FOnKnightGearFixtureResult OnGearFixtureResult;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;
	virtual void SetupInputComponent() override;

private:
	friend class FKnightRPGUIClosedReceiptTest;
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
	void HandlePanelCommand(const FKnightRPGPanelCommand& Intent);
	void HandleBuildPreview(const FKnightSkillBuild& Build);
	void HandleGearCommand(const FKnightGearPanelIntent& Intent);
	void HandleGearPreview(const FKnightGearPanelIntent& Intent);
	void HandleGearFixtureResult(const FGuid& RequestId, const FKnightResult& Result);
	void BindGearSources();
	void UnbindGearViewedKnight();
	AKnightCharacter* FindGearKnight() const;
	void HandleRPGResult(const FKnightRPGCommandResult& Outcome);
	void HandleLoopResult(const FKnightResult& Result);
	void RefreshRPGPresentation();
	void ReconcileClosedPanelReceipt(const FGuid& RequestId);
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
	TWeakObjectPtr<AKnightGearLabState> GearLabState;
	TWeakObjectPtr<AKnightCharacter> GearViewedKnight;
	TWeakObjectPtr<UKnightAbilitySystemComponent> GearViewedASC;
	TWeakObjectPtr<UKnightSkillRuntimeComponent> GearViewedRuntime;
	TWeakObjectPtr<UKnightCombatComponent> GearViewedCombat;
	TArray<TPair<FGameplayAttribute, FDelegateHandle>> GearAttributeHandles;
	FDelegateHandle GearLifeHandle;
	FDelegateHandle GearRuntimeHandle;
	FDelegateHandle GearCombatHandle;
	FDelegateHandle GearProjectionHandle;
	FTimerHandle GearReadinessTimer;
	int32 GearBindingAttempts = 0;
	TWeakObjectPtr<AKnightCharacter> DisplayedKnight;
	TWeakObjectPtr<UKnightAbilitySystemComponent> DisplayedASC;
	TWeakObjectPtr<UKnightSkillRuntimeComponent> DisplayedRuntime;
	TWeakObjectPtr<UKnightCombatComponent> DisplayedCombat;
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
	FKnightEntityId GearSelectedKnightId;
	FGuid PendingRequestId;
	FGuid LastCompletedRequestId;
	FTimerHandle SourceReadinessTimer;
	FText Feedback;
	int32 BindingAttempts = 0;
	bool bEndingUIPlay = false;
};
