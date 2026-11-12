#include "Xenon/GolemXenonBrainComponent.h"

#include "AbilitySystemGlobals.h"
#include "Attributes/GolemAttributeSet.h"
#include "Components/GolemAbilitySystemComponent.h"
#include "Components/GolemCombatComponent.h"
#include "Components/GolemLifeComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "Tags/GolemGameplayTags.h"
#include "UObject/UObjectIterator.h"
#include "Voids/GolemVoidHostComponent.h"

namespace
{
	/** A Xenon starts an attack only when it faces its target this closely. */
	constexpr float GolemXenonAttackFacingDegrees = 25.0f;
}

UGolemXenonBrainComponent::UGolemXenonBrainComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	SetIsReplicatedByDefault(true);
}

void UGolemXenonBrainComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UGolemXenonBrainComponent, Action);
	DOREPLIFETIME(UGolemXenonBrainComponent, ActionEnd);
	DOREPLIFETIME(UGolemXenonBrainComponent, ZoneRange);
	DOREPLIFETIME(UGolemXenonBrainComponent, ZoneHalfAngle);
	DOREPLIFETIME(UGolemXenonBrainComponent, ZoneYaw);
}

FGolemResult UGolemXenonBrainComponent::InitializeBrain(UGolemAbilitySystemComponent* InAbilitySystem,
	UGolemVoidHostComponent* InHost, const FGolemXenonBehavior& InBehavior)
{
	if (GetOwner() == nullptr || !GetOwner()->HasAuthority())
	{
		return FGolemResult::Failure(EGolemError::NotAuthority, TEXT("InitializeXenonBrain"), FText::GetEmpty());
	}
	if (bInitialized || InAbilitySystem == nullptr || InAbilitySystem->GetOwner() != GetOwner()
		|| InHost == nullptr || InHost->GetOwner() != GetOwner())
	{
		return FGolemResult::Failure(EGolemError::InvalidState, TEXT("XenonBrainComposition"), FText::GetEmpty());
	}
	const FGolemResult Result = InBehavior.Validate();
	if (!Result.IsSuccess())
	{
		return Result;
	}
	AbilitySystem = InAbilitySystem;
	Host = InHost;
	Behavior = InBehavior;
	bInitialized = true;
	if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		Character->GetCharacterMovement()->MaxWalkSpeed = Behavior.MoveSpeed;
		// A Xenon has no controller; its brain feeds movement input directly.
		Character->GetCharacterMovement()->bRunPhysicsWithNoController = true;
		Character->GetCharacterMovement()->bOrientRotationToMovement = false;
		Character->bUseControllerRotationYaw = false;
	}
	return FGolemResult::Success();
}

void UGolemXenonBrainComponent::SetBrainEnabled(const bool bEnabled)
{
	bBrainEnabled = bEnabled;
	if (!bEnabled && GetOwner() != nullptr && GetOwner()->HasAuthority()
		&& Action != EGolemXenonAction::Restored && Action != EGolemXenonAction::Staggered)
	{
		SetAction(EGolemXenonAction::Idle, 0.0);
	}
}

void UGolemXenonBrainComponent::TickComponent(const float DeltaTime, const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	const UWorld* World = GetWorld();
	if (World != nullptr && GetOwner() != nullptr && GetOwner()->HasAuthority())
	{
		Step(DeltaTime, World->GetTimeSeconds());
	}
}

void UGolemXenonBrainComponent::SetAction(const EGolemXenonAction NewAction, const double NewEnd)
{
	if (Action == NewAction && ActionEnd == NewEnd)
	{
		return;
	}
	Action = NewAction;
	ActionEnd = NewEnd;
	if (AActor* Owner = GetOwner())
	{
		Owner->ForceNetUpdate();
	}
	OnActionChanged.Broadcast();
}

void UGolemXenonBrainComponent::OnRep_Action()
{
	OnActionChanged.Broadcast();
}

