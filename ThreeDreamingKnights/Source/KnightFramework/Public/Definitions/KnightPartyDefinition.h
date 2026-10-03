#pragma once

#include "CoreMinimal.h"
#include "Assets/KnightDefinition.h"
#include "KnightPartyDefinition.generated.h"

UCLASS(BlueprintType)
class KNIGHTFRAMEWORK_API UKnightPartyDefinition : public UKnightDefinition
{
	GENERATED_BODY()

public:
	UKnightPartyDefinition();
	const TArray<FPrimaryAssetId>& GetHeroDefinitions() const;
	FKnightResult ValidateParty() const;
	FKnightResult TrySetHeroDefinitions(const TArray<FPrimaryAssetId>& InHeroDefinitions);

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

private:
	static FKnightResult ValidateHeroIds(const TArray<FPrimaryAssetId>& HeroIds);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Knight|Framework|Party",
		meta = (AllowPrivateAccess = "true"))
	TArray<FPrimaryAssetId> HeroDefinitions;
};
