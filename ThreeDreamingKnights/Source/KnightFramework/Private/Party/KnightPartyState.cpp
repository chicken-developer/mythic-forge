#include "Party/KnightPartyState.h"

#include "Character/KnightCharacter.h"
#include "Events/KnightEventSubsystem.h"
#include "Logging/KnightLog.h"
#include "Player/KnightPlayerState.h"
#include "Settings/KnightSettings.h"
#include "AIController.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"

#define LOCTEXT_NAMESPACE "KnightPartyState"

bool FKnightRosterArray::NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParams)
{
	return FFastArraySerializer::FastArrayDeltaSerialize<FKnightRosterEntry, FKnightRosterArray>(
		Entries, DeltaParams, *this);
}

void FKnightRosterArray::PostReplicatedReceive(const FFastArraySerializer::FPostReplicatedReceiveParameters& Parameters)
{
	if (AKnightPartyState* Party = Owner.Get())
	{
		Party->NotifyRosterChanged();
	}
}

AKnightPartyState::AKnightPartyState()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	PrimaryActorTick.bCanEverTick = false;
}

void AKnightPartyState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AKnightPartyState, Roster);
	DOREPLIFETIME(AKnightPartyState, PartyId);
	DOREPLIFETIME(AKnightPartyState, RosterRevision);
}

const TArray<FKnightRosterEntry>& AKnightPartyState::GetRoster() const
{
	return Roster.Entries;
}

const FKnightRosterEntry* AKnightPartyState::FindSlot(int32 Slot) const
{
	return Roster.Entries.FindByPredicate([Slot](const FKnightRosterEntry& Entry)
		{
			return Entry.Slot == Slot;
		});
}

const FKnightRosterEntry* AKnightPartyState::FindKnight(const AKnightCharacter* Knight) const
{
	if (Knight == nullptr)
	{
		return nullptr;
	}
	return Roster.Entries.FindByPredicate([Knight](const FKnightRosterEntry& Entry)
		{
			return Entry.Knight == Knight;
		});
}

int32 AKnightPartyState::GetControlledSlot(const AController* Controller) const
{
	if (Controller == nullptr)
	{
		return INDEX_NONE;
	}
	for (const FKnightRosterEntry& Entry : Roster.Entries)
	{
		if (Entry.Controller == Controller)
		{
			return Entry.Slot;
		}
	}
	return INDEX_NONE;
}

int32 AKnightPartyState::GetHumanCount() const
{
	int32 Count = 0;
	for (const FKnightRosterEntry& Entry : Roster.Entries)
	{
		if (Entry.OwnerKind == EKnightControllerOwner::Human)
		{
			++Count;
		}
	}
	return Count;
}

int32 AKnightPartyState::GetRosterRevision() const
{
	return RosterRevision;
}

FKnightEntityId AKnightPartyState::GetPartyId() const
{
	return PartyId;
}

bool AKnightPartyState::IsCanonicalRosterReady() const
{
	if (Roster.Entries.Num() != UKnightSettings::CanonicalPartySize)
	{
		return false;
	}
	for (int32 Slot = 0; Slot < UKnightSettings::CanonicalPartySize; ++Slot)
	{
		const FKnightRosterEntry* Entry = FindSlot(Slot);
		if (Entry == nullptr || !Entry->KnightId.IsValid() || !IsValid(Entry->Knight)
			|| !Entry->HeroDefinitionId.IsValid())
		{
			return false;
		}
	}
	return true;
}

