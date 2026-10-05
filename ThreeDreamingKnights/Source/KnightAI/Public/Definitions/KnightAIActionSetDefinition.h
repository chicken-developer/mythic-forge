#pragma once

#include "CoreMinimal.h"
#include "Assets/KnightDefinition.h"
#include "GameplayTagContainer.h"
#include "KnightAIActionSetDefinition.generated.h"

/** Eligibility data only. GAS remains responsible for validated costs, activation and action completion. */
USTRUCT(BlueprintType)
struct KNIGHTAI_API FKnightAIActionEntry
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Knight|AI|Actions")
	FName ActionKey;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Knight|AI|Actions")
	FGameplayTag ActionTag;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Knight|AI|Actions", meta = (Units = "cm", ClampMin = "0"))
	float MinRange = 0.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Knight|AI|Actions", meta = (Units = "cm", ClampMin = "0"))
	float MaxRange = 300.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Knight|AI|Actions", meta = (ClampMin = "0"))
	float RequiredStamina = 0.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Knight|AI|Actions", meta = (ClampMin = "0"))
	float RequiredRuneEnergy = 0.0f;
	/** StateTree may record this delay after confirmed activation; selection never starts a cooldown. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Knight|AI|Actions", meta = (Units = "s", ClampMin = "0"))
	float CooldownSeconds = 0.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Knight|AI|Actions", meta = (ClampMin = "-10000", ClampMax = "10000"))
	int32 Priority = 0;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Knight|AI|Actions")
	FGameplayTagContainer RequiredStateTags;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Knight|AI|Actions")
	FGameplayTagContainer BlockedStateTags;
};

/** Explicit snapshot provided by a StateTree task. No actor, timer, random state or mutable ASC is queried. */
USTRUCT(BlueprintType)
struct KNIGHTAI_API FKnightAIActionSelectionContext
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Knight|AI|Actions", meta = (Units = "cm"))
	float Distance = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category = "Knight|AI|Actions")
	float Stamina = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category = "Knight|AI|Actions")
	float RuneEnergy = 0.0f;
	/** Exact tags of granted actions, rather than parent tags that merely describe an ability family. */
	UPROPERTY(BlueprintReadOnly, Category = "Knight|AI|Actions")
	FGameplayTagContainer OwnedAbilityActionTags;
	UPROPERTY(BlueprintReadOnly, Category = "Knight|AI|Actions")
	FGameplayTagContainer StateTags;
	/** Missing key means no remaining selection delay. Values are finite, nonnegative seconds. */
	UPROPERTY(BlueprintReadOnly, Category = "Knight|AI|Actions")
	TMap<FName, float> RemainingCooldowns;
};

UCLASS(BlueprintType)
class KNIGHTAI_API UKnightAIActionSetDefinition : public UKnightDefinition
{
	GENERATED_BODY()

public:
	UKnightAIActionSetDefinition();
	const TArray<FKnightAIActionEntry>& GetActions() const;
	FKnightResult ValidateActionSet() const;
	FKnightResult TrySetActions(const TArray<FKnightAIActionEntry>& InActions);
	/** Highest priority wins; ties use lexical stable key order, independent of authoring array order. */
	FKnightResult TrySelectAction(const FKnightAIActionSelectionContext& Context, FKnightAIActionEntry& OutAction) const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

private:
	static FKnightResult ValidateActions(const TArray<FKnightAIActionEntry>& InActions);
	static FKnightResult ValidateSelectionContext(const FKnightAIActionSelectionContext& Context);
	UPROPERTY(EditDefaultsOnly, Category = "Knight|AI|Actions")
	TArray<FKnightAIActionEntry> Actions;
};
