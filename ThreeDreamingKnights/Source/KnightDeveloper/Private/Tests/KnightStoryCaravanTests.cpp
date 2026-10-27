#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Attributes/KnightAttributeSet.h"
#include "Character/KnightCharacter.h"
#include "Components/KnightAbilitySystemComponent.h"
#include "Components/KnightLifeComponent.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/PlatformTime.h"
#include "Interaction/KnightStoryAnchor.h"
#include "Inventory/KnightInventoryComponent.h"
#include "PartyArena/KnightArenaEnemy.h"
#include "PlayInEditorDataTypes.h"
#include "Progression/KnightRPGComponent.h"
#include "RPGLoop/KnightRPGStateActor.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Story/KnightProtectedCaravan.h"
#include "Story/KnightStoryGameMode.h"
#include "Story/KnightStoryPlayerController.h"
#include "Story/KnightStoryState.h"
#include "SkillLab/KnightBossArmorComponent.h"
#include "SkillLab/KnightRunestone.h"
#include "Events/KnightEventSubsystem.h"
#include "Events/KnightSkillLabEvent.h"
#include "UObject/StrongObjectPtr.h"

namespace KnightStoryCaravan
{
	class FEncounterTest final : public IAutomationLatentCommand
	{
	public:
		explicit FEncounterTest(FAutomationTestBase& InTest) : Test(InTest), Started(FPlatformTime::Seconds())
		{
		}
		~FEncounterTest()
		{
			if (GEditor->IsPlaySessionInProgress())
			{
				GEditor->EndPlayMap();
			}
		}
		virtual bool Update() override
		{
			if (Test.HasAnyErrors() || FPlatformTime::Seconds() - Started > 90.0)
			{
				if (!Test.HasAnyErrors())
				{
					Test.AddError(FString::Printf(TEXT("Caravan fixture timed out phase%d health%.1f"), Phase,
						Caravan.IsValid() ? Caravan->GetKnightAttributeSet()->GetHealth() : -1.0f));
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
				Request.GlobalMapOverride = TEXT("/Game/Knight/Maps/Playables/L_M06_CaravanFixture");
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
					|| !Mode->GetStoryInitializationResult().IsSuccess())
				{
					return false;
				}
				for (TActorIterator<AKnightProtectedCaravan> It(World.Get()); It; ++It)
				{
					Caravan = *It;
				}
				if (!Caravan.IsValid() || !Caravan->GetInitializationResult().IsSuccess())
				{
					return false;
				}
				Place(TEXT("Fixture_Start"));
				Test.TestTrue(TEXT("Start actual M4 wave"), Mode->TryInteract(Player.Get()).IsSuccess());
				OldRun = Mode->GetStoryState()->WorldState->GetSnapshot().EncounterRunId;
				Stone = World->SpawnActor<AKnightRunestone>();
				Stone->SetActorLocation(Player->GetControlledKnight()->GetActorLocation() + FVector(0.0, 160.0, -40.0));
				Stone->ConfigureFixture(EKnightRunestoneKind::Crystal, OldRun);
				Test.TestTrue(TEXT("Actual Runestone channel"), Stone->TryActivate(Player->GetControlledKnight(), FGuid::NewGuid()).IsSuccess());
				UKnightBossArmorComponent* Armor = Mode->GetEncounterEnemies()[0]->FindComponentByClass<UKnightBossArmorComponent>();
				Armor->EnableArmorZone(OldRun);
				Test.TestTrue(TEXT("Actual armor transition"), Armor->MakeBrittle());
				Test.TestTrue(TEXT("Armor adapter retains concrete state"), Mode->GetStoryState()->WorldState->GetSnapshot().Facts.ContainsByPredicate(
					[](const FKnightWorldFact& Fact)
					{
						return Fact.EventName == TEXT("ArmorStateChanged") && Fact.ValueId == TEXT("Brittle");
					}));
				Phase = 2;
				return false;
			}
			if (Phase == 2)
			{
				if (Caravan->GetKnightAttributeSet()->GetHealth() >= 600.0f || Stone->GetState() != EKnightRunestoneState::Cooldown)
				{
					return false;
				}
				Test.TestTrue(TEXT("Real StateTree/GAS enemy damaged protected target"),
					Caravan->GetKnightAttributeSet()->GetHealth() > 0.0f);
				Test.TestTrue(TEXT("Actual Runestone confirmed fact reaches story owner"),
					Mode->GetStoryState()->WorldState->GetSnapshot().Facts.ContainsByPredicate(
						[](const FKnightWorldFact& Fact)
						{
							return Fact.EventName == TEXT("RunestoneActivated") && Fact.SubjectId == TEXT("RS.Crystal");
						}));
				Test.TestTrue(TEXT("Fixture failure uses actual GAS life pipeline"),
					UKnightAbilitySystemComponent::ApplyDamage(Mode->GetEncounterEnemies()[0], Caravan.Get(), 100000.0f).IsSuccess());
				Test.TestEqual(TEXT("Caravan loss fails encounter"),
					Mode->GetStoryState()->WorldState->GetSnapshot().EncounterPhase, EKnightStoryEncounterPhase::Failed);
				Test.TestEqual(TEXT("Failure grants no reward"), Mode->GetRPGState()->GetRPG()->GetView().ClaimedRewards.Num(), 0);
				Test.TestTrue(TEXT("Retry resets actors through existing M4 owner"), Mode->TryRestartDrill(Player.Get()).IsSuccess());
				Place(TEXT("Fixture_Start"));
				Test.TestTrue(TEXT("Retry starts new encounter"), Mode->TryInteract(Player.Get()).IsSuccess());
				Test.TestFalse(TEXT("New encounter generation"),
					Mode->GetStoryState()->WorldState->GetSnapshot().EncounterRunId == OldRun);
				Test.TestFalse(TEXT("Old outcome cannot complete current run"),
					Mode->GetStoryState()->WorldState->FinishEncounter(true, OldRun).IsSuccess());
				Test.TestEqual(TEXT("Actual Runestone resets with the encounter"), Stone->GetState(), EKnightRunestoneState::Ready);
				Test.TestFalse(TEXT("Transient adapter facts reset"), Mode->GetStoryState()->WorldState->GetSnapshot().Facts.ContainsByPredicate(
					[](const FKnightWorldFact& Fact)
					{
						return Fact.EventName == TEXT("RunestoneActivated") || Fact.EventName == TEXT("ArmorStateChanged");
					}));
				FKnightSkillLabEvent Stale;
				Stale.EventName = TEXT("RunestoneActivated");
				Stale.SubjectId = TEXT("RS.Crystal");
				Stale.EncounterId = OldRun;
				Stale.ConfirmationId = FGuid::NewGuid();
				Stale.SourceId = Player->GetControlledKnight()->GetKnightEntityId();
				Stale.Revision = 999;
				const int64 BeforeStale = Mode->GetStoryState()->WorldState->GetSnapshot().FactRevision;
				World->GetSubsystem<UKnightEventSubsystem>()->PublishConfirmed(Stale);
				Test.TestEqual(TEXT("Old adapter generation creates no fact"), Mode->GetStoryState()->WorldState->GetSnapshot().FactRevision, BeforeStale);
				KillWave();
				Phase = 3;
				return false;
			}
			if (Phase == 3)
			{
				if (Mode->GetStoryState()->WorldState->GetSnapshot().EncounterPhase != EKnightStoryEncounterPhase::Complete)
				{
					return false;
				}
				Place(TEXT("Fixture_Claim"));
				Test.TestTrue(TEXT("Claim actual existing item transaction"), Mode->TryInteract(Player.Get()).IsSuccess());
				Test.TestEqual(TEXT("One-time Q5 receipt"), Mode->GetRPGState()->GetRPG()->GetView().ClaimedRewards.Num(), 1);
				ItemCount = Mode->GetRPGState()->GetInventory()->GetView().Items.Num();
				Test.TestTrue(TEXT("Success retry"), Mode->TryRestartDrill(Player.Get()).IsSuccess());
				Place(TEXT("Fixture_Start"));
				Test.TestTrue(TEXT("Replay encounter"), Mode->TryInteract(Player.Get()).IsSuccess());
				KillWave();
				Phase = 4;
				return false;
			}
			if (Phase == 4)
			{
				if (Mode->GetStoryState()->WorldState->GetSnapshot().EncounterPhase != EKnightStoryEncounterPhase::Complete)
				{
					return false;
				}
				Place(TEXT("Fixture_Claim"));
				Test.TestFalse(TEXT("Replay cannot issue reward again"), Mode->TryInteract(Player.Get()).IsSuccess());
				Test.TestEqual(TEXT("Receipt count unchanged"), Mode->GetRPGState()->GetRPG()->GetView().ClaimedRewards.Num(), 1);
				Test.TestEqual(TEXT("Items unchanged"), Mode->GetRPGState()->GetInventory()->GetView().Items.Num(), ItemCount);
				GEditor->EndPlayMap();
				Phase = 5;
				return false;
			}
			return !GEditor->IsPlaySessionInProgress();
		}

