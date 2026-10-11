#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectTypes.h"
#include "RPGLoop/KnightRPGHUD.h"
#include "Skills/KnightSkillRuntimeComponent.h"
#include "KnightSkillLabHUD.generated.h"

class AKnightCharacter;
class AKnightRPGUIPlayerController;
class UKnightBossArmorComponent;
class AKnightPartyArenaState;
class AKnightPartyState;
class AKnightRunestone;
class UKnightAbilitySystemComponent;
class UKnightLifeComponent;
class UKnightCombatComponent;
struct FKnightPartyArenaSnapshot;

struct FKnightSkillLabPartyHUDBinding
{
	TWeakObjectPtr<AKnightCharacter> Knight;
	TWeakObjectPtr<UKnightAbilitySystemComponent> AbilitySystem;
	TWeakObjectPtr<UKnightLifeComponent> Life;
	TWeakObjectPtr<UKnightSkillRuntimeComponent> Runtime;
	TArray<TPair<FGameplayAttribute, FDelegateHandle>> Attributes;
	FDelegateHandle LifeHandle;
	FDelegateHandle SkillHandle;
};

struct FKnightSkillLabRunestoneHUDBinding
{
	TWeakObjectPtr<AKnightRunestone> Stone;
	FDelegateHandle Handle;
	FText Label;
	float EndServerTime = 0.0f;
};

/** Event-bound skill state; DrawHUD only interpolates already-replicated cooldown timestamps. */
UCLASS()
class THREEDREAMINGKNIGHTS_API AKnightSkillLabHUD : public AKnightRPGHUD
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
	virtual FText GetKnightLabel(const AKnightCharacter* InKnight) const override;

private:
	void BindSkillSources();
	void HandleControlledKnight(AKnightCharacter* Previous, AKnightCharacter* Current);
	void HandleSkillState(const FKnightSkillRuntimeState& InState);
	void HandleArenaState(const FKnightPartyArenaSnapshot& Snapshot);
	void BindArmorSource();
	void RefreshArmor();
	void BindWorldOffers();
	void RefreshWorldOffers();
	void BindPartyVitals();
	void RefreshPartyVitals();
	void UnbindPartyVitals();
	void RefreshBossVitals();
	void UnbindBossVitals();
	TWeakObjectPtr<AKnightRPGUIPlayerController> Controller;
	TWeakObjectPtr<UKnightSkillRuntimeComponent> Runtime;
	TWeakObjectPtr<AKnightPartyArenaState> Arena;
	TWeakObjectPtr<UKnightBossArmorComponent> Armor;
	TWeakObjectPtr<AKnightPartyState> Party;
	TWeakObjectPtr<UKnightAbilitySystemComponent> BossAbilitySystem;
	TWeakObjectPtr<UKnightCombatComponent> BossCombat;
	TArray<FKnightSkillLabPartyHUDBinding> PartyBindings;
	TArray<FKnightSkillLabRunestoneHUDBinding> RunestoneBindings;
	TArray<TPair<FGameplayAttribute, FDelegateHandle>> BossAttributeHandles;
	FText PartyVitals;
	FText BossVitals;
	float ArmorEndServerTime = 0.0f;
	FKnightSkillRuntimeState CachedState;
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
