#include "ColorAssist/GolemColorAssistComponent.h"

#include "AIController.h"
#include "AbilitySystemGlobals.h"
#include "Components/GolemAbilitySystemComponent.h"
#include "Components/GolemCombatComponent.h"
#include "Components/GolemLifeComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Projectiles/GolemColorProjectile.h"
#include "Tags/GolemGameplayTags.h"
#include "Voids/GolemColorVoidComponent.h"

namespace
{
	/** A swing that never reports its end is abandoned after this long. */
	constexpr double GolemAssistSwingTimeout = 4.0;
	constexpr double GolemAssistRepathSeconds = 0.4;
}

UGolemColorAssistComponent::UGolemColorAssistComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	PrimaryComponentTick.TickInterval = 0.1f;
	Planner.SetSeed(static_cast<int32>(GetUniqueID()));
}

void UGolemColorAssistComponent::Configure(const FGolemColorAssistTuning& InTuning)
{
	if (InTuning.Validate().IsSuccess())
	{
		Tuning = InTuning;
	}
}

void UGolemColorAssistComponent::SetSeed(const int32 Seed)
{
	Planner.SetSeed(Seed);
}

void UGolemColorAssistComponent::SetBlocked(const bool bInBlocked)
{
	bBlocked = bInBlocked;
	if (bBlocked && !bSwinging)
	{
		EndJob();
	}
}

void UGolemColorAssistComponent::TickComponent(const float DeltaTime, const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	const UWorld* World = GetWorld();
	if (World != nullptr && GetOwner() != nullptr && GetOwner()->HasAuthority())
	{
		Step(World->GetTimeSeconds());
	}
}

void UGolemColorAssistComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ResetAssist();
	OnJobChanged.Clear();
	Super::EndPlay(EndPlayReason);
}

void UGolemColorAssistComponent::ResetAssist()
{
	EndJob();
	Planner.Reset();
}

void UGolemColorAssistComponent::Step(const double Now)
{
	const AAIController* Controller = Cast<AAIController>(GetOwner());
	APawn* Pawn = Controller != nullptr ? Controller->GetPawn() : nullptr;
	UGolemAbilitySystemComponent* ASC = IsValid(Pawn) ? Cast<UGolemAbilitySystemComponent>(
		UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Pawn)) : nullptr;
	UGolemCombatComponent* Combat = IsValid(Pawn) ? Pawn->FindComponentByClass<UGolemCombatComponent>() : nullptr;
	FGameplayTagContainer OwnedTags;
	if (ASC != nullptr)
	{
		ASC->GetOwnedGameplayTags(OwnedTags);
	}
	EGolemElement Element = EGolemElement::Earth;
	if (Controller == nullptr || !Controller->HasAuthority() || ASC == nullptr || Combat == nullptr
		|| Combat->GetProfile() == nullptr || ASC->GetLifeComponent() == nullptr || !ASC->GetLifeComponent()->IsAlive()
		|| !GolemElements::FromOwnedTags(OwnedTags, Element))
	{
		ResetAssist();
		return;
	}
	const uint8 OwnMask = GolemElements::Mask(Element);
	const TArray<UGolemColorVoidComponent*> Voids = UGolemColorVoidComponent::GetHittableVoids(GetWorld());
	TArray<FGolemColorAssistVoid> Views;
	Views.Reserve(Voids.Num());
	for (const UGolemColorVoidComponent* Void : Voids)
	{
		// Only voids on something this Golem is fighting.
		if (!UGolemCombatComponent::AreHostile(Pawn, Void->GetOwner()))
		{
			continue;
		}
		FGolemColorAssistVoid& View = Views.AddDefaulted_GetRef();
		View.Id = Void->GetUniqueID();
		View.bAcceptsHits = Void->AcceptsHits();
		View.bPrimed = Void->GetVoidState() == EGolemVoidState::Primed;
		View.MissingMask = Void->GetMissingMask();
		View.SignalStart = Void->GetSignalStartServerTime();
		View.SignalEnd = Void->GetSignalEndServerTime();
		View.Distance = static_cast<float>(FVector::Dist(Pawn->GetActorLocation(), Void->GetComponentLocation()));
	}
	const FGolemColorAssistChoice Choice = Planner.Choose(Element, Views, Now, Tuning);

	if (JobVoid.IsValid() && !bSwinging)
	{
		const UGolemColorVoidComponent* Void = JobVoid.Get();
		const bool bStillWanted = Void->AcceptsHits() && (Void->GetMissingMask() & OwnMask) != 0
			&& (JobReason != EGolemColorAssistReason::FollowUp || Void->GetVoidState() == EGolemVoidState::Primed);
		if (!bStillWanted || bBlocked || Now - JobStartedAt > Tuning.GiveUpSeconds)
		{
			EndJob();
		}
	}
	else if (!JobVoid.IsValid() && (bSwinging || JobReason != EGolemColorAssistReason::None))
	{
		// The void went away under the job (its Xenon was removed).
		EndJob();
	}
	if (!JobVoid.IsValid() && Choice.IsValid() && !bBlocked)
	{
		for (UGolemColorVoidComponent* Void : Voids)
		{
			if (Void->GetUniqueID() == Choice.Id)
			{
				BeginJob(Void, Choice.Reason, Now);
				break;
			}
		}
	}
	if (JobVoid.IsValid())
	{
		DriveJob(Now);
	}
}

