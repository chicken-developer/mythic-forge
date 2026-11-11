#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Character.h"
#include "Types/GolemResult.h"
#include "Types/GolemStableIds.h"
#include "GolemCombatFixture.generated.h"

class UGolemAbilitySystemComponent;
class UGolemAttributeSet;
class UGolemCombatComponent;
class UGolemFinisherComponent;
class UGolemLifeComponent;
class UTextRenderComponent;

UENUM(BlueprintType)
enum class EGolemCombatFixtureKind : uint8
{
	MeleeTarget,
	ScriptedAttacker,
	RuneTarget
};

/** Scripted training actors use the same GAS attacks and weapon notifies as party Golems. */
UCLASS()
class THREEDREAMGOLEMS_API AGolemCombatFixture : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AGolemCombatFixture();
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	UGolemAbilitySystemComponent* GetGolemAbilitySystem() const;
	UGolemAttributeSet* GetGolemAttributeSet() const;
	UGolemLifeComponent* GetLifeComponent() const;
	UGolemCombatComponent* GetCombatComponent() const;
	FGolemEntityId GetFixtureId() const;
	EGolemCombatFixtureKind GetFixtureKind() const;
	bool IsTelegraphing() const;
	const FGolemResult& GetInitializationResult() const;
	void SetFixtureKind(EGolemCombatFixtureKind InKind);
	FGolemResult TryTelegraphAttack(AActor* Target);
	FGolemResult ResetForCheckpoint();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;
	virtual void OnConstruction(const FTransform& Transform) override;

private:
	void InitializeFixture();
	void ReleaseScriptedAttack();
	void ClearTelegraph();
	void RefreshLabel();
	UFUNCTION()
	void OnRep_FixtureId();
	UFUNCTION()
	void OnRep_Telegraphing();
	UPROPERTY(VisibleAnywhere, Category = "Golem|CombatChallenge")
	TObjectPtr<UGolemAbilitySystemComponent> AbilitySystem;
	UPROPERTY(VisibleAnywhere, Category = "Golem|CombatChallenge")
	TObjectPtr<UGolemAttributeSet> Attributes;
	UPROPERTY(VisibleAnywhere, Category = "Golem|CombatChallenge")
	TObjectPtr<UGolemLifeComponent> Life;
	UPROPERTY(VisibleAnywhere, Category = "Golem|CombatChallenge")
	TObjectPtr<UGolemCombatComponent> Combat;
	UPROPERTY(VisibleAnywhere, Category = "Golem|CombatChallenge")
	TObjectPtr<UGolemFinisherComponent> Finisher;
	UPROPERTY(VisibleAnywhere, Category = "Golem|CombatChallenge")
	TObjectPtr<UTextRenderComponent> Label;
	UPROPERTY(EditInstanceOnly, Category = "Golem|CombatChallenge")
	EGolemCombatFixtureKind FixtureKind = EGolemCombatFixtureKind::MeleeTarget;
	UPROPERTY(ReplicatedUsing = OnRep_FixtureId)
	FGolemEntityId FixtureId;
	UPROPERTY(ReplicatedUsing = OnRep_Telegraphing)
	bool bTelegraphing = false;
	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> AttackTarget;
	UPROPERTY(Transient)
	FGolemResult InitializationResult;
	FTransform InitialTransform;
	FTimerHandle TelegraphTimer;
};
