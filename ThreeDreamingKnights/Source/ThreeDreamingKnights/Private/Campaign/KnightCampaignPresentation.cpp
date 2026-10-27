#include "Campaign/KnightCampaignPlayerController.h"

#include "Attributes/KnightAttributeSet.h"
#include "Character/KnightCharacter.h"
#include "Checkpoint/KnightCheckpointSubsystem.h"
#include "Components/KnightAbilitySystemComponent.h"
#include "Components/KnightCombatComponent.h"
#include "Components/KnightLifeComponent.h"
#include "Definitions/KnightCampaignDefinition.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Interaction/KnightStoryAnchor.h"
#include "Inventory/KnightInventoryComponent.h"
#include "Party/KnightPartyState.h"
#include "Progression/KnightRPGComponent.h"
#include "RPGLoop/KnightRPGStateActor.h"
#include "Settings/KnightGameUserSettings.h"
#include "Skills/KnightSkillRuntimeComponent.h"
#include "Story/KnightStoryState.h"
#include "Widgets/KnightRPGPanel.h"

namespace
{
	FText QuestPhaseLabel(const EKnightQuestPhase Phase)
	{
		switch (Phase)
		{
		case EKnightQuestPhase::Locked: return NSLOCTEXT("KnightJournal", "Locked", "Locked — complete prerequisites");
		case EKnightQuestPhase::Available: return NSLOCTEXT("KnightJournal", "Available", "Available — accept to start");
		case EKnightQuestPhase::Active: return NSLOCTEXT("KnightJournal", "Active", "In progress");
		case EKnightQuestPhase::RewardPending: return NSLOCTEXT("KnightJournal", "Pending", "Reward ready — claim at the reward marker");
		case EKnightQuestPhase::Complete: return NSLOCTEXT("KnightJournal", "Complete", "Complete");
		}
		return FText();
	}

	FText MarkerSymbol(const EKnightMapMarkerKind Kind)
	{
		switch (Kind)
		{
		case EKnightMapMarkerKind::Quest: return NSLOCTEXT("KnightMap", "QuestSymbol", "Q");
		case EKnightMapMarkerKind::Checkpoint: return NSLOCTEXT("KnightMap", "CheckpointSymbol", "C");
		case EKnightMapMarkerKind::Travel: return NSLOCTEXT("KnightMap", "TravelSymbol", "T");
		case EKnightMapMarkerKind::Hazard: return NSLOCTEXT("KnightMap", "HazardSymbol", "!");
		default: return NSLOCTEXT("KnightMap", "LandmarkSymbol", "L");
		}
	}
}

FString AKnightCampaignPlayerController::CurrentSaveStamp() const
{
	const AKnightStoryState* State = GetWorld()->GetGameState<AKnightStoryState>();
	if (State == nullptr || State->GetRPGState() == nullptr)
	{
		return FString();
	}
	return FString::Printf(TEXT("%s:%lld:%lld:%lld:%lld"), *State->Quest->GetSnapshot().RunId.ToString(),
		State->Quest->GetSnapshot().Revision,
		State->WorldState->GetSnapshot().FactRevision,
		State->GetRPGState()->GetInventory()->GetView().Revision,
		State->GetRPGState()->GetRPG()->GetView().Revision);
}

void AKnightCampaignPlayerController::ReadCheckpointSummary(const bool bJustSaved)
{
	AKnightStoryState* State = GetWorld()->GetGameState<AKnightStoryState>();
	if (State == nullptr || CampaignWorld == nullptr || GetNetMode() == NM_Client)
	{
		return;
	}
	const bool bInitialRead = !bReadCheckpoint;
	bReadCheckpoint = true;
	const FKnightQuestSnapshot& Quest = State->Quest->GetSnapshot();
	FKnightCampaignCheckpoint Loaded;
	const FKnightResult Result = GetGameInstance()->GetSubsystem<UKnightCheckpointSubsystem>()->LoadCheckpoint(
		Quest.CampaignId, Quest.ScenarioKind, Loaded);
	bHasDiskCheckpoint = Result.IsSuccess();
	if (!bHasDiskCheckpoint)
	{
		CheckpointDescription = NSLOCTEXT("KnightCampaignUI", "NoCheckpoint", "No readable checkpoint. Start a new campaign.");
		return;
	}
	ContinueRegion = CampaignWorld->MapRegions.IsEmpty() ? NAME_None : CampaignWorld->MapRegions[0].RegionId;
	for (const FKnightMapRegion& Region : CampaignWorld->MapRegions)
	{
		if (Region.Map.ToSoftObjectPath().GetLongPackageName() == Loaded.MapPackage)
		{
			ContinueRegion = Region.RegionId;
		}
	}
	CheckpointDescription = FText::Format(NSLOCTEXT("KnightCampaignUI", "CheckpointSummary",
		"Checkpoint: {0} | {1} items | {2} claimed rewards. Continue resumes this safe boundary."),
		CampaignWorld->FindRegion(ContinueRegion) != nullptr ? CampaignWorld->FindRegion(ContinueRegion)->Label : FText(),
		FText::AsNumber(Loaded.RPG.Inventory.Items.Num()),
		FText::AsNumber(Loaded.RPG.View.ClaimedRewards.Num()));
	SavedStamp = FString::Printf(TEXT("%s:%lld:%lld:%lld:%lld"), *Loaded.Quest.RunId.ToString(),
		Loaded.Quest.Revision, Loaded.World.FactRevision, Loaded.RPG.Inventory.Revision, Loaded.RPG.View.Revision);
	if (bJustSaved || bInitialRead && Loaded.Quest.RunId == Quest.RunId
		&& Loaded.World.FactRevision == State->WorldState->GetSnapshot().FactRevision
		&& Loaded.RPG.Inventory.Revision == State->GetRPGState()->GetInventory()->GetView().Revision)
	{
		// Restore can rebuild metadata revisions. Establish the disk baseline once sources are ready.
		SavedStamp = CurrentSaveStamp();
	}
}

