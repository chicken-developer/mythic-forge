#include "PartyArena/GolemPartyArenaHUD.h"

#include "Attributes/GolemAttributeSet.h"
#include "Character/GolemCharacter.h"
#include "Components/GolemAbilitySystemComponent.h"
#include "Components/GolemCombatComponent.h"
#include "Components/GolemLifeComponent.h"
#include "Controllers/GolemArenaCompanionController.h"
#include "Controllers/GolemEnemyController.h"
#include "Framework/GolemGameState.h"
#include "Party/GolemPartyState.h"
#include "PartyArena/GolemArenaEnemy.h"
#include "PartyArena/GolemArenaPlayerController.h"
#include "PartyArena/GolemPartyArenaGameMode.h"

#include "Engine/Canvas.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "TimerManager.h"

void AGolemPartyArenaHUD::BeginPlay()
{
	Super::BeginPlay();
	BindSources();
}

void AGolemPartyArenaHUD::BindSources()
{
	if (Player.IsValid())
	{
		return;
	}
	AGolemArenaPlayerController* Controller = Cast<AGolemArenaPlayerController>(GetOwningPlayerController());
	AGolemGameState* State = GetWorld()->GetGameState<AGolemGameState>();
	AGolemPartyState* NewParty = State != nullptr ? State->GetPartyState() : nullptr;
	AGolemPartyArenaState* NewArena = nullptr;
	for (TActorIterator<AGolemPartyArenaState> Iterator(GetWorld()); Iterator; ++Iterator)
	{
		NewArena = *Iterator;
		break;
	}
	if (Controller == nullptr || NewParty == nullptr || !NewParty->IsCanonicalRosterReady() || NewArena == nullptr)
	{
		// Bounded initial replication readiness. Once bound, presentation never polls actors on Tick.
		if (++BindingAttempts <= 100)
		{
			GetWorldTimerManager().SetTimer(InitialBindingTimer, this, &ThisClass::BindSources, 0.1f, false);
		}
		else
		{
			CommandFeedback = NSLOCTEXT("GolemArena", "HUDSourcesMissing", "Arena state failed to initialize.");
		}
		return;
	}
	GetWorldTimerManager().ClearTimer(InitialBindingTimer);
	Player = Controller;
	Party = NewParty;
	Arena = NewArena;
	ControlHandle = Controller->OnControlledGolemChanged.AddUObject(this, &ThisClass::BindControlledGolem);
	CommandHandle = Controller->OnCompanionCommandResult.AddUObject(this, &ThisClass::HandleCommandResult);
	BaseCommandHandle = Controller->OnCommandResult.AddUObject(this, &ThisClass::HandleBaseCommandResult);
	ArenaHandle = NewArena->OnSnapshotChanged.AddUObject(this, &ThisClass::HandleArenaChanged);
	RosterHandle = NewParty->OnRosterChanged.AddUObject(this, &ThisClass::RefreshParty);
	BindControlledGolem(nullptr, Controller->GetControlledGolem());
	RefreshParty();
	HandleArenaChanged(NewArena->GetSnapshot());
}

void AGolemPartyArenaHUD::BindControlledGolem(AGolemCharacter* Previous, AGolemCharacter* Current)
{
	UnbindControlledGolem();
	Golem = Current;
	if (!IsValid(Current))
	{
		DisplayedName = NSLOCTEXT("GolemArena", "WaitingForPawn", "Waiting for your Golem");
		return;
	}
	DisplayedName = GetGolemLabel(Current);
	UGolemAbilitySystemComponent* ASC = Current->GetGolemAbilitySystem();
	AbilitySystem = ASC;
	Life = Current->GetLifeComponent();
	Combat = Current->GetCombatComponent();
	const TArray<FGameplayAttribute> Observed{UGolemAttributeSet::GetHealthAttribute(),
		UGolemAttributeSet::GetMaxHealthAttribute()};
	for (const FGameplayAttribute& Attribute : Observed)
	{
		const FDelegateHandle Handle = ASC->GetGameplayAttributeValueChangeDelegate(Attribute).AddUObject(
			this, &ThisClass::HandleAttributeChanged);
		AttributeHandles.Emplace(Attribute, Handle);
	}
	LifeHandle = Life->OnLifeStateChanged.AddUObject(this, &ThisClass::HandleLifeChanged);
	CombatHandle = Combat->OnCombatSnapshotChanged.AddUObject(this, &ThisClass::HandleCombatChanged);
	FOnAttributeChangeData Initial;
	HandleAttributeChanged(Initial);
	HandleLifeChanged(Life->GetLifeState(), Life->GetLifeState());
	HandleCombatChanged(Combat->GetSnapshot());
}

