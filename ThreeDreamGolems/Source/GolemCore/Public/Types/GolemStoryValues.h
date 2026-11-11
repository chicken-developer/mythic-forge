#pragma once

#include "CoreMinimal.h"
#include "GolemStoryValues.generated.h"

USTRUCT()
struct GOLEMCORE_API FGolemStoryCount
{
	GENERATED_BODY()
	UPROPERTY(SaveGame)
	FName Key;
	UPROPERTY(SaveGame)
	int32 Value = 0;
};

/** Reflected arrays support standard UE replication; map-like access stays local to the owner. */
USTRUCT()
struct GOLEMCORE_API FGolemStoryCountMap
{
	GENERATED_BODY()
	UPROPERTY(SaveGame)
	TArray<FGolemStoryCount> Entries;

	int32 Num() const
	{
		return Entries.Num();
	}
	const int32* Find(FName Key) const
	{
		const FGolemStoryCount* Row = Entries.FindByPredicate([Key](const FGolemStoryCount& Entry)
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
		FGolemStoryCount* Row = Entries.FindByPredicate([Key](const FGolemStoryCount& Entry)
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
		FGolemStoryCount& Row = Entries.AddDefaulted_GetRef();
		Row.Key = Key;
		Row.Value = Value;
	}
};

USTRUCT()
struct GOLEMCORE_API FGolemStoryChoice
{
	GENERATED_BODY()
	UPROPERTY(SaveGame)
	FName Key;
	UPROPERTY(SaveGame)
	FName Value;
};

USTRUCT()
struct GOLEMCORE_API FGolemStoryChoiceMap
{
	GENERATED_BODY()
	UPROPERTY(SaveGame)
	TArray<FGolemStoryChoice> Entries;

	const FName* Find(FName Key) const
	{
		const FGolemStoryChoice* Row = Entries.FindByPredicate([Key](const FGolemStoryChoice& Entry)
		{
			return Entry.Key == Key;
		});
		return Row != nullptr ? &Row->Value : nullptr;
	}
	void Add(FName Key, FName Value)
	{
		FGolemStoryChoice* Row = Entries.FindByPredicate([Key](const FGolemStoryChoice& Entry)
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
