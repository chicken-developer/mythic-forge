#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectTypes.h"
#include "RPGLoop/GolemRPGHUD.h"
#include "Skills/GolemSkillRuntimeComponent.h"
#include "GolemSkillLabHUD.generated.h"

class AGolemCharacter;
class AGolemRPGUIPlayerController;
class UGolemBossArmorComponent;
class AGolemPartyArenaState;
class AGolemPartyState;
class AGolemRunestone;
class UGolemAbilitySystemComponent;
class UGolemLifeComponent;
class UGolemCombatComponent;
struct FGolemPartyArenaSnapshot;

struct FGolemSkillLabPartyHUDBinding
{
	TWeakObjectPtr<AGolemCharacter> Golem;
	TWeakObjectPtr<UGolemAbilitySystemComponent> AbilitySystem;
	TWeakObjectPtr<UGolemLifeComponent> Life;
	TWeakObjectPtr<UGolemSkillRuntimeComponent> Runtime;
	TArray<TPair<FGameplayAttribute, FDelegateHandle>> Attributes;
	FDelegateHandle LifeHandle;
	FDelegateHandle SkillHandle;
};

struct FGolemSkillLabRunestoneHUDBinding
{
	TWeakObjectPtr<AGolemRunestone> Stone;
	FDelegateHandle Handle;
	FText Label;
	float EndServerTime = 0.0f;
};

/** Event-bound skill state; DrawHUD only interpolates already-replicated cooldown timestamps. */
UCLASS()
class THREEDREAMGOLEMS_API AGolemSkillLabHUD : public AGolemRPGHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;
	virtual FText GetCombatHelp() const override;
	virtual FText GetPartyHelp() const override;
	virtual FText GetRecoveryHelp() const override;
	virtual FText GetGolemLabel(const AGolemCharacter* InGolem) const override;

private:
	void BindSkillSources();
	void HandleControlledGolem(AGolemCharacter* Previous, AGolemCharacter* Current);
	void HandleSkillState(const FGolemSkillRuntimeState& InState);
	void HandleArenaState(const FGolemPartyArenaSnapshot& Snapshot);
	void BindArmorSource();
	void RefreshArmor();
	void BindWorldOffers();
	void RefreshWorldOffers();
	void BindPartyVitals();
	void RefreshPartyVitals();
	void UnbindPartyVitals();
	void RefreshBossVitals();
	void UnbindBossVitals();
	TWeakObjectPtr<AGolemRPGUIPlayerController> Controller;
	TWeakObjectPtr<UGolemSkillRuntimeComponent> Runtime;
	TWeakObjectPtr<AGolemPartyArenaState> Arena;
	TWeakObjectPtr<UGolemBossArmorComponent> Armor;
	TWeakObjectPtr<AGolemPartyState> Party;
	TWeakObjectPtr<UGolemAbilitySystemComponent> BossAbilitySystem;
	TWeakObjectPtr<UGolemCombatComponent> BossCombat;
	TArray<FGolemSkillLabPartyHUDBinding> PartyBindings;
	TArray<FGolemSkillLabRunestoneHUDBinding> RunestoneBindings;
	TArray<TPair<FGameplayAttribute, FDelegateHandle>> BossAttributeHandles;
	FText PartyVitals;
	FText BossVitals;
	float ArmorEndServerTime = 0.0f;
	FGolemSkillRuntimeState CachedState;
	FText ArmorText;
	FDelegateHandle ControlHandle;
	FDelegateHandle RuntimeHandle;
	FDelegateHandle ArenaHandle;
	FDelegateHandle ArmorHandle;
	FDelegateHandle PartyHandle;
	FDelegateHandle BossCombatHandle;
	FDelegateHandle BossProfileHandle;
	FTimerHandle ReadinessTimer;
	FTimerHandle ArmorReadinessTimer;
	FTimerHandle WorldReadinessTimer;
	int32 BindingAttempts = 0;
	int32 ArmorBindingAttempts = 0;
	int32 WorldBindingAttempts = 0;
};
