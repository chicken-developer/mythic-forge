#include "Components/GolemCombatComponent.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/GolemAbilitySystemComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Skills/GolemSkillRuntimeComponent.h"
#include "Tags/GolemGameplayTags.h"

DEFINE_LOG_CATEGORY_STATIC(LogGolemTraceGeometry, Log, All);

namespace
{
	constexpr int32 TraceGeometryLogLimit = 256;
	constexpr float TraceGeometryTargetRange = 400.0f;
}

void UGolemCombatComponent::BeginTraceGeometryDiagnostics()
{
	EndTraceGeometryDiagnostics(TEXT("WindowReplaced"));
	TraceGeometryDiagnostics = FTraceGeometryDiagnostics();
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	const UGolemSkillRuntimeComponent* Skills = IsValid(Character)
		? Character->FindComponentByClass<UGolemSkillRuntimeComponent>() : nullptr;
	if (TraceGeometryLogCount >= TraceGeometryLogLimit ||
		!FParse::Param(FCommandLine::Get(), TEXT("M56TraceGeometry")) ||
		!IsValid(Character) || !Character->HasAuthority() || !Character->IsPlayerControlled() ||
		Skills == nullptr || !Skills->GetSkillState().EquipmentCombatBuild.bEnabled ||
		AttackAction != GolemGameplayTags::Ability_Combat_Heavy || GetWorld() == nullptr)
	{
		return;
	}
	float ClosestDistance = FMath::Square(TraceGeometryTargetRange);
	for (TActorIterator<AActor> Actor(GetWorld()); Actor; ++Actor)
	{
		const float Distance = FVector::DistSquared(Character->GetActorLocation(), Actor->GetActorLocation());
		if (Distance > ClosestDistance || *Actor == Character)
		{
			continue;
		}
		TInlineComponentArray<UPrimitiveComponent*> Components(*Actor);
		for (UPrimitiveComponent* Component : Components)
		{
			if (IsValid(Component) && Component->IsRegistered() &&
				Component->ComponentHasTag(TEXT("GolemArmorZone")))
			{
				ClosestDistance = Distance;
				TraceGeometryDiagnostics.ArmorZone = Component;
			}
		}
	}
	const UPrimitiveComponent* ArmorZone = TraceGeometryDiagnostics.ArmorZone.Get();
	if (ArmorZone == nullptr)
	{
		return;
	}
	TraceGeometryDiagnostics.bActive = true;
	const FBox Bounds = ArmorZone->Bounds.GetBox();
	const FVector Direction = (ArmorZone->GetOwner()->GetActorLocation() - Character->GetActorLocation())
		.GetSafeNormal2D();
	++TraceGeometryLogCount;
	UE_LOG(LogGolemTraceGeometry, Log,
		TEXT("M56TraceGeometry Begin actor=%s target=%s activation=%s hit=%d window=%s time=%.3f ")
		TEXT("actorPos=%s targetPos=%s forward=%s controlYaw=%.2f facingDot=%.3f distance=%.2f ")
		TEXT("armor=%s boundsMin=%s boundsMax=%s collision=%d objectType=%d"),
		*GetNameSafe(Character), *GetNameSafe(ArmorZone->GetOwner()), *AttackActivationId.ToString(),
		WindowHitIndex, *ActiveWindow.ToString(), GetWorld()->GetTimeSeconds(),
		*Character->GetActorLocation().ToString(), *ArmorZone->GetOwner()->GetActorLocation().ToString(),
		*Character->GetActorForwardVector().ToString(), Character->GetControlRotation().Yaw,
		FVector::DotProduct(Character->GetActorForwardVector(), Direction), FMath::Sqrt(ClosestDistance),
		*GetNameSafe(ArmorZone), *Bounds.Min.ToString(), *Bounds.Max.ToString(),
		static_cast<int32>(ArmorZone->GetCollisionEnabled()), static_cast<int32>(ArmorZone->GetCollisionObjectType()));
}

