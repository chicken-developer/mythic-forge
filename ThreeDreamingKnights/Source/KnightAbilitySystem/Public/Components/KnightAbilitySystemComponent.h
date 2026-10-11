#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "Attributes/KnightAttributeSet.h"
#include "Definitions/KnightAbilitySetDefinition.h"
#include "Events/KnightConfirmedEvent.h"
#include "Projection/KnightSourceProjection.h"
#include "Types/KnightStableIds.h"
#include "KnightAbilitySystemComponent.generated.h"

class UKnightLifeComponent;
class AKnightCueVisualActor;
class UKnightDefinition;
class AController;
class UKnightSkillRuntimeComponent;
struct FHitResult;

USTRUCT()
struct KNIGHTABILITYSYSTEM_API FKnightOwnedAbilityGrant
{
	GENERATED_BODY()

	UPROPERTY()
	FKnightAbilitySetHandle Handle;
	UPROPERTY()
	TObjectPtr<UKnightAbilitySetDefinition> Definition;
	UPROPERTY()
	TArray<FGameplayAbilitySpecHandle> AbilityHandles;
	UPROPERTY()
	TArray<FActiveGameplayEffectHandle> EffectHandles;
};

USTRUCT()
struct FKnightProjectionRuntime
{
	GENERATED_BODY()

	UPROPERTY()
	FKnightSourceProjection Snapshot;
	UPROPERTY()
	TObjectPtr<UKnightDefinition> OwnerDefinition;
	UPROPERTY()
	TArray<FKnightOwnedAbilityGrant> OptionalGrants;
	UPROPERTY()
	FActiveGameplayEffectHandle StatEffect;
};

USTRUCT()
struct KNIGHTABILITYSYSTEM_API FKnightDamageConfirmedEvent : public FKnightConfirmedEvent
{
	GENERATED_BODY()
	UPROPERTY()
	FKnightEntityId TargetId;
	UPROPERTY()
	float AppliedDamage = 0.0f;
	UPROPERTY()
	float RemainingHealth = 0.0f;
};

struct KNIGHTABILITYSYSTEM_API FKnightCueDiagnostics
{
	int32 ExecutedCount = 0;
	int32 ActiveCount = 0;
	int32 RemovedCount = 0;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FOnKnightAbilityResult, const FKnightResult&);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnKnightGameplayCueProcessed, FGameplayTag, EGameplayCueEvent::Type);
DECLARE_MULTICAST_DELEGATE(FOnKnightAbilityGrantsChanged);

UCLASS(ClassGroup = Knight, meta = (BlueprintSpawnableComponent))
class KNIGHTABILITYSYSTEM_API UKnightAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:
	UKnightAbilitySystemComponent();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	FKnightResult InitializeKnightActor(AActor* Knight, UKnightAttributeSet* InAttributes,
		UKnightLifeComponent* InLife, const FKnightEntityId& Identity,
		const FKnightAttributeDefaults& Defaults = FKnightAttributeDefaults(), bool bPartyKnight = true);
	UKnightAttributeSet* GetKnightAttributes() const;
	UKnightLifeComponent* GetLifeComponent() const;
	FKnightEntityId GetEntityId() const;
	FKnightResult GrantAbilitySet(const UKnightAbilitySetDefinition* Definition, FName SourceKey,
		FKnightAbilitySetHandle& OutHandle);
	FKnightResult RevokeAbilitySet(const FKnightAbilitySetHandle& Handle);
	int32 GetOwnedGrantCount() const;
	void HandleControllerChanged();
	void RefreshReplicatedMontagePresentation();
	virtual void ClearActorInfo() override;
	FKnightResult RequestAbilityInput(const FGameplayTag& InputTag, bool bPressed, AActor* Target = nullptr);
	FKnightResult ActivateSpecializationHandle(FGameplayAbilitySpecHandle Handle, AActor* Target);
	AActor* FindAbilityTarget(const FGameplayTag& InputTag) const;
	static FKnightResult ApplyDamage(AActor* Source, AActor* Target, float Amount);
	static FKnightResult ApplyCombatDamage(AActor* Source, AActor* Target, float Amount, float PoiseDamage,
		bool bCanDefend = true, const FHitResult* Hit = nullptr, bool bHeavy = false,
		FGuid ActivationId = FGuid(), int32 HitIndex = 0);
	FKnightResult ResetAttributesForCheckpoint();
	FKnightResult BeginMutationFence(FGuid RequestId, FKnightMutationFence& OutFence);
	FKnightResult ValidateItemMutationFence(const FKnightMutationFence& Fence) const;
	bool IsMutationFenced() const;
	FKnightResult PrepareSourceProjection(const FKnightMutationFence& Fence, const FKnightGASProjectionPlan& Plan,
		int64 ExpectedRevision, FKnightProjectionCapability& OutCapability);
	FKnightResult StageSourceProjection(const FKnightMutationFence& Fence,
		const FKnightProjectionCapability& Capability);
	FKnightResult ValidateStagedSourceProjection(const FKnightMutationFence& Fence,
		const FKnightProjectionCapability& Capability) const;
	FKnightResult CommitSourceProjection(const FKnightMutationFence& Fence,
		const FKnightProjectionCapability& Capability);
	FKnightResult AbortSourceProjection(const FKnightMutationFence& Fence,
		const FKnightProjectionCapability& Capability);
	/** Always finalizes committed cleanup; an optional live publication owner gates subscriber notifications. */
	FKnightResult PublishSourceProjection(const FKnightMutationFence& Fence,
		const FKnightProjectionCapability& Capability, bool bNotifySubscribers = true,
		AActor* InPublicationOwner = nullptr);
	FKnightResult EndMutationFence(const FKnightMutationFence& Fence);
	FKnightSourceProjection GetSourceProjection(FName SourceKey) const;
	FKnightActionModifier GetActionModifier(FGameplayTag AbilityTag) const;
	float GetInitialAttackPower() const;
	FOnKnightSourceProjectionChanged OnSourceProjectionChanged;
	void GetAdditionalActivationTagRequirements(const FGameplayTagContainer& AbilityTags,
		FGameplayTagContainer& OutRequiredTags, FGameplayTagContainer& OutBlockedTags) const;
	virtual void ApplyAbilityBlockAndCancelTags(const FGameplayTagContainer& AbilityTags,
		UGameplayAbility* RequestingAbility, bool bEnableBlockTags, const FGameplayTagContainer& BlockTags,
		bool bExecuteCancelTags, const FGameplayTagContainer& CancelTags) override;
	void ReportAbilityResult(const FKnightResult& Result);
	void RecordGameplayCue(FGameplayTag Tag, EGameplayCueEvent::Type EventType);
	FKnightCueDiagnostics GetCueDiagnostics(FGameplayTag Tag) const;
	void PresentGameplayCue(FGameplayTag Tag, EGameplayCueEvent::Type EventType);
	void PublishDamageConfirmation(AActor* Source, float AppliedDamage);
	FOnKnightAbilityResult OnAbilityResult;
	FOnKnightGameplayCueProcessed OnGameplayCueProcessed;
	FOnKnightAbilityGrantsChanged OnAbilityGrantsChanged;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnRep_ReplicatedAnimMontage() override;

