#include "Attributes/KnightAttributeSet.h"

#include "Components/KnightAbilitySystemComponent.h"
#include "Components/KnightLifeComponent.h"
#include "GameplayEffectExtension.h"
#include "Effects/KnightArenaResourceRecoveryEffect.h"
#include "Skills/KnightSkillRuntimeComponent.h"
#include "Net/UnrealNetwork.h"
#include "Tags/KnightGameplayTags.h"

FKnightResult FKnightAttributeDefaults::Validate() const
{
	if (!FMath::IsFinite(MaxHealth) || MaxHealth < 1.0f || MaxHealth > 1000000.0f ||
		!FMath::IsFinite(MaxStamina) || MaxStamina < 1.0f || MaxStamina > 1000000.0f ||
		!FMath::IsFinite(MaxRuneEnergy) || MaxRuneEnergy < 1.0f || MaxRuneEnergy > 1000000.0f ||
		!FMath::IsFinite(AttackPower) || AttackPower < 0.0f || AttackPower > 1000000.0f ||
		!FMath::IsFinite(Defense) || Defense < 0.0f || Defense > 1000000.0f)
	{
		return FKnightResult::Failure(EKnightError::InvalidData, TEXT("AttributeDefaults"),
			NSLOCTEXT("KnightGAS", "AttributeDefaultsInvalid", "Attribute defaults must be finite and within supported resource bounds."));
	}
	return FKnightResult::Success();
}

UKnightAttributeSet::UKnightAttributeSet()
{
	InitHealth(100.0f);
	InitMaxHealth(100.0f);
	InitStamina(100.0f);
	InitMaxStamina(100.0f);
	InitRuneEnergy(100.0f);
	InitMaxRuneEnergy(100.0f);
	InitAttackPower(25.0f);
	InitDefense(0.0f);
	InitIncomingDamage(0.0f);
	InitArcaneResistance(0.0f);
	InitLightningResistance(0.0f);
	InitFrostResistance(0.0f);
	InitFireResistance(0.0f);
	InitCritChance(0.10f);
	InitPoiseBonus(0.0f);
	InitCritMultiplier(1.50f);
	InitHaste(0.0f);
	InitCooldownRecovery(0.0f);
	InitMasteryBonus(0.0f);
	InitVersatility(0.0f);
	InitDamageReduction(0.0f);
}

void UKnightAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION_NOTIFY(UKnightAttributeSet, Health, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UKnightAttributeSet, MaxHealth, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UKnightAttributeSet, Stamina, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UKnightAttributeSet, MaxStamina, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UKnightAttributeSet, RuneEnergy, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UKnightAttributeSet, MaxRuneEnergy, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UKnightAttributeSet, AttackPower, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UKnightAttributeSet, Defense, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UKnightAttributeSet, ArcaneResistance, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UKnightAttributeSet, LightningResistance, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UKnightAttributeSet, FrostResistance, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UKnightAttributeSet, FireResistance, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UKnightAttributeSet, CritChance, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UKnightAttributeSet, CritMultiplier, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UKnightAttributeSet, Haste, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UKnightAttributeSet, CooldownRecovery, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UKnightAttributeSet, MasteryBonus, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UKnightAttributeSet, Versatility, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UKnightAttributeSet, DamageReduction, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UKnightAttributeSet, PoiseBonus, COND_None, REPNOTIFY_Always);
}

