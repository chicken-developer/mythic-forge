#include "Training/KnightTrainingMarker.h"

#include "Character/KnightCharacter.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Framework/KnightGameState.h"
#include "GameFramework/PlayerController.h"
#include "Logging/KnightLog.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

AKnightTrainingMarker::AKnightTrainingMarker()
{
	PrimaryActorTick.bCanEverTick = false;
	Trigger = CreateDefaultSubobject<UBoxComponent>(TEXT("TrainingTrigger"));
	SetRootComponent(Trigger);
	Trigger->SetBoxExtent(FVector(100.0, 100.0, 120.0));
	Trigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Trigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	Trigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Trigger->SetGenerateOverlapEvents(true);
	MarkerMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MarkerMesh"));
	MarkerMesh->SetupAttachment(Trigger);
	MarkerMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MarkerMesh->SetRelativeScale3D(FVector(1.8, 1.8, 0.08));
	MarkerMesh->SetRelativeLocation(FVector(0.0, 0.0, -85.0));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (Cylinder.Succeeded())
	{
		MarkerMesh->SetStaticMesh(Cylinder.Object);
	}
	static ConstructorHelpers::FObjectFinder<UMaterial> Material(TEXT("/Engine/BasicShapes/BasicShapeMaterial"));
	if (Material.Succeeded())
	{
		MarkerMesh->SetMaterial(0, Material.Object);
	}
	MarkerLabel = CreateDefaultSubobject<UTextRenderComponent>(TEXT("MarkerLabel"));
	MarkerLabel->SetupAttachment(Trigger);
	MarkerLabel->SetRelativeLocation(FVector(0.0, 0.0, 90.0));
	MarkerLabel->SetRelativeRotation(FRotator(0.0, 180.0, 0.0));
	MarkerLabel->SetHorizontalAlignment(EHTA_Center);
	MarkerLabel->SetWorldSize(28.0f);
}

void AKnightTrainingMarker::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	HandleTrainingProgress(FKnightTrainingProgress());
}

void AKnightTrainingMarker::BeginPlay()
{
	Super::BeginPlay();
	Trigger->OnComponentBeginOverlap.AddDynamic(this, &AKnightTrainingMarker::HandleOverlap);
	GameStateHandle = GetWorld()->GameStateSetEvent.AddUObject(this, &AKnightTrainingMarker::HandleGameStateSet);
	HandleGameStateSet(GetWorld()->GetGameState());
}

void AKnightTrainingMarker::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Trigger->OnComponentBeginOverlap.RemoveDynamic(this, &AKnightTrainingMarker::HandleOverlap);
	GetWorld()->GameStateSetEvent.Remove(GameStateHandle);
	if (TrainingSource.IsValid())
	{
		TrainingSource->OnTrainingProgressChanged.Remove(TrainingChangedHandle);
	}
	TrainingChangedHandle.Reset();
	TrainingSource.Reset();
	Super::EndPlay(EndPlayReason);
}

void AKnightTrainingMarker::HandleGameStateSet(AGameStateBase* InGameState)
{
	if (TrainingSource.IsValid())
	{
		TrainingSource->OnTrainingProgressChanged.Remove(TrainingChangedHandle);
	}
	TrainingChangedHandle.Reset();
	TrainingSource = Cast<AKnightGameState>(InGameState);
	HandleTrainingProgress(TrainingSource.IsValid() ? TrainingSource->GetTrainingProgress() : FKnightTrainingProgress());
	if (TrainingSource.IsValid())
	{
		TrainingChangedHandle = TrainingSource->OnTrainingProgressChanged.AddUObject(this,
			&AKnightTrainingMarker::HandleTrainingProgress);
	}
}

void AKnightTrainingMarker::HandleOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!HasAuthority() || !TrainingSource.IsValid())
	{
		return;
	}
	const AKnightCharacter* Knight = Cast<AKnightCharacter>(OtherActor);
	if (Knight == nullptr || !Knight->IsPlayerControlled())
	{
		return;
	}
	APlayerController* Controller = Cast<APlayerController>(Knight->GetController());
	const FKnightResult Result = TrainingSource->RecordTrainingMarker(Controller, MarkerIndex);
	if (!Result.IsSuccess())
	{
		UE_LOG(LogKnightCore, Warning, TEXT("Training marker %d rejected Knight %s: %s"),
			MarkerIndex, *Knight->GetKnightEntityId().Value.ToString(), *Result.Message.ToString());
	}
}

void AKnightTrainingMarker::HandleTrainingProgress(const FKnightTrainingProgress& Progress)
{
	const bool bIsReached = MarkerIndex >= 0 && MarkerIndex < AKnightGameState::RequiredMarkerCount
		&& (Progress.MarkerMask & (1 << MarkerIndex)) != 0;
	const FLinearColor DisplayColor = bIsReached ? FLinearColor::Green : MarkerColor;
	if (MarkerMaterial == nullptr)
	{
		MarkerMaterial = MarkerMesh->CreateDynamicMaterialInstance(0);
	}
	if (MarkerMaterial != nullptr)
	{
		MarkerMaterial->SetVectorParameterValue(TEXT("Color"), DisplayColor);
	}
	MarkerLabel->SetText(FText::Format(bIsReached
		? NSLOCTEXT("KnightTraining", "MarkerReached", "Marker {0} - reached")
		: NSLOCTEXT("KnightTraining", "MarkerPending", "Walk to marker {0}"), FText::AsNumber(MarkerIndex + 1)));
	MarkerLabel->SetTextRenderColor(DisplayColor.ToFColor(true));
}
