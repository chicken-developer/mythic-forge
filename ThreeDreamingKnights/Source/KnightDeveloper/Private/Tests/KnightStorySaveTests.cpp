#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Checkpoint/KnightCheckpointSubsystem.h"
#include "Engine/GameInstance.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKnightStoryDiskTest, "Knight.M6.DiskDurabilityAndRecovery",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FKnightStoryDiskTest::RunTest(const FString& Parameters)
{
	FKnightCampaignCheckpoint Snapshot;
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
		FKnightRPGKnightView& Knight = Snapshot.RPG.View.Knights.AddDefaulted_GetRef();
		Knight.KnightId = FKnightEntityId::Create();
		Snapshot.Poise.Add(Knight.KnightId, 100.0f);
	}
	const FString Path = FPaths::ProjectSavedDir() / TEXT("Automation/M6")
		/ (FGuid::NewGuid().ToString(EGuidFormats::Digits) + TEXT(".knight"));
	TestTrue(TEXT("Validated flush and replace"), UKnightCheckpointSubsystem::WriteCheckpointFile(Path, Snapshot).IsSuccess());
	FKnightCampaignCheckpoint Loaded;
	TestTrue(TEXT("Cold byte decode"), UKnightCheckpointSubsystem::ReadCheckpointFile(Path, Loaded).IsSuccess());
	TestEqual(TEXT("Canonical GUID preserved"), Loaded.RPG.View.Knights[0].KnightId, Snapshot.RPG.View.Knights[0].KnightId);
	const FGuid FirstCommit = Snapshot.CheckpointId;
	Snapshot.CheckpointId = FGuid::NewGuid();
	TestFalse(TEXT("Interrupted temp write never reports Saved"),
		UKnightCheckpointSubsystem::WriteCheckpointFile(Path, Snapshot, true).IsSuccess());
	UKnightCheckpointSubsystem::ReadCheckpointFile(Path, Loaded);
	TestEqual(TEXT("Interrupted write preserves durable revision"), Loaded.CheckpointId, FirstCommit);
	TestTrue(TEXT("Next complete write"), UKnightCheckpointSubsystem::WriteCheckpointFile(Path, Snapshot).IsSuccess());
	FKnightCampaignCheckpoint Backup;
	TestTrue(TEXT("Backup remains valid"), UKnightCheckpointSubsystem::ReadCheckpointFile(Path + TEXT(".bak"), Backup).IsSuccess());
	TestEqual(TEXT("Backup is prior durable commit"), Backup.CheckpointId, FirstCommit);
	TArray<uint8> Corrupted;
	TestTrue(TEXT("Read durable bytes for checksum fault"), FFileHelper::LoadFileToArray(Corrupted, *Path));
	if (Corrupted.Num() > 28)
	{
		Corrupted.Last() ^= 1;
		TestTrue(TEXT("Write actual payload corruption"),
			FFileHelper::SaveArrayToFile(Corrupted, *(Path + TEXT(".corrupt"))));
		const FKnightResult Corruption = UKnightCheckpointSubsystem::ReadCheckpointFile(Path + TEXT(".corrupt"), Loaded);
		TestFalse(TEXT("Payload corruption rejected"), Corruption.IsSuccess());
		TestEqual(TEXT("Checksum fence precedes object decode"),
			Corruption.Context, FName(TEXT("CheckpointChecksum")));
	}
	TArray<uint8> Truncated{1, 2, 3};
	FFileHelper::SaveArrayToFile(Truncated, *Path);
	TestFalse(TEXT("Truncated main rejected"), UKnightCheckpointSubsystem::ReadCheckpointFile(Path, Loaded).IsSuccess());
	TestTrue(TEXT("Recovery backup readable"), UKnightCheckpointSubsystem::ReadCheckpointFile(Path + TEXT(".bak"), Loaded).IsSuccess());
	TestTrue(TEXT("Save after backup recovery"), UKnightCheckpointSubsystem::WriteCheckpointFile(Path, Snapshot).IsSuccess());
	FKnightCampaignCheckpoint Future = Snapshot;
	Future.EnvelopeVersion = 2;
	TestFalse(TEXT("Unsupported future envelope rejected"), UKnightCheckpointSubsystem::WriteCheckpointFile(Path, Future).IsSuccess());
	Future = Snapshot;
	Future.World.RunId = FGuid::NewGuid();
	TestFalse(TEXT("Mixed world/item generation rejected"), Future.ValidateScope().IsSuccess());
	FKnightCampaignCheckpoint Recovery = Snapshot;
	Recovery.Quest.CampaignId = FName(*(TEXT("DiskRecovery_")
		+ FGuid::NewGuid().ToString(EGuidFormats::Digits)));
	Recovery.World.CampaignId = Recovery.Quest.CampaignId;
	const FString RecoveryPath = UKnightCheckpointSubsystem::GetCheckpointPath(
		Recovery.Quest.CampaignId, Recovery.Quest.ScenarioKind);
	const FGuid RecoveryCommit = Recovery.CheckpointId;
	TestTrue(TEXT("First recovery-owner commit"),
		UKnightCheckpointSubsystem::WriteCheckpointFile(RecoveryPath, Recovery).IsSuccess());
	Recovery.CheckpointId = FGuid::NewGuid();
	TestTrue(TEXT("Second recovery-owner commit"),
		UKnightCheckpointSubsystem::WriteCheckpointFile(RecoveryPath, Recovery).IsSuccess());
	FFileHelper::SaveArrayToFile(Truncated, *RecoveryPath);
	UGameInstance* Instance = NewObject<UGameInstance>();
	UKnightCheckpointSubsystem* Owner = NewObject<UKnightCheckpointSubsystem>(Instance);
	TestTrue(TEXT("Actual disk owner falls back to valid backup"),
		Owner->LoadCheckpoint(Recovery.Quest.CampaignId, Recovery.Quest.ScenarioKind, Loaded).IsSuccess());
	TestEqual(TEXT("Owner recovered prior durable commit"), Loaded.CheckpointId, RecoveryCommit);
	FFileHelper::SaveArrayToFile(Truncated, *(RecoveryPath + TEXT(".bak")));
	TestFalse(TEXT("Owner rejects corrupt main and backup"),
		Owner->LoadCheckpoint(Recovery.Quest.CampaignId, Recovery.Quest.ScenarioKind, Loaded).IsSuccess());
	TestFalse(TEXT("Unrecoverable file never reports Saved"),
		UKnightCheckpointSubsystem::WriteCheckpointFile(RecoveryPath, Recovery).IsSuccess());
	const FString ReadOnly = FPaths::ProjectSavedDir() / TEXT("Automation/M6/ReadOnly_")
		+ FGuid::NewGuid().ToString(EGuidFormats::Digits) + TEXT(".knight");
	TestTrue(TEXT("Create read-only fixture"), UKnightCheckpointSubsystem::WriteCheckpointFile(ReadOnly, Snapshot).IsSuccess());
	FPlatformFileManager::Get().GetPlatformFile().SetReadOnly(*ReadOnly, true);
	Snapshot.CheckpointId = FGuid::NewGuid();
	TestFalse(TEXT("Failed replacement never reports Saved"),
		UKnightCheckpointSubsystem::WriteCheckpointFile(ReadOnly, Snapshot).IsSuccess());
	FPlatformFileManager::Get().GetPlatformFile().SetReadOnly(*ReadOnly, false);
	return true;
}

#endif
