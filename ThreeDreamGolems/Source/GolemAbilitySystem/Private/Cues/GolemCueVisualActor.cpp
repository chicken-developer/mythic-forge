#include "Cues/GolemCueVisualActor.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Tags/GolemGameplayTags.h"
#include "UObject/ConstructorHelpers.h"

AGolemCueVisualActor::AGolemCueVisualActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
	SetActorEnableCollision(false);
	CueRoot = CreateDefaultSubobject<USceneComponent>(TEXT("CueRoot"));
	SetRootComponent(CueRoot);
	Disc = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CueDisc"));
	Disc->SetupAttachment(CueRoot);
	Disc->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Disc->SetCanEverAffectNavigation(false);
	Disc->SetCastShadow(false);
	Disc->SetRelativeLocation(FVector(0.0, 0.0, -78.0));
	Disc->SetRelativeScale3D(FVector(1.8, 1.8, 0.04));
	Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("CueLabel"));
	Label->SetupAttachment(CueRoot);
	Label->SetRelativeLocation(FVector(0.0, 0.0, 130.0));
	Label->SetRelativeRotation(FRotator(0.0, 180.0, 0.0));
	Label->SetHorizontalAlignment(EHTA_Center);
	Label->SetWorldSize(28.0f);
	Label->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Mesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BaseMaterial(
		TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (Mesh.Succeeded())
	{
		Disc->SetStaticMesh(Mesh.Object);
	}
	if (BaseMaterial.Succeeded())
	{
		Disc->SetMaterial(0, BaseMaterial.Object);
	}
}

void AGolemCueVisualActor::Configure(AActor* Target, FGameplayTag InCueTag, bool bInPersistent)
{
	if (!IsValid(Target) || Target->GetRootComponent() == nullptr || !InCueTag.IsValid())
	{
		Destroy();
		return;
	}
	CueTag = InCueTag;
	bPersistent = bInPersistent;
	SetOwner(Target);
	AttachToComponent(Target->GetRootComponent(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	FLinearColor Color = FLinearColor(1.0f, 0.12f, 0.06f);
	FText Text = NSLOCTEXT("GolemCue", "Damage", "HIT");
	if (CueTag == GolemGameplayTags::GameplayCue_Downed)
	{
		Color = FLinearColor(1.0f, 0.7f, 0.05f);
		Text = NSLOCTEXT("GolemCue", "Downed", "DOWNED - hold E to revive");
	}
	else if (CueTag == GolemGameplayTags::GameplayCue_Revive)
	{
		Color = FLinearColor(0.1f, 1.0f, 0.25f);
		Text = bPersistent ? NSLOCTEXT("GolemCue", "Reviving", "REVIVING")
			: NSLOCTEXT("GolemCue", "Revived", "REVIVED");
	}
	else if (CueTag == GolemGameplayTags::GameplayCue_Parry)
	{
		Color = FLinearColor(0.2f, 0.9f, 1.0f);
		Text = NSLOCTEXT("GolemCue", "Parry", "PARRY");
	}
	else if (CueTag == GolemGameplayTags::GameplayCue_Stagger)
	{
		Color = FLinearColor(1.0f, 0.45f, 0.02f);
		Text = NSLOCTEXT("GolemCue", "Stagger", "STAGGERED");
	}
	else if (CueTag == GolemGameplayTags::GameplayCue_Rune)
	{
		Color = FLinearColor(0.55f, 0.2f, 1.0f);
		Text = NSLOCTEXT("GolemCue", "Rune", "RUNE");
	}
	Material = Disc->CreateDynamicMaterialInstance(0);
	if (Material != nullptr)
	{
		Material->SetVectorParameterValue(FName(TEXT("Color")), Color);
	}
	Label->SetText(Text);
	Label->SetTextRenderColor(Color.ToFColor(true));
	SetLifeSpan(bPersistent ? 0.0f : 0.65f);
}

FGameplayTag AGolemCueVisualActor::GetCueTag() const
{
	return CueTag;
}

bool AGolemCueVisualActor::IsPersistent() const
{
	return bPersistent;
}
