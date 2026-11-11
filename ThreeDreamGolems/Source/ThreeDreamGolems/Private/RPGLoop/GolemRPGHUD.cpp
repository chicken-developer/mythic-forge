#include "RPGLoop/GolemRPGHUD.h"

#include "Character/GolemCharacter.h"
#include "Inventory/GolemInventoryComponent.h"
#include "Progression/GolemRPGComponent.h"
#include "RPGLoop/GolemRPGGameState.h"
#include "RPGLoop/GolemRPGStateActor.h"
#include "RPGLoop/GolemRPGUIPlayerController.h"

#include "Engine/Canvas.h"
#include "Engine/World.h"
#include "TimerManager.h"

void AGolemRPGHUD::BeginPlay()
{
	Super::BeginPlay();
	BindRPGSources();
}

void AGolemRPGHUD::BindRPGSources()
{
	AGolemRPGGameState* State = GetWorld()->GetGameState<AGolemRPGGameState>();
	AGolemRPGUIPlayerController* Controller = Cast<AGolemRPGUIPlayerController>(GetOwningPlayerController());
	if (State == nullptr || Controller == nullptr)
	{
		if (++BindingAttempts <= 100)
		{
			GetWorldTimerManager().SetTimer(InitialRPGBindingTimer, this, &ThisClass::BindRPGSources, 0.1f, false);
		}
		return;
	}
	GetWorldTimerManager().ClearTimer(InitialRPGBindingTimer);
	RPGGameState = State;
	RPGPlayer = Controller;
	StateReadyHandle = State->OnRPGStateChanged.AddUObject(this, &ThisClass::HandleRPGStateReady);
	ControlHandle = Controller->OnControlledGolemChanged.AddWeakLambda(this,
		[this](AGolemCharacter* Previous, AGolemCharacter* Current)
		{
			RefreshRPGObjective();
		});
	PresentationHandle = Controller->OnRPGPresentationChanged.AddUObject(this, &ThisClass::RefreshRPGObjective);
	HandleRPGStateReady(State->GetRPGState());
}

void AGolemRPGHUD::HandleRPGStateReady(AGolemRPGStateActor* InState)
{
	if (RPGState.Get() != InState)
	{
		if (RPGState.IsValid())
		{
			RPGState->OnLoopChanged.Remove(LoopHandle);
			RPGState->GetRPG()->OnSnapshotChanged.Remove(RPGHandle);
			RPGState->GetInventory()->OnSnapshotChanged.Remove(InventoryHandle);
		}
		LoopHandle.Reset();
		RPGHandle.Reset();
		InventoryHandle.Reset();
		RPGState = InState;
		if (IsValid(InState))
		{
			LoopHandle = InState->OnLoopChanged.AddWeakLambda(this,
				[this](const FGolemRPGLoopSnapshot& Snapshot)
				{
					RefreshRPGObjective();
				});
			RPGHandle = InState->GetRPG()->OnSnapshotChanged.AddUObject(this, &ThisClass::RefreshRPGObjective);
			InventoryHandle = InState->GetInventory()->OnSnapshotChanged.AddUObject(this, &ThisClass::RefreshRPGObjective);
		}
	}
	RefreshRPGObjective();
}

