// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Base/MFBaseCharacter.h"
#include "MFEnemyCharacter.generated.h"

/**
 *
 */
UCLASS()
class MYTHICFORGE_API AMFEnemyCharacter : public AMFBaseCharacter
{
	GENERATED_BODY()

public:
	AMFEnemyCharacter();

protected:
	virtual void BeginPlay() override;

	virtual void InitAbilityActorInfo() override;
};
