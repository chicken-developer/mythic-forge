#include "Projectiles/GolemColorProjectile.h"

#include "AbilitySystemGlobals.h"
#include "Attributes/GolemAttributeSet.h"
#include "Components/GolemAbilitySystemComponent.h"
#include "Components/GolemCombatComponent.h"
#include "Components/GolemLifeComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "Tags/GolemGameplayTags.h"
#include "UObject/ConstructorHelpers.h"
#include "Voids/GolemColorVoidComponent.h"

namespace
{
	UGolemAbilitySystemComponent* GolemProjectileASC(AActor* Actor)
	{
		return IsValid(Actor) ? Cast<UGolemAbilitySystemComponent>(
			UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Actor, true)) : nullptr;
	}
}

AGolemColorProjectile::AGolemColorProjectile()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicatingMovement(true);
	SetNetUpdateFrequency(60.0f);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShapeMaterial(
		TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	Visual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Visual"));
	SetRootComponent(Visual);
	Visual->SetStaticMesh(SphereMesh.Object);
	Visual->SetMaterial(0, ShapeMaterial.Object);
	// The host sweeps for hits itself; the mesh is only what players see.
	Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Visual->SetCanEverAffectNavigation(false);
	Visual->SetRelativeScale3D(FVector(0.36f));
}

void AGolemColorProjectile::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AGolemColorProjectile, ColorMask);
	DOREPLIFETIME(AGolemColorProjectile, VisualRadius);
}

void AGolemColorProjectile::BeginPlay()
{
	Super::BeginPlay();
	RefreshVisual();
}

bool AGolemColorProjectile::Launch(AActor* InSource, const FVector& Direction, const FGolemProjectileShot& InShot)
{
	const FVector Normal = Direction.GetSafeNormal();
	if (!HasAuthority() || bLaunched || !IsValid(InSource) || Normal.IsNearlyZero()
		|| !FMath::IsFinite(InShot.Speed) || InShot.Speed <= 0.0f || !FMath::IsFinite(InShot.Range) || InShot.Range <= 0.0f
		|| !FMath::IsFinite(InShot.Radius) || InShot.Radius <= 0.0f)
	{
		return false;
	}
	Source = InSource;
	Shot = InShot;
	FlightDirection = Normal;
	ColorMask = Shot.bAppliesColor ? GolemElements::Mask(Shot.Element) : 0;
	VisualRadius = Shot.Radius;
	bLaunched = true;
	SetActorRotation(Normal.Rotation());
	RefreshVisual();
	return true;
}

void AGolemColorProjectile::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (HasAuthority())
	{
		AdvanceBy(DeltaSeconds);
	}
}

void AGolemColorProjectile::AdvanceBy(const float DeltaSeconds)
{
	UWorld* World = GetWorld();
	if (!HasAuthority() || !bLaunched || bFinished || World == nullptr || !FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0.0f)
	{
		return;
	}
	const float Step = FMath::Min(Shot.Speed * DeltaSeconds, Shot.Range - DistanceFlown);
	const FVector Start = GetActorLocation();
	const FVector End = Start + FlightDirection * Step;
	TArray<FHitResult> Hits;
	SweepPath(World, Source.Get(), this, Start, End, Shot.Radius, Hits);
	for (const FHitResult& Hit : Hits)
	{
		if (ResolveHit(Hit))
		{
			SetActorLocation(Start + FlightDirection * FMath::Clamp(Hit.Distance, 0.0f, Step));
			Finish();
			return;
		}
	}
	SetActorLocation(End);
	DistanceFlown += Step;
	if (DistanceFlown >= Shot.Range - UE_KINDA_SMALL_NUMBER)
	{
		Finish();
	}
}

void AGolemColorProjectile::SweepPath(const UWorld* World, const AActor* InSource, const AActor* Ignored,
	const FVector& Start, const FVector& End, const float Radius, TArray<FHitResult>& OutHits)
{
	OutHits.Reset();
	if (World == nullptr)
	{
		return;
	}
	FCollisionQueryParams Query(SCENE_QUERY_STAT(GolemColorProjectile), false, Ignored);
	Query.AddIgnoredActor(InSource);
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_Pawn);
	Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
	Objects.AddObjectTypesToQuery(ECC_WorldStatic);
	World->SweepMultiByObjectType(OutHits, Start, End, FQuat::Identity, Objects, FCollisionShape::MakeSphere(Radius), Query);
	// Whatever the shot reaches first decides: a void in front of the body takes the colour, a void behind it does not.
	OutHits.StableSort([](const FHitResult& Left, const FHitResult& Right)
	{
		return Left.Distance < Right.Distance;
	});
}

