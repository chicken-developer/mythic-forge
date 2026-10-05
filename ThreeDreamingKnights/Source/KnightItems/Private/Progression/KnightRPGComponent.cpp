#include "Progression/KnightRPGComponent.h"

#include "Assets/KnightAssetManager.h"
#include "Assets/KnightDefinition.h"
#include "Components/KnightAbilitySystemComponent.h"
#include "Components/KnightLifeComponent.h"
#include "Definitions/KnightItemDefinition.h"
#include "Definitions/KnightLootTableDefinition.h"
#include "Definitions/KnightSkillDefinition.h"
#include "Definitions/KnightSkillTreeDefinition.h"
#include "Inventory/KnightInventoryComponent.h"
#include "Loot/KnightItemPickup.h"
#include "Engine/World.h"
#include "Misc/ScopeExit.h"
#include "Net/UnrealNetwork.h"
#include "UObject/UObjectGlobals.h"

#define LOCTEXT_NAMESPACE "KnightRPGComponent"

bool FKnightRPGArray::NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParams)
{
	return FastArrayDeltaSerialize<FKnightRPGRecord, FKnightRPGArray>(Records, DeltaParams, *this);
}

void FKnightRPGArray::PostReplicatedReceive(const FFastArraySerializer::FPostReplicatedReceiveParameters& Parameters)
{
	if (UKnightRPGComponent* Component = Owner.Get())
	{
		Component->HandleReplicatedStateReady();
	}
}

UKnightRPGComponent::UKnightRPGComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UKnightRPGComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UKnightRPGComponent, ReplicatedState);
}

FKnightResult UKnightRPGComponent::InitializeRPG(UKnightInventoryComponent* InInventory,
	const FPrimaryAssetId& InSkillTreeId, TConstArrayView<FKnightRPGKnightRegistration> InRegistrations, int32 InitialPoints)
{
	const FKnightResult Authority = ValidateAuthority();
	if (!Authority.IsSuccess())
	{
		return Authority;
	}
	if (IsInitialized() || bTransactionInProgress || !IsValid(InInventory) || InInventory->GetOwner() != GetOwner()
		|| !InInventory->IsInitialized() || InInventory->IsReserved()
		|| InitialPoints < 0 || InitialPoints > KnightRPG::MaximumSkillPoints)
	{
		return FKnightResult::Failure(EKnightError::InvalidState, TEXT("RPGInitialize"),
			LOCTEXT("InitializeState", "RPG initialization requires a fresh coordinator and its initialized unreserved sibling inventory."));
	}
	const FKnightResult RegistrationResult = ValidateRegistrations(InRegistrations);
	if (!RegistrationResult.IsSuccess())
	{
		return RegistrationResult;
	}
	const FKnightResult GraphResult = ResolveSkills(InSkillTreeId);
	if (!GraphResult.IsSuccess())
	{
		return GraphResult;
	}
	FKnightInventorySnapshot Baseline;
	const FKnightResult CaptureResult = InInventory->CaptureSnapshot(Baseline);
	if (!CaptureResult.IsSuccess())
	{
		return CaptureResult;
	}
	FKnightRPGView Candidate;
	Candidate.Revision = 1;
	Candidate.InventoryId = Baseline.InventoryId;
	Candidate.InventoryRevision = Baseline.Revision;
	Candidate.SkillTreeId = InSkillTreeId;
	for (const FKnightRPGKnightRegistration& Registration : InRegistrations)
	{
		FKnightRPGKnightView Knight;
		Knight.KnightId = Registration.KnightId;
		Knight.HeroDefinitionId = Registration.HeroDefinitionId;
		Knight.Revision = 1;
		Knight.SkillPoints = InitialPoints;
		Candidate.Knights.Add(Knight);
		Registrations.Add(Registration.KnightId, Registration);
	}
	SortView(Candidate);
	InitialSkillPoints = InitialPoints;
	InitialInventory = MoveTemp(Baseline);
	InitialView = Candidate;
	PublishedView = MoveTemp(Candidate);
	BindInventory(InInventory);
	RefreshReplicatedState();
	OnSnapshotChanged.Broadcast();
	return FKnightResult::Success();
}

FKnightResult UKnightRPGComponent::BindReplicatedInventory(UKnightInventoryComponent* InInventory)
{
	if (!IsValid(InInventory) || InInventory->GetOwner() != GetOwner() || bEndingPlay
		|| (Inventory != nullptr && Inventory != InInventory))
	{
		return FKnightResult::Failure(EKnightError::InvalidState, TEXT("RPGBindInventory"),
			LOCTEXT("BindInventory", "The replicated read model requires its own live inventory sibling."));
	}
	BindInventory(InInventory);
	HandleReplicatedStateReady();
	return FKnightResult::Success();
}

