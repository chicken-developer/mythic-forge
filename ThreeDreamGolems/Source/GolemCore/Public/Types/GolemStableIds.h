#pragma once

#include "CoreMinimal.h"
#include "UObject/PrimaryAssetId.h"
#include "GolemStableIds.generated.h"

namespace GolemIdentity
{
	/** ASCII identifiers remain independent of package names, localization and UObject lifetime. */
	GOLEMCORE_API bool IsValidIdentifier(FName Identifier);
	GOLEMCORE_API bool IsValidDefinitionId(const FPrimaryAssetId& DefinitionId);
}

USTRUCT(BlueprintType)
struct GOLEMCORE_API FGolemDefinitionId
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Core|Identity")
	FPrimaryAssetId Value;

	FGolemDefinitionId() = default;
	explicit FGolemDefinitionId(const FPrimaryAssetId& InValue);
	bool IsValid() const;
	bool operator==(const FGolemDefinitionId& Other) const;
	friend uint32 GetTypeHash(const FGolemDefinitionId& Id)
	{
		return GetTypeHash(Id.Value);
	}
};

/** Persistent instance identity; replicated/saved instead of a UObject or actor address. */
USTRUCT(BlueprintType)
struct GOLEMCORE_API FGolemEntityId
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Golem|Core|Identity")
	FGuid Value;

	FGolemEntityId() = default;
	explicit FGolemEntityId(const FGuid& InValue);
	static FGolemEntityId Create();
	bool IsValid() const;
	bool operator==(const FGolemEntityId& Other) const;
	friend uint32 GetTypeHash(const FGolemEntityId& Id)
	{
		return GetTypeHash(Id.Value);
	}
};
