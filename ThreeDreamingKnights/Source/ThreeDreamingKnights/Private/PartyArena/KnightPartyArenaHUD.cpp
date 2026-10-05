#include "PartyArena/KnightPartyArenaHUD.h"

#include "Attributes/KnightAttributeSet.h"
#include "Character/KnightCharacter.h"
#include "Components/KnightAbilitySystemComponent.h"
#include "Components/KnightCombatComponent.h"
#include "Components/KnightLifeComponent.h"
#include "Controllers/KnightArenaCompanionController.h"
#include "Controllers/KnightEnemyController.h"
#include "Framework/KnightGameState.h"
#include "Party/KnightPartyState.h"
#include "PartyArena/KnightArenaEnemy.h"
#include "PartyArena/KnightArenaPlayerController.h"
#include "PartyArena/KnightPartyArenaGameMode.h"

#include "Engine/Canvas.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "TimerManager.h"

void AKnightPartyArenaHUD::BeginPlay()
{
	Super::BeginPlay();
	BindSources();
}

void AKnightPartyArenaHUD::BindSources()
{
	if (Player.IsValid())
	{
		return;
	}
	AKnightArenaPlayerController* Controller = Cast<AKnightArenaPlayerController>(GetOwningPlayerController());
	AKnightGameState* State = GetWorld()->GetGameState<AKnightGameState>();
	AKnightPartyState* NewParty = State != nullptr ? State->GetPartyState() : nullptr;
	AKnightPartyArenaState* NewArena = nullptr;
	for (TActorIterator<AKnightPartyArenaState> Iterator(GetWorld()); Iterator; ++Iterator)
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
			CommandFeedback = NSLOCTEXT("KnightArena", "HUDSourcesMissing", "Arena state failed to initialize.");
		}
		return;
	}
	GetWorldTimerManager().ClearTimer(InitialBindingTimer);
	Player = Controller;
	Party = NewParty;
	Arena = NewArena;
	ControlHandle = Controller->OnControlledKnightChanged.AddUObject(this, &ThisClass::BindControlledKnight);
	CommandHandle = Controller->OnCompanionCommandResult.AddUObject(this, &ThisClass::HandleCommandResult);
	BaseCommandHandle = Controller->OnCommandResult.AddUObject(this, &ThisClass::HandleBaseCommandResult);
	ArenaHandle = NewArena->OnSnapshotChanged.AddUObject(this, &ThisClass::HandleArenaChanged);
	RosterHandle = NewParty->OnRosterChanged.AddUObject(this, &ThisClass::RefreshParty);
	BindControlledKnight(nullptr, Controller->GetControlledKnight());
	RefreshParty();
	HandleArenaChanged(NewArena->GetSnapshot());
}

void AKnightPartyArenaHUD::BindControlledKnight(AKnightCharacter* Previous, AKnightCharacter* Current)
{
	UnbindControlledKnight();
	Knight = Current;
	if (!IsValid(Current))
	{
		DisplayedName = NSLOCTEXT("KnightArena", "WaitingForPawn", "Waiting for your Knight");
		return;
	}
	DisplayedName = Current->GetKnightDisplayName();
	UKnightAbilitySystemComponent* ASC = Current->GetKnightAbilitySystem();
	AbilitySystem = ASC;
	Life = Current->GetLifeComponent();
	Combat = Current->GetCombatComponent();
	const TArray<FGameplayAttribute> Observed{UKnightAttributeSet::GetHealthAttribute(),
		UKnightAttributeSet::GetMaxHealthAttribute(), UKnightAttributeSet::GetStaminaAttribute(),
		UKnightAttributeSet::GetRuneEnergyAttribute()};
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

void AKnightPartyArenaHUD::UnbindControlledKnight()
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
	Knight.Reset();
	AbilitySystem.Reset();
	Life.Reset();
	Combat.Reset();
	Health = 0.0f;
	MaxHealth = 0.0f;
	Stamina = 0.0f;
	RuneEnergy = 0.0f;
}

