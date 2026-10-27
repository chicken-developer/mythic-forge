#include "Story/KnightStoryPlayerController.h"

#include "Assets/KnightAssetManager.h"
#include "Character/KnightCharacter.h"
#include "Definitions/KnightCampaignDefinition.h"
#include "Definitions/KnightStoryWorldDefinition.h"
#include "Engine/World.h"
#include "Story/KnightStoryGameMode.h"
#include "Story/KnightStoryState.h"
#include "TimerManager.h"
#include "Widgets/KnightRPGPanel.h"

void AKnightStoryPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (IsLocalController())
	{
		PresentationHandle = OnRPGPresentationChanged.AddUObject(this, &ThisClass::RefreshStoryPresentation);
		BindStorySources();
	}
}

void AKnightStoryPlayerController::BindStorySources()
{
	AKnightStoryState* State = GetWorld()->GetGameState<AKnightStoryState>();
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

void AKnightStoryPlayerController::RefreshStoryPresentation()
{
	if (!Story.IsValid())
	{
		return;
	}
	const FKnightQuestSnapshot& Snapshot = Story->Quest->GetSnapshot();
	FString Lines;
	TArray<FKnightStoryChoicePresentation> Choices;
	UKnightDefinition* Resolved = nullptr;
	UKnightAssetManager* Manager = UKnightAssetManager::GetKnightAssetManager();
	UKnightCampaignDefinition* Definition = Manager != nullptr
		&& Manager->ResolveDefinition(Snapshot.DefinitionId, UKnightCampaignDefinition::StaticClass(), Resolved).IsSuccess()
		? Cast<UKnightCampaignDefinition>(Resolved) : nullptr;
	UKnightStoryWorldDefinition* WorldDefinition = nullptr;
	Resolved = nullptr;
	if (Manager != nullptr && Manager->ResolveDefinition(Story->GetWorldDefinitionId(),
		UKnightStoryWorldDefinition::StaticClass(), Resolved).IsSuccess())
	{
		WorldDefinition = Cast<UKnightStoryWorldDefinition>(Resolved);
	}
	for (const FKnightQuestProgress& Quest : Snapshot.Quests)
	{
		if (Quest.Phase == EKnightQuestPhase::Active || Quest.Phase == EKnightQuestPhase::RewardPending)
		{
			const FKnightQuestDefinition* Row = Definition != nullptr ? Definition->FindQuest(Quest.QuestId) : nullptr;
			Lines += Row != nullptr ? Row->Title.ToString() : Quest.QuestId.ToString();
			if (Quest.Phase == EKnightQuestPhase::RewardPending)
			{
				Lines += TEXT(": reward ready. F at CLAIM.\n");
			}
			else if (Row != nullptr)
			{
				for (const FKnightObjectiveDefinition& Objective : Row->Objectives)
				{
					const int32 Count = Quest.Counts.FindRef(Objective.ObjectiveId);
					if (Count < Objective.RequiredCount)
					{
						Lines += TEXT(": ") + Objective.Label.ToString() + TEXT("\n");
						if (WorldDefinition != nullptr)
						{
							for (const FKnightStoryInteraction& Interaction : WorldDefinition->Interactions)
							{
								if (Interaction.RequiredObjectiveId != Objective.ObjectiveId)
								{
									continue;
								}
								for (const FName ChoiceId : Interaction.AllowedChoices)
								{
									FKnightStoryChoicePresentation& Choice = Choices.AddDefaulted_GetRef();
									Choice.AnchorId = Interaction.AnchorId;
									Choice.ChoiceId = ChoiceId;
									Choice.Label = FText::Format(NSLOCTEXT("KnightStoryUI", "Choice", "Choose {0}"),
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
	if (UKnightRPGPanel* Panel = GetRPGPanel())
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

void AKnightStoryPlayerController::HandlePanelAction(const FName Action)
{
	if (Action == TEXT("Activate") && Story.IsValid())
	{
		for (const FKnightQuestProgress& Quest : Story->Quest->GetSnapshot().Quests)
		{
			if (Quest.Phase == EKnightQuestPhase::Available)
			{
				RequestStoryAction(Action, Quest.QuestId);
				return;
			}
		}
	}
	RequestStoryAction(Action);
}

void AKnightStoryPlayerController::HandlePanelChoice(const FName AnchorId, const FName ChoiceId)
{
	RequestStoryAction(TEXT("Interact"), AnchorId, ChoiceId);
}

void AKnightStoryPlayerController::RequestStoryAction(const FName Action, const FName AnchorId, const FName ChoiceId)
{
	if (PendingRequest.IsValid() || !Story.IsValid() || GetControlledKnight() == nullptr)
	{
		return;
	}
	PendingRequest = FGuid::NewGuid();
	const int64 Revision = Action == TEXT("Advance") ? Story->Dialogue->GetSnapshot().Revision
		: Story->WorldState->GetSnapshot().Revision;
	ServerRequestStoryAction(Action, AnchorId, ChoiceId, GetControlledKnight()->GetKnightEntityId(),
		PendingRequest, Revision);
	RefreshStoryPresentation();
}

void AKnightStoryPlayerController::ServerRequestStoryAction_Implementation(const FName Action, const FName AnchorId,
	const FName ChoiceId, const FKnightEntityId ExpectedKnight, const FGuid RequestId, const int64 ExpectedRevision)
{
	AKnightStoryGameMode* Mode = GetWorld()->GetAuthGameMode<AKnightStoryGameMode>();
	const FKnightResult Result = Mode != nullptr
		? Mode->ExecuteStoryAction(this, Action, AnchorId, ChoiceId, ExpectedKnight, RequestId, ExpectedRevision)
		: FKnightResult::Failure(EKnightError::InvalidState, TEXT("StoryMode"), FText::GetEmpty());
	ClientReceiveStoryResult(RequestId, Action, Result);
}

void AKnightStoryPlayerController::ClientReceiveStoryResult_Implementation(const FGuid RequestId, const FName Action,
	const FKnightResult Result)
{
	if (PendingRequest != RequestId)
	{
		return;
	}
	PendingRequest.Invalidate();
	LastResult = Result;
	Feedback = Result.IsSuccess()
		? (Action == TEXT("Save") ? NSLOCTEXT("KnightStoryUI", "Saved", "Saved to disk.")
			: NSLOCTEXT("KnightStoryUI", "Confirmed", "Confirmed."))
		: (!Result.Message.IsEmpty() ? Result.Message : FText::FromName(Result.Context));
	RefreshStoryPresentation();
}

const FText& AKnightStoryPlayerController::GetStoryStatus() const
{
	return Status;
}

const FKnightResult& AKnightStoryPlayerController::GetLastStoryResult() const
{
	return LastResult;
}

void AKnightStoryPlayerController::EndPlay(const EEndPlayReason::Type Reason)
{
	GetWorldTimerManager().ClearTimer(ReadinessTimer);
	if (Story.IsValid())
	{
		Story->Quest->OnSnapshotChanged.Remove(QuestHandle);
		Story->WorldState->OnSnapshotChanged.Remove(WorldHandle);
		Story->Dialogue->OnSnapshotChanged.Remove(DialogueHandle);
	}
	OnRPGPresentationChanged.Remove(PresentationHandle);
	if (UKnightRPGPanel* Panel = GetRPGPanel())
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
