#pragma once

#include "CoreMinimal.h"
#include "Game/GolemGameMode.h"
#include "GolemAbilityYardGameMode.generated.h"

class AGolemAbilityDummy;
class AGolemAbilityYardState;
class AGolemCharacter;
class UGolemAbilitySystemComponent;
class UGolemLifeComponent;
enum class EGolemLifeState : uint8;
struct FOnAttributeChangeData;

struct FGolemAbilityYardBinding
{
	TWeakObjectPtr<UGolemAbilitySystemComponent> AbilitySystem;
	TWeakObjectPtr<UGolemLifeComponent> Life;
	FDelegateHandle HealthHandle;
	FDelegateHandle LifeHandle;
};

/** Composition for the cumulative GAS exercise; objectives subscribe to actual authoritative outcomes. */
UCLASS()
class THREEDREAMGOLEMS_API AGolemAbilityYardGameMode : public AGolemGameMode
{
	GENERATED_BODY()

public:
	AGolemAbilityYardGameMode();
	virtual void InitGameState() override;
	virtual FGolemResult TryInteract(APlayerController* Player) override;
	virtual FGolemResult TryRestartDrill(APlayerController* Player) override;
	AGolemAbilityYardState* GetYardState() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void BindExercise();
	void HandleDummyHealthChanged(const FOnAttributeChangeData& Change);
	void HandleGolemHealthChanged(TWeakObjectPtr<AGolemCharacter> Golem, const FOnAttributeChangeData& Change);
	void HandleGolemLifeChanged(TWeakObjectPtr<AGolemCharacter> Golem, EGolemLifeState Previous,
		EGolemLifeState Current);
	void UnbindExercise();

	UPROPERTY(Transient)
	TObjectPtr<AGolemAbilityYardState> YardState;

	UPROPERTY(Transient)
	TObjectPtr<AGolemAbilityDummy> Dummy;

	TArray<FGolemAbilityYardBinding> Bindings;
	bool bIsResetting = false;
};
