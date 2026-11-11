#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "Attributes/GolemAttributeSet.h"
#include "Definitions/GolemAbilitySetDefinition.h"
#include "Events/GolemConfirmedEvent.h"
#include "Projection/GolemSourceProjection.h"
#include "Types/GolemStableIds.h"
#include "GolemAbilitySystemComponent.generated.h"

class UGolemLifeComponent;
class AGolemCueVisualActor;
class UGolemDefinition;
class AController;
class UGolemSkillRuntimeComponent;
struct FHitResult;

USTRUCT()
struct GOLEMABILITYSYSTEM_API FGolemOwnedAbilityGrant
{
	GENERATED_BODY()

	UPROPERTY()
	FGolemAbilitySetHandle Handle;
	UPROPERTY()
	TObjectPtr<UGolemAbilitySetDefinition> Definition;
	UPROPERTY()
	TArray<FGameplayAbilitySpecHandle> AbilityHandles;
	UPROPERTY()
	TArray<FActiveGameplayEffectHandle> EffectHandles;
};

USTRUCT()
struct FGolemProjectionRuntime
{
	GENERATED_BODY()

	UPROPERTY()
	FGolemSourceProjection Snapshot;
	UPROPERTY()
	TObjectPtr<UGolemDefinition> OwnerDefinition;
	UPROPERTY()
	TArray<FGolemOwnedAbilityGrant> OptionalGrants;
	UPROPERTY()
	FActiveGameplayEffectHandle StatEffect;
};

USTRUCT()
struct GOLEMABILITYSYSTEM_API FGolemDamageConfirmedEvent : public FGolemConfirmedEvent
{
	GENERATED_BODY()
	UPROPERTY()
	FGolemEntityId TargetId;
	UPROPERTY()
	float AppliedDamage = 0.0f;
	UPROPERTY()
	float RemainingHealth = 0.0f;
};

struct GOLEMABILITYSYSTEM_API FGolemCueDiagnostics
{
	int32 ExecutedCount = 0;
	int32 ActiveCount = 0;
	int32 RemovedCount = 0;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FOnGolemAbilityResult, const FGolemResult&);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnGolemGameplayCueProcessed, FGameplayTag, EGameplayCueEvent::Type);
DECLARE_MULTICAST_DELEGATE(FOnGolemAbilityGrantsChanged);

UCLASS(ClassGroup = Golem, meta = (BlueprintSpawnableComponent))
class GOLEMABILITYSYSTEM_API UGolemAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:
	UGolemAbilitySystemComponent();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	FGolemResult InitializeGolemActor(AActor* Golem, UGolemAttributeSet* InAttributes,
		UGolemLifeComponent* InLife, const FGolemEntityId& Identity,
		const FGolemAttributeDefaults& Defaults = FGolemAttributeDefaults(), bool bPartyGolem = true);
	UGolemAttributeSet* GetGolemAttributes() const;
	UGolemLifeComponent* GetLifeComponent() const;
	FGolemEntityId GetEntityId() const;
	FGolemResult GrantAbilitySet(const UGolemAbilitySetDefinition* Definition, FName SourceKey,
		FGolemAbilitySetHandle& OutHandle);
	FGolemResult RevokeAbilitySet(const FGolemAbilitySetHandle& Handle);
	int32 GetOwnedGrantCount() const;
	void HandleControllerChanged();
	void RefreshReplicatedMontagePresentation();
	virtual void ClearActorInfo() override;
	FGolemResult RequestAbilityInput(const FGameplayTag& InputTag, bool bPressed, AActor* Target = nullptr);
	FGolemResult ActivateSpecializationHandle(FGameplayAbilitySpecHandle Handle, AActor* Target);
	AActor* FindAbilityTarget(const FGameplayTag& InputTag) const;
	static FGolemResult ApplyDamage(AActor* Source, AActor* Target, float Amount);
	static FGolemResult ApplyCombatDamage(AActor* Source, AActor* Target, float Amount, float PoiseDamage,
		bool bCanDefend = true, const FHitResult* Hit = nullptr, bool bHeavy = false,
		FGuid ActivationId = FGuid(), int32 HitIndex = 0);
	FGolemResult ResetAttributesForCheckpoint();
	FGolemResult BeginMutationFence(FGuid RequestId, FGolemMutationFence& OutFence);
	FGolemResult ValidateItemMutationFence(const FGolemMutationFence& Fence) const;
	bool IsMutationFenced() const;
	FGolemResult PrepareSourceProjection(const FGolemMutationFence& Fence, const FGolemGASProjectionPlan& Plan,
		int64 ExpectedRevision, FGolemProjectionCapability& OutCapability);
	FGolemResult StageSourceProjection(const FGolemMutationFence& Fence,
		const FGolemProjectionCapability& Capability);
	FGolemResult ValidateStagedSourceProjection(const FGolemMutationFence& Fence,
		const FGolemProjectionCapability& Capability) const;
	FGolemResult CommitSourceProjection(const FGolemMutationFence& Fence,
		const FGolemProjectionCapability& Capability);
	FGolemResult AbortSourceProjection(const FGolemMutationFence& Fence,
		const FGolemProjectionCapability& Capability);
	/** Always finalizes committed cleanup; an optional live publication owner gates subscriber notifications. */
	FGolemResult PublishSourceProjection(const FGolemMutationFence& Fence,
		const FGolemProjectionCapability& Capability, bool bNotifySubscribers = true,
		AActor* InPublicationOwner = nullptr);
	FGolemResult EndMutationFence(const FGolemMutationFence& Fence);
	FGolemSourceProjection GetSourceProjection(FName SourceKey) const;
	FGolemActionModifier GetActionModifier(FGameplayTag AbilityTag) const;
	float GetInitialAttackPower() const;
	FOnGolemSourceProjectionChanged OnSourceProjectionChanged;
	void GetAdditionalActivationTagRequirements(const FGameplayTagContainer& AbilityTags,
		FGameplayTagContainer& OutRequiredTags, FGameplayTagContainer& OutBlockedTags) const;
	virtual void ApplyAbilityBlockAndCancelTags(const FGameplayTagContainer& AbilityTags,
		UGameplayAbility* RequestingAbility, bool bEnableBlockTags, const FGameplayTagContainer& BlockTags,
		bool bExecuteCancelTags, const FGameplayTagContainer& CancelTags) override;
	void ReportAbilityResult(const FGolemResult& Result);
	void RecordGameplayCue(FGameplayTag Tag, EGameplayCueEvent::Type EventType);
	FGolemCueDiagnostics GetCueDiagnostics(FGameplayTag Tag) const;
	void PresentGameplayCue(FGameplayTag Tag, EGameplayCueEvent::Type EventType);
	void PublishDamageConfirmation(AActor* Source, float AppliedDamage);
	FOnGolemAbilityResult OnAbilityResult;
	FOnGolemGameplayCueProcessed OnGameplayCueProcessed;
	FOnGolemAbilityGrantsChanged OnAbilityGrantsChanged;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnRep_ReplicatedAnimMontage() override;

