// Fill out your copyright notice in the Description page of Project Settings.


#include "Base/MFBaseCharacter.h"
#include "AbilitySystemComponent.h"

AMFBaseCharacter::AMFBaseCharacter() :
	WeaponHandSocket(FName("WeaponHandSocket"))
{
	PrimaryActorTick.bCanEverTick = false;
	Weapon = CreateDefaultSubobject<UStaticMeshComponent>("Weapon");
	Weapon->SetupAttachment(GetMesh(), WeaponHandSocket);
	Weapon->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

UAbilitySystemComponent* AMFBaseCharacter::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void AMFBaseCharacter::BeginPlay()
{
	Super::BeginPlay();
	
}

void AMFBaseCharacter::InitAbilityActorInfo()
{
}
