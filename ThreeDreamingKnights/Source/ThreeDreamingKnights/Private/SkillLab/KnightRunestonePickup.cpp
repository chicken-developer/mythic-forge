#include "SkillLab/KnightRunestonePickup.h"

#include "Character/KnightCharacter.h"
#include "Attributes/KnightAttributeSet.h"
#include "Components/KnightLifeComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Events/KnightEventSubsystem.h"
#include "Events/KnightSkillLabEvent.h"
#include "Misc/ScopeExit.h"
#include "Net/UnrealNetwork.h"
#include "Skills/KnightSkillRuntimeComponent.h"
#include "UObject/ConstructorHelpers.h"

AKnightRunestonePickup::AKnightRunestonePickup()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	PrimaryActorTick.bCanEverTick = false;
	Visual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PickupVisual"));
	SetRootComponent(Visual);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	Visual->SetStaticMesh(Sphere.Object);
	Visual->SetRelativeScale3D(FVector(0.5));
	Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Visual->SetCanEverAffectNavigation(false);
	Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("PickupLabel"));
	Label->SetupAttachment(Visual);
	Label->SetRelativeLocation(FVector(0.0, 0.0, 100.0));
	Label->SetWorldSize(20.0f);
}

void AKnightRunestonePickup::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority())
	{
		PickupId = FKnightEntityId::Create();
		SetLifeSpan(20.0f);
	}
	OnRep_Pickup();
}

void AKnightRunestonePickup::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ThisClass, Kind);
	DOREPLIFETIME(ThisClass, Charges);
	DOREPLIFETIME(ThisClass, PickupId);
}

void AKnightRunestonePickup::ConfigurePickup(const EKnightRunestonePickupKind InKind, const FGuid InEncounterId)
{
	if (!HasAuthority())
	{
		return;
	}
	Kind = InKind;
	Charges = Kind == EKnightRunestonePickupKind::Fire ? 2 : 1;
	EncounterId = InEncounterId;
	OnRep_Pickup();
}

FKnightResult AKnightRunestonePickup::TryCollect(AKnightCharacter* Knight, const FGuid RequestId)
{
	if (!HasAuthority() || !IsValid(Knight) || Knight->GetWorld() != GetWorld() || !RequestId.IsValid())
	{
		return FKnightResult::Failure(EKnightError::NotAuthority, TEXT("RunestonePickup"), FText::GetEmpty());
	}
	if (bCollecting || Charges <= 0 || ConsumedRequests.Contains(RequestId))
	{
		return FKnightResult::Failure(EKnightError::Conflict, TEXT("RunestonePickup"),
			NSLOCTEXT("KnightSkillLab", "PickupConsumed", "This pickup is already consumed or reserved."));
	}
	if (!Knight->GetLifeComponent()->IsAlive()
		|| FVector::DistSquared(Knight->GetActorLocation(), GetActorLocation()) > FMath::Square(200.0))
	{
		return FKnightResult::Failure(EKnightError::OutOfRange, TEXT("RunestonePickup"), FText::GetEmpty());
	}
	FHitResult Hit;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(KnightRunestonePickupLOS), false, Knight);
	Query.AddIgnoredActor(this);
	if (GetWorld()->LineTraceSingleByChannel(Hit, Knight->GetActorLocation(), GetActorLocation(), ECC_Visibility, Query))
	{
		return FKnightResult::Failure(EKnightError::OutOfRange, TEXT("RunestonePickupLOS"), FText::GetEmpty());
	}
	UKnightSkillRuntimeComponent* Runtime = Knight->FindComponentByClass<UKnightSkillRuntimeComponent>();
	if (Runtime == nullptr)
	{
		return FKnightResult::Failure(EKnightError::InvalidState, TEXT("RunestonePickupRuntime"), FText::GetEmpty());
	}
	bCollecting = true;
	ON_SCOPE_EXIT
	{
		bCollecting = false;
	};
	FKnightResult Result;
	if (Kind == EKnightRunestonePickupKind::PowerCrystal)
	{
		Result = Runtime->AddPowerCrystalBuff();
	}
	else
	{
		const UKnightAttributeSet* Attributes = Knight->GetKnightAttributeSet();
		if (Attributes->GetHealth() >= Attributes->GetMaxHealth())
		{
			return FKnightResult::Failure(EKnightError::InvalidState, TEXT("RunestoneHealingFull"),
				NSLOCTEXT("KnightSkillLab", "FullHP", "HP is full; the healing charge remains available."));
		}
		Result = Runtime->ApplyExternalHeal(0.15f * Attributes->GetMaxHealth());
	}
	if (!Result.IsSuccess())
	{
		return Result;
	}
	--Charges;
	++Revision;
	ConsumedRequests.Add(RequestId);
	OnRep_Pickup();
	ForceNetUpdate();
	if (UKnightEventSubsystem* Events = GetWorld()->GetSubsystem<UKnightEventSubsystem>())
	{
		FKnightSkillLabEvent Event;
		Event.EventName = TEXT("PickupConsumed");
		Event.ConfirmationId = FGuid::NewGuid();
		Event.EncounterId = EncounterId;
		Event.RequestId = RequestId;
		Event.SourceId = Knight->GetKnightEntityId();
		Event.TargetId = PickupId;
		Event.SubjectId = Kind == EKnightRunestonePickupKind::PowerCrystal ? TEXT("RS.Crystal") : TEXT("RS.Healing");
		Event.Revision = Revision;
		Events->PublishConfirmed(Event);
	}
	return Result;
}

void AKnightRunestonePickup::OnRep_Pickup()
{
	const bool bAvailable = Charges > 0;
	Visual->SetVisibility(bAvailable);
	Label->SetVisibility(bAvailable);
	const FText Name = Kind == EKnightRunestonePickupKind::PowerCrystal
		? NSLOCTEXT("KnightSkillLab", "CrystalPickup", "POWER CRYSTAL: F collect, +15% Power 10s")
		: (Kind == EKnightRunestonePickupKind::Fire
		? FText::Format(NSLOCTEXT("KnightSkillLab", "FirePickupCharges", "HEALING FIRE: F use, {0} charges, 15% max HP"),
			FText::AsNumber(Charges))
		: NSLOCTEXT("KnightSkillLab", "HealingPickup", "HEALING PICKUP: F collect, 15% max HP"));
	Label->SetText(Name);
	Label->SetTextRenderColor(Kind == EKnightRunestonePickupKind::PowerCrystal ? FColor::Yellow : FColor::Green);
}

int32 AKnightRunestonePickup::GetRemainingCharges() const
{
	return Charges;
}
EKnightRunestonePickupKind AKnightRunestonePickup::GetKind() const
{
	return Kind;
}
