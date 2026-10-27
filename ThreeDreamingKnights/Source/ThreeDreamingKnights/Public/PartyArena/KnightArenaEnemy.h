#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Character.h"
#include "GameplayEffectTypes.h"
#include "Types/KnightResult.h"
#include "Types/KnightStableIds.h"
#include "KnightArenaEnemy.generated.h"

class UKnightAbilitySystemComponent;
class UKnightAttributeSet;
class UKnightLifeComponent;
class UKnightCombatComponent;
class UKnightFinisherComponent;
class UKnightThreatComponent;
class UKnightBossPhaseComponent;
class UKnightAIArchetypeDefinition;
class UTextRenderComponent;
class UKnightBossArmorComponent;
class UKnightSkillRuntimeComponent;
enum class EKnightLifeState : uint8;

/** Noncanonical combat pawn. Its persistent identity and ASC survive boss phase changes. */
UCLASS()
class THREEDREAMINGKNIGHTS_API AKnightArenaEnemy : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AKnightArenaEnemy();
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	FKnightResult InitializeArenaEnemy(const FPrimaryAssetId& InArchetypeId,
		const FPrimaryAssetId& InBossDefinitionId = FPrimaryAssetId());
	UKnightAbilitySystemComponent* GetKnightAbilitySystem() const;
	UKnightAttributeSet* GetKnightAttributeSet() const;
	UKnightLifeComponent* GetLifeComponent() const;
	UKnightCombatComponent* GetCombatComponent() const;
	UKnightThreatComponent* GetThreatComponent() const;
	UKnightBossPhaseComponent* GetBossPhaseComponent() const;
	FKnightEntityId GetArenaEnemyId() const;
	FPrimaryAssetId GetArchetypeId() const;
	const FKnightResult& GetInitializationResult() const;
	FActiveGameplayEffectHandle GetRecoveryEffectHandle() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UKnightSkillRuntimeComponent> SpecializationRuntime;
	UPROPERTY(VisibleAnywhere, Category = "Knight|SkillLab")
	TObjectPtr<UKnightBossArmorComponent> SkillLabArmor;
	void HandleLifeChanged(EKnightLifeState Previous, EKnightLifeState Current);
	UFUNCTION()
	void OnRep_EnemyIdentity();
	UPROPERTY(VisibleAnywhere, Category = "Knight|Arena")
	TObjectPtr<UKnightAbilitySystemComponent> AbilitySystem;
	UPROPERTY(VisibleAnywhere, Category = "Knight|Arena")
	TObjectPtr<UKnightAttributeSet> Attributes;
	UPROPERTY(VisibleAnywhere, Category = "Knight|Arena")
	TObjectPtr<UKnightLifeComponent> Life;
	UPROPERTY(VisibleAnywhere, Category = "Knight|Arena")
	TObjectPtr<UKnightCombatComponent> Combat;
	UPROPERTY(VisibleAnywhere, Category = "Knight|Arena")
	TObjectPtr<UKnightFinisherComponent> Finisher;
	UPROPERTY(VisibleAnywhere, Category = "Knight|Arena")
	TObjectPtr<UKnightThreatComponent> Threat;
	UPROPERTY(VisibleAnywhere, Category = "Knight|Arena")
	TObjectPtr<UKnightBossPhaseComponent> BossPhase;
	UPROPERTY(VisibleAnywhere, Category = "Knight|Arena")
	TObjectPtr<UTextRenderComponent> Label;
	UPROPERTY(ReplicatedUsing = OnRep_EnemyIdentity)
	FKnightEntityId EnemyId;
	UPROPERTY(ReplicatedUsing = OnRep_EnemyIdentity)
	FPrimaryAssetId ArchetypeId;
	UPROPERTY(ReplicatedUsing = OnRep_EnemyIdentity)
	FPrimaryAssetId BossDefinitionId;
	UPROPERTY(Transient)
	TObjectPtr<UKnightAIArchetypeDefinition> Archetype;
	FKnightResult InitializationResult;
	FActiveGameplayEffectHandle RecoveryEffect;
	FDelegateHandle LifeHandle;
};
