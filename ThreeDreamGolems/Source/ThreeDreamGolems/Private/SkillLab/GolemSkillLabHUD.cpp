#include "SkillLab/GolemSkillLabHUD.h"

#include "Character/GolemCharacter.h"
#include "Attributes/GolemAttributeSet.h"
#include "Components/GolemAbilitySystemComponent.h"
#include "Components/GolemLifeComponent.h"
#include "Components/GolemCombatComponent.h"
#include "Engine/Canvas.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "Framework/GolemGameState.h"
#include "Party/GolemPartyState.h"
#include "PartyArena/GolemArenaEnemy.h"
#include "PartyArena/GolemPartyArenaState.h"
#include "RPGLoop/GolemRPGUIPlayerController.h"
#include "SkillLab/GolemBossArmorComponent.h"
#include "SkillLab/GolemRunestone.h"
#include "TimerManager.h"

FText AGolemSkillLabHUD::GetCombatHelp() const
{
	return NSLOCTEXT("GolemSkillLab", "CombatHelp", "WASD move | LMB light / RMB heavy | Q block / Alt parry / Shift dodge | MMB lock | G rune / H finisher | T traverse");
}

FText AGolemSkillLabHUD::GetPartyHelp() const
{
	return NSLOCTEXT("GolemSkillLab", "PartyHelp", "N follow / J hold / B focus / K regroup | E revive | Tab switch | Gamepad: LT+RT heavy, LT+D-pad skills");
}

FText AGolemSkillLabHUD::GetRecoveryHelp() const
{
	return NSLOCTEXT("GolemSkillLab", "RecoveryHelp", "Recovery: Stamina 25/s; class resource 10/s, paused 1s after spending. HP requires healing, revive or checkpoint.");
}

FText AGolemSkillLabHUD::GetGolemLabel(const AGolemCharacter* InGolem) const
{
	const FName Hero = InGolem->GetHeroDefinitionId().PrimaryAssetName;
	if (Hero == TEXT("Training_Red"))
	{
		return NSLOCTEXT("GolemSkillLab", "SkyLabel", "Sky");
	}
	if (Hero == TEXT("Training_Blue"))
	{
		return NSLOCTEXT("GolemSkillLab", "SeaLabel", "Sea");
	}
	return NSLOCTEXT("GolemSkillLab", "EarthLabel", "Earth");
}

void AGolemSkillLabHUD::BeginPlay()
{
	Super::BeginPlay();
	BindSkillSources();
	BindWorldOffers();
}

void AGolemSkillLabHUD::BindWorldOffers()
{
	const AGolemGameState* State = GetWorld()->GetGameState<AGolemGameState>();
	AGolemPartyState* NewParty = State != nullptr ? State->GetPartyState() : nullptr;
	if (!Party.IsValid() && NewParty != nullptr && NewParty->IsCanonicalRosterReady())
	{
		Party = NewParty;
		PartyHandle = NewParty->OnRosterChanged.AddUObject(this, &ThisClass::BindPartyVitals);
		BindPartyVitals();
	}
	for (TActorIterator<AGolemRunestone> It(GetWorld()); It; ++It)
	{
		if (!RunestoneBindings.ContainsByPredicate([&It](const FGolemSkillLabRunestoneHUDBinding& Binding)
			{
				return Binding.Stone.Get() == *It;
			}))
		{
			FGolemSkillLabRunestoneHUDBinding Binding;
			Binding.Stone = *It;
			Binding.Handle = It->OnStateChanged.AddUObject(this, &ThisClass::RefreshWorldOffers);
			RunestoneBindings.Add(Binding);
		}
	}
	RefreshWorldOffers();
	if ((!Party.IsValid() || RunestoneBindings.Num() < 3) && ++WorldBindingAttempts < 100)
	{
		// Only wait for initial actor replication; subsequent changes arrive through owned delegates.
		GetWorldTimerManager().SetTimer(WorldReadinessTimer, this, &ThisClass::BindWorldOffers, 0.1f, false);
	}
}