void AGolemPartyArenaHUD::UnbindControlledGolem()
{
	if (AbilitySystem.IsValid())
	{
		for (const TPair<FGameplayAttribute, FDelegateHandle>& Registration : AttributeHandles)
		{
			AbilitySystem->GetGameplayAttributeValueChangeDelegate(Registration.Key).Remove(Registration.Value);
		}
	}
	AttributeHandles.Reset();
	if (Life.IsValid())
	{
		Life->OnLifeStateChanged.Remove(LifeHandle);
	}
	if (Combat.IsValid())
	{
		Combat->OnCombatSnapshotChanged.Remove(CombatHandle);
	}
	LifeHandle.Reset();
	CombatHandle.Reset();
	Golem.Reset();
	AbilitySystem.Reset();
	Life.Reset();
	Combat.Reset();
	Health = 0.0f;
	MaxHealth = 0.0f;
}

void AGolemPartyArenaHUD::RefreshParty()
{
	if (!Party.IsValid())
	{
		return;
	}
	PartyFeedback = FText::Format(NSLOCTEXT("GolemArena", "PartyOwners", "Humans {0}/3; AI {1}/3"),
		FText::AsNumber(Party->GetHumanCount()), FText::AsNumber(3 - Party->GetHumanCount()));
	UnbindAIDebug();
	for (const FGolemRosterEntry& Entry : Party->GetRoster())
	{
		if (AGolemArenaCompanionController* Companion = Cast<AGolemArenaCompanionController>(Entry.Golem->GetController()))
		{
			FGolemArenaDebugBinding Binding;
			Binding.Companion = Companion;
			Binding.Handle = Companion->OnCommandChanged.AddWeakLambda(this,
				[this](const FGolemCompanionCommandSnapshot& Snapshot)
				{
					RefreshAIDebug();
				});
			DebugBindings.Add(Binding);
		}
	}
	RefreshAIDebug();
}

void AGolemPartyArenaHUD::RefreshAIDebug()
{
	AIDebugFeedback.Reset();
	for (const FGolemArenaDebugBinding& Binding : DebugBindings)
	{
		if (Binding.Companion.IsValid())
		{
			const FGolemCompanionCommandSnapshot& Snapshot = Binding.Companion->GetCommandSnapshot();
			AIDebugFeedback += FString::Printf(TEXT("AI slot %s cmd%d LOD%d tick%lld  "),
				*Snapshot.GolemId.Value.ToString().Left(6), static_cast<int32>(Snapshot.Command),
				static_cast<int32>(Snapshot.DecisionLOD), Snapshot.DecisionTickCount);
		}
		if (Binding.Enemy.IsValid())
		{
			AIDebugFeedback += FString::Printf(TEXT("Enemy LOD%d tick%lld  "),
				static_cast<int32>(Binding.Enemy->GetDecisionLOD()), Binding.Enemy->GetDecisionTickCount());
		}
	}
}

void AGolemPartyArenaHUD::UnbindAIDebug()
{
	for (const FGolemArenaDebugBinding& Binding : DebugBindings)
	{
		if (Binding.Companion.IsValid())
		{
			Binding.Companion->OnCommandChanged.Remove(Binding.Handle);
		}
		if (Binding.Enemy.IsValid())
		{
			Binding.Enemy->OnDecisionChanged.Remove(Binding.Handle);
		}
	}
	DebugBindings.Reset();
}

