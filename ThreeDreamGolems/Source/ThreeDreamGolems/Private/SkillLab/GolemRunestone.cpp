#include "SkillLab/GolemRunestone.h"

#include "Character/GolemCharacter.h"
#include "Components/GolemLifeComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Events/GolemEventSubsystem.h"
#include "Events/GolemSkillLabEvent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"
#include "SkillLab/GolemBossArmorComponent.h"
#include "SkillLab/GolemFrozenSurface.h"
#include "SkillLab/GolemRunestonePickup.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

AGolemRunestone::AGolemRunestone()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	PrimaryActorTick.bCanEverTick = false;
	Stone = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Runestone"));
	SetRootComponent(Stone);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	Stone->SetStaticMesh(Cylinder.Object);
	Stone->SetRelativeScale3D(FVector(0.65, 0.65, 1.2));
	Stone->SetCollisionProfileName(TEXT("BlockAll"));
	Stone->SetCanEverAffectNavigation(false);
	Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("RunestoneLabel"));
	Label->SetupAttachment(Stone);
	Label->SetRelativeLocation(FVector(0.0, 0.0, 125.0));
	Label->SetWorldSize(20.0f);
}

void AGolemRunestone::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ThisClass, Kind);
	DOREPLIFETIME(ThisClass, State);
	DOREPLIFETIME(ThisClass, EndServerTime);
	DOREPLIFETIME(ThisClass, FixtureId);
}

void AGolemRunestone::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority())
	{
		FixtureId = FGolemEntityId::Create();
	}
	OnRep_State();
}

void AGolemRunestone::ConfigureFixture(const EGolemRunestoneKind InKind, const FGuid InEncounterId)
{
	if (HasAuthority())
	{
		Kind = InKind;
		ResetFixture(InEncounterId);
	}
}

bool AGolemRunestone::ValidateChannel() const
{
	AGolemCharacter* Golem = ChannelGolem.Get();
	if (!IsValid(Golem) || !Golem->GetLifeComponent()->IsAlive()
		|| Golem->GetGolemEntityId() != ChannelIdentity || Golem->GetController() != ChannelController.Get()
		|| !ChannelController.IsValid()
		|| FVector::DistSquared(Golem->GetActorLocation(), GetActorLocation()) > FMath::Square(200.0))
	{
		return false;
	}
	FHitResult Hit;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(GolemRunestoneChannelLOS), false, Golem);
	Query.AddIgnoredActor(this);
	return !GetWorld()->LineTraceSingleByChannel(Hit, Golem->GetActorLocation(),
		GetActorLocation() + FVector(0.0, 0.0, 70.0), ECC_Visibility, Query);
}

FGolemResult AGolemRunestone::TryActivate(AGolemCharacter* Golem, const FGuid RequestId)
{
	if (!HasAuthority() || !IsValid(Golem) || Golem->GetWorld() != GetWorld() || !RequestId.IsValid())
	{
		return FGolemResult::Failure(EGolemError::NotAuthority, TEXT("RunestoneActivate"), FText::GetEmpty());
	}
	if (State != EGolemRunestoneState::Ready || CommittedRequests.Contains(RequestId))
	{
		return FGolemResult::Failure(EGolemError::Conflict, TEXT("RunestoneReservation"),
			NSLOCTEXT("GolemSkillLab", "StoneReserved", "Runestone is channeling or on cooldown."));
	}
	ChannelGolem = Golem;
	ChannelController = Golem->GetController();
	ChannelIdentity = Golem->GetGolemEntityId();
	if (!ValidateChannel())
	{
		ChannelGolem.Reset();
		ChannelController.Reset();
		return FGolemResult::Failure(EGolemError::OutOfRange, TEXT("RunestoneChannel"),
			NSLOCTEXT("GolemSkillLab", "StoneRange", "A living Golem must stay within 2m with line of sight."));
	}
	ChannelRequest = RequestId;
	State = EGolemRunestoneState::Channeling;
	EndServerTime = GetWorld()->GetTimeSeconds() + 2.0f;
	GetWorldTimerManager().SetTimer(ChannelTimer, this, &ThisClass::AdvanceChannel, 0.1f, true);
	OnRep_State();
	ForceNetUpdate();
	return FGolemResult::Success();
}

bool AGolemRunestone::SpawnWorldResult()
{
	if (Kind == EGolemRunestoneKind::Frost)
	{
		bool bChanged = false;
		for (TActorIterator<AGolemFrozenSurface> It(GetWorld()); It; ++It)
		{
			if (It->ActorHasTag(TEXT("SkillLab.Freezable")))
			{
				It->FreezeSurface();
				bChanged = true;
			}
		}
		for (TActorIterator<AActor> It(GetWorld()); It; ++It)
		{
			UGolemBossArmorComponent* Armor = It->FindComponentByClass<UGolemBossArmorComponent>();
			if (Armor != nullptr && Armor->GetArmorZone() != nullptr)
			{
				bChanged |= Armor->MakeBrittle();
				break;
			}
		}
		return bChanged;
	}
	const int32 Count = Kind == EGolemRunestoneKind::Healing ? 3 : 1;
	TArray<TObjectPtr<AActor>> Batch;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const FVector Offset(130.0 + 100.0 * Index, 0.0, 10.0);
		const FTransform Transform(GetActorLocation() + Offset);
		AGolemRunestonePickup* Pickup = GetWorld()->SpawnActorDeferred<AGolemRunestonePickup>(
			AGolemRunestonePickup::StaticClass(), Transform, this, nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (Pickup == nullptr)
		{
			for (AActor* Actor : Batch)
			{
				Actor->Destroy();
			}
			return false;
		}
		Pickup->ConfigurePickup(Kind == EGolemRunestoneKind::Crystal ? EGolemRunestonePickupKind::PowerCrystal
			: (Index == 2 ? EGolemRunestonePickupKind::Fire : EGolemRunestonePickupKind::Healing), EncounterId);
		Pickup->FinishSpawning(Transform);
		Batch.Add(Pickup);
	}
	SpawnedObjects.Append(Batch);
	return true;
}

