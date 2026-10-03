#pragma once

#include "CoreMinimal.h"
#include "UObject/PrimaryAssetId.h"
#include "KnightStableIds.generated.h"

namespace KnightIdentity
{
	/** ASCII identifiers remain independent of package names, localization and UObject lifetime. */
	KNIGHTCORE_API bool IsValidIdentifier(FName Identifier);
	KNIGHTCORE_API bool IsValidDefinitionId(const FPrimaryAssetId& DefinitionId);
}

USTRUCT(BlueprintType)
struct KNIGHTCORE_API FKnightDefinitionId
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Knight|Core|Identity")
	FPrimaryAssetId Value;

	FKnightDefinitionId() = default;
	explicit FKnightDefinitionId(const FPrimaryAssetId& InValue);
	bool IsValid() const;
	bool operator==(const FKnightDefinitionId& Other) const;
	friend uint32 GetTypeHash(const FKnightDefinitionId& Id)
	{
		return GetTypeHash(Id.Value);
	}
};

/** Persistent instance identity; replicated/saved instead of a UObject or actor address. */
USTRUCT(BlueprintType)
struct KNIGHTCORE_API FKnightEntityId
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Knight|Core|Identity")
	FGuid Value;

	FKnightEntityId() = default;
	explicit FKnightEntityId(const FGuid& InValue);
	static FKnightEntityId Create();
	bool IsValid() const;
	bool operator==(const FKnightEntityId& Other) const;
	friend uint32 GetTypeHash(const FKnightEntityId& Id)
	{
		return GetTypeHash(Id.Value);
	}
};
