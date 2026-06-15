// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Base/MFBaseCharacter.h"
#include "MFPlayerCharacter.generated.h"

/**
 *
 */
UCLASS()
class MYTHICFORGE_API AMFPlayerCharacter : public AMFBaseCharacter
{
	GENERATED_BODY()

public:
	AMFPlayerCharacter();

	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_PlayerState() override;

	virtual void InitAbilityActorInfo() override;
};
