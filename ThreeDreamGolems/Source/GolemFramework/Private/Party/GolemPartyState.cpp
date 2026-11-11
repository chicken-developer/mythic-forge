#include "Party/GolemPartyState.h"

#include "Character/GolemCharacter.h"
#include "Events/GolemEventSubsystem.h"
#include "Logging/GolemLog.h"
#include "Player/GolemPlayerState.h"
#include "Settings/GolemSettings.h"
#include "AIController.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"

#define LOCTEXT_NAMESPACE "GolemPartyState"

bool FGolemRosterArray::NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParams)
{
	return FFastArraySerializer::FastArrayDeltaSerialize<FGolemRosterEntry, FGolemRosterArray>(
		Entries, DeltaParams, *this);
}

void FGolemRosterArray::PostReplicatedReceive(const FFastArraySerializer::FPostReplicatedReceiveParameters& Parameters)
{
	if (AGolemPartyState* Party = Owner.Get())
	{
		Party->NotifyRosterChanged();
	}
}

AGolemPartyState::AGolemPartyState()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	PrimaryActorTick.bCanEverTick = false;
}

void AGolemPartyState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AGolemPartyState, Roster);
	DOREPLIFETIME(AGolemPartyState, PartyId);
	DOREPLIFETIME(AGolemPartyState, RosterRevision);
}

const TArray<FGolemRosterEntry>& AGolemPartyState::GetRoster() const
{
	return Roster.Entries;
}

const FGolemRosterEntry* AGolemPartyState::FindSlot(int32 Slot) const
{
	return Roster.Entries.FindByPredicate([Slot](const FGolemRosterEntry& Entry)
		{
			return Entry.Slot == Slot;
		});
}

const FGolemRosterEntry* AGolemPartyState::FindGolem(const AGolemCharacter* Golem) const
{
	if (Golem == nullptr)
	{
		return nullptr;
	}
	return Roster.Entries.FindByPredicate([Golem](const FGolemRosterEntry& Entry)
		{
			return Entry.Golem == Golem;
		});
}

int32 AGolemPartyState::GetControlledSlot(const AController* Controller) const
{
	if (Controller == nullptr)
	{
		return INDEX_NONE;
	}
	for (const FGolemRosterEntry& Entry : Roster.Entries)
	{
		if (Entry.Controller == Controller)
		{
			return Entry.Slot;
		}
	}
	return INDEX_NONE;
}

int32 AGolemPartyState::GetHumanCount() const
{
	int32 Count = 0;
	for (const FGolemRosterEntry& Entry : Roster.Entries)
	{
		if (Entry.OwnerKind == EGolemControllerOwner::Human)
		{
			++Count;
		}
	}
	return Count;
}

int32 AGolemPartyState::GetRosterRevision() const
{
	return RosterRevision;
}

FGolemEntityId AGolemPartyState::GetPartyId() const
{
	return PartyId;
}

bool AGolemPartyState::IsCanonicalRosterReady() const
{
	if (Roster.Entries.Num() != UGolemSettings::CanonicalPartySize)
	{
		return false;
	}
	for (int32 Slot = 0; Slot < UGolemSettings::CanonicalPartySize; ++Slot)
	{
		const FGolemRosterEntry* Entry = FindSlot(Slot);
		if (Entry == nullptr || !Entry->GolemId.IsValid() || !IsValid(Entry->Golem)
			|| !Entry->HeroDefinitionId.IsValid())
		{
			return false;
		}
	}
	return true;
}

