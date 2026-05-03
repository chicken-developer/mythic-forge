// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Base/MFBaseCharacter.h"
#include "Interaction/EnemyInterface.h"
#include "MFEnemy.generated.h"

/**
 * 
 */
UCLASS()
class MYTHICFORGE_API AMFEnemy : public AMFBaseCharacter, public IEnemyInterface
{
	GENERATED_BODY()

public:
	virtual void HighlightActor() override;
	virtual void UnHighlightActor() override;

	UPROPERTY(BlueprintReadOnly)
	bool bHighlighted = false;

};
