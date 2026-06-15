// Fill out your copyright notice in the Description page of Project Settings.

#include "Player/MFPlayerState.h"
#include "AbilitySystem/MFAbilitySystemComponent.h"
#include "AbilitySystem/MFAttributeSet.h"

AMFPlayerState::AMFPlayerState()
{
	NetUpdateFrequency = 100.f;

	AbilitySystemComponent = CreateDefaultSubobject<UMFAbilitySystemComponent>("AbilitySystemComponent");
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	AttributeSet = CreateDefaultSubobject<UMFAttributeSet>("AttributeSet");
}

UAbilitySystemComponent* AMFPlayerState::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}
