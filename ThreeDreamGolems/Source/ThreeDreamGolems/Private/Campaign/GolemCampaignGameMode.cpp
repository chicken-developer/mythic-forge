#include "Campaign/GolemCampaignGameMode.h"

#include "Assets/GolemAssetManager.h"
#include "Campaign/GolemCampaignEntry.h"
#include "Campaign/GolemCampaignPlayerController.h"
#include "Campaign/GolemCampaignHUD.h"
#include "Character/GolemCharacter.h"
#include "Checkpoint/GolemCheckpointSubsystem.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "Party/GolemPartyState.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Story/GolemStoryState.h"
#include "TimerManager.h"

AGolemCampaignGameMode::AGolemCampaignGameMode()
{
	PlayerControllerClass = AGolemCampaignPlayerController::StaticClass();
	HUDClass = AGolemCampaignHUD::StaticClass();
}

void AGolemCampaignGameMode::BeginPlay()
{
	Super::BeginPlay();
	AGolemStoryState* State = GetStoryState();
	if (State == nullptr || !GetStoryInitializationResult().IsSuccess())
	{
		return;
	}
	UGolemDefinition* Resolved = nullptr;
	UGolemAssetManager* Manager = UGolemAssetManager::GetGolemAssetManager();
	if (Manager == nullptr || !Manager->ResolveDefinition(State->GetWorldDefinitionId(),
		UGolemStoryWorldDefinition::StaticClass(), Resolved).IsSuccess())
	{
		return;
	}
	for (TActorIterator<AGolemCampaignEntry> It(GetWorld()); It; ++It)
	{
		if (State->WorldState->ConfigureMap(Cast<UGolemStoryWorldDefinition>(Resolved), It->RegionId).IsSuccess())
		{
			RefreshDiscovery();
			GetWorldTimerManager().SetTimer(DiscoveryTimer, this, &ThisClass::RefreshDiscovery, 0.25f, true);
		}
		break;
	}
#if !UE_BUILD_SHIPPING
	if (FParse::Param(FCommandLine::Get(), TEXT("GolemCampaignColdProof")))
	{
		FString ExpectedFile;
		FGolemCampaignCheckpoint Expected;
		FGolemCampaignCheckpoint Actual;
		bool bPassed = FParse::Value(FCommandLine::Get(), TEXT("GolemCampaignExpected="), ExpectedFile)
			&& UGolemCheckpointSubsystem::ReadCheckpointFile(ExpectedFile, Expected).IsSuccess()
			&& CaptureCheckpoint(Actual).IsSuccess()
			&& Actual.Quest.RunId == Expected.Quest.RunId
			&& Actual.RPG.Inventory.Items == Expected.RPG.Inventory.Items
			&& Actual.RPG.View.ClaimedRewards == Expected.RPG.View.ClaimedRewards
			&& Actual.RPG.View.Golems.Num() == 3
			&& Expected.RPG.View.Golems.Num() == 3
			&& State->WorldState->IsRegionDiscovered(TEXT("School"))
			&& State->WorldState->IsRegionDiscovered(TEXT("Courtyard"))
			&& State->WorldState->GetMapPings().IsEmpty();
		for (int32 Index = 0; bPassed && Index < 3; ++Index)
		{
			bPassed = Actual.RPG.View.Golems[Index].GolemId == Expected.RPG.View.Golems[Index].GolemId;
		}
		if (bPassed && FParse::Param(FCommandLine::Get(), TEXT("GolemCampaignColdResave")))
		{
			bPassed = GetGameInstance()->GetSubsystem<UGolemCheckpointSubsystem>()->SaveCheckpoint(Actual).IsSuccess();
		}
		UE_LOG(LogTemp, Display, TEXT("CampaignColdProof result=%s run=%s items=%d receipts=%d regions=2 pings=%d"),
			bPassed ? TEXT("PASS") : TEXT("FAIL"), *Actual.Quest.RunId.ToString(), Actual.RPG.Inventory.Items.Num(),
			Actual.RPG.View.ClaimedRewards.Num(), State->WorldState->GetMapPings().Num());
		FPlatformMisc::RequestExit(false);
	}
#endif
}

void AGolemCampaignGameMode::RefreshDiscovery()
{
	TArray<FVector> Positions;
	for (const FGolemRosterEntry& Entry : GetPartyState()->GetRoster())
	{
		if (IsValid(Entry.Golem))
		{
			Positions.Add(Entry.Golem->GetActorLocation() + FVector(GetWorld()->OriginLocation));
		}
	}
	GetStoryState()->WorldState->RevealNearbyMarkers(Positions);
}

void AGolemCampaignGameMode::EndPlay(const EEndPlayReason::Type Reason)
{
	GetWorldTimerManager().ClearTimer(DiscoveryTimer);
	Super::EndPlay(Reason);
}
