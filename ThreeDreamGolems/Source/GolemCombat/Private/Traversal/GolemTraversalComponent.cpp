#include "Traversal/GolemTraversalComponent.h"

#include "Abilities/GameplayAbility.h"
#include "AbilitySystemGlobals.h"
#include "AIController.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Camera/GolemCameraModeComponent.h"
#include "BrainComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/GolemAbilitySystemComponent.h"
#include "Components/GolemCombatComponent.h"
#include "Components/GolemLifeComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "MotionWarpingComponent.h"
#include "Net/UnrealNetwork.h"
#include "Tags/GolemGameplayTags.h"

UGolemTraversalComponent::UGolemTraversalComponent()
{
	SetIsReplicatedByDefault(true);
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

FGolemResult UGolemTraversalComponent::TryBeginTraversal(UAnimMontage* Montage, FName WarpTargetName,
	FGuid& OutHandle, UGameplayAbility* SourceAbility)
{
	OutHandle.Invalidate();
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	UGolemAbilitySystemComponent* ASC = Character != nullptr
		? Cast<UGolemAbilitySystemComponent>(UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Character)) : nullptr;
	UMotionWarpingComponent* Warping = Character != nullptr
		? Character->FindComponentByClass<UMotionWarpingComponent>() : nullptr;
	if (Character == nullptr || !Character->HasAuthority())
	{
		return FGolemResult::Failure(EGolemError::NotAuthority, TEXT("BeginTraversal"),
			NSLOCTEXT("GolemCombat", "TraversalAuthority", "Traversal is validated by the server."));
	}
	if (bEndingPlay || HasActiveTraversal() || ASC == nullptr || ASC->GetLifeComponent() == nullptr
		|| !ASC->GetLifeComponent()->IsAlive() || Warping == nullptr || !IsValid(Montage)
		|| !Montage->HasRootMotion() || WarpTargetName.IsNone() || SourceAbility == nullptr
		|| SourceAbility->GetAvatarActorFromActorInfo() != Character
		|| Character->GetCharacterMovement()->MovementMode != MOVE_Walking)
	{
		return FGolemResult::Failure(EGolemError::InvalidState, TEXT("BeginTraversal"),
			NSLOCTEXT("GolemCombat", "TraversalState", "Traversal requires a living grounded Golem and root motion montage."));
	}
	FTransform Landing;
	const FGolemResult Probe = FindValidatedLanding(Landing);
	if (!Probe.IsSuccess())
	{
		return Probe;
	}
	Snapshot.Handle = FGuid::NewGuid();
	Snapshot.Landing = Landing;
	Snapshot.WarpTargetName = WarpTargetName;
	ActiveMontage = Montage;
	StartingController = Character->GetController();
	UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
	PreviousMovementMode = Movement->MovementMode;
	PreviousCustomMode = Movement->CustomMovementMode;
	bPreviousOrientToMovement = Movement->bOrientRotationToMovement;
	Movement->StopMovementImmediately();
	Character->StopJumping();
	Character->ConsumeMovementInputVector();
	Movement->bOrientRotationToMovement = false;
	Movement->SetMovementMode(MOVE_Flying);
	Warping->AddOrUpdateWarpTargetFromTransform(WarpTargetName, Landing);
	AppliedWarpTarget = WarpTargetName;
	ASC->AddLooseGameplayTag(GolemGameplayTags::State_Combat_Traversing, 1,
		EGameplayTagReplicationState::TagAndCountToAll);
	bOwnTraversingTag = true;
	if (UGolemCameraModeComponent* Camera = Character->FindComponentByClass<UGolemCameraModeComponent>())
	{
		FGolemCameraModeSettings Settings;
		Settings.FieldOfView = 75.0f;
		Settings.ArmLength = 360.0f;
		Settings.Priority = 20;
		Settings.bLockMovementInput = true;
		Settings.bLockLookInput = true;
		CameraHandle = Camera->PushMode(Settings);
	}
	StartedAt = GetWorld()->GetTimeSeconds();
	ActiveDuration = ASC->PlayMontage(SourceAbility, SourceAbility->GetCurrentActivationInfo(), Montage, 1.0f);
	if (ActiveDuration <= 0.0f)
	{
		const FGuid FailedHandle = Snapshot.Handle;
		EndTraversal(FailedHandle, true);
		return FGolemResult::Failure(EGolemError::InvalidState, TEXT("BeginTraversal"),
			NSLOCTEXT("GolemCombat", "TraversalMontage", "Traversal montage could not start on the Golem GAS animation channel."));
	}
	OutHandle = Snapshot.Handle;
	SetComponentTickEnabled(true);
	Character->ForceNetUpdate();
	return FGolemResult::Success();
}