void AGolemSkillLabHUD::RefreshWorldOffers()
{
	for (FGolemSkillLabRunestoneHUDBinding& Binding : RunestoneBindings)
	{
		if (!Binding.Stone.IsValid())
		{
			continue;
		}
		const AGolemRunestone* Stone = Binding.Stone.Get();
		const FText Name = Stone->GetKind() == EGolemRunestoneKind::Healing
			? NSLOCTEXT("GolemSkillLab", "OfferHealing", "Healing")
			: (Stone->GetKind() == EGolemRunestoneKind::Crystal
			? NSLOCTEXT("GolemSkillLab", "OfferCrystal", "Crystal")
			: NSLOCTEXT("GolemSkillLab", "OfferFrost", "Frost"));
		const FText Status = Stone->GetState() == EGolemRunestoneState::Ready
			? NSLOCTEXT("GolemSkillLab", "OfferReady", "Ready: F within 2m")
			: (Stone->GetState() == EGolemRunestoneState::Channeling
			? NSLOCTEXT("GolemSkillLab", "OfferChannel", "Channeling")
			: NSLOCTEXT("GolemSkillLab", "OfferCooldown", "Cooldown"));
		Binding.Label = FText::Format(NSLOCTEXT("GolemSkillLab", "OfferLabel", "{0}: {1}"), Name, Status);
		Binding.EndServerTime = Stone->GetStateEndServerTime();
	}
}

void AGolemSkillLabHUD::BindPartyVitals()
{
	UnbindPartyVitals();
	if (!Party.IsValid())
	{
		return;
	}
	for (const FGolemRosterEntry& Entry : Party->GetRoster())
	{
		if (!IsValid(Entry.Golem))
		{
			continue;
		}
		FGolemSkillLabPartyHUDBinding Binding;
		Binding.Golem = Entry.Golem;
		Binding.AbilitySystem = Entry.Golem->GetGolemAbilitySystem();
		Binding.Life = Entry.Golem->GetLifeComponent();
		Binding.Runtime = Entry.Golem->FindComponentByClass<UGolemSkillRuntimeComponent>();
		for (const FGameplayAttribute& Attribute : {UGolemAttributeSet::GetHealthAttribute(),
			UGolemAttributeSet::GetMaxHealthAttribute()})
		{
			const FDelegateHandle Handle = Binding.AbilitySystem->GetGameplayAttributeValueChangeDelegate(Attribute)
				.AddWeakLambda(this, [this](const FOnAttributeChangeData& Change)
					{
						RefreshPartyVitals();
					});
			Binding.Attributes.Emplace(Attribute, Handle);
		}
		Binding.LifeHandle = Binding.Life->OnLifeStateChanged.AddWeakLambda(this,
			[this](EGolemLifeState Previous, EGolemLifeState Current)
			{
				RefreshPartyVitals();
			});
		if (Binding.Runtime.IsValid())
		{
			Binding.SkillHandle = Binding.Runtime->OnSkillStateChanged.AddWeakLambda(this,
				[this](const FGolemSkillRuntimeState& SkillState)
				{
					RefreshPartyVitals();
				});
		}
		PartyBindings.Add(MoveTemp(Binding));
	}
	RefreshPartyVitals();
}

void AGolemSkillLabHUD::RefreshPartyVitals()
{
	FString Text;
	for (const FGolemSkillLabPartyHUDBinding& Binding : PartyBindings)
	{
		if (!Binding.Golem.IsValid() || !Binding.AbilitySystem.IsValid() || !Binding.Life.IsValid())
		{
			continue;
		}
		const FText LifeName = Binding.Life->IsAlive() ? NSLOCTEXT("GolemSkillLab", "PartyAlive", "Alive")
			: (Binding.Life->GetLifeState() == EGolemLifeState::Downed
			? NSLOCTEXT("GolemSkillLab", "PartyDowned", "DOWNED")
			: NSLOCTEXT("GolemSkillLab", "PartyDead", "DEAD"));
		Text += FText::Format(NSLOCTEXT("GolemSkillLab", "PartyVitals", "{0} HP {1}/{2} shield {3} {4}   "),
			GetGolemLabel(Binding.Golem.Get()),
			FText::AsNumber(FMath::RoundToInt(Binding.AbilitySystem->GetNumericAttribute(UGolemAttributeSet::GetHealthAttribute()))),
			FText::AsNumber(FMath::RoundToInt(Binding.AbilitySystem->GetNumericAttribute(UGolemAttributeSet::GetMaxHealthAttribute()))),
			FText::AsNumber(FMath::RoundToInt(Binding.Runtime.IsValid() ? Binding.Runtime->GetSkillState().Shield : 0.0f)),
			LifeName).ToString();
	}
	PartyVitals = FText::FromString(Text);
}

