#include "SkillLab/KnightSkillLabHUD.h"

#include "Character/KnightCharacter.h"
#include "Attributes/KnightAttributeSet.h"
#include "Components/KnightAbilitySystemComponent.h"
#include "Components/KnightLifeComponent.h"
#include "Components/KnightCombatComponent.h"
#include "Engine/Canvas.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "Framework/KnightGameState.h"
#include "Party/KnightPartyState.h"
#include "PartyArena/KnightArenaEnemy.h"
#include "PartyArena/KnightPartyArenaState.h"
#include "RPGLoop/KnightRPGUIPlayerController.h"
#include "SkillLab/KnightBossArmorComponent.h"
#include "SkillLab/KnightRunestone.h"
#include "TimerManager.h"

FText AKnightSkillLabHUD::GetCombatHelp() const
{
	return NSLOCTEXT("KnightSkillLab", "CombatHelp", "WASD move | LMB light / RMB heavy | Q block / Alt parry / Shift dodge | MMB lock | G rune / H finisher | T traverse");
}

FText AKnightSkillLabHUD::GetPartyHelp() const
{
	return NSLOCTEXT("KnightSkillLab", "PartyHelp", "N follow / J hold / B focus / K regroup | E revive | Tab switch | Gamepad: LT+RT heavy, LT+D-pad skills");
}

FText AKnightSkillLabHUD::GetRecoveryHelp() const
{
	return NSLOCTEXT("KnightSkillLab", "RecoveryHelp", "Recovery: Stamina 25/s; class resource 10/s, paused 1s after spending. HP requires healing, revive or checkpoint.");
}

FText AKnightSkillLabHUD::GetKnightLabel(const AKnightCharacter* InKnight) const
{
	const FName Hero = InKnight->GetHeroDefinitionId().PrimaryAssetName;
	if (Hero == TEXT("Training_Red"))
	{
		return NSLOCTEXT("KnightSkillLab", "SkyLabel", "Sky");
	}
	if (Hero == TEXT("Training_Blue"))
	{
		return NSLOCTEXT("KnightSkillLab", "SeaLabel", "Sea");
	}
	return NSLOCTEXT("KnightSkillLab", "EarthLabel", "Earth");
}

void AKnightSkillLabHUD::BeginPlay()
{
	Super::BeginPlay();
	BindSkillSources();
	BindWorldOffers();
}

