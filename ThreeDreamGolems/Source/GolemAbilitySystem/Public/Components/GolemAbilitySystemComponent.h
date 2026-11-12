#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "Attributes/GolemAttributeSet.h"
#include "Definitions/GolemAbilitySetDefinition.h"
#include "Events/GolemConfirmedEvent.h"
#include "Types/GolemStableIds.h"
#include "GolemAbilitySystemComponent.generated.h"

class UGolemLifeComponent;
class AGolemCueVisualActor;
class UGolemDefinition;
class AController;
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
	AActor* FindAbilityTarget(const FGameplayTag& InputTag) const;
	static FGolemResult ApplyDamage(AActor* Source, AActor* Target, float Amount);
	static FGolemResult ApplyCombatDamage(AActor* Source, AActor* Target, float Amount, float PoiseDamage,
		bool bCanDefend = true, const FHitResult* Hit = nullptr);
	FGolemResult ResetAttributesForCheckpoint();
	float GetInitialPower() const;

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
	TArray<TObjectPtr<AGolemCueVisualActor>> CueVisuals;

	TMap<FGameplayTag, FGolemCueDiagnostics> CueDiagnostics;
	TMap<FName, FGolemAbilitySetHandle> LastRevokedGrants;
	FGolemResult LastAbilityResult;
	bool bAttributesInitialized = false;
	int32 DamageRevision = 0;
};