FKnightResult UKnightRPGComponent::InitializeFixtureCatalog(TConstArrayView<UKnightDefinition*> Definitions)
{
	const FKnightResult Authority = ValidateAuthority();
	if (!Authority.IsSuccess())
	{
		return Authority;
	}
	if (IsInitialized() || bHasFixtureCatalog || bTransactionInProgress || Definitions.IsEmpty()
		|| Definitions.Num() > KnightItems::MaximumInventoryCapacity)
	{
		return FKnightResult::Failure(EKnightError::InvalidState, TEXT("RPGFixtureCatalog"),
			LOCTEXT("FixtureState", "An immutable actual-definition fixture catalog is configured once before initialization."));
	}
	TMap<FPrimaryAssetId, TObjectPtr<UKnightDefinition>> Copies;
	for (UKnightDefinition* Definition : Definitions)
	{
		if (!IsValid(Definition) || !Definition->ValidateIdentity().IsSuccess()
			|| Copies.Contains(Definition->GetPrimaryAssetId()))
		{
			return FKnightResult::Failure(EKnightError::InvalidData, TEXT("RPGFixtureCatalog"),
				LOCTEXT("FixtureIdentity", "Fixture entries must be actual definitions with unique validated stable identities."));
		}
		FKnightResult Validation = FKnightResult::Success();
		if (const UKnightItemDefinition* Item = Cast<UKnightItemDefinition>(Definition))
		{
			Validation = Item->ValidateItem();
		}
		else if (const UKnightSkillDefinition* Skill = Cast<UKnightSkillDefinition>(Definition))
		{
			Validation = Skill->ValidateSkill();
		}
		else if (const UKnightSkillTreeDefinition* Tree = Cast<UKnightSkillTreeDefinition>(Definition))
		{
			Validation = Tree->ValidateSkillTree();
		}
		else if (const UKnightLootTableDefinition* Loot = Cast<UKnightLootTableDefinition>(Definition))
		{
			Validation = Loot->ValidateLootTable();
		}
		if (!Validation.IsSuccess())
		{
			return Validation;
		}
		UKnightDefinition* Copy = DuplicateObject<UKnightDefinition>(Definition, this);
		if (!IsValid(Copy) || Copy->GetPrimaryAssetId() != Definition->GetPrimaryAssetId())
		{
			return FKnightResult::Failure(EKnightError::LoadFailed, TEXT("RPGFixtureCatalog"),
				LOCTEXT("FixtureCopy", "A validated definition could not be copied into the isolated fixture catalog."));
		}
		Copy->SetFlags(RF_Transient);
		Copies.Add(Copy->GetPrimaryAssetId(), Copy);
	}
	FixtureDefinitions = MoveTemp(Copies);
	bHasFixtureCatalog = true;
	return FKnightResult::Success();
}

