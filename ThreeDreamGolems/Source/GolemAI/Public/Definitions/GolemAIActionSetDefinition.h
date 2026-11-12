#pragma once

#include "CoreMinimal.h"
#include "Assets/GolemDefinition.h"
#include "GameplayTagContainer.h"
#include "GolemAIActionSetDefinition.generated.h"

/** Eligibility data only. GAS remains responsible for validated costs, activation and action completion. */
USTRUCT(BlueprintType)
struct GOLEMAI_API FGolemAIActionEntry
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|AI|Actions")
	FName ActionKey;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|AI|Actions")
	FGameplayTag ActionTag;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|AI|Actions", meta = (Units = "cm", ClampMin = "0"))
	float MinRange = 0.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|AI|Actions", meta = (Units = "cm", ClampMin = "0"))
	float MaxRange = 300.0f;


	/** StateTree may record this delay after confirmed activation; selection never starts a cooldown. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|AI|Actions", meta = (Units = "s", ClampMin = "0"))
	float CooldownSeconds = 0.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|AI|Actions", meta = (ClampMin = "-10000", ClampMax = "10000"))
	int32 Priority = 0;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|AI|Actions")
	FGameplayTagContainer RequiredStateTags;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|AI|Actions")
	FGameplayTagContainer BlockedStateTags;
};

/** Explicit snapshot provided by a StateTree task. No actor, timer, random state or mutable ASC is queried. */
USTRUCT(BlueprintType)
struct GOLEMAI_API FGolemAIActionSelectionContext
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Golem|AI|Actions", meta = (Units = "cm"))
	float Distance = 0.0f;


	/** Exact tags of granted actions, rather than parent tags that merely describe an ability family. */
	UPROPERTY(BlueprintReadOnly, Category = "Golem|AI|Actions")
	FGameplayTagContainer OwnedAbilityActionTags;
	UPROPERTY(BlueprintReadOnly, Category = "Golem|AI|Actions")
	FGameplayTagContainer StateTags;
	/** Missing key means no remaining selection delay. Values are finite, nonnegative seconds. */
	UPROPERTY(BlueprintReadOnly, Category = "Golem|AI|Actions")
	TMap<FName, float> RemainingCooldowns;
};

UCLASS(BlueprintType)
class GOLEMAI_API UGolemAIActionSetDefinition : public UGolemDefinition
{
	GENERATED_BODY()

public:
	UGolemAIActionSetDefinition();
	const TArray<FGolemAIActionEntry>& GetActions() const;
	FGolemResult ValidateActionSet() const;
	FGolemResult TrySetActions(const TArray<FGolemAIActionEntry>& InActions);
	/** Highest priority wins; ties use lexical stable key order, independent of authoring array order. */
	FGolemResult TrySelectAction(const FGolemAIActionSelectionContext& Context, FGolemAIActionEntry& OutAction) const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

private:
	static FGolemResult ValidateActions(const TArray<FGolemAIActionEntry>& InActions);
	static FGolemResult ValidateSelectionContext(const FGolemAIActionSelectionContext& Context);
	UPROPERTY(EditDefaultsOnly, Category = "Golem|AI|Actions")
	TArray<FGolemAIActionEntry> Actions;
};
