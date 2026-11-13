#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "Types/GolemResult.h"
#include "GolemAttributeSet.generated.h"

#define GOLEM_ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

/** Starting values of the four designed stats. Tuning lives in data; these are only fallbacks. */
USTRUCT(BlueprintType)
struct GOLEMABILITYSYSTEM_API FGolemAttributeDefaults
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|GAS|Attributes")
	float MaxHealth = 100.0f;

	/** Multiplies the coefficient of every attack and skill. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|GAS|Attributes")
	float Power = 10.0f;

	/** Percent; 100 is the baseline move speed and dodge recovery. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|GAS|Attributes")
	float Agility = 100.0f;

	/** Seconds added to the mixing window of a void this Golem colours first. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|GAS|Attributes", meta = (Units = "s"))
	float ColorHold = 0.0f;

	FGolemResult Validate() const;
};

/** IncomingDamage is a server-only meta attribute. The rest replicate through standard GAS callbacks. */
UCLASS()
class GOLEMABILITYSYSTEM_API UGolemAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UGolemAttributeSet();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const override;
	virtual void PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue) override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

	GOLEM_ATTRIBUTE_ACCESSORS(UGolemAttributeSet, Health)
	GOLEM_ATTRIBUTE_ACCESSORS(UGolemAttributeSet, MaxHealth)
	GOLEM_ATTRIBUTE_ACCESSORS(UGolemAttributeSet, Power)
	GOLEM_ATTRIBUTE_ACCESSORS(UGolemAttributeSet, Agility)
	GOLEM_ATTRIBUTE_ACCESSORS(UGolemAttributeSet, ColorHold)
	GOLEM_ATTRIBUTE_ACCESSORS(UGolemAttributeSet, IncomingDamage)

private:
	void ClampAttribute(const FGameplayAttribute& Attribute, float& Value) const;

	UFUNCTION()
	void OnRep_Health(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_MaxHealth(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_Power(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_Agility(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_ColorHold(const FGameplayAttributeData& OldValue);

	UPROPERTY(ReplicatedUsing = OnRep_Health)
	FGameplayAttributeData Health;

	UPROPERTY(ReplicatedUsing = OnRep_MaxHealth)
	FGameplayAttributeData MaxHealth;

	UPROPERTY(ReplicatedUsing = OnRep_Power)
	FGameplayAttributeData Power;

	UPROPERTY(ReplicatedUsing = OnRep_Agility)
	FGameplayAttributeData Agility;

	UPROPERTY(ReplicatedUsing = OnRep_ColorHold)
	FGameplayAttributeData ColorHold;

	UPROPERTY()
	FGameplayAttributeData IncomingDamage;
};
