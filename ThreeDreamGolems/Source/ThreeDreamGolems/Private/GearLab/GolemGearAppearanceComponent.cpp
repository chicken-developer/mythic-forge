#include "GearLab/GolemGearAppearanceComponent.h"

#include "Assets/GolemAssetManager.h"
#include "Character/GolemCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Definitions/GolemItemDefinition.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Inventory/GolemInventoryComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Progression/GolemRPGComponent.h"
#include "RPGLoop/GolemRPGGameState.h"
#include "RPGLoop/GolemRPGStateActor.h"
#include "TimerManager.h"

UGolemGearAppearanceComponent::UGolemGearAppearanceComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

UStaticMeshComponent* UGolemGearAppearanceComponent::CreatePart(const FName SlotName,
	const FVector& Location, const FVector& Scale, UStaticMesh* Mesh, const bool bWeapon)
{
	AGolemCharacter* Golem = Cast<AGolemCharacter>(GetOwner());
	UStaticMeshComponent* Part = NewObject<UStaticMeshComponent>(Golem);
	Golem->AddInstanceComponent(Part);
	Part->SetStaticMesh(Mesh);
	Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Part->SetCanEverAffectNavigation(false);
	Part->RegisterComponent();
	const bool bSocketReady = bWeapon && Golem->GetMesh()->GetSkeletalMeshAsset() != nullptr;
	Part->AttachToComponent(bSocketReady ? Golem->GetMesh() : Golem->GetRootComponent(),
		FAttachmentTransformRules::KeepRelativeTransform, bSocketReady ? FName(TEXT("hand_r")) : NAME_None);
	Part->SetRelativeLocation(Location);
	Part->SetRelativeScale3D(Scale);
	Part->SetVisibility(false);
	Parts.Add(Part);
	PartSlots.Add(SlotName);
	return Part;
}

void UGolemGearAppearanceComponent::BeginPlay()
{
	Super::BeginPlay();
	if (Cast<AGolemCharacter>(GetOwner()) == nullptr)
	{
		return;
	}
	Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	Cone = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cone.Cone"));
	if (Cube == nullptr || Sphere == nullptr || Cone == nullptr)
	{
		return;
	}
	CreatePart(TEXT("Head"), FVector(0.0, 0.0, 76.0), FVector(0.38, 0.38, 0.14), Cube);
	CreatePart(TEXT("Chest"), FVector(0.0, 0.0, 30.0), FVector(0.50, 0.36, 0.40), Cube);
	CreatePart(TEXT("Waist"), FVector(0.0, 0.0, -8.0), FVector(0.44, 0.38, 0.10), Cube);
	for (const double Side : {-1.0, 1.0})
	{
		CreatePart(TEXT("Shoulders"), FVector(0.0, Side * 35.0, 55.0), FVector(0.28, 0.22, 0.16), Cube);
		CreatePart(TEXT("Wrists"), FVector(5.0, Side * 42.0, 5.0), FVector(0.13, 0.15, 0.11), Cube);
		CreatePart(TEXT("Hands"), FVector(7.0, Side * 43.0, -8.0), FVector(0.18, 0.18, 0.19), Cube);
		CreatePart(TEXT("Feet"), FVector(12.0, Side * 17.0, -80.0), FVector(0.28, 0.18, 0.17), Cube);
	}
	CreatePart(TEXT("Neck"), FVector(25.0, 0.0, 50.0), FVector(0.07), Sphere);
	CreatePart(TEXT("Relic"), FVector(26.0, 0.0, 12.0), FVector(0.10), Cone);
	CreatePart(TEXT("Weapon"), FVector(0.0, 0.0, 38.0), FVector(0.05, 0.05, 0.74), Cube, true);
	CreatePart(TEXT("Weapon"), FVector(0.0, 0.0, 77.0), FVector(0.44, 0.18, 0.23), Cube, true);
	CreatePart(TEXT("Weapon"), FVector(-20.0, 0.0, 84.0), FVector(0.10, 0.22, 0.20), Cube, true);
	CreatePart(TEXT("Weapon"), FVector(20.0, 0.0, 84.0), FVector(0.10, 0.22, 0.20), Cube, true);
	CreatePart(TEXT("Weapon"), FVector(0.0, 0.0, 99.0), FVector(0.18), Cone, true);
	BindSource();
}

void UGolemGearAppearanceComponent::BindSource()
{
	AGolemRPGGameState* State = GetWorld()->GetGameState<AGolemRPGGameState>();
	if (State == nullptr)
	{
		if (++Attempts < 100)
		{
			GetWorld()->GetTimerManager().SetTimer(ReadinessTimer, this, &ThisClass::BindSource, 0.1f, false);
		}
		return;
	}
	GameState = State;
	StateHandle = State->OnRPGStateChanged.AddUObject(this, &ThisClass::HandleStateReady);
	HandleStateReady(State->GetRPGState());
}

void UGolemGearAppearanceComponent::HandleStateReady(AGolemRPGStateActor* State)
{
	if (RPGState.IsValid())
	{
		RPGState->GetRPG()->OnSnapshotChanged.Remove(RPGHandle);
		RPGState->GetInventory()->OnSnapshotChanged.Remove(InventoryHandle);
	}
	RPGState = State;
	if (State != nullptr)
	{
		RPGHandle = State->GetRPG()->OnSnapshotChanged.AddUObject(this, &ThisClass::RefreshAppearance);
		InventoryHandle = State->GetInventory()->OnSnapshotChanged.AddUObject(this, &ThisClass::RefreshAppearance);
	}
	RefreshAppearance();
}

