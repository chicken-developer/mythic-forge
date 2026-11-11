#pragma once

#include "CoreMinimal.h"
#include "Assets/GolemDefinition.h"
#include "GolemPartyDefinition.generated.h"

UCLASS(BlueprintType)
class GOLEMFRAMEWORK_API UGolemPartyDefinition : public UGolemDefinition
{
	GENERATED_BODY()

public:
	UGolemPartyDefinition();
	const TArray<FPrimaryAssetId>& GetHeroDefinitions() const;
	FGolemResult ValidateParty() const;
	FGolemResult TrySetHeroDefinitions(const TArray<FPrimaryAssetId>& InHeroDefinitions);

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

private:
	static FGolemResult ValidateHeroIds(const TArray<FPrimaryAssetId>& HeroIds);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Framework|Party",
		meta = (AllowPrivateAccess = "true"))
	TArray<FPrimaryAssetId> HeroDefinitions;
};
