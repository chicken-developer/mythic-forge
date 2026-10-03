#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Types/KnightResult.h"
#include "KnightDefinition.generated.h"

/** Definitions persist a deliberately authored ID. Renaming or moving an asset does not change identity. */
UCLASS(BlueprintType)
class KNIGHTCORE_API UKnightDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
	FName GetDefinitionId() const;
	FName GetDefinitionType() const;
	const FText& GetDisplayName() const;
	FKnightResult ValidateIdentity() const;

	/** Authoring API; rejected values leave the original identity intact. */
	bool TrySetIdentity(FName InDefinitionId, FName InDefinitionType = FName(TEXT("KnightDefinition")),
		FKnightResult* OutFailure = nullptr);
	void SetDisplayName(const FText& InDisplayName);

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, AssetRegistrySearchable, Category = "Knight|Core|Identity")
	FName DefinitionId;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, AssetRegistrySearchable, Category = "Knight|Core|Identity")
	FName DefinitionType = TEXT("KnightDefinition");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Knight|Core|Presentation")
	FText DisplayName;
};
