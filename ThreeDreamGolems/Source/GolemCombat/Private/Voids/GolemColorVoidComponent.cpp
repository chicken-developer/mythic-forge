#include "Voids/GolemColorVoidComponent.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"

UGolemColorVoidComponent::UGolemColorVoidComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	SetIsReplicatedByDefault(true);
	InitSphereRadius(35.0f);
	// Weapon sweeps prefer a tagged zone over the capsule of the same actor.
	ComponentTags.Add(TEXT("GolemArmorZone"));
	SetGenerateOverlapEvents(false);
	SetCanEverAffectNavigation(false);
	// A void is a target for traces and projectiles only; it never pushes characters around.
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetCollisionObjectType(ECC_WorldDynamic);
	SetCollisionResponseToAllChannels(ECR_Ignore);
	SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
}

void UGolemColorVoidComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UGolemColorVoidComponent, ReplicatedState);
	DOREPLIFETIME(UGolemColorVoidComponent, ReplicatedRequiredMask);
	DOREPLIFETIME(UGolemColorVoidComponent, ReplicatedAppliedMask);
	DOREPLIFETIME(UGolemColorVoidComponent, ReplicatedTimerEnd);
}

void UGolemColorVoidComponent::TickComponent(const float DeltaTime, const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (GetOwner() != nullptr && GetOwner()->HasAuthority())
	{
		AdvanceTo(HostNow());
	}
}

double UGolemColorVoidComponent::HostNow() const
{
	const UWorld* World = GetWorld();
	return World != nullptr ? World->GetTimeSeconds() : 0.0;
}

FGolemResult UGolemColorVoidComponent::ConfigureVoid(const uint8 InRequiredMask, const FGolemVoidTuning& InTuning,
	const bool bStartHidden)
{
	if (GetOwner() == nullptr || !GetOwner()->HasAuthority())
	{
		return FGolemResult::Failure(EGolemError::NotAuthority, TEXT("ConfigureVoid"), FText::GetEmpty());
	}
	const FGolemResult TuningResult = InTuning.Validate();
	if (!TuningResult.IsSuccess())
	{
		return TuningResult;
	}
	FGolemVoidRuntime Candidate;
	if (!Candidate.Configure(InRequiredMask, bStartHidden))
	{
		return FGolemResult::Failure(EGolemError::InvalidData, TEXT("VoidRequiredElements"), FText::GetEmpty());
	}
	Tuning = InTuning;
	Runtime = Candidate;
	PublishState();
	return FGolemResult::Success();
}

bool UGolemColorVoidComponent::SetRevealed(const bool bRevealed)
{
	if (GetOwner() == nullptr || !GetOwner()->HasAuthority())
	{
		return false;
	}
	const bool bChanged = bRevealed ? Runtime.Reveal() : Runtime.Hide();
	if (bChanged)
	{
		PublishState();
	}
	return bChanged;
}

EGolemVoidHit UGolemColorVoidComponent::ApplyElement(const EGolemElement Element, const float ColorHoldSeconds,
	AActor* InstigatorActor)
{
	return ApplyElementAt(Element, ColorHoldSeconds, InstigatorActor, HostNow());
}

EGolemVoidHit UGolemColorVoidComponent::ApplyElementAt(const EGolemElement Element, const float ColorHoldSeconds,
	AActor* InstigatorActor, const double Now)
{
	if (GetOwner() == nullptr || !GetOwner()->HasAuthority())
	{
		return EGolemVoidHit::Ignored;
	}
	const EGolemVoidState Before = Runtime.State;
	const uint8 AppliedBefore = Runtime.AppliedMask;
	const EGolemVoidHit Hit = Runtime.Apply(Element, ColorHoldSeconds, Now, Tuning);
	if (Runtime.State != Before || Runtime.AppliedMask != AppliedBefore)
	{
		PublishState();
	}
	if (Hit == EGolemVoidHit::Filled)
	{
		OnVoidFilled.Broadcast(this, InstigatorActor);
	}
	return Hit;
}

void UGolemColorVoidComponent::AdvanceTo(const double Now)
{
	if (GetOwner() != nullptr && GetOwner()->HasAuthority() && Runtime.Update(Now))
	{
		PublishState();
	}
}

void UGolemColorVoidComponent::PublishState()
{
	ReplicatedState = Runtime.State;
	ReplicatedRequiredMask = Runtime.RequiredMask;
	ReplicatedAppliedMask = Runtime.AppliedMask;
	ReplicatedTimerEnd = Runtime.TimerEnd;
	// Only a void that can take a hit should stop a trace or a projectile.
	SetCollisionEnabled(Runtime.AcceptsHits() || Runtime.State == EGolemVoidState::Dulled
		? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
	if (AActor* Owner = GetOwner())
	{
		Owner->ForceNetUpdate();
	}
	OnVoidChanged.Broadcast(this);
}

void UGolemColorVoidComponent::OnRep_VoidState()
{
	Runtime.State = ReplicatedState;
	Runtime.RequiredMask = ReplicatedRequiredMask;
	Runtime.AppliedMask = ReplicatedAppliedMask;
	Runtime.TimerEnd = ReplicatedTimerEnd;
	OnVoidChanged.Broadcast(this);
}
