#include "SkillLab/GolemFrozenSurface.h"

#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

AGolemFrozenSurface::AGolemFrozenSurface()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	PrimaryActorTick.bCanEverTick = false;
	Tags.Add(TEXT("SkillLab.Freezable"));
	Surface = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FrozenSurface"));
	SetRootComponent(Surface);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	Surface->SetStaticMesh(Cube.Object);
	Surface->SetRelativeScale3D(FVector(4.0, 4.0, 0.15));
	Surface->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Surface->SetCanEverAffectNavigation(false);
	Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("SurfaceLabel"));
	Label->SetupAttachment(Surface);
	Label->SetRelativeLocation(FVector(0.0, 0.0, 120.0));
	Label->SetWorldSize(24.0f);
	Label->SetText(NSLOCTEXT("KnightSkillLab", "WaterSurface", "WATER: frost Runestone forms a bridge for 12s"));
}

void AGolemFrozenSurface::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ThisClass, bFrozen);
	DOREPLIFETIME(ThisClass, bWarning);
}

void AGolemFrozenSurface::FreezeSurface()
{
	if (!HasAuthority())
	{
		return;
	}
	bFrozen = true;
	bWarning = false;
	OnRep_Surface();
	GetWorldTimerManager().SetTimer(WarningTimer, this, &ThisClass::WarnThaw, 10.0f, false);
	GetWorldTimerManager().SetTimer(ThawTimer, this, &ThisClass::ThawSurface, 12.0f, false);
	ForceNetUpdate();
}

void AGolemFrozenSurface::WarnThaw()
{
	bWarning = true;
	OnRep_Surface();
	ForceNetUpdate();
}

void AGolemFrozenSurface::ThawSurface()
{
	const FBox Bounds = Surface->Bounds.GetBox();
	for (TActorIterator<ACharacter> It(GetWorld()); It; ++It)
	{
		const FVector Position = It->GetActorLocation();
		if (Position.X >= Bounds.Min.X - 50.0 && Position.X <= Bounds.Max.X + 50.0
			&& Position.Y >= Bounds.Min.Y - 50.0 && Position.Y <= Bounds.Max.Y + 50.0
			&& Position.Z >= Bounds.Min.Z && Position.Z <= Bounds.Max.Z + 180.0)
		{
			It->SetActorLocation(FVector(Bounds.Max.X + 150.0, Position.Y, Bounds.Max.Z + 100.0), false);
			It->GetCharacterMovement()->StopMovementImmediately();
		}
	}
	bFrozen = false;
	bWarning = false;
	OnRep_Surface();
	ForceNetUpdate();
}

void AGolemFrozenSurface::ResetSurface()
{
	if (!HasAuthority())
	{
		return;
	}
	GetWorldTimerManager().ClearTimer(WarningTimer);
	GetWorldTimerManager().ClearTimer(ThawTimer);
	ThawSurface();
}

void AGolemFrozenSurface::OnRep_Surface()
{
	Surface->SetCollisionEnabled(bFrozen ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	Surface->SetCollisionResponseToAllChannels(ECR_Block);
	Surface->SetRenderCustomDepth(bFrozen);
	Surface->SetCustomDepthStencilValue(bWarning ? 3 : 2);
	Label->SetText(bWarning ? NSLOCTEXT("KnightSkillLab", "ThawWarning", "WARNING: ice thaws in 2s — move to shore")
		: (bFrozen ? NSLOCTEXT("KnightSkillLab", "Frozen", "FROZEN BRIDGE — 12s")
		: NSLOCTEXT("KnightSkillLab", "Thawed", "WATER — activate frost Runestone")));
	Label->SetTextRenderColor(bWarning ? FColor::Orange : (bFrozen ? FColor::Cyan : FColor::Blue));
}

bool AGolemFrozenSurface::IsFrozen() const
{
	return bFrozen;
}
bool AGolemFrozenSurface::IsThawWarning() const
{
	return bWarning;
}

void AGolemFrozenSurface::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(WarningTimer);
	GetWorldTimerManager().ClearTimer(ThawTimer);
	Super::EndPlay(EndPlayReason);
}
