#include "SkillLab/KnightRunestone.h"

#include "Character/KnightCharacter.h"
#include "Components/KnightLifeComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Events/KnightEventSubsystem.h"
#include "Events/KnightSkillLabEvent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"
#include "SkillLab/KnightBossArmorComponent.h"
#include "SkillLab/KnightFrozenSurface.h"
#include "SkillLab/KnightRunestonePickup.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

AKnightRunestone::AKnightRunestone()
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

void AKnightRunestone::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ThisClass, Kind);
	DOREPLIFETIME(ThisClass, State);
	DOREPLIFETIME(ThisClass, EndServerTime);
	DOREPLIFETIME(ThisClass, FixtureId);
}

void AKnightRunestone::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority())
	{
		FixtureId = FKnightEntityId::Create();
	}
	OnRep_State();
}

void AKnightRunestone::ConfigureFixture(const EKnightRunestoneKind InKind, const FGuid InEncounterId)
{
	if (HasAuthority())
	{
		Kind = InKind;
		ResetFixture(InEncounterId);
	}
}

bool AKnightRunestone::ValidateChannel() const
{
	AKnightCharacter* Knight = ChannelKnight.Get();
	if (!IsValid(Knight) || !Knight->GetLifeComponent()->IsAlive()
		|| Knight->GetKnightEntityId() != ChannelIdentity || Knight->GetController() != ChannelController.Get()
		|| !ChannelController.IsValid()
		|| FVector::DistSquared(Knight->GetActorLocation(), GetActorLocation()) > FMath::Square(200.0))
	{
		return false;
	}
	FHitResult Hit;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(KnightRunestoneChannelLOS), false, Knight);
	Query.AddIgnoredActor(this);
	return !GetWorld()->LineTraceSingleByChannel(Hit, Knight->GetActorLocation(),
		GetActorLocation() + FVector(0.0, 0.0, 70.0), ECC_Visibility, Query);
}

FKnightResult AKnightRunestone::TryActivate(AKnightCharacter* Knight, const FGuid RequestId)
{
	if (!HasAuthority() || !IsValid(Knight) || Knight->GetWorld() != GetWorld() || !RequestId.IsValid())
	{
		return FKnightResult::Failure(EKnightError::NotAuthority, TEXT("RunestoneActivate"), FText::GetEmpty());
	}
	if (State != EKnightRunestoneState::Ready || CommittedRequests.Contains(RequestId))
	{
		return FKnightResult::Failure(EKnightError::Conflict, TEXT("RunestoneReservation"),
			NSLOCTEXT("KnightSkillLab", "StoneReserved", "Runestone is channeling or on cooldown."));
	}
	ChannelKnight = Knight;
	ChannelController = Knight->GetController();
	ChannelIdentity = Knight->GetKnightEntityId();
	if (!ValidateChannel())
	{
		ChannelKnight.Reset();
		ChannelController.Reset();
		return FKnightResult::Failure(EKnightError::OutOfRange, TEXT("RunestoneChannel"),
			NSLOCTEXT("KnightSkillLab", "StoneRange", "A living Knight must stay within 2m with line of sight."));
	}
	ChannelRequest = RequestId;
	State = EKnightRunestoneState::Channeling;
	EndServerTime = GetWorld()->GetTimeSeconds() + 2.0f;
	GetWorldTimerManager().SetTimer(ChannelTimer, this, &ThisClass::AdvanceChannel, 0.1f, true);
	OnRep_State();
	ForceNetUpdate();
	return FKnightResult::Success();
}

bool AKnightRunestone::SpawnWorldResult()
{
	if (Kind == EKnightRunestoneKind::Frost)
	{
		bool bChanged = false;
		for (TActorIterator<AKnightFrozenSurface> It(GetWorld()); It; ++It)
		{
			if (It->ActorHasTag(TEXT("SkillLab.Freezable")))
			{
				It->FreezeSurface();
				bChanged = true;
			}
		}
		for (TActorIterator<AActor> It(GetWorld()); It; ++It)
		{
			UKnightBossArmorComponent* Armor = It->FindComponentByClass<UKnightBossArmorComponent>();
			if (Armor != nullptr && Armor->GetArmorZone() != nullptr)
			{
				bChanged |= Armor->MakeBrittle();
				break;
			}
		}
		return bChanged;
	}
	const int32 Count = Kind == EKnightRunestoneKind::Healing ? 3 : 1;
	TArray<TObjectPtr<AActor>> Batch;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const FVector Offset(130.0 + 100.0 * Index, 0.0, 10.0);
		const FTransform Transform(GetActorLocation() + Offset);
		AKnightRunestonePickup* Pickup = GetWorld()->SpawnActorDeferred<AKnightRunestonePickup>(
			AKnightRunestonePickup::StaticClass(), Transform, this, nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (Pickup == nullptr)
		{
			for (AActor* Actor : Batch)
			{
				Actor->Destroy();
			}
			return false;
		}
		Pickup->ConfigurePickup(Kind == EKnightRunestoneKind::Crystal ? EKnightRunestonePickupKind::PowerCrystal
			: (Index == 2 ? EKnightRunestonePickupKind::Fire : EKnightRunestonePickupKind::Healing), EncounterId);
		Pickup->FinishSpawning(Transform);
		Batch.Add(Pickup);
	}
	SpawnedObjects.Append(Batch);
	return true;
}