void AKnightPartyArenaHUD::RefreshParty()
{
	if (!Party.IsValid())
	{
		return;
	}
	PartyFeedback = FText::Format(NSLOCTEXT("KnightArena", "PartyOwners", "Humans {0}/3; AI {1}/3"),
		FText::AsNumber(Party->GetHumanCount()), FText::AsNumber(3 - Party->GetHumanCount()));
	UnbindAIDebug();
	for (const FKnightRosterEntry& Entry : Party->GetRoster())
	{
		if (AKnightArenaCompanionController* Companion = Cast<AKnightArenaCompanionController>(Entry.Knight->GetController()))
		{
			FKnightArenaDebugBinding Binding;
			Binding.Companion = Companion;
			Binding.Handle = Companion->OnCommandChanged.AddWeakLambda(this,
				[this](const FKnightCompanionCommandSnapshot& Snapshot)
				{
					RefreshAIDebug();
				});
			DebugBindings.Add(Binding);
		}
	}
	RefreshAIDebug();
}

void AKnightPartyArenaHUD::RefreshAIDebug()
{
	AIDebugFeedback.Reset();
	for (const FKnightArenaDebugBinding& Binding : DebugBindings)
	{
		if (Binding.Companion.IsValid())
		{
			const FKnightCompanionCommandSnapshot& Snapshot = Binding.Companion->GetCommandSnapshot();
			AIDebugFeedback += FString::Printf(TEXT("AI slot %s cmd%d LOD%d tick%lld  "),
				*Snapshot.KnightId.Value.ToString().Left(6), static_cast<int32>(Snapshot.Command),
				static_cast<int32>(Snapshot.DecisionLOD), Snapshot.DecisionTickCount);
		}
		if (Binding.Enemy.IsValid())
		{
			AIDebugFeedback += FString::Printf(TEXT("Enemy LOD%d tick%lld  "),
				static_cast<int32>(Binding.Enemy->GetDecisionLOD()), Binding.Enemy->GetDecisionTickCount());
		}
	}
}

