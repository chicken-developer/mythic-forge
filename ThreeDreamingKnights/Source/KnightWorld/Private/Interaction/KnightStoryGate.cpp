#include "Interaction/KnightStoryGate.h"

#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "Runtime/KnightStoryWorldComponent.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

AKnightStoryGate::AKnightStoryGate()
{
	PrimaryActorTick.bCanEverTick = false;
	Blocker = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GateBlocker"));
	SetRootComponent(Blocker);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	Blocker->SetStaticMesh(Cube.Object);
	Blocker->SetMobility(EComponentMobility::Movable);
	Blocker->SetCollisionProfileName(TEXT("BlockAll"));
	Blocker->SetCanEverAffectNavigation(true);
	Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("GateLabel"));
	Label->SetupAttachment(Blocker);
	Label->SetUsingAbsoluteScale(true);
	Label->SetRelativeLocation(FVector(0.0, -80.0, 100.0));
	Label->SetText(NSLOCTEXT("KnightGate", "Locked", "LOCKED GATE — use the lever"));
	Label->SetWorldSize(24.0f);
}

void AKnightStoryGate::BeginPlay()
{
	Super::BeginPlay();
	BindWorldState();
}

void AKnightStoryGate::BindWorldState()
{
	AGameStateBase* State = GetWorld()->GetGameState();
	UKnightStoryWorldComponent* Source = State != nullptr ? State->FindComponentByClass<UKnightStoryWorldComponent>() : nullptr;
	if (Source == nullptr)
	{
		if (++ReadinessAttempts < 100)
		{
			GetWorldTimerManager().SetTimer(ReadinessTimer, this, &ThisClass::BindWorldState, 0.1f, false);
		}
		return;
	}
	WorldState = Source;
	WorldHandle = Source->OnSnapshotChanged.AddUObject(this, &ThisClass::RefreshGate);
	RefreshGate();
}

void AKnightStoryGate::RefreshGate()
{
	bOpen = WorldState.IsValid() && !RequiredBranchId.IsNone() && !RequiredChoice.IsNone()
		&& WorldState->GetSnapshot().Facts.ContainsByPredicate([this](const FKnightWorldFact& Fact)
	{
		return Fact.EventName == TEXT("BranchCommitted") && Fact.SubjectId == RequiredBranchId && Fact.ValueId == RequiredChoice;
	});
	Blocker->SetCollisionEnabled(bOpen ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryAndPhysics);
	Blocker->SetVisibility(!bOpen);
	Label->SetText(bOpen ? NSLOCTEXT("KnightGate", "Open", "GATE OPEN")
		: NSLOCTEXT("KnightGate", "Locked", "LOCKED GATE — use the lever"));
}

bool AKnightStoryGate::IsOpen() const
{
	return bOpen;
}

void AKnightStoryGate::EndPlay(const EEndPlayReason::Type Reason)
{
	GetWorldTimerManager().ClearTimer(ReadinessTimer);
	if (WorldState.IsValid())
	{
		WorldState->OnSnapshotChanged.Remove(WorldHandle);
	}
	Super::EndPlay(Reason);
}
