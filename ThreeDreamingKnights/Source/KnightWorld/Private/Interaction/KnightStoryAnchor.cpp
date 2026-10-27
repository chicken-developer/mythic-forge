#include "Interaction/KnightStoryAnchor.h"

#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "Runtime/KnightStoryWorldComponent.h"
#include "Types/KnightIdentifiableInterface.h"

AKnightStoryAnchor::AKnightStoryAnchor()
{
	PrimaryActorTick.bCanEverTick = false;
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Marker"));
	SetRootComponent(Mesh);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Shape(TEXT("/Engine/BasicShapes/Cube.Cube"));
	Mesh->SetStaticMesh(Shape.Object);
	Mesh->SetCollisionProfileName(TEXT("BlockAll"));
	Mesh->SetRelativeScale3D(FVector(0.8, 0.8, 1.0));
	Caption = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Caption"));
	Caption->SetupAttachment(Mesh);
	Caption->SetRelativeLocation(FVector(0.0, 0.0, 120.0));
	Caption->SetWorldSize(24.0f);
	Caption->SetHorizontalAlignment(EHTA_Center);
	Area = CreateDefaultSubobject<UBoxComponent>(TEXT("AreaTrigger"));
	Area->SetupAttachment(Mesh);
	Area->SetBoxExtent(FVector(180.0));
	Area->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Area->SetCollisionResponseToAllChannels(ECR_Ignore);
	Area->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Area->SetGenerateOverlapEvents(true);
}

void AKnightStoryAnchor::RefreshPresentation()
{
	Caption->SetText(Label);
}

void AKnightStoryAnchor::BeginPlay()
{
	Super::BeginPlay();
	RefreshPresentation();
	Area->OnComponentBeginOverlap.AddDynamic(this, &ThisClass::HandleAreaEntered);
}

void AKnightStoryAnchor::HandleAreaEntered(UPrimitiveComponent* Overlapped, AActor* OtherActor,
	UPrimitiveComponent* OtherComponent, const int32 BodyIndex, const bool bFromSweep, const FHitResult& Hit)
{
	const IKnightIdentifiableInterface* Identity = Cast<IKnightIdentifiableInterface>(OtherActor);
	if (!HasAuthority() || AnchorId.IsNone() || Cast<APawn>(OtherActor) == nullptr
		|| Identity == nullptr || !Identity->GetKnightEntityId().IsValid())
	{
		return;
	}
	AGameStateBase* State = GetWorld()->GetGameState();
	UKnightStoryWorldComponent* WorldState = State != nullptr ? State->FindComponentByClass<UKnightStoryWorldComponent>() : nullptr;
	if (WorldState != nullptr)
	{
		WorldState->CommitFact(TEXT("EnterArea"), AnchorId);
	}
}

void AKnightStoryAnchor::EndPlay(const EEndPlayReason::Type Reason)
{
	Area->OnComponentBeginOverlap.RemoveDynamic(this, &ThisClass::HandleAreaEntered);
	Super::EndPlay(Reason);
}
