#include "Story/GolemStoryPlayerController.h"

#include "Assets/GolemAssetManager.h"
#include "Character/GolemCharacter.h"
#include "Definitions/GolemCampaignDefinition.h"
#include "Definitions/GolemStoryWorldDefinition.h"
#include "Engine/World.h"
#include "Story/GolemStoryGameMode.h"
#include "Story/GolemStoryState.h"
#include "TimerManager.h"
#include "Widgets/GolemRPGPanel.h"

void AGolemStoryPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (IsLocalController())
	{
		PresentationHandle = OnRPGPresentationChanged.AddUObject(this, &ThisClass::RefreshStoryPresentation);
		BindStorySources();
	}
}

void AGolemStoryPlayerController::BindStorySources()
{
	AGolemStoryState* State = GetWorld()->GetGameState<AGolemStoryState>();
	if (State == nullptr || State->Quest->GetSnapshot().CampaignId.IsNone())
	{
		if (++BindingAttempts < 100)
		{
			GetWorldTimerManager().SetTimer(ReadinessTimer, this, &ThisClass::BindStorySources, 0.1f, false);
		}
		return;
	}
	Story = State;
	QuestHandle = State->Quest->OnSnapshotChanged.AddUObject(this, &ThisClass::RefreshStoryPresentation);
	WorldHandle = State->WorldState->OnSnapshotChanged.AddUObject(this, &ThisClass::RefreshStoryPresentation);
	DialogueHandle = State->Dialogue->OnSnapshotChanged.AddUObject(this, &ThisClass::RefreshStoryPresentation);
	RefreshStoryPresentation();
}

void AGolemStoryPlayerController::RefreshStoryPresentation()
{
	if (!Story.IsValid())
	{
		return;
	}
	const FGolemQuestSnapshot& Snapshot = Story->Quest->GetSnapshot();
	FString Lines;
	TArray<FGolemStoryChoicePresentation> Choices;
	UGolemDefinition* Resolved = nullptr;
	UGolemAssetManager* Manager = UGolemAssetManager::GetGolemAssetManager();
	UGolemCampaignDefinition* Definition = Manager != nullptr
		&& Manager->ResolveDefinition(Snapshot.DefinitionId, UGolemCampaignDefinition::StaticClass(), Resolved).IsSuccess()
		? Cast<UGolemCampaignDefinition>(Resolved) : nullptr;
	UGolemStoryWorldDefinition* WorldDefinition = nullptr;
	Resolved = nullptr;
	if (Manager != nullptr && Manager->ResolveDefinition(Story->GetWorldDefinitionId(),
		UGolemStoryWorldDefinition::StaticClass(), Resolved).IsSuccess())
	{
		WorldDefinition = Cast<UGolemStoryWorldDefinition>(Resolved);
	}
	for (const FGolemQuestProgress& Quest : Snapshot.Quests)
	{
		if (Quest.Phase == EGolemQuestPhase::Active || Quest.Phase == EGolemQuestPhase::RewardPending)
		{
			const FGolemQuestDefinition* Row = Definition != nullptr ? Definition->FindQuest(Quest.QuestId) : nullptr;
			Lines += Row != nullptr ? Row->Title.ToString() : Quest.QuestId.ToString();
			if (Quest.Phase == EGolemQuestPhase::RewardPending)
			{
				Lines += TEXT(": reward ready. F at CLAIM.\n");
			}
			else if (Row != nullptr)
			{
				for (const FGolemObjectiveDefinition& Objective : Row->Objectives)
				{
					const int32 Count = Quest.Counts.FindRef(Objective.ObjectiveId);
					if (Count < Objective.RequiredCount)
					{
						Lines += TEXT(": ") + Objective.Label.ToString() + TEXT("\n");
						if (WorldDefinition != nullptr)
						{
							for (const FGolemStoryInteraction& Interaction : WorldDefinition->Interactions)
							{
								if (Interaction.RequiredObjectiveId != Objective.ObjectiveId)
								{
									continue;
								}
								for (const FName ChoiceId : Interaction.AllowedChoices)
								{
									FGolemStoryChoicePresentation& Choice = Choices.AddDefaulted_GetRef();
									Choice.AnchorId = Interaction.AnchorId;
									Choice.ChoiceId = ChoiceId;
									Choice.Label = FText::Format(NSLOCTEXT("GolemStoryUI", "Choice", "Choose {0}"),
										FText::FromName(ChoiceId));
								}
							}
						}
						break;
					}
				}
			}
		}
	}
	if (Lines.IsEmpty())
	{
		Lines = TEXT("Lesson complete. I: Save checkpoint / Continue from disk.\n");
	}
	if (Story->Dialogue->GetSnapshot().bActive)
	{
		Lines += Story->Dialogue->GetSnapshot().Text.ToString() + TEXT("\nF: next (host).\n");
	}
	const bool bSceneActive = Story->WorldState->GetSnapshot().bSceneActive;
	if (bSceneInputOwned != bSceneActive)
	{
		SetIgnoreMoveInput(bSceneActive);
		SetIgnoreLookInput(bSceneActive);
		bSceneInputOwned = bSceneActive;
	}
	if (bSceneActive)
	{
		Lines += TEXT("Scene active. I: Skip / Cancel.\n");
	}
	Lines += Feedback.ToString();
	Status = FText::FromString(Lines);
	if (UGolemRPGPanel* Panel = GetRPGPanel())
	{
		if (!PanelHandle.IsValid())
		{
			PanelHandle = Panel->OnStoryActionRequested.AddUObject(this, &ThisClass::HandlePanelAction);
			ChoiceHandle = Panel->OnStoryChoiceRequested.AddUObject(this, &ThisClass::HandlePanelChoice);
		}
		Panel->SetStoryPresentation(Status, GetWorld()->GetNetMode() != NM_Client, PendingRequest.IsValid());
		Panel->SetStoryChoices(Choices, PendingRequest.IsValid());
	}
}

