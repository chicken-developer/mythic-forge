#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Types/KnightResult.h"
#include "KnightBossPhaseComponent.generated.h"

class UKnightAbilitySystemComponent;
class UKnightAIContextComponent;
class UKnightBossPhaseDefinition;
struct FOnAttributeChangeData;

USTRUCT(BlueprintType)
struct KNIGHTAI_API FKnightBossPhaseSnapshot
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "Knight|AI|Boss")
	FPrimaryAssetId DefinitionId;
	UPROPERTY(BlueprintReadOnly, Category = "Knight|AI|Boss")
	int32 PhaseIndex = INDEX_NONE;
	UPROPERTY(BlueprintReadOnly, Category = "Knight|AI|Boss")
	FName PhaseKey;
	UPROPERTY(BlueprintReadOnly, Category = "Knight|AI|Boss")
	FPrimaryAssetId ActionSetId;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FOnKnightBossPhaseChanged, const FKnightBossPhaseSnapshot&);

UCLASS(ClassGroup = Knight, meta = (BlueprintSpawnableComponent))
class KNIGHTAI_API UKnightBossPhaseComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UKnightBossPhaseComponent();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	FKnightResult Configure(const FPrimaryAssetId& InDefinitionId);
	FKnightResult BindContext(UKnightAIContextComponent* InContext);
	FKnightResult InitializePhases(const FPrimaryAssetId& InDefinitionId, UKnightAIContextComponent* InContext);
	FKnightResult ResetForCheckpoint();
	FKnightResult RefreshPhase();
	int32 GetPhaseIndex() const;
	FPrimaryAssetId GetDefinitionId() const;
	FKnightBossPhaseSnapshot GetPhaseSnapshot() const;
	const FKnightResult& GetLastPhaseResult() const;
	FOnKnightBossPhaseChanged OnPhaseChanged;

protected:
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;

private:
	void HandleHealthChanged(const FOnAttributeChangeData& Change);
	FKnightResult TryApplyPhase(int32 InPhaseIndex);
	UFUNCTION()
	void OnRep_PhaseIndex();
	UPROPERTY(Replicated)
	FPrimaryAssetId DefinitionId;
	UPROPERTY(ReplicatedUsing = OnRep_PhaseIndex)
	int32 PhaseIndex = INDEX_NONE;
	UPROPERTY(Replicated)
	FName PhaseKey;
	UPROPERTY(Replicated)
	FPrimaryAssetId PhaseActionSetId;
	UPROPERTY(Transient)
	TObjectPtr<UKnightBossPhaseDefinition> Definition;
	TWeakObjectPtr<UKnightAIContextComponent> Context;
	TWeakObjectPtr<UKnightAbilitySystemComponent> AbilitySystem;
	FDelegateHandle HealthHandle;
	FDelegateHandle MaxHealthHandle;
	FKnightResult LastPhaseResult;
	bool bApplyingPhase = false;
	bool bEndingPlay = false;
};
