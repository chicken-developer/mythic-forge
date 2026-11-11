#pragma once

#include "CoreMinimal.h"
#include "Engine/AssetManager.h"
#include "Types/GolemResult.h"
#include "GolemAssetManager.generated.h"

class UGolemDefinition;

UCLASS()
class GOLEMCORE_API UGolemAssetManager : public UAssetManager
{
	GENERATED_BODY()

public:
	static UGolemAssetManager* GetGolemAssetManager();
	virtual void StartInitialLoading() override;

	/** Rebuilds the catalog from real registry assets; duplicate identities are quarantined. */
	FGolemResult ValidateDefinitionCatalog();
	FGolemResult ResolveDefinition(const FPrimaryAssetId& DefinitionId, UGolemDefinition*& OutDefinition);
	FGolemResult ResolveDefinition(const FPrimaryAssetId& DefinitionId, UClass* ExpectedClass,
		UGolemDefinition*& OutDefinition);
	const TArray<FGolemResult>& GetCatalogErrors() const;
	int32 GetDefinitionCount() const;

	/** Shared validator for authored definition batches, including fixtures imported by editor tooling. */
	static TArray<FGolemResult> ValidateDefinitionIdentities(TConstArrayView<UGolemDefinition*> Definitions);

private:
	TMap<FPrimaryAssetId, FSoftObjectPath> DefinitionCatalog;
	TSet<FPrimaryAssetId> DuplicateIds;

	UPROPERTY(Transient)
	TMap<FPrimaryAssetId, TObjectPtr<UGolemDefinition>> ResolvedDefinitions;

	UPROPERTY(Transient)
	TArray<FGolemResult> CatalogErrors;
};
