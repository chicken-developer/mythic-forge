#pragma once

#include "CoreMinimal.h"
#include "Assets/KnightDefinition.h"
#include "Definitions/KnightTagRelationshipMapping.h"
#include "GameplayAbilitySpec.h"
#include "GameplayEffectTypes.h"
#include "KnightAbilitySetDefinition.generated.h"

class UKnightGameplayAbility;
class UGameplayEffect;

USTRUCT(BlueprintType)
struct KNIGHTABILITYSYSTEM_API FKnightAbilitySetEntry
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Knight|GAS|Grants")
	TSubclassOf<UKnightGameplayAbility> Ability;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Knight|GAS|Grants")
	FGameplayTag InputTag;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Knight|GAS|Grants", meta = (ClampMin = "1"))
	int32 Level = 1;
};

/** This capability identifies only one ASC's source-owned grant; it cannot revoke an unrelated source. */
USTRUCT()
struct KNIGHTABILITYSYSTEM_API FKnightAbilitySetHandle
{
	GENERATED_BODY()

	UPROPERTY()
	FGuid GrantId;
	UPROPERTY()
	FName SourceKey;
	UPROPERTY()
	FPrimaryAssetId AbilitySetId;

	bool IsValid() const;
};

UCLASS(BlueprintType)
class KNIGHTABILITYSYSTEM_API UKnightAbilitySetDefinition : public UKnightDefinition
{
	GENERATED_BODY()

public:
	UKnightAbilitySetDefinition();
	const TArray<FKnightAbilitySetEntry>& GetAbilities() const;
	const TArray<TSubclassOf<UGameplayEffect>>& GetEffects() const;
	const TArray<FKnightTagRelationship>& GetRelationships() const;
	FKnightResult ValidateAbilitySet() const;
	FKnightResult TrySetAbilities(const TArray<FKnightAbilitySetEntry>& InAbilities);
	FKnightResult TrySetEffects(const TArray<TSubclassOf<UGameplayEffect>>& InEffects);
	FKnightResult TrySetRelationships(const TArray<FKnightTagRelationship>& InRelationships);

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

private:
	static FKnightResult ValidateAbilities(const TArray<FKnightAbilitySetEntry>& InAbilities);
	static FKnightResult ValidateEffects(const TArray<TSubclassOf<UGameplayEffect>>& InEffects);

	UPROPERTY(EditDefaultsOnly, Category = "Knight|GAS|Grants")
	TArray<FKnightAbilitySetEntry> Abilities;
	UPROPERTY(EditDefaultsOnly, Category = "Knight|GAS|Grants")
	TArray<TSubclassOf<UGameplayEffect>> Effects;
	UPROPERTY(EditDefaultsOnly, Category = "Knight|GAS|Relationships")
	TArray<FKnightTagRelationship> Relationships;
};
