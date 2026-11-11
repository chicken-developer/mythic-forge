#include "Checkpoint/GolemCheckpointSubsystem.h"

#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"

namespace
{
	constexpr uint32 CheckpointMagic = 0x4B4E4336;
	constexpr int32 MaximumCheckpointBytes = 16 * 1024 * 1024;

	bool FlushBytes(const FString& Path, const TArray<uint8>& Bytes)
	{
		TUniquePtr<IFileHandle> Handle(FPlatformFileManager::Get().GetPlatformFile().OpenWrite(*Path));
		return Handle.IsValid() && Handle->Write(Bytes.GetData(), Bytes.Num()) && Handle->Flush(true);
	}

	FGolemResult Decode(const TArray<uint8>& Bytes, FGolemCampaignCheckpoint& OutCheckpoint)
	{
		if (Bytes.Num() < 28 || Bytes.Num() > MaximumCheckpointBytes)
		{
			return FGolemResult::Failure(EGolemError::InvalidData, TEXT("CheckpointLength"), FText::GetEmpty());
		}
		FMemoryReader Reader(Bytes);
		uint32 Magic = 0;
		int32 PayloadLength = 0;
		uint8 StoredHash[20];
		Reader << Magic;
		Reader << PayloadLength;
		Reader.Serialize(StoredHash, sizeof(StoredHash));
		if (Reader.IsError() || Magic != CheckpointMagic || PayloadLength != Bytes.Num() - 28)
		{
			return FGolemResult::Failure(EGolemError::InvalidData, TEXT("CheckpointEnvelope"), FText::GetEmpty());
		}
		TArray<uint8> Payload;
		Payload.Append(Bytes.GetData() + 28, PayloadLength);
		uint8 ActualHash[20];
		FSHA1::HashBuffer(Payload.GetData(), Payload.Num(), ActualHash);
		if (FMemory::Memcmp(StoredHash, ActualHash, sizeof(ActualHash)) != 0)
		{
			return FGolemResult::Failure(EGolemError::InvalidData, TEXT("CheckpointChecksum"), FText::GetEmpty());
		}
		UGolemCheckpointSave* Save = Cast<UGolemCheckpointSave>(UGameplayStatics::LoadGameFromMemory(Payload));
		if (Save == nullptr)
		{
			return FGolemResult::Failure(EGolemError::LoadFailed, TEXT("CheckpointDecode"), FText::GetEmpty());
		}
		const FGolemResult Result = Save->Checkpoint.ValidateScope();
		if (Result.IsSuccess())
		{
			OutCheckpoint = Save->Checkpoint;
		}
		return Result;
	}
}

FString UGolemCheckpointSubsystem::GetCheckpointPath(const FName CampaignId, const FName ScenarioKind)
{
	if (!GolemIdentity::IsValidIdentifier(CampaignId) || !GolemIdentity::IsValidIdentifier(ScenarioKind))
	{
		return FString();
	}
	return FPaths::ProjectSavedDir() / TEXT("CampaignCheckpoints")
		/ CampaignId.ToString() / (ScenarioKind.ToString() + TEXT(".golem"));
}