FKnightRPGCommandResult UKnightRPGComponent::ExecuteCommand(const FKnightRPGCallerContext& InCaller,
	const FKnightRPGCommand& InCommand)
{
	const FKnightRPGCallerContext Caller = InCaller;
	const FKnightRPGCommand Command = InCommand;
	FKnightRPGCommandResult Outcome;
	Outcome.Operation = Command.Operation;
	Outcome.RequestId = Command.RequestId;
	Outcome.KnightId = Command.ExpectedKnightId;
	Outcome.InventoryId = Command.InventoryId;
	Outcome.Result = ValidateAuthority();
	if (!Outcome.Result.IsSuccess())
	{
		return Outcome;
	}
	if (bTransactionInProgress)
	{
		Outcome.Result = FKnightResult::Failure(EKnightError::Conflict, TEXT("RPGTransaction"),
			LOCTEXT("Reentry", "An atomic RPG transaction is already preparing, committing or publishing."));
		return Outcome;
	}
	Outcome.Result = ValidateCaller(Caller);
	if (!Outcome.Result.IsSuccess())
	{
		return Outcome;
	}
	if (const int32* Index = ReceiptIndices.Find(Command.RequestId))
	{
		const FKnightRPGReceipt Receipt = Receipts[*Index];
		if (Receipt.HumanId != Caller.HumanId || !(Receipt.Command == Command))
		{
			Outcome.Result = FKnightResult::Failure(EKnightError::Conflict, TEXT("RPGReplay"),
				LOCTEXT("ReplayIdentity", "This request already committed for another human or an altered exact payload."));
			return Outcome;
		}
		Outcome.Result = FKnightResult::Success();
		Outcome.CommittedInventoryRevision = Receipt.CommittedInventoryRevision;
		Outcome.CommittedKnightRevision = Receipt.CommittedKnight.Revision;
		Outcome.AffectedInstanceId = Receipt.AffectedInstanceId;
		Outcome.bWasReplay = true;
		return Outcome;
	}
	Outcome.Result = Command.ValidateShape();
	if (!Outcome.Result.IsSuccess())
	{
		return Outcome;
	}
	if (Caller.KnightId != Command.ExpectedKnightId)
	{
		Outcome.Result = FKnightResult::Failure(EKnightError::NotOwned, TEXT("RPGCaller"),
			LOCTEXT("WrongKnight", "A fresh RPG command must target the caller's current canonical Knight."));
		return Outcome;
	}
	const FKnightRPGKnightView KnightBefore = GetKnightView(Caller.KnightId);
	const FKnightInventoryView InventoryBefore = Inventory->GetView();
	if (Command.InventoryId != InventoryBefore.InventoryId || Command.ExpectedInventoryRevision != InventoryBefore.Revision
		|| Command.ExpectedKnightRevision != KnightBefore.Revision)
	{
		Outcome.Result = FKnightResult::Failure(EKnightError::InvalidRevision, TEXT("RPGRevision"),
			LOCTEXT("Stale", "The inventory or Knight revision changed before this new command."));
		return Outcome;
	}
	if (Receipts.Num() >= KnightItems::MaximumTransactionReceipts || PublishedView.Revision >= MAX_int64)
	{
		Outcome.Result = FKnightResult::Failure(EKnightError::CapacityExceeded, TEXT("RPGJournal"),
			LOCTEXT("JournalFull", "The finite RPG journal cannot discard earlier successful replay protection."));
		return Outcome;
	}
	UKnightAbilitySystemComponent* ASC = Caller.AbilitySystem.Get();
	UKnightInventoryComponent* TransactionInventory = Inventory;
	const TWeakObjectPtr<AActor> PublicationOwner(GetOwner());
	if (ASC->GetLifeComponent() == nullptr || !ASC->GetLifeComponent()->IsAlive()
		|| (Command.Operation != EKnightRPGOperation::Pickup && !Caller.bInSafeZone))
	{
		Outcome.Result = FKnightResult::Failure(EKnightError::InvalidState, TEXT("RPGSafeZone"),
			LOCTEXT("SafeZone", "Loadout and skill changes require a living Knight in the authoritative safe zone."));
		return Outcome;
	}
	bTransactionInProgress = true;
	FKnightInventoryCapability InventoryCapability;
	FKnightMutationFence Fence;
	FKnightProjectionCapability ProjectionCapability;
	AKnightItemPickup* Pickup = nullptr;
	FGuid PickupToken;
	bool bCommitted = false;
	ON_SCOPE_EXIT
	{
		if (!bCommitted)
		{
			if (IsValid(ASC) && ProjectionCapability.IsValid())
			{
				ASC->AbortSourceProjection(Fence, ProjectionCapability);
			}
			if (IsValid(TransactionInventory) && InventoryCapability.IsValid())
			{
				TransactionInventory->AbortPrepared(InventoryCapability);
			}
			if (IsValid(Pickup) && PickupToken.IsValid())
			{
				Pickup->AbortClaim(PickupToken);
			}
		}
		if (IsValid(ASC) && Fence.IsValid())
		{
			if (bCommitted && ProjectionCapability.IsValid())
			{
				ASC->PublishSourceProjection(Fence, ProjectionCapability, false, PublicationOwner.Get());
			}
			ASC->EndMutationFence(Fence);
		}
		if (bCommitted && IsValid(TransactionInventory))
		{
			TransactionInventory->ResetPrepared();
		}
		bTransactionInProgress = false;
	};
	FKnightItemInstance PickedItem;
	if (Command.Operation == EKnightRPGOperation::Pickup)
	{
		const TWeakObjectPtr<AKnightItemPickup>* Registered = Pickups.Find(Command.PickupId);
		Pickup = Registered == nullptr ? nullptr : Registered->Get();
		if (!IsValid(Pickup) || Pickup->GetWorld() != GetWorld() || Pickup->GetPickupId() != Command.PickupId)
		{
			Outcome.Result = FKnightResult::Failure(EKnightError::NotFound, TEXT("RPGPickup"),
				LOCTEXT("PickupMissing", "The exact registered live pickup is unavailable."));
			return Outcome;
		}
		if (Receipts.ContainsByPredicate([&Command](const FKnightRPGReceipt& Receipt)
			{
				return Receipt.Command.Operation == EKnightRPGOperation::Pickup
					&& Receipt.Command.PickupId == Command.PickupId;
			}))
		{
			Outcome.Result = FKnightResult::Failure(EKnightError::Conflict, TEXT("RPGPickup"),
				LOCTEXT("DurablePickupClaim", "This pickup identity already has a durable successful claim."));
			return Outcome;
		}
		AActor* Pawn = Caller.Pawn.Get();
		if (FVector::DistSquared(Pawn->GetActorLocation(), Pickup->GetActorLocation())
			> FMath::Square(KnightRPG::PickupRange))
		{
			Outcome.Result = FKnightResult::Failure(EKnightError::OutOfRange, TEXT("RPGPickup"),
				LOCTEXT("PickupRange", "Move within 300 centimeters of the pickup."));
			return Outcome;
		}
		FCollisionQueryParams Query(SCENE_QUERY_STAT(KnightPickupVisibility), false, Pawn);
		Query.AddIgnoredActor(Pickup);
		FHitResult Hit;
		if (GetWorld()->LineTraceSingleByChannel(Hit, Pawn->GetActorLocation(), Pickup->GetActorLocation(),
			ECC_Visibility, Query))
		{
			Outcome.Result = FKnightResult::Failure(EKnightError::OutOfRange, TEXT("RPGPickup"),
				LOCTEXT("PickupLOS", "The authoritative pickup line of sight is blocked."));
			return Outcome;
		}
		Outcome.Result = Pickup->ReserveClaim(Command.RequestId, PickupToken);
		if (!Outcome.Result.IsSuccess())
		{
			return Outcome;
		}
		PickedItem = Pickup->GetView().Item;
	}
	FKnightRPGView Candidate;
	FKnightInventoryRequest InventoryRequest;
	Outcome.Result = BuildCandidate(PublishedView, InventoryBefore, Command,
		Pickup == nullptr ? nullptr : &PickedItem, nullptr, Candidate, InventoryRequest, Outcome.AffectedInstanceId);
	if (!Outcome.Result.IsSuccess())
	{
		return Outcome;
	}
	Outcome.Result = InventoryRequest.Mutations.IsEmpty()
		? Inventory->ReserveRead(Command.InventoryId, Command.ExpectedInventoryRevision, InventoryCapability)
		: Inventory->PrepareTransaction(InventoryRequest, InventoryCapability);
	if (!Outcome.Result.IsSuccess())
	{
		return Outcome;
	}
	FKnightInventoryView PreparedInventory;
	Outcome.Result = Inventory->GetPreparedView(InventoryCapability, PreparedInventory);
	if (!Outcome.Result.IsSuccess())
	{
		return Outcome;
	}
	FKnightRPGKnightView* CandidateKnight = Candidate.Knights.FindByPredicate([&Caller](const FKnightRPGKnightView& Knight)
		{
			return Knight.KnightId == Caller.KnightId;
		});
	FKnightGASProjectionPlan Plan;
	Outcome.Result = BuildProjection(*CandidateKnight, PreparedInventory.Items, Plan);
	if (!Outcome.Result.IsSuccess())
	{
		return Outcome;
	}
	if (Command.Operation != EKnightRPGOperation::Pickup)
	{
		Outcome.Result = ASC->BeginMutationFence(Command.RequestId, Fence);
		if (Outcome.Result.IsSuccess())
		{
			Outcome.Result = ASC->PrepareSourceProjection(Fence, Plan,
				ASC->GetSourceProjection(KnightRPG::ProjectionSource).Revision, ProjectionCapability);
		}
		if (Outcome.Result.IsSuccess())
		{
			Outcome.Result = ASC->StageSourceProjection(Fence, ProjectionCapability);
		}
		if (!Outcome.Result.IsSuccess())
		{
			return Outcome;
		}
		// Raw GAS staging callbacks may end either owner or invalidate a staged SDK handle.
		Outcome.Result = ValidateAuthority();
		if (Outcome.Result.IsSuccess())
		{
			Outcome.Result = ValidateCaller(Caller);
		}
		FKnightInventoryView RevalidatedInventory;
		if (Outcome.Result.IsSuccess())
		{
			Outcome.Result = TransactionInventory->GetPreparedView(InventoryCapability, RevalidatedInventory);
		}
		if (Outcome.Result.IsSuccess() && !(RevalidatedInventory == PreparedInventory))
		{
			Outcome.Result = FKnightResult::Failure(EKnightError::Conflict, TEXT("RPGStage"),
				LOCTEXT("ChangedPreparedInventory", "The exact prepared inventory changed during GAS staging."));
		}
		if (Outcome.Result.IsSuccess())
		{
			Outcome.Result = ASC->ValidateStagedSourceProjection(Fence, ProjectionCapability);
		}
		if (!Outcome.Result.IsSuccess())
		{
			return Outcome;
		}
		Outcome.Result = ASC->CommitSourceProjection(Fence, ProjectionCapability);
		if (!Outcome.Result.IsSuccess())
		{
			return Outcome;
		}
	}
	// No fallible data work remains. Both exact prepared domains commit before any subscriber runs.
	Outcome.Result = Inventory->CommitPrepared(InventoryCapability);
	if (!Outcome.Result.IsSuccess())
	{
		return Outcome;
	}
	Candidate.InventoryRevision = PreparedInventory.Revision;
	FKnightRPGReceipt Receipt;
	Receipt.HumanId = Caller.HumanId;
	Receipt.Command = Command;
	Receipt.InventoryRequest = InventoryRequest;
	Receipt.PickedItem = PickedItem;
	Receipt.CommittedKnight = *CandidateKnight;
	Receipt.CommittedInventoryRevision = PreparedInventory.Revision;
	Receipt.AffectedInstanceId = Outcome.AffectedInstanceId;
	Outcome.CommittedInventoryRevision = Receipt.CommittedInventoryRevision;
	Outcome.CommittedKnightRevision = Receipt.CommittedKnight.Revision;
	ReceiptIndices.Add(Command.RequestId, Receipts.Num());
	Receipts.Add(MoveTemp(Receipt));
	PublishedView = MoveTemp(Candidate);
	Inventory->AssignedInstanceIds.Reset();
	for (const FKnightRPGKnightView& Knight : PublishedView.Knights)
	{
		for (const FKnightEquipmentAssignment& Assignment : Knight.Assignments)
		{
			Inventory->AssignedInstanceIds.Add(Assignment.ItemInstanceId);
		}
	}
	if (Pickup != nullptr)
	{
		Pickup->CommitClaim(PickupToken, Command.RequestId);
	}
	bCommitted = true;
	RefreshReplicatedState();
	const auto CanPublish = [this, &Caller]()
	{
		return ValidateAuthority().IsSuccess() && ValidateCaller(Caller).IsSuccess();
	};
	if (ProjectionCapability.IsValid())
	{
		ASC->PublishSourceProjection(Fence, ProjectionCapability, true, PublicationOwner.Get());
		if (!CanPublish())
		{
			return Outcome;
		}
	}
	TransactionInventory->PublishPrepared(InventoryCapability, false);
	if (!CanPublish())
	{
		return Outcome;
	}
	if (IsValid(Pickup) && !Pickup->IsActorBeingDestroyed())
	{
		Pickup->PublishClaim();
		if (!CanPublish())
		{
			return Outcome;
		}
	}
	OnSnapshotChanged.Broadcast();
	if (!CanPublish())
	{
		return Outcome;
	}
	OnCommandCompleted.Broadcast(Outcome);
	return Outcome;
}

