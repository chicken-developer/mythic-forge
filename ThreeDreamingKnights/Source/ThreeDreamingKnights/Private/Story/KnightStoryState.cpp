#include "Story/KnightStoryState.h"
#include "Net/UnrealNetwork.h"

AKnightStoryState::AKnightStoryState()
{
	Quest = CreateDefaultSubobject<UKnightQuestComponent>(TEXT("Quest"));
	Dialogue = CreateDefaultSubobject<UKnightDialogueComponent>(TEXT("Dialogue"));
	WorldState = CreateDefaultSubobject<UKnightStoryWorldComponent>(TEXT("World"));
}

void AKnightStoryState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AKnightStoryState, WorldDefinitionId);
}

void AKnightStoryState::SetWorldDefinitionId(const FPrimaryAssetId& Id)
{
	if (HasAuthority() && Id.IsValid())
	{
		WorldDefinitionId = Id;
		ForceNetUpdate();
	}
}

FPrimaryAssetId AKnightStoryState::GetWorldDefinitionId() const
{
	return WorldDefinitionId;
}