bool AGolemColorProjectile::StopsShot(const AActor* InSource, const FHitResult& Hit)
{
	AActor* Target = Hit.GetActor();
	const UPrimitiveComponent* Component = Hit.GetComponent();
	if (!IsValid(Target) || !IsValid(Component) || !Component->IsQueryCollisionEnabled() || Target == InSource)
	{
		return false;
	}
	const UGolemAbilitySystemComponent* TargetASC = GolemProjectileASC(Target);
	if (TargetASC == nullptr)
	{
		// Walls and floors stop the shot; triggers and markers without a body do not.
		return Component->GetCollisionObjectType() == ECC_WorldStatic;
	}
	// Shots pass through living teammates. A restored Xenon is no longer a target but still a solid body.
	return UGolemCombatComponent::AreHostile(InSource, Target)
		|| TargetASC->GetLifeComponent() == nullptr || !TargetASC->GetLifeComponent()->IsAlive();
}

UPrimitiveComponent* AGolemColorProjectile::FindFirstStop(const UWorld* World, const AActor* InSource,
	const FVector& Start, const FVector& End, const float Radius)
{
	TArray<FHitResult> Hits;
	SweepPath(World, InSource, nullptr, Start, End, Radius, Hits);
	for (const FHitResult& Hit : Hits)
	{
		if (StopsShot(InSource, Hit))
		{
			return Hit.GetComponent();
		}
	}
	return nullptr;
}

bool AGolemColorProjectile::ResolveHit(const FHitResult& Hit)
{
	AActor* Target = Hit.GetActor();
	UPrimitiveComponent* Component = Hit.GetComponent();
	AActor* SourceActor = Source.Get();
	if (!StopsShot(SourceActor, Hit))
	{
		return false;
	}
	UGolemAbilitySystemComponent* TargetASC = GolemProjectileASC(Target);
	if (TargetASC == nullptr || !UGolemCombatComponent::AreHostile(SourceActor, Target))
	{
		return true;
	}
	UGolemColorVoidComponent* Void = Cast<UGolemColorVoidComponent>(Component);
	if (Void != nullptr && Shot.bAppliesColor && Void->AcceptsHits()
		&& (!Shot.bRestrictColor || (Shot.OnlyVoid.Get() == Void
			&& (!Shot.bOnlyIfPrimed || Void->GetVoidState() == EGolemVoidState::Primed))))
	{
		Void->ApplyElement(Shot.Element, Shot.ColorHoldSeconds, SourceActor);
		return true;
	}
	if (Shot.Damage > 0.0f && TargetASC->GetGolemAttributes() != nullptr)
	{
		const TWeakObjectPtr<UGolemAbilitySystemComponent> WeakTarget = TargetASC;
		const float Before = TargetASC->GetGolemAttributes()->GetHealth();
		const FGolemResult Result = UGolemAbilitySystemComponent::ApplyCombatDamage(SourceActor, Target, Shot.Damage, true, &Hit);
		UGolemCombatComponent* SourceCombat = IsValid(SourceActor)
			? SourceActor->FindComponentByClass<UGolemCombatComponent>() : nullptr;
		if (Result.IsSuccess() && WeakTarget.IsValid() && WeakTarget->GetGolemAttributes() != nullptr && SourceCombat != nullptr)
		{
			const float Applied = Before - WeakTarget->GetGolemAttributes()->GetHealth();
			if (Applied > 0.0f)
			{
				SourceCombat->PublishOutcome(Shot.ActionTag == GolemGameplayTags::Ability_Combat_Heavy
					? EGolemCombatOutcome::HeavyHit : EGolemCombatOutcome::LightHit, Shot.ActionTag, Target, Applied);
			}
		}
	}
	return true;
}

void AGolemColorProjectile::Finish()
{
	if (!bFinished)
	{
		bFinished = true;
		SetActorTickEnabled(false);
		// A short life after the hit lets the last position reach clients before the actor goes away.
		SetLifeSpan(0.15f);
	}
}

void AGolemColorProjectile::OnRep_ColorMask()
{
	RefreshVisual();
}

void AGolemColorProjectile::RefreshVisual()
{
	if (Visual == nullptr)
	{
		return;
	}
	// The engine sphere is 100 cm wide.
	Visual->SetRelativeScale3D(FVector(FMath::Max(VisualRadius, 1.0f) * 2.0f / 100.0f));
	if (VisualMaterial == nullptr && GetNetMode() != NM_DedicatedServer)
	{
		VisualMaterial = Visual->CreateDynamicMaterialInstance(0);
	}
	if (VisualMaterial != nullptr)
	{
		VisualMaterial->SetVectorParameterValue(TEXT("Color"), GolemElements::DisplayColor(ColorMask));
	}
}
