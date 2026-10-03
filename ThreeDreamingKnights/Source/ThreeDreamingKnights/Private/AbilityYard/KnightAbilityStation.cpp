#include "AbilityYard/KnightAbilityStation.h"

#include "AbilityYard/KnightAbilityDummy.h"
#include "Attributes/KnightAttributeSet.h"
#include "Character/KnightCharacter.h"
#include "Components/KnightAbilitySystemComponent.h"
#include "Components/KnightLifeComponent.h"
#include "Party/KnightPartyState.h"

#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

AKnightAbilityStation::AKnightAbilityStation()
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

void AKnightAbilityStation::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Label->SetText(StationKind == EKnightAbilityStationKind::DamagePlayer
		? NSLOCTEXT("KnightAbilityYard", "DamageStation", "F: Receive test damage")
		: NSLOCTEXT("KnightAbilityYard", "DownStation", "F: Down teammate / E: Revive"));
	Label->SetTextRenderColor(StationKind == EKnightAbilityStationKind::DamagePlayer
		? FColor(255, 170, 40) : FColor(100, 210, 255));
}

EKnightAbilityStationKind AKnightAbilityStation::GetStationKind() const
{
	return StationKind;
}

float AKnightAbilityStation::GetInteractionRange() const
{
	return InteractionRange;
}

void AKnightAbilityStation::SetStationKind(const EKnightAbilityStationKind InKind)
{
	StationKind = InKind;
	OnConstruction(GetActorTransform());
}

FKnightResult AKnightAbilityStation::TryActivate(AKnightCharacter* InstigatorKnight, AKnightPartyState* Party,
	AKnightAbilityDummy* DamageSource, AKnightCharacter*& OutAffectedKnight)
{
	OutAffectedKnight = nullptr;
	if (!HasAuthority())
	{
		return FKnightResult::Failure(EKnightError::NotAuthority, TEXT("Interact"),
			NSLOCTEXT("KnightAbilityYard", "StationAuthority", "Only the server may activate an exercise station."));
	}
	if (!IsValid(InstigatorKnight) || Party == nullptr || Party->FindKnight(InstigatorKnight) == nullptr
		|| !IsValid(DamageSource) || !InstigatorKnight->GetLifeComponent()->IsAlive())
	{
		return FKnightResult::Failure(EKnightError::InvalidState, TEXT("Interact"),
			NSLOCTEXT("KnightAbilityYard", "StationNotReady", "The Knight and the exercise must be alive and ready."));
	}
	if (FVector::DistSquared(GetActorLocation(), InstigatorKnight->GetActorLocation()) > FMath::Square(InteractionRange))
	{
		return FKnightResult::Failure(EKnightError::OutOfRange, TEXT("Interact"),
			NSLOCTEXT("KnightAbilityYard", "StationRange", "Move closer to the exercise station."));
	}
	if (StationKind == EKnightAbilityStationKind::DamagePlayer)
	{
		const FKnightResult Damage = UKnightAbilitySystemComponent::ApplyDamage(DamageSource, InstigatorKnight, TestDamage);
		if (Damage.IsSuccess())
		{
			OutAffectedKnight = InstigatorKnight;
		}
		return Damage;
	}
	AKnightCharacter* Target = nullptr;
	double ClosestDistance = FMath::Square(CompanionRange);
	for (const FKnightRosterEntry& Entry : Party->GetRoster())
	{
		AKnightCharacter* Candidate = Entry.Knight;
		if (!IsValid(Candidate) || Candidate == InstigatorKnight || !Candidate->GetLifeComponent()->IsAlive())
		{
			continue;
		}
		const double Distance = FVector::DistSquared(InstigatorKnight->GetActorLocation(), Candidate->GetActorLocation());
		if (Distance < ClosestDistance)
		{
			ClosestDistance = Distance;
			Target = Candidate;
		}
	}
	if (Target == nullptr)
	{
		return FKnightResult::Failure(EKnightError::OutOfRange, TEXT("Interact"),
			NSLOCTEXT("KnightAbilityYard", "CompanionRange", "Wait for a living teammate to approach the station."));
	}
	const FKnightResult Damage = UKnightAbilitySystemComponent::ApplyDamage(DamageSource, Target,
		Target->GetKnightAttributeSet()->GetMaxHealth() * 2.0f);
	if (Damage.IsSuccess())
	{
		OutAffectedKnight = Target;
	}
	return Damage;
}
