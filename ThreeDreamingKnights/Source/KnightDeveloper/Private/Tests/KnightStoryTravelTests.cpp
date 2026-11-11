#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Character/KnightCharacter.h"
#include "Checkpoint/KnightCheckpointSubsystem.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/PlatformTime.h"
#include "Interaction/KnightStoryAnchor.h"
#include "Inventory/KnightInventoryComponent.h"
#include "Misc/Paths.h"
#include "Party/KnightPartyState.h"
#include "PlayInEditorDataTypes.h"
#include "Progression/KnightRPGComponent.h"
#include "RPGLoop/KnightRPGStateActor.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Story/KnightStoryGameMode.h"
#include "Story/KnightStoryPlayerController.h"
#include "Story/KnightStoryState.h"
#include "UObject/StrongObjectPtr.h"

namespace KnightStoryTravel
{
	class FTravel final : public IAutomationLatentCommand
	{
	public:
		explicit FTravel(FAutomationTestBase& InTest) : Test(InTest), Started(FPlatformTime::Seconds())
		{
		}
		~FTravel()
		{
			if (GEditor->IsPlaySessionInProgress())
			{
				GEditor->EndPlayMap();
			}
		}
		virtual bool Update() override
		{
			if (Test.HasAnyErrors() || FPlatformTime::Seconds() - Started > 120.0)
			{
				if (!Test.HasAnyErrors())
				{
					Test.AddError(FString::Printf(TEXT("Continue/JIP/travel timeout phase%d"), Phase));
				}
				return true;
			}
			if (Phase == 0)
			{
				Test.TestTrue(TEXT("Immutable genuine V1 fixture opens"), UKnightCheckpointSubsystem::ReadCheckpointFile(
					FPaths::ProjectDir() / TEXT("Source/KnightDeveloper/Fixtures/M6/V1_completed.knight"), Original).IsSuccess());
				Settings.Reset(DuplicateObject<ULevelEditorPlaySettings>(GetDefault<ULevelEditorPlaySettings>(), GetTransientPackage()));
				Settings->SetPlayNetMode(PIE_ListenServer);
				Settings->SetPlayNumberOfClients(1);
				Settings->SetRunUnderOneProcess(true);
				Settings->SetServerPort(17967);
				Settings->EnableGameSound = false;
				FRequestPlaySessionParams Request;
				Request.EditorPlaySettings = Settings.Get();
				Request.GlobalMapOverride = TEXT("/Game/Knight/Maps/Playables/L_M06_StoryFrameworkDemo");
				Request.bAllowOnlineSubsystem = false;
				GEditor->RequestPlaySession(Request);
				Phase = 1;
				return false;
			}
			RefreshHost();
			if (Phase == 7)
			{
				return !GEditor->IsPlaySessionInProgress();
			}
			if (!Mode.IsValid() || !Player.IsValid() || Player->GetControlledKnight() == nullptr
				|| !Mode->GetStoryInitializationResult().IsSuccess())
			{
				return false;
			}
			if (Phase == 1)
			{
				Test.TestTrue(TEXT("Existing disk owner stores the immutable fixture"),
					World->GetGameInstance()->GetSubsystem<UKnightCheckpointSubsystem>()->SaveCheckpoint(Original).IsSuccess());
				Test.TestTrue(TEXT("Actual host Continue command"),
					Action(TEXT("Continue"), NAME_None).IsSuccess());
				Phase = 2;
				return false;
			}
			if (Mode->GetStoryState()->Quest->GetSnapshot().RunId != Original.Quest.RunId)
			{
				return false;
			}
			if (Phase == 2)
			{
				Test.TestTrue(TEXT("Continue reconstructs canonical IDs, exact items and receipt"), MatchesHost());
				GEditor->RequestLateJoin();
				Phase = 3;
				return false;
			}
			if (Phase == 3)
			{
				if (Mode->GetPartyState()->GetHumanCount() != 2 || !ClientsConverged(1))
				{
					return false;
				}
				for (const FWorldContext& Context : GEngine->GetWorldContexts())
				{
					if (Context.WorldType == EWorldType::PIE && Context.World() != nullptr
						&& Context.World()->GetNetMode() == NM_Client)
					{
						Guest = Cast<AKnightStoryPlayerController>(Context.World()->GetFirstPlayerController());
					}
				}
				if (!Guest.IsValid() || Guest->GetStoryStatus().IsEmpty())
				{
					return false;
				}
				Guest->RequestStoryAction(TEXT("Save"));
				Phase = 4;
				return false;
			}
			if (Phase == 4)
			{
				if (Guest->GetLastStoryResult().Context != TEXT("HostDecision"))
				{
					return false;
				}
				Test.TestFalse(TEXT("Guest RPC cannot decide campaign checkpoint"), Guest->GetLastStoryResult().IsSuccess());
				GEditor->RequestLateJoin();
				Phase = 5;
				return false;
			}
			if (Phase == 5)
			{
				if (Mode->GetPartyState()->GetHumanCount() != 3 || !ClientsConverged(2))
				{
					return false;
				}
				AKnightStoryAnchor* Anchor = Mode->GetStoryState()->WorldState->FindAnchor(TEXT("School_Travel"));
				if (!Test.TestNotNull(TEXT("Authored travel binding exists"), Anchor))
				{
					return true;
				}
				Player->GetControlledKnight()->SetActorLocation(Anchor->GetActorLocation() + FVector(0.0, -180.0, 30.0));
				Test.TestTrue(TEXT("Actual authored host party travel"), Action(TEXT("Interact"), Anchor->AnchorId).IsSuccess());
				Phase = 6;
				return false;
			}
			if (Phase == 6)
			{
				if (!World->GetOutermost()->GetName().Contains(TEXT("L_M06_StoryCourtyard"))
					|| Mode->GetPartyState()->GetHumanCount() != 3 || !ClientsConverged(2))
				{
					return false;
				}
				Test.TestTrue(TEXT("Safe party travel retains all three IDs and exact reward"), MatchesHost());
				Test.TestEqual(TEXT("V2 quest available after data-only travel"),
					Mode->GetStoryState()->Quest->FindProgress(TEXT("Demo_Append"))->Phase, EKnightQuestPhase::Available);
				Test.TestFalse(TEXT("Move input released after travel"), Player->IsMoveInputIgnored());
				Test.TestFalse(TEXT("Look input released after travel"), Player->IsLookInputIgnored());
				FKnightCampaignCheckpoint After;
				Test.TestTrue(TEXT("Post-travel checkpoint remains capturable"), Mode->CaptureCheckpoint(After).IsSuccess());
				Test.TestEqual(TEXT("No repeated one-time receipt"), After.RPG.View.ClaimedRewards.Num(), 1);
				GEditor->EndPlayMap();
				Phase = 7;
				return false;
			}
			return !GEditor->IsPlaySessionInProgress();
		}
	private:
		void RefreshHost()
		{
			for (const FWorldContext& Context : GEngine->GetWorldContexts())
			{
				if (Context.WorldType == EWorldType::PIE && Context.World() != nullptr
					&& Context.World()->GetNetMode() != NM_Client)
				{
					World = Context.World();
					Mode = World->GetAuthGameMode<AKnightStoryGameMode>();
					Player = Cast<AKnightStoryPlayerController>(World->GetFirstPlayerController());
				}
			}
		}
		FKnightResult Action(FName Name, FName Anchor)
		{
			return Mode->ExecuteStoryAction(Player.Get(), Name, Anchor, NAME_None,
				Player->GetControlledKnight()->GetKnightEntityId(), FGuid::NewGuid(),
				Mode->GetStoryState()->WorldState->GetSnapshot().Revision);
		}
		bool MatchesHost() const
		{
			const FKnightRPGView Current = Mode->GetRPGState()->GetRPG()->GetView();
			if (Current.Knights.Num() != 3 || Current.ClaimedRewards != Original.RPG.View.ClaimedRewards
				|| Mode->GetRPGState()->GetInventory()->GetView().Items != Original.RPG.Inventory.Items)
			{
				return false;
			}
			for (const FKnightRPGKnightView& Knight : Original.RPG.View.Knights)
			{
				if (!Current.Knights.ContainsByPredicate([&Knight](const FKnightRPGKnightView& Row)
					{
						return Row.KnightId == Knight.KnightId && Row.Build == Knight.Build;
					}))
				{
					return false;
				}
			}
			return true;
		}
		bool ClientsConverged(int32 Expected) const
		{
			int32 Count = 0;
			for (const FWorldContext& Context : GEngine->GetWorldContexts())
			{
				UWorld* Client = Context.World();
				if (Context.WorldType != EWorldType::PIE || Client == nullptr || Client->GetNetMode() != NM_Client)
				{
					continue;
				}
				AKnightStoryState* State = Client->GetGameState<AKnightStoryState>();
				AKnightRPGStateActor* RPG = nullptr;
				for (TActorIterator<AKnightRPGStateActor> It(Client); It; ++It)
				{
					RPG = *It;
				}
				if (State == nullptr || State->Quest->GetSnapshot().RunId != Original.Quest.RunId
					|| State->Quest->FindProgress(TEXT("ActI_Q2")) == nullptr
					|| State->Quest->FindProgress(TEXT("ActI_Q2"))->Phase != EKnightQuestPhase::Complete
					|| RPG == nullptr || RPG->GetRPG()->GetView().ClaimedRewards != Original.RPG.View.ClaimedRewards)
				{
					return false;
				}
				++Count;
			}
			return Count == Expected;
		}
		FAutomationTestBase& Test;
		double Started;
		int32 Phase = 0;
		FKnightCampaignCheckpoint Original;
		TStrongObjectPtr<ULevelEditorPlaySettings> Settings;
		TWeakObjectPtr<UWorld> World;
		TWeakObjectPtr<AKnightStoryGameMode> Mode;
		TWeakObjectPtr<AKnightStoryPlayerController> Player;
		TWeakObjectPtr<AKnightStoryPlayerController> Guest;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKnightStoryTravelTest, "Knight.M6.ContinueLateJoinAndPartyTravel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FKnightStoryTravelTest::RunTest(const FString& Parameters)
{
	ADD_LATENT_AUTOMATION_COMMAND(KnightStoryTravel::FTravel(*this));
	return true;
}

#endif
