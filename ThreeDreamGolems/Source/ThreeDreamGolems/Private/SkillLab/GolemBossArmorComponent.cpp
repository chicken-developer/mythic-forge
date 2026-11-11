#include "SkillLab/GolemBossArmorComponent.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Events/GolemEventSubsystem.h"
#include "Events/GolemSkillLabEvent.h"
#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"
#include "PartyArena/GolemArenaEnemy.h"
#include "TimerManager.h"

UGolemBossArmorComponent::UGolemBossArmorComponent()
{
	SetIsReplicatedByDefault(true);
	PrimaryComponentTick.bCanEverTick = false;
}

void UGolemBossArmorComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ThisClass, ArmorState);
	DOREPLIFETIME(ThisClass, Durability);
	DOREPLIFETIME(ThisClass, EndServerTime);
	DOREPLIFETIME(ThisClass, bEnabled);
}

void UGolemBossArmorComponent::EnableArmorZone(const FGuid InEncounterId)
{
	if (!GetOwner()->HasAuthority())
	{
		return;
	}
	bEnabled = true;
	ResetArmor(InEncounterId);
	OnRep_Armor();
}

void UGolemBossArmorComponent::OnRep_Armor()
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
		ArmorZone->ComponentTags.Add(TEXT("GolemArmorZone"));
		GetOwner()->AddInstanceComponent(ArmorZone);
		ArmorZone->RegisterComponent();
	}
	if (ArmorZone != nullptr)
	{
		ArmorZone->SetVisibility(bEnabled);
		ArmorZone->SetRenderCustomDepth(ArmorState != EGolemBossArmorState::Intact);
		ArmorZone->SetCustomDepthStencilValue(ArmorState == EGolemBossArmorState::Brittle ? 2 : 3);
	}
	OnArmorChanged.Broadcast();
}

void UGolemBossArmorComponent::ResetArmor(const FGuid InEncounterId)
{
	if (!GetOwner()->HasAuthority())
	{
		return;
	}
	GetWorld()->GetTimerManager().ClearTimer(ExpiryTimer);
	EncounterId = InEncounterId;
	ArmorState = EGolemBossArmorState::Intact;
	Durability = 100.0f;
	EndServerTime = 0.0f;
	AppliedHits.Reset();
	PublishArmorChange();
}

bool UGolemBossArmorComponent::MakeBrittle()
{
	if (!GetOwner()->HasAuthority() || !bEnabled || ArmorState == EGolemBossArmorState::Broken)
	{
		return false;
	}
	ArmorState = EGolemBossArmorState::Brittle;
	EndServerTime = GetWorld()->GetTimeSeconds() + 12.0f;
	GetWorld()->GetTimerManager().SetTimer(ExpiryTimer, this, &ThisClass::ExpireArmorState, 12.0f, false);
	PublishArmorChange();
	return true;
}

void UGolemBossArmorComponent::ResolveArmorHit(const FHitResult& Hit, const FGuid& ActivationId,
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
	OutDamage *= ArmorState == EGolemBossArmorState::Intact ? 0.2f
		: (ArmorState == EGolemBossArmorState::Brittle ? 0.5f : 1.2f);
	if (ArmorState != EGolemBossArmorState::Broken)
	{
		const float DurabilityDamage = (bHeavy ? 20.0f : 10.0f)
			* (ArmorState == EGolemBossArmorState::Brittle ? 2.0f : 1.0f);
		Durability = FMath::Max(0.0f, Durability - DurabilityDamage);
		if (Durability == 0.0f)
		{
			ArmorState = EGolemBossArmorState::Broken;
			EndServerTime = GetWorld()->GetTimeSeconds() + 10.0f;
			GetWorld()->GetTimerManager().SetTimer(ExpiryTimer, this, &ThisClass::ExpireArmorState, 10.0f, false);
		}
		PublishArmorChange();
	}
}

void UGolemBossArmorComponent::ExpireArmorState()
{
	if (ArmorState == EGolemBossArmorState::Broken)
	{
		Durability = 100.0f;
	}
	ArmorState = EGolemBossArmorState::Intact;
	EndServerTime = 0.0f;
	PublishArmorChange();
}

void UGolemBossArmorComponent::PublishArmorChange()
{
	++Revision;
	OnRep_Armor();
	GetOwner()->ForceNetUpdate();
	if (UGolemEventSubsystem* Events = GetWorld()->GetSubsystem<UGolemEventSubsystem>())
	{
		FGolemSkillLabEvent Event;
		Event.EventName = TEXT("ArmorStateChanged");
		Event.ConfirmationId = FGuid::NewGuid();
		Event.RequestId = Event.ConfirmationId;
		Event.EncounterId = EncounterId;
		Event.Revision = Revision;
		const AGolemArenaEnemy* Enemy = Cast<AGolemArenaEnemy>(GetOwner());
		Event.SourceId = Enemy != nullptr ? Enemy->GetArenaEnemyId() : FGolemEntityId(EncounterId);
		Event.TargetId = Event.SourceId;
		Event.SubjectId = TEXT("Boss.Armor.Leg");
		Event.ValueId = ArmorState == EGolemBossArmorState::Intact ? TEXT("Intact")
			: ArmorState == EGolemBossArmorState::Brittle ? TEXT("Brittle") : TEXT("Broken");
		Events->PublishConfirmed(Event);
	}
}

EGolemBossArmorState UGolemBossArmorComponent::GetArmorState() const
{
	return ArmorState;
}
float UGolemBossArmorComponent::GetDurability() const
{
	return Durability;
}
float UGolemBossArmorComponent::GetStateEndServerTime() const
{
	return EndServerTime;
}
UStaticMeshComponent* UGolemBossArmorComponent::GetArmorZone() const
{
	return ArmorZone;
}

void UGolemBossArmorComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorld()->GetTimerManager().ClearTimer(ExpiryTimer);
	Super::EndPlay(EndPlayReason);
}
