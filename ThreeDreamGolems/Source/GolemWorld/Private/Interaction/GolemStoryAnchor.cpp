#include "Interaction/GolemStoryAnchor.h"

#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "Runtime/GolemStoryWorldComponent.h"
#include "Types/GolemIdentifiableInterface.h"

AGolemStoryAnchor::AGolemStoryAnchor()
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

void AGolemStoryAnchor::RefreshPresentation()
{
	Caption->SetText(Label);
}

void AGolemStoryAnchor::BeginPlay()
{
	Super::BeginPlay();
	RefreshPresentation();
	Area->OnComponentBeginOverlap.AddDynamic(this, &ThisClass::HandleAreaEntered);
}

void AGolemStoryAnchor::HandleAreaEntered(UPrimitiveComponent* Overlapped, AActor* OtherActor,
	UPrimitiveComponent* OtherComponent, const int32 BodyIndex, const bool bFromSweep, const FHitResult& Hit)
{
	const IGolemIdentifiableInterface* Identity = Cast<IGolemIdentifiableInterface>(OtherActor);
	if (!HasAuthority() || AnchorId.IsNone() || Cast<APawn>(OtherActor) == nullptr
		|| Identity == nullptr || !Identity->GetGolemEntityId().IsValid())
	{
		return;
	}
	AGameStateBase* State = GetWorld()->GetGameState();
	UGolemStoryWorldComponent* WorldState = State != nullptr ? State->FindComponentByClass<UGolemStoryWorldComponent>() : nullptr;
	if (WorldState != nullptr)
	{
		WorldState->CommitFact(TEXT("EnterArea"), AnchorId);
	}
}

void AGolemStoryAnchor::EndPlay(const EEndPlayReason::Type Reason)
{
	Area->OnComponentBeginOverlap.RemoveDynamic(this, &ThisClass::HandleAreaEntered);
	Super::EndPlay(Reason);
}
