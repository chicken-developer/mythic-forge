#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Types/GolemResult.h"
#include "GolemBossPhaseComponent.generated.h"

class UGolemAbilitySystemComponent;
class UGolemAIContextComponent;
class UGolemBossPhaseDefinition;
struct FOnAttributeChangeData;

USTRUCT(BlueprintType)
struct GOLEMAI_API FGolemBossPhaseSnapshot
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "Golem|AI|Boss")
	FPrimaryAssetId DefinitionId;
	UPROPERTY(BlueprintReadOnly, Category = "Golem|AI|Boss")
	int32 PhaseIndex = INDEX_NONE;
	UPROPERTY(BlueprintReadOnly, Category = "Golem|AI|Boss")
	FName PhaseKey;
	UPROPERTY(BlueprintReadOnly, Category = "Golem|AI|Boss")
	FPrimaryAssetId ActionSetId;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FOnGolemBossPhaseChanged, const FGolemBossPhaseSnapshot&);

UCLASS(ClassGroup = Golem, meta = (BlueprintSpawnableComponent))
class GOLEMAI_API UGolemBossPhaseComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGolemBossPhaseComponent();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	FGolemResult Configure(const FPrimaryAssetId& InDefinitionId);
	FGolemResult BindContext(UGolemAIContextComponent* InContext);
	FGolemResult InitializePhases(const FPrimaryAssetId& InDefinitionId, UGolemAIContextComponent* InContext);
	FGolemResult ResetForCheckpoint();
	FGolemResult RefreshPhase();
	int32 GetPhaseIndex() const;
	FPrimaryAssetId GetDefinitionId() const;
	FGolemBossPhaseSnapshot GetPhaseSnapshot() const;
	const FGolemResult& GetLastPhaseResult() const;
	FOnGolemBossPhaseChanged OnPhaseChanged;

protected:
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;

private:
	void HandleHealthChanged(const FOnAttributeChangeData& Change);
	FGolemResult TryApplyPhase(int32 InPhaseIndex);
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
	TObjectPtr<UGolemBossPhaseDefinition> Definition;
	TWeakObjectPtr<UGolemAIContextComponent> Context;
	TWeakObjectPtr<UGolemAbilitySystemComponent> AbilitySystem;
	FDelegateHandle HealthHandle;
	FDelegateHandle MaxHealthHandle;
	FGolemResult LastPhaseResult;
	bool bApplyingPhase = false;
	bool bEndingPlay = false;
};
