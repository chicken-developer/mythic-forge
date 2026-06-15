// Fill out your copyright notice in the Description page of Project Settings.

#include "Character/MFEnemyCharacter.h"
#include "AbilitySystem/MFAbilitySystemComponent.h"
#include "AbilitySystem/MFAttributeSet.h"

AMFEnemyCharacter::AMFEnemyCharacter()
{
	AbilitySystemComponent = CreateDefaultSubobject<UMFAbilitySystemComponent>("AbilitySystemComponent");
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);

	AttributeSet = CreateDefaultSubobject<UMFAttributeSet>("AttributeSet");
}

void AMFEnemyCharacter::BeginPlay()
{
	Super::BeginPlay();

	InitAbilityActorInfo();
}

void AMFEnemyCharacter::InitAbilityActorInfo()
{
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->InitAbilityActorInfo(this, this);
	}
}