FKnightResult UKnightRPGComponent::RegisterPickup(AKnightItemPickup* Pickup)
{
	const FKnightResult Authority = ValidateAuthority();
	if (!Authority.IsSuccess())
	{
		return Authority;
	}
	if (!IsInitialized() || bTransactionInProgress || !IsValid(Pickup) || Pickup->GetWorld() != GetWorld()
		|| !Pickup->HasAuthority() || !Pickup->GetPickupId().IsValid() || Pickup->IsClaimed()
		|| !Pickup->GetView().Item.ValidateIdentityAndCount().IsSuccess())
	{
		return FKnightResult::Failure(EKnightError::InvalidState, TEXT("RPGPickupRegister"),
			LOCTEXT("RegisterPickup", "Register a live initialized authoritative pickup in this coordinator's world."));
	}
	const FGuid PickupId = Pickup->GetPickupId();
	if (Receipts.ContainsByPredicate([&PickupId](const FKnightRPGReceipt& Receipt)
		{
			return Receipt.Command.Operation == EKnightRPGOperation::Pickup && Receipt.Command.PickupId == PickupId;
		}))
	{
		return FKnightResult::Failure(EKnightError::Conflict, TEXT("RPGPickupRegister"),
			LOCTEXT("RegisterClaimedIdentity", "A durably claimed pickup identity cannot register a fresh unclaimed actor."));
	}
	if (const TWeakObjectPtr<AKnightItemPickup>* Existing = Pickups.Find(Pickup->GetPickupId()))
	{
		if (Existing->Get() == Pickup)
		{
			return FKnightResult::Success();
		}
		return FKnightResult::Failure(EKnightError::DuplicateId, TEXT("RPGPickupRegister"),
			LOCTEXT("DuplicatePickup", "A pickup identity cannot be registered for another actor."));
	}
	Pickups.Add(Pickup->GetPickupId(), Pickup);
	return FKnightResult::Success();
}