void UGolemGearAppearanceComponent::RefreshAppearance()
{
	const AGolemCharacter* Golem = Cast<AGolemCharacter>(GetOwner());
	const FGolemRPGGolemView GolemView = RPGState.IsValid()
		? RPGState->GetRPG()->GetGolemView(Golem->GetGolemEntityId()) : FGolemRPGGolemView();
	WeaponSilhouette = NAME_None;
	for (int32 Index = 0; Index < Parts.Num(); ++Index)
	{
		const FName Name = PartSlots[Index];
		const FGolemEquipmentAssignment* Assignment = GolemView.Assignments.FindByPredicate(
			[Name](const FGolemEquipmentAssignment& Entry)
			{
				return Entry.SlotTag.ToString().EndsWith(TEXT(".") + Name.ToString());
			});
		Parts[Index]->SetVisibility(GolemView.bGearEnabled && Assignment != nullptr);
		if (!GolemView.bGearEnabled || Assignment == nullptr)
		{
			continue;
		}
		UGolemDefinition* Resolved = nullptr;
		UGolemAssetManager* Manager = UGolemAssetManager::GetGolemAssetManager();
		const UGolemItemDefinition* Definition = Manager != nullptr
			&& Manager->ResolveDefinition(Assignment->DefinitionId, UGolemItemDefinition::StaticClass(), Resolved).IsSuccess()
			? Cast<UGolemItemDefinition>(Resolved) : nullptr;
		if (Definition == nullptr)
		{
			continue;
		}
		const FGolemItemMetadata& Metadata = Definition->GetMetadata();
		const FLinearColor Color = Metadata.Rarity == EGolemItemRarity::Epic ? FLinearColor(0.65f, 0.15f, 0.90f)
			: Metadata.Rarity == EGolemItemRarity::Rare ? FLinearColor(0.10f, 0.40f, 0.85f)
			: FLinearColor(0.45f, 0.60f, 0.40f);
		UMaterialInstanceDynamic* Material = Cast<UMaterialInstanceDynamic>(Parts[Index]->GetMaterial(0));
		if (Material == nullptr)
		{
			Material = Parts[Index]->CreateDynamicMaterialInstance(0);
		}
		if (Material != nullptr)
		{
			Material->SetVectorParameterValue(TEXT("Color"), Color);
		}
		if (Name == TEXT("Weapon"))
		{
			if (Golem->GetMesh()->GetSkeletalMeshAsset() != nullptr
				&& Parts[Index]->GetAttachParent() != Golem->GetMesh())
			{
				Parts[Index]->AttachToComponent(Golem->GetMesh(), FAttachmentTransformRules::KeepRelativeTransform,
					TEXT("hand_r"));
			}
			WeaponSilhouette = Metadata.Theme;
			const int32 Style = FCString::Atoi(*Metadata.Theme.ToString().Mid(1));
			const int32 Part = Index - (Parts.Num() - 5);
			if (Part == 1)
			{
				Parts[Index]->SetStaticMesh(Style == 3 || Style == 6 ? Sphere : Style == 7 ? Cone : Cube);
				Parts[Index]->SetRelativeScale3D(Style == 3 ? FVector(0.45, 0.13, 0.45)
					: Style == 4 ? FVector(0.54, 0.15, 0.18) : Style == 6 ? FVector(0.32)
					: Style == 7 ? FVector(0.42, 0.28, 0.35) : FVector(0.44, 0.18, 0.23));
			}
			else if (Part >= 2)
			{
				Parts[Index]->SetVisibility((Part < 4 && (Style == 2 || Style >= 4)) || (Part == 4 && Style >= 5));
				Parts[Index]->SetStaticMesh(Style == 6 || Style == 7 ? Cone : Cube);
			}
		}
	}
}

int32 UGolemGearAppearanceComponent::GetVisiblePartCount(const FName SlotName) const
{
	int32 Count = 0;
	for (int32 Index = 0; Index < Parts.Num(); ++Index)
	{
		Count += PartSlots[Index] == SlotName && Parts[Index]->IsVisible() ? 1 : 0;
	}
	return Count;
}

FName UGolemGearAppearanceComponent::GetWeaponSilhouette() const
{
	return WeaponSilhouette;
}

void UGolemGearAppearanceComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	GetWorld()->GetTimerManager().ClearTimer(ReadinessTimer);
	if (GameState.IsValid())
	{
		GameState->OnRPGStateChanged.Remove(StateHandle);
	}
	if (RPGState.IsValid())
	{
		RPGState->GetRPG()->OnSnapshotChanged.Remove(RPGHandle);
		RPGState->GetInventory()->OnSnapshotChanged.Remove(InventoryHandle);
	}
	for (UStaticMeshComponent* Part : Parts)
	{
		if (IsValid(Part))
		{
			Part->DestroyComponent();
		}
	}
	Parts.Reset();
	Super::EndPlay(Reason);
}
