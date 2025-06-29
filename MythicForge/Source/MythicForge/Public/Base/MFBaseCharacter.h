// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "MFBaseCharacter.generated.h"

UCLASS(Abstract)
class MYTHICFORGE_API AMFBaseCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AMFBaseCharacter();

protected:
	virtual void BeginPlay() override;


};
