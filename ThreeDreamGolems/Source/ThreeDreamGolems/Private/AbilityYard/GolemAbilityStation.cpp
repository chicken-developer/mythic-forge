#include "AbilityYard/GolemAbilityStation.h"

#include "AbilityYard/GolemAbilityDummy.h"
#include "Attributes/GolemAttributeSet.h"
#include "Character/GolemCharacter.h"
#include "Components/GolemAbilitySystemComponent.h"
#include "Components/GolemLifeComponent.h"
#include "Party/GolemPartyState.h"

#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

AGolemAbilityStation::AGolemAbilityStation()
{
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("StationRoot")));
	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StationBody"));
	Body->SetupAttachment(GetRootComponent());
	Body->SetCollisionProfileName(FName(TEXT("NoCollision")));
	Body->SetRelativeScale3D(FVector(2.0, 2.0, 0.5));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Mesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (Mesh.Succeeded())
	{
		Body->SetStaticMesh(Mesh.Object);
	}
	Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("StationLabel"));
	Label->SetupAttachment(Body);
	Label->SetAbsolute(false, false, true);
	Label->SetRelativeLocation(FVector(0.0, 0.0, 180.0));
	Label->SetRelativeRotation(FRotator(0.0, 180.0, 0.0));
	Label->SetWorldSize(24.0f);
	Label->SetHorizontalAlignment(EHTA_Center);
}

void AGolemAbilityStation::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Label->SetText(StationKind == EGolemAbilityStationKind::DamagePlayer
		? NSLOCTEXT("GolemAbilityYard", "DamageStation", "F: Receive test damage")
		: NSLOCTEXT("GolemAbilityYard", "DownStation", "F: Down teammate / E: Revive"));
	Label->SetTextRenderColor(StationKind == EGolemAbilityStationKind::DamagePlayer
		? FColor(255, 170, 40) : FColor(100, 210, 255));
}

EGolemAbilityStationKind AGolemAbilityStation::GetStationKind() const
{
	return StationKind;
}

float AGolemAbilityStation::GetInteractionRange() const
{
	return InteractionRange;
}

void AGolemAbilityStation::SetStationKind(const EGolemAbilityStationKind InKind)
{
	StationKind = InKind;
	OnConstruction(GetActorTransform());
}

FGolemResult AGolemAbilityStation::TryActivate(AGolemCharacter* InstigatorGolem, AGolemPartyState* Party,
	AGolemAbilityDummy* DamageSource, AGolemCharacter*& OutAffectedGolem)
{
	OutAffectedGolem = nullptr;
	if (!HasAuthority())
	{
		return FGolemResult::Failure(EGolemError::NotAuthority, TEXT("Interact"),
			NSLOCTEXT("GolemAbilityYard", "StationAuthority", "Only the server may activate an exercise station."));
	}
	if (!IsValid(InstigatorGolem) || Party == nullptr || Party->FindGolem(InstigatorGolem) == nullptr
		|| !IsValid(DamageSource) || !InstigatorGolem->GetLifeComponent()->IsAlive())
	{
		return FGolemResult::Failure(EGolemError::InvalidState, TEXT("Interact"),
			NSLOCTEXT("GolemAbilityYard", "StationNotReady", "The Golem and the exercise must be alive and ready."));
	}
	if (FVector::DistSquared(GetActorLocation(), InstigatorGolem->GetActorLocation()) > FMath::Square(InteractionRange))
	{
		return FGolemResult::Failure(EGolemError::OutOfRange, TEXT("Interact"),
			NSLOCTEXT("GolemAbilityYard", "StationRange", "Move closer to the exercise station."));
	}
	if (StationKind == EGolemAbilityStationKind::DamagePlayer)
	{
		const FGolemResult Damage = UGolemAbilitySystemComponent::ApplyDamage(DamageSource, InstigatorGolem, TestDamage);
		if (Damage.IsSuccess())
		{
			OutAffectedGolem = InstigatorGolem;
		}
		return Damage;
	}
	AGolemCharacter* Target = nullptr;
	double ClosestDistance = FMath::Square(CompanionRange);
	for (const FGolemRosterEntry& Entry : Party->GetRoster())
	{
		AGolemCharacter* Candidate = Entry.Golem;
		if (!IsValid(Candidate) || Candidate == InstigatorGolem || !Candidate->GetLifeComponent()->IsAlive())
		{
			continue;
		}
		const double Distance = FVector::DistSquared(InstigatorGolem->GetActorLocation(), Candidate->GetActorLocation());
		if (Distance < ClosestDistance)
		{
			ClosestDistance = Distance;
			Target = Candidate;
		}
	}
	if (Target == nullptr)
	{
		return FGolemResult::Failure(EGolemError::OutOfRange, TEXT("Interact"),
			NSLOCTEXT("GolemAbilityYard", "CompanionRange", "Wait for a living teammate to approach the station."));
	}
	const FGolemResult Damage = UGolemAbilitySystemComponent::ApplyDamage(DamageSource, Target,
		Target->GetGolemAttributeSet()->GetMaxHealth() * 2.0f);
	if (Damage.IsSuccess())
	{
		OutAffectedGolem = Target;
	}
	return Damage;
}
