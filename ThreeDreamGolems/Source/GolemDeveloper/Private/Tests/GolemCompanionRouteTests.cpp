#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Campaign/GolemCampaignPlayerController.h"
#include "Campaign/GolemEncounterQualityGameMode.h"
#include "Character/GolemCharacter.h"
#include "Components/GolemAbilitySystemComponent.h"
#include "Components/GolemLifeComponent.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "Interaction/GolemStoryGate.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "NavigationData.h"
#include "NavigationSystem.h"
#include "Party/GolemPartyState.h"
#include "PlayInEditorDataTypes.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Story/GolemStoryState.h"
#include "UObject/StrongObjectPtr.h"

namespace GolemCompanionRouteTests
{
	class FRun final : public IAutomationLatentCommand
	{
	public:
		explicit FRun(FAutomationTestBase& InTest) : Test(InTest), Started(FPlatformTime::Seconds())
		{
		}
		~FRun()
		{
			if (GEditor->IsPlaySessionInProgress())
			{
				GEditor->EndPlayMap();
			}
		}
		virtual bool Update() override
		{
			if (Test.HasAnyErrors())
			{
				return true;
			}
			if (FPlatformTime::Seconds() - Started > 100.0)
			{
				Test.AddError(FString::Printf(TEXT("Companion gate/bridge/revive timeout phase%d maxZ %.1f/%.1f"), Phase, MaxZ[0], MaxZ[1]));
				if (Mode.IsValid())
				{
					for (const FGolemRosterEntry& Entry : Mode->GetPartyState()->GetRoster())
					{
						Test.AddInfo(FString::Printf(TEXT("Nav timeout Golem=%s position=%s alive=%d"),
							*Entry.GolemId.Value.ToString(), *Entry.Golem->GetActorLocation().ToString(), Entry.Golem->GetLifeComponent()->IsAlive()));
					}
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
				Settings->GameGetsMouseControl = false;
				FRequestPlaySessionParams Request;
				Request.EditorPlaySettings = Settings.Get();
				Request.GlobalMapOverride = TEXT("/Game/Knight/Maps/Playables/L_M07_5_ForgeTrial");
				Request.bAllowOnlineSubsystem = false;
				GEditor->RequestPlaySession(Request);
				Phase = 1;
				return false;
			}
			if (Phase == 5)
			{
				return !GEditor->IsPlaySessionInProgress();
			}
			for (const FWorldContext& Context : GEngine->GetWorldContexts())
			{
				if (Context.WorldType == EWorldType::PIE && Context.World() != nullptr)
				{
					Mode = Context.World()->GetAuthGameMode<AGolemEncounterQualityGameMode>();
					Player = Cast<AGolemCampaignPlayerController>(Context.World()->GetFirstPlayerController());
				}
			}
			if (!Mode.IsValid() || !Player.IsValid() || !Player->GetCampaignPresentation().bEnabled)
			{
				return false;
			}
			const TArray<FGolemRosterEntry>& Roster = Mode->GetPartyState()->GetRoster();
			if (Phase == 1)
			{
				for (TActorIterator<AGolemStoryGate> It(Mode->GetWorld()); It; ++It)
				{
					Gate = *It;
				}
				if (!Test.TestTrue(TEXT("Authored physical practice gate exists and starts closed"), Gate.IsValid() && !Gate->IsOpen()))
				{
					return true;
				}
				UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(Mode->GetWorld());
				ANavigationData* Data = Navigation != nullptr ? Navigation->GetDefaultNavDataInstance(FNavigationSystem::DontCreate) : nullptr;
				Test.TestTrue(TEXT("Actual loaded route supports dynamic gate navigation"), Data != nullptr && Data->GetRuntimeGenerationMode() == ERuntimeGenerationType::Dynamic);
				for (const FGolemRosterEntry& Entry : Roster)
				{
					Ids.Add(Entry.GolemId);
				}
				// Fixture setup separates the human and companions; measured movement never teleports them.
				Roster[0].Golem->SetActorLocation(FVector(-1550.0, 1900.0, 100.0));
				Roster[0].Golem->SetActorRotation(FRotator(0.0, 90.0, 0.0));
				Player->SetControlRotation(FRotator(0.0, 90.0, 0.0));
				Roster[1].Golem->SetActorLocation(FVector(-1650.0, -900.0, 100.0));
				Roster[2].Golem->SetActorLocation(FVector(-1450.0, -1100.0, 100.0));
				Player->RequestCompanionCommand(EGolemCompanionCommand::Regroup);
				PhaseStarted = Mode->GetWorld()->GetTimeSeconds();
				Previous[0] = Roster[1].Golem->GetActorLocation();
				Previous[1] = Roster[2].Golem->GetActorLocation();
				Phase = 2;
				return false;
			}
			if (Phase == 2 || Phase == 3)
			{
				for (int32 Index = 0; Index < 2; ++Index)
				{
					const FVector Position = Roster[Index + 1].Golem->GetActorLocation();
					MaxStep = FMath::Max(MaxStep, FVector::Distance(Position, Previous[Index]));
					MaxZ[Index] = FMath::Max(MaxZ[Index], Position.Z);
					Previous[Index] = Position;
				}
			}
			if (Phase == 2 && Mode->GetWorld()->GetTimeSeconds() - PhaseStarted > 4.0)
			{
				Test.TestFalse(TEXT("AI cannot open the quest-owned gate"), Gate->IsOpen());
				for (int32 Index = 1; Index < 3; ++Index)
				{
					const FVector Position = Roster[Index].Golem->GetActorLocation();
					Test.AddInfo(FString::Printf(TEXT("Closed gate companion%d position=%s"), Index, *Position.ToString()));
					Test.TestFalse(TEXT("Neither companion reaches the gated corridor north of the locked barrier"),
						Position.X < -1300.0 && Position.Y > -40.0);
				}
				Player->RequestInteract();
				Phase = 3;
				return false;
			}
			if (Phase == 3)
			{
				const auto IsOnLanding = [](const FVector& Position)
				{
					return Position.X < -1330.0 && Position.X > -1770.0 && Position.Y >= 1500.0;
				};
				if (!Gate->IsOpen() || !IsOnLanding(Roster[1].Golem->GetActorLocation())
					|| !IsOnLanding(Roster[2].Golem->GetActorLocation()))
				{
					return false;
				}
				Test.TestTrue(TEXT("Both companions use the raised bridge and steps"), MaxZ[0] > 160.0 && MaxZ[1] > 160.0);
				Test.AddInfo(FString::Printf(TEXT("Both companions reached the north landing; bridge Z %.3f/%.3f max step %.3f"), MaxZ[0], MaxZ[1], MaxStep));
				Test.TestTrue(TEXT("Measured route has no unstuck teleport"), MaxStep < 250.0);
				// Deliberate injury setup uses real authoritative GAS/life; the other AI must revive across the bridge.
				Roster[1].Golem->SetActorLocation(FVector(-1550.0, 400.0, 100.0));
				Test.TestTrue(TEXT("Fixture injury passes through real GAS damage"), UGolemAbilitySystemComponent::ApplyDamage(
					Roster[1].Golem, Roster[1].Golem, 2000.0f).IsSuccess());
				Test.TestTrue(TEXT("Canonical companion is actually downed"), Roster[1].Golem->GetLifeComponent()->IsDowned());
				Phase = 4;
				return false;
			}
			if (Phase == 4 && Roster[1].Golem->GetLifeComponent()->IsAlive())
			{
				for (int32 Index = 0; Index < Roster.Num(); ++Index)
				{
					Test.TestTrue(TEXT("Navigation/revive preserves canonical identity and ASC ownership"), Roster[Index].GolemId == Ids[Index]
						&& Roster[Index].Golem->GetGolemAbilitySystem()->GetOwnerActor() == Roster[Index].Golem
						&& Roster[Index].Golem->GetGolemAbilitySystem()->GetAvatarActor() == Roster[Index].Golem);
				}
				const FString Directory = FPaths::ProjectDir() / TEXT("Saved/Evidence/M75/navigation");
				IFileManager::Get().MakeDirectory(*Directory, true);
				FFileHelper::SaveStringToFile(FString::Printf(TEXT("result,max_frame_displacement_cm,bridge_z_1,bridge_z_2,ai_revive\nPASS,%.3f,%.3f,%.3f,1\n"),
					MaxStep, MaxZ[0], MaxZ[1]), *(Directory / TEXT("gate_bridge_revive.csv")));
				GEditor->EndPlayMap();
				Phase = 5;
			}
			return false;
		}

	private:
		FAutomationTestBase& Test;
		double Started;
		double PhaseStarted = 0.0;
		double MaxZ[2]{0.0, 0.0};
		double MaxStep = 0.0;
		FVector Previous[2];
		int32 Phase = 0;
		TArray<FGolemEntityId> Ids;
		TStrongObjectPtr<ULevelEditorPlaySettings> Settings;
		TWeakObjectPtr<AGolemEncounterQualityGameMode> Mode;
		TWeakObjectPtr<AGolemCampaignPlayerController> Player;
		TWeakObjectPtr<AGolemStoryGate> Gate;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGolemCompanionRouteTest, "Golem.M75.Companion.GateBridgeRevive",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::NonNullRHI)

bool FGolemCompanionRouteTest::RunTest(const FString& Parameters)
{
	FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<GolemCompanionRouteTests::FRun>(*this));
	return true;
}

#endif
