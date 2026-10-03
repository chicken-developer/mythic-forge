#pragma once

#include "CoreMinimal.h"
#include "Assets/KnightDefinition.h"
#include "UObject/PrimaryAssetId.h"
#include "KnightHeroDefinition.generated.h"

/** Knight data is selected by stable definition ID, independently of human or companion ownership. */
UCLASS(BlueprintType)
class KNIGHTFRAMEWORK_API UKnightHeroDefinition : public UKnightDefinition
{
	GENERATED_BODY()

public:
	UKnightHeroDefinition();
	FLinearColor GetBodyColor() const;
	float GetWalkSpeed() const;
	float GetSprintSpeed() const;
	FVector GetFormationOffset() const;
	float GetFollowDistance() const;
	FPrimaryAssetId GetDefaultAbilitySetId() const;
	FKnightResult ValidateHero() const;
	FKnightResult TrySetDefaultAbilitySetId(const FPrimaryAssetId& InAbilitySetId);
	FKnightResult TrySetPresentation(const FLinearColor& InBodyColor, float InWalkSpeed, float InSprintSpeed,
		const FVector& InFormationOffset, float InFollowDistance = 180.0f);

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

private:
	/** Stable definition identity; controller transfers never replace the granted source. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Knight|Framework|Abilities",
		meta = (AllowPrivateAccess = "true"))
	FPrimaryAssetId DefaultAbilitySetId = FPrimaryAssetId(TEXT("KnightAbilitySet"), TEXT("Foundation"));

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Knight|Framework|Presentation",
		meta = (AllowPrivateAccess = "true"))
	FLinearColor BodyColor = FLinearColor::White;

	/** Unreal units per second. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Knight|Framework|Movement",
		meta = (AllowPrivateAccess = "true", ClampMin = "1.0", Units = "cm/s"))
	float WalkSpeed = 420.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Knight|Framework|Movement",
		meta = (AllowPrivateAccess = "true", ClampMin = "1.0", Units = "cm/s"))
	float SprintSpeed = 650.0f;

	/** Local offset in centimeters relative to the human-controlled party leader. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Knight|Framework|Party",
		meta = (AllowPrivateAccess = "true", Units = "cm"))
	FVector FormationOffset = FVector(-220.0, 180.0, 0.0);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Knight|Framework|Party",
		meta = (AllowPrivateAccess = "true", ClampMin = "1.0", Units = "cm"))
	float FollowDistance = 180.0f;
};
