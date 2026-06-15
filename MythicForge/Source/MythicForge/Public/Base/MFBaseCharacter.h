// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "MFBaseCharacter.generated.h"

class UAbilitySystemComponent;
class UAttributeSet;

UCLASS(Abstract)
class MYTHICFORGE_API AMFBaseCharacter : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

private:

public:
	AMFBaseCharacter();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	UAttributeSet* GetAttributeSet() const { return AttributeSet; }

protected:
	UPROPERTY(EditAnywhere, Category="Combat")
	TObjectPtr<UStaticMeshComponent> Weapon;

	FName WeaponHandSocket;

	UPROPERTY()
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY()
	TObjectPtr<UAttributeSet> AttributeSet;

	virtual void BeginPlay() override;

	virtual void InitAbilityActorInfo();
};
