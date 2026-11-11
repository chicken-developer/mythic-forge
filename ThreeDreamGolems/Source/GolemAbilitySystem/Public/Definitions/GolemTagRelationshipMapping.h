#pragma once

#include "CoreMinimal.h"
#include "Assets/GolemDefinition.h"
#include "GameplayTagContainer.h"
#include "GolemTagRelationshipMapping.generated.h"

USTRUCT(BlueprintType)
struct GOLEMABILITYSYSTEM_API FGolemTagRelationship
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|GAS|Relationships")
	FGameplayTag AbilityTag;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|GAS|Relationships")
	FGameplayTagContainer AbilityTagsToBlock;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|GAS|Relationships")
	FGameplayTagContainer AbilityTagsToCancel;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|GAS|Relationships")
	FGameplayTagContainer ActivationRequiredTags;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|GAS|Relationships")
	FGameplayTagContainer ActivationBlockedTags;
};

UCLASS(BlueprintType)
class GOLEMABILITYSYSTEM_API UGolemTagRelationshipMapping : public UGolemDefinition
{
	GENERATED_BODY()

public:
	UGolemTagRelationshipMapping();
	const TArray<FGolemTagRelationship>& GetRelationships() const;
	FGolemResult TrySetRelationships(const TArray<FGolemTagRelationship>& InRelationships);
	static FGolemResult ValidateRelationships(const TArray<FGolemTagRelationship>& InRelationships);

private:
	UPROPERTY(EditDefaultsOnly, Category = "Golem|GAS|Relationships")
	TArray<FGolemTagRelationship> Relationships;
};
