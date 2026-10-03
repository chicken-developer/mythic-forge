#include "AbilityYard/KnightAbilityDummy.h"

#include "Attributes/KnightAttributeSet.h"
#include "Components/KnightAbilitySystemComponent.h"
#include "Components/KnightLifeComponent.h"
#include "Tags/KnightGameplayTags.h"

#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogKnightAbilityDummy, Log, All);

AKnightAbilityDummy::AKnightAbilityDummy()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	SetNetUpdateFrequency(20.0f);
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("DummyRoot")));
	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DummyBody"));
	Body->SetupAttachment(GetRootComponent());
	Body->SetCollisionProfileName(FName(TEXT("BlockAll")));
	Body->SetRelativeScale3D(FVector(1.0, 1.0, 2.0));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Mesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (Mesh.Succeeded())
	{
		Body->SetStaticMesh(Mesh.Object);
	}
	Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("DummyLabel"));
	Label->SetupAttachment(Body);
	Label->SetAbsolute(false, false, true);
	Label->SetRelativeLocation(FVector(0.0, 0.0, 100.0));
	Label->SetRelativeRotation(FRotator(0.0, 180.0, 0.0));
	Label->SetWorldSize(24.0f);
	Label->SetHorizontalAlignment(EHTA_Center);
	Label->SetText(NSLOCTEXT("KnightAbilityYard", "PulseDummy", "T: Test pulse target"));
	Label->SetTextRenderColor(FColor(255, 200, 80));
	AbilitySystem = CreateDefaultSubobject<UKnightAbilitySystemComponent>(TEXT("AbilitySystem"));
	Attributes = CreateDefaultSubobject<UKnightAttributeSet>(TEXT("Attributes"));
	Life = CreateDefaultSubobject<UKnightLifeComponent>(TEXT("Life"));
}

UAbilitySystemComponent* AKnightAbilityDummy::GetAbilitySystemComponent() const
{
	return AbilitySystem;
}

void AKnightAbilityDummy::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AKnightAbilityDummy, DummyId);
}

UKnightAbilitySystemComponent* AKnightAbilityDummy::GetKnightAbilitySystem() const
{
	return AbilitySystem;
}

UKnightAttributeSet* AKnightAbilityDummy::GetKnightAttributeSet() const
{
	return Attributes;
}

UKnightLifeComponent* AKnightAbilityDummy::GetLifeComponent() const
{
	return Life;
}

FKnightEntityId AKnightAbilityDummy::GetDummyId() const
{
	return DummyId;
}

const FKnightResult& AKnightAbilityDummy::GetInitializationResult() const
{
	return InitializationResult;
}

void AKnightAbilityDummy::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority() && !DummyId.IsValid())
	{
		DummyId = FKnightEntityId::Create();
	}
	InitializeAbilitySystem();
}

void AKnightAbilityDummy::InitializeAbilitySystem()
{
	if (!DummyId.IsValid())
	{
		return;
	}
	InitializationResult = AbilitySystem->InitializeKnightActor(this, Attributes, Life, DummyId,
		FKnightAttributeDefaults(), false);
	if (!InitializationResult.IsSuccess())
	{
		UE_LOG(LogKnightAbilityDummy, Error, TEXT("Dummy GAS initialization failed: %s."),
			*InitializationResult.Message.ToString());
		return;
	}
	if (HasAuthority() && !AbilitySystem->HasMatchingGameplayTag(KnightGameplayTags::Role_Enemy))
	{
		AbilitySystem->AddLooseGameplayTag(KnightGameplayTags::Role_Enemy, 1,
			EGameplayTagReplicationState::TagAndCountToAll);
	}
}

void AKnightAbilityDummy::OnRep_DummyId()
{
	InitializeAbilitySystem();
}