void AGolemStoryPlayerController::HandlePanelAction(const FName Action)
{
	if (Action == TEXT("Activate") && Story.IsValid())
	{
		for (const FGolemQuestProgress& Quest : Story->Quest->GetSnapshot().Quests)
		{
			if (Quest.Phase == EGolemQuestPhase::Available)
			{
				RequestStoryAction(Action, Quest.QuestId);
				return;
			}
		}
	}
	RequestStoryAction(Action);
}

void AGolemStoryPlayerController::HandlePanelChoice(const FName AnchorId, const FName ChoiceId)
{
	RequestStoryAction(TEXT("Interact"), AnchorId, ChoiceId);
}

void AGolemStoryPlayerController::RequestStoryAction(const FName Action, const FName AnchorId, const FName ChoiceId)
{
	if (PendingRequest.IsValid() || !Story.IsValid() || GetControlledGolem() == nullptr)
	{
		return;
	}
	PendingRequest = FGuid::NewGuid();
	const int64 Revision = Action == TEXT("Advance") ? Story->Dialogue->GetSnapshot().Revision
		: Story->WorldState->GetSnapshot().Revision;
	ServerRequestStoryAction(Action, AnchorId, ChoiceId, GetControlledGolem()->GetGolemEntityId(),
		PendingRequest, Revision);
	RefreshStoryPresentation();
}

void AGolemStoryPlayerController::ServerRequestStoryAction_Implementation(const FName Action, const FName AnchorId,
	const FName ChoiceId, const FGolemEntityId ExpectedGolem, const FGuid RequestId, const int64 ExpectedRevision)
{
	AGolemStoryGameMode* Mode = GetWorld()->GetAuthGameMode<AGolemStoryGameMode>();
	const FGolemResult Result = Mode != nullptr
		? Mode->ExecuteStoryAction(this, Action, AnchorId, ChoiceId, ExpectedGolem, RequestId, ExpectedRevision)
		: FGolemResult::Failure(EGolemError::InvalidState, TEXT("StoryMode"), FText::GetEmpty());
	ClientReceiveStoryResult(RequestId, Action, Result);
}

void AGolemStoryPlayerController::ClientReceiveStoryResult_Implementation(const FGuid RequestId, const FName Action,
	const FGolemResult Result)
{
	if (PendingRequest != RequestId)
	{
		return;
	}
	PendingRequest.Invalidate();
	LastResult = Result;
	Feedback = Result.IsSuccess()
		? (Action == TEXT("Save") ? NSLOCTEXT("GolemStoryUI", "Saved", "Saved to disk.")
			: NSLOCTEXT("GolemStoryUI", "Confirmed", "Confirmed."))
		: (!Result.Message.IsEmpty() ? Result.Message : FText::FromName(Result.Context));
	RefreshStoryPresentation();
}

const FText& AGolemStoryPlayerController::GetStoryStatus() const
{
	return Status;
}

const FGolemResult& AGolemStoryPlayerController::GetLastStoryResult() const
{
	return LastResult;
}

void AGolemStoryPlayerController::EndPlay(const EEndPlayReason::Type Reason)
{
	GetWorldTimerManager().ClearTimer(ReadinessTimer);
	if (Story.IsValid())
	{
		Story->Quest->OnSnapshotChanged.Remove(QuestHandle);
		Story->WorldState->OnSnapshotChanged.Remove(WorldHandle);
		Story->Dialogue->OnSnapshotChanged.Remove(DialogueHandle);
	}
	OnRPGPresentationChanged.Remove(PresentationHandle);
	if (UGolemRPGPanel* Panel = GetRPGPanel())
	{
		Panel->OnStoryActionRequested.Remove(PanelHandle);
		Panel->OnStoryChoiceRequested.Remove(ChoiceHandle);
	}
	if (bSceneInputOwned)
	{
		SetIgnoreMoveInput(false);
		SetIgnoreLookInput(false);
		bSceneInputOwned = false;
	}
	Super::EndPlay(Reason);
}