void UKnightAttributeSet::ClampAttribute(const FGameplayAttribute& Attribute, float& Value) const
{
	if (!FMath::IsFinite(Value))
	{
		Value = 0.0f;
	}
	if (Attribute == GetHealthAttribute())
	{
		Value = FMath::Clamp(Value, 0.0f, GetMaxHealth());
	}
	else if (Attribute == GetStaminaAttribute())
	{
		Value = FMath::Clamp(Value, 0.0f, GetMaxStamina());
	}
	else if (Attribute == GetRuneEnergyAttribute())
	{
		Value = FMath::Clamp(Value, 0.0f, GetMaxRuneEnergy());
	}
	else if (Attribute == GetMaxHealthAttribute() || Attribute == GetMaxStaminaAttribute() ||
		Attribute == GetMaxRuneEnergyAttribute())
	{
		Value = FMath::Clamp(Value, 1.0f, 1000000.0f);
	}
	else if (Attribute == GetArcaneResistanceAttribute() || Attribute == GetLightningResistanceAttribute()
		|| Attribute == GetFrostResistanceAttribute() || Attribute == GetFireResistanceAttribute())
	{
		Value = FMath::Clamp(Value, -0.25f, 0.60f);
	}
	else
	{
		Value = FMath::Clamp(Value, 0.0f, 1000000.0f);
	}
	if (Attribute == GetArcaneResistanceAttribute() || Attribute == GetLightningResistanceAttribute()
		|| Attribute == GetFrostResistanceAttribute() || Attribute == GetFireResistanceAttribute())
	{
		Value = FMath::Clamp(Value, -0.25f, 0.60f);
	}
	else if (Attribute == GetCritChanceAttribute())
	{
		Value = FMath::Clamp(Value, 0.0f, 0.40f);
	}
	else if (Attribute == GetCritMultiplierAttribute())
	{
		Value = FMath::Clamp(Value, 1.0f, 3.0f);
	}
	else if (Attribute == GetHasteAttribute() || Attribute == GetCooldownRecoveryAttribute()
		|| Attribute == GetMasteryBonusAttribute())
	{
		Value = FMath::Clamp(Value, 0.0f, 0.30f);
	}
	else if (Attribute == GetVersatilityAttribute())
	{
		Value = FMath::Clamp(Value, 0.0f, 0.20f);
	}
	else if (Attribute == GetDamageReductionAttribute())
	{
		Value = FMath::Clamp(Value, 0.0f, 0.50f);
	}
}

void UKnightAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);
	ClampAttribute(Attribute, NewValue);
}

void UKnightAttributeSet::PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const
{
	Super::PreAttributeBaseChange(Attribute, NewValue);
	ClampAttribute(Attribute, NewValue);
}

bool UKnightAttributeSet::PreGameplayEffectExecute(FGameplayEffectModCallbackData& Data)
{
	if (Data.EffectSpec.Def != nullptr && Data.EffectSpec.Def->IsA<UKnightArenaResourceRecoveryEffect>()
		&& (Data.EvaluatedData.Attribute == GetRuneEnergyAttribute() || Data.EvaluatedData.Attribute == GetStaminaAttribute()))
	{
		const UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent();
		const UKnightSkillRuntimeComponent* Runtime = ASC != nullptr && ASC->GetOwnerActor() != nullptr
			? ASC->GetOwnerActor()->FindComponentByClass<UKnightSkillRuntimeComponent>() : nullptr;
		if (Runtime != nullptr && !Runtime->GetBuild().SpecId.IsNone())
		{
			return false;
		}
	}
	return Super::PreGameplayEffectExecute(Data);
}

void UKnightAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);
	UKnightAbilitySystemComponent* AbilitySystem = Cast<UKnightAbilitySystemComponent>(GetOwningAbilitySystemComponent());
	if (Data.EvaluatedData.Attribute == GetIncomingDamageAttribute())
	{
		const float Damage = FMath::Max(0.0f, GetIncomingDamage());
		SetIncomingDamage(0.0f);
		if (AbilitySystem == nullptr || Damage <= 0.0f)
		{
			return;
		}
		UKnightLifeComponent* Life = AbilitySystem->GetLifeComponent();
		const bool bWasDowned = Life != nullptr && Life->IsDowned();
		const float PreviousHealth = GetHealth();
		SetHealth(FMath::Clamp(PreviousHealth - Damage, 0.0f, GetMaxHealth()));
		if (bWasDowned)
		{
			Life->NotifyDamageWhileDowned();
		}
		AActor* Source = Data.EffectSpec.GetContext().GetOriginalInstigator();
		AbilitySystem->PublishDamageConfirmation(Source, bWasDowned ? Damage : PreviousHealth - GetHealth());
		AbilitySystem->ExecuteGameplayCue(KnightGameplayTags::GameplayCue_Damage, Data.EffectSpec.GetContext());
		return;
	}
	SetHealth(FMath::Clamp(GetHealth(), 0.0f, GetMaxHealth()));
	SetStamina(FMath::Clamp(GetStamina(), 0.0f, GetMaxStamina()));
	SetRuneEnergy(FMath::Clamp(GetRuneEnergy(), 0.0f, GetMaxRuneEnergy()));
}