void AKnightRunestone::AdvanceChannel()
{
	if (!ValidateChannel())
	{
		GetWorldTimerManager().ClearTimer(ChannelTimer);
		State = EKnightRunestoneState::Ready;
		EndServerTime = 0.0f;
		ChannelKnight.Reset();
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
		State = EKnightRunestoneState::Ready;
		EndServerTime = 0.0f;
		OnRep_State();
		ForceNetUpdate();
		return;
	}
	State = EKnightRunestoneState::Cooldown;
	CommittedRequests.Add(ChannelRequest);
	EndServerTime = GetWorld()->GetTimeSeconds() + 30.0f;
	GetWorldTimerManager().SetTimer(CooldownTimer, this, &ThisClass::FinishCooldown, 30.0f, false);
	++Revision;
	OnRep_State();
	ForceNetUpdate();
	if (UKnightEventSubsystem* Events = GetWorld()->GetSubsystem<UKnightEventSubsystem>())
	{
		FKnightSkillLabEvent Event;
		Event.EventName = TEXT("RunestoneActivated");
		Event.ConfirmationId = FGuid::NewGuid();
		Event.RequestId = ChannelRequest;
		Event.EncounterId = EncounterId;
		Event.SourceId = ChannelIdentity;
		Event.TargetId = FixtureId;
		Event.SubjectId = Kind == EKnightRunestoneKind::Healing ? TEXT("RS.Healing")
			: (Kind == EKnightRunestoneKind::Crystal ? TEXT("RS.Crystal") : TEXT("RS.Frost"));
		Event.Revision = Revision;
		Events->PublishConfirmed(Event);
	}
	ChannelKnight.Reset();
	ChannelController.Reset();
}

void AKnightRunestone::FinishCooldown()
{
	State = EKnightRunestoneState::Ready;
	EndServerTime = 0.0f;
	OnRep_State();
	ForceNetUpdate();
}

void AKnightRunestone::ResetFixture(const FGuid InEncounterId)
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
	ChannelKnight.Reset();
	ChannelController.Reset();
	EncounterId = InEncounterId;
	CommittedRequests.Reset();
	FinishCooldown();
}

void AKnightRunestone::OnRep_State()
{
	const FText Name = Kind == EKnightRunestoneKind::Healing ? NSLOCTEXT("KnightSkillLab", "RSHealing", "HEALING RUNESTONE")
		: (Kind == EKnightRunestoneKind::Crystal ? NSLOCTEXT("KnightSkillLab", "RSCrystal", "POWER CRYSTAL RUNESTONE")
		: NSLOCTEXT("KnightSkillLab", "RSFrost", "FROST RUNESTONE"));
	const FText Status = State == EKnightRunestoneState::Ready ? NSLOCTEXT("KnightSkillLab", "RSReady", "F: channel 2s within 2m")
		: (State == EKnightRunestoneState::Channeling ? NSLOCTEXT("KnightSkillLab", "RSChannel", "CHANNELING — stay nearby")
		: NSLOCTEXT("KnightSkillLab", "RSCooldown", "COOLDOWN — 30s"));
	Label->SetText(FText::Format(NSLOCTEXT("KnightSkillLab", "RSLabel", "{0}\n{1}"), Name, Status));
	Label->SetTextRenderColor(State == EKnightRunestoneState::Channeling ? FColor::Yellow
		: (State == EKnightRunestoneState::Cooldown ? FColor::Silver : FColor::Cyan));
	OnStateChanged.Broadcast();
}

EKnightRunestoneKind AKnightRunestone::GetKind() const
{
	return Kind;
}
EKnightRunestoneState AKnightRunestone::GetState() const
{
	return State;
}
float AKnightRunestone::GetRemainingTime() const
{
	const AGameStateBase* GameState = GetWorld()->GetGameState();
	const float Now = GameState != nullptr ? GameState->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
	return FMath::Max(0.0f, EndServerTime - Now);
}

float AKnightRunestone::GetStateEndServerTime() const
{
	return EndServerTime;
}

void AKnightRunestone::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(ChannelTimer);
	GetWorldTimerManager().ClearTimer(CooldownTimer);
	Super::EndPlay(EndPlayReason);
}