void AGolemSkillLabHUD::UnbindPartyVitals()
{
	for (const FGolemSkillLabPartyHUDBinding& Binding : PartyBindings)
	{
		if (Binding.AbilitySystem.IsValid())
		{
			for (const TPair<FGameplayAttribute, FDelegateHandle>& Registration : Binding.Attributes)
			{
				Binding.AbilitySystem->GetGameplayAttributeValueChangeDelegate(Registration.Key).Remove(Registration.Value);
			}
		}
		if (Binding.Life.IsValid())
		{
			Binding.Life->OnLifeStateChanged.Remove(Binding.LifeHandle);
		}
		if (Binding.Runtime.IsValid())
		{
			Binding.Runtime->OnSkillStateChanged.Remove(Binding.SkillHandle);
		}
	}
	PartyBindings.Reset();
}

void AGolemSkillLabHUD::BindSkillSources()
{
	AGolemRPGUIPlayerController* SkillPlayer = Cast<AGolemRPGUIPlayerController>(GetOwningPlayerController());
	if (SkillPlayer == nullptr || SkillPlayer->GetControlledGolem() == nullptr)
	{
		if (++BindingAttempts < 100)
		{
			GetWorldTimerManager().SetTimer(ReadinessTimer, this, &ThisClass::BindSkillSources, 0.1f, false);
		}
		return;
	}
	Controller = SkillPlayer;
	ControlHandle = SkillPlayer->OnControlledGolemChanged.AddUObject(this, &ThisClass::HandleControlledGolem);
	HandleControlledGolem(nullptr, SkillPlayer->GetControlledGolem());
	for (TActorIterator<AGolemPartyArenaState> It(GetWorld()); It; ++It)
	{
		Arena = *It;
		ArenaHandle = It->OnSnapshotChanged.AddUObject(this, &ThisClass::HandleArenaState);
		HandleArenaState(It->GetSnapshot());
		break;
	}
}

void AGolemSkillLabHUD::HandleControlledGolem(AGolemCharacter* Previous, AGolemCharacter* Current)
{
	if (Runtime.IsValid())
	{
		Runtime->OnSkillStateChanged.Remove(RuntimeHandle);
	}
	Runtime.Reset();
	RuntimeHandle.Reset();
	CachedState = FGolemSkillRuntimeState();
	if (IsValid(Current))
	{
		Runtime = Current->FindComponentByClass<UGolemSkillRuntimeComponent>();
		if (Runtime.IsValid())
		{
			RuntimeHandle = Runtime->OnSkillStateChanged.AddUObject(this, &ThisClass::HandleSkillState);
			HandleSkillState(Runtime->GetSkillState());
		}
	}
}

void AGolemSkillLabHUD::HandleSkillState(const FGolemSkillRuntimeState& InState)
{
	CachedState = InState;
}

void AGolemSkillLabHUD::HandleArenaState(const FGolemPartyArenaSnapshot& Snapshot)
{
	GetWorldTimerManager().ClearTimer(ArmorReadinessTimer);
	if (Armor.IsValid())
	{
		Armor->OnArmorChanged.Remove(ArmorHandle);
	}
	Armor.Reset();
	ArmorHandle.Reset();
	UnbindBossVitals();
	ArmorBindingAttempts = 0;
	if (Snapshot.Stage == EGolemArenaStage::Boss)
	{
		BindArmorSource();
	}
	RefreshArmor();
}