void UGolemCombatComponent::RecordTraceGeometrySweep(const FVector& Hand, const FVector& Head,
	const FVector& PreviousHead, const TArray<FHitResult>& Hits, const TArray<FHitResult>& MovingHits)
{
	FTraceGeometryDiagnostics& Diagnostics = TraceGeometryDiagnostics;
	const UPrimitiveComponent* ArmorZone = Diagnostics.ArmorZone.Get();
	if (!Diagnostics.bActive || ArmorZone == nullptr)
	{
		return;
	}
	++Diagnostics.Sweeps;
	if (Diagnostics.Sweeps == 1)
	{
		Diagnostics.HandMin = Diagnostics.HandMax = Hand;
		Diagnostics.HeadMin = Diagnostics.HeadMax = Head;
	}
	else
	{
		Diagnostics.HandMin = Diagnostics.HandMin.ComponentMin(Hand);
		Diagnostics.HandMax = Diagnostics.HandMax.ComponentMax(Hand);
		Diagnostics.HeadMin = Diagnostics.HeadMin.ComponentMin(Head);
		Diagnostics.HeadMax = Diagnostics.HeadMax.ComponentMax(Head);
	}
	const FVector Center = ArmorZone->Bounds.Origin;
	const float ShaftDistance = FVector::Distance(Center, FMath::ClosestPointOnSegment(Center, Hand, Head));
	const float MovingDistance = FVector::Distance(Center,
		FMath::ClosestPointOnSegment(Center, PreviousHead, Head));
	Diagnostics.MinSegmentCenterDistance = FMath::Min(Diagnostics.MinSegmentCenterDistance,
		FMath::Min(ShaftDistance, MovingDistance));
	const FGolemAttackDefinition* Attack = GetCurrentAttack();
	if (Diagnostics.Sweeps == 1 && TraceGeometryLogCount < TraceGeometryLogLimit)
	{
		const ACharacter* Character = Cast<ACharacter>(GetOwner());
		const UAnimInstance* Animation = Character != nullptr && Character->GetMesh() != nullptr
			? Character->GetMesh()->GetAnimInstance() : nullptr;
		const UAnimMontage* Montage = AbilitySystem.IsValid() ? AbilitySystem->GetCurrentMontage() : nullptr;
		++TraceGeometryLogCount;
		UE_LOG(LogGolemTraceGeometry, Log,
			TEXT("M56TraceGeometry FirstSweep activation=%s hit=%d hand=%s head=%s previousHead=%s ")
			TEXT("radius=%.2f shaftHits=%d movingHits=%d armorCenterDistance=%.2f montage=%s montagePosition=%.3f ")
			TEXT("frame=%llu appDelta=%.6f worldDelta=%.6f"),
			*AttackActivationId.ToString(), WindowHitIndex, *Hand.ToString(), *Head.ToString(),
			*PreviousHead.ToString(), Attack != nullptr ? Attack->TraceRadius : 0.0f,
			Hits.Num(), MovingHits.Num(), Diagnostics.MinSegmentCenterDistance, *GetNameSafe(Montage),
			Animation != nullptr && Montage != nullptr ? Animation->Montage_GetPosition(Montage) : -1.0f,
			GFrameCounter, FApp::GetDeltaTime(), GetWorld() != nullptr ? GetWorld()->GetDeltaSeconds() : 0.0f);
	}
	const auto RecordCandidates = [this, ArmorZone](const TArray<FHitResult>& Candidates, const TCHAR* Sweep)
	{
		for (const FHitResult& Hit : Candidates)
		{
			if (Hit.GetActor() != ArmorZone->GetOwner())
			{
				continue;
			}
			FTraceGeometryDiagnostics& CurrentDiagnostics = TraceGeometryDiagnostics;
			const bool bArmor = Hit.GetComponent() == ArmorZone;
			if (bArmor)
			{
				++CurrentDiagnostics.ArmorCandidates;
			}
			else
			{
				++CurrentDiagnostics.OtherCandidates;
			}
			const bool bFirst = !CurrentDiagnostics.bLoggedContact;
			const bool bFirstArmor = bArmor && !CurrentDiagnostics.bLoggedZone;
			if (bFirst)
			{
				CurrentDiagnostics.FirstContact = Hit.GetComponent() != nullptr
					? Hit.GetComponent()->GetFName() : NAME_None;
				CurrentDiagnostics.bLoggedContact = true;
			}
			CurrentDiagnostics.bLoggedZone |= bArmor;
			if ((bFirst || bFirstArmor) && TraceGeometryLogCount < TraceGeometryLogLimit)
			{
				const FBox ComponentBounds = Hit.GetComponent() != nullptr
					? Hit.GetComponent()->Bounds.GetBox() : FBox(ForceInit);
				++TraceGeometryLogCount;
				UE_LOG(LogGolemTraceGeometry, Log,
					TEXT("M56TraceGeometry Contact activation=%s hit=%d sweepNumber=%d sweep=%s ")
					TEXT("component=%s armor=%d actorAlreadyHit=%d impact=%s traceStart=%s traceEnd=%s ")
					TEXT("fraction=%.4f initialOverlap=%d componentBoundsMin=%s componentBoundsMax=%s"),
					*AttackActivationId.ToString(), WindowHitIndex, CurrentDiagnostics.Sweeps, Sweep,
					*GetNameSafe(Hit.GetComponent()), bArmor, WindowHitActors.Contains(Hit.GetActor()),
					*Hit.ImpactPoint.ToString(), *Hit.TraceStart.ToString(), *Hit.TraceEnd.ToString(),
					Hit.Time, Hit.bStartPenetrating, *ComponentBounds.Min.ToString(), *ComponentBounds.Max.ToString());
			}
		}
	};
	RecordCandidates(Hits, TEXT("HandToHead"));
	RecordCandidates(MovingHits, TEXT("PreviousHeadToHead"));
}

