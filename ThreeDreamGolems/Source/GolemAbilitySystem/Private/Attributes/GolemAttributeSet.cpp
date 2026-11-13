#include "Attributes/GolemAttributeSet.h"

#include "Components/GolemAbilitySystemComponent.h"
#include "Components/GolemLifeComponent.h"
#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"
#include "Tags/GolemGameplayTags.h"

namespace
{
	constexpr float GolemMaxAttributeValue = 1000000.0f;
	constexpr float GolemMaxColorHoldSeconds = 60.0f;
}

FGolemResult FGolemAttributeDefaults::Validate() const
{
	if (!FMath::IsFinite(MaxHealth) || MaxHealth < 1.0f || MaxHealth > GolemMaxAttributeValue ||
		!FMath::IsFinite(Power) || Power < 0.0f || Power > GolemMaxAttributeValue ||
		!FMath::IsFinite(Agility) || Agility < 1.0f || Agility > GolemMaxAttributeValue ||
		!FMath::IsFinite(ColorHold) || ColorHold < 0.0f || ColorHold > GolemMaxColorHoldSeconds)
	{
		return FGolemResult::Failure(EGolemError::InvalidData, TEXT("AttributeDefaults"),
			NSLOCTEXT("GolemGAS", "AttributeDefaultsInvalid", "Attribute defaults must be finite and within supported bounds."));
	}
	return FGolemResult::Success();
}

UGolemAttributeSet::UGolemAttributeSet()
{
	const FGolemAttributeDefaults Defaults;
	InitHealth(Defaults.MaxHealth);
	InitMaxHealth(Defaults.MaxHealth);
	InitPower(Defaults.Power);
	InitAgility(Defaults.Agility);
	InitColorHold(Defaults.ColorHold);
	InitIncomingDamage(0.0f);
}

void UGolemAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION_NOTIFY(UGolemAttributeSet, Health, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UGolemAttributeSet, MaxHealth, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UGolemAttributeSet, Power, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UGolemAttributeSet, Agility, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UGolemAttributeSet, ColorHold, COND_None, REPNOTIFY_Always);
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
	else if (Attribute == GetMaxHealthAttribute() || Attribute == GetAgilityAttribute())
	{
		Value = FMath::Clamp(Value, 1.0f, GolemMaxAttributeValue);
	}
	else if (Attribute == GetColorHoldAttribute())
	{
		Value = FMath::Clamp(Value, 0.0f, GolemMaxColorHoldSeconds);
	}
	else
	{
		Value = FMath::Clamp(Value, 0.0f, GolemMaxAttributeValue);
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
}

void UGolemAttributeSet::OnRep_Health(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UGolemAttributeSet, Health, OldValue);
}

void UGolemAttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UGolemAttributeSet, MaxHealth, OldValue);
}

void UGolemAttributeSet::OnRep_Power(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UGolemAttributeSet, Power, OldValue);
}

void UGolemAttributeSet::OnRep_Agility(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UGolemAttributeSet, Agility, OldValue);
}

void UGolemAttributeSet::OnRep_ColorHold(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UGolemAttributeSet, ColorHold, OldValue);
}