FKnightResult UKnightRPGComponent::UnregisterUnclaimedPickup(AKnightItemPickup* Pickup)
{
	const FKnightResult Authority = ValidateAuthority();
	if (!Authority.IsSuccess())
	{
		return Authority;
	}
	if (bTransactionInProgress)
	{
		return FKnightResult::Failure(EKnightError::Conflict, TEXT("RPGPickupUnregister"),
			LOCTEXT("UnregisterReserved", "A pickup cannot be withdrawn during an RPG transaction."));
	}
	if (!IsInitialized() || !IsValid(Pickup) || Pickup->IsActorBeingDestroyed() || !Pickup->HasAuthority()
		|| Pickup->GetWorld() != GetWorld() || !Pickup->GetPickupId().IsValid())
	{
		return FKnightResult::Failure(EKnightError::InvalidState, TEXT("RPGPickupUnregister"),
			LOCTEXT("UnregisterLifetime", "Withdraw only a live authoritative pickup from this coordinator's world."));
	}
	const FGuid PickupId = Pickup->GetPickupId();
	const TWeakObjectPtr<AKnightItemPickup>* Registered = Pickups.Find(PickupId);
	if (Registered == nullptr || Registered->Get() != Pickup)
	{
		return FKnightResult::Failure(EKnightError::NotOwned, TEXT("RPGPickupUnregister"),
			LOCTEXT("UnregisterExactActor", "Only the exact registered pickup actor owns this withdrawal."));
	}
	if (Pickup->IsClaimed() || Pickup->ReservedToken.IsValid() || Receipts.ContainsByPredicate(
		[&PickupId](const FKnightRPGReceipt& Receipt)
		{
			return Receipt.Command.Operation == EKnightRPGOperation::Pickup && Receipt.Command.PickupId == PickupId;
		}))
	{
		return FKnightResult::Failure(EKnightError::Conflict, TEXT("RPGPickupUnregister"),
			LOCTEXT("UnregisterClaim", "A reserved or durably claimed pickup identity cannot be withdrawn."));
	}
	Pickups.Remove(PickupId);
	return FKnightResult::Success();
}

