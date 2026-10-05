#include "Definitions/KnightLootTableDefinition.h"

#include "Assets/KnightAssetManager.h"
#include "Definitions/KnightItemDefinition.h"
#include "Math/RandomStream.h"
#include "Misc/DataValidation.h"

#define LOCTEXT_NAMESPACE "KnightLootTableDefinition"

namespace
{
	FKnightResult ValidateEntries(const TArray<FKnightLootEntry>& Entries)
	{
		if (Entries.IsEmpty() || Entries.Num() > KnightRPG::MaximumLootEntries)
		{
			return FKnightResult::Failure(EKnightError::InvalidData, TEXT("LootTable"),
				LOCTEXT("Count", "A loot table requires a bounded nonempty set of entries."));
		}
		TSet<FPrimaryAssetId> Ids;
		for (const FKnightLootEntry& Entry : Entries)
		{
			if (!KnightIdentity::IsValidDefinitionId(Entry.DefinitionId)
				|| Entry.DefinitionId.PrimaryAssetType != FPrimaryAssetType(TEXT("KnightItem"))
				|| Entry.MinimumCount < 1 || Entry.MaximumCount < Entry.MinimumCount
				|| Entry.MaximumCount > KnightItems::MaximumStackCount || !FMath::IsFinite(Entry.Probability)
				|| Entry.Probability <= 0.0f || Entry.Probability > 1.0f || Ids.Contains(Entry.DefinitionId))
			{
				return FKnightResult::Failure(EKnightError::InvalidData, TEXT("LootTable"),
					LOCTEXT("Entry", "Loot entries require distinct item IDs, bounded counts and a positive finite probability."));
			}
			Ids.Add(Entry.DefinitionId);
		}
		return FKnightResult::Success();
	}
}

UKnightLootTableDefinition::UKnightLootTableDefinition()
{
	DefinitionType = TEXT("KnightLootTable");
}

const TArray<FKnightLootEntry>& UKnightLootTableDefinition::GetEntries() const
{
	return Entries;
}

FKnightResult UKnightLootTableDefinition::TrySetEntries(const TArray<FKnightLootEntry>& InEntries)
{
	const FKnightResult Result = ValidateEntries(InEntries);
	if (Result.IsSuccess())
	{
		Entries = InEntries;
	}
	return Result;
}

FKnightResult UKnightLootTableDefinition::ValidateLootTable() const
{
	const FKnightResult Identity = ValidateIdentity();
	if (!Identity.IsSuccess())
	{
		return Identity;
	}
	if (GetDefinitionType() != TEXT("KnightLootTable"))
	{
		return FKnightResult::Failure(EKnightError::TypeMismatch, GetDefinitionId(),
			LOCTEXT("Type", "Loot definitions require the KnightLootTable primary asset type."));
	}
	return ValidateEntries(Entries);
}

FKnightResult UKnightLootTableDefinition::GenerateLoot(int32 Seed, const FGuid& RewardId,
	TArray<FKnightItemInstance>& OutItems) const
{
	OutItems.Reset();
	const FKnightResult Validation = ValidateLootTable();
	if (!Validation.IsSuccess())
	{
		return Validation;
	}
	UKnightAssetManager* Manager = UKnightAssetManager::GetKnightAssetManager();
	if (!RewardId.IsValid() || Manager == nullptr)
	{
		return FKnightResult::Failure(EKnightError::InvalidState, GetDefinitionId(),
			LOCTEXT("GenerationSetup", "Loot generation requires a stable reward identity and the real typed asset catalog."));
	}
	TArray<FKnightItemInstance> Candidate;
	FRandomStream Random(Seed);
	for (int32 Index = 0; Index < Entries.Num(); ++Index)
	{
		const FKnightLootEntry& Entry = Entries[Index];
		UKnightDefinition* Resolved = nullptr;
		const FKnightResult Resolution = Manager->ResolveDefinition(Entry.DefinitionId,
			UKnightItemDefinition::StaticClass(), Resolved);
		if (!Resolution.IsSuccess())
		{
			return Resolution;
		}
		const UKnightItemDefinition* ItemDefinition = Cast<UKnightItemDefinition>(Resolved);
		if (ItemDefinition == nullptr || !ItemDefinition->ValidateItem().IsSuccess()
			|| Entry.MaximumCount > ItemDefinition->GetMaximumStack())
		{
			return FKnightResult::Failure(EKnightError::InvalidData, GetDefinitionId(),
				LOCTEXT("ResolvedEntry", "An actual loot item definition cannot support this entry's maximum count."));
		}
		const float Roll = Random.FRand();
		const int32 Count = Random.RandRange(Entry.MinimumCount, Entry.MaximumCount);
		if (Entry.Probability < 1.0f && Roll >= Entry.Probability)
		{
			continue;
		}
		FKnightItemInstance Item;
		const FString Identity = FString::Printf(TEXT("%s/%s/%d"), *GetPrimaryAssetId().ToString(),
			*RewardId.ToString(EGuidFormats::Digits), Index);
		Item.InstanceId = FGuid::NewDeterministicGuid(Identity, static_cast<uint64>(static_cast<uint32>(Seed)));
		Item.DefinitionId = Entry.DefinitionId;
		Item.Count = Count;
		Candidate.Add(Item);
	}
	OutItems = MoveTemp(Candidate);
	return FKnightResult::Success();
}

#if WITH_EDITOR
EDataValidationResult UKnightLootTableDefinition::IsDataValid(FDataValidationContext& Context) const
{
	const EDataValidationResult Parent = Super::IsDataValid(Context);
	const FKnightResult Result = ValidateLootTable();
	if (!Result.IsSuccess())
	{
		Context.AddError(Result.Message);
		return EDataValidationResult::Invalid;
	}
	return Parent == EDataValidationResult::Invalid ? Parent : EDataValidationResult::Valid;
}
#endif

#undef LOCTEXT_NAMESPACE
