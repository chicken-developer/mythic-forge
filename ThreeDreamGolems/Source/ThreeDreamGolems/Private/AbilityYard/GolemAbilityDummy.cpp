#include "AbilityYard/GolemAbilityDummy.h"

#include "Attributes/GolemAttributeSet.h"
#include "Components/GolemAbilitySystemComponent.h"
#include "Components/GolemLifeComponent.h"
#include "Tags/GolemGameplayTags.h"

#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogGolemAbilityDummy, Log, All);

AGolemAbilityDummy::AGolemAbilityDummy()
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
	Label->SetText(NSLOCTEXT("GolemAbilityYard", "PulseDummy", "T: Test pulse target"));
	Label->SetTextRenderColor(FColor(255, 200, 80));
	AbilitySystem = CreateDefaultSubobject<UGolemAbilitySystemComponent>(TEXT("AbilitySystem"));
	Attributes = CreateDefaultSubobject<UGolemAttributeSet>(TEXT("Attributes"));
	Life = CreateDefaultSubobject<UGolemLifeComponent>(TEXT("Life"));
}

UAbilitySystemComponent* AGolemAbilityDummy::GetAbilitySystemComponent() const
{
	return AbilitySystem;
}

void AGolemAbilityDummy::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AGolemAbilityDummy, DummyId);
}

UGolemAbilitySystemComponent* AGolemAbilityDummy::GetGolemAbilitySystem() const
{
	return AbilitySystem;
}

UGolemAttributeSet* AGolemAbilityDummy::GetGolemAttributeSet() const
{
	return Attributes;
}

UGolemLifeComponent* AGolemAbilityDummy::GetLifeComponent() const
{
	return Life;
}

FGolemEntityId AGolemAbilityDummy::GetDummyId() const
{
	return DummyId;
}

const FGolemResult& AGolemAbilityDummy::GetInitializationResult() const
{
	return InitializationResult;
}

void AGolemAbilityDummy::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority() && !DummyId.IsValid())
	{
		DummyId = FGolemEntityId::Create();
	}
	InitializeAbilitySystem();
}

void AGolemAbilityDummy::InitializeAbilitySystem()
{
	if (!DummyId.IsValid())
	{
		return;
	}
	InitializationResult = AbilitySystem->InitializeGolemActor(this, Attributes, Life, DummyId,
		FGolemAttributeDefaults(), false);
	if (!InitializationResult.IsSuccess())
	{
		UE_LOG(LogGolemAbilityDummy, Error, TEXT("Dummy GAS initialization failed: %s."),
			*InitializationResult.Message.ToString());
		return;
	}
	if (HasAuthority() && !AbilitySystem->HasMatchingGameplayTag(GolemGameplayTags::Role_Enemy))
	{
		AbilitySystem->AddLooseGameplayTag(GolemGameplayTags::Role_Enemy, 1,
			EGameplayTagReplicationState::TagAndCountToAll);
	}
}

void AGolemAbilityDummy::OnRep_DummyId()
{
	InitializeAbilitySystem();
}
