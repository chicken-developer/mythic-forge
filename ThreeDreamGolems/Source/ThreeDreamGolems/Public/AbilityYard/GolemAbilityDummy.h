#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Actor.h"
#include "Types/GolemResult.h"
#include "Types/GolemStableIds.h"
#include "GolemAbilityDummy.generated.h"

class UGolemAbilitySystemComponent;
class UGolemAttributeSet;
class UGolemLifeComponent;
class UStaticMeshComponent;
class UTextRenderComponent;

/** A damage target has its own GAS aggregate and identity without adding a fourth party Golem. */
UCLASS()
class THREEDREAMGOLEMS_API AGolemAbilityDummy : public AActor, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AGolemAbilityDummy();
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	UGolemAbilitySystemComponent* GetGolemAbilitySystem() const;
	UGolemAttributeSet* GetGolemAttributeSet() const;
	UGolemLifeComponent* GetLifeComponent() const;
	FGolemEntityId GetDummyId() const;
	const FGolemResult& GetInitializationResult() const;

protected:
	virtual void BeginPlay() override;

private:
	void InitializeAbilitySystem();

	UFUNCTION()
	void OnRep_DummyId();

	UPROPERTY(VisibleAnywhere, Category = "Golem|AbilityYard")
	TObjectPtr<UStaticMeshComponent> Body;

	UPROPERTY(VisibleAnywhere, Category = "Golem|AbilityYard")
	TObjectPtr<UTextRenderComponent> Label;

	UPROPERTY(VisibleAnywhere, Category = "Golem|AbilityYard")
	TObjectPtr<UGolemAbilitySystemComponent> AbilitySystem;

	UPROPERTY(VisibleAnywhere, Category = "Golem|AbilityYard")
	TObjectPtr<UGolemAttributeSet> Attributes;

	UPROPERTY(VisibleAnywhere, Category = "Golem|AbilityYard")
	TObjectPtr<UGolemLifeComponent> Life;

	UPROPERTY(ReplicatedUsing = OnRep_DummyId)
	FGolemEntityId DummyId;

	UPROPERTY(Transient)
	FGolemResult InitializationResult;
};
