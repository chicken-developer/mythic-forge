#pragma once

#include "CoreMinimal.h"
#include "AbilityYard/GolemAbilityYardGameMode.h"
#include "Types/GolemStableIds.h"
#include "GolemCombatChallengeGameMode.generated.h"

class AGolemCombatChallengeState;
class UGolemCombatComponent;
struct FGolemCombatOutcome;

struct FGolemCombatChallengeBinding
{
	TWeakObjectPtr<UGolemCombatComponent> Combat;
	TWeakObjectPtr<UGolemLifeComponent> Life;
	FDelegateHandle OutcomeHandle;
	FDelegateHandle LifeHandle;
};

/** M3 composition enables authored combat while retaining the party and ability exercises. */
UCLASS()
class THREEDREAMGOLEMS_API AGolemCombatChallengeGameMode : public AGolemAbilityYardGameMode
{
	GENERATED_BODY()

public:
	AGolemCombatChallengeGameMode();
	virtual void InitGameState() override;
	virtual FGolemResult TryInteract(APlayerController* Player) override;
	virtual FGolemResult TryRestartDrill(APlayerController* Player) override;
	AGolemCombatChallengeState* GetChallengeState() const;
	const FGolemResult& GetCombatInitializationResult() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;

private:
	void BindChallenge();
	void UnbindChallenge();
	void HandleCombatOutcome(const FGolemCombatOutcome& Outcome);
	void HandleGolemLifeChanged(TWeakObjectPtr<AGolemCharacter> Golem,
		EGolemLifeState Previous, EGolemLifeState Current);
	UPROPERTY(Transient)
	TObjectPtr<AGolemCombatChallengeState> ChallengeState;
	UPROPERTY(Transient)
	FGolemResult CombatInitializationResult;
	TArray<FGolemCombatChallengeBinding> Bindings;
	TSet<FGolemEntityId> CanonicalIds;
	TSet<FGuid> RecordedConfirmations;
	bool bResettingChallenge = false;
};
