#include "SkillLab/GolemRunestonePickup.h"

#include "Character/GolemCharacter.h"
#include "Attributes/GolemAttributeSet.h"
#include "Components/GolemLifeComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Events/GolemEventSubsystem.h"
#include "Events/GolemSkillLabEvent.h"
#include "Misc/ScopeExit.h"
#include "Net/UnrealNetwork.h"
#include "Skills/GolemSkillRuntimeComponent.h"
#include "UObject/ConstructorHelpers.h"

AGolemRunestonePickup::AGolemRunestonePickup()
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

void AGolemRunestonePickup::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority())
	{
		PickupId = FGolemEntityId::Create();
		SetLifeSpan(20.0f);
	}
	OnRep_Pickup();
}

void AGolemRunestonePickup::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ThisClass, Kind);
	DOREPLIFETIME(ThisClass, Charges);
	DOREPLIFETIME(ThisClass, PickupId);
}

void AGolemRunestonePickup::ConfigurePickup(const EGolemRunestonePickupKind InKind, const FGuid InEncounterId)
{
	if (!HasAuthority())
	{
		return;
	}
	Kind = InKind;
	Charges = Kind == EGolemRunestonePickupKind::Fire ? 2 : 1;
	EncounterId = InEncounterId;
	OnRep_Pickup();
}

FGolemResult AGolemRunestonePickup::TryCollect(AGolemCharacter* Golem, const FGuid RequestId)
{
	if (!HasAuthority() || !IsValid(Golem) || Golem->GetWorld() != GetWorld() || !RequestId.IsValid())
	{
		return FGolemResult::Failure(EGolemError::NotAuthority, TEXT("RunestonePickup"), FText::GetEmpty());
	}
	if (bCollecting || Charges <= 0 || ConsumedRequests.Contains(RequestId))
	{
		return FGolemResult::Failure(EGolemError::Conflict, TEXT("RunestonePickup"),
			NSLOCTEXT("GolemSkillLab", "PickupConsumed", "This pickup is already consumed or reserved."));
	}
	if (!Golem->GetLifeComponent()->IsAlive()
		|| FVector::DistSquared(Golem->GetActorLocation(), GetActorLocation()) > FMath::Square(200.0))
	{
		return FGolemResult::Failure(EGolemError::OutOfRange, TEXT("RunestonePickup"), FText::GetEmpty());
	}
	FHitResult Hit;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(GolemRunestonePickupLOS), false, Golem);
	Query.AddIgnoredActor(this);
	if (GetWorld()->LineTraceSingleByChannel(Hit, Golem->GetActorLocation(), GetActorLocation(), ECC_Visibility, Query))
	{
		return FGolemResult::Failure(EGolemError::OutOfRange, TEXT("RunestonePickupLOS"), FText::GetEmpty());
	}
	UGolemSkillRuntimeComponent* Runtime = Golem->FindComponentByClass<UGolemSkillRuntimeComponent>();
	if (Runtime == nullptr)
	{
		return FGolemResult::Failure(EGolemError::InvalidState, TEXT("RunestonePickupRuntime"), FText::GetEmpty());
	}
	bCollecting = true;
	ON_SCOPE_EXIT
	{
		bCollecting = false;
	};
	FGolemResult Result;
	if (Kind == EGolemRunestonePickupKind::PowerCrystal)
	{
		Result = Runtime->AddPowerCrystalBuff();
	}
	else
	{
		const UGolemAttributeSet* Attributes = Golem->GetGolemAttributeSet();
		if (Attributes->GetHealth() >= Attributes->GetMaxHealth())
		{
			return FGolemResult::Failure(EGolemError::InvalidState, TEXT("RunestoneHealingFull"),
				NSLOCTEXT("GolemSkillLab", "FullHP", "HP is full; the healing charge remains available."));
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
	if (UGolemEventSubsystem* Events = GetWorld()->GetSubsystem<UGolemEventSubsystem>())
	{
		FGolemSkillLabEvent Event;
		Event.EventName = TEXT("PickupConsumed");
		Event.ConfirmationId = FGuid::NewGuid();
		Event.EncounterId = EncounterId;
		Event.RequestId = RequestId;
		Event.SourceId = Golem->GetGolemEntityId();
		Event.TargetId = PickupId;
		Event.SubjectId = Kind == EGolemRunestonePickupKind::PowerCrystal ? TEXT("RS.Crystal") : TEXT("RS.Healing");
		Event.Revision = Revision;
		Events->PublishConfirmed(Event);
	}
	return Result;
}

void AGolemRunestonePickup::OnRep_Pickup()
{
	const bool bAvailable = Charges > 0;
	Visual->SetVisibility(bAvailable);
	Label->SetVisibility(bAvailable);
	const FText Name = Kind == EGolemRunestonePickupKind::PowerCrystal
		? NSLOCTEXT("GolemSkillLab", "CrystalPickup", "POWER CRYSTAL: F collect, +15% Power 10s")
		: (Kind == EGolemRunestonePickupKind::Fire
		? FText::Format(NSLOCTEXT("GolemSkillLab", "FirePickupCharges", "HEALING FIRE: F use, {0} charges, 15% max HP"),
			FText::AsNumber(Charges))
		: NSLOCTEXT("GolemSkillLab", "HealingPickup", "HEALING PICKUP: F collect, 15% max HP"));
	Label->SetText(Name);
	Label->SetTextRenderColor(Kind == EGolemRunestonePickupKind::PowerCrystal ? FColor::Yellow : FColor::Green);
}

int32 AGolemRunestonePickup::GetRemainingCharges() const
{
	return Charges;
}
EGolemRunestonePickupKind AGolemRunestonePickup::GetKind() const
{
	return Kind;
}
