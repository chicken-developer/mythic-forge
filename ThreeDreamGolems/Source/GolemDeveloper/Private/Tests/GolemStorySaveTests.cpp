#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Checkpoint/GolemCheckpointSubsystem.h"
#include "Engine/GameInstance.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGolemStoryDiskTest, "Golem.M6.DiskDurabilityAndRecovery",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGolemStoryDiskTest::RunTest(const FString& Parameters)
{
	FGolemCampaignCheckpoint Snapshot;
	Snapshot.CheckpointId = FGuid::NewGuid();
	Snapshot.Quest.CampaignId = TEXT("DiskFixture");
	Snapshot.Quest.ScenarioKind = TEXT("Test");
	Snapshot.Quest.RunId = FGuid::NewGuid();
	Snapshot.World.CampaignId = Snapshot.Quest.CampaignId;
	Snapshot.World.RunId = Snapshot.Quest.RunId;
	Snapshot.RPG.View.DemoRunId = Snapshot.Quest.RunId;
	Snapshot.RPG.View.bGearDemo = true;
	Snapshot.MapPackage = TEXT("/Game/Knight/Maps/Playables/L_M06_StoryFrameworkDemo");
	for (int32 Index = 0; Index < 3; ++Index)
	{
		FGolemRPGGolemView& Golem = Snapshot.RPG.View.Golems.AddDefaulted_GetRef();
		Golem.GolemId = FGolemEntityId::Create();
		Snapshot.Poise.Add(Golem.GolemId, 100.0f);
	}
	const FString Path = FPaths::ProjectSavedDir() / TEXT("Automation/M6")
		/ (FGuid::NewGuid().ToString(EGuidFormats::Digits) + TEXT(".golem"));
	TestTrue(TEXT("Validated flush and replace"), UGolemCheckpointSubsystem::WriteCheckpointFile(Path, Snapshot).IsSuccess());
	FGolemCampaignCheckpoint Loaded;
	TestTrue(TEXT("Cold byte decode"), UGolemCheckpointSubsystem::ReadCheckpointFile(Path, Loaded).IsSuccess());
	TestEqual(TEXT("Canonical GUID preserved"), Loaded.RPG.View.Golems[0].GolemId, Snapshot.RPG.View.Golems[0].GolemId);
	const FGuid FirstCommit = Snapshot.CheckpointId;
	Snapshot.CheckpointId = FGuid::NewGuid();
	TestFalse(TEXT("Interrupted temp write never reports Saved"),
		UGolemCheckpointSubsystem::WriteCheckpointFile(Path, Snapshot, true).IsSuccess());
	UGolemCheckpointSubsystem::ReadCheckpointFile(Path, Loaded);
	TestEqual(TEXT("Interrupted write preserves durable revision"), Loaded.CheckpointId, FirstCommit);
	TestTrue(TEXT("Next complete write"), UGolemCheckpointSubsystem::WriteCheckpointFile(Path, Snapshot).IsSuccess());
	FGolemCampaignCheckpoint Backup;
	TestTrue(TEXT("Backup remains valid"), UGolemCheckpointSubsystem::ReadCheckpointFile(Path + TEXT(".bak"), Backup).IsSuccess());
	TestEqual(TEXT("Backup is prior durable commit"), Backup.CheckpointId, FirstCommit);
	TArray<uint8> Corrupted;
	TestTrue(TEXT("Read durable bytes for checksum fault"), FFileHelper::LoadFileToArray(Corrupted, *Path));
	if (Corrupted.Num() > 28)
	{
		Corrupted.Last() ^= 1;
		TestTrue(TEXT("Write actual payload corruption"),
			FFileHelper::SaveArrayToFile(Corrupted, *(Path + TEXT(".corrupt"))));
		const FGolemResult Corruption = UGolemCheckpointSubsystem::ReadCheckpointFile(Path + TEXT(".corrupt"), Loaded);
		TestFalse(TEXT("Payload corruption rejected"), Corruption.IsSuccess());
		TestEqual(TEXT("Checksum fence precedes object decode"),
			Corruption.Context, FName(TEXT("CheckpointChecksum")));
	}
	TArray<uint8> Truncated{1, 2, 3};
	FFileHelper::SaveArrayToFile(Truncated, *Path);
	TestFalse(TEXT("Truncated main rejected"), UGolemCheckpointSubsystem::ReadCheckpointFile(Path, Loaded).IsSuccess());
	TestTrue(TEXT("Recovery backup readable"), UGolemCheckpointSubsystem::ReadCheckpointFile(Path + TEXT(".bak"), Loaded).IsSuccess());
	TestTrue(TEXT("Save after backup recovery"), UGolemCheckpointSubsystem::WriteCheckpointFile(Path, Snapshot).IsSuccess());
	FGolemCampaignCheckpoint Future = Snapshot;
	Future.EnvelopeVersion = 2;
	TestFalse(TEXT("Unsupported future envelope rejected"), UGolemCheckpointSubsystem::WriteCheckpointFile(Path, Future).IsSuccess());
	Future = Snapshot;
	Future.World.RunId = FGuid::NewGuid();
	TestFalse(TEXT("Mixed world/item generation rejected"), Future.ValidateScope().IsSuccess());
	FGolemCampaignCheckpoint Recovery = Snapshot;
	Recovery.Quest.CampaignId = FName(*(TEXT("DiskRecovery_")
		+ FGuid::NewGuid().ToString(EGuidFormats::Digits)));
	Recovery.World.CampaignId = Recovery.Quest.CampaignId;
	const FString RecoveryPath = UGolemCheckpointSubsystem::GetCheckpointPath(
		Recovery.Quest.CampaignId, Recovery.Quest.ScenarioKind);
	const FGuid RecoveryCommit = Recovery.CheckpointId;
	TestTrue(TEXT("First recovery-owner commit"),
		UGolemCheckpointSubsystem::WriteCheckpointFile(RecoveryPath, Recovery).IsSuccess());
	Recovery.CheckpointId = FGuid::NewGuid();
	TestTrue(TEXT("Second recovery-owner commit"),
		UGolemCheckpointSubsystem::WriteCheckpointFile(RecoveryPath, Recovery).IsSuccess());
	FFileHelper::SaveArrayToFile(Truncated, *RecoveryPath);
	UGameInstance* Instance = NewObject<UGameInstance>();
	UGolemCheckpointSubsystem* Owner = NewObject<UGolemCheckpointSubsystem>(Instance);
	TestTrue(TEXT("Actual disk owner falls back to valid backup"),
		Owner->LoadCheckpoint(Recovery.Quest.CampaignId, Recovery.Quest.ScenarioKind, Loaded).IsSuccess());
	TestEqual(TEXT("Owner recovered prior durable commit"), Loaded.CheckpointId, RecoveryCommit);
	FFileHelper::SaveArrayToFile(Truncated, *(RecoveryPath + TEXT(".bak")));
	TestFalse(TEXT("Owner rejects corrupt main and backup"),
		Owner->LoadCheckpoint(Recovery.Quest.CampaignId, Recovery.Quest.ScenarioKind, Loaded).IsSuccess());
	TestFalse(TEXT("Unrecoverable file never reports Saved"),
		UGolemCheckpointSubsystem::WriteCheckpointFile(RecoveryPath, Recovery).IsSuccess());
	const FString ReadOnly = FPaths::ProjectSavedDir() / TEXT("Automation/M6/ReadOnly_")
		+ FGuid::NewGuid().ToString(EGuidFormats::Digits) + TEXT(".golem");
	TestTrue(TEXT("Create read-only fixture"), UGolemCheckpointSubsystem::WriteCheckpointFile(ReadOnly, Snapshot).IsSuccess());
	FPlatformFileManager::Get().GetPlatformFile().SetReadOnly(*ReadOnly, true);
	Snapshot.CheckpointId = FGuid::NewGuid();
	TestFalse(TEXT("Failed replacement never reports Saved"),
		UGolemCheckpointSubsystem::WriteCheckpointFile(ReadOnly, Snapshot).IsSuccess());
	FPlatformFileManager::Get().GetPlatformFile().SetReadOnly(*ReadOnly, false);
	return true;
}

#endif
