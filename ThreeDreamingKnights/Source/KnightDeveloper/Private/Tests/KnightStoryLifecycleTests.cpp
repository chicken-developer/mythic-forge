#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Character/KnightCharacter.h"
#include "Components/KnightAbilitySystemComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Interaction/KnightStoryAnchor.h"
#include "Misc/PackageName.h"
#include "Party/KnightPartyState.h"
#include "PlayInEditorDataTypes.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Story/KnightStoryGameMode.h"
#include "Story/KnightStoryPlayerController.h"
#include "Story/KnightStoryState.h"
#include "UObject/StrongObjectPtr.h"
#include "HAL/PlatformTime.h"
#include "GameFramework/CharacterMovementComponent.h"

namespace KnightStoryLifecycle
{
	class FSceneTest final : public IAutomationLatentCommand
	{
	public:
		explicit FSceneTest(FAutomationTestBase& InTest) : Test(InTest), Started(FPlatformTime::Seconds())
		{
		}
		~FSceneTest()
		{
			if (GEditor->IsPlaySessionInProgress())
			{
				GEditor->EndPlayMap();
			}
		}
		virtual bool Update() override
		{
			if (Test.HasAnyErrors() || FPlatformTime::Seconds() - Started > 60.0)
			{
				if (!Test.HasAnyErrors())
				{
					Test.AddError(TEXT("Scene lifecycle fixture timed out."));
				}
				return true;
			}
			if (Phase == 0)
			{
				Settings.Reset(DuplicateObject<ULevelEditorPlaySettings>(GetDefault<ULevelEditorPlaySettings>(), GetTransientPackage()));
				Settings->SetPlayNetMode(PIE_Standalone);
				Settings->SetPlayNumberOfClients(1);
				Settings->SetRunUnderOneProcess(true);
				Settings->EnableGameSound = false;
				FRequestPlaySessionParams Request;
				Request.EditorPlaySettings = Settings.Get();
				Request.GlobalMapOverride = TEXT("/Game/Knight/Maps/Playables/L_M06_LifecycleFixture");
				Request.bAllowOnlineSubsystem = false;
				GEditor->RequestPlaySession(Request);
				Phase = 1;
				return false;
			}
			if (Phase == 1)
			{
				for (const FWorldContext& Context : GEngine->GetWorldContexts())
				{
					if (Context.WorldType == EWorldType::PIE && Context.World() != nullptr)
					{
						World = Context.World();
						Mode = World->GetAuthGameMode<AKnightStoryGameMode>();
						Player = Cast<AKnightStoryPlayerController>(World->GetFirstPlayerController());
					}
				}
				if (!Mode.IsValid() || !Player.IsValid() || Player->GetControlledKnight() == nullptr
					|| !Mode->GetStoryInitializationResult().IsSuccess() || Player->GetStoryStatus().IsEmpty())
				{
					return false;
				}
				Anchor = Mode->GetStoryState()->WorldState->FindAnchor(TEXT("Fixture_Start"));
				AKnightCharacter* Knight = Player->GetControlledKnight();
				Knight->SetActorLocation(Anchor->GetActorLocation() + FVector(0.0, -180.0, 30.0));
				Knight->GetCharacterMovement()->StopMovementImmediately();
				FGuid Reservation;
				UKnightStoryWorldComponent* State = Mode->GetStoryState()->WorldState;
				Test.TestTrue(TEXT("Exclusive server reservation"), State->ReserveInteraction(Anchor->AnchorId, Knight,
					FGuid::NewGuid(), State->GetSnapshot().Revision, 260.0f, Reservation).IsSuccess());
				FGuid Competitor;
				Test.TestFalse(TEXT("Second request cannot double commit same station"),
					State->ReserveInteraction(Anchor->AnchorId, Knight, FGuid::NewGuid(),
						State->GetSnapshot().Revision, 260.0f, Competitor).IsSuccess());
				State->ReleasePawnReservations(Knight);
				Test.TestFalse(TEXT("Cancellation releases token"), State->ValidateReservation(Reservation).IsSuccess());
				Knight->SetActorLocation(Anchor->GetActorLocation() + FVector(0.0, -600.0, 30.0));
				Test.TestFalse(TEXT("Out-of-range command cannot reserve"), State->ReserveInteraction(Anchor->AnchorId, Knight,
					FGuid::NewGuid(), State->GetSnapshot().Revision, 260.0f, Reservation).IsSuccess());
				Knight->SetActorLocation(Anchor->GetActorLocation() + FVector(0.0, -180.0, 30.0));
				AStaticMeshActor* Occluder = World->SpawnActor<AStaticMeshActor>();
				Occluder->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
				Occluder->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
				Occluder->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
				Occluder->SetActorLocation(Anchor->GetActorLocation() + FVector(0.0, -90.0, 50.0));
				Test.TestFalse(TEXT("Real LOS occlusion cannot reserve"), State->ReserveInteraction(Anchor->AnchorId, Knight,
					FGuid::NewGuid(), State->GetSnapshot().Revision, 260.0f, Reservation).IsSuccess());
				Occluder->Destroy();
				const FName StableAnchor = Anchor->AnchorId;
				const FVector Position = Anchor->GetActorLocation();
				Anchor->Destroy();
				Anchor = World->SpawnActor<AKnightStoryAnchor>();
				Anchor->AnchorId = StableAnchor;
				Anchor->SetActorLocation(Position);
				Test.TestTrue(TEXT("Replacement actor binds by stable ID"), State->BindAnchor(Anchor.Get()).IsSuccess());
				Test.TestTrue(TEXT("Actual scene entry"), Mode->TryInteract(Player.Get()).IsSuccess());
				SceneRun = State->GetSnapshot().SceneRunId;
				Revision = State->GetSnapshot().Revision;
				Phase = 2;
				return false;
			}
			if (Phase == 2)
			{
				if (!Player->IsMoveInputIgnored() || !Player->IsLookInputIgnored())
				{
					return false;
				}
				const FGameplayTagContainer Ability(FGameplayTag::RequestGameplayTag(TEXT("Ability")));
				for (const FKnightRosterEntry& Entry : Mode->GetPartyState()->GetRoster())
				{
					Test.TestTrue(TEXT("Scene owns GAS action block"), Entry.Knight->GetKnightAbilitySystem()->AreAbilityTagsBlocked(Ability));
				}
				FKnightCampaignCheckpoint Unsafe;
				Test.TestFalse(TEXT("Active scene cannot save"), Mode->CaptureCheckpoint(Unsafe).IsSuccess());
				Test.TestTrue(TEXT("Actual host skip"), Mode->ExecuteStoryAction(Player.Get(), TEXT("Skip"), NAME_None, NAME_None,
					Player->GetControlledKnight()->GetKnightEntityId(), FGuid::NewGuid(), Revision).IsSuccess());
				const int64 FactRevision = Mode->GetStoryState()->WorldState->GetSnapshot().FactRevision;
				Test.TestTrue(TEXT("Same scene outcome replay idempotent"),
					Mode->GetStoryState()->WorldState->FinishScene(TEXT("Skipped"), SceneRun).IsSuccess());
				Test.TestEqual(TEXT("Replay creates no new fact"), Mode->GetStoryState()->WorldState->GetSnapshot().FactRevision, FactRevision);
				Test.TestFalse(TEXT("Conflicting replay rejected"), Mode->GetStoryState()->WorldState->FinishScene(TEXT("Cancelled"), SceneRun).IsSuccess());
				Phase = 3;
				return false;
			}
			if (Phase == 3)
			{
				if (Player->IsMoveInputIgnored() || Player->IsLookInputIgnored())
				{
					return false;
				}
				const FGameplayTagContainer Ability(FGameplayTag::RequestGameplayTag(TEXT("Ability")));
				for (const FKnightRosterEntry& Entry : Mode->GetPartyState()->GetRoster())
				{
					Test.TestFalse(TEXT("Only scene-owned GAS block released"), Entry.Knight->GetKnightAbilitySystem()->AreAbilityTagsBlocked(Ability));
				}
				Test.TestEqual(TEXT("Scene fact completes synthetic objective"),
					Mode->GetStoryState()->Quest->FindProgress(TEXT("Fixture_Scene"))->Phase, EKnightQuestPhase::Complete);
				Test.TestTrue(TEXT("Explicit current-state quest activation"),
					Mode->ExecuteStoryAction(Player.Get(), TEXT("Activate"), TEXT("Fixture_Collect"), NAME_None,
						Player->GetControlledKnight()->GetKnightEntityId(), FGuid::NewGuid(),
						Mode->GetStoryState()->WorldState->GetSnapshot().Revision).IsSuccess());
				Test.TestEqual(TEXT("Inventory predicate credits existing item at activation"),
					Mode->GetStoryState()->Quest->FindProgress(TEXT("Fixture_Collect"))->Counts.FindRef(TEXT("Fixture_CurrentItem")), 1);
				AKnightStoryAnchor* Deliver = Mode->GetStoryState()->WorldState->FindAnchor(TEXT("Fixture_Deliver"));
				Player->GetControlledKnight()->SetActorLocation(Deliver->GetActorLocation() + FVector(0.0, -180.0, 30.0));
				Test.TestTrue(TEXT("Delivery consumes through the existing coordinator"), Mode->TryInteract(Player.Get()).IsSuccess());
				Test.TestEqual(TEXT("Post-commit delivery completes objective"),
					Mode->GetStoryState()->Quest->FindProgress(TEXT("Fixture_Collect"))->Phase, EKnightQuestPhase::Complete);
				Test.TestFalse(TEXT("Completed delivery cannot debit again"), Mode->TryInteract(Player.Get()).IsSuccess());
				FKnightCampaignCheckpoint Delivered;
				Test.TestTrue(TEXT("Delivery receipt journal reconstructs in a consistent checkpoint"),
					Mode->CaptureCheckpoint(Delivered).IsSuccess());
				auto Use = [this](FName Id, FName Choice = NAME_None)
				{
					AKnightStoryAnchor* Binding = Mode->GetStoryState()->WorldState->FindAnchor(Id);
					Player->GetControlledKnight()->SetActorLocation(Binding->GetActorLocation() + FVector(0.0, -180.0, 30.0));
					return Mode->ExecuteStoryAction(Player.Get(), TEXT("Interact"), Id, Choice,
						Player->GetControlledKnight()->GetKnightEntityId(), FGuid::NewGuid(),
						Mode->GetStoryState()->WorldState->GetSnapshot().Revision);
				};
				Test.TestFalse(TEXT("Synthetic branch gate starts closed"), Use(TEXT("Fixture_Gate")).IsSuccess());
				Test.TestFalse(TEXT("Unauthored branch choice is rejected"), Use(TEXT("Fixture_Branch"), TEXT("Unknown")).IsSuccess());
				Test.TestTrue(TEXT("Host commits authored branch"), Use(TEXT("Fixture_Branch"), TEXT("Left")).IsSuccess());
				Test.TestTrue(TEXT("Retained branch opens the data-defined gate"), Use(TEXT("Fixture_Gate")).IsSuccess());
				Test.TestFalse(TEXT("Opposite branch cannot rewrite the durable decision"), Use(TEXT("Fixture_Branch"), TEXT("Right")).IsSuccess());
				Test.TestTrue(TEXT("Branch is included in the complete checkpoint"), Mode->CaptureCheckpoint(Delivered).IsSuccess()
					&& Delivered.Quest.Branches.Find(TEXT("Fixture_Route")) != nullptr
					&& *Delivered.Quest.Branches.Find(TEXT("Fixture_Route")) == TEXT("Left"));
				Test.TestTrue(TEXT("Scene re-entry"), Use(TEXT("Fixture_Start")).IsSuccess());
				Test.TestTrue(TEXT("Cancellation releases owned scene state"), Mode->ExecuteStoryAction(Player.Get(), TEXT("Cancel"),
					NAME_None, NAME_None, Player->GetControlledKnight()->GetKnightEntityId(), FGuid::NewGuid(),
					Mode->GetStoryState()->WorldState->GetSnapshot().Revision).IsSuccess());
				Test.TestFalse(TEXT("Cancel releases local move input"), Player->IsMoveInputIgnored());
				Test.TestTrue(TEXT("Scene before disconnect"), Use(TEXT("Fixture_Start")).IsSuccess());
				Mode->Logout(Player.Get());
				Test.TestFalse(TEXT("Server logout cancels scene"), Mode->GetStoryState()->WorldState->GetSnapshot().bSceneActive);
				Test.TestFalse(TEXT("Disconnect releases local look input"), Player->IsLookInputIgnored());
				GEditor->EndPlayMap();
				Phase = 4;
				return false;
			}
			return !GEditor->IsPlaySessionInProgress();
		}

	private:
		FAutomationTestBase& Test;
		double Started = 0.0;
		int32 Phase = 0;
		TStrongObjectPtr<ULevelEditorPlaySettings> Settings;
		TWeakObjectPtr<UWorld> World;
		TWeakObjectPtr<AKnightStoryGameMode> Mode;
		TWeakObjectPtr<AKnightStoryPlayerController> Player;
		TWeakObjectPtr<AKnightStoryAnchor> Anchor;
		FGuid SceneRun;
		int64 Revision = 0;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKnightStorySceneTest, "Knight.M6.SceneReservationAndBindingLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FKnightStorySceneTest::RunTest(const FString& Parameters)
{
	ADD_LATENT_AUTOMATION_COMMAND(KnightStoryLifecycle::FSceneTest(*this));
	return true;
}

#endif
