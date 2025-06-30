// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "MFBaseCharacter.generated.h"

UCLASS(Abstract)
class MYTHICFORGE_API AMFBaseCharacter : public ACharacter
{
	GENERATED_BODY()

private:

public:
	AMFBaseCharacter();

protected:
	UPROPERTY(EditAnywhere, Category="Combat")
	TObjectPtr<USkeletalMeshComponent> Weapon;

	FName WeaponHandSocket;

	virtual void BeginPlay() override;
};
