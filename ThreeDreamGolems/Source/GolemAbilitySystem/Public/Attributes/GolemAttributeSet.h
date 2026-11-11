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

USTRUCT(BlueprintType)
struct GOLEMABILITYSYSTEM_API FGolemAttributeDefaults
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|GAS|Attributes")
	float MaxHealth = 100.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|GAS|Attributes")
	float MaxStamina = 100.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|GAS|Attributes")
	float MaxRuneEnergy = 100.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|GAS|Attributes")
	float AttackPower = 25.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|GAS|Attributes")
	float Defense = 0.0f;

	FGolemResult Validate() const;
};

/** IncomingDamage is a server-only meta attribute. Resource values replicate through standard GAS callbacks. */
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
	virtual bool PreGameplayEffectExecute(FGameplayEffectModCallbackData& Data) override;

	GOLEM_ATTRIBUTE_ACCESSORS(UGolemAttributeSet, Health)
	GOLEM_ATTRIBUTE_ACCESSORS(UGolemAttributeSet, MaxHealth)
	GOLEM_ATTRIBUTE_ACCESSORS(UGolemAttributeSet, Stamina)
	GOLEM_ATTRIBUTE_ACCESSORS(UGolemAttributeSet, MaxStamina)
	GOLEM_ATTRIBUTE_ACCESSORS(UGolemAttributeSet, RuneEnergy)
	GOLEM_ATTRIBUTE_ACCESSORS(UGolemAttributeSet, MaxRuneEnergy)
	GOLEM_ATTRIBUTE_ACCESSORS(UGolemAttributeSet, AttackPower)
	GOLEM_ATTRIBUTE_ACCESSORS(UGolemAttributeSet, Defense)
	GOLEM_ATTRIBUTE_ACCESSORS(UGolemAttributeSet, IncomingDamage)
	GOLEM_ATTRIBUTE_ACCESSORS(UGolemAttributeSet, ArcaneResistance)
	GOLEM_ATTRIBUTE_ACCESSORS(UGolemAttributeSet, LightningResistance)
	GOLEM_ATTRIBUTE_ACCESSORS(UGolemAttributeSet, FrostResistance)
	GOLEM_ATTRIBUTE_ACCESSORS(UGolemAttributeSet, FireResistance)
	GOLEM_ATTRIBUTE_ACCESSORS(UGolemAttributeSet, CritChance)
	GOLEM_ATTRIBUTE_ACCESSORS(UGolemAttributeSet, CritMultiplier)
	GOLEM_ATTRIBUTE_ACCESSORS(UGolemAttributeSet, Haste)
	GOLEM_ATTRIBUTE_ACCESSORS(UGolemAttributeSet, CooldownRecovery)
	GOLEM_ATTRIBUTE_ACCESSORS(UGolemAttributeSet, MasteryBonus)
	GOLEM_ATTRIBUTE_ACCESSORS(UGolemAttributeSet, Versatility)
	GOLEM_ATTRIBUTE_ACCESSORS(UGolemAttributeSet, DamageReduction)
	GOLEM_ATTRIBUTE_ACCESSORS(UGolemAttributeSet, PoiseBonus)

private:
	UFUNCTION()
	void OnRep_PoiseBonus(const FGameplayAttributeData& OldValue);
	UPROPERTY(ReplicatedUsing = OnRep_PoiseBonus)
	FGameplayAttributeData PoiseBonus;
	void ClampAttribute(const FGameplayAttribute& Attribute, float& Value) const;

	UFUNCTION()
	void OnRep_Health(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void OnRep_MaxHealth(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void OnRep_Stamina(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void OnRep_MaxStamina(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void OnRep_RuneEnergy(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void OnRep_MaxRuneEnergy(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void OnRep_AttackPower(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void OnRep_Defense(const FGameplayAttributeData& OldValue);

	UPROPERTY(ReplicatedUsing = OnRep_Health)
	FGameplayAttributeData Health;
	UPROPERTY(ReplicatedUsing = OnRep_MaxHealth)
	FGameplayAttributeData MaxHealth;
	UPROPERTY(ReplicatedUsing = OnRep_Stamina)
	FGameplayAttributeData Stamina;
	UPROPERTY(ReplicatedUsing = OnRep_MaxStamina)
	FGameplayAttributeData MaxStamina;
	UPROPERTY(ReplicatedUsing = OnRep_RuneEnergy)
	FGameplayAttributeData RuneEnergy;
	UPROPERTY(ReplicatedUsing = OnRep_MaxRuneEnergy)
	FGameplayAttributeData MaxRuneEnergy;
	UPROPERTY(ReplicatedUsing = OnRep_AttackPower)
	FGameplayAttributeData AttackPower;
	UPROPERTY(ReplicatedUsing = OnRep_Defense)
	FGameplayAttributeData Defense;
	UPROPERTY()
	FGameplayAttributeData IncomingDamage;
	UFUNCTION()
	void OnRep_ArcaneResistance(const FGameplayAttributeData& OldValue);
	UPROPERTY(ReplicatedUsing = OnRep_ArcaneResistance)
	FGameplayAttributeData ArcaneResistance;
	UFUNCTION()
	void OnRep_LightningResistance(const FGameplayAttributeData& OldValue);
	UPROPERTY(ReplicatedUsing = OnRep_LightningResistance)
	FGameplayAttributeData LightningResistance;
	UFUNCTION()
	void OnRep_FrostResistance(const FGameplayAttributeData& OldValue);
	UPROPERTY(ReplicatedUsing = OnRep_FrostResistance)
	FGameplayAttributeData FrostResistance;
	UFUNCTION()
	void OnRep_FireResistance(const FGameplayAttributeData& OldValue);
	UPROPERTY(ReplicatedUsing = OnRep_FireResistance)
	FGameplayAttributeData FireResistance;
	UFUNCTION()
	void OnRep_CritChance(const FGameplayAttributeData& OldValue);
	UPROPERTY(ReplicatedUsing = OnRep_CritChance)
	FGameplayAttributeData CritChance;
	UFUNCTION()
	void OnRep_CritMultiplier(const FGameplayAttributeData& OldValue);
	UPROPERTY(ReplicatedUsing = OnRep_CritMultiplier)
	FGameplayAttributeData CritMultiplier;
	UFUNCTION()
	void OnRep_Haste(const FGameplayAttributeData& OldValue);
	UPROPERTY(ReplicatedUsing = OnRep_Haste)
	FGameplayAttributeData Haste;
	UFUNCTION()
	void OnRep_CooldownRecovery(const FGameplayAttributeData& OldValue);
	UPROPERTY(ReplicatedUsing = OnRep_CooldownRecovery)
	FGameplayAttributeData CooldownRecovery;
	UFUNCTION()
	void OnRep_MasteryBonus(const FGameplayAttributeData& OldValue);
	UPROPERTY(ReplicatedUsing = OnRep_MasteryBonus)
	FGameplayAttributeData MasteryBonus;
	UFUNCTION()
	void OnRep_Versatility(const FGameplayAttributeData& OldValue);
	UPROPERTY(ReplicatedUsing = OnRep_Versatility)
	FGameplayAttributeData Versatility;
	UFUNCTION()
	void OnRep_DamageReduction(const FGameplayAttributeData& OldValue);
	UPROPERTY(ReplicatedUsing = OnRep_DamageReduction)
	FGameplayAttributeData DamageReduction;
};
