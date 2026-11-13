#include "Xenon/GolemXenon.h"

#include "Attributes/GolemAttributeSet.h"
#include "Components/CapsuleComponent.h"
#include "Components/GolemAbilitySystemComponent.h"
#include "Components/GolemLifeComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "Tags/GolemGameplayTags.h"
#include "UObject/ConstructorHelpers.h"
#include "Voids/GolemColorVoidComponent.h"
#include "Voids/GolemVoidHostComponent.h"
#include "Xenon/GolemXenonBrainComponent.h"
#include "Xenon/GolemXenonDefinition.h"

namespace
{
	const FLinearColor GolemXenonGray(0.22f, 0.22f, 0.24f);
	const FLinearColor GolemXenonDulled(0.08f, 0.08f, 0.08f);
}

AGolemXenon::AGolemXenon()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetNetUpdateFrequency(30.0f);
	AutoPossessAI = EAutoPossessAI::Disabled;

	AbilitySystem = CreateDefaultSubobject<UGolemAbilitySystemComponent>(TEXT("AbilitySystem"));
	AbilitySystem->SetIsReplicated(true);
	AbilitySystem->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);
	Attributes = CreateDefaultSubobject<UGolemAttributeSet>(TEXT("Attributes"));
	Life = CreateDefaultSubobject<UGolemLifeComponent>(TEXT("Life"));
	VoidHost = CreateDefaultSubobject<UGolemVoidHostComponent>(TEXT("VoidHost"));
	Brain = CreateDefaultSubobject<UGolemXenonBrainComponent>(TEXT("Brain"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShapeMaterial(
		TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(GetCapsuleComponent());
	Body->SetStaticMesh(CylinderMesh.Object);
	Body->SetMaterial(0, ShapeMaterial.Object);
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body->SetRelativeScale3D(FVector(0.75f, 0.75f, 1.8f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	TelegraphZone = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TelegraphZone"));
	TelegraphZone->SetupAttachment(GetCapsuleComponent());
	TelegraphZone->SetStaticMesh(CubeMesh.Object);
	TelegraphZone->SetMaterial(0, ShapeMaterial.Object);
	TelegraphZone->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TelegraphZone->SetCanEverAffectNavigation(false);
	TelegraphZone->SetVisibility(false);

	for (int32 Index = 0; Index <= UGolemXenonDefinition::MaxVoidSlots; ++Index)
	{
		const bool bCore = Index == UGolemXenonDefinition::MaxVoidSlots;
		UGolemColorVoidComponent* Void = CreateDefaultSubobject<UGolemColorVoidComponent>(
			bCore ? FName(TEXT("CoreVoid")) : FName(*FString::Printf(TEXT("Void%d"), Index)));
		Void->SetupAttachment(GetCapsuleComponent());
		UStaticMeshComponent* Visual = CreateDefaultSubobject<UStaticMeshComponent>(
			bCore ? FName(TEXT("CoreVoidVisual")) : FName(*FString::Printf(TEXT("VoidVisual%d"), Index)));
		Visual->SetupAttachment(Void);
		Visual->SetStaticMesh(SphereMesh.Object);
		Visual->SetMaterial(0, ShapeMaterial.Object);
		Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Visual->SetVisibility(false);
		VoidVisuals.Add(Visual);
		if (bCore)
		{
			CoreVoid = Void;
		}
		else
		{
			VoidPool.Add(Void);
		}
	}
}

UAbilitySystemComponent* AGolemXenon::GetAbilitySystemComponent() const
{
	return AbilitySystem;
}

void AGolemXenon::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AGolemXenon, DefinitionId);
	DOREPLIFETIME(AGolemXenon, XenonId);
	DOREPLIFETIME(AGolemXenon, ReplicatedBodyScale);
}

void AGolemXenon::BeginPlay()
{
	Super::BeginPlay();
	BodyMaterial = Body->CreateDynamicMaterialInstance(0);
	VoidMaterials.Reset();
	for (UStaticMeshComponent* Visual : VoidVisuals)
	{
		VoidMaterials.Add(Visual->CreateDynamicMaterialInstance(0));
	}
	for (UGolemColorVoidComponent* Void : VoidPool)
	{
		Void->OnVoidChanged.AddUObject(this, &AGolemXenon::HandleVoidChanged);
		HandleVoidChanged(Void);
	}
	CoreVoid->OnVoidChanged.AddUObject(this, &AGolemXenon::HandleVoidChanged);
	HandleVoidChanged(CoreVoid);
	VoidHost->OnHostChanged.AddUObject(this, &AGolemXenon::HandleHostChanged);
	TelegraphMaterial = TelegraphZone->CreateDynamicMaterialInstance(0);
	if (TelegraphMaterial != nullptr)
	{
		TelegraphMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor(1.0f, 0.25f, 0.05f));
	}
	Brain->OnActionChanged.AddUObject(this, &AGolemXenon::HandleBrainChanged);
	HandleBrainChanged();
	RefreshBody();
}

bool AGolemXenon::IsRestored() const
{
	return VoidHost != nullptr && VoidHost->IsRestored();
}

FGolemResult AGolemXenon::InitializeXenon(const UGolemXenonDefinition* InDefinition, const int32 Seed,
	const float MixedChanceOverride)
{
	if (!HasAuthority())
	{
		return FGolemResult::Failure(EGolemError::NotAuthority, TEXT("InitializeXenon"), FText::GetEmpty());
	}
	if (bInitialized)
	{
		return FGolemResult::Failure(EGolemError::InvalidState, TEXT("XenonAlreadyInitialized"), FText::GetEmpty());
	}
	if (!IsValid(InDefinition))
	{
		return FGolemResult::Failure(EGolemError::InvalidData, TEXT("XenonDefinition"), FText::GetEmpty());
	}
	const FGolemResult Validation = InDefinition->ValidateXenon();
	if (!Validation.IsSuccess())
	{
		return Validation;
	}
	FGolemAttributeDefaults Defaults;
	Defaults.MaxHealth = InDefinition->GetMaxHealth();
	Defaults.Power = InDefinition->GetContactDamage();
	const FGolemEntityId NewId = FGolemEntityId::Create();
	const FGolemResult AbilityResult = AbilitySystem->InitializeGolemActor(this, Attributes, Life, NewId, Defaults, false);
	if (!AbilityResult.IsSuccess())
	{
		return AbilityResult;
	}
	AbilitySystem->SetLooseGameplayTagCount(GolemGameplayTags::Role_Enemy, 1, EGameplayTagReplicationState::TagAndCountToAll);

	const TArray<FGolemXenonVoidSlot>& SlotData = InDefinition->GetVoidSlots();
	TArray<UGolemColorVoidComponent*> Slots;
	for (int32 Index = 0; Index < SlotData.Num(); ++Index)
	{
		VoidPool[Index]->SetRelativeLocation(SlotData[Index].Offset);
		VoidPool[Index]->SetSphereRadius(SlotData[Index].Radius);
		Slots.Add(VoidPool[Index]);
	}
	CoreVoid->SetRelativeLocation(InDefinition->GetCoreSlot().Offset);
	CoreVoid->SetSphereRadius(InDefinition->GetCoreSlot().Radius);
	const FGolemResult HostResult = VoidHost->InitializeHost(AbilitySystem, InDefinition->GetVoidTuning(), Slots,
		InDefinition->BuildColorPlan(Seed, MixedChanceOverride), CoreVoid);
	if (!HostResult.IsSuccess())
	{
		return HostResult;
	}
	const FGolemResult BrainResult = Brain->InitializeBrain(AbilitySystem, VoidHost, InDefinition->GetBehavior());
	if (!BrainResult.IsSuccess())
	{
		return BrainResult;
	}
	DefinitionId = InDefinition->GetPrimaryAssetId();
	XenonId = NewId;
	ReplicatedBodyScale = InDefinition->GetBodyScale();
	SetActorScale3D(FVector(ReplicatedBodyScale));
	bInitialized = true;
	RefreshBody();
	ForceNetUpdate();
	return FGolemResult::Success();
}

FLinearColor AGolemXenon::ColorForMask(const uint8 ElementMask)
{
	return GolemElements::DisplayColor(ElementMask);
}

void AGolemXenon::HandleVoidChanged(UGolemColorVoidComponent* Void)
{
	const int32 Index = Void == CoreVoid ? UGolemXenonDefinition::MaxVoidSlots : VoidPool.IndexOfByKey(Void);
	if (!VoidVisuals.IsValidIndex(Index) || !VoidMaterials.IsValidIndex(Index) || VoidMaterials[Index] == nullptr)
	{
		return;
	}
	const EGolemVoidState State = Void->GetVoidState();
	UStaticMeshComponent* Visual = VoidVisuals[Index];
	Visual->SetVisibility(State != EGolemVoidState::Hidden);
	// The engine sphere is 100 cm wide; match it to the collision radius, and swell a primed void a little.
	const float Diameter = Void->GetUnscaledSphereRadius() * 2.0f / 100.0f;
	Visual->SetRelativeScale3D(FVector(Diameter * (State == EGolemVoidState::Primed ? 1.25f : 1.0f)));
	const FLinearColor Required = ColorForMask(Void->GetRequiredMask());
	VoidMaterials[Index]->SetVectorParameterValue(TEXT("Color"),
		State == EGolemVoidState::Dulled ? GolemXenonDulled
		: State == EGolemVoidState::Primed ? ColorForMask(Void->GetAppliedMask())
		: Required);
}

void AGolemXenon::HandleHostChanged()
{
	RefreshBody();
}

void AGolemXenon::HandleBrainChanged()
{
	const bool bShow = Brain->GetAction() == EGolemXenonAction::Telegraph && Brain->GetZoneRange() > 0.0f;
	TelegraphZone->SetVisibility(bShow);
	if (!bShow)
	{
		return;
	}
	// A flat slab on the floor covering the wedge: as long as the reach, as wide as the arc at its end.
	// The actor is scaled, so sizes are divided by that scale; the engine cube is 100 cm.
	const float Scale = FMath::Max(static_cast<float>(GetActorScale3D().X), 0.01f);
	const float Range = Brain->GetZoneRange();
	const float Width = 2.0f * Range * FMath::Sin(FMath::DegreesToRadians(FMath::Min(Brain->GetZoneHalfAngleDegrees(), 90.0f)));
	const float LocalYaw = FMath::FindDeltaAngleDegrees(static_cast<float>(GetActorRotation().Yaw), Brain->GetZoneYaw());
	const float HalfHeight = GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
	TelegraphZone->SetRelativeRotation(FRotator(0.0, LocalYaw, 0.0));
	TelegraphZone->SetRelativeLocation(FRotator(0.0, LocalYaw, 0.0).RotateVector(FVector(Range * 0.5f / Scale, 0.0, 0.0))
		+ FVector(0.0, 0.0, -HalfHeight + 1.5f));
	TelegraphZone->SetRelativeScale3D(FVector(Range / 100.0f / Scale, Width / 100.0f / Scale, 0.02f / Scale));
}

void AGolemXenon::RefreshBody()
{
	if (BodyMaterial != nullptr)
	{
		// A restored Xenon gets its colour back; until then the body is grey.
		BodyMaterial->SetVectorParameterValue(TEXT("Color"), IsRestored() ? FLinearColor(0.85f, 0.75f, 0.45f) : GolemXenonGray);
	}
}

void AGolemXenon::OnRep_Identity()
{
	SetActorScale3D(FVector(ReplicatedBodyScale));
	RefreshBody();
}
