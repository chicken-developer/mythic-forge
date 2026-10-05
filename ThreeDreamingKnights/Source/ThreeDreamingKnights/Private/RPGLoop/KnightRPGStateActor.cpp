#include "RPGLoop/KnightRPGStateActor.h"

#include "Engine/World.h"
#include "Inventory/KnightInventoryComponent.h"
#include "Net/UnrealNetwork.h"
#include "Progression/KnightRPGComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogKnightRPGState, Log, All);

AKnightRPGStateActor::AKnightRPGStateActor()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	PrimaryActorTick.bCanEverTick = false;
	Inventory = CreateDefaultSubobject<UKnightInventoryComponent>(TEXT("SharedInventory"));
	RPG = CreateDefaultSubobject<UKnightRPGComponent>(TEXT("KnightProgression"));
}

void AKnightRPGStateActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AKnightRPGStateActor, LoopSnapshot);
}

UKnightInventoryComponent* AKnightRPGStateActor::GetInventory() const
{
	return Inventory;
}

void AKnightRPGStateActor::BeginPlay()
{
	Super::BeginPlay();
	if (!HasAuthority())
	{
		const FKnightResult Bind = RPG->BindReplicatedInventory(Inventory);
		if (!Bind.IsSuccess())
		{
			UE_LOG(LogKnightRPGState, Error, TEXT("Replicated RPG inventory binding failed [%s]: %s"),
				*Bind.Context.ToString(), *Bind.Message.ToString());
		}
	}
}

UKnightRPGComponent* AKnightRPGStateActor::GetRPG() const
{
	return RPG;
}

const FKnightRPGLoopSnapshot& AKnightRPGStateActor::GetLoopSnapshot() const
{
	return LoopSnapshot;
}

FKnightResult AKnightRPGStateActor::CommitLoopSnapshot(const FKnightRPGLoopSnapshot& Candidate,
	int64 ExpectedRevision)
{
	if (!HasAuthority() || GetWorld() == nullptr || GetWorld()->bIsTearingDown || IsActorBeingDestroyed())
	{
		return FKnightResult::Failure(EKnightError::NotAuthority, TEXT("RPGLoopCommit"), FText::GetEmpty());
	}
	if (ExpectedRevision != LoopSnapshot.Revision || ExpectedRevision == MAX_int64
		|| static_cast<uint8>(Candidate.Stage) > static_cast<uint8>(EKnightRPGLoopStage::Complete)
		|| Candidate.RewardPickupCount < 0 || Candidate.RewardPickupCount > 32
		|| (LoopSnapshot.bRewardIssued && !Candidate.bRewardIssued))
	{
		return FKnightResult::Failure(EKnightError::InvalidRevision, TEXT("RPGLoopCommit"), FText::GetEmpty());
	}
	LoopSnapshot = Candidate;
	LoopSnapshot.Revision = ExpectedRevision + 1;
	ForceNetUpdate();
	OnLoopChanged.Broadcast(LoopSnapshot);
	return FKnightResult::Success();
}

void AKnightRPGStateActor::OnRep_LoopSnapshot()
{
	OnLoopChanged.Broadcast(LoopSnapshot);
}
