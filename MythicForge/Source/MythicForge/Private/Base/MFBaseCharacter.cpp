// Fill out your copyright notice in the Description page of Project Settings.


#include "Base/MFBaseCharacter.h"

// Sets default values
AMFBaseCharacter::AMFBaseCharacter()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

// Called when the game starts or when spawned
void AMFBaseCharacter::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void AMFBaseCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

// Called to bind functionality to input
void AMFBaseCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

}