FGolemResult AGolemPartyState::InitializeRoster(const TArray<FGolemRosterEntry>& InEntries)
{
	if (!HasAuthority())
	{
		return FGolemResult::Failure(EGolemError::NotAuthority, TEXT("PartyRoster"),
			LOCTEXT("RosterAuthority", "Only the server may create the canonical roster."));
	}
	if (!Roster.Entries.IsEmpty())
	{
		return FGolemResult::Failure(EGolemError::InvalidState, TEXT("PartyRoster"),
			LOCTEXT("RosterAlreadyCreated", "The canonical roster already exists and cannot be rebuilt."));
	}
	const FGolemResult Validation = ValidateRosterEntries(InEntries);
	if (!Validation.IsSuccess())
	{
		return Validation;
	}
	TArray<FGolemRosterEntry> ValidatedEntries = InEntries;
	for (FGolemRosterEntry& Entry : ValidatedEntries)
	{
		if (Entry.Golem->GetWorld() != GetWorld() || !Entry.Golem->HasAuthority())
		{
			return FGolemResult::Failure(EGolemError::NotOwned, TEXT("PartyRoster"),
				LOCTEXT("RosterWorldOwnership", "Canonical Golems must belong to this authoritative world."));
		}
		const FGolemResult Ownership = ReadControllerOwnership(Entry);
		if (!Ownership.IsSuccess())
		{
			return Ownership;
		}
	}
	Roster.Owner = this;
	Roster.Entries = MoveTemp(ValidatedEntries);
	PartyId = FGolemEntityId::Create();
	RosterRevision = 1;
	TArray<int32> Slots;
	for (FGolemRosterEntry& Entry : Roster.Entries)
	{
		Roster.MarkItemDirty(Entry);
		Slots.Add(Entry.Slot);
	}
	ForceNetUpdate();
	NotifyRosterChanged();
	PublishRosterConfirmation(Slots);
	return FGolemResult::Success();
}

FGolemResult AGolemPartyState::RefreshControllerOwnership(TConstArrayView<int32> ChangedSlots)
{
	if (!HasAuthority())
	{
		return FGolemResult::Failure(EGolemError::NotAuthority, TEXT("PartyOwnership"),
			LOCTEXT("OwnershipAuthority", "Only the server may commit controller ownership."));
	}
	if (!IsCanonicalRosterReady() || ChangedSlots.IsEmpty())
	{
		return FGolemResult::Failure(EGolemError::InvalidState, TEXT("PartyOwnership"),
			LOCTEXT("OwnershipNotReady", "Ownership cannot commit before the canonical party is ready."));
	}
	TArray<FGolemRosterEntry> Updates;
	TSet<int32> UniqueSlots;
	for (int32 Slot : ChangedSlots)
	{
		const FGolemRosterEntry* Existing = FindSlot(Slot);
		if (Existing == nullptr)
		{
			return FGolemResult::Failure(EGolemError::OutOfRange, TEXT("PartyOwnership"),
				LOCTEXT("OwnershipInvalidSlot", "The ownership transaction references an invalid party slot."));
		}
		if (UniqueSlots.Contains(Slot))
		{
			continue;
		}
		UniqueSlots.Add(Slot);
		FGolemRosterEntry Updated = *Existing;
		const FGolemResult ReadResult = ReadControllerOwnership(Updated);
		if (!ReadResult.IsSuccess())
		{
			return ReadResult;
		}
		Updates.Add(Updated);
	}
	TArray<int32> CommittedSlots;
	for (const FGolemRosterEntry& Updated : Updates)
	{
		FGolemRosterEntry* Existing = Roster.Entries.FindByPredicate([&Updated](const FGolemRosterEntry& Entry)
			{
				return Entry.Slot == Updated.Slot;
			});
		*Existing = Updated;
		Roster.MarkItemDirty(*Existing);
		CommittedSlots.Add(Updated.Slot);
	}
	++RosterRevision;
	ForceNetUpdate();
	NotifyRosterChanged();
	PublishRosterConfirmation(CommittedSlots);
	return FGolemResult::Success();
}

void AGolemPartyState::NotifyRosterChanged()
{
	OnRosterChanged.Broadcast();
}

void AGolemPartyState::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	Roster.Owner = this;
}

