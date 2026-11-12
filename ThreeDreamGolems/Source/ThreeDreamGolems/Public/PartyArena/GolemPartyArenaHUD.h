#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "GameplayEffectTypes.h"
#include "PartyArena/GolemPartyArenaState.h"
#include "Types/GolemResult.h"
#include "GolemPartyArenaHUD.generated.h"

class AGolemCharacter;
class AGolemPartyState;
class AGolemArenaPlayerController;
class UGolemAbilitySystemComponent;
class UGolemLifeComponent;
class UGolemCombatComponent;
class AGolemArenaCompanionController;
class AGolemEnemyController;
struct FGolemCombatSnapshot;
enum class EGolemLifeState : uint8;

struct FGolemArenaDebugBinding
{
	TWeakObjectPtr<AGolemArenaCompanionController> Companion;
	TWeakObjectPtr<AGolemEnemyController> Enemy;
	FDelegateHandle Handle;
};

/** Early playable presentation uses initial domain snapshots and owned subscriptions, before M7 CommonUI screens. */
UCLASS()
class THREEDREAMGOLEMS_API AGolemPartyArenaHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;
	const FGolemPartyArenaSnapshot& GetDisplayedArenaSnapshot() const;
	bool IsDisplayingArenaVictory() const;
	bool IsDisplayingArenaDefeat() const;
	float GetDisplayedHealth() const;
	FText GetDisplayedCommandFeedback() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;
	virtual FText GetArenaTitle() const;
	virtual FText GetCombatHelp() const;
	virtual FText GetPartyHelp() const;
	virtual FText GetRecoveryHelp() const;
	virtual FText GetGolemLabel(const AGolemCharacter* InGolem) const;

private:
	void BindSources();
	void BindControlledGolem(AGolemCharacter* Previous, AGolemCharacter* Current);
	void UnbindControlledGolem();
	void RefreshParty();
	void RefreshAIDebug();
	void UnbindAIDebug();
	void HandleArenaChanged(const FGolemPartyArenaSnapshot& Snapshot);
	void HandleCommandResult(const FGolemResult& Result);
	void HandleBaseCommandResult(const FGolemResult& Result);
	void HandleAttributeChanged(const FOnAttributeChangeData& Change);
	void HandleLifeChanged(EGolemLifeState Previous, EGolemLifeState Current);
	void HandleCombatChanged(const FGolemCombatSnapshot& Snapshot);
	TWeakObjectPtr<AGolemArenaPlayerController> Player;
	TWeakObjectPtr<AGolemPartyArenaState> Arena;
	TWeakObjectPtr<AGolemPartyState> Party;
	TWeakObjectPtr<AGolemCharacter> Golem;
	TWeakObjectPtr<UGolemAbilitySystemComponent> AbilitySystem;
	TWeakObjectPtr<UGolemLifeComponent> Life;
	TWeakObjectPtr<UGolemCombatComponent> Combat;
	FDelegateHandle ControlHandle;
	FDelegateHandle CommandHandle;
	FDelegateHandle BaseCommandHandle;
	FDelegateHandle ArenaHandle;
	FDelegateHandle RosterHandle;
	FDelegateHandle LifeHandle;
	FDelegateHandle CombatHandle;
	TArray<TPair<FGameplayAttribute, FDelegateHandle>> AttributeHandles;
	FGolemPartyArenaSnapshot DisplayedArena;
	FText DisplayedName;
	FText PartyFeedback;
	FText CommandFeedback;
	FText LifeFeedback;
	FString AIDebugFeedback;
	TArray<FGolemArenaDebugBinding> DebugBindings;
	float Health = 0.0f;
	float MaxHealth = 0.0f;
	bool bAttacking = false;
	bool bBlocking = false;
	FTimerHandle InitialBindingTimer;
	int32 BindingAttempts = 0;
};
