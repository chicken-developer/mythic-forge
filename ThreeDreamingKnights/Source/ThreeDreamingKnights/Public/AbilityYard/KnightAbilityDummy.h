#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Actor.h"
#include "Types/KnightResult.h"
#include "Types/KnightStableIds.h"
#include "KnightAbilityDummy.generated.h"

class UKnightAbilitySystemComponent;
class UKnightAttributeSet;
class UKnightLifeComponent;
class UStaticMeshComponent;
class UTextRenderComponent;

/** A damage target has its own GAS aggregate and identity without adding a fourth party Knight. */
UCLASS()
class THREEDREAMINGKNIGHTS_API AKnightAbilityDummy : public AActor, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AKnightAbilityDummy();
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	UKnightAbilitySystemComponent* GetKnightAbilitySystem() const;
	UKnightAttributeSet* GetKnightAttributeSet() const;
	UKnightLifeComponent* GetLifeComponent() const;
	FKnightEntityId GetDummyId() const;
	const FKnightResult& GetInitializationResult() const;

protected:
	virtual void BeginPlay() override;

private:
	void InitializeAbilitySystem();

	UFUNCTION()
	void OnRep_DummyId();

	UPROPERTY(VisibleAnywhere, Category = "Knight|AbilityYard")
	TObjectPtr<UStaticMeshComponent> Body;

	UPROPERTY(VisibleAnywhere, Category = "Knight|AbilityYard")
	TObjectPtr<UTextRenderComponent> Label;

	UPROPERTY(VisibleAnywhere, Category = "Knight|AbilityYard")
	TObjectPtr<UKnightAbilitySystemComponent> AbilitySystem;

	UPROPERTY(VisibleAnywhere, Category = "Knight|AbilityYard")
	TObjectPtr<UKnightAttributeSet> Attributes;

	UPROPERTY(VisibleAnywhere, Category = "Knight|AbilityYard")
	TObjectPtr<UKnightLifeComponent> Life;

	UPROPERTY(ReplicatedUsing = OnRep_DummyId)
	FKnightEntityId DummyId;

	UPROPERTY(Transient)
	FKnightResult InitializationResult;
};