void AGolemRunestone::AdvanceChannel()
{
	if (!ValidateChannel())
	{
		GetWorldTimerManager().ClearTimer(ChannelTimer);
		State = EGolemRunestoneState::Ready;
		EndServerTime = 0.0f;
		ChannelGolem.Reset();
		ChannelController.Reset();
		OnRep_State();
		ForceNetUpdate();
		return;
	}
	if (GetWorld()->GetTimeSeconds() < EndServerTime)
	{
		return;
	}
	GetWorldTimerManager().ClearTimer(ChannelTimer);
	if (!SpawnWorldResult())
	{
		State = EGolemRunestoneState::Ready;
		EndServerTime = 0.0f;
		OnRep_State();
		ForceNetUpdate();
		return;
	}
	State = EGolemRunestoneState::Cooldown;
	CommittedRequests.Add(ChannelRequest);
	EndServerTime = GetWorld()->GetTimeSeconds() + 30.0f;
	GetWorldTimerManager().SetTimer(CooldownTimer, this, &ThisClass::FinishCooldown, 30.0f, false);
	++Revision;
	OnRep_State();
	ForceNetUpdate();
	if (UGolemEventSubsystem* Events = GetWorld()->GetSubsystem<UGolemEventSubsystem>())
	{
		FGolemSkillLabEvent Event;
		Event.EventName = TEXT("RunestoneActivated");
		Event.ConfirmationId = FGuid::NewGuid();
		Event.RequestId = ChannelRequest;
		Event.EncounterId = EncounterId;
		Event.SourceId = ChannelIdentity;
		Event.TargetId = FixtureId;
		Event.SubjectId = Kind == EGolemRunestoneKind::Healing ? TEXT("RS.Healing")
			: (Kind == EGolemRunestoneKind::Crystal ? TEXT("RS.Crystal") : TEXT("RS.Frost"));
		Event.Revision = Revision;
		Events->PublishConfirmed(Event);
	}
	ChannelGolem.Reset();
	ChannelController.Reset();
}

void AGolemRunestone::FinishCooldown()
{
	State = EGolemRunestoneState::Ready;
	EndServerTime = 0.0f;
	OnRep_State();
	ForceNetUpdate();
}

void AGolemRunestone::ResetFixture(const FGuid InEncounterId)
{
	if (!HasAuthority())
	{
		return;
	}
	GetWorldTimerManager().ClearTimer(ChannelTimer);
	GetWorldTimerManager().ClearTimer(CooldownTimer);
	for (AActor* Actor : SpawnedObjects)
	{
		if (IsValid(Actor))
		{
			Actor->Destroy();
		}
	}
	SpawnedObjects.Reset();
	ChannelGolem.Reset();
	ChannelController.Reset();
	EncounterId = InEncounterId;
	CommittedRequests.Reset();
	FinishCooldown();
}

void AGolemRunestone::OnRep_State()
{
	const FText Name = Kind == EGolemRunestoneKind::Healing ? NSLOCTEXT("GolemSkillLab", "RSHealing", "HEALING RUNESTONE")
		: (Kind == EGolemRunestoneKind::Crystal ? NSLOCTEXT("GolemSkillLab", "RSCrystal", "POWER CRYSTAL RUNESTONE")
		: NSLOCTEXT("GolemSkillLab", "RSFrost", "FROST RUNESTONE"));
	const FText Status = State == EGolemRunestoneState::Ready ? NSLOCTEXT("GolemSkillLab", "RSReady", "F: channel 2s within 2m")
		: (State == EGolemRunestoneState::Channeling ? NSLOCTEXT("GolemSkillLab", "RSChannel", "CHANNELING — stay nearby")
		: NSLOCTEXT("GolemSkillLab", "RSCooldown", "COOLDOWN — 30s"));
	Label->SetText(FText::Format(NSLOCTEXT("GolemSkillLab", "RSLabel", "{0}\n{1}"), Name, Status));
	Label->SetTextRenderColor(State == EGolemRunestoneState::Channeling ? FColor::Yellow
		: (State == EGolemRunestoneState::Cooldown ? FColor::Silver : FColor::Cyan));
	OnStateChanged.Broadcast();
}

EGolemRunestoneKind AGolemRunestone::GetKind() const
{
	return Kind;
}
EGolemRunestoneState AGolemRunestone::GetState() const
{
	return State;
}
float AGolemRunestone::GetRemainingTime() const
{
	const AGameStateBase* GameState = GetWorld()->GetGameState();
	const float Now = GameState != nullptr ? GameState->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
	return FMath::Max(0.0f, EndServerTime - Now);
}

float AGolemRunestone::GetStateEndServerTime() const
{
	return EndServerTime;
}

void AGolemRunestone::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(ChannelTimer);
	GetWorldTimerManager().ClearTimer(CooldownTimer);
	Super::EndPlay(EndPlayReason);
}
