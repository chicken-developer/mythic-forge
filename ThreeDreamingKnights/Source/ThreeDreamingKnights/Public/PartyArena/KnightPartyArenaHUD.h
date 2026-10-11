#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "GameplayEffectTypes.h"
#include "PartyArena/KnightPartyArenaState.h"
#include "Types/KnightResult.h"
#include "KnightPartyArenaHUD.generated.h"

class AKnightCharacter;
class AKnightPartyState;
class AKnightArenaPlayerController;
class UKnightAbilitySystemComponent;
class UKnightLifeComponent;
class UKnightCombatComponent;
class AKnightArenaCompanionController;
class AKnightEnemyController;
struct FKnightCombatSnapshot;
enum class EKnightLifeState : uint8;

struct FKnightArenaDebugBinding
{
	TWeakObjectPtr<AKnightArenaCompanionController> Companion;
	TWeakObjectPtr<AKnightEnemyController> Enemy;
	FDelegateHandle Handle;
};

/** Early playable presentation uses initial domain snapshots and owned subscriptions, before M7 CommonUI screens. */
UCLASS()
class THREEDREAMINGKNIGHTS_API AKnightPartyArenaHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;
	const FKnightPartyArenaSnapshot& GetDisplayedArenaSnapshot() const;
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
	virtual FText GetKnightLabel(const AKnightCharacter* InKnight) const;

private:
	void BindSources();
	void BindControlledKnight(AKnightCharacter* Previous, AKnightCharacter* Current);
	void UnbindControlledKnight();
	void RefreshParty();
	void RefreshAIDebug();
	void UnbindAIDebug();
	void HandleArenaChanged(const FKnightPartyArenaSnapshot& Snapshot);
	void HandleCommandResult(const FKnightResult& Result);
	void HandleBaseCommandResult(const FKnightResult& Result);
	void HandleAttributeChanged(const FOnAttributeChangeData& Change);
	void HandleLifeChanged(EKnightLifeState Previous, EKnightLifeState Current);
	void HandleCombatChanged(const FKnightCombatSnapshot& Snapshot);
	TWeakObjectPtr<AKnightArenaPlayerController> Player;
	TWeakObjectPtr<AKnightPartyArenaState> Arena;
	TWeakObjectPtr<AKnightPartyState> Party;
	TWeakObjectPtr<AKnightCharacter> Knight;
	TWeakObjectPtr<UKnightAbilitySystemComponent> AbilitySystem;
	TWeakObjectPtr<UKnightLifeComponent> Life;
	TWeakObjectPtr<UKnightCombatComponent> Combat;
	FDelegateHandle ControlHandle;
	FDelegateHandle CommandHandle;
	FDelegateHandle BaseCommandHandle;
	FDelegateHandle ArenaHandle;
	FDelegateHandle RosterHandle;
	FDelegateHandle LifeHandle;
	FDelegateHandle CombatHandle;
	TArray<TPair<FGameplayAttribute, FDelegateHandle>> AttributeHandles;
	FKnightPartyArenaSnapshot DisplayedArena;
	FText DisplayedName;
	FText PartyFeedback;
	FText CommandFeedback;
	FText LifeFeedback;
	FString AIDebugFeedback;
	TArray<FKnightArenaDebugBinding> DebugBindings;
	float Health = 0.0f;
	float MaxHealth = 0.0f;
	float Stamina = 0.0f;
	float RuneEnergy = 0.0f;
	bool bAttacking = false;
	bool bBlocking = false;
	FTimerHandle InitialBindingTimer;
	int32 BindingAttempts = 0;
};