FKnightRPGView UKnightRPGComponent::GetView() const
{
	return PublishedView;
}

FKnightRPGKnightView UKnightRPGComponent::GetKnightView(const FKnightEntityId& KnightId) const
{
	const FKnightRPGKnightView* Knight = PublishedView.Knights.FindByPredicate([&KnightId](const FKnightRPGKnightView& Entry)
		{
			return Entry.KnightId == KnightId;
		});
	return Knight == nullptr ? FKnightRPGKnightView() : *Knight;
}

bool UKnightRPGComponent::IsInitialized() const
{
	return PublishedView.IsInitialized();
}

bool UKnightRPGComponent::IsTransactionInProgress() const
{
	return bTransactionInProgress;
}

FKnightResult UKnightRPGComponent::ValidateAuthority() const
{
	const AActor* OwnerActor = GetOwner();
	const UWorld* World = GetWorld();
	if (!IsInGameThread() || bEndingPlay || !IsValid(OwnerActor) || OwnerActor->IsActorBeingDestroyed()
		|| World == nullptr || World->bIsTearingDown)
	{
		return FKnightResult::Failure(EKnightError::ShuttingDown, TEXT("RPGAuthority"),
			LOCTEXT("Lifetime", "The RPG owner or its world has ended its mutation lifetime."));
	}
	if (!OwnerActor->HasAuthority() || World->GetNetMode() == NM_Client)
	{
		return FKnightResult::Failure(EKnightError::NotAuthority, TEXT("RPGAuthority"),
			LOCTEXT("Authority", "Only the authoritative campaign server may execute RPG commands."));
	}
	return FKnightResult::Success();
}

FKnightResult UKnightRPGComponent::ValidateCaller(const FKnightRPGCallerContext& Caller) const
{
	const FKnightRPGKnightRegistration* Registration = Registrations.Find(Caller.KnightId);
	AActor* Pawn = Caller.Pawn.Get();
	UKnightAbilitySystemComponent* ASC = Caller.AbilitySystem.Get();
	if (!IsInitialized() || Inventory == nullptr || !Caller.HumanId.IsValid() || Registration == nullptr
		|| !IsValid(Pawn) || !IsValid(ASC) || Pawn->IsActorBeingDestroyed() || !Pawn->HasAuthority()
		|| Pawn->GetWorld() != GetWorld() || Registration->Pawn.Get() != Pawn
		|| Registration->AbilitySystem.Get() != ASC || Registration->HeroDefinitionId != Caller.HeroDefinitionId
		|| ASC->GetEntityId() != Caller.KnightId || ASC->GetOwnerActor() != Pawn || ASC->GetAvatarActor() != Pawn)
	{
		return FKnightResult::Failure(EKnightError::NotOwned, TEXT("RPGCaller"),
			LOCTEXT("Caller", "The trusted caller must match a live registered canonical Knight and its own initialized ASC."));
	}
	return FKnightResult::Success();
}