void AKnightSkillLabHUD::BindWorldOffers()
{
	const AKnightGameState* State = GetWorld()->GetGameState<AKnightGameState>();
	AKnightPartyState* NewParty = State != nullptr ? State->GetPartyState() : nullptr;
	if (!Party.IsValid() && NewParty != nullptr && NewParty->IsCanonicalRosterReady())
	{
		Party = NewParty;
		PartyHandle = NewParty->OnRosterChanged.AddUObject(this, &ThisClass::BindPartyVitals);
		BindPartyVitals();
	}
	for (TActorIterator<AKnightRunestone> It(GetWorld()); It; ++It)
	{
		if (!RunestoneBindings.ContainsByPredicate([&It](const FKnightSkillLabRunestoneHUDBinding& Binding)
			{
				return Binding.Stone.Get() == *It;
			}))
		{
			FKnightSkillLabRunestoneHUDBinding Binding;
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

void AKnightSkillLabHUD::RefreshWorldOffers()
{
	for (FKnightSkillLabRunestoneHUDBinding& Binding : RunestoneBindings)
	{
		if (!Binding.Stone.IsValid())
		{
			continue;
		}
		const AKnightRunestone* Stone = Binding.Stone.Get();
		const FText Name = Stone->GetKind() == EKnightRunestoneKind::Healing
			? NSLOCTEXT("KnightSkillLab", "OfferHealing", "Healing")
			: (Stone->GetKind() == EKnightRunestoneKind::Crystal
			? NSLOCTEXT("KnightSkillLab", "OfferCrystal", "Crystal")
			: NSLOCTEXT("KnightSkillLab", "OfferFrost", "Frost"));
		const FText Status = Stone->GetState() == EKnightRunestoneState::Ready
			? NSLOCTEXT("KnightSkillLab", "OfferReady", "Ready: F within 2m")
			: (Stone->GetState() == EKnightRunestoneState::Channeling
			? NSLOCTEXT("KnightSkillLab", "OfferChannel", "Channeling")
			: NSLOCTEXT("KnightSkillLab", "OfferCooldown", "Cooldown"));
		Binding.Label = FText::Format(NSLOCTEXT("KnightSkillLab", "OfferLabel", "{0}: {1}"), Name, Status);
		Binding.EndServerTime = Stone->GetStateEndServerTime();
	}
}

void AKnightSkillLabHUD::BindPartyVitals()
{
	UnbindPartyVitals();
	if (!Party.IsValid())
	{
		return;
	}
	for (const FKnightRosterEntry& Entry : Party->GetRoster())
	{
		if (!IsValid(Entry.Knight))
		{
			continue;
		}
		FKnightSkillLabPartyHUDBinding Binding;
		Binding.Knight = Entry.Knight;
		Binding.AbilitySystem = Entry.Knight->GetKnightAbilitySystem();
		Binding.Life = Entry.Knight->GetLifeComponent();
		Binding.Runtime = Entry.Knight->FindComponentByClass<UKnightSkillRuntimeComponent>();
		for (const FGameplayAttribute& Attribute : {UKnightAttributeSet::GetHealthAttribute(),
			UKnightAttributeSet::GetMaxHealthAttribute()})
		{
			const FDelegateHandle Handle = Binding.AbilitySystem->GetGameplayAttributeValueChangeDelegate(Attribute)
				.AddWeakLambda(this, [this](const FOnAttributeChangeData& Change)
					{
						RefreshPartyVitals();
					});
			Binding.Attributes.Emplace(Attribute, Handle);
		}
		Binding.LifeHandle = Binding.Life->OnLifeStateChanged.AddWeakLambda(this,
			[this](EKnightLifeState Previous, EKnightLifeState Current)
			{
				RefreshPartyVitals();
			});
		if (Binding.Runtime.IsValid())
		{
			Binding.SkillHandle = Binding.Runtime->OnSkillStateChanged.AddWeakLambda(this,
				[this](const FKnightSkillRuntimeState& SkillState)
				{
					RefreshPartyVitals();
				});
		}
		PartyBindings.Add(MoveTemp(Binding));
	}
	RefreshPartyVitals();
}

void AKnightSkillLabHUD::RefreshPartyVitals()
{
	FString Text;
	for (const FKnightSkillLabPartyHUDBinding& Binding : PartyBindings)
	{
		if (!Binding.Knight.IsValid() || !Binding.AbilitySystem.IsValid() || !Binding.Life.IsValid())
		{
			continue;
		}
		const FText LifeName = Binding.Life->IsAlive() ? NSLOCTEXT("KnightSkillLab", "PartyAlive", "Alive")
			: (Binding.Life->GetLifeState() == EKnightLifeState::Downed
			? NSLOCTEXT("KnightSkillLab", "PartyDowned", "DOWNED")
			: NSLOCTEXT("KnightSkillLab", "PartyDead", "DEAD"));
		Text += FText::Format(NSLOCTEXT("KnightSkillLab", "PartyVitals", "{0} HP {1}/{2} shield {3} {4}   "),
			GetKnightLabel(Binding.Knight.Get()),
			FText::AsNumber(FMath::RoundToInt(Binding.AbilitySystem->GetNumericAttribute(UKnightAttributeSet::GetHealthAttribute()))),
			FText::AsNumber(FMath::RoundToInt(Binding.AbilitySystem->GetNumericAttribute(UKnightAttributeSet::GetMaxHealthAttribute()))),
			FText::AsNumber(FMath::RoundToInt(Binding.Runtime.IsValid() ? Binding.Runtime->GetSkillState().Shield : 0.0f)),
			LifeName).ToString();
	}
	PartyVitals = FText::FromString(Text);
}

void AKnightSkillLabHUD::UnbindPartyVitals()
{
	for (const FKnightSkillLabPartyHUDBinding& Binding : PartyBindings)
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

void AKnightSkillLabHUD::BindSkillSources()
{
	AKnightRPGUIPlayerController* SkillPlayer = Cast<AKnightRPGUIPlayerController>(GetOwningPlayerController());
	if (SkillPlayer == nullptr || SkillPlayer->GetControlledKnight() == nullptr)
	{
		if (++BindingAttempts < 100)
		{
			GetWorldTimerManager().SetTimer(ReadinessTimer, this, &ThisClass::BindSkillSources, 0.1f, false);
		}
		return;
	}
	Controller = SkillPlayer;
	ControlHandle = SkillPlayer->OnControlledKnightChanged.AddUObject(this, &ThisClass::HandleControlledKnight);
	HandleControlledKnight(nullptr, SkillPlayer->GetControlledKnight());
	for (TActorIterator<AKnightPartyArenaState> It(GetWorld()); It; ++It)
	{
		Arena = *It;
		ArenaHandle = It->OnSnapshotChanged.AddUObject(this, &ThisClass::HandleArenaState);
		HandleArenaState(It->GetSnapshot());
		break;
	}
}

void AKnightSkillLabHUD::HandleControlledKnight(AKnightCharacter* Previous, AKnightCharacter* Current)
{
	if (Runtime.IsValid())
	{
		Runtime->OnSkillStateChanged.Remove(RuntimeHandle);
	}
	Runtime.Reset();
	RuntimeHandle.Reset();
	CachedState = FKnightSkillRuntimeState();
	if (IsValid(Current))
	{
		Runtime = Current->FindComponentByClass<UKnightSkillRuntimeComponent>();
		if (Runtime.IsValid())
		{
			RuntimeHandle = Runtime->OnSkillStateChanged.AddUObject(this, &ThisClass::HandleSkillState);
			HandleSkillState(Runtime->GetSkillState());
		}
	}
}

void AKnightSkillLabHUD::HandleSkillState(const FKnightSkillRuntimeState& InState)
{
	CachedState = InState;
}

void AKnightSkillLabHUD::HandleArenaState(const FKnightPartyArenaSnapshot& Snapshot)
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
	if (Snapshot.Stage == EKnightArenaStage::Boss)
	{
		BindArmorSource();
	}
	RefreshArmor();
}

void AKnightSkillLabHUD::BindArmorSource()
{
	for (TActorIterator<AKnightArenaEnemy> It(GetWorld()); It; ++It)
	{
		UKnightBossArmorComponent* Component = It->FindComponentByClass<UKnightBossArmorComponent>();
		if (Component != nullptr && (Component->GetArmorZone() != nullptr
			|| It->GetArchetypeId().PrimaryAssetName == TEXT("ArenaBoss")))
		{
			Armor = Component;
			ArmorHandle = Component->OnArmorChanged.AddUObject(this, &ThisClass::RefreshArmor);
			BossAbilitySystem = It->GetKnightAbilitySystem();
			for (const FGameplayAttribute& Attribute : {UKnightAttributeSet::GetHealthAttribute(),
				UKnightAttributeSet::GetMaxHealthAttribute()})
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
				[this](const FKnightCombatSnapshot& Snapshot)
				{
					RefreshBossVitals();
				});
			BossProfileHandle = BossCombat->OnProfileChanged.AddWeakLambda(this,
				[this](UKnightCombatProfileDefinition* Profile)
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

void AKnightSkillLabHUD::RefreshBossVitals()
{
	if (BossAbilitySystem.IsValid())
	{
		BossVitals = FText::Format(NSLOCTEXT("KnightSkillLab", "BossVitals", "Boss HP {0}/{1} | Poise {2}/{3}"),
			FText::AsNumber(FMath::RoundToInt(BossAbilitySystem->GetNumericAttribute(UKnightAttributeSet::GetHealthAttribute()))),
			FText::AsNumber(FMath::RoundToInt(BossAbilitySystem->GetNumericAttribute(UKnightAttributeSet::GetMaxHealthAttribute()))),
			FText::AsNumber(FMath::RoundToInt(BossCombat.IsValid() ? BossCombat->GetSnapshot().Poise : 0.0f)),
			FText::AsNumber(FMath::RoundToInt(BossCombat.IsValid() && BossCombat->GetProfile() != nullptr
				? BossCombat->GetProfile()->GetTuning().MaxPoise : 0.0f)));
	}
}

void AKnightSkillLabHUD::UnbindBossVitals()
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

void AKnightSkillLabHUD::RefreshArmor()
{
	if (Armor.IsValid() && Armor->GetArmorZone() != nullptr)
	{
		const FText StateName = Armor->GetArmorState() == EKnightBossArmorState::Intact
			? NSLOCTEXT("KnightSkillLab", "ArmorIntact", "Intact")
			: (Armor->GetArmorState() == EKnightBossArmorState::Brittle
			? NSLOCTEXT("KnightSkillLab", "ArmorBrittle", "Brittle") : NSLOCTEXT("KnightSkillLab", "ArmorBroken", "Broken"));
		ArmorText = FText::Format(NSLOCTEXT("KnightSkillLab", "ArmorHUD", "Boss leg armor: {0}, durability {1}/100"),
			StateName, FText::AsNumber(Armor->GetDurability()));
		ArmorEndServerTime = Armor->GetStateEndServerTime();
	}
	else
	{
		ArmorText = NSLOCTEXT("KnightSkillLab", "ArmorHelp", "Boss: FROST Runestone makes leg zone brittle; aim heavy at lower armor.");
		ArmorEndServerTime = 0.0f;
	}
}

void AKnightSkillLabHUD::DrawHUD()
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
		const FKnightSpecActionDefinition Skill = FKnightSkillCatalog::GetSkill(CachedState.Build, Index + 1);
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
	for (const FKnightSkillLabRunestoneHUDBinding& Binding : RunestoneBindings)
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

void AKnightSkillLabHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
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
	for (const FKnightSkillLabRunestoneHUDBinding& Binding : RunestoneBindings)
	{
		if (Binding.Stone.IsValid())
		{
			Binding.Stone->OnStateChanged.Remove(Binding.Handle);
		}
	}
	RunestoneBindings.Reset();
	if (Controller.IsValid())
	{
		Controller->OnControlledKnightChanged.Remove(ControlHandle);
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
