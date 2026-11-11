#include "Campaign/GolemCampaignPlayerController.h"

#include "Attributes/GolemAttributeSet.h"
#include "Character/GolemCharacter.h"
#include "Checkpoint/GolemCheckpointSubsystem.h"
#include "Components/GolemAbilitySystemComponent.h"
#include "Components/GolemCombatComponent.h"
#include "Components/GolemLifeComponent.h"
#include "Definitions/GolemCampaignDefinition.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Interaction/GolemStoryAnchor.h"
#include "Inventory/GolemInventoryComponent.h"
#include "Party/GolemPartyState.h"
#include "Progression/GolemRPGComponent.h"
#include "RPGLoop/GolemRPGStateActor.h"
#include "Settings/GolemGameUserSettings.h"
#include "Skills/GolemSkillRuntimeComponent.h"
#include "Story/GolemStoryState.h"
#include "Widgets/GolemRPGPanel.h"

namespace
{
	FText QuestPhaseLabel(const EGolemQuestPhase Phase)
	{
		switch (Phase)
		{
		case EGolemQuestPhase::Locked: return NSLOCTEXT("GolemJournal", "Locked", "Locked — complete prerequisites");
		case EGolemQuestPhase::Available: return NSLOCTEXT("GolemJournal", "Available", "Available — accept to start");
		case EGolemQuestPhase::Active: return NSLOCTEXT("GolemJournal", "Active", "In progress");
		case EGolemQuestPhase::RewardPending: return NSLOCTEXT("GolemJournal", "Pending", "Reward ready — claim at the reward marker");
		case EGolemQuestPhase::Complete: return NSLOCTEXT("GolemJournal", "Complete", "Complete");
		}
		return FText();
	}

	FText MarkerSymbol(const EGolemMapMarkerKind Kind)
	{
		switch (Kind)
		{
		case EGolemMapMarkerKind::Quest: return NSLOCTEXT("GolemMap", "QuestSymbol", "Q");
		case EGolemMapMarkerKind::Checkpoint: return NSLOCTEXT("GolemMap", "CheckpointSymbol", "C");
		case EGolemMapMarkerKind::Travel: return NSLOCTEXT("GolemMap", "TravelSymbol", "T");
		case EGolemMapMarkerKind::Hazard: return NSLOCTEXT("GolemMap", "HazardSymbol", "!");
		default: return NSLOCTEXT("GolemMap", "LandmarkSymbol", "L");
		}
	}
}