FKnightResult UKnightRPGComponent::ValidateRegistrations(TConstArrayView<FKnightRPGKnightRegistration> InRegistrations,
	const FKnightRPGView* ExpectedView, bool bAllowMutationFence, bool bRequireFreshSource) const
{
	if (InRegistrations.Num() != 3)
	{
		return FKnightResult::Failure(EKnightError::InvalidData, TEXT("RPGRegistrations"),
			LOCTEXT("CanonicalCount", "RPG composition requires exactly three canonical Knight registrations."));
	}
	TSet<FKnightEntityId> Ids;
	TSet<AActor*> Pawns;
	for (const FKnightRPGKnightRegistration& Registration : InRegistrations)
	{
		AActor* Pawn = Registration.Pawn.Get();
		UKnightAbilitySystemComponent* ASC = Registration.AbilitySystem.Get();
		if (!Registration.KnightId.IsValid() || !KnightIdentity::IsValidDefinitionId(Registration.HeroDefinitionId)
			|| Registration.HeroDefinitionId.PrimaryAssetType != FPrimaryAssetType(TEXT("KnightHero"))
			|| !IsValid(Pawn) || !IsValid(ASC) || Pawn->IsActorBeingDestroyed()
			|| !Pawn->HasAuthority() || Pawn->GetWorld() != GetWorld()
			|| ASC->GetEntityId() != Registration.KnightId || ASC->GetOwnerActor() != Pawn || ASC->GetAvatarActor() != Pawn
			|| (bRequireFreshSource && ASC->GetSourceProjection(KnightRPG::ProjectionSource).Revision != 0)
			|| (!bAllowMutationFence && ASC->IsMutationFenced())
			|| Ids.Contains(Registration.KnightId) || Pawns.Contains(Pawn))
		{
			return FKnightResult::Failure(EKnightError::InvalidData, TEXT("RPGRegistrations"),
				LOCTEXT("Registration", "Each canonical registration requires a distinct live initialized ASC and a fresh RPG source."));
		}
		if (ExpectedView != nullptr && !ExpectedView->Knights.ContainsByPredicate(
			[&Registration](const FKnightRPGKnightView& Knight)
			{
				return Knight.KnightId == Registration.KnightId && Knight.HeroDefinitionId == Registration.HeroDefinitionId;
			}))
		{
			return FKnightResult::Failure(EKnightError::InvalidId, TEXT("RPGRegistrations"),
				LOCTEXT("RestoreRegistration", "Reconstruction registrations must retain the snapshot's canonical Knight and hero identities."));
		}
		Ids.Add(Registration.KnightId);
		Pawns.Add(Pawn);
	}
	return FKnightResult::Success();
}

FKnightResult UKnightRPGComponent::ResolveDefinition(const FPrimaryAssetId& Id, UClass* ExpectedClass,
	const UKnightDefinition*& OutDefinition) const
{
	OutDefinition = nullptr;
	if (!KnightIdentity::IsValidDefinitionId(Id))
	{
		return FKnightResult::Failure(EKnightError::InvalidId, TEXT("RPGDefinition"),
			LOCTEXT("DefinitionId", "The requested definition identity is invalid."));
	}
	if (bHasFixtureCatalog)
	{
		const TObjectPtr<UKnightDefinition>* Definition = FixtureDefinitions.Find(Id);
		OutDefinition = Definition == nullptr ? nullptr : Definition->Get();
	}
	else
	{
		UKnightAssetManager* Manager = UKnightAssetManager::GetKnightAssetManager();
		if (Manager == nullptr)
		{
			return FKnightResult::Failure(EKnightError::LoadFailed, TEXT("RPGDefinition"),
				LOCTEXT("AssetManager", "The production typed definition catalog is unavailable."));
		}
		UKnightDefinition* Definition = nullptr;
		const FKnightResult Resolution = Manager->ResolveDefinition(Id, ExpectedClass, Definition);
		if (!Resolution.IsSuccess())
		{
			return Resolution;
		}
		OutDefinition = Definition;
	}
	if (!IsValid(OutDefinition) || OutDefinition->GetPrimaryAssetId() != Id)
	{
		return FKnightResult::Failure(EKnightError::NotFound, FName(*Id.ToString()),
			LOCTEXT("DefinitionMissing", "No actual definition resolves this stable identity."));
	}
	if (!OutDefinition->IsA(ExpectedClass))
	{
		OutDefinition = nullptr;
		return FKnightResult::Failure(EKnightError::TypeMismatch, FName(*Id.ToString()),
			LOCTEXT("DefinitionClass", "The actual definition does not have the required domain type."));
	}
	return OutDefinition->ValidateIdentity();
}