private:
	friend class UKnightSkillRuntimeComponent;
	FKnightResult PrepareConsumableEffect(const FKnightMutationFence& Fence, FGameplayEffectSpec& Spec);
	void ExecutePreparedConsumableEffect(const FKnightMutationFence& Fence, FGameplayEffectSpec& Spec);
	FKnightResult ValidateProjectionOwner() const;
	FKnightResult ValidateMutationFence(const FKnightMutationFence& Fence) const;
	FKnightResult ValidateProjectionCapability(const FKnightMutationFence& Fence,
		const FKnightProjectionCapability& Capability) const;
	FKnightResult ValidateProjectionAbilities(const FKnightGASProjectionPlan& Plan,
		const TArray<FKnightOwnedAbilityGrant>& OptionalGrants) const;
	void RemoveProjectionRuntime(FKnightProjectionRuntime Runtime);
	void CancelPendingProjection();
	UFUNCTION()
	void OnRep_SourceProjections();
	FKnightResult ApplyAttributeDefaults();
	static void RegisterNativeGameplayCues();
	UFUNCTION()
	void OnRep_OwnedGrantCount();
	UPROPERTY(ReplicatedUsing = OnRep_OwnedGrantCount)
	int32 OwnedGrantCount = 0;

	UPROPERTY(Transient)
	TObjectPtr<UKnightAttributeSet> Attributes;
	UPROPERTY(Transient)
	TWeakObjectPtr<UKnightLifeComponent> LifeComponent;
	UPROPERTY(Transient)
	FKnightEntityId EntityId;
	UPROPERTY(Transient)
	FKnightAttributeDefaults InitialDefaults;
	UPROPERTY(Transient)
	TMap<FName, FKnightOwnedAbilityGrant> OwnedGrants;
	UPROPERTY(Transient)
	TMap<FName, FKnightProjectionRuntime> ProjectionSources;
	UPROPERTY(Transient)
	FKnightProjectionRuntime PendingProjection;
	UPROPERTY(Transient)
	FKnightProjectionRuntime RetiredProjection;
	UPROPERTY(ReplicatedUsing = OnRep_SourceProjections)
	TArray<FKnightSourceProjection> PublishedProjections;
	UPROPERTY(Transient)
	TArray<TObjectPtr<AKnightCueVisualActor>> CueVisuals;

	TMap<FGameplayTag, FKnightCueDiagnostics> CueDiagnostics;
	TMap<FName, FKnightAbilitySetHandle> LastRevokedGrants;
	FKnightResult LastAbilityResult;
	bool bAttributesInitialized = false;
	int32 DamageRevision = 0;
	FKnightMutationFence MutationFence;
	FKnightMutationFence LastEndedFence;
	FKnightProjectionCapability ProjectionCapability;
	FKnightProjectionCapability LastAbortedProjection;
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