void UGolemTraversalComponent::EndTraversal(const FGuid& Handle, bool bAborted)
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (Character == nullptr || !Character->HasAuthority() || !Handle.IsValid() || Snapshot.Handle != Handle)
	{
		return;
	}
	// Tick and teardown may pass Snapshot.Handle directly; callbacks still need the accepted identity after cleanup.
	const FGuid AcceptedHandle = Handle;
	const FVector Landing = Snapshot.Landing.GetLocation();
	bAborted = bAborted || FVector::Dist(Character->GetActorLocation(), Landing) > CompletionTolerance;
	Snapshot.Handle.Invalidate();
	SetComponentTickEnabled(false);
	UGolemAbilitySystemComponent* ASC = Cast<UGolemAbilitySystemComponent>(
		UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Character));
	if (ASC != nullptr && bOwnTraversingTag)
	{
		ASC->RemoveLooseGameplayTag(GolemGameplayTags::State_Combat_Traversing, 1,
			EGameplayTagReplicationState::TagAndCountToAll);
	}
	bOwnTraversingTag = false;
	if (ASC != nullptr && ASC->GetCurrentMontage() == ActiveMontage && bAborted)
	{
		ASC->CurrentMontageStop(0.1f);
	}
	if (UMotionWarpingComponent* Warping = Character->FindComponentByClass<UMotionWarpingComponent>())
	{
		Warping->RemoveWarpTarget(AppliedWarpTarget);
	}
	AppliedWarpTarget = NAME_None;
	UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
	Movement->StopMovementImmediately();
	Movement->bOrientRotationToMovement = bPreviousOrientToMovement;
	const bool bAlive = ASC != nullptr && ASC->GetLifeComponent() != nullptr && ASC->GetLifeComponent()->IsAlive();
	if (!bEndingPlay && !Character->IsActorBeingDestroyed() && GetWorld() != nullptr && !GetWorld()->bIsTearingDown)
	{
		Movement->SetMovementMode(bAlive ? static_cast<EMovementMode>(PreviousMovementMode) : MOVE_None,
			PreviousCustomMode);
		if (AAIController* AI = Cast<AAIController>(Character->GetController()); bAlive && AI != nullptr
			&& AI->GetBrainComponent() != nullptr)
		{
			AI->GetBrainComponent()->RestartLogic();
		}
	}
	if (UGolemCameraModeComponent* Camera = Character->FindComponentByClass<UGolemCameraModeComponent>();
		Camera != nullptr && CameraHandle.IsValid())
	{
		Camera->PopMode(CameraHandle);
	}
	CameraHandle.Invalidate();
	ActiveMontage = nullptr;
	StartingController.Reset();
	if (!bAborted)
	{
		if (UGolemCombatComponent* Combat = Character->FindComponentByClass<UGolemCombatComponent>())
		{
			Combat->PublishOutcome(EGolemCombatOutcome::Traversal, GolemGameplayTags::Ability_World_Traverse);
		}
	}
	Character->ForceNetUpdate();
	OnTraversalEnded.Broadcast(AcceptedHandle, bAborted);
}

bool UGolemTraversalComponent::HasActiveTraversal() const
{
	return Snapshot.Handle.IsValid();
}

void UGolemTraversalComponent::PrepareForEndPlay()
{
	bEndingPlay = true;
	EndTraversal(Snapshot.Handle, true);
}

float UGolemTraversalComponent::GetActiveDuration() const
{
	return ActiveDuration;
}

const FGolemTraversalSnapshot& UGolemTraversalComponent::GetSnapshot() const
{
	return Snapshot;
}

void UGolemTraversalComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UGolemTraversalComponent, Snapshot);
}

void UGolemTraversalComponent::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	PrepareForEndPlay();
	if (GetOwner() != nullptr)
	{
		if (UMotionWarpingComponent* Warping = GetOwner()->FindComponentByClass<UMotionWarpingComponent>())
		{
			Warping->RemoveWarpTarget(AppliedWarpTarget);
		}
	}
	OnTraversalEnded.Clear();
	Super::EndPlay(EndPlayReason);
}

void UGolemTraversalComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	UGolemAbilitySystemComponent* ASC = Character != nullptr
		? Cast<UGolemAbilitySystemComponent>(UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Character)) : nullptr;
	const UAnimInstance* Animation = Character != nullptr ? Character->GetMesh()->GetAnimInstance() : nullptr;
	if (!bEndingPlay && Character != nullptr && Character->HasAuthority() && HasActiveTraversal()
		&& (Character->GetController() != StartingController.Get() || ASC == nullptr
			|| ASC->GetLifeComponent() == nullptr || !ASC->GetLifeComponent()->IsAlive()
			|| (GetWorld()->GetTimeSeconds() - StartedAt < ActiveDuration - 0.15f
				&& (Animation == nullptr || !Animation->Montage_IsPlaying(ActiveMontage)))))
	{
		EndTraversal(Snapshot.Handle, true);
	}
	else if (!bEndingPlay && Character != nullptr && Character->HasAuthority() && HasActiveTraversal()
		&& GetWorld()->GetTimeSeconds() - StartedAt >= ActiveDuration)
	{
		EndTraversal(Snapshot.Handle, false);
	}
}