FKnightResult AKnightPartyState::InitializeRoster(const TArray<FKnightRosterEntry>& InEntries)
{
	if (!HasAuthority())
	{
		return FKnightResult::Failure(EKnightError::NotAuthority, TEXT("PartyRoster"),
			LOCTEXT("RosterAuthority", "Only the server may create the canonical roster."));
	}
	if (!Roster.Entries.IsEmpty())
	{
		return FKnightResult::Failure(EKnightError::InvalidState, TEXT("PartyRoster"),
			LOCTEXT("RosterAlreadyCreated", "The canonical roster already exists and cannot be rebuilt."));
	}
	const FKnightResult Validation = ValidateRosterEntries(InEntries);
	if (!Validation.IsSuccess())
	{
		return Validation;
	}
	TArray<FKnightRosterEntry> ValidatedEntries = InEntries;
	for (FKnightRosterEntry& Entry : ValidatedEntries)
	{
		if (Entry.Knight->GetWorld() != GetWorld() || !Entry.Knight->HasAuthority())
		{
			return FKnightResult::Failure(EKnightError::NotOwned, TEXT("PartyRoster"),
				LOCTEXT("RosterWorldOwnership", "Canonical Knights must belong to this authoritative world."));
		}
		const FKnightResult Ownership = ReadControllerOwnership(Entry);
		if (!Ownership.IsSuccess())
		{
			return Ownership;
		}
	}
	Roster.Owner = this;
	Roster.Entries = MoveTemp(ValidatedEntries);
	PartyId = FKnightEntityId::Create();
	RosterRevision = 1;
	TArray<int32> Slots;
	for (FKnightRosterEntry& Entry : Roster.Entries)
	{
		Roster.MarkItemDirty(Entry);
		Slots.Add(Entry.Slot);
	}
	ForceNetUpdate();
	NotifyRosterChanged();
	PublishRosterConfirmation(Slots);
	return FKnightResult::Success();
}