AActor* UGolemXenonBrainComponent::FindTarget() const
{
	const UWorld* World = GetWorld();
	AActor* Owner = GetOwner();
	AActor* Best = nullptr;
	double BestDistance = FMath::Square(static_cast<double>(Behavior.AggroRange));
	if (World == nullptr || Owner == nullptr)
	{
		return nullptr;
	}
	// Every living party Golem has an ability system with a life component; the nearest one is the target.
	for (TObjectIterator<UGolemAbilitySystemComponent> It; It; ++It)
	{
		const UGolemAbilitySystemComponent* Candidate = *It;
		AActor* Actor = IsValid(Candidate) ? Candidate->GetOwner() : nullptr;
		if (!IsValid(Actor) || Actor->GetWorld() != World || !UGolemCombatComponent::AreHostile(Owner, Actor))
		{
			continue;
		}
		const double Distance = FVector::DistSquared2D(Owner->GetActorLocation(), Actor->GetActorLocation());
		// Ties go to the lower path name so host runs repeat.
		if (Distance < BestDistance || (Distance == BestDistance && Best != nullptr && Actor->GetPathName() < Best->GetPathName()))
		{
			BestDistance = Distance;
			Best = Actor;
		}
	}
	return Best;
}

bool UGolemXenonBrainComponent::IsInsideZone(const FVector& Location) const
{
	const AActor* Owner = GetOwner();
	if (Owner == nullptr || ZoneRange <= 0.0f)
	{
		return false;
	}
	const FVector Offset = Location - Owner->GetActorLocation();
	if (Offset.SizeSquared2D() > FMath::Square(ZoneRange))
	{
		return false;
	}
	const FVector Forward = FRotator(0.0, ZoneYaw, 0.0).Vector();
	const FVector Direction = Offset.GetSafeNormal2D();
	// A Golem standing inside the body's own footprint is always in front enough to be hit.
	return Direction.IsNearlyZero()
		|| FVector::DotProduct(Forward, Direction) >= FMath::Cos(FMath::DegreesToRadians(ZoneHalfAngle));
}

bool UGolemXenonBrainComponent::IsThreatening(const FVector& Location) const
{
	return Action == EGolemXenonAction::Telegraph && IsInsideZone(Location);
}

AActor* UGolemXenonBrainComponent::FindThreat(const UWorld* World, const FVector& Location)
{
	if (World == nullptr)
	{
		return nullptr;
	}
	for (TObjectIterator<UGolemXenonBrainComponent> It; It; ++It)
	{
		const UGolemXenonBrainComponent* Brain = *It;
		if (IsValid(Brain) && Brain->GetWorld() == World && IsValid(Brain->GetOwner()) && Brain->IsThreatening(Location))
		{
			return Brain->GetOwner();
		}
	}
	return nullptr;
}

void UGolemXenonBrainComponent::Strike()
{
	AActor* Owner = GetOwner();
	const UWorld* World = GetWorld();
	if (Owner == nullptr || World == nullptr || !AbilitySystem.IsValid() || !Behavior.Moves.IsValidIndex(MoveIndex))
	{
		return;
	}
	const FGolemXenonMove Move = Behavior.Moves[MoveIndex];
	const float Damage = AbilitySystem->GetGolemAttributes() != nullptr
		? AbilitySystem->GetGolemAttributes()->GetPower() * Move.DamageScale : 0.0f;
	++StrikeCount;
	TArray<AActor*> Victims;
	for (TObjectIterator<UGolemAbilitySystemComponent> It; It; ++It)
	{
		AActor* Actor = IsValid(*It) ? It->GetOwner() : nullptr;
		if (IsValid(Actor) && Actor->GetWorld() == World && UGolemCombatComponent::AreHostile(Owner, Actor)
			&& IsInsideZone(Actor->GetActorLocation()))
		{
			Victims.Add(Actor);
		}
	}
	for (AActor* Victim : Victims)
	{
		if (!IsValid(Victim))
		{
			continue;
		}
		// A dodge inside its 0.35 s window, or a Warrior's block from the front, answers this hit.
		if (Damage > 0.0f)
		{
			UGolemAbilitySystemComponent::ApplyCombatDamage(Owner, Victim, Damage, 0.0f, true);
		}
		ACharacter* Character = Cast<ACharacter>(Victim);
		if (Move.Knockback > 0.0f && IsValid(Character))
		{
			FVector Away = (Character->GetActorLocation() - Owner->GetActorLocation()).GetSafeNormal2D();
			if (Away.IsNearlyZero())
			{
				Away = FRotator(0.0, ZoneYaw, 0.0).Vector();
			}
			Character->LaunchCharacter(Away * Move.Knockback + FVector(0.0, 0.0, Move.Knockback * 0.25f), true, true);
		}
	}
}