FKnightResult UKnightRPGComponent::ResolveSkills(const FPrimaryAssetId& TreeId)
{
	const UKnightDefinition* Definition = nullptr;
	FKnightResult Result = ResolveDefinition(TreeId, UKnightSkillTreeDefinition::StaticClass(), Definition);
	if (!Result.IsSuccess())
	{
		return Result;
	}
	const UKnightSkillTreeDefinition* Tree = CastChecked<UKnightSkillTreeDefinition>(Definition);
	Result = Tree->ValidateSkillTree();
	if (!Result.IsSuccess())
	{
		return Result;
	}
	TMap<FPrimaryAssetId, TObjectPtr<UKnightSkillDefinition>> Candidate;
	TArray<const UKnightSkillDefinition*> Graph;
	for (const FPrimaryAssetId& Id : Tree->GetSkills())
	{
		const UKnightDefinition* SkillDefinition = nullptr;
		Result = ResolveDefinition(Id, UKnightSkillDefinition::StaticClass(), SkillDefinition);
		if (!Result.IsSuccess())
		{
			return Result;
		}
		UKnightSkillDefinition* Copy = DuplicateObject<UKnightSkillDefinition>(
			const_cast<UKnightSkillDefinition*>(CastChecked<UKnightSkillDefinition>(SkillDefinition)), this);
		if (!IsValid(Copy))
		{
			return FKnightResult::Failure(EKnightError::LoadFailed, TEXT("RPGSkillGraph"),
				LOCTEXT("SkillCopy", "A skill node could not be retained as an immutable runtime definition."));
		}
		Copy->SetFlags(RF_Transient);
		Candidate.Add(Id, Copy);
		Graph.Add(Copy);
		for (const FKnightMaterialCost& Cost : Copy->GetMaterialCosts())
		{
			const UKnightDefinition* Material = nullptr;
			Result = ResolveDefinition(Cost.DefinitionId, UKnightItemDefinition::StaticClass(), Material);
			if (!Result.IsSuccess())
			{
				return Result;
			}
			const UKnightItemDefinition* Item = CastChecked<UKnightItemDefinition>(Material);
			if (!Item->ValidateItem().IsSuccess() || Item->GetItemKind() != EKnightItemKind::Material)
			{
				return FKnightResult::Failure(EKnightError::TypeMismatch, TEXT("RPGSkillCost"),
					LOCTEXT("MaterialType", "Skill costs require actual validated Material item definitions."));
			}
		}
	}
	Result = Tree->ValidateGraph(Graph);
	if (Result.IsSuccess())
	{
		Skills = MoveTemp(Candidate);
	}
	return Result;
}

void UKnightRPGComponent::BindInventory(UKnightInventoryComponent* InInventory)
{
	if (Inventory != nullptr && InventoryChangedHandle.IsValid())
	{
		Inventory->OnSnapshotChanged.Remove(InventoryChangedHandle);
	}
	Inventory = InInventory;
	InventoryChangedHandle = Inventory->OnSnapshotChanged.AddUObject(this, &UKnightRPGComponent::HandleInventoryChanged);
}

void UKnightRPGComponent::HandleInventoryChanged()
{
	if (GetOwner() != nullptr && !GetOwner()->HasAuthority())
	{
		HandleReplicatedStateReady();
	}
}

void UKnightRPGComponent::OnRegister()
{
	Super::OnRegister();
	ReplicatedState.Owner = this;
}

void UKnightRPGComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bEndingPlay = true;
	if (Inventory != nullptr && InventoryChangedHandle.IsValid())
	{
		Inventory->OnSnapshotChanged.Remove(InventoryChangedHandle);
		InventoryChangedHandle.Reset();
	}
	ReplicatedState.Owner.Reset();
	Pickups.Reset();
	Registrations.Reset();
	Super::EndPlay(EndPlayReason);
}

void UKnightRPGComponent::SortView(FKnightRPGView& View)
{
	View.Knights.Sort([](const FKnightRPGKnightView& Left, const FKnightRPGKnightView& Right)
		{
			return Left.KnightId.Value < Right.KnightId.Value;
		});
	for (FKnightRPGKnightView& Knight : View.Knights)
	{
		Knight.Assignments.Sort([](const FKnightEquipmentAssignment& Left, const FKnightEquipmentAssignment& Right)
			{
				return Left.SlotTag.ToString() < Right.SlotTag.ToString();
			});
		Knight.PurchasedSkills.Sort([](const FKnightPurchasedSkill& Left, const FKnightPurchasedSkill& Right)
			{
				return Left.DefinitionId.ToString() < Right.DefinitionId.ToString();
			});
	}
}

#undef LOCTEXT_NAMESPACE