	private:
		void Place(FName AnchorId)
		{
			AKnightStoryAnchor* Anchor = Mode->GetStoryState()->WorldState->FindAnchor(AnchorId);
			Player->GetControlledKnight()->SetActorLocation(Anchor->GetActorLocation() + FVector(0.0, -180.0, 30.0));
		}
		void KillWave()
		{
			// This lifecycle fixture deliberately uses real server GAS damage, separate from mapped main-demo evidence.
			const TArray<TObjectPtr<AKnightArenaEnemy>> Enemies = Mode->GetEncounterEnemies();
			for (AKnightArenaEnemy* Enemy : Enemies)
			{
				Test.TestTrue(TEXT("GAS defeat commit"), UKnightAbilitySystemComponent::ApplyDamage(
					Player->GetControlledKnight(), Enemy, 100000.0f).IsSuccess());
			}
		}
		FAutomationTestBase& Test;
		double Started = 0.0;
		int32 Phase = 0;
		int32 ItemCount = 0;
		TStrongObjectPtr<ULevelEditorPlaySettings> Settings;
		TWeakObjectPtr<UWorld> World;
		TWeakObjectPtr<AKnightStoryGameMode> Mode;
		TWeakObjectPtr<AKnightStoryPlayerController> Player;
		TWeakObjectPtr<AKnightProtectedCaravan> Caravan;
		TWeakObjectPtr<AKnightRunestone> Stone;
		FGuid OldRun;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKnightStoryCaravanTest, "Knight.M6.ProtectedCaravanFailureRetryAndReceipt",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FKnightStoryCaravanTest::RunTest(const FString& Parameters)
{
	ADD_LATENT_AUTOMATION_COMMAND(KnightStoryCaravan::FEncounterTest(*this));
	return true;
}

#endif
