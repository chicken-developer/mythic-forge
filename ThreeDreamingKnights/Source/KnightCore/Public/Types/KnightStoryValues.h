#pragma once

#include "CoreMinimal.h"
#include "KnightStoryValues.generated.h"

USTRUCT()
struct KNIGHTCORE_API FKnightStoryCount
{
	GENERATED_BODY()
	UPROPERTY(SaveGame)
	FName Key;
	UPROPERTY(SaveGame)
	int32 Value = 0;
};

/** Reflected arrays support standard UE replication; map-like access stays local to the owner. */
USTRUCT()
struct KNIGHTCORE_API FKnightStoryCountMap
{
	GENERATED_BODY()
	UPROPERTY(SaveGame)
	TArray<FKnightStoryCount> Entries;

	int32 Num() const
	{
		return Entries.Num();
	}
	const int32* Find(FName Key) const
	{
		const FKnightStoryCount* Row = Entries.FindByPredicate([Key](const FKnightStoryCount& Entry)
		{
			return Entry.Key == Key;
		});
		return Row != nullptr ? &Row->Value : nullptr;
	}
	int32 FindRef(FName Key) const
	{
		const int32* Value = Find(Key);
		return Value != nullptr ? *Value : 0;
	}
	int32& FindChecked(FName Key)
	{
		FKnightStoryCount* Row = Entries.FindByPredicate([Key](const FKnightStoryCount& Entry)
		{
			return Entry.Key == Key;
		});
		check(Row != nullptr);
		return Row->Value;
	}
	const int32& FindChecked(FName Key) const
	{
		const int32* Value = Find(Key);
		check(Value != nullptr);
		return *Value;
	}
	void Add(FName Key, int32 Value)
	{
		if (Find(Key) != nullptr)
		{
			FindChecked(Key) = Value;
			return;
		}
		FKnightStoryCount& Row = Entries.AddDefaulted_GetRef();
		Row.Key = Key;
		Row.Value = Value;
	}
};

USTRUCT()
struct KNIGHTCORE_API FKnightStoryChoice
{
	GENERATED_BODY()
	UPROPERTY(SaveGame)
	FName Key;
	UPROPERTY(SaveGame)
	FName Value;
};

USTRUCT()
struct KNIGHTCORE_API FKnightStoryChoiceMap
{
	GENERATED_BODY()
	UPROPERTY(SaveGame)
	TArray<FKnightStoryChoice> Entries;

	const FName* Find(FName Key) const
	{
		const FKnightStoryChoice* Row = Entries.FindByPredicate([Key](const FKnightStoryChoice& Entry)
		{
			return Entry.Key == Key;
		});
		return Row != nullptr ? &Row->Value : nullptr;
	}
	void Add(FName Key, FName Value)
	{
		FKnightStoryChoice* Row = Entries.FindByPredicate([Key](const FKnightStoryChoice& Entry)
		{
			return Entry.Key == Key;
		});
		if (Row == nullptr)
		{
			Row = &Entries.AddDefaulted_GetRef();
			Row->Key = Key;
		}
		Row->Value = Value;
	}
};
