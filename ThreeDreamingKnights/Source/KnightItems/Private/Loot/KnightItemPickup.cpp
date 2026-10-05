#include "Loot/KnightItemPickup.h"

#include "Assets/KnightAssetManager.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Definitions/KnightItemDefinition.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

#define LOCTEXT_NAMESPACE "KnightItemPickup"

AKnightItemPickup::AKnightItemPickup()
{
	bReplicates = true;
	SetReplicateMovement(true);
	SetNetUpdateFrequency(10.0f);
	PrimaryActorTick.bCanEverTick = false;
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PickupMesh"));
	SetRootComponent(Mesh);
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Mesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	Mesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	Mesh->SetRelativeScale3D(FVector(0.4, 0.4, 0.4));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (Cube.Succeeded())
	{
		Mesh->SetStaticMesh(Cube.Object);
	}
	Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("PickupLabel"));
	Label->SetupAttachment(Mesh);
	Label->SetRelativeLocation(FVector(0.0, 0.0, 90.0));
	Label->SetRelativeScale3D(FVector(2.5));
	Label->SetHorizontalAlignment(EHTA_Center);
	Label->SetWorldSize(20.0f);
	Label->SetTextRenderColor(FColor(255, 215, 80));
}

void AKnightItemPickup::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AKnightItemPickup, View);
}

FKnightResult AKnightItemPickup::InitializePickup(const FGuid& InPickupId, const FKnightItemInstance& InItem)
{
	if (!HasAuthority() || GetWorld() == nullptr || GetWorld()->GetNetMode() == NM_Client)
	{
		return FKnightResult::Failure(EKnightError::NotAuthority, TEXT("PickupInitialize"),
			LOCTEXT("Authority", "Only the authoritative world may initialize a loot pickup."));
	}
	if (View.PickupId.IsValid() || !InPickupId.IsValid())
	{
		return FKnightResult::Failure(EKnightError::InvalidId, TEXT("PickupInitialize"),
			LOCTEXT("Identity", "A pickup requires a new stable identity and cannot be reinitialized."));
	}
	const FKnightResult Shape = InItem.ValidateIdentityAndCount();
	if (!Shape.IsSuccess())
	{
		return Shape;
	}
	UKnightAssetManager* Manager = UKnightAssetManager::GetKnightAssetManager();
	UKnightDefinition* Resolved = nullptr;
	if (Manager == nullptr)
	{
		return FKnightResult::Failure(EKnightError::LoadFailed, TEXT("PickupDefinition"),
			LOCTEXT("Catalog", "The typed production item catalog is unavailable."));
	}
	const FKnightResult Resolution = Manager->ResolveDefinition(InItem.DefinitionId,
		UKnightItemDefinition::StaticClass(), Resolved);
	if (!Resolution.IsSuccess())
	{
		return Resolution;
	}
	const UKnightItemDefinition* Definition = Cast<UKnightItemDefinition>(Resolved);
	if (Definition == nullptr || !Definition->ValidateItem().IsSuccess() || InItem.Count > Definition->GetMaximumStack())
	{
		return FKnightResult::Failure(EKnightError::InvalidData, TEXT("PickupDefinition"),
			LOCTEXT("Item", "The pickup count exceeds its actual validated item definition."));
	}
	View.PickupId = InPickupId;
	View.Item = InItem;
	RefreshPresentation();
	ForceNetUpdate();
	return FKnightResult::Success();
}

FGuid AKnightItemPickup::GetPickupId() const
{
	return View.PickupId;
}

FKnightPickupView AKnightItemPickup::GetView() const
{
	return View;
}

bool AKnightItemPickup::IsClaimed() const
{
	return View.bClaimed;
}

FKnightResult AKnightItemPickup::ReserveClaim(const FGuid& RequestId, FGuid& OutToken)
{
	OutToken.Invalidate();
	if (!HasAuthority() || !RequestId.IsValid() || !View.PickupId.IsValid() || IsActorBeingDestroyed()
		|| GetWorld() == nullptr || GetWorld()->bIsTearingDown)
	{
		return FKnightResult::Failure(EKnightError::InvalidState, TEXT("PickupReserve"),
			LOCTEXT("Unavailable", "The authoritative pickup is unavailable for this reservation."));
	}
	if (View.bClaimed || ReservedToken.IsValid())
	{
		return FKnightResult::Failure(EKnightError::Conflict, TEXT("PickupReserve"),
			LOCTEXT("Claimed", "This pickup was already consumed or reserved by another command."));
	}
	ReservedToken = FGuid::NewGuid();
	OutToken = ReservedToken;
	return FKnightResult::Success();
}

FKnightResult AKnightItemPickup::AbortClaim(const FGuid& Token)
{
	if (!Token.IsValid() || Token != ReservedToken || View.bClaimed)
	{
		return FKnightResult::Failure(EKnightError::NotOwned, TEXT("PickupAbort"),
			LOCTEXT("Reservation", "Only the current uncommitted pickup reservation may be aborted."));
	}
	ReservedToken.Invalidate();
	return FKnightResult::Success();
}

void AKnightItemPickup::CommitClaim(const FGuid& Token, const FGuid& RequestId)
{
	check(Token.IsValid() && Token == ReservedToken && !View.bClaimed && RequestId.IsValid());
	View.bClaimed = true;
	View.ClaimRequestId = RequestId;
	ReservedToken.Invalidate();
	ForceNetUpdate();
}

void AKnightItemPickup::PublishClaim()
{
	RefreshPresentation();
	OnPickupChanged.Broadcast();
}

void AKnightItemPickup::RefreshPresentation()
{
	Mesh->SetVisibility(!View.bClaimed);
	Mesh->SetCollisionEnabled(View.bClaimed ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryOnly);
	Label->SetVisibility(!View.bClaimed);
	if (View.PickupId.IsValid())
	{
		Label->SetText(FText::Format(LOCTEXT("Label", "{0} x{1}\nF: pick up"),
			FText::FromName(View.Item.DefinitionId.PrimaryAssetName), FText::AsNumber(View.Item.Count)));
	}
}

void AKnightItemPickup::OnRep_View()
{
	RefreshPresentation();
	OnPickupChanged.Broadcast();
}

#undef LOCTEXT_NAMESPACE