FGolemResult AGolemPartyState::ValidateRosterEntries(const TArray<FGolemRosterEntry>& Entries)
{
	if (Entries.Num() != UGolemSettings::CanonicalPartySize)
	{
		return FGolemResult::Failure(EGolemError::InvalidData, TEXT("PartyRoster"),
			LOCTEXT("RosterWrongCount", "The canonical roster must contain exactly three Golems."));
	}
	TSet<int32> Slots;
	TSet<FGolemEntityId> Ids;
	TSet<FPrimaryAssetId> Definitions;
	TSet<const AGolemCharacter*> Actors;
	for (const FGolemRosterEntry& Entry : Entries)
	{
		if (Entry.Slot < 0 || Entry.Slot >= UGolemSettings::CanonicalPartySize
			|| !Entry.GolemId.IsValid() || !IsValid(Entry.Golem)
			|| !GolemIdentity::IsValidDefinitionId(Entry.HeroDefinitionId)
			|| Entry.HeroDefinitionId.PrimaryAssetType != FPrimaryAssetType(TEXT("GolemHero")))
		{
			return FGolemResult::Failure(EGolemError::InvalidData, TEXT("PartyRoster"),
				LOCTEXT("RosterInvalidEntry", "A roster entry has an invalid slot, identity, definition or character."));
		}
		if (Slots.Contains(Entry.Slot) || Ids.Contains(Entry.GolemId)
			|| Definitions.Contains(Entry.HeroDefinitionId) || Actors.Contains(Entry.Golem.Get()))
		{
			return FGolemResult::Failure(EGolemError::DuplicateId, TEXT("PartyRoster"),
				LOCTEXT("RosterDuplicate", "Canonical slots, identities, definitions and characters must be unique."));
		}
		if (Entry.Golem->GetGolemEntityId() != Entry.GolemId
			|| Entry.Golem->GetHeroDefinitionId() != Entry.HeroDefinitionId)
		{
			return FGolemResult::Failure(EGolemError::Conflict, TEXT("PartyRoster"),
				LOCTEXT("RosterIdentityMismatch", "A character's immutable identity does not match its roster entry."));
		}
		Slots.Add(Entry.Slot);
		Ids.Add(Entry.GolemId);
		Definitions.Add(Entry.HeroDefinitionId);
		Actors.Add(Entry.Golem.Get());
	}
	return FGolemResult::Success();
}

FGolemResult AGolemPartyState::ReadControllerOwnership(FGolemRosterEntry& Entry)
{
	AController* Controller = Entry.Golem->GetController();
	if (!IsValid(Controller) || Controller->GetPawn() != Entry.Golem)
	{
		return FGolemResult::Failure(EGolemError::InvalidState, TEXT("PartyOwnership"),
			LOCTEXT("ControllerMissing", "Every canonical Golem requires a valid controller before ownership commits."));
	}
	Entry.Controller = Controller;
	Entry.HumanPlayerState = nullptr;
	Entry.HumanId.Invalidate();
	Entry.HumanPlayerId = INDEX_NONE;
	if (const APlayerController* Player = Cast<APlayerController>(Controller))
	{
		AGolemPlayerState* Human = Player->GetPlayerState<AGolemPlayerState>();
		if (Human == nullptr || !Human->GetHumanId().IsValid())
		{
			return FGolemResult::Failure(EGolemError::InvalidState, TEXT("PartyOwnership"),
				LOCTEXT("HumanIdentityMissing", "A human controller requires an initialized human PlayerState."));
		}
		Entry.OwnerKind = EGolemControllerOwner::Human;
		Entry.HumanPlayerState = Human;
		Entry.HumanId = Human->GetHumanId();
		Entry.HumanPlayerId = Human->GetPlayerId();
		return FGolemResult::Success();
	}
	if (!Controller->IsA<AAIController>())
	{
		return FGolemResult::Failure(EGolemError::TypeMismatch, TEXT("PartyOwnership"),
			LOCTEXT("UnsupportedController", "Canonical party ownership supports human or companion AI controllers."));
	}
	Entry.OwnerKind = EGolemControllerOwner::Companion;
	return FGolemResult::Success();
}

void AGolemPartyState::PublishRosterConfirmation(const TArray<int32>& ChangedSlots)
{
	if (UGolemEventSubsystem* Bus = GetWorld()->GetSubsystem<UGolemEventSubsystem>())
	{
		FGolemPartyRosterChangedEvent Event;
		Event.ConfirmationId = FGuid::NewGuid();
		Event.Revision = RosterRevision;
		Event.SourceId = PartyId;
		Event.ChangedSlots = ChangedSlots;
		const FGolemResult Result = Bus->PublishConfirmed(Event);
		if (!Result.IsSuccess())
		{
			UE_LOG(LogGolemCore, Error, TEXT("Roster confirmation failed [%s]: %s"),
				*PartyId.Value.ToString(), *Result.Message.ToString());
		}
	}
}

#undef LOCTEXT_NAMESPACE