FKnightResult AKnightPartyState::RefreshControllerOwnership(TConstArrayView<int32> ChangedSlots)
{
	if (!HasAuthority())
	{
		return FKnightResult::Failure(EKnightError::NotAuthority, TEXT("PartyOwnership"),
			LOCTEXT("OwnershipAuthority", "Only the server may commit controller ownership."));
	}
	if (!IsCanonicalRosterReady() || ChangedSlots.IsEmpty())
	{
		return FKnightResult::Failure(EKnightError::InvalidState, TEXT("PartyOwnership"),
			LOCTEXT("OwnershipNotReady", "Ownership cannot commit before the canonical party is ready."));
	}
	TArray<FKnightRosterEntry> Updates;
	TSet<int32> UniqueSlots;
	for (int32 Slot : ChangedSlots)
	{
		const FKnightRosterEntry* Existing = FindSlot(Slot);
		if (Existing == nullptr)
		{
			return FKnightResult::Failure(EKnightError::OutOfRange, TEXT("PartyOwnership"),
				LOCTEXT("OwnershipInvalidSlot", "The ownership transaction references an invalid party slot."));
		}
		if (UniqueSlots.Contains(Slot))
		{
			continue;
		}
		UniqueSlots.Add(Slot);
		FKnightRosterEntry Updated = *Existing;
		const FKnightResult ReadResult = ReadControllerOwnership(Updated);
		if (!ReadResult.IsSuccess())
		{
			return ReadResult;
		}
		Updates.Add(Updated);
	}
	TArray<int32> CommittedSlots;
	for (const FKnightRosterEntry& Updated : Updates)
	{
		FKnightRosterEntry* Existing = Roster.Entries.FindByPredicate([&Updated](const FKnightRosterEntry& Entry)
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
	return FKnightResult::Success();
}

void AKnightPartyState::NotifyRosterChanged()
{
	OnRosterChanged.Broadcast();
}

void AKnightPartyState::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	Roster.Owner = this;
}

FKnightResult AKnightPartyState::ValidateRosterEntries(const TArray<FKnightRosterEntry>& Entries)
{
	if (Entries.Num() != UKnightSettings::CanonicalPartySize)
	{
		return FKnightResult::Failure(EKnightError::InvalidData, TEXT("PartyRoster"),
			LOCTEXT("RosterWrongCount", "The canonical roster must contain exactly three Knights."));
	}
	TSet<int32> Slots;
	TSet<FKnightEntityId> Ids;
	TSet<FPrimaryAssetId> Definitions;
	TSet<const AKnightCharacter*> Actors;
	for (const FKnightRosterEntry& Entry : Entries)
	{
		if (Entry.Slot < 0 || Entry.Slot >= UKnightSettings::CanonicalPartySize
			|| !Entry.KnightId.IsValid() || !IsValid(Entry.Knight)
			|| !KnightIdentity::IsValidDefinitionId(Entry.HeroDefinitionId)
			|| Entry.HeroDefinitionId.PrimaryAssetType != FPrimaryAssetType(TEXT("KnightHero")))
		{
			return FKnightResult::Failure(EKnightError::InvalidData, TEXT("PartyRoster"),
				LOCTEXT("RosterInvalidEntry", "A roster entry has an invalid slot, identity, definition or character."));
		}
		if (Slots.Contains(Entry.Slot) || Ids.Contains(Entry.KnightId)
			|| Definitions.Contains(Entry.HeroDefinitionId) || Actors.Contains(Entry.Knight.Get()))
		{
			return FKnightResult::Failure(EKnightError::DuplicateId, TEXT("PartyRoster"),
				LOCTEXT("RosterDuplicate", "Canonical slots, identities, definitions and characters must be unique."));
		}
		if (Entry.Knight->GetKnightEntityId() != Entry.KnightId
			|| Entry.Knight->GetHeroDefinitionId() != Entry.HeroDefinitionId)
		{
			return FKnightResult::Failure(EKnightError::Conflict, TEXT("PartyRoster"),
				LOCTEXT("RosterIdentityMismatch", "A character's immutable identity does not match its roster entry."));
		}
		Slots.Add(Entry.Slot);
		Ids.Add(Entry.KnightId);
		Definitions.Add(Entry.HeroDefinitionId);
		Actors.Add(Entry.Knight.Get());
	}
	return FKnightResult::Success();
}

FKnightResult AKnightPartyState::ReadControllerOwnership(FKnightRosterEntry& Entry)
{
	AController* Controller = Entry.Knight->GetController();
	if (!IsValid(Controller) || Controller->GetPawn() != Entry.Knight)
	{
		return FKnightResult::Failure(EKnightError::InvalidState, TEXT("PartyOwnership"),
			LOCTEXT("ControllerMissing", "Every canonical Knight requires a valid controller before ownership commits."));
	}
	Entry.Controller = Controller;
	Entry.HumanPlayerState = nullptr;
	Entry.HumanId.Invalidate();
	Entry.HumanPlayerId = INDEX_NONE;
	if (const APlayerController* Player = Cast<APlayerController>(Controller))
	{
		AKnightPlayerState* Human = Player->GetPlayerState<AKnightPlayerState>();
		if (Human == nullptr || !Human->GetHumanId().IsValid())
		{
			return FKnightResult::Failure(EKnightError::InvalidState, TEXT("PartyOwnership"),
				LOCTEXT("HumanIdentityMissing", "A human controller requires an initialized human PlayerState."));
		}
		Entry.OwnerKind = EKnightControllerOwner::Human;
		Entry.HumanPlayerState = Human;
		Entry.HumanId = Human->GetHumanId();
		Entry.HumanPlayerId = Human->GetPlayerId();
		return FKnightResult::Success();
	}
	if (!Controller->IsA<AAIController>())
	{
		return FKnightResult::Failure(EKnightError::TypeMismatch, TEXT("PartyOwnership"),
			LOCTEXT("UnsupportedController", "Canonical party ownership supports human or companion AI controllers."));
	}
	Entry.OwnerKind = EKnightControllerOwner::Companion;
	return FKnightResult::Success();
}

void AKnightPartyState::PublishRosterConfirmation(const TArray<int32>& ChangedSlots)
{
	if (UKnightEventSubsystem* Bus = GetWorld()->GetSubsystem<UKnightEventSubsystem>())
	{
		FKnightPartyRosterChangedEvent Event;
		Event.ConfirmationId = FGuid::NewGuid();
		Event.Revision = RosterRevision;
		Event.SourceId = PartyId;
		Event.ChangedSlots = ChangedSlots;
		const FKnightResult Result = Bus->PublishConfirmed(Event);
		if (!Result.IsSuccess())
		{
			UE_LOG(LogKnightCore, Error, TEXT("Roster confirmation failed [%s]: %s"),
				*PartyId.Value.ToString(), *Result.Message.ToString());
		}
	}
}

#undef LOCTEXT_NAMESPACE