void UKnightAttributeSet::PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue)
{
	Super::PostAttributeChange(Attribute, OldValue, NewValue);
	const UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent();
	if (ASC == nullptr || ASC->GetOwnerActor() == nullptr || !ASC->GetOwnerActor()->HasAuthority())
	{
		return;
	}
	if (Attribute == GetMaxHealthAttribute() && GetHealth() > NewValue)
	{
		SetHealth(NewValue);
	}
	else if (Attribute == GetMaxStaminaAttribute() && GetStamina() > NewValue)
	{
		SetStamina(NewValue);
	}
	else if (Attribute == GetMaxRuneEnergyAttribute() && GetRuneEnergy() > NewValue)
	{
		SetRuneEnergy(NewValue);
	}
}

void UKnightAttributeSet::OnRep_Health(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UKnightAttributeSet, Health, OldValue);
}

void UKnightAttributeSet::OnRep_PoiseBonus(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UKnightAttributeSet, PoiseBonus, OldValue);
}
void UKnightAttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UKnightAttributeSet, MaxHealth, OldValue);
}
void UKnightAttributeSet::OnRep_Stamina(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UKnightAttributeSet, Stamina, OldValue);
}
void UKnightAttributeSet::OnRep_MaxStamina(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UKnightAttributeSet, MaxStamina, OldValue);
}
void UKnightAttributeSet::OnRep_RuneEnergy(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UKnightAttributeSet, RuneEnergy, OldValue);
}
void UKnightAttributeSet::OnRep_MaxRuneEnergy(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UKnightAttributeSet, MaxRuneEnergy, OldValue);
}
void UKnightAttributeSet::OnRep_AttackPower(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UKnightAttributeSet, AttackPower, OldValue);
}
void UKnightAttributeSet::OnRep_Defense(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UKnightAttributeSet, Defense, OldValue);
}

void UKnightAttributeSet::OnRep_ArcaneResistance(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UKnightAttributeSet, ArcaneResistance, OldValue);
}

void UKnightAttributeSet::OnRep_LightningResistance(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UKnightAttributeSet, LightningResistance, OldValue);
}

void UKnightAttributeSet::OnRep_FrostResistance(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UKnightAttributeSet, FrostResistance, OldValue);
}

void UKnightAttributeSet::OnRep_FireResistance(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UKnightAttributeSet, FireResistance, OldValue);
}

void UKnightAttributeSet::OnRep_CritChance(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UKnightAttributeSet, CritChance, OldValue);
}

void UKnightAttributeSet::OnRep_CritMultiplier(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UKnightAttributeSet, CritMultiplier, OldValue);
}

void UKnightAttributeSet::OnRep_Haste(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UKnightAttributeSet, Haste, OldValue);
}

void UKnightAttributeSet::OnRep_CooldownRecovery(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UKnightAttributeSet, CooldownRecovery, OldValue);
}

void UKnightAttributeSet::OnRep_MasteryBonus(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UKnightAttributeSet, MasteryBonus, OldValue);
}

void UKnightAttributeSet::OnRep_Versatility(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UKnightAttributeSet, Versatility, OldValue);
}

void UKnightAttributeSet::OnRep_DamageReduction(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UKnightAttributeSet, DamageReduction, OldValue);
}
