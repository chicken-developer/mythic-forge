#pragma once

#include "CoreMinimal.h"
#include "Assets/GolemDefinition.h"
#include "GameplayTagContainer.h"
#include "UObject/PrimaryAssetId.h"
#include "GolemHeroDefinition.generated.h"

/** Golem data is selected by stable definition ID, independently of human or companion ownership. */
UCLASS(BlueprintType)
class GOLEMFRAMEWORK_API UGolemHeroDefinition : public UGolemDefinition
{
	GENERATED_BODY()

public:
	UGolemHeroDefinition();
	FLinearColor GetBodyColor() const;
	float GetWalkSpeed() const;
	float GetSprintSpeed() const;
	FVector GetFormationOffset() const;
	float GetFollowDistance() const;
	FPrimaryAssetId GetDefaultAbilitySetId() const;

	/** Element.* tag: the one colour this Golem can place on a void. */
	FGameplayTag GetElementTag() const;

	/** Class.* tag: Warrior, Hunter or Mage. */
	FGameplayTag GetClassTag() const;
	FGolemResult TrySetArchetype(FGameplayTag InElementTag, FGameplayTag InClassTag);
	FGolemResult ValidateHero() const;
	FGolemResult TrySetDefaultAbilitySetId(const FPrimaryAssetId& InAbilitySetId);
	FGolemResult TrySetPresentation(const FLinearColor& InBodyColor, float InWalkSpeed, float InSprintSpeed,
		const FVector& InFormationOffset, float InFollowDistance = 180.0f);

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

private:
	/** Stable definition identity; controller transfers never replace the granted source. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Framework|Abilities",
		meta = (AllowPrivateAccess = "true"))
	FPrimaryAssetId DefaultAbilitySetId = FPrimaryAssetId(TEXT("GolemAbilitySet"), TEXT("Foundation"));

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Framework|Presentation",
		meta = (AllowPrivateAccess = "true"))
	FLinearColor BodyColor = FLinearColor::White;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Framework|Archetype",
		meta = (AllowPrivateAccess = "true", Categories = "Element"))
	FGameplayTag ElementTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Framework|Archetype",
		meta = (AllowPrivateAccess = "true", Categories = "Class"))
	FGameplayTag ClassTag;

	/** Unreal units per second. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Framework|Movement",
		meta = (AllowPrivateAccess = "true", ClampMin = "1.0", Units = "cm/s"))
	float WalkSpeed = 420.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Framework|Movement",
		meta = (AllowPrivateAccess = "true", ClampMin = "1.0", Units = "cm/s"))
	float SprintSpeed = 650.0f;

	/** Local offset in centimeters relative to the human-controlled party leader. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Framework|Party",
		meta = (AllowPrivateAccess = "true", Units = "cm"))
	FVector FormationOffset = FVector(-220.0, 180.0, 0.0);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Framework|Party",
		meta = (AllowPrivateAccess = "true", ClampMin = "1.0", Units = "cm"))
	float FollowDistance = 180.0f;
};