void UGolemCombatComponent::RecordTraceGeometryDecision(const FHitResult& Hit, const FName Decision)
{
	FTraceGeometryDiagnostics& Diagnostics = TraceGeometryDiagnostics;
	const UPrimitiveComponent* ArmorZone = Diagnostics.ArmorZone.Get();
	if (!Diagnostics.bActive || ArmorZone == nullptr || Hit.GetActor() != ArmorZone->GetOwner())
	{
		return;
	}
	const bool bAccepted = Decision == TEXT("Accepted") && !Diagnostics.bLoggedAccepted;
	const bool bDeferred = Decision == TEXT("DeferredBody");
	const bool bSuppressedArmor = Decision == TEXT("ActorAlreadyHit") && Hit.GetComponent() == ArmorZone &&
		!Diagnostics.bLoggedZoneDedup;
	if (!bAccepted && !bSuppressedArmor && !bDeferred)
	{
		return;
	}
	if (bAccepted)
	{
		Diagnostics.bLoggedAccepted = true;
		Diagnostics.AcceptedComponent = Hit.GetComponent() != nullptr ? Hit.GetComponent()->GetFName() : NAME_None;
	}
	Diagnostics.bLoggedZoneDedup |= bSuppressedArmor;
	if (TraceGeometryLogCount < TraceGeometryLogLimit)
	{
		++TraceGeometryLogCount;
		UE_LOG(LogGolemTraceGeometry, Log,
			TEXT("M56TraceGeometry Decision activation=%s hit=%d sweepNumber=%d decision=%s ")
			TEXT("component=%s armor=%d firstAccepted=%s"),
			*AttackActivationId.ToString(), WindowHitIndex, Diagnostics.Sweeps, *Decision.ToString(),
			*GetNameSafe(Hit.GetComponent()), Hit.GetComponent() == ArmorZone,
			*Diagnostics.AcceptedComponent.ToString());
	}
}

void UGolemCombatComponent::EndTraceGeometryDiagnostics(const FName Reason)
{
	FTraceGeometryDiagnostics& Diagnostics = TraceGeometryDiagnostics;
	if (!Diagnostics.bActive)
	{
		return;
	}
	Diagnostics.bActive = false;
	if (TraceGeometryLogCount >= TraceGeometryLogLimit)
	{
		return;
	}
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	const UAnimInstance* Animation = Character != nullptr && Character->GetMesh() != nullptr
		? Character->GetMesh()->GetAnimInstance() : nullptr;
	const UAnimMontage* Montage = AbilitySystem.IsValid() ? AbilitySystem->GetCurrentMontage() : nullptr;
	++TraceGeometryLogCount;
	UE_LOG(LogGolemTraceGeometry, Log,
		TEXT("M56TraceGeometry End activation=%s hit=%d reason=%s sweeps=%d armorCandidates=%d ")
		TEXT("otherCandidates=%d firstContact=%s accepted=%s zoneDedup=%d handMin=%s handMax=%s ")
		TEXT("headMin=%s headMax=%s minSegmentCenterDistance=%.2f endTime=%.3f endMontagePosition=%.3f ")
		TEXT("frame=%llu appDelta=%.6f worldDelta=%.6f"),
		*AttackActivationId.ToString(), WindowHitIndex, *Reason.ToString(), Diagnostics.Sweeps,
		Diagnostics.ArmorCandidates, Diagnostics.OtherCandidates, *Diagnostics.FirstContact.ToString(),
		*Diagnostics.AcceptedComponent.ToString(), Diagnostics.bLoggedZoneDedup,
		*Diagnostics.HandMin.ToString(), *Diagnostics.HandMax.ToString(),
		*Diagnostics.HeadMin.ToString(), *Diagnostics.HeadMax.ToString(), Diagnostics.MinSegmentCenterDistance,
		GetWorld() != nullptr ? GetWorld()->GetTimeSeconds() : 0.0,
		Animation != nullptr && Montage != nullptr ? Animation->Montage_GetPosition(Montage) : -1.0f,
		GFrameCounter, FApp::GetDeltaTime(), GetWorld() != nullptr ? GetWorld()->GetDeltaSeconds() : 0.0f);
}