void AKnightCampaignPlayerController::RefreshCampaignPresentation()
{
	if (bEndingCampaign || CampaignWorld == nullptr || CampaignDefinition == nullptr)
	{
		return;
	}
	AKnightStoryState* State = GetWorld()->GetGameState<AKnightStoryState>();
	if (State == nullptr || State->GetRPGState() == nullptr)
	{
		return;
	}
	Presentation.bEnabled = true;
	Presentation.bHost = GetNetMode() != NM_Client;
	Presentation.bFrontEnd = bFrontEnd;
	Presentation.bPending = CampaignRequest.IsValid();
	Presentation.bHasCheckpoint = bHasDiskCheckpoint;
	Presentation.bHasDifficultyControls = CampaignWorld->EncounterQualityDefinitionId.IsValid();
	FName DifficultyId(TEXT("Standard"));
	for (const FKnightWorldFact& Fact : State->WorldState->GetSnapshot().Facts)
	{
		if (Fact.EventName == TEXT("DifficultyChosen") && Fact.SubjectId == CampaignWorld->EncounterQualityDefinitionId.PrimaryAssetName)
		{
			DifficultyId = Fact.ValueId;
		}
	}
	Presentation.Difficulty = DifficultyId == TEXT("Story") ? NSLOCTEXT("KnightCampaignUI", "StoryDifficulty", "Story — one simultaneous hostile attack")
		: NSLOCTEXT("KnightCampaignUI", "StandardDifficulty", "Standard — two simultaneous hostile attacks, at most one per Knight");
	Presentation.Confirmation = Confirmation;
	Presentation.SessionStatus = GetNetMode() == NM_Standalone
		? (GetWorld()->IsPaused() ? NSLOCTEXT("KnightCampaignUI", "Paused", "Solo — simulation paused")
			: NSLOCTEXT("KnightCampaignUI", "Solo", "Solo — simulation running"))
		: NSLOCTEXT("KnightCampaignUI", "CoopRunning", "Co-op — menu does not pause the world; host owns campaign saves and travel.");
	Presentation.SaveStatus = Presentation.bPending ? NSLOCTEXT("KnightCampaignUI", "SavePending", "Pending request...")
		: !Presentation.bHost ? NSLOCTEXT("KnightCampaignUI", "HostSave", "Campaign progress is saved by the host.")
		: !bHasDiskCheckpoint ? NSLOCTEXT("KnightCampaignUI", "NoDurableSave", "No checkpoint yet — save at a safe boundary to keep your progress.")
		: bHasDiskCheckpoint && SavedStamp == CurrentSaveStamp()
			? NSLOCTEXT("KnightCampaignUI", "Clean", "Saved checkpoint — no new persistent changes")
			: NSLOCTEXT("KnightCampaignUI", "Dirty", "Unsaved progress — quit resumes the last durable checkpoint.");
	Presentation.Quests.Reset();
	Presentation.ActiveObjective = FText();
	UKnightGameUserSettings* Settings = UKnightGameUserSettings::Get();
	const FText InteractKey = Settings != nullptr ? Settings->GetActionKey(TEXT("Interact")).GetDisplayName()
		: NSLOCTEXT("KnightCampaignUI", "DefaultInteract", "F");
	for (const FKnightQuestProgress& Progress : State->Quest->GetSnapshot().Quests)
	{
		const FKnightQuestDefinition* Definition = CampaignDefinition->FindQuest(Progress.QuestId);
		if (Definition == nullptr)
		{
			continue;
		}
		FKnightCampaignJournalRow& Row = Presentation.Quests.AddDefaulted_GetRef();
		Row.QuestId = Progress.QuestId;
		Row.Title = Definition->Title;
		Row.bCanActivate = Progress.Phase == EKnightQuestPhase::Available;
		FString Detail = QuestPhaseLabel(Progress.Phase).ToString();
		for (const FKnightObjectiveDefinition& Objective : Definition->Objectives)
		{
			const int32 Count = Progress.Counts.FindRef(Objective.ObjectiveId);
			const FString Label = Objective.Label.ToString().Replace(TEXT("[F]"),
				*FText::Format(NSLOCTEXT("KnightCampaignUI", "KeyHint", "[{0}]"), InteractKey).ToString());
			Detail += FString::Printf(TEXT("\n%s  %d/%d"), *Label, Count, Objective.RequiredCount);
			if (Presentation.ActiveObjective.IsEmpty() && Progress.Phase == EKnightQuestPhase::Active
				&& Count < Objective.RequiredCount)
			{
				Presentation.ActiveObjective = FText::Format(NSLOCTEXT("KnightJournal", "TrackedObjective", "{0}: {1}"),
					Definition->Title, FText::FromString(Label));
			}
		}
		Row.Progress = FText::FromString(Detail);
		if (Progress.Phase == EKnightQuestPhase::RewardPending)
		{
			Presentation.ActiveObjective = FText::Format(NSLOCTEXT("KnightJournal", "RewardReady", "{0}: reward ready — visit CLAIM."), Definition->Title);
		}
	}
	Presentation.Subtitle = State->Dialogue->GetSnapshot().bActive ? State->Dialogue->GetSnapshot().Text : FText();
	Presentation.Tutorial = BuildTutorial();
	Presentation.TextScale = Settings != nullptr ? Settings->TextScale : 1.0f;
	if (!State->WorldState->GetActiveRegionId().IsNone()
		&& (SelectedRegion.IsNone() || PresentedActiveRegion != State->WorldState->GetActiveRegionId()))
	{
		SelectedRegion = State->WorldState->GetActiveRegionId();
		PresentedActiveRegion = SelectedRegion;
	}
	Presentation.RegionId = SelectedRegion;
	const FKnightMapRegion* Selected = CampaignWorld->FindRegion(SelectedRegion);
	Presentation.RegionLabel = Selected != nullptr ? Selected->Label : FText();
	Presentation.Regions.Reset();
	for (const FKnightMapRegion& Region : CampaignWorld->MapRegions)
	{
		FKnightCampaignRegionRow& Row = Presentation.Regions.AddDefaulted_GetRef();
		Row.RegionId = Region.RegionId;
		Row.Label = Region.Label;
		Row.bDiscovered = State->WorldState->IsRegionDiscovered(Region.RegionId);
	}
	Presentation.Markers.Reset();
	Hud.MinimapMarkers.Reset();
	const FKnightMapRegion* ActiveRegion = CampaignWorld->FindRegion(State->WorldState->GetActiveRegionId());
	for (const FKnightMapMarker& Marker : CampaignWorld->MapMarkers)
	{
		FVector2D MinimapUV;
		if (ActiveRegion != nullptr && Marker.RegionId == ActiveRegion->RegionId
			&& State->WorldState->IsMarkerDiscovered(Marker.MarkerId) && ActiveRegion->Project(Marker.WorldPosition, MinimapUV))
		{
			Hud.MinimapMarkers.Add(MinimapUV);
		}
		if (Marker.RegionId != SelectedRegion || Selected == nullptr)
		{
			continue;
		}
		FKnightCampaignMarkerRow& Row = Presentation.Markers.AddDefaulted_GetRef();
		Row.MarkerId = Marker.MarkerId;
		Row.RegionId = Marker.RegionId;
		Row.Label = Marker.Label;
		Row.Symbol = MarkerSymbol(Marker.Kind);
		Row.bDiscovered = State->WorldState->IsMarkerDiscovered(Marker.MarkerId);
		Row.bCanPing = Row.bDiscovered && Marker.RegionId == State->WorldState->GetActiveRegionId();
		Row.bCanTravel = Row.bDiscovered && !Marker.TravelRegionId.IsNone()
			&& Marker.RegionId == State->WorldState->GetActiveRegionId()
			&& Marker.TravelRegionId != State->WorldState->GetActiveRegionId()
			&& State->WorldState->IsRegionDiscovered(Marker.TravelRegionId);
		Selected->Project(Marker.WorldPosition, Row.UV);
	}
	for (const FKnightMapPing& Ping : State->WorldState->GetMapPings())
	{
		if (Selected != nullptr && Ping.RegionId == SelectedRegion)
		{
			FKnightCampaignMarkerRow& Row = Presentation.Markers.AddDefaulted_GetRef();
			Row.MarkerId = FName(*Ping.PingId.ToString(EGuidFormats::Digits));
			Row.Label = NSLOCTEXT("KnightMap", "PartyPing", "Party ping (expires automatically)");
			Row.Symbol = NSLOCTEXT("KnightMap", "PingSymbol", "!");
			Row.bDiscovered = true;
			Selected->Project(Ping.WorldPosition, Row.UV);
		}
	}
	UpdateMapPosition();
	RefreshHud();
	BindCampaignPanel();
	if (UKnightRPGPanel* Panel = GetRPGPanel(); Panel != nullptr && !Panel->IsActivated())
	{
		Panel->SetCampaignPresentation(Presentation);
	}
	if (UKnightRPGPanel* Panel = GetRPGPanel(); Panel != nullptr && Panel->IsActivated())
	{
		Panel->SetCampaignPresentation(Presentation);
	}
}