void AGolemSkillLabHUD::BindArmorSource()
{
	for (TActorIterator<AGolemArenaEnemy> It(GetWorld()); It; ++It)
	{
		UGolemBossArmorComponent* Component = It->FindComponentByClass<UGolemBossArmorComponent>();
		if (Component != nullptr && (Component->GetArmorZone() != nullptr
			|| It->GetArchetypeId().PrimaryAssetName == TEXT("ArenaBoss")))
		{
			Armor = Component;
			ArmorHandle = Component->OnArmorChanged.AddUObject(this, &ThisClass::RefreshArmor);
			BossAbilitySystem = It->GetGolemAbilitySystem();
			for (const FGameplayAttribute& Attribute : {UGolemAttributeSet::GetHealthAttribute(),
				UGolemAttributeSet::GetMaxHealthAttribute()})
			{
				const FDelegateHandle Handle = BossAbilitySystem->GetGameplayAttributeValueChangeDelegate(Attribute)
					.AddWeakLambda(this, [this](const FOnAttributeChangeData& Change)
						{
							RefreshBossVitals();
						});
				BossAttributeHandles.Emplace(Attribute, Handle);
			}
			BossCombat = It->GetCombatComponent();
			BossCombatHandle = BossCombat->OnCombatSnapshotChanged.AddWeakLambda(this,
				[this](const FGolemCombatSnapshot& Snapshot)
				{
					RefreshBossVitals();
				});
			BossProfileHandle = BossCombat->OnProfileChanged.AddWeakLambda(this,
				[this](UGolemCombatProfileDefinition* Profile)
				{
					RefreshBossVitals();
				});
			RefreshBossVitals();
			RefreshArmor();
			return;
		}
	}
	// Arena snapshot and boss actor channels can arrive in either order on a client.
	if (++ArmorBindingAttempts < 50)
	{
		GetWorldTimerManager().SetTimer(ArmorReadinessTimer, this, &ThisClass::BindArmorSource, 0.1f, false);
	}
}

void AGolemSkillLabHUD::RefreshBossVitals()
{
	if (BossAbilitySystem.IsValid())
	{
		BossVitals = FText::Format(NSLOCTEXT("GolemSkillLab", "BossVitals", "Boss HP {0}/{1} | Poise {2}/{3}"),
			FText::AsNumber(FMath::RoundToInt(BossAbilitySystem->GetNumericAttribute(UGolemAttributeSet::GetHealthAttribute()))),
			FText::AsNumber(FMath::RoundToInt(BossAbilitySystem->GetNumericAttribute(UGolemAttributeSet::GetMaxHealthAttribute()))),
			FText::AsNumber(FMath::RoundToInt(BossCombat.IsValid() ? BossCombat->GetSnapshot().Poise : 0.0f)),
			FText::AsNumber(FMath::RoundToInt(BossCombat.IsValid() && BossCombat->GetProfile() != nullptr
				? BossCombat->GetProfile()->GetTuning().MaxPoise : 0.0f)));
	}
}

void AGolemSkillLabHUD::UnbindBossVitals()
{
	if (BossAbilitySystem.IsValid())
	{
		for (const TPair<FGameplayAttribute, FDelegateHandle>& Registration : BossAttributeHandles)
		{
			BossAbilitySystem->GetGameplayAttributeValueChangeDelegate(Registration.Key).Remove(Registration.Value);
		}
	}
	BossAttributeHandles.Reset();
	BossAbilitySystem.Reset();
	if (BossCombat.IsValid())
	{
		BossCombat->OnCombatSnapshotChanged.Remove(BossCombatHandle);
		BossCombat->OnProfileChanged.Remove(BossProfileHandle);
	}
	BossCombat.Reset();
	BossCombatHandle.Reset();
	BossProfileHandle.Reset();
	BossVitals = FText::GetEmpty();
}

void AGolemSkillLabHUD::RefreshArmor()
{
	if (Armor.IsValid() && Armor->GetArmorZone() != nullptr)
	{
		const FText StateName = Armor->GetArmorState() == EGolemBossArmorState::Intact
			? NSLOCTEXT("GolemSkillLab", "ArmorIntact", "Intact")
			: (Armor->GetArmorState() == EGolemBossArmorState::Brittle
			? NSLOCTEXT("GolemSkillLab", "ArmorBrittle", "Brittle") : NSLOCTEXT("GolemSkillLab", "ArmorBroken", "Broken"));
		ArmorText = FText::Format(NSLOCTEXT("GolemSkillLab", "ArmorHUD", "Boss leg armor: {0}, durability {1}/100"),
			StateName, FText::AsNumber(Armor->GetDurability()));
		ArmorEndServerTime = Armor->GetStateEndServerTime();
	}
	else
	{
		ArmorText = NSLOCTEXT("GolemSkillLab", "ArmorHelp", "Boss: FROST Runestone makes leg zone brittle; aim heavy at lower armor.");
		ArmorEndServerTime = 0.0f;
	}
}

