#pragma once

#include "CoreMinimal.h"
#include "AbilityYard/KnightAbilityYardGameMode.h"
#include "Types/KnightStableIds.h"
#include "KnightCombatChallengeGameMode.generated.h"

class AKnightCombatChallengeState;
class UKnightCombatComponent;
struct FKnightCombatOutcome;

struct FKnightCombatChallengeBinding
{
	TWeakObjectPtr<UKnightCombatComponent> Combat;
	TWeakObjectPtr<UKnightLifeComponent> Life;
	FDelegateHandle OutcomeHandle;
	FDelegateHandle LifeHandle;
};

/** M3 composition enables authored combat while retaining the party and ability exercises. */
UCLASS()
class THREEDREAMINGKNIGHTS_API AKnightCombatChallengeGameMode : public AKnightAbilityYardGameMode
{
	GENERATED_BODY()

public:
	AKnightCombatChallengeGameMode();
	virtual void InitGameState() override;
	virtual FKnightResult TryInteract(APlayerController* Player) override;
	virtual FKnightResult TryRestartDrill(APlayerController* Player) override;
	AKnightCombatChallengeState* GetChallengeState() const;
	const FKnightResult& GetCombatInitializationResult() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;

private:
	void BindChallenge();
	void UnbindChallenge();
	void HandleCombatOutcome(const FKnightCombatOutcome& Outcome);
	void HandleKnightLifeChanged(TWeakObjectPtr<AKnightCharacter> Knight,
		EKnightLifeState Previous, EKnightLifeState Current);
	UPROPERTY(Transient)
	TObjectPtr<AKnightCombatChallengeState> ChallengeState;
	UPROPERTY(Transient)
	FKnightResult CombatInitializationResult;
	TArray<FKnightCombatChallengeBinding> Bindings;
	TSet<FKnightEntityId> CanonicalIds;
	TSet<FGuid> RecordedConfirmations;
	bool bResettingChallenge = false;
};
