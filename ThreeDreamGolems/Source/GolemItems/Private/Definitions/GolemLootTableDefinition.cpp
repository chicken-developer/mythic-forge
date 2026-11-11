#include "Definitions/GolemLootTableDefinition.h"

#include "Assets/GolemAssetManager.h"
#include "Definitions/GolemItemDefinition.h"
#include "Math/RandomStream.h"
#include "Misc/DataValidation.h"

#define LOCTEXT_NAMESPACE "GolemLootTableDefinition"

namespace
{
	FGolemResult ValidateEntries(const TArray<FGolemLootEntry>& Entries)
	{
		if (Entries.IsEmpty() || Entries.Num() > GolemRPG::MaximumLootEntries)
		{
			return FGolemResult::Failure(EGolemError::InvalidData, TEXT("LootTable"),
				LOCTEXT("Count", "A loot table requires a bounded nonempty set of entries."));
		}
		TSet<FPrimaryAssetId> Ids;
		for (const FGolemLootEntry& Entry : Entries)
		{
			if (!GolemIdentity::IsValidDefinitionId(Entry.DefinitionId)
				|| Entry.DefinitionId.PrimaryAssetType != FPrimaryAssetType(TEXT("GolemItem"))
				|| Entry.MinimumCount < 1 || Entry.MaximumCount < Entry.MinimumCount
				|| Entry.MaximumCount > GolemItems::MaximumStackCount || !FMath::IsFinite(Entry.Probability)
				|| Entry.Probability <= 0.0f || Entry.Probability > 1.0f || Ids.Contains(Entry.DefinitionId))
			{
				return FGolemResult::Failure(EGolemError::InvalidData, TEXT("LootTable"),
					LOCTEXT("Entry", "Loot entries require distinct item IDs, bounded counts and a positive finite probability."));
			}
			Ids.Add(Entry.DefinitionId);
		}
		return FGolemResult::Success();
	}
}

UGolemLootTableDefinition::UGolemLootTableDefinition()
{
	DefinitionType = TEXT("GolemLootTable");
}

const TArray<FGolemLootEntry>& UGolemLootTableDefinition::GetEntries() const
{
	return Entries;
}

FGolemResult UGolemLootTableDefinition::TrySetEntries(const TArray<FGolemLootEntry>& InEntries)
{
	const FGolemResult Result = ValidateEntries(InEntries);
	if (Result.IsSuccess())
	{
		Entries = InEntries;
	}
	return Result;
}

FGolemResult UGolemLootTableDefinition::ValidateLootTable() const
{
	const FGolemResult Identity = ValidateIdentity();
	if (!Identity.IsSuccess())
	{
		return Identity;
	}
	if (GetDefinitionType() != TEXT("GolemLootTable"))
	{
		return FGolemResult::Failure(EGolemError::TypeMismatch, GetDefinitionId(),
			LOCTEXT("Type", "Loot definitions require the GolemLootTable primary asset type."));
	}
	return ValidateEntries(Entries);
}

FGolemResult UGolemLootTableDefinition::GenerateLoot(int32 Seed, const FGuid& RewardId,
	TArray<FGolemItemInstance>& OutItems) const
{
	OutItems.Reset();
	const FGolemResult Validation = ValidateLootTable();
	if (!Validation.IsSuccess())
	{
		return Validation;
	}
	UGolemAssetManager* Manager = UGolemAssetManager::GetGolemAssetManager();
	if (!RewardId.IsValid() || Manager == nullptr)
	{
		return FGolemResult::Failure(EGolemError::InvalidState, GetDefinitionId(),
			LOCTEXT("GenerationSetup", "Loot generation requires a stable reward identity and the real typed asset catalog."));
	}
	TArray<FGolemItemInstance> Candidate;
	FRandomStream Random(Seed);
	for (int32 Index = 0; Index < Entries.Num(); ++Index)
	{
		const FGolemLootEntry& Entry = Entries[Index];
		UGolemDefinition* Resolved = nullptr;
		const FGolemResult Resolution = Manager->ResolveDefinition(Entry.DefinitionId,
			UGolemItemDefinition::StaticClass(), Resolved);
		if (!Resolution.IsSuccess())
		{
			return Resolution;
		}
		const UGolemItemDefinition* ItemDefinition = Cast<UGolemItemDefinition>(Resolved);
		if (ItemDefinition == nullptr || !ItemDefinition->ValidateItem().IsSuccess()
			|| Entry.MaximumCount > ItemDefinition->GetMaximumStack())
		{
			return FGolemResult::Failure(EGolemError::InvalidData, GetDefinitionId(),
				LOCTEXT("ResolvedEntry", "An actual loot item definition cannot support this entry's maximum count."));
		}
		const float Roll = Random.FRand();
		const int32 Count = Random.RandRange(Entry.MinimumCount, Entry.MaximumCount);
		if (Entry.Probability < 1.0f && Roll >= Entry.Probability)
		{
			continue;
		}
		FGolemItemInstance Item;
		const FString Identity = FString::Printf(TEXT("%s/%s/%d"), *GetPrimaryAssetId().ToString(),
			*RewardId.ToString(EGuidFormats::Digits), Index);
		Item.InstanceId = FGuid::NewDeterministicGuid(Identity, static_cast<uint64>(static_cast<uint32>(Seed)));
		Item.DefinitionId = Entry.DefinitionId;
		Item.Count = Count;
		Candidate.Add(Item);
	}
	OutItems = MoveTemp(Candidate);
	return FGolemResult::Success();
}

#if WITH_EDITOR
EDataValidationResult UGolemLootTableDefinition::IsDataValid(FDataValidationContext& Context) const
{
	const EDataValidationResult Parent = Super::IsDataValid(Context);
	const FGolemResult Result = ValidateLootTable();
	if (!Result.IsSuccess())
	{
		Context.AddError(Result.Message);
		return EDataValidationResult::Invalid;
	}
	return Parent == EDataValidationResult::Invalid ? Parent : EDataValidationResult::Valid;
}
#endif

#undef LOCTEXT_NAMESPACE
