#include "RPGLoop/GolemRPGUpgradeStation.h"

#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

AGolemRPGUpgradeStation::AGolemRPGUpgradeStation()
{
	PrimaryActorTick.bCanEverTick = false;
	Platform = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("UpgradePlatform"));
	SetRootComponent(Platform);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Shape(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	Platform->SetStaticMesh(Shape.Object);
	Platform->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Platform->SetRelativeScale3D(FVector(4.0, 4.0, 0.15));
	Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("UpgradeLabel"));
	Label->SetupAttachment(Platform);
	Label->SetAbsolute(false, false, true);
	Label->SetRelativeLocation(FVector(0.0, 0.0, 150.0));
	Label->SetWorldSize(28.0f);
	Label->SetTextRenderColor(FColor::Cyan);
	Label->SetHorizontalAlignment(EHTA_Center);
	Label->SetText(NSLOCTEXT("GolemRPG", "UpgradeStation", "UPGRADE: I / Menu\nEquip, Rune, Skills, Respec\nContinue to encounter B"));
}

float AGolemRPGUpgradeStation::GetInteractionRange() const
{
	return InteractionRange;
}
