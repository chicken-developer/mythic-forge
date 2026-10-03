#include "CombatChallenge/KnightTraversalObstacle.h"

#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

AKnightTraversalObstacle::AKnightTraversalObstacle()
{
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("ObstacleRoot")));
	GetRootComponent()->SetMobility(EComponentMobility::Static);
	Obstacle = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Obstacle"));
	Obstacle->SetupAttachment(GetRootComponent());
	Obstacle->SetRelativeScale3D(FVector(0.8, 4.0, 0.9));
	Obstacle->SetMobility(EComponentMobility::Static);
	Obstacle->SetCollisionProfileName(TEXT("BlockAll"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (Cube.Succeeded())
	{
		Obstacle->SetStaticMesh(Cube.Object);
	}
	Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("TraversalLabel"));
	Label->SetupAttachment(Obstacle);
	Label->SetAbsolute(false, false, true);
	Label->SetRelativeLocation(FVector(0.0, 0.0, 100.0));
	Label->SetRelativeRotation(FRotator(0.0, 180.0, 0.0));
	Label->SetWorldSize(24.0f);
	Label->SetHorizontalAlignment(EHTA_Center);
	Label->SetTextRenderColor(FColor::Green);
	Label->SetText(NSLOCTEXT("KnightCombatChallenge", "Traversal", "Face hurdle within 240 cm: V / D-pad Down"));
}

UStaticMeshComponent* AKnightTraversalObstacle::GetObstacleMesh() const
{
	return Obstacle;
}
