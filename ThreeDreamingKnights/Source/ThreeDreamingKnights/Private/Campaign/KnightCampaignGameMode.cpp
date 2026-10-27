#include "Campaign/KnightCampaignGameMode.h"

#include "Assets/KnightAssetManager.h"
#include "Campaign/KnightCampaignEntry.h"
#include "Campaign/KnightCampaignPlayerController.h"
#include "Campaign/KnightCampaignHUD.h"
#include "Character/KnightCharacter.h"
#include "Checkpoint/KnightCheckpointSubsystem.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "Party/KnightPartyState.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Story/KnightStoryState.h"
#include "TimerManager.h"

AKnightCampaignGameMode::AKnightCampaignGameMode()
{
	PlayerControllerClass = AKnightCampaignPlayerController::StaticClass();
	HUDClass = AKnightCampaignHUD::StaticClass();
}

void AKnightCampaignGameMode::BeginPlay()
{
	Super::BeginPlay();
	AKnightStoryState* State = GetStoryState();
	if (State == nullptr || !GetStoryInitializationResult().IsSuccess())
	{
		return;
	}
	UKnightDefinition* Resolved = nullptr;
	UKnightAssetManager* Manager = UKnightAssetManager::GetKnightAssetManager();
	if (Manager == nullptr || !Manager->ResolveDefinition(State->GetWorldDefinitionId(),
		UKnightStoryWorldDefinition::StaticClass(), Resolved).IsSuccess())
	{
		return;
	}
	for (TActorIterator<AKnightCampaignEntry> It(GetWorld()); It; ++It)
	{
		if (State->WorldState->ConfigureMap(Cast<UKnightStoryWorldDefinition>(Resolved), It->RegionId).IsSuccess())
		{
			RefreshDiscovery();
			GetWorldTimerManager().SetTimer(DiscoveryTimer, this, &ThisClass::RefreshDiscovery, 0.25f, true);
		}
		break;
	}
#if !UE_BUILD_SHIPPING
	if (FParse::Param(FCommandLine::Get(), TEXT("KnightCampaignColdProof")))
	{
		FString ExpectedFile;
		FKnightCampaignCheckpoint Expected;
		FKnightCampaignCheckpoint Actual;
		bool bPassed = FParse::Value(FCommandLine::Get(), TEXT("KnightCampaignExpected="), ExpectedFile)
			&& UKnightCheckpointSubsystem::ReadCheckpointFile(ExpectedFile, Expected).IsSuccess()
			&& CaptureCheckpoint(Actual).IsSuccess()
			&& Actual.Quest.RunId == Expected.Quest.RunId
			&& Actual.RPG.Inventory.Items == Expected.RPG.Inventory.Items
			&& Actual.RPG.View.ClaimedRewards == Expected.RPG.View.ClaimedRewards
			&& Actual.RPG.View.Knights.Num() == 3
			&& Expected.RPG.View.Knights.Num() == 3
			&& State->WorldState->IsRegionDiscovered(TEXT("School"))
			&& State->WorldState->IsRegionDiscovered(TEXT("Courtyard"))
			&& State->WorldState->GetMapPings().IsEmpty();
		for (int32 Index = 0; bPassed && Index < 3; ++Index)
		{
			bPassed = Actual.RPG.View.Knights[Index].KnightId == Expected.RPG.View.Knights[Index].KnightId;
		}
		if (bPassed && FParse::Param(FCommandLine::Get(), TEXT("KnightCampaignColdResave")))
		{
			bPassed = GetGameInstance()->GetSubsystem<UKnightCheckpointSubsystem>()->SaveCheckpoint(Actual).IsSuccess();
		}
		UE_LOG(LogTemp, Display, TEXT("CampaignColdProof result=%s run=%s items=%d receipts=%d regions=2 pings=%d"),
			bPassed ? TEXT("PASS") : TEXT("FAIL"), *Actual.Quest.RunId.ToString(), Actual.RPG.Inventory.Items.Num(),
			Actual.RPG.View.ClaimedRewards.Num(), State->WorldState->GetMapPings().Num());
		FPlatformMisc::RequestExit(false);
	}
#endif
}

void AKnightCampaignGameMode::RefreshDiscovery()
{
	TArray<FVector> Positions;
	for (const FKnightRosterEntry& Entry : GetPartyState()->GetRoster())
	{
		if (IsValid(Entry.Knight))
		{
			Positions.Add(Entry.Knight->GetActorLocation() + FVector(GetWorld()->OriginLocation));
		}
	}
	GetStoryState()->WorldState->RevealNearbyMarkers(Positions);
}

void AKnightCampaignGameMode::EndPlay(const EEndPlayReason::Type Reason)
{
	GetWorldTimerManager().ClearTimer(DiscoveryTimer);
	Super::EndPlay(Reason);
}
