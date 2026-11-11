#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Character.h"
#include "GameplayEffectTypes.h"
#include "Types/GolemResult.h"
#include "Types/GolemStableIds.h"
#include "GolemArenaEnemy.generated.h"

class UGolemAbilitySystemComponent;
class UGolemAttributeSet;
class UGolemLifeComponent;
class UGolemCombatComponent;
class UGolemFinisherComponent;
class UGolemThreatComponent;
class UGolemBossPhaseComponent;
class UGolemAIArchetypeDefinition;
class UTextRenderComponent;
class UGolemBossArmorComponent;
class UGolemSkillRuntimeComponent;
enum class EGolemLifeState : uint8;

/** Noncanonical combat pawn. Its persistent identity and ASC survive boss phase changes. */
UCLASS()
class THREEDREAMGOLEMS_API AGolemArenaEnemy : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AGolemArenaEnemy();
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	FGolemResult InitializeArenaEnemy(const FPrimaryAssetId& InArchetypeId,
		const FPrimaryAssetId& InBossDefinitionId = FPrimaryAssetId());
	UGolemAbilitySystemComponent* GetGolemAbilitySystem() const;
	UGolemAttributeSet* GetGolemAttributeSet() const;
	UGolemLifeComponent* GetLifeComponent() const;
	UGolemCombatComponent* GetCombatComponent() const;
	UGolemThreatComponent* GetThreatComponent() const;
	UGolemBossPhaseComponent* GetBossPhaseComponent() const;
	FGolemEntityId GetArenaEnemyId() const;
	FPrimaryAssetId GetArchetypeId() const;
	const FGolemResult& GetInitializationResult() const;
	FActiveGameplayEffectHandle GetRecoveryEffectHandle() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UGolemSkillRuntimeComponent> SpecializationRuntime;
	UPROPERTY(VisibleAnywhere, Category = "Golem|SkillLab")
	TObjectPtr<UGolemBossArmorComponent> SkillLabArmor;
	void HandleLifeChanged(EGolemLifeState Previous, EGolemLifeState Current);
	UFUNCTION()
	void OnRep_EnemyIdentity();
	UPROPERTY(VisibleAnywhere, Category = "Golem|Arena")
	TObjectPtr<UGolemAbilitySystemComponent> AbilitySystem;
	UPROPERTY(VisibleAnywhere, Category = "Golem|Arena")
	TObjectPtr<UGolemAttributeSet> Attributes;
	UPROPERTY(VisibleAnywhere, Category = "Golem|Arena")
	TObjectPtr<UGolemLifeComponent> Life;
	UPROPERTY(VisibleAnywhere, Category = "Golem|Arena")
	TObjectPtr<UGolemCombatComponent> Combat;
	UPROPERTY(VisibleAnywhere, Category = "Golem|Arena")
	TObjectPtr<UGolemFinisherComponent> Finisher;
	UPROPERTY(VisibleAnywhere, Category = "Golem|Arena")
	TObjectPtr<UGolemThreatComponent> Threat;
	UPROPERTY(VisibleAnywhere, Category = "Golem|Arena")
	TObjectPtr<UGolemBossPhaseComponent> BossPhase;
	UPROPERTY(VisibleAnywhere, Category = "Golem|Arena")
	TObjectPtr<UTextRenderComponent> Label;
	UPROPERTY(ReplicatedUsing = OnRep_EnemyIdentity)
	FGolemEntityId EnemyId;
	UPROPERTY(ReplicatedUsing = OnRep_EnemyIdentity)
	FPrimaryAssetId ArchetypeId;
	UPROPERTY(ReplicatedUsing = OnRep_EnemyIdentity)
	FPrimaryAssetId BossDefinitionId;
	UPROPERTY(Transient)
	TObjectPtr<UGolemAIArchetypeDefinition> Archetype;
	FGolemResult InitializationResult;
	FActiveGameplayEffectHandle RecoveryEffect;
	FDelegateHandle LifeHandle;
};