void UGolemTraversalComponent::OnRep_Snapshot()
{
	UMotionWarpingComponent* Warping = GetOwner() != nullptr
		? GetOwner()->FindComponentByClass<UMotionWarpingComponent>() : nullptr;
	if (Warping == nullptr)
	{
		return;
	}
	if (!AppliedWarpTarget.IsNone() && AppliedWarpTarget != Snapshot.WarpTargetName)
	{
		Warping->RemoveWarpTarget(AppliedWarpTarget);
	}
	if (Snapshot.Handle.IsValid())
	{
		Warping->AddOrUpdateWarpTargetFromTransform(Snapshot.WarpTargetName, Snapshot.Landing);
		AppliedWarpTarget = Snapshot.WarpTargetName;
	}
	else
	{
		Warping->RemoveWarpTarget(AppliedWarpTarget);
		AppliedWarpTarget = NAME_None;
	}
}

FGolemResult UGolemTraversalComponent::FindValidatedLanding(FTransform& OutLanding) const
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	UWorld* World = GetWorld();
	if (Character == nullptr || World == nullptr || World->bIsTearingDown)
	{
		return FGolemResult::Failure(EGolemError::InvalidState, TEXT("ProbeTraversal"),
			NSLOCTEXT("GolemCombat", "TraversalWorld", "Traversal world is unavailable."));
	}
	const UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
	const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	const float Radius = Capsule->GetScaledCapsuleRadius();
	const FVector Forward = Character->GetActorForwardVector().GetSafeNormal2D();
	const FVector Bottom = Character->GetActorLocation() - FVector(0.0f, 0.0f, HalfHeight);
	FCollisionQueryParams Query(SCENE_QUERY_STAT(GolemTraversal), false, Character);
	FHitResult Obstacle;
	const FVector ProbeStart = Bottom + FVector(0.0f, 0.0f, 35.0f);
	if (!World->LineTraceSingleByChannel(Obstacle, ProbeStart, ProbeStart + Forward * ProbeDistance,
		ECC_Visibility, Query) || Obstacle.Component == nullptr || FVector::DotProduct(Obstacle.ImpactNormal, Forward) > -0.2f)
	{
		return FGolemResult::Failure(EGolemError::NotFound, TEXT("ProbeTraversal"),
			NSLOCTEXT("GolemCombat", "TraversalObstacle", "Face a nearby low obstacle to traverse."));
	}
	const FBox Bounds = Obstacle.Component->Bounds.GetBox();
	const float Height = Bounds.Max.Z - Bottom.Z;
	if (Height < 30.0f || Height > MaxObstacleHeight)
	{
		return FGolemResult::Failure(EGolemError::OutOfRange, TEXT("ProbeTraversal"),
			NSLOCTEXT("GolemCombat", "TraversalHeight", "Obstacle height is outside the traversal range."));
	}
	const FVector BoundsCenter = Bounds.GetCenter();
	const FVector Extent = Bounds.GetExtent();
	const float ForwardExtent = FMath::Abs(Forward.X) * Extent.X + FMath::Abs(Forward.Y) * Extent.Y;
	const float DistanceToCenter = FVector::DotProduct(BoundsCenter - Character->GetActorLocation(), Forward);
	const float Travel = DistanceToCenter + ForwardExtent + Radius + 60.0f;
	if (Travel < Radius * 2.0f || Travel > MaxTravelDistance || ForwardExtent * 2.0f > 220.0f)
	{
		return FGolemResult::Failure(EGolemError::OutOfRange, TEXT("ProbeTraversal"),
			NSLOCTEXT("GolemCombat", "TraversalWidth", "Obstacle width exceeds the supported traversal distance."));
	}
	const FVector Candidate = Bottom + Forward * Travel;
	FHitResult Floor;
	if (!World->LineTraceSingleByChannel(Floor, Candidate + FVector(0.0f, 0.0f, MaxObstacleHeight + 100.0f),
		Candidate - FVector(0.0f, 0.0f, 80.0f), ECC_Visibility, Query) || Floor.ImpactNormal.Z < 0.65f)
	{
		return FGolemResult::Failure(EGolemError::InvalidState, TEXT("ProbeTraversal"),
			NSLOCTEXT("GolemCombat", "TraversalFloor", "Traversal landing has no walkable supporting floor."));
	}
	const FVector Landing = Floor.ImpactPoint + FVector(0.0f, 0.0f, HalfHeight + 3.0f);
	const FCollisionShape Shape = FCollisionShape::MakeCapsule(Radius, HalfHeight);
	const FVector Clearance = Bottom + FVector(0.0f, 0.0f, Height + HalfHeight + 5.0f);
	FHitResult Ceiling;
	if (World->OverlapBlockingTestByChannel(Landing, Character->GetActorQuat(), ECC_Pawn, Shape, Query)
		|| World->SweepSingleByChannel(Ceiling, Clearance, Clearance + Forward * Travel,
			Character->GetActorQuat(), ECC_Pawn, Shape, Query))
	{
		return FGolemResult::Failure(EGolemError::Conflict, TEXT("ProbeTraversal"),
			NSLOCTEXT("GolemCombat", "TraversalClearance", "Traversal path or landing is obstructed."));
	}
	OutLanding = FTransform(Forward.Rotation(), Landing);
	return FGolemResult::Success();
}
