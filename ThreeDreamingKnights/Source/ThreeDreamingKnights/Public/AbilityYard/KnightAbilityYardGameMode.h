#pragma once

#include "CoreMinimal.h"
#include "Game/KnightGameMode.h"
#include "KnightAbilityYardGameMode.generated.h"

class AKnightAbilityDummy;
class AKnightAbilityYardState;
class AKnightCharacter;
class UKnightAbilitySystemComponent;
class UKnightLifeComponent;
enum class EKnightLifeState : uint8;
struct FOnAttributeChangeData;

struct FKnightAbilityYardBinding
{
	TWeakObjectPtr<UKnightAbilitySystemComponent> AbilitySystem;
	TWeakObjectPtr<UKnightLifeComponent> Life;
	FDelegateHandle HealthHandle;
	FDelegateHandle LifeHandle;
};

/** Composition for the cumulative GAS exercise; objectives subscribe to actual authoritative outcomes. */
UCLASS()
class THREEDREAMINGKNIGHTS_API AKnightAbilityYardGameMode : public AKnightGameMode
{
	GENERATED_BODY()

public:
	AKnightAbilityYardGameMode();
	virtual void InitGameState() override;
	virtual FKnightResult TryInteract(APlayerController* Player) override;
	virtual FKnightResult TryRestartDrill(APlayerController* Player) override;
	AKnightAbilityYardState* GetYardState() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void BindExercise();
	void HandleDummyHealthChanged(const FOnAttributeChangeData& Change);
	void HandleKnightHealthChanged(TWeakObjectPtr<AKnightCharacter> Knight, const FOnAttributeChangeData& Change);
	void HandleKnightLifeChanged(TWeakObjectPtr<AKnightCharacter> Knight, EKnightLifeState Previous,
		EKnightLifeState Current);
	void UnbindExercise();

	UPROPERTY(Transient)
	TObjectPtr<AKnightAbilityYardState> YardState;

	UPROPERTY(Transient)
	TObjectPtr<AKnightAbilityDummy> Dummy;

	TArray<FKnightAbilityYardBinding> Bindings;
	bool bIsResetting = false;
};