FGolemResult UGolemCheckpointSubsystem::WriteCheckpointFile(const FString& Path,
	const FGolemCampaignCheckpoint& Checkpoint, const bool bInterruptBeforeReplace)
{
	FGolemResult Result = Checkpoint.ValidateScope();
	if (!Result.IsSuccess() || Path.IsEmpty())
	{
		return Result.IsSuccess() ? FGolemResult::Failure(EGolemError::InvalidId,
			TEXT("CheckpointPath"), FText::GetEmpty()) : Result;
	}
	UGolemCheckpointSave* Save = NewObject<UGolemCheckpointSave>();
	Save->Checkpoint = Checkpoint;
	TArray<uint8> Payload;
	if (!UGameplayStatics::SaveGameToMemory(Save, Payload) || Payload.Num() > MaximumCheckpointBytes - 28)
	{
		return FGolemResult::Failure(EGolemError::InvalidData, TEXT("CheckpointEncode"), FText::GetEmpty());
	}
	uint8 Hash[20];
	FSHA1::HashBuffer(Payload.GetData(), Payload.Num(), Hash);
	TArray<uint8> Bytes;
	FMemoryWriter Writer(Bytes);
	uint32 Magic = CheckpointMagic;
	int32 Length = Payload.Num();
	Writer << Magic;
	Writer << Length;
	Writer.Serialize(Hash, sizeof(Hash));
	Writer.Serialize(Payload.GetData(), Payload.Num());
	const FString Temp = Path + TEXT(".tmp");
	const FString Backup = Path + TEXT(".bak");
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
	FGolemCampaignCheckpoint Verified;
	if (!FlushBytes(Temp, Bytes) || !ReadCheckpointFile(Temp, Verified).IsSuccess()
		|| Verified.CheckpointId != Checkpoint.CheckpointId || bInterruptBeforeReplace)
	{
		return FGolemResult::Failure(EGolemError::LoadFailed, TEXT("CheckpointTempWrite"),
			NSLOCTEXT("GolemSave", "WriteFailed", "Checkpoint write failed. The previous checkpoint is preserved."));
	}
	if (IFileManager::Get().FileExists(*Path))
	{
		TArray<uint8> Previous;
		FGolemCampaignCheckpoint PreviousCheckpoint;
		const bool bCurrentValid = FFileHelper::LoadFileToArray(Previous, *Path)
			&& Decode(Previous, PreviousCheckpoint).IsSuccess();
		if (!bCurrentValid)
		{
			if (!FFileHelper::LoadFileToArray(Previous, *Backup) || !Decode(Previous, PreviousCheckpoint).IsSuccess())
			{
				return FGolemResult::Failure(EGolemError::LoadFailed, TEXT("CheckpointRecovery"), FText::GetEmpty());
			}
		}
		if (bCurrentValid && !FlushBytes(Backup, Previous))
		{
			return FGolemResult::Failure(EGolemError::LoadFailed, TEXT("CheckpointBackup"), FText::GetEmpty());
		}
	}
	if (!IFileManager::Get().Move(*Path, *Temp, true, false, false, true)
		|| !ReadCheckpointFile(Path, Verified).IsSuccess() || Verified.CheckpointId != Checkpoint.CheckpointId)
	{
		return FGolemResult::Failure(EGolemError::LoadFailed, TEXT("CheckpointReplace"), FText::GetEmpty());
	}
	return FGolemResult::Success();
}

FGolemResult UGolemCheckpointSubsystem::ReadCheckpointFile(const FString& Path,
	FGolemCampaignCheckpoint& OutCheckpoint)
{
	TArray<uint8> Bytes;
	if (!FFileHelper::LoadFileToArray(Bytes, *Path))
	{
		return FGolemResult::Failure(EGolemError::NotFound, TEXT("CheckpointFile"),
			NSLOCTEXT("GolemSave", "NoCheckpoint", "No readable checkpoint exists for this profile."));
	}
	return Decode(Bytes, OutCheckpoint);
}

FGolemResult UGolemCheckpointSubsystem::SaveCheckpoint(const FGolemCampaignCheckpoint& Checkpoint)
{
	return WriteCheckpointFile(GetCheckpointPath(Checkpoint.Quest.CampaignId, Checkpoint.Quest.ScenarioKind), Checkpoint);
}

FGolemResult UGolemCheckpointSubsystem::LoadCheckpoint(const FName CampaignId, const FName ScenarioKind,
	FGolemCampaignCheckpoint& OutCheckpoint)
{
	const FString Path = GetCheckpointPath(CampaignId, ScenarioKind);
	FGolemCampaignCheckpoint Candidate;
	FGolemResult Result = ReadCheckpointFile(Path, Candidate);
	if (!Result.IsSuccess())
	{
		Result = ReadCheckpointFile(Path + TEXT(".bak"), Candidate);
	}
	if (!Result.IsSuccess())
	{
		return Result;
	}
	if (Candidate.Quest.CampaignId != CampaignId || Candidate.Quest.ScenarioKind != ScenarioKind)
	{
		return FGolemResult::Failure(EGolemError::Conflict, CampaignId,
			NSLOCTEXT("GolemSave", "ProfileMismatch", "This checkpoint belongs to another campaign or scenario."));
	}
	OutCheckpoint = MoveTemp(Candidate);
	return FGolemResult::Success();
}

FGolemResult UGolemCheckpointSubsystem::StageCheckpoint(const FGolemCampaignCheckpoint& Checkpoint)
{
	const FGolemResult Result = Checkpoint.ValidateScope();
	if (Result.IsSuccess())
	{
		Staged = Checkpoint;
		bHasStagedCheckpoint = true;
	}
	return Result;
}

const FGolemCampaignCheckpoint* UGolemCheckpointSubsystem::GetStagedCheckpoint() const
{
	return bHasStagedCheckpoint ? &Staged : nullptr;
}

void UGolemCheckpointSubsystem::ConsumeStagedCheckpoint()
{
	bHasStagedCheckpoint = false;
	Staged = FGolemCampaignCheckpoint();
}
