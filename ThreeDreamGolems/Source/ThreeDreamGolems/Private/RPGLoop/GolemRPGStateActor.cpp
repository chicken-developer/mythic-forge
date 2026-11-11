#include "RPGLoop/GolemRPGStateActor.h"

#include "Engine/World.h"
#include "Inventory/GolemInventoryComponent.h"
#include "Net/UnrealNetwork.h"
#include "Progression/GolemRPGComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogGolemRPGState, Log, All);

AGolemRPGStateActor::AGolemRPGStateActor()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	PrimaryActorTick.bCanEverTick = false;
	Inventory = CreateDefaultSubobject<UGolemInventoryComponent>(TEXT("SharedInventory"));
	RPG = CreateDefaultSubobject<UGolemRPGComponent>(TEXT("GolemProgression"));
}

void AGolemRPGStateActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AGolemRPGStateActor, LoopSnapshot);
}

UGolemInventoryComponent* AGolemRPGStateActor::GetInventory() const
{
	return Inventory;
}

void AGolemRPGStateActor::BeginPlay()
{
	Super::BeginPlay();
	if (!HasAuthority())
	{
		const FGolemResult Bind = RPG->BindReplicatedInventory(Inventory);
		if (!Bind.IsSuccess())
		{
			UE_LOG(LogGolemRPGState, Error, TEXT("Replicated RPG inventory binding failed [%s]: %s"),
				*Bind.Context.ToString(), *Bind.Message.ToString());
		}
	}
}

UGolemRPGComponent* AGolemRPGStateActor::GetRPG() const
{
	return RPG;
}

const FGolemRPGLoopSnapshot& AGolemRPGStateActor::GetLoopSnapshot() const
{
	return LoopSnapshot;
}

FGolemResult AGolemRPGStateActor::CommitLoopSnapshot(const FGolemRPGLoopSnapshot& Candidate,
	int64 ExpectedRevision)
{
	if (!HasAuthority() || GetWorld() == nullptr || GetWorld()->bIsTearingDown || IsActorBeingDestroyed())
	{
		return FGolemResult::Failure(EGolemError::NotAuthority, TEXT("RPGLoopCommit"), FText::GetEmpty());
	}
	if (ExpectedRevision != LoopSnapshot.Revision || ExpectedRevision == MAX_int64
		|| static_cast<uint8>(Candidate.Stage) > static_cast<uint8>(EGolemRPGLoopStage::Complete)
		|| Candidate.RewardPickupCount < 0 || Candidate.RewardPickupCount > 32
		|| (LoopSnapshot.bRewardIssued && !Candidate.bRewardIssued))
	{
		return FGolemResult::Failure(EGolemError::InvalidRevision, TEXT("RPGLoopCommit"), FText::GetEmpty());
	}
	LoopSnapshot = Candidate;
	LoopSnapshot.Revision = ExpectedRevision + 1;
	ForceNetUpdate();
	OnLoopChanged.Broadcast(LoopSnapshot);
	return FGolemResult::Success();
}

void AGolemRPGStateActor::OnRep_LoopSnapshot()
{
	OnLoopChanged.Broadcast(LoopSnapshot);
}
