#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Types/GolemResult.h"
#include "GolemDefinition.generated.h"

/** Definitions persist a deliberately authored ID. Renaming or moving an asset does not change identity. */
UCLASS(BlueprintType)
class GOLEMCORE_API UGolemDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
	FName GetDefinitionId() const;
	FName GetDefinitionType() const;
	const FText& GetDisplayName() const;
	FGolemResult ValidateIdentity() const;

	/** Authoring API; rejected values leave the original identity intact. */
	bool TrySetIdentity(FName InDefinitionId, FName InDefinitionType = FName(TEXT("KnightDefinition")),
		FGolemResult* OutFailure = nullptr);
	void SetDisplayName(const FText& InDisplayName);

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, AssetRegistrySearchable, Category = "Golem|Core|Identity")
	FName DefinitionId;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, AssetRegistrySearchable, Category = "Golem|Core|Identity")
	FName DefinitionType = TEXT("KnightDefinition");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Core|Presentation")
	FText DisplayName;
};