FString AGolemCampaignPlayerController::CurrentSaveStamp() const
{
	const AGolemStoryState* State = GetWorld()->GetGameState<AGolemStoryState>();
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

void AGolemCampaignPlayerController::ReadCheckpointSummary(const bool bJustSaved)
{
	AGolemStoryState* State = GetWorld()->GetGameState<AGolemStoryState>();
	if (State == nullptr || CampaignWorld == nullptr || GetNetMode() == NM_Client)
	{
		return;
	}
	const bool bInitialRead = !bReadCheckpoint;
	bReadCheckpoint = true;
	const FGolemQuestSnapshot& Quest = State->Quest->GetSnapshot();
	FGolemCampaignCheckpoint Loaded;
	const FGolemResult Result = GetGameInstance()->GetSubsystem<UGolemCheckpointSubsystem>()->LoadCheckpoint(
		Quest.CampaignId, Quest.ScenarioKind, Loaded);
	bHasDiskCheckpoint = Result.IsSuccess();
	if (!bHasDiskCheckpoint)
	{
		CheckpointDescription = NSLOCTEXT("GolemCampaignUI", "NoCheckpoint", "No readable checkpoint. Start a new campaign.");
		return;
	}
	ContinueRegion = CampaignWorld->MapRegions.IsEmpty() ? NAME_None : CampaignWorld->MapRegions[0].RegionId;
	for (const FGolemMapRegion& Region : CampaignWorld->MapRegions)
	{
		if (Region.Map.ToSoftObjectPath().GetLongPackageName() == Loaded.MapPackage)
		{
			ContinueRegion = Region.RegionId;
		}
	}
	CheckpointDescription = FText::Format(NSLOCTEXT("GolemCampaignUI", "CheckpointSummary",
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

void AGolemCampaignPlayerController::RefreshCampaignPresentation()
{
	if (bEndingCampaign || CampaignWorld == nullptr || CampaignDefinition == nullptr)
	{
		return;
	}
	AGolemStoryState* State = GetWorld()->GetGameState<AGolemStoryState>();
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
	for (const FGolemWorldFact& Fact : State->WorldState->GetSnapshot().Facts)
	{
		if (Fact.EventName == TEXT("DifficultyChosen") && Fact.SubjectId == CampaignWorld->EncounterQualityDefinitionId.PrimaryAssetName)
		{
			DifficultyId = Fact.ValueId;
		}
	}
	Presentation.Difficulty = DifficultyId == TEXT("Story") ? NSLOCTEXT("GolemCampaignUI", "StoryDifficulty", "Story — one simultaneous hostile attack")
		: NSLOCTEXT("GolemCampaignUI", "StandardDifficulty", "Standard — two simultaneous hostile attacks, at most one per Golem");
	Presentation.Confirmation = Confirmation;
	Presentation.SessionStatus = GetNetMode() == NM_Standalone
		? (GetWorld()->IsPaused() ? NSLOCTEXT("GolemCampaignUI", "Paused", "Solo — simulation paused")
			: NSLOCTEXT("GolemCampaignUI", "Solo", "Solo — simulation running"))
		: NSLOCTEXT("GolemCampaignUI", "CoopRunning", "Co-op — menu does not pause the world; host owns campaign saves and travel.");
	Presentation.SaveStatus = Presentation.bPending ? NSLOCTEXT("GolemCampaignUI", "SavePending", "Pending request...")
		: !Presentation.bHost ? NSLOCTEXT("GolemCampaignUI", "HostSave", "Campaign progress is saved by the host.")
		: !bHasDiskCheckpoint ? NSLOCTEXT("GolemCampaignUI", "NoDurableSave", "No checkpoint yet — save at a safe boundary to keep your progress.")
		: bHasDiskCheckpoint && SavedStamp == CurrentSaveStamp()
			? NSLOCTEXT("GolemCampaignUI", "Clean", "Saved checkpoint — no new persistent changes")
			: NSLOCTEXT("GolemCampaignUI", "Dirty", "Unsaved progress — quit resumes the last durable checkpoint.");
	Presentation.Quests.Reset();
	Presentation.ActiveObjective = FText();
	UGolemGameUserSettings* Settings = UGolemGameUserSettings::Get();
	const FText InteractKey = Settings != nullptr ? Settings->GetActionKey(TEXT("Interact")).GetDisplayName()
		: NSLOCTEXT("GolemCampaignUI", "DefaultInteract", "F");
	for (const FGolemQuestProgress& Progress : State->Quest->GetSnapshot().Quests)
	{
		const FGolemQuestDefinition* Definition = CampaignDefinition->FindQuest(Progress.QuestId);
		if (Definition == nullptr)
		{
			continue;
		}
		FGolemCampaignJournalRow& Row = Presentation.Quests.AddDefaulted_GetRef();
		Row.QuestId = Progress.QuestId;
		Row.Title = Definition->Title;
		Row.bCanActivate = Progress.Phase == EGolemQuestPhase::Available;
		FString Detail = QuestPhaseLabel(Progress.Phase).ToString();
		for (const FGolemObjectiveDefinition& Objective : Definition->Objectives)
		{
			const int32 Count = Progress.Counts.FindRef(Objective.ObjectiveId);
			const FString Label = Objective.Label.ToString().Replace(TEXT("[F]"),
				*FText::Format(NSLOCTEXT("GolemCampaignUI", "KeyHint", "[{0}]"), InteractKey).ToString());
			Detail += FString::Printf(TEXT("\n%s  %d/%d"), *Label, Count, Objective.RequiredCount);
			if (Presentation.ActiveObjective.IsEmpty() && Progress.Phase == EGolemQuestPhase::Active
				&& Count < Objective.RequiredCount)
			{
				Presentation.ActiveObjective = FText::Format(NSLOCTEXT("GolemJournal", "TrackedObjective", "{0}: {1}"),
					Definition->Title, FText::FromString(Label));
			}
		}
		Row.Progress = FText::FromString(Detail);
		if (Progress.Phase == EGolemQuestPhase::RewardPending)
		{
			Presentation.ActiveObjective = FText::Format(NSLOCTEXT("GolemJournal", "RewardReady", "{0}: reward ready — visit CLAIM."), Definition->Title);
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
	const FGolemMapRegion* Selected = CampaignWorld->FindRegion(SelectedRegion);
	Presentation.RegionLabel = Selected != nullptr ? Selected->Label : FText();
	Presentation.Regions.Reset();
	for (const FGolemMapRegion& Region : CampaignWorld->MapRegions)
	{
		FGolemCampaignRegionRow& Row = Presentation.Regions.AddDefaulted_GetRef();
		Row.RegionId = Region.RegionId;
		Row.Label = Region.Label;
		Row.bDiscovered = State->WorldState->IsRegionDiscovered(Region.RegionId);
	}
	Presentation.Markers.Reset();
	Hud.MinimapMarkers.Reset();
	const FGolemMapRegion* ActiveRegion = CampaignWorld->FindRegion(State->WorldState->GetActiveRegionId());
	for (const FGolemMapMarker& Marker : CampaignWorld->MapMarkers)
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
		FGolemCampaignMarkerRow& Row = Presentation.Markers.AddDefaulted_GetRef();
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
	for (const FGolemMapPing& Ping : State->WorldState->GetMapPings())
	{
		if (Selected != nullptr && Ping.RegionId == SelectedRegion)
		{
			FGolemCampaignMarkerRow& Row = Presentation.Markers.AddDefaulted_GetRef();
			Row.MarkerId = FName(*Ping.PingId.ToString(EGuidFormats::Digits));
			Row.Label = NSLOCTEXT("GolemMap", "PartyPing", "Party ping (expires automatically)");
			Row.Symbol = NSLOCTEXT("GolemMap", "PingSymbol", "!");
			Row.bDiscovered = true;
			Selected->Project(Ping.WorldPosition, Row.UV);
		}
	}
	UpdateMapPosition();
	RefreshHud();
	BindCampaignPanel();
	if (UGolemRPGPanel* Panel = GetRPGPanel(); Panel != nullptr && !Panel->IsActivated())
	{
		Panel->SetCampaignPresentation(Presentation);
	}
	if (UGolemRPGPanel* Panel = GetRPGPanel(); Panel != nullptr && Panel->IsActivated())
	{
		Panel->SetCampaignPresentation(Presentation);
	}
}
