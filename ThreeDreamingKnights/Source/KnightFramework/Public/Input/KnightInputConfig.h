#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "InputActionValue.h"
#include "UObject/Object.h"
#include "KnightInputConfig.generated.h"

class UInputAction;
class UInputMappingContext;
struct FKey;

/** Native semantic actions and default desktop/gamepad mappings, owned by one local controller. */
UCLASS()
class KNIGHTFRAMEWORK_API UKnightInputConfig : public UObject
{
	GENERATED_BODY()

public:
	void InitializeDefaultMapping();
	const UInputAction* FindAction(const FGameplayTag& InputTag) const;
	UInputMappingContext* GetMappingContext() const;
	UInputMappingContext* GetCombatMappingContext() const;
	bool IsCombatInput(const FGameplayTag& InputTag) const;

private:
	UInputAction* CreateAction(const FGameplayTag& InputTag, EInputActionValueType ValueType, FName ActionName);
	void MapAxis(UInputAction* Action, const FKey& Key, bool bIsVertical, bool bIsNegative);

	UPROPERTY(Transient)
	TMap<FGameplayTag, TObjectPtr<UInputAction>> Actions;

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> MappingContext;
	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> CombatMappingContext;
	TSet<FGameplayTag> CombatInputTags;

	UPROPERTY(EditDefaultsOnly, Category = "Knight|Input|Gamepad", meta = (ClampMin = "0", ClampMax = "0.9"))
	float GamepadDeadZone = 0.2f;

	UPROPERTY(EditDefaultsOnly, Category = "Knight|Input|Gamepad", meta = (ClampMin = "1", Units = "deg/s"))
	float GamepadLookRate = 120.0f;
};
