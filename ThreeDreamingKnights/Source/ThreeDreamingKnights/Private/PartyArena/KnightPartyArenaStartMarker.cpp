#include "PartyArena/KnightPartyArenaStartMarker.h"

#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

AKnightPartyArenaStartMarker::AKnightPartyArenaStartMarker()
{
	PrimaryActorTick.bCanEverTick = false;
	MarkerMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StartMarker"));
	SetRootComponent(MarkerMesh);
	MarkerMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MarkerMesh->SetRelativeScale3D(FVector(1.5, 1.5, 0.12));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (Cylinder.Succeeded())
	{
		MarkerMesh->SetStaticMesh(Cylinder.Object);
	}
	Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("StartLabel"));
	Label->SetupAttachment(MarkerMesh);
	Label->SetAbsolute(false, false, true);
	Label->SetRelativeLocation(FVector(0.0, 0.0, 220.0));
	Label->SetRelativeRotation(FRotator(0.0, 180.0, 0.0));
	Label->SetWorldSize(26.0f);
	Label->SetHorizontalAlignment(EHTA_Center);
	Label->SetTextRenderColor(FColor::Yellow);
	Label->SetText(NSLOCTEXT("KnightArena", "StartLabel", "F / D-pad Left: START ARENA\nR / Y: host retry"));
}

float AKnightPartyArenaStartMarker::GetInteractionRange() const
{
	return InteractionRange;
}