void AKnightPartyArenaHUD::UnbindAIDebug()
{
	for (const FKnightArenaDebugBinding& Binding : DebugBindings)
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

void AKnightPartyArenaHUD::HandleArenaChanged(const FKnightPartyArenaSnapshot& Snapshot)
{
	const bool bStageChanged = DisplayedArena.Stage != Snapshot.Stage;
	DisplayedArena = Snapshot;
	if (bStageChanged)
	{
		RefreshParty();
		if (AKnightPartyArenaGameMode* Mode = GetWorld()->GetAuthGameMode<AKnightPartyArenaGameMode>())
		{
			for (AKnightArenaEnemy* Enemy : Mode->GetEncounterEnemies())
			{
				if (IsValid(Enemy))
				{
					if (AKnightEnemyController* Controller = Cast<AKnightEnemyController>(Enemy->GetController()))
					{
						FKnightArenaDebugBinding Binding;
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

void AKnightPartyArenaHUD::HandleCommandResult(const FKnightResult& Result)
{
	if (Result.IsSuccess() && Player.IsValid())
	{
		const UEnum* Commands = StaticEnum<EKnightCompanionCommand>();
		CommandFeedback = FText::Format(NSLOCTEXT("KnightArena", "CommandIssued", "Companions: {0}"),
			Commands->GetDisplayNameTextByValue(static_cast<int64>(Player->GetLastCompanionCommand())));
	}
	else
	{
		CommandFeedback = Result.Message;
	}
}

void AKnightPartyArenaHUD::HandleBaseCommandResult(const FKnightResult& Result)
{
	if (!Result.IsSuccess())
	{
		CommandFeedback = Result.Message;
	}
}

void AKnightPartyArenaHUD::HandleAttributeChanged(const FOnAttributeChangeData& Change)
{
	if (AbilitySystem.IsValid())
	{
		Health = AbilitySystem->GetNumericAttribute(UKnightAttributeSet::GetHealthAttribute());
		MaxHealth = AbilitySystem->GetNumericAttribute(UKnightAttributeSet::GetMaxHealthAttribute());
		Stamina = AbilitySystem->GetNumericAttribute(UKnightAttributeSet::GetStaminaAttribute());
		RuneEnergy = AbilitySystem->GetNumericAttribute(UKnightAttributeSet::GetRuneEnergyAttribute());
	}
}

void AKnightPartyArenaHUD::HandleLifeChanged(EKnightLifeState Previous, EKnightLifeState Current)
{
	LifeFeedback = Current == EKnightLifeState::Alive ? NSLOCTEXT("KnightArena", "Alive", "Alive")
		: Current == EKnightLifeState::Downed ? NSLOCTEXT("KnightArena", "Downed", "DOWNED: companion can revive you")
		: NSLOCTEXT("KnightArena", "Dead", "DEAD: host R / Y to retry");
}

void AKnightPartyArenaHUD::HandleCombatChanged(const FKnightCombatSnapshot& Snapshot)
{
	bAttacking = Snapshot.bAttacking;
	bBlocking = Snapshot.bBlocking;
}

void AKnightPartyArenaHUD::DrawHUD()
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
	DrawLine(NSLOCTEXT("KnightArena", "ArenaTitle", "PARTY ARENA: wave > two-phase boss"), FLinearColor::Yellow);
	DrawLine(FText::Format(NSLOCTEXT("KnightArena", "Resources", "{0} | Health {1}/{2} | Stamina {3} | Rune {4}"),
		DisplayedName, FText::AsNumber(FMath::RoundToInt(Health)), FText::AsNumber(FMath::RoundToInt(MaxHealth)),
		FText::AsNumber(FMath::RoundToInt(Stamina)), FText::AsNumber(FMath::RoundToInt(RuneEnergy))));
	DrawLine(FText::Format(NSLOCTEXT("KnightArena", "PartyStatus", "{0} | alive {1}, downed {2} | {3}"),
		PartyFeedback, FText::AsNumber(DisplayedArena.AliveKnights), FText::AsNumber(DisplayedArena.DownedKnights),
		LifeFeedback));
	FText Objective;
	switch (DisplayedArena.Stage)
	{
	case EKnightArenaStage::AwaitingStart:
		Objective = NSLOCTEXT("KnightArena", "AwaitingStart", "Move to yellow START: F / D-pad Left to begin.");
		break;
	case EKnightArenaStage::Wave:
		Objective = FText::Format(NSLOCTEXT("KnightArena", "Wave", "WAVE: defeat the enemies ({0} remaining)."),
			FText::AsNumber(DisplayedArena.AliveEnemies));
		break;
	case EKnightArenaStage::Boss:
		Objective = FText::Format(NSLOCTEXT("KnightArena", "Boss", "BOSS: phase {0}/2; defeat the boss."),
			FText::AsNumber(DisplayedArena.BossPhase + 1));
		break;
	case EKnightArenaStage::Victory:
		Objective = NSLOCTEXT("KnightArena", "Victory", "VICTORY! R / Y: host retry. All three Knights retain their identities.");
		break;
	case EKnightArenaStage::Defeat:
		Objective = DisplayedArena.Failure == EKnightArenaFailure::EncounterUnavailable
			? NSLOCTEXT("KnightArena", "EncounterUnavailable", "Encounter interrupted: an enemy or required asset became unavailable. R / Y: host retry.")
			: NSLOCTEXT("KnightArena", "Defeat", "PARTY DEFEATED: no living Knight can revive the party. R / Y: host retry.");
		break;
	}
	DrawLine(Objective, IsDisplayingArenaDefeat() ? FLinearColor::Red : FLinearColor::Green);
	DrawLine(NSLOCTEXT("KnightArena", "CombatKeys", "WASD move | LMB light / RMB heavy | Q block / C parry / Shift dodge | MMB lock | G rune / H finisher"));
	DrawLine(NSLOCTEXT("KnightArena", "PartyKeys", "N follow / Z hold / B focus locked enemy / K regroup | gamepad View cycles | E / X hold revive | Tab / RB switch"));
	DrawLine(NSLOCTEXT("KnightArena", "ResourceHint", "Arena recovery: +8 stamina / +6 rune each second; health requires revive/checkpoint."));
	DrawLine(CommandFeedback, FLinearColor(0.0f, 1.0f, 1.0f));
	DrawLine(FText::FromString(AIDebugFeedback), FLinearColor(0.6f, 0.8f, 1.0f));
}

const FKnightPartyArenaSnapshot& AKnightPartyArenaHUD::GetDisplayedArenaSnapshot() const
{
	return DisplayedArena;
}

bool AKnightPartyArenaHUD::IsDisplayingArenaVictory() const
{
	return DisplayedArena.Stage == EKnightArenaStage::Victory;
}

bool AKnightPartyArenaHUD::IsDisplayingArenaDefeat() const
{
	return DisplayedArena.Stage == EKnightArenaStage::Defeat;
}

float AKnightPartyArenaHUD::GetDisplayedHealth() const
{
	return Health;
}

FText AKnightPartyArenaHUD::GetDisplayedCommandFeedback() const
{
	return CommandFeedback;
}

void AKnightPartyArenaHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(InitialBindingTimer);
	UnbindControlledKnight();
	UnbindAIDebug();
	if (Player.IsValid())
	{
		Player->OnControlledKnightChanged.Remove(ControlHandle);
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
