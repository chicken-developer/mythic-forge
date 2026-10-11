#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "Types/KnightResult.h"
#include "KnightAttributeSet.generated.h"

#define KNIGHT_ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

USTRUCT(BlueprintType)
struct KNIGHTABILITYSYSTEM_API FKnightAttributeDefaults
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Knight|GAS|Attributes")
	float MaxHealth = 100.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Knight|GAS|Attributes")
	float MaxStamina = 100.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Knight|GAS|Attributes")
	float MaxRuneEnergy = 100.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Knight|GAS|Attributes")
	float AttackPower = 25.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Knight|GAS|Attributes")
	float Defense = 0.0f;

	FKnightResult Validate() const;
};

/** IncomingDamage is a server-only meta attribute. Resource values replicate through standard GAS callbacks. */
UCLASS()
class KNIGHTABILITYSYSTEM_API UKnightAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UKnightAttributeSet();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const override;
	virtual void PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue) override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;
	virtual bool PreGameplayEffectExecute(FGameplayEffectModCallbackData& Data) override;

	KNIGHT_ATTRIBUTE_ACCESSORS(UKnightAttributeSet, Health)
	KNIGHT_ATTRIBUTE_ACCESSORS(UKnightAttributeSet, MaxHealth)
	KNIGHT_ATTRIBUTE_ACCESSORS(UKnightAttributeSet, Stamina)
	KNIGHT_ATTRIBUTE_ACCESSORS(UKnightAttributeSet, MaxStamina)
	KNIGHT_ATTRIBUTE_ACCESSORS(UKnightAttributeSet, RuneEnergy)
	KNIGHT_ATTRIBUTE_ACCESSORS(UKnightAttributeSet, MaxRuneEnergy)
	KNIGHT_ATTRIBUTE_ACCESSORS(UKnightAttributeSet, AttackPower)
	KNIGHT_ATTRIBUTE_ACCESSORS(UKnightAttributeSet, Defense)
	KNIGHT_ATTRIBUTE_ACCESSORS(UKnightAttributeSet, IncomingDamage)
	KNIGHT_ATTRIBUTE_ACCESSORS(UKnightAttributeSet, ArcaneResistance)
	KNIGHT_ATTRIBUTE_ACCESSORS(UKnightAttributeSet, LightningResistance)
	KNIGHT_ATTRIBUTE_ACCESSORS(UKnightAttributeSet, FrostResistance)
	KNIGHT_ATTRIBUTE_ACCESSORS(UKnightAttributeSet, FireResistance)
	KNIGHT_ATTRIBUTE_ACCESSORS(UKnightAttributeSet, CritChance)
	KNIGHT_ATTRIBUTE_ACCESSORS(UKnightAttributeSet, CritMultiplier)
	KNIGHT_ATTRIBUTE_ACCESSORS(UKnightAttributeSet, Haste)
	KNIGHT_ATTRIBUTE_ACCESSORS(UKnightAttributeSet, CooldownRecovery)
	KNIGHT_ATTRIBUTE_ACCESSORS(UKnightAttributeSet, MasteryBonus)
	KNIGHT_ATTRIBUTE_ACCESSORS(UKnightAttributeSet, Versatility)
	KNIGHT_ATTRIBUTE_ACCESSORS(UKnightAttributeSet, DamageReduction)
	KNIGHT_ATTRIBUTE_ACCESSORS(UKnightAttributeSet, PoiseBonus)

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