void AGolemPartyArenaHUD::HandleArenaChanged(const FGolemPartyArenaSnapshot& Snapshot)
{
	const bool bStageChanged = DisplayedArena.Stage != Snapshot.Stage;
	DisplayedArena = Snapshot;
	if (bStageChanged)
	{
		RefreshParty();
		if (AGolemPartyArenaGameMode* Mode = GetWorld()->GetAuthGameMode<AGolemPartyArenaGameMode>())
		{
			for (AGolemArenaEnemy* Enemy : Mode->GetEncounterEnemies())
			{
				if (IsValid(Enemy))
				{
					if (AGolemEnemyController* Controller = Cast<AGolemEnemyController>(Enemy->GetController()))
					{
						FGolemArenaDebugBinding Binding;
						Binding.Enemy = Controller;
						Binding.Handle = Controller->OnDecisionChanged.AddUObject(this, &ThisClass::RefreshAIDebug);
						DebugBindings.Add(Binding);
					}
				}
			}
		}
		RefreshAIDebug();
	}
}

void AGolemPartyArenaHUD::HandleCommandResult(const FGolemResult& Result)
{
	if (Result.IsSuccess() && Player.IsValid())
	{
		const UEnum* Commands = StaticEnum<EGolemCompanionCommand>();
		CommandFeedback = FText::Format(NSLOCTEXT("GolemArena", "CommandIssued", "Companions: {0}"),
			Commands->GetDisplayNameTextByValue(static_cast<int64>(Player->GetLastCompanionCommand())));
	}
	else
	{
		CommandFeedback = Result.Message;
	}
}

void AGolemPartyArenaHUD::HandleBaseCommandResult(const FGolemResult& Result)
{
	if (!Result.IsSuccess())
	{
		CommandFeedback = Result.Message;
	}
}

void AGolemPartyArenaHUD::HandleAttributeChanged(const FOnAttributeChangeData& Change)
{
	if (AbilitySystem.IsValid())
	{
		Health = AbilitySystem->GetNumericAttribute(UGolemAttributeSet::GetHealthAttribute());
		MaxHealth = AbilitySystem->GetNumericAttribute(UGolemAttributeSet::GetMaxHealthAttribute());
	}
}

void AGolemPartyArenaHUD::HandleLifeChanged(EGolemLifeState Previous, EGolemLifeState Current)
{
	LifeFeedback = Current == EGolemLifeState::Alive ? NSLOCTEXT("GolemArena", "Alive", "Alive")
		: Current == EGolemLifeState::Downed ? NSLOCTEXT("GolemArena", "Downed", "DOWNED: companion can revive you")
		: NSLOCTEXT("GolemArena", "Dead", "DEAD: host R / Y to retry");
}

void AGolemPartyArenaHUD::HandleCombatChanged(const FGolemCombatSnapshot& Snapshot)
{
	bAttacking = Snapshot.bAttacking;
	bBlocking = Snapshot.bBlocking;
}