private:
	friend class UGolemSkillRuntimeComponent;
	FGolemResult PrepareConsumableEffect(const FGolemMutationFence& Fence, FGameplayEffectSpec& Spec);
	void ExecutePreparedConsumableEffect(const FGolemMutationFence& Fence, FGameplayEffectSpec& Spec);
	FGolemResult ValidateProjectionOwner() const;
	FGolemResult ValidateMutationFence(const FGolemMutationFence& Fence) const;
	FGolemResult ValidateProjectionCapability(const FGolemMutationFence& Fence,
		const FGolemProjectionCapability& Capability) const;
	FGolemResult ValidateProjectionAbilities(const FGolemGASProjectionPlan& Plan,
		const TArray<FGolemOwnedAbilityGrant>& OptionalGrants) const;
	void RemoveProjectionRuntime(FGolemProjectionRuntime Runtime);
	void CancelPendingProjection();
	UFUNCTION()
	void OnRep_SourceProjections();
	FGolemResult ApplyAttributeDefaults();
	static void RegisterNativeGameplayCues();
	UFUNCTION()
	void OnRep_OwnedGrantCount();
	UPROPERTY(ReplicatedUsing = OnRep_OwnedGrantCount)
	int32 OwnedGrantCount = 0;

	UPROPERTY(Transient)
	TObjectPtr<UGolemAttributeSet> Attributes;
	UPROPERTY(Transient)
	TWeakObjectPtr<UGolemLifeComponent> LifeComponent;
	UPROPERTY(Transient)
	FGolemEntityId EntityId;
	UPROPERTY(Transient)
	FGolemAttributeDefaults InitialDefaults;
	UPROPERTY(Transient)
	TMap<FName, FGolemOwnedAbilityGrant> OwnedGrants;
	UPROPERTY(Transient)
	TMap<FName, FGolemProjectionRuntime> ProjectionSources;
	UPROPERTY(Transient)
	FGolemProjectionRuntime PendingProjection;
	UPROPERTY(Transient)
	FGolemProjectionRuntime RetiredProjection;
	UPROPERTY(ReplicatedUsing = OnRep_SourceProjections)
	TArray<FGolemSourceProjection> PublishedProjections;
	UPROPERTY(Transient)
	TArray<TObjectPtr<AGolemCueVisualActor>> CueVisuals;

	TMap<FGameplayTag, FGolemCueDiagnostics> CueDiagnostics;
	TMap<FName, FGolemAbilitySetHandle> LastRevokedGrants;
	FGolemResult LastAbilityResult;
	bool bAttributesInitialized = false;
	int32 DamageRevision = 0;
	FGolemMutationFence MutationFence;
	FGolemMutationFence LastEndedFence;
	FGolemProjectionCapability ProjectionCapability;
	FGolemProjectionCapability LastAbortedProjection;
	TWeakObjectPtr<AActor> FenceOwner;
	TWeakObjectPtr<AActor> FenceAvatar;
	TWeakObjectPtr<AController> FenceController;
	uint64 ProjectionGeneration = 0;
	uint64 FenceGeneration = 0;
	uint8 ProjectionStage = 0;
	bool bProjectionOperationInProgress = false;
	bool bProjectionEndingPlay = false;
	bool bProjectionGrantsChanged = false;
};