void AGolemRPGHUD::RefreshRPGObjective()
{
	InteractionFeedback = RPGPlayer.IsValid() ? RPGPlayer->GetRPGFeedback() : FText::GetEmpty();
	if (!RPGState.IsValid())
	{
		RPGObjective = NSLOCTEXT("KnightRPG", "HUDWaiting", "Waiting for RPG campaign state.");
		return;
	}
	AGolemCharacter* CurrentGolem = RPGPlayer.IsValid() ? RPGPlayer->GetControlledGolem() : nullptr;
	const FGolemRPGGolemView View = CurrentGolem != nullptr
		? RPGState->GetRPG()->GetGolemView(CurrentGolem->GetGolemEntityId()) : FGolemRPGGolemView();
	if (View.bGearEnabled)
	{
		const FGolemInventoryView Bag = RPGState->GetInventory()->GetView();
		RPGObjective = NSLOCTEXT("KnightGearLab", "GearHUDHelp",
			"I: Character / talents / spellbook / inventory. Return to the cyan sanctuary to change gear or claim rewards.");
		ProgressionFeedback = FText::Format(NSLOCTEXT("KnightGearLab", "GearHUDProgression",
			"Shared bag {0}/{1} | level {2}, equipment {3}/12, Power bonus +{4}"),
			FText::AsNumber(Bag.GetBagCount()), FText::AsNumber(Bag.Capacity), FText::AsNumber(View.Level),
			FText::AsNumber(View.Assignments.Num()), FText::AsNumber(View.StatBonuses.AttackPower));
		return;
	}
	switch (RPGState->GetLoopSnapshot().Stage)
	{
	case EGolemRPGLoopStage::EncounterA:
		RPGObjective = NSLOCTEXT("KnightRPG", "HUDA", "RPG A: defeat wave + boss to earn loot at the cyan station.");
		break;
	case EGolemRPGLoopStage::RewardAvailable:
		RPGObjective = NSLOCTEXT("KnightRPG", "HUDLoot", "Reward ready: cyan station on the west side. P / F picks up nearby loot; I / Menu opens upgrades.");
		break;
	case EGolemRPGLoopStage::Upgrade:
		RPGObjective = NSLOCTEXT("KnightRPG", "HUDUpgrade", "Equip or purchase at the cyan station; host Continue in the panel, then return to START for B.");
		break;
	case EGolemRPGLoopStage::EncounterB:
		RPGObjective = NSLOCTEXT("KnightRPG", "HUDB", "RPG B: use your upgraded hammer and rune. Retry preserves inventory, skills and equipment.");
		break;
	case EGolemRPGLoopStage::Complete:
		RPGObjective = NSLOCTEXT("KnightRPG", "HUDComplete", "RPG complete! Host R / Y replays B. Loot is awarded once per run.");
		break;
	}
	ProgressionFeedback = FText::Format(NSLOCTEXT("KnightRPG", "HUDProgression", "Shared loot stacks {0} | current Golem: {1} skill points, {2} equipment slots, attack +{3}"),
		FText::AsNumber(RPGState->GetInventory()->GetView().Items.Num()), FText::AsNumber(View.SkillPoints),
		FText::AsNumber(View.Assignments.Num()), FText::AsNumber(View.StatBonuses.AttackPower));
}

void AGolemRPGHUD::DrawHUD()
{
	Super::DrawHUD();
	if (Canvas != nullptr)
	{
		DrawText(RPGObjective.ToString(), FLinearColor::Yellow, 25.0f, 265.0f, nullptr, 1.1f);
		DrawText(ProgressionFeedback.ToString(), FLinearColor(0.0f, 0.9f, 1.0f), 25.0f, 292.0f, nullptr, 1.1f);
		DrawText(InteractionFeedback.ToString(), FLinearColor::White, 25.0f, 319.0f, nullptr, 1.1f);
	}
}

FText AGolemRPGHUD::GetDisplayedRPGObjective() const
{
	return RPGObjective;
}

void AGolemRPGHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(InitialRPGBindingTimer);
	if (RPGState.IsValid())
	{
		RPGState->OnLoopChanged.Remove(LoopHandle);
		RPGState->GetRPG()->OnSnapshotChanged.Remove(RPGHandle);
		RPGState->GetInventory()->OnSnapshotChanged.Remove(InventoryHandle);
	}
	if (RPGGameState.IsValid())
	{
		RPGGameState->OnRPGStateChanged.Remove(StateReadyHandle);
	}
	if (RPGPlayer.IsValid())
	{
		RPGPlayer->OnControlledGolemChanged.Remove(ControlHandle);
		RPGPlayer->OnRPGPresentationChanged.Remove(PresentationHandle);
	}
	RPGState.Reset();
	RPGGameState.Reset();
	RPGPlayer.Reset();
	Super::EndPlay(EndPlayReason);
}
