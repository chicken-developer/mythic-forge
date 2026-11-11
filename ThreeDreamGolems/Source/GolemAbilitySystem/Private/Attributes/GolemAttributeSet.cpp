#include "Attributes/GolemAttributeSet.h"

#include "Components/GolemAbilitySystemComponent.h"
#include "Components/GolemLifeComponent.h"
#include "GameplayEffectExtension.h"
#include "Effects/GolemArenaResourceRecoveryEffect.h"
#include "Skills/GolemSkillRuntimeComponent.h"
#include "Net/UnrealNetwork.h"
#include "Tags/GolemGameplayTags.h"

FGolemResult FGolemAttributeDefaults::Validate() const
{
	if (!FMath::IsFinite(MaxHealth) || MaxHealth < 1.0f || MaxHealth > 1000000.0f ||
		!FMath::IsFinite(MaxStamina) || MaxStamina < 1.0f || MaxStamina > 1000000.0f ||
		!FMath::IsFinite(MaxRuneEnergy) || MaxRuneEnergy < 1.0f || MaxRuneEnergy > 1000000.0f ||
		!FMath::IsFinite(AttackPower) || AttackPower < 0.0f || AttackPower > 1000000.0f ||
		!FMath::IsFinite(Defense) || Defense < 0.0f || Defense > 1000000.0f)
	{
		return FGolemResult::Failure(EGolemError::InvalidData, TEXT("AttributeDefaults"),
			NSLOCTEXT("GolemGAS", "AttributeDefaultsInvalid", "Attribute defaults must be finite and within supported resource bounds."));
	}
	return FGolemResult::Success();
}

UGolemAttributeSet::UGolemAttributeSet()
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

void UGolemAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION_NOTIFY(UGolemAttributeSet, Health, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UGolemAttributeSet, MaxHealth, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UGolemAttributeSet, Stamina, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UGolemAttributeSet, MaxStamina, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UGolemAttributeSet, RuneEnergy, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UGolemAttributeSet, MaxRuneEnergy, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UGolemAttributeSet, AttackPower, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UGolemAttributeSet, Defense, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UGolemAttributeSet, ArcaneResistance, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UGolemAttributeSet, LightningResistance, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UGolemAttributeSet, FrostResistance, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UGolemAttributeSet, FireResistance, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UGolemAttributeSet, CritChance, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UGolemAttributeSet, CritMultiplier, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UGolemAttributeSet, Haste, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UGolemAttributeSet, CooldownRecovery, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UGolemAttributeSet, MasteryBonus, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UGolemAttributeSet, Versatility, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UGolemAttributeSet, DamageReduction, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UGolemAttributeSet, PoiseBonus, COND_None, REPNOTIFY_Always);
}

void UGolemAttributeSet::ClampAttribute(const FGameplayAttribute& Attribute, float& Value) const
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

void UGolemAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);
	ClampAttribute(Attribute, NewValue);
}

void UGolemAttributeSet::PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const
{
	Super::PreAttributeBaseChange(Attribute, NewValue);
	ClampAttribute(Attribute, NewValue);
}

bool UGolemAttributeSet::PreGameplayEffectExecute(FGameplayEffectModCallbackData& Data)
{
	if (Data.EffectSpec.Def != nullptr && Data.EffectSpec.Def->IsA<UGolemArenaResourceRecoveryEffect>()
		&& (Data.EvaluatedData.Attribute == GetRuneEnergyAttribute() || Data.EvaluatedData.Attribute == GetStaminaAttribute()))
	{
		const UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent();
		const UGolemSkillRuntimeComponent* Runtime = ASC != nullptr && ASC->GetOwnerActor() != nullptr
			? ASC->GetOwnerActor()->FindComponentByClass<UGolemSkillRuntimeComponent>() : nullptr;
		if (Runtime != nullptr && !Runtime->GetBuild().SpecId.IsNone())
		{
			return false;
		}
	}
	return Super::PreGameplayEffectExecute(Data);
}

void UGolemAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);
	UGolemAbilitySystemComponent* AbilitySystem = Cast<UGolemAbilitySystemComponent>(GetOwningAbilitySystemComponent());
	if (Data.EvaluatedData.Attribute == GetIncomingDamageAttribute())
	{
		const float Damage = FMath::Max(0.0f, GetIncomingDamage());
		SetIncomingDamage(0.0f);
		if (AbilitySystem == nullptr || Damage <= 0.0f)
		{
			return;
		}
		UGolemLifeComponent* Life = AbilitySystem->GetLifeComponent();
		const bool bWasDowned = Life != nullptr && Life->IsDowned();
		const float PreviousHealth = GetHealth();
		SetHealth(FMath::Clamp(PreviousHealth - Damage, 0.0f, GetMaxHealth()));
		if (bWasDowned)
		{
			Life->NotifyDamageWhileDowned();
		}
		AActor* Source = Data.EffectSpec.GetContext().GetOriginalInstigator();
		AbilitySystem->PublishDamageConfirmation(Source, bWasDowned ? Damage : PreviousHealth - GetHealth());
		AbilitySystem->ExecuteGameplayCue(GolemGameplayTags::GameplayCue_Damage, Data.EffectSpec.GetContext());
		return;
	}
	SetHealth(FMath::Clamp(GetHealth(), 0.0f, GetMaxHealth()));
	SetStamina(FMath::Clamp(GetStamina(), 0.0f, GetMaxStamina()));
	SetRuneEnergy(FMath::Clamp(GetRuneEnergy(), 0.0f, GetMaxRuneEnergy()));
}

void UGolemAttributeSet::PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue)
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

void UGolemAttributeSet::OnRep_Health(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UGolemAttributeSet, Health, OldValue);
}

void UGolemAttributeSet::OnRep_PoiseBonus(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UGolemAttributeSet, PoiseBonus, OldValue);
}
void UGolemAttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UGolemAttributeSet, MaxHealth, OldValue);
}
void UGolemAttributeSet::OnRep_Stamina(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UGolemAttributeSet, Stamina, OldValue);
}
void UGolemAttributeSet::OnRep_MaxStamina(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UGolemAttributeSet, MaxStamina, OldValue);
}
void UGolemAttributeSet::OnRep_RuneEnergy(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UGolemAttributeSet, RuneEnergy, OldValue);
}
void UGolemAttributeSet::OnRep_MaxRuneEnergy(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UGolemAttributeSet, MaxRuneEnergy, OldValue);
}
void UGolemAttributeSet::OnRep_AttackPower(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UGolemAttributeSet, AttackPower, OldValue);
}
void UGolemAttributeSet::OnRep_Defense(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UGolemAttributeSet, Defense, OldValue);
}

void UGolemAttributeSet::OnRep_ArcaneResistance(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UGolemAttributeSet, ArcaneResistance, OldValue);
}

void UGolemAttributeSet::OnRep_LightningResistance(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UGolemAttributeSet, LightningResistance, OldValue);
}

void UGolemAttributeSet::OnRep_FrostResistance(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UGolemAttributeSet, FrostResistance, OldValue);
}

void UGolemAttributeSet::OnRep_FireResistance(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UGolemAttributeSet, FireResistance, OldValue);
}

void UGolemAttributeSet::OnRep_CritChance(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UGolemAttributeSet, CritChance, OldValue);
}

void UGolemAttributeSet::OnRep_CritMultiplier(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UGolemAttributeSet, CritMultiplier, OldValue);
}

void UGolemAttributeSet::OnRep_Haste(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UGolemAttributeSet, Haste, OldValue);
}

void UGolemAttributeSet::OnRep_CooldownRecovery(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UGolemAttributeSet, CooldownRecovery, OldValue);
}

void UGolemAttributeSet::OnRep_MasteryBonus(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UGolemAttributeSet, MasteryBonus, OldValue);
}

void UGolemAttributeSet::OnRep_Versatility(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UGolemAttributeSet, Versatility, OldValue);
}

void UGolemAttributeSet::OnRep_DamageReduction(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UGolemAttributeSet, DamageReduction, OldValue);
}