void UGolemColorAssistComponent::BeginJob(UGolemColorVoidComponent* Void, const EGolemColorAssistReason Reason,
	const double Now)
{
	JobVoid = Void;
	JobReason = Reason;
	JobStartedAt = Now;
	NextMoveAt = 0.0;
	bSwinging = false;
	bSawAttack = false;
	OnJobChanged.Broadcast(true);
}

void UGolemColorAssistComponent::EndJob()
{
	const bool bHadJob = JobReason != EGolemColorAssistReason::None;
	JobVoid.Reset();
	JobReason = EGolemColorAssistReason::None;
	bSwinging = false;
	bSawAttack = false;
	if (!bHadJob)
	{
		return;
	}
	const AAIController* Controller = Cast<AAIController>(GetOwner());
	const APawn* Pawn = Controller != nullptr ? Controller->GetPawn() : nullptr;
	if (UGolemCombatComponent* Combat = IsValid(Pawn) ? Pawn->FindComponentByClass<UGolemCombatComponent>() : nullptr)
	{
		// A shot already in the air keeps its own copy of the target; nothing else may be coloured from here on.
		Combat->SetColorIntent(nullptr);
	}
	OnJobChanged.Broadcast(false);
}

void UGolemColorAssistComponent::DriveJob(const double Now)
{
	AAIController* Controller = Cast<AAIController>(GetOwner());
	APawn* Pawn = Controller != nullptr ? Controller->GetPawn() : nullptr;
	UGolemColorVoidComponent* Void = JobVoid.Get();
	UGolemCombatComponent* Combat = IsValid(Pawn) ? Pawn->FindComponentByClass<UGolemCombatComponent>() : nullptr;
	UGolemAbilitySystemComponent* ASC = IsValid(Pawn) ? Cast<UGolemAbilitySystemComponent>(
		UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Pawn)) : nullptr;
	if (Void == nullptr || Combat == nullptr || ASC == nullptr || !IsValid(Void->GetOwner()))
	{
		EndJob();
		return;
	}
	if (bSwinging)
	{
		const bool bAttacking = Combat->GetSnapshot().bAttacking;
		bSawAttack |= bAttacking;
		if ((bSawAttack && !bAttacking) || Now - SwingStartedAt > GolemAssistSwingTimeout)
		{
			EndJob();
		}
		return;
	}
	const FVector PawnLocation = Pawn->GetActorLocation();
	const FVector VoidLocation = Void->GetComponentLocation();
	// "Outward" points from the body through the void: the side the void can be reached from.
	FVector Outward = (VoidLocation - Void->GetOwner()->GetActorLocation()).GetSafeNormal2D();
	if (Outward.IsNearlyZero())
	{
		Outward = (PawnLocation - VoidLocation).GetSafeNormal2D();
	}
	const FGolemHeroMoveTuning& Moves = Combat->GetHeroMoves();
	const float VoidRadius = Void->GetScaledSphereRadius();
	bool bReady = false;
	FVector Stand = PawnLocation;
	if (Moves.bHeavyIsProjectile)
	{
		// Stay where it is when the shot is already clear; otherwise go and face the void.
		const FVector Muzzle = PawnLocation + FVector(0.0, 0.0, 35.0);
		bReady = FVector::Dist(Muzzle, VoidLocation) <= Moves.ProjectileRange * 0.9f
			&& AGolemColorProjectile::FindFirstStop(GetWorld(), Pawn, Muzzle, VoidLocation, Moves.ProjectileRadius) == Void;
		Stand = VoidLocation + Outward * (VoidRadius + Tuning.RangedStandDistance);
	}
	else
	{
		Stand = VoidLocation + Outward * (VoidRadius + Tuning.MeleeStandDistance);
		bReady = FVector::Dist2D(PawnLocation, Stand) <= 45.0;
	}
	Stand.Z = PawnLocation.Z;
	if (!bReady)
	{
		if (Now >= NextMoveAt)
		{
			NextMoveAt = Now + GolemAssistRepathSeconds;
			Controller->MoveToLocation(Stand, 20.0f, false, true, true, true, nullptr, true);
		}
		return;
	}
	Controller->StopMovement();
	Combat->SetColorIntent(Void, JobReason == EGolemColorAssistReason::FollowUp);
	if (ASC->RequestAbilityInput(GolemGameplayTags::Input_Combat_Heavy, true).IsSuccess())
	{
		ASC->RequestAbilityInput(GolemGameplayTags::Input_Combat_Heavy, false);
		bSwinging = true;
		bSawAttack = Combat->GetSnapshot().bAttacking;
		SwingStartedAt = Now;
	}
}
