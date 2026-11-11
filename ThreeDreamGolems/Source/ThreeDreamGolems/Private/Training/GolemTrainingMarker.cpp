#include "Training/GolemTrainingMarker.h"

#include "Character/GolemCharacter.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Framework/GolemGameState.h"
#include "GameFramework/PlayerController.h"
#include "Logging/GolemLog.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

AGolemTrainingMarker::AGolemTrainingMarker()
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

void AGolemTrainingMarker::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	HandleTrainingProgress(FGolemTrainingProgress());
}

void AGolemTrainingMarker::BeginPlay()
{
	Super::BeginPlay();
	Trigger->OnComponentBeginOverlap.AddDynamic(this, &AGolemTrainingMarker::HandleOverlap);
	GameStateHandle = GetWorld()->GameStateSetEvent.AddUObject(this, &AGolemTrainingMarker::HandleGameStateSet);
	HandleGameStateSet(GetWorld()->GetGameState());
}

void AGolemTrainingMarker::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Trigger->OnComponentBeginOverlap.RemoveDynamic(this, &AGolemTrainingMarker::HandleOverlap);
	GetWorld()->GameStateSetEvent.Remove(GameStateHandle);
	if (TrainingSource.IsValid())
	{
		TrainingSource->OnTrainingProgressChanged.Remove(TrainingChangedHandle);
	}
	TrainingChangedHandle.Reset();
	TrainingSource.Reset();
	Super::EndPlay(EndPlayReason);
}

void AGolemTrainingMarker::HandleGameStateSet(AGameStateBase* InGameState)
{
	if (TrainingSource.IsValid())
	{
		TrainingSource->OnTrainingProgressChanged.Remove(TrainingChangedHandle);
	}
	TrainingChangedHandle.Reset();
	TrainingSource = Cast<AGolemGameState>(InGameState);
	HandleTrainingProgress(TrainingSource.IsValid() ? TrainingSource->GetTrainingProgress() : FGolemTrainingProgress());
	if (TrainingSource.IsValid())
	{
		TrainingChangedHandle = TrainingSource->OnTrainingProgressChanged.AddUObject(this,
			&AGolemTrainingMarker::HandleTrainingProgress);
	}
}

void AGolemTrainingMarker::HandleOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!HasAuthority() || !TrainingSource.IsValid())
	{
		return;
	}
	const AGolemCharacter* Golem = Cast<AGolemCharacter>(OtherActor);
	if (Golem == nullptr || !Golem->IsPlayerControlled())
	{
		return;
	}
	APlayerController* Controller = Cast<APlayerController>(Golem->GetController());
	const FGolemResult Result = TrainingSource->RecordTrainingMarker(Controller, MarkerIndex);
	if (!Result.IsSuccess())
	{
		UE_LOG(LogGolemCore, Warning, TEXT("Training marker %d rejected Golem %s: %s"),
			MarkerIndex, *Golem->GetGolemEntityId().Value.ToString(), *Result.Message.ToString());
	}
}

void AGolemTrainingMarker::HandleTrainingProgress(const FGolemTrainingProgress& Progress)
{
	const bool bIsReached = MarkerIndex >= 0 && MarkerIndex < AGolemGameState::RequiredMarkerCount
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
		? NSLOCTEXT("GolemTraining", "MarkerReached", "Marker {0} - reached")
		: NSLOCTEXT("GolemTraining", "MarkerPending", "Walk to marker {0}"), FText::AsNumber(MarkerIndex + 1)));
	MarkerLabel->SetTextRenderColor(DisplayColor.ToFColor(true));
}
