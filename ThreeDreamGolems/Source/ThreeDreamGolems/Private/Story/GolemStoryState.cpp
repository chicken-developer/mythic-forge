#include "Story/GolemStoryState.h"
#include "Net/UnrealNetwork.h"

AGolemStoryState::AGolemStoryState()
{
	Quest = CreateDefaultSubobject<UGolemQuestComponent>(TEXT("Quest"));
	Dialogue = CreateDefaultSubobject<UGolemDialogueComponent>(TEXT("Dialogue"));
	WorldState = CreateDefaultSubobject<UGolemStoryWorldComponent>(TEXT("World"));
}

void AGolemStoryState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AGolemStoryState, WorldDefinitionId);
}

void AGolemStoryState::SetWorldDefinitionId(const FPrimaryAssetId& Id)
{
	if (HasAuthority() && Id.IsValid())
	{
		WorldDefinitionId = Id;
		ForceNetUpdate();
	}
}

FPrimaryAssetId AGolemStoryState::GetWorldDefinitionId() const
{
	return WorldDefinitionId;
}
