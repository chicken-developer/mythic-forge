#include "RPGLoop/GolemRPGCharacter.h"
#include "Attributes/GolemAttributeSet.h"

#include "Progression/GolemRPGComponent.h"
#include "RPGLoop/GolemRPGGameState.h"
#include "RPGLoop/GolemRPGStateActor.h"
#include "Tags/GolemGameplayTags.h"
#include "Skills/GolemSkillRuntimeComponent.h"
#include "SkillLab/GolemSkillLabGameMode.h"
#include "GearLab/GolemGearAppearanceComponent.h"

#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "TimerManager.h"

AGolemRPGCharacter::AGolemRPGCharacter()
{
	SkillRuntime = CreateDefaultSubobject<UGolemSkillRuntimeComponent>(TEXT("SkillRuntime"));
	GearAppearance = CreateDefaultSubobject<UGolemGearAppearanceComponent>(TEXT("GearAppearance"));
	WeaponUpgradeVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WeaponUpgradeVisual"));
	WeaponUpgradeVisual->SetupAttachment(GetMesh(), TEXT("hand_r"));
	WeaponUpgradeVisual->SetRelativeLocation(FVector(0.0f, 0.0f, 60.0f));
	WeaponUpgradeVisual->SetRelativeScale3D(FVector(0.18f));
	WeaponUpgradeVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WeaponUpgradeVisual->SetCanEverAffectNavigation(false);
	WeaponUpgradeVisual->SetVisibility(false);
	RuneVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RuneVisual"));
	RuneVisual->SetupAttachment(GetMesh(), TEXT("hand_l"));
	RuneVisual->SetRelativeLocation(FVector(0.0f, 0.0f, 8.0f));
	RuneVisual->SetRelativeScale3D(FVector(0.14f));
	RuneVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RuneVisual->SetCanEverAffectNavigation(false);
	RuneVisual->SetVisibility(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (Sphere.Succeeded())
	{
		WeaponUpgradeVisual->SetStaticMesh(Sphere.Object);
		RuneVisual->SetStaticMesh(Sphere.Object);
	}
}

UStaticMeshComponent* AGolemRPGCharacter::GetWeaponUpgradeVisual() const
{
	return WeaponUpgradeVisual;
}

FGolemAttributeDefaults AGolemRPGCharacter::GetInitialAttributeDefaults() const
{
	FGolemAttributeDefaults Defaults = Super::GetInitialAttributeDefaults();
	if (GetWorld() != nullptr && GetWorld()->GetAuthGameMode<AGolemSkillLabGameMode>() != nullptr)
	{
		Defaults.MaxHealth = 1000.0f;
		Defaults.AttackPower = 100.0f;
		Defaults.Defense = 100.0f;
	}
	return Defaults;
}

UStaticMeshComponent* AGolemRPGCharacter::GetRuneVisual() const
{
	return RuneVisual;
}

void AGolemRPGCharacter::BeginPlay()
{
	Super::BeginPlay();
	const TArray<UStaticMeshComponent*> Components{WeaponUpgradeVisual, RuneVisual};
	const TArray<FLinearColor> Colors{FLinearColor(1.0f, 0.65f, 0.05f), FLinearColor(0.0f, 0.8f, 1.0f)};
	for (int32 Index = 0; Index < Components.Num(); ++Index)
	{
		UMaterialInstanceDynamic* Material = Components[Index]->CreateDynamicMaterialInstance(0);
		if (Material != nullptr)
		{
			Material->SetVectorParameterValue(TEXT("Color"), Colors[Index]);
			VisualMaterials.Add(Material);
		}
	}
	IdentityHandle = OnIdentityChanged.AddUObject(this, &ThisClass::RefreshEquipmentVisuals);
	BindRPGPresentation();
}

void AGolemRPGCharacter::BindRPGPresentation()
{
	if (bEndingRPGPlay || GetWorld() == nullptr)
	{
		return;
	}
	AGolemRPGGameState* State = GetWorld()->GetGameState<AGolemRPGGameState>();
	if (State == nullptr)
	{
		if (++BindingAttempts <= 100)
		{
			GetWorldTimerManager().SetTimer(InitialBindingTimer, this, &ThisClass::BindRPGPresentation, 0.1f, false);
		}
		return;
	}
	GetWorldTimerManager().ClearTimer(InitialBindingTimer);
	RPGGameState = State;
	StateReadyHandle = State->OnRPGStateChanged.AddUObject(this, &ThisClass::HandleRPGStateReady);
	HandleRPGStateReady(State->GetRPGState());
}

void AGolemRPGCharacter::HandleRPGStateReady(AGolemRPGStateActor* InState)
{
	if (RPGState.Get() != InState)
	{
		if (RPGState.IsValid())
		{
			RPGState->GetRPG()->OnSnapshotChanged.Remove(RPGHandle);
		}
		RPGHandle.Reset();
		RPGState = InState;
		if (IsValid(InState))
		{
			RPGHandle = InState->GetRPG()->OnSnapshotChanged.AddUObject(this, &ThisClass::RefreshEquipmentVisuals);
		}
	}
	RefreshEquipmentVisuals();
}

void AGolemRPGCharacter::RefreshEquipmentVisuals()
{
	if (bEndingRPGPlay)
	{
		return;
	}
	const FGolemRPGGolemView View = RPGState.IsValid()
		? RPGState->GetRPG()->GetGolemView(GetGolemEntityId()) : FGolemRPGGolemView();
	WeaponUpgradeVisual->SetVisibility(!View.bGearEnabled
		&& View.FindAssignment(GolemGameplayTags::Slot_Equipment_Weapon) != nullptr);
	RuneVisual->SetVisibility(View.FindAssignment(GolemGameplayTags::Slot_Equipment_Rune) != nullptr);
}

void AGolemRPGCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bEndingRPGPlay = true;
	GetWorldTimerManager().ClearTimer(InitialBindingTimer);
	OnIdentityChanged.Remove(IdentityHandle);
	if (RPGGameState.IsValid())
	{
		RPGGameState->OnRPGStateChanged.Remove(StateReadyHandle);
	}
	if (RPGState.IsValid())
	{
		RPGState->GetRPG()->OnSnapshotChanged.Remove(RPGHandle);
	}
	RPGGameState.Reset();
	RPGState.Reset();
	IdentityHandle.Reset();
	StateReadyHandle.Reset();
	RPGHandle.Reset();
	Super::EndPlay(EndPlayReason);
}
