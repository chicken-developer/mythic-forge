#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "Attributes/KnightAttributeSet.h"
#include "Definitions/KnightAbilitySetDefinition.h"
#include "Events/KnightConfirmedEvent.h"
#include "Types/KnightStableIds.h"
#include "KnightAbilitySystemComponent.generated.h"

class UKnightLifeComponent;
class AKnightCueVisualActor;
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
	AActor* FindAbilityTarget(const FGameplayTag& InputTag) const;
	static FKnightResult ApplyDamage(AActor* Source, AActor* Target, float Amount);
	static FKnightResult ApplyCombatDamage(AActor* Source, AActor* Target, float Amount, float PoiseDamage,
		bool bCanDefend = true, const FHitResult* Hit = nullptr);
	FKnightResult ResetAttributesForCheckpoint();
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
	TArray<TObjectPtr<AKnightCueVisualActor>> CueVisuals;

	TMap<FGameplayTag, FKnightCueDiagnostics> CueDiagnostics;
	TMap<FName, FKnightAbilitySetHandle> LastRevokedGrants;
	FKnightResult LastAbilityResult;
	bool bAttributesInitialized = false;
	int32 DamageRevision = 0;
};