void AGolemSkillLabHUD::DrawHUD()
{
	Super::DrawHUD();
	if (Canvas == nullptr)
	{
		return;
	}
	const AGameStateBase* State = GetWorld()->GetGameState();
	const double Now = State != nullptr ? State->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
	const FString Kit = FString::Printf(TEXT("%s | stacks %d | shield %.0f | Neptune debt %.0f | buff %.1fs"),
		*CachedState.Build.SpecId.ToString(), CachedState.Stacks, CachedState.Shield, CachedState.DeferredDamage,
		FMath::Max(0.0, CachedState.BuffEnd - Now));
	DrawText(Kit, FLinearColor::White, 25.0f, 350.0f, nullptr, 1.1f);
	FString Slots;
	const TCHAR* Keys[]{TEXT("Z"), TEXT("X"), TEXT("C"), TEXT("V")};
	for (int32 Index = 0; Index < 4; ++Index)
	{
		const FGolemSpecActionDefinition Skill = FGolemSkillCatalog::GetSkill(CachedState.Build, Index + 1);
		const double Remaining = CachedState.CooldownEnds.IsValidIndex(Index)
			? FMath::Max(0.0, CachedState.CooldownEnds[Index] - Now) : 0.0;
		Slots += FString::Printf(TEXT("%s: %s [%.1fs]   "), Keys[Index], *Skill.DisplayName.ToString(), Remaining);
	}
	DrawText(Slots, FLinearColor::Yellow, 25.0f, 377.0f, nullptr, 1.0f);
	const FString ArmorLine = ArmorText.ToString() + (ArmorEndServerTime > 0.0f
		? FString::Printf(TEXT(" | %.1fs"), FMath::Max(0.0, ArmorEndServerTime - Now)) : FString());
	DrawText(ArmorLine, FLinearColor(0.0f, 1.0f, 1.0f), 25.0f, 404.0f, nullptr, 1.0f);
	DrawText(TEXT("I: Character/Talents/Spellbook/Runes | F at cyan station: hurt fixture | F Runestone: channel2s | F pickup/fire: collect"),
		FLinearColor::White, 25.0f, 431.0f, nullptr, 0.9f);
	DrawText(PartyVitals.ToString(), FLinearColor::White, 25.0f, 458.0f, nullptr, 0.9f);
	DrawText(BossVitals.ToString(), FLinearColor::Yellow, 25.0f, 485.0f, nullptr, 1.0f);
	FString Offers;
	for (const FGolemSkillLabRunestoneHUDBinding& Binding : RunestoneBindings)
	{
		Offers += Binding.Label.ToString();
		if (Binding.EndServerTime > 0.0f)
		{
			Offers += FString::Printf(TEXT(" %.1fs"), FMath::Max(0.0, Binding.EndServerTime - Now));
		}
		Offers += TEXT("   ");
	}
	DrawText(Offers, FLinearColor(0.0f, 1.0f, 1.0f), 25.0f, 512.0f, nullptr, 0.9f);
}

void AGolemSkillLabHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(ReadinessTimer);
	GetWorldTimerManager().ClearTimer(ArmorReadinessTimer);
	GetWorldTimerManager().ClearTimer(WorldReadinessTimer);
	UnbindPartyVitals();
	UnbindBossVitals();
	if (Party.IsValid())
	{
		Party->OnRosterChanged.Remove(PartyHandle);
	}
	for (const FGolemSkillLabRunestoneHUDBinding& Binding : RunestoneBindings)
	{
		if (Binding.Stone.IsValid())
		{
			Binding.Stone->OnStateChanged.Remove(Binding.Handle);
		}
	}
	RunestoneBindings.Reset();
	if (Controller.IsValid())
	{
		Controller->OnControlledGolemChanged.Remove(ControlHandle);
	}
	if (Runtime.IsValid())
	{
		Runtime->OnSkillStateChanged.Remove(RuntimeHandle);
	}
	if (Arena.IsValid())
	{
		Arena->OnSnapshotChanged.Remove(ArenaHandle);
	}
	if (Armor.IsValid())
	{
		Armor->OnArmorChanged.Remove(ArmorHandle);
	}
	Super::EndPlay(EndPlayReason);
}
