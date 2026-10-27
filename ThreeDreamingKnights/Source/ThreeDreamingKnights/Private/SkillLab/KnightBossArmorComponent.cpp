#include "SkillLab/KnightBossArmorComponent.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Events/KnightEventSubsystem.h"
#include "Events/KnightSkillLabEvent.h"
#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"
#include "PartyArena/KnightArenaEnemy.h"
#include "TimerManager.h"

UKnightBossArmorComponent::UKnightBossArmorComponent()
{
	SetIsReplicatedByDefault(true);
	PrimaryComponentTick.bCanEverTick = false;
}

void UKnightBossArmorComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ThisClass, ArmorState);
	DOREPLIFETIME(ThisClass, Durability);
	DOREPLIFETIME(ThisClass, EndServerTime);
	DOREPLIFETIME(ThisClass, bEnabled);
}

void UKnightBossArmorComponent::EnableArmorZone(const FGuid InEncounterId)
{
	if (!GetOwner()->HasAuthority())
	{
		return;
	}
	bEnabled = true;
	ResetArmor(InEncounterId);
	OnRep_Armor();
}

void UKnightBossArmorComponent::OnRep_Armor()
{
	if (bEnabled && ArmorZone == nullptr)
	{
		ArmorZone = NewObject<UStaticMeshComponent>(GetOwner(), TEXT("SkillLabArmorZone"));
		ArmorZone->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
		ArmorZone->SetupAttachment(GetOwner()->GetRootComponent());
		ArmorZone->SetRelativeLocation(FVector(0.0, 0.0, -35.0));
		ArmorZone->SetRelativeScale3D(FVector(1.25, 1.25, 0.65));
		ArmorZone->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		ArmorZone->SetCollisionObjectType(ECC_WorldDynamic);
		ArmorZone->SetCollisionResponseToAllChannels(ECR_Ignore);
		ArmorZone->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
		ArmorZone->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
		ArmorZone->SetCanEverAffectNavigation(false);
		ArmorZone->ComponentTags.Add(TEXT("KnightArmorZone"));
		GetOwner()->AddInstanceComponent(ArmorZone);
		ArmorZone->RegisterComponent();
	}
	if (ArmorZone != nullptr)
	{
		ArmorZone->SetVisibility(bEnabled);
		ArmorZone->SetRenderCustomDepth(ArmorState != EKnightBossArmorState::Intact);
		ArmorZone->SetCustomDepthStencilValue(ArmorState == EKnightBossArmorState::Brittle ? 2 : 3);
	}
	OnArmorChanged.Broadcast();
}

void UKnightBossArmorComponent::ResetArmor(const FGuid InEncounterId)
{
	if (!GetOwner()->HasAuthority())
	{
		return;
	}
	GetWorld()->GetTimerManager().ClearTimer(ExpiryTimer);
	EncounterId = InEncounterId;
	ArmorState = EKnightBossArmorState::Intact;
	Durability = 100.0f;
	EndServerTime = 0.0f;
	AppliedHits.Reset();
	PublishArmorChange();
}

bool UKnightBossArmorComponent::MakeBrittle()
{
	if (!GetOwner()->HasAuthority() || !bEnabled || ArmorState == EKnightBossArmorState::Broken)
	{
		return false;
	}
	ArmorState = EKnightBossArmorState::Brittle;
	EndServerTime = GetWorld()->GetTimeSeconds() + 12.0f;
	GetWorld()->GetTimerManager().SetTimer(ExpiryTimer, this, &ThisClass::ExpireArmorState, 12.0f, false);
	PublishArmorChange();
	return true;
}

void UKnightBossArmorComponent::ResolveArmorHit(const FHitResult& Hit, const FGuid& ActivationId,
	const int32 HitIndex, const bool bHeavy, const float RawDamage, float& OutDamage)
{
	OutDamage = RawDamage;
	if (!GetOwner()->HasAuthority() || !bEnabled || Hit.GetComponent() != ArmorZone
		|| !ActivationId.IsValid() || HitIndex < 0 || !FMath::IsFinite(RawDamage) || RawDamage <= 0.0f)
	{
		return;
	}
	const FString Key = ActivationId.ToString() + TEXT(":") + FString::FromInt(HitIndex);
	if (AppliedHits.Contains(Key))
	{
		OutDamage = 0.0f;
		return;
	}
	AppliedHits.Add(Key);
	// The breaking hit uses the pre-hit state; the following hit benefits from Broken.
	OutDamage *= ArmorState == EKnightBossArmorState::Intact ? 0.2f
		: (ArmorState == EKnightBossArmorState::Brittle ? 0.5f : 1.2f);
	if (ArmorState != EKnightBossArmorState::Broken)
	{
		const float DurabilityDamage = (bHeavy ? 20.0f : 10.0f)
			* (ArmorState == EKnightBossArmorState::Brittle ? 2.0f : 1.0f);
		Durability = FMath::Max(0.0f, Durability - DurabilityDamage);
		if (Durability == 0.0f)
		{
			ArmorState = EKnightBossArmorState::Broken;
			EndServerTime = GetWorld()->GetTimeSeconds() + 10.0f;
			GetWorld()->GetTimerManager().SetTimer(ExpiryTimer, this, &ThisClass::ExpireArmorState, 10.0f, false);
		}
		PublishArmorChange();
	}
}

void UKnightBossArmorComponent::ExpireArmorState()
{
	if (ArmorState == EKnightBossArmorState::Broken)
	{
		Durability = 100.0f;
	}
	ArmorState = EKnightBossArmorState::Intact;
	EndServerTime = 0.0f;
	PublishArmorChange();
}

void UKnightBossArmorComponent::PublishArmorChange()
{
	++Revision;
	OnRep_Armor();
	GetOwner()->ForceNetUpdate();
	if (UKnightEventSubsystem* Events = GetWorld()->GetSubsystem<UKnightEventSubsystem>())
	{
		FKnightSkillLabEvent Event;
		Event.EventName = TEXT("ArmorStateChanged");
		Event.ConfirmationId = FGuid::NewGuid();
		Event.RequestId = Event.ConfirmationId;
		Event.EncounterId = EncounterId;
		Event.Revision = Revision;
		const AKnightArenaEnemy* Enemy = Cast<AKnightArenaEnemy>(GetOwner());
		Event.SourceId = Enemy != nullptr ? Enemy->GetArenaEnemyId() : FKnightEntityId(EncounterId);
		Event.TargetId = Event.SourceId;
		Event.SubjectId = TEXT("Boss.Armor.Leg");
		Event.ValueId = ArmorState == EKnightBossArmorState::Intact ? TEXT("Intact")
			: ArmorState == EKnightBossArmorState::Brittle ? TEXT("Brittle") : TEXT("Broken");
		Events->PublishConfirmed(Event);
	}
}

EKnightBossArmorState UKnightBossArmorComponent::GetArmorState() const
{
	return ArmorState;
}
float UKnightBossArmorComponent::GetDurability() const
{
	return Durability;
}
float UKnightBossArmorComponent::GetStateEndServerTime() const
{
	return EndServerTime;
}
UStaticMeshComponent* UKnightBossArmorComponent::GetArmorZone() const
{
	return ArmorZone;
}

void UKnightBossArmorComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorld()->GetTimerManager().ClearTimer(ExpiryTimer);
	Super::EndPlay(EndPlayReason);
}
