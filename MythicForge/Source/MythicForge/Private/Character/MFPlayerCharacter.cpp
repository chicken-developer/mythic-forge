// Fill out your copyright notice in the Description page of Project Settings.

#include "Character/MFPlayerCharacter.h"
#include "AbilitySystemComponent.h"
#include "Player/MFPlayerState.h"

AMFPlayerCharacter::AMFPlayerCharacter()
{
}

void AMFPlayerCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	// Server Init
	InitAbilityActorInfo();
}

void AMFPlayerCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	// Client Init
	InitAbilityActorInfo();
}

void AMFPlayerCharacter::InitAbilityActorInfo()
{
	AMFPlayerState* MFPlayerState = GetPlayerState<AMFPlayerState>();
	if (MFPlayerState)
	{
		MFPlayerState->GetAbilitySystemComponent()->InitAbilityActorInfo(MFPlayerState, this);
		AbilitySystemComponent = MFPlayerState->GetAbilitySystemComponent();
		AttributeSet = MFPlayerState->GetAttributeSet();
	}
}
