#include "Checkpoint/KnightCheckpointSubsystem.h"

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

	FKnightResult Decode(const TArray<uint8>& Bytes, FKnightCampaignCheckpoint& OutCheckpoint)
	{
		if (Bytes.Num() < 28 || Bytes.Num() > MaximumCheckpointBytes)
		{
			return FKnightResult::Failure(EKnightError::InvalidData, TEXT("CheckpointLength"), FText::GetEmpty());
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
			return FKnightResult::Failure(EKnightError::InvalidData, TEXT("CheckpointEnvelope"), FText::GetEmpty());
		}
		TArray<uint8> Payload;
		Payload.Append(Bytes.GetData() + 28, PayloadLength);
		uint8 ActualHash[20];
		FSHA1::HashBuffer(Payload.GetData(), Payload.Num(), ActualHash);
		if (FMemory::Memcmp(StoredHash, ActualHash, sizeof(ActualHash)) != 0)
		{
			return FKnightResult::Failure(EKnightError::InvalidData, TEXT("CheckpointChecksum"), FText::GetEmpty());
		}
		UKnightCheckpointSave* Save = Cast<UKnightCheckpointSave>(UGameplayStatics::LoadGameFromMemory(Payload));
		if (Save == nullptr)
		{
			return FKnightResult::Failure(EKnightError::LoadFailed, TEXT("CheckpointDecode"), FText::GetEmpty());
		}
		const FKnightResult Result = Save->Checkpoint.ValidateScope();
		if (Result.IsSuccess())
		{
			OutCheckpoint = Save->Checkpoint;
		}
		return Result;
	}
}

FString UKnightCheckpointSubsystem::GetCheckpointPath(const FName CampaignId, const FName ScenarioKind)
{
	if (!KnightIdentity::IsValidIdentifier(CampaignId) || !KnightIdentity::IsValidIdentifier(ScenarioKind))
	{
		return FString();
	}
	return FPaths::ProjectSavedDir() / TEXT("CampaignCheckpoints")
		/ CampaignId.ToString() / (ScenarioKind.ToString() + TEXT(".knight"));
}

FKnightResult UKnightCheckpointSubsystem::WriteCheckpointFile(const FString& Path,
	const FKnightCampaignCheckpoint& Checkpoint, const bool bInterruptBeforeReplace)
{
	FKnightResult Result = Checkpoint.ValidateScope();
	if (!Result.IsSuccess() || Path.IsEmpty())
	{
		return Result.IsSuccess() ? FKnightResult::Failure(EKnightError::InvalidId,
			TEXT("CheckpointPath"), FText::GetEmpty()) : Result;
	}
	UKnightCheckpointSave* Save = NewObject<UKnightCheckpointSave>();
	Save->Checkpoint = Checkpoint;
	TArray<uint8> Payload;
	if (!UGameplayStatics::SaveGameToMemory(Save, Payload) || Payload.Num() > MaximumCheckpointBytes - 28)
	{
		return FKnightResult::Failure(EKnightError::InvalidData, TEXT("CheckpointEncode"), FText::GetEmpty());
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
	FKnightCampaignCheckpoint Verified;
	if (!FlushBytes(Temp, Bytes) || !ReadCheckpointFile(Temp, Verified).IsSuccess()
		|| Verified.CheckpointId != Checkpoint.CheckpointId || bInterruptBeforeReplace)
	{
		return FKnightResult::Failure(EKnightError::LoadFailed, TEXT("CheckpointTempWrite"),
			NSLOCTEXT("KnightSave", "WriteFailed", "Checkpoint write failed. The previous checkpoint is preserved."));
	}
	if (IFileManager::Get().FileExists(*Path))
	{
		TArray<uint8> Previous;
		FKnightCampaignCheckpoint PreviousCheckpoint;
		const bool bCurrentValid = FFileHelper::LoadFileToArray(Previous, *Path)
			&& Decode(Previous, PreviousCheckpoint).IsSuccess();
		if (!bCurrentValid)
		{
			if (!FFileHelper::LoadFileToArray(Previous, *Backup) || !Decode(Previous, PreviousCheckpoint).IsSuccess())
			{
				return FKnightResult::Failure(EKnightError::LoadFailed, TEXT("CheckpointRecovery"), FText::GetEmpty());
			}
		}
		if (bCurrentValid && !FlushBytes(Backup, Previous))
		{
			return FKnightResult::Failure(EKnightError::LoadFailed, TEXT("CheckpointBackup"), FText::GetEmpty());
		}
	}
	if (!IFileManager::Get().Move(*Path, *Temp, true, false, false, true)
		|| !ReadCheckpointFile(Path, Verified).IsSuccess() || Verified.CheckpointId != Checkpoint.CheckpointId)
	{
		return FKnightResult::Failure(EKnightError::LoadFailed, TEXT("CheckpointReplace"), FText::GetEmpty());
	}
	return FKnightResult::Success();
}

FKnightResult UKnightCheckpointSubsystem::ReadCheckpointFile(const FString& Path,
	FKnightCampaignCheckpoint& OutCheckpoint)
{
	TArray<uint8> Bytes;
	if (!FFileHelper::LoadFileToArray(Bytes, *Path))
	{
		return FKnightResult::Failure(EKnightError::NotFound, TEXT("CheckpointFile"),
			NSLOCTEXT("KnightSave", "NoCheckpoint", "No readable checkpoint exists for this profile."));
	}
	return Decode(Bytes, OutCheckpoint);
}

FKnightResult UKnightCheckpointSubsystem::SaveCheckpoint(const FKnightCampaignCheckpoint& Checkpoint)
{
	return WriteCheckpointFile(GetCheckpointPath(Checkpoint.Quest.CampaignId, Checkpoint.Quest.ScenarioKind), Checkpoint);
}

FKnightResult UKnightCheckpointSubsystem::LoadCheckpoint(const FName CampaignId, const FName ScenarioKind,
	FKnightCampaignCheckpoint& OutCheckpoint)
{
	const FString Path = GetCheckpointPath(CampaignId, ScenarioKind);
	FKnightCampaignCheckpoint Candidate;
	FKnightResult Result = ReadCheckpointFile(Path, Candidate);
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
		return FKnightResult::Failure(EKnightError::Conflict, CampaignId,
			NSLOCTEXT("KnightSave", "ProfileMismatch", "This checkpoint belongs to another campaign or scenario."));
	}
	OutCheckpoint = MoveTemp(Candidate);
	return FKnightResult::Success();
}

FKnightResult UKnightCheckpointSubsystem::StageCheckpoint(const FKnightCampaignCheckpoint& Checkpoint)
{
	const FKnightResult Result = Checkpoint.ValidateScope();
	if (Result.IsSuccess())
	{
		Staged = Checkpoint;
		bHasStagedCheckpoint = true;
	}
	return Result;
}

const FKnightCampaignCheckpoint* UKnightCheckpointSubsystem::GetStagedCheckpoint() const
{
	return bHasStagedCheckpoint ? &Staged : nullptr;
}

void UKnightCheckpointSubsystem::ConsumeStagedCheckpoint()
{
	bHasStagedCheckpoint = false;
	Staged = FKnightCampaignCheckpoint();
}