void AGolemPartyArenaHUD::DrawHUD()
{
	Super::DrawHUD();
	if (Canvas == nullptr)
	{
		return;
	}
	float Y = 20.0f;
	const auto DrawLine = [this, &Y](const FText& Text, const FLinearColor Color = FLinearColor::White)
	{
		DrawText(Text.ToString(), Color, 25.0f, Y, nullptr, 1.15f);
		Y += 25.0f;
	};
	DrawLine(GetArenaTitle(), FLinearColor::Yellow);
	DrawLine(FText::Format(NSLOCTEXT("GolemArena", "Resources", "{0} | Health {1}/{2}"),
		DisplayedName, FText::AsNumber(FMath::RoundToInt(Health)), FText::AsNumber(FMath::RoundToInt(MaxHealth))));
	DrawLine(FText::Format(NSLOCTEXT("GolemArena", "PartyStatus", "{0} | alive {1}, downed {2} | {3}"),
		PartyFeedback, FText::AsNumber(DisplayedArena.AliveGolems), FText::AsNumber(DisplayedArena.DownedGolems),
		LifeFeedback));
	FText Objective;
	switch (DisplayedArena.Stage)
	{
	case EGolemArenaStage::AwaitingStart:
		Objective = NSLOCTEXT("GolemArena", "AwaitingStart", "Move to yellow START: F / D-pad Left to begin.");
		break;
	case EGolemArenaStage::Wave:
		Objective = FText::Format(NSLOCTEXT("GolemArena", "Wave", "WAVE: defeat the enemies ({0} remaining)."),
			FText::AsNumber(DisplayedArena.AliveEnemies));
		break;
	case EGolemArenaStage::Boss:
		Objective = FText::Format(NSLOCTEXT("GolemArena", "Boss", "BOSS: phase {0}/2; defeat the boss."),
			FText::AsNumber(DisplayedArena.BossPhase + 1));
		break;
	case EGolemArenaStage::Victory:
		Objective = NSLOCTEXT("GolemArena", "Victory", "VICTORY! R / Y: host retry. All three Golems retain their identities.");
		break;
	case EGolemArenaStage::Defeat:
		Objective = DisplayedArena.Failure == EGolemArenaFailure::EncounterUnavailable
			? NSLOCTEXT("GolemArena", "EncounterUnavailable", "Encounter interrupted: an enemy or required asset became unavailable. R / Y: host retry.")
			: NSLOCTEXT("GolemArena", "Defeat", "PARTY DEFEATED: no living Golem can revive the party. R / Y: host retry.");
		break;
	}
	DrawLine(Objective, IsDisplayingArenaDefeat() ? FLinearColor::Red : FLinearColor::Green);
	DrawLine(GetCombatHelp());
	DrawLine(GetPartyHelp());
	DrawLine(GetRecoveryHelp());
	DrawLine(CommandFeedback, FLinearColor(0.0f, 1.0f, 1.0f));
	DrawLine(FText::FromString(AIDebugFeedback), FLinearColor(0.6f, 0.8f, 1.0f));
}

const FGolemPartyArenaSnapshot& AGolemPartyArenaHUD::GetDisplayedArenaSnapshot() const
{
	return DisplayedArena;
}

FText AGolemPartyArenaHUD::GetArenaTitle() const
{
	return NSLOCTEXT("GolemArena", "ArenaTitle", "PARTY ARENA: wave > two-phase boss");
}

FText AGolemPartyArenaHUD::GetCombatHelp() const
{
	return NSLOCTEXT("GolemArena", "CombatKeys", "WASD move | LMB light / RMB heavy | Q block / C parry / Shift dodge | MMB lock | G rune / H finisher");
}

FText AGolemPartyArenaHUD::GetPartyHelp() const
{
	return NSLOCTEXT("GolemArena", "PartyKeys", "N follow / Z hold / B focus locked enemy / K regroup | gamepad View cycles | E / X hold revive | Tab / RB switch");
}

FText AGolemPartyArenaHUD::GetRecoveryHelp() const
{
	return NSLOCTEXT("GolemArena", "ResourceHint", "Arena recovery: +8 stamina / +6 rune each second; health requires revive/checkpoint.");
}

FText AGolemPartyArenaHUD::GetGolemLabel(const AGolemCharacter* InGolem) const
{
	return InGolem->GetGolemDisplayName();
}

bool AGolemPartyArenaHUD::IsDisplayingArenaVictory() const
{
	return DisplayedArena.Stage == EGolemArenaStage::Victory;
}

bool AGolemPartyArenaHUD::IsDisplayingArenaDefeat() const
{
	return DisplayedArena.Stage == EGolemArenaStage::Defeat;
}

float AGolemPartyArenaHUD::GetDisplayedHealth() const
{
	return Health;
}

FText AGolemPartyArenaHUD::GetDisplayedCommandFeedback() const
{
	return CommandFeedback;
}

void AGolemPartyArenaHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(InitialBindingTimer);
	UnbindControlledGolem();
	UnbindAIDebug();
	if (Player.IsValid())
	{
		Player->OnControlledGolemChanged.Remove(ControlHandle);
		Player->OnCompanionCommandResult.Remove(CommandHandle);
		Player->OnCommandResult.Remove(BaseCommandHandle);
	}
	if (Party.IsValid())
	{
		Party->OnRosterChanged.Remove(RosterHandle);
	}
	if (Arena.IsValid())
	{
		Arena->OnSnapshotChanged.Remove(ArenaHandle);
	}
	Super::EndPlay(EndPlayReason);
}
