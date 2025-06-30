// Fill out your copyright notice in the Description page of Project Settings.


#include "Base/MFBaseCharacter.h"

AMFBaseCharacter::AMFBaseCharacter() :
	WeaponHandSocket(FName("WeaponHandSocket"))
{
	PrimaryActorTick.bCanEverTick = false;
	Weapon = CreateDefaultSubobject<USkeletalMeshComponent>("Weapon");
	Weapon->SetupAttachment(GetMesh(), WeaponHandSocket);
	Weapon->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AMFBaseCharacter::BeginPlay()
{
	Super::BeginPlay();
	
}