void UGolemXenonBrainComponent::Step(const float DeltaSeconds, const double Now)
{
	AActor* Owner = GetOwner();
	if (!bInitialized || Owner == nullptr || !Owner->HasAuthority() || !Host.IsValid())
	{
		return;
	}
	ACharacter* Character = Cast<ACharacter>(Owner);
	if (Host->IsRestored())
	{
		Target.Reset();
		SetAction(EGolemXenonAction::Restored, 0.0);
		return;
	}
	if (Host->IsStaggered())
	{
		// A fill interrupts whatever the Xenon was doing, including an attack it had already shown.
		Target.Reset();
		SetAction(EGolemXenonAction::Staggered, Host->GetStaggerEndServerTime());
		return;
	}
	if (!bBrainEnabled)
	{
		SetAction(EGolemXenonAction::Idle, 0.0);
		return;
	}
	if (Action == EGolemXenonAction::Telegraph)
	{
		if (Now >= ActionEnd)
		{
			Strike();
			const float Recover = Behavior.Moves.IsValidIndex(MoveIndex) ? Behavior.Moves[MoveIndex].RecoverSeconds : 0.0f;
			SetAction(EGolemXenonAction::Recover, Now + Recover);
		}
		return;
	}
	if (Action == EGolemXenonAction::Recover && Now < ActionEnd)
	{
		return;
	}
	Target = FindTarget();
	if (!Target.IsValid())
	{
		SetAction(EGolemXenonAction::Idle, 0.0);
		return;
	}
	SetAction(EGolemXenonAction::Chase, 0.0);
	const FVector ToTarget = Target->GetActorLocation() - Owner->GetActorLocation();
	const float Distance = static_cast<float>(ToTarget.Size2D());
	const float WantedYaw = ToTarget.GetSafeNormal2D().IsNearlyZero() ? Owner->GetActorRotation().Yaw
		: static_cast<float>(ToTarget.Rotation().Yaw);
	const float CurrentYaw = static_cast<float>(Owner->GetActorRotation().Yaw);
	const float NewYaw = FMath::FixedTurn(CurrentYaw, WantedYaw, Behavior.TurnRateDegrees * FMath::Max(DeltaSeconds, 0.0f));
	Owner->SetActorRotation(FRotator(0.0, NewYaw, 0.0));
	const float FacingError = FMath::Abs(FMath::FindDeltaAngleDegrees(NewYaw, WantedYaw));
	for (int32 Index = 0; Index < Behavior.Moves.Num(); ++Index)
	{
		const FGolemXenonMove& Move = Behavior.Moves[Index];
		if (Distance <= Move.StartRange && FacingError <= GolemXenonAttackFacingDegrees)
		{
			// The zone is fixed now: a Golem that steps out of it during the warning is not hit.
			MoveIndex = Index;
			ZoneRange = Move.Range;
			ZoneHalfAngle = Move.HalfAngleDegrees;
			ZoneYaw = NewYaw;
			SetAction(EGolemXenonAction::Telegraph, Now + Move.TelegraphSeconds);
			return;
		}
	}
	const float StopDistance = Behavior.Moves.IsEmpty() ? 200.0f : Behavior.Moves[0].StartRange * 0.8f;
	if (IsValid(Character) && Distance > StopDistance)
	{
		Character->AddMovementInput(FRotator(0.0, NewYaw, 0.0).Vector(), 1.0f);
	}
}
