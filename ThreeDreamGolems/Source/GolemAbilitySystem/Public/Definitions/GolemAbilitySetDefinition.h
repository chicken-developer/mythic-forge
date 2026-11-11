#pragma once

#include "CoreMinimal.h"
#include "Assets/GolemDefinition.h"
#include "Definitions/GolemTagRelationshipMapping.h"
#include "GameplayAbilitySpec.h"
#include "GameplayEffectTypes.h"
#include "GolemAbilitySetDefinition.generated.h"

class UGolemGameplayAbility;
class UGameplayEffect;

USTRUCT(BlueprintType)
struct GOLEMABILITYSYSTEM_API FGolemAbilitySetEntry
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|GAS|Grants")
	TSubclassOf<UGolemGameplayAbility> Ability;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|GAS|Grants")
	FGameplayTag InputTag;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|GAS|Grants", meta = (ClampMin = "1"))
	int32 Level = 1;
};

/** This capability identifies only one ASC's source-owned grant; it cannot revoke an unrelated source. */
USTRUCT()
struct GOLEMABILITYSYSTEM_API FGolemAbilitySetHandle
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
class GOLEMABILITYSYSTEM_API UGolemAbilitySetDefinition : public UGolemDefinition
{
	GENERATED_BODY()

public:
	UGolemAbilitySetDefinition();
	const TArray<FGolemAbilitySetEntry>& GetAbilities() const;
	const TArray<TSubclassOf<UGameplayEffect>>& GetEffects() const;
	const TArray<FGolemTagRelationship>& GetRelationships() const;
	FGolemResult ValidateAbilitySet() const;
	FGolemResult TrySetAbilities(const TArray<FGolemAbilitySetEntry>& InAbilities);
	FGolemResult TrySetEffects(const TArray<TSubclassOf<UGameplayEffect>>& InEffects);
	FGolemResult TrySetRelationships(const TArray<FGolemTagRelationship>& InRelationships);

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

private:
	static FGolemResult ValidateAbilities(const TArray<FGolemAbilitySetEntry>& InAbilities);
	static FGolemResult ValidateEffects(const TArray<TSubclassOf<UGameplayEffect>>& InEffects);

	UPROPERTY(EditDefaultsOnly, Category = "Golem|GAS|Grants")
	TArray<FGolemAbilitySetEntry> Abilities;
	UPROPERTY(EditDefaultsOnly, Category = "Golem|GAS|Grants")
	TArray<TSubclassOf<UGameplayEffect>> Effects;
	UPROPERTY(EditDefaultsOnly, Category = "Golem|GAS|Relationships")
	TArray<FGolemTagRelationship> Relationships;
};
