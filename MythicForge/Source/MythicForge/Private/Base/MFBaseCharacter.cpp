// Fill out your copyright notice in the Description page of Project Settings.


#include "Base/MFBaseCharacter.h"

AMFBaseCharacter::AMFBaseCharacter()
{
	PrimaryActorTick.bCanEverTick = false;
	Weapon = CreateDefaultSubobject<USkeletalMeshComponent>("Weapon");
	Weapon->SetupAttachment(GetRootComponent());

}

void AMFBaseCharacter::BeginPlay()
{
	Super::BeginPlay();
	
}

