#pragma once

#include "CoreMinimal.h"
#include "Assets/KnightDefinition.h"
#include "GameplayTagContainer.h"
#include "KnightTagRelationshipMapping.generated.h"

USTRUCT(BlueprintType)
struct KNIGHTABILITYSYSTEM_API FKnightTagRelationship
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Knight|GAS|Relationships")
	FGameplayTag AbilityTag;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Knight|GAS|Relationships")
	FGameplayTagContainer AbilityTagsToBlock;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Knight|GAS|Relationships")
	FGameplayTagContainer AbilityTagsToCancel;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Knight|GAS|Relationships")
	FGameplayTagContainer ActivationRequiredTags;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Knight|GAS|Relationships")
	FGameplayTagContainer ActivationBlockedTags;
};

UCLASS(BlueprintType)
class KNIGHTABILITYSYSTEM_API UKnightTagRelationshipMapping : public UKnightDefinition
{
	GENERATED_BODY()

public:
	UKnightTagRelationshipMapping();
	const TArray<FKnightTagRelationship>& GetRelationships() const;
	FKnightResult TrySetRelationships(const TArray<FKnightTagRelationship>& InRelationships);
	static FKnightResult ValidateRelationships(const TArray<FKnightTagRelationship>& InRelationships);

private:
	UPROPERTY(EditDefaultsOnly, Category = "Knight|GAS|Relationships")
	TArray<FKnightTagRelationship> Relationships;
};
