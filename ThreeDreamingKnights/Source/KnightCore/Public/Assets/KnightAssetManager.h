#pragma once

#include "CoreMinimal.h"
#include "Engine/AssetManager.h"
#include "Types/KnightResult.h"
#include "KnightAssetManager.generated.h"

class UKnightDefinition;

UCLASS()
class KNIGHTCORE_API UKnightAssetManager : public UAssetManager
{
	GENERATED_BODY()

public:
	static UKnightAssetManager* GetKnightAssetManager();
	virtual void StartInitialLoading() override;

	/** Rebuilds the catalog from real registry assets; duplicate identities are quarantined. */
	FKnightResult ValidateDefinitionCatalog();
	FKnightResult ResolveDefinition(const FPrimaryAssetId& DefinitionId, UKnightDefinition*& OutDefinition);
	FKnightResult ResolveDefinition(const FPrimaryAssetId& DefinitionId, UClass* ExpectedClass,
		UKnightDefinition*& OutDefinition);
	const TArray<FKnightResult>& GetCatalogErrors() const;
	int32 GetDefinitionCount() const;

	/** Shared validator for authored definition batches, including fixtures imported by editor tooling. */
	static TArray<FKnightResult> ValidateDefinitionIdentities(TConstArrayView<UKnightDefinition*> Definitions);

private:
	TMap<FPrimaryAssetId, FSoftObjectPath> DefinitionCatalog;
	TSet<FPrimaryAssetId> DuplicateIds;

	UPROPERTY(Transient)
	TMap<FPrimaryAssetId, TObjectPtr<UKnightDefinition>> ResolvedDefinitions;

	UPROPERTY(Transient)
	TArray<FKnightResult> CatalogErrors;
};
