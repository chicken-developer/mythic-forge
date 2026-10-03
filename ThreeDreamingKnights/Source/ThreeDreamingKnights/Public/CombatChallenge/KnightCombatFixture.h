#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Character.h"
#include "Types/KnightResult.h"
#include "Types/KnightStableIds.h"
#include "KnightCombatFixture.generated.h"

class UKnightAbilitySystemComponent;
class UKnightAttributeSet;
class UKnightCombatComponent;
class UKnightFinisherComponent;
class UKnightLifeComponent;
class UTextRenderComponent;

UENUM(BlueprintType)
enum class EKnightCombatFixtureKind : uint8
{
	MeleeTarget,
	ScriptedAttacker,
	RuneTarget
};

/** Scripted training actors use the same GAS attacks and weapon notifies as party Knights. */
UCLASS()
class THREEDREAMINGKNIGHTS_API AKnightCombatFixture : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AKnightCombatFixture();
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	UKnightAbilitySystemComponent* GetKnightAbilitySystem() const;
	UKnightAttributeSet* GetKnightAttributeSet() const;
	UKnightLifeComponent* GetLifeComponent() const;
	UKnightCombatComponent* GetCombatComponent() const;
	FKnightEntityId GetFixtureId() const;
	EKnightCombatFixtureKind GetFixtureKind() const;
	bool IsTelegraphing() const;
	const FKnightResult& GetInitializationResult() const;
	void SetFixtureKind(EKnightCombatFixtureKind InKind);
	FKnightResult TryTelegraphAttack(AActor* Target);
	FKnightResult ResetForCheckpoint();

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
	UPROPERTY(VisibleAnywhere, Category = "Knight|CombatChallenge")
	TObjectPtr<UKnightAbilitySystemComponent> AbilitySystem;
	UPROPERTY(VisibleAnywhere, Category = "Knight|CombatChallenge")
	TObjectPtr<UKnightAttributeSet> Attributes;
	UPROPERTY(VisibleAnywhere, Category = "Knight|CombatChallenge")
	TObjectPtr<UKnightLifeComponent> Life;
	UPROPERTY(VisibleAnywhere, Category = "Knight|CombatChallenge")
	TObjectPtr<UKnightCombatComponent> Combat;
	UPROPERTY(VisibleAnywhere, Category = "Knight|CombatChallenge")
	TObjectPtr<UKnightFinisherComponent> Finisher;
	UPROPERTY(VisibleAnywhere, Category = "Knight|CombatChallenge")
	TObjectPtr<UTextRenderComponent> Label;
	UPROPERTY(EditInstanceOnly, Category = "Knight|CombatChallenge")
	EKnightCombatFixtureKind FixtureKind = EKnightCombatFixtureKind::MeleeTarget;
	UPROPERTY(ReplicatedUsing = OnRep_FixtureId)
	FKnightEntityId FixtureId;
	UPROPERTY(ReplicatedUsing = OnRep_Telegraphing)
	bool bTelegraphing = false;
	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> AttackTarget;
	UPROPERTY(Transient)
	FKnightResult InitializationResult;
	FTransform InitialTransform;
	FTimerHandle TelegraphTimer;
};
