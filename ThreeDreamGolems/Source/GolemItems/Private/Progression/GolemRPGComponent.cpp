#include "Progression/GolemRPGComponent.h"

#include "Assets/GolemAssetManager.h"
#include "Assets/GolemDefinition.h"
#include "Catalog/GolemGearCatalog.h"
#include "Components/GolemAbilitySystemComponent.h"
#include "Components/GolemLifeComponent.h"
#include "Definitions/GolemItemDefinition.h"
#include "Definitions/GolemLootTableDefinition.h"
#include "Definitions/GolemSkillDefinition.h"
#include "Definitions/GolemSkillTreeDefinition.h"
#include "Inventory/GolemInventoryComponent.h"
#include "Events/GolemEventSubsystem.h"
#include "Events/GolemSkillLabEvent.h"
#include "Skills/GolemSkillRuntimeComponent.h"
#include "Loot/GolemItemPickup.h"
#include "Engine/World.h"
#include "Misc/ScopeExit.h"
#include "Net/UnrealNetwork.h"
#include "UObject/UObjectGlobals.h"

#define LOCTEXT_NAMESPACE "GolemRPGComponent"

bool FGolemRPGArray::NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParams)
{
	return FastArrayDeltaSerialize<FGolemRPGRecord, FGolemRPGArray>(Records, DeltaParams, *this);
}

void FGolemRPGArray::PostReplicatedReceive(const FFastArraySerializer::FPostReplicatedReceiveParameters& Parameters)
{
	if (UGolemRPGComponent* Component = Owner.Get())
	{
		Component->HandleReplicatedStateReady();
	}
}

UGolemRPGComponent::UGolemRPGComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UGolemRPGComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UGolemRPGComponent, ReplicatedState);
}

FGolemResult UGolemRPGComponent::InitializeRPG(UGolemInventoryComponent* InInventory,
	const FPrimaryAssetId& InSkillTreeId, TConstArrayView<FGolemRPGGolemRegistration> InRegistrations, int32 InitialPoints)
{
	const FGolemResult Authority = ValidateAuthority();
	if (!Authority.IsSuccess())
	{
		return Authority;
	}
	if (IsInitialized() || bTransactionInProgress || !IsValid(InInventory) || InInventory->GetOwner() != GetOwner()
		|| !InInventory->IsInitialized() || InInventory->IsReserved()
		|| InitialPoints < 0 || InitialPoints > GolemRPG::MaximumSkillPoints)
	{
		return FGolemResult::Failure(EGolemError::InvalidState, TEXT("RPGInitialize"),
			LOCTEXT("InitializeState", "RPG initialization requires a fresh coordinator and its initialized unreserved sibling inventory."));
	}
	const FGolemResult RegistrationResult = ValidateRegistrations(InRegistrations);
	if (!RegistrationResult.IsSuccess())
	{
		return RegistrationResult;
	}
	const FGolemResult GraphResult = ResolveSkills(InSkillTreeId);
	if (!GraphResult.IsSuccess())
	{
		return GraphResult;
	}
	FGolemInventorySnapshot Baseline;
	const FGolemResult CaptureResult = InInventory->CaptureSnapshot(Baseline);
	if (!CaptureResult.IsSuccess())
	{
		return CaptureResult;
	}
	FGolemRPGView Candidate;
	Candidate.Revision = 1;
	Candidate.InventoryId = Baseline.InventoryId;
	Candidate.InventoryRevision = Baseline.Revision;
	Candidate.SkillTreeId = InSkillTreeId;
	Candidate.bGearDemo = GearDemoRunId.IsValid();
	Candidate.DemoRunId = GearDemoRunId;
	Candidate.Currency = Candidate.bGearDemo ? GearInitialCurrency : 0;
	for (const FGolemRPGGolemRegistration& Registration : InRegistrations)
	{
		FGolemRPGGolemView Golem;
		Golem.GolemId = Registration.GolemId;
		Golem.HeroDefinitionId = Registration.HeroDefinitionId;
		Golem.Revision = 1;
		Golem.SkillPoints = InitialPoints;
		Golem.bGearEnabled = Candidate.bGearDemo;
		Golem.Level = Candidate.bGearDemo ? GearInitialLevel : 1;
		if (Golem.bGearEnabled)
		{
			Golem.ConsumableBindings = { FGolemGearCatalog::DefinitionId(TEXT("C01")),
				FGolemGearCatalog::DefinitionId(TEXT("C02")) };
		}
		if (!InitialSkillLabBuilds.IsEmpty())
		{
			const FGolemSkillBuild* Seed = InitialSkillLabBuilds.Find(Registration.HeroDefinitionId);
			if (Seed == nullptr)
			{
				return FGolemResult::Failure(EGolemError::InvalidId, TEXT("SkillLabSeed"),
					LOCTEXT("SeedHero", "Each canonical registration must match its exact configured SkillLab hero."));
			}
			Golem.Build = *Seed;
		}
		Candidate.Golems.Add(Golem);
		Registrations.Add(Registration.GolemId, Registration);
	}
	SortView(Candidate);
	FGolemInventoryView SeedView = InInventory->GetView();
	FGolemInventoryRequest SeedRequest;
	if (Candidate.bGearDemo)
	{
		const FGolemResult SeedResult = SeedGearAssignments(Candidate, SeedView, SeedRequest);
		if (!SeedResult.IsSuccess())
		{
			Registrations.Reset();
			return SeedResult;
		}
		SortView(Candidate);
	}
	TArray<FGolemMutationFence> Fences;
	TArray<FGolemProjectionCapability> Capabilities;
	TArray<UGolemAbilitySystemComponent*> ASCs;
	FGolemInventoryCapability SeedInventoryCapability;
	bool bCommitted = false;
	bTransactionInProgress = true;
	ON_SCOPE_EXIT
	{
		for (int32 Index = ASCs.Num() - 1; Index >= 0; --Index)
		{
			if (IsValid(ASCs[Index]) && Fences[Index].IsValid())
			{
				if (Capabilities[Index].IsValid())
				{
					if (bCommitted)
					{
						ASCs[Index]->PublishSourceProjection(Fences[Index], Capabilities[Index], false, GetOwner());
					}
					else
					{
						ASCs[Index]->AbortSourceProjection(Fences[Index], Capabilities[Index]);
					}
				}
				ASCs[Index]->EndMutationFence(Fences[Index]);
			}
		}
		if (!bCommitted)
		{
			Registrations.Reset();
		}
		if (SeedInventoryCapability.IsValid() && IsValid(InInventory))
		{
			if (bCommitted)
			{
				InInventory->ResetPrepared();
			}
			else
			{
				InInventory->AbortPrepared(SeedInventoryCapability);
			}
		}
		bTransactionInProgress = false;
	};
	if (!InitialSkillLabBuilds.IsEmpty() || Candidate.bGearDemo)
	{
		const FGolemResult Reservation = SeedRequest.Mutations.IsEmpty()
			? InInventory->ReserveRead(Baseline.InventoryId, Baseline.Revision, SeedInventoryCapability)
			: InInventory->PrepareCoordinatorTransaction(SeedRequest, SeedInventoryCapability);
		if (!Reservation.IsSuccess())
		{
			return Reservation;
		}
		const FGolemResult SeedPrepared = InInventory->GetPreparedView(SeedInventoryCapability, SeedView);
		if (!SeedPrepared.IsSuccess())
		{
			return SeedPrepared;
		}
		Candidate.InventoryRevision = SeedView.Revision;
		for (const FGolemRPGGolemRegistration& Registration : InRegistrations)
		{
			UGolemAbilitySystemComponent* ASC = Registration.AbilitySystem.Get();
			ASCs.Add(ASC);
			Fences.AddDefaulted();
			Capabilities.AddDefaulted();
			FGolemRPGGolemView* Golem = Candidate.Golems.FindByPredicate(
				[&Registration](const FGolemRPGGolemView& Entry)
				{
					return Entry.GolemId == Registration.GolemId;
				});
			FGolemGASProjectionPlan Plan;
			FGolemResult Result = BuildProjection(*Golem, SeedView.Items, Plan);
			if (Result.IsSuccess())
			{
				Result = ASC->BeginMutationFence(FGuid::NewGuid(), Fences.Last());
			}
			if (Result.IsSuccess())
			{
				Result = ASC->PrepareSourceProjection(Fences.Last(), Plan, 0, Capabilities.Last());
			}
			if (Result.IsSuccess())
			{
				Result = ASC->StageSourceProjection(Fences.Last(), Capabilities.Last());
			}
			if (!Result.IsSuccess())
			{
				return Result;
			}
		}
		FGolemResult Revalidation = ValidateAuthority();
		if (Revalidation.IsSuccess())
		{
			Revalidation = ValidateRegistrations(InRegistrations, &Candidate, true);
		}
		if (!Revalidation.IsSuccess())
		{
			return Revalidation;
		}
		for (int32 Index = 0; Index < ASCs.Num(); ++Index)
		{
			const FGolemResult Result = ASCs[Index]->ValidateStagedSourceProjection(Fences[Index], Capabilities[Index]);
			if (!Result.IsSuccess())
			{
				return Result;
			}
		}
		const FGolemResult InventoryCommit = InInventory->CommitPrepared(SeedInventoryCapability);
		if (!InventoryCommit.IsSuccess())
		{
			return InventoryCommit;
		}
		for (int32 Index = 0; Index < ASCs.Num(); ++Index)
		{
			const FGolemResult Result = ASCs[Index]->CommitSourceProjection(Fences[Index], Capabilities[Index]);
			if (!Result.IsSuccess())
			{
				return Result;
			}
		}
	}
	InitialSkillPoints = InitialPoints;
	if (Candidate.bGearDemo)
	{
		const FGolemResult GearCapture = InInventory->CaptureSnapshot(Baseline);
		if (!GearCapture.IsSuccess())
		{
			return GearCapture;
		}
	}
	InitialInventory = MoveTemp(Baseline);
	InitialView = Candidate;
	PublishedView = MoveTemp(Candidate);
	BindInventory(InInventory);
	InInventory->AssignedInstanceIds.Reset();
	for (const FGolemItemInstance& Item : SeedView.Items)
	{
		if (Item.LocationId != GolemGear::InventoryLocation)
		{
			InInventory->AssignedInstanceIds.Add(Item.InstanceId);
		}
	}
	bCommitted = true;
	RefreshReplicatedState();
	for (int32 Index = 0; Index < ASCs.Num(); ++Index)
	{
		ASCs[Index]->PublishSourceProjection(Fences[Index], Capabilities[Index], true, GetOwner());
		if (!ValidateAuthority().IsSuccess()
			|| !ValidateRegistrations(InRegistrations, &PublishedView, true, false).IsSuccess())
		{
			return FGolemResult::Success();
		}
	}
	if (SeedInventoryCapability.IsValid())
	{
		InInventory->PublishPrepared(SeedInventoryCapability, false);
		if (!ValidateAuthority().IsSuccess())
		{
			return FGolemResult::Success();
		}
	}
	OnSnapshotChanged.Broadcast();
	return FGolemResult::Success();
}

FGolemResult UGolemRPGComponent::BindReplicatedInventory(UGolemInventoryComponent* InInventory)
{
	if (!IsValid(InInventory) || InInventory->GetOwner() != GetOwner() || bEndingPlay
		|| (Inventory != nullptr && Inventory != InInventory))
	{
		return FGolemResult::Failure(EGolemError::InvalidState, TEXT("RPGBindInventory"),
			LOCTEXT("BindInventory", "The replicated read model requires its own live inventory sibling."));
	}
	BindInventory(InInventory);
	HandleReplicatedStateReady();
	return FGolemResult::Success();
}

FGolemResult UGolemRPGComponent::InitializeFixtureCatalog(TConstArrayView<UGolemDefinition*> Definitions)
{
	const FGolemResult Authority = ValidateAuthority();
	if (!Authority.IsSuccess())
	{
		return Authority;
	}
	if (IsInitialized() || bHasFixtureCatalog || bTransactionInProgress || Definitions.IsEmpty()
		|| Definitions.Num() > GolemItems::MaximumInventoryCapacity)
	{
		return FGolemResult::Failure(EGolemError::InvalidState, TEXT("RPGFixtureCatalog"),
			LOCTEXT("FixtureState", "An immutable actual-definition fixture catalog is configured once before initialization."));
	}
	TMap<FPrimaryAssetId, TObjectPtr<UGolemDefinition>> Copies;
	for (UGolemDefinition* Definition : Definitions)
	{
		if (!IsValid(Definition) || !Definition->ValidateIdentity().IsSuccess()
			|| Copies.Contains(Definition->GetPrimaryAssetId()))
		{
			return FGolemResult::Failure(EGolemError::InvalidData, TEXT("RPGFixtureCatalog"),
				LOCTEXT("FixtureIdentity", "Fixture entries must be actual definitions with unique validated stable identities."));
		}
		FGolemResult Validation = FGolemResult::Success();
		if (const UGolemItemDefinition* Item = Cast<UGolemItemDefinition>(Definition))
		{
			Validation = Item->ValidateItem();
		}
		else if (const UGolemSkillDefinition* Skill = Cast<UGolemSkillDefinition>(Definition))
		{
			Validation = Skill->ValidateSkill();
		}
		else if (const UGolemSkillTreeDefinition* Tree = Cast<UGolemSkillTreeDefinition>(Definition))
		{
			Validation = Tree->ValidateSkillTree();
		}
		else if (const UGolemLootTableDefinition* Loot = Cast<UGolemLootTableDefinition>(Definition))
		{
			Validation = Loot->ValidateLootTable();
		}
		if (!Validation.IsSuccess())
		{
			return Validation;
		}
		UGolemDefinition* Copy = DuplicateObject<UGolemDefinition>(Definition, this);
		if (!IsValid(Copy) || Copy->GetPrimaryAssetId() != Definition->GetPrimaryAssetId())
		{
			return FGolemResult::Failure(EGolemError::LoadFailed, TEXT("RPGFixtureCatalog"),
				LOCTEXT("FixtureCopy", "A validated definition could not be copied into the isolated fixture catalog."));
		}
		Copy->SetFlags(RF_Transient);
		Copies.Add(Copy->GetPrimaryAssetId(), Copy);
	}
	FixtureDefinitions = MoveTemp(Copies);
	bHasFixtureCatalog = true;
	return FGolemResult::Success();
}

FGolemRPGCommandResult UGolemRPGComponent::QueryCommittedCommand(const FGolemRPGCallerContext& InCaller,
	const FGolemRPGCommand& InCommand) const
{
	const FGolemRPGCallerContext Caller = InCaller;
	const FGolemRPGCommand Command = InCommand;
	FGolemRPGCommandResult Outcome;
	Outcome.Operation = Command.Operation;
	Outcome.RequestId = Command.RequestId;
	Outcome.GolemId = Command.ExpectedGolemId;
	Outcome.InventoryId = Command.InventoryId;
	Outcome.Result = ValidateAuthority();
	if (!Outcome.Result.IsSuccess())
	{
		return Outcome;
	}
	if (bTransactionInProgress)
	{
		Outcome.Result = FGolemResult::Failure(EGolemError::Conflict, TEXT("RPGTransaction"),
			LOCTEXT("Reentry", "An atomic RPG transaction is already preparing, committing or publishing."));
		return Outcome;
	}
	Outcome.Result = ValidateCaller(Caller);
	if (!Outcome.Result.IsSuccess())
	{
		return Outcome;
	}
	if (ItemizationMigration.IsPresent() && Command.RequestId == ItemizationMigration.MigrationId)
	{
		Outcome.Result = FGolemResult::Failure(EGolemError::Conflict, TEXT("MigrationRequestId"),
			LOCTEXT("MigrationRequestId", "This request identity belongs to the retained administrative migration boundary."));
		return Outcome;
	}
	if (const int32* Index = ReceiptIndices.Find(Command.RequestId))
	{
		const FGolemRPGReceipt Receipt = Receipts[*Index];
		if (Receipt.HumanId != Caller.HumanId || !(Receipt.Command == Command))
		{
			Outcome.Result = FGolemResult::Failure(EGolemError::Conflict, TEXT("RPGReplay"),
				LOCTEXT("ReplayIdentity", "This request already committed for another human or an altered exact payload."));
			return Outcome;
		}
		Outcome.Result = FGolemResult::Success();
		Outcome.CommittedInventoryRevision = Receipt.CommittedInventoryRevision;
		Outcome.CommittedGolemRevision = Receipt.CommittedGolem.Revision;
		Outcome.AffectedInstanceId = Receipt.AffectedInstanceId;
		Outcome.bWasReplay = true;
		return Outcome;
	}
	Outcome.Result = FGolemResult::Failure(EGolemError::NotFound, TEXT("RPGReceipt"),
		LOCTEXT("UncommittedRequest", "This request has no committed receipt; fresh command validation is required."));
	return Outcome;
}

FGolemRPGCommandResult UGolemRPGComponent::ExecuteCommand(const FGolemRPGCallerContext& InCaller,
	const FGolemRPGCommand& InCommand)
{
	const FGolemRPGCallerContext Caller = InCaller;
	const FGolemRPGCommand Command = InCommand;
	FGolemRPGCommandResult Outcome = QueryCommittedCommand(Caller, Command);
	if (Outcome.Result.Error != EGolemError::NotFound)
	{
		return Outcome;
	}
	Outcome.Result = Command.ValidateShape();
	if (!Outcome.Result.IsSuccess())
	{
		return Outcome;
	}
	if (Caller.GolemId != Command.ExpectedGolemId)
	{
		Outcome.Result = FGolemResult::Failure(EGolemError::NotOwned, TEXT("RPGCaller"),
			LOCTEXT("WrongGolem", "A fresh RPG command must target the caller's current canonical Golem."));
		return Outcome;
	}
	const FGolemRPGGolemView GolemBefore = GetGolemView(Caller.GolemId);
	const FGolemInventoryView InventoryBefore = Inventory->GetView();
	if (Command.InventoryId != InventoryBefore.InventoryId || Command.ExpectedInventoryRevision != InventoryBefore.Revision
		|| Command.ExpectedGolemRevision != GolemBefore.Revision)
	{
		Outcome.Result = FGolemResult::Failure(EGolemError::InvalidRevision, TEXT("RPGRevision"),
			LOCTEXT("Stale", "The inventory or Golem revision changed before this new command."));
		return Outcome;
	}
	if (Receipts.Num() >= GolemItems::MaximumTransactionReceipts || PublishedView.Revision >= MAX_int64)
	{
		Outcome.Result = FGolemResult::Failure(EGolemError::CapacityExceeded, TEXT("RPGJournal"),
			LOCTEXT("JournalFull", "The finite RPG journal cannot discard earlier successful replay protection."));
		return Outcome;
	}
	UGolemAbilitySystemComponent* ASC = Caller.AbilitySystem.Get();
	UGolemInventoryComponent* TransactionInventory = Inventory;
	const TWeakObjectPtr<AActor> PublicationOwner(GetOwner());
	if (ASC->GetLifeComponent() == nullptr || !ASC->GetLifeComponent()->IsAlive()
		|| (Command.Operation != EGolemRPGOperation::Pickup && Command.Operation != EGolemRPGOperation::UseConsumable
			&& (!Caller.bInSafeZone || Caller.bInCombat)))
	{
		Outcome.Result = FGolemResult::Failure(EGolemError::InvalidState, TEXT("RPGSafeZone"),
			LOCTEXT("SafeZone", "Loadout and skill changes require a living Golem in the authoritative safe zone."));
		return Outcome;
	}
	if (PublishedView.IsItemizationEnabled() && Command.Operation != EGolemRPGOperation::Pickup)
	{
		return ExecuteGearCommand(Caller, Command);
	}
	const UGolemSkillRuntimeComponent* SkillRuntime = Caller.Pawn->FindComponentByClass<UGolemSkillRuntimeComponent>();
	if (Command.Operation != EGolemRPGOperation::Pickup && SkillRuntime != nullptr && SkillRuntime->HasDeferredDamage())
	{
		Outcome.Result = FGolemResult::Failure(EGolemError::InvalidState, TEXT("SkillBuildDebt"),
			LOCTEXT("DeferredDebt", "Resolve all deferred damage before changing equipment, talents, specialization or runes."));
		return Outcome;
	}
	bTransactionInProgress = true;
	FGolemInventoryCapability InventoryCapability;
	FGolemMutationFence Fence;
	FGolemProjectionCapability ProjectionCapability;
	AGolemItemPickup* Pickup = nullptr;
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
	FGolemItemInstance PickedItem;
	if (Command.Operation == EGolemRPGOperation::Pickup)
	{
		const TWeakObjectPtr<AGolemItemPickup>* Registered = Pickups.Find(Command.PickupId);
		Pickup = Registered == nullptr ? nullptr : Registered->Get();
		if (!IsValid(Pickup) || Pickup->GetWorld() != GetWorld() || Pickup->GetPickupId() != Command.PickupId)
		{
			Outcome.Result = FGolemResult::Failure(EGolemError::NotFound, TEXT("RPGPickup"),
				LOCTEXT("PickupMissing", "The exact registered live pickup is unavailable."));
			return Outcome;
		}
		if (Receipts.ContainsByPredicate([&Command](const FGolemRPGReceipt& Receipt)
			{
				return Receipt.Command.Operation == EGolemRPGOperation::Pickup
					&& Receipt.Command.PickupId == Command.PickupId;
			}))
		{
			Outcome.Result = FGolemResult::Failure(EGolemError::Conflict, TEXT("RPGPickup"),
				LOCTEXT("DurablePickupClaim", "This pickup identity already has a durable successful claim."));
			return Outcome;
		}
		AActor* Pawn = Caller.Pawn.Get();
		if (FVector::DistSquared(Pawn->GetActorLocation(), Pickup->GetActorLocation())
			> FMath::Square(GolemRPG::PickupRange))
		{
			Outcome.Result = FGolemResult::Failure(EGolemError::OutOfRange, TEXT("RPGPickup"),
				LOCTEXT("PickupRange", "Move within 300 centimeters of the pickup."));
			return Outcome;
		}
		FCollisionQueryParams Query(SCENE_QUERY_STAT(GolemPickupVisibility), false, Pawn);
		Query.AddIgnoredActor(Pickup);
		FHitResult Hit;
		if (GetWorld()->LineTraceSingleByChannel(Hit, Pawn->GetActorLocation(), Pickup->GetActorLocation(),
			ECC_Visibility, Query))
		{
			Outcome.Result = FGolemResult::Failure(EGolemError::OutOfRange, TEXT("RPGPickup"),
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
	FGolemRPGView Candidate;
	FGolemInventoryRequest InventoryRequest;
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
	FGolemInventoryView PreparedInventory;
	Outcome.Result = Inventory->GetPreparedView(InventoryCapability, PreparedInventory);
	if (!Outcome.Result.IsSuccess())
	{
		return Outcome;
	}
	FGolemRPGGolemView* CandidateGolem = Candidate.Golems.FindByPredicate([&Caller](const FGolemRPGGolemView& Golem)
		{
			return Golem.GolemId == Caller.GolemId;
		});
	FGolemGASProjectionPlan Plan;
	Outcome.Result = BuildProjection(*CandidateGolem, PreparedInventory.Items, Plan);
	if (Outcome.Result.IsSuccess())
	{
		Outcome.Result = ValidateBuildAttachments(*CandidateGolem, Candidate, PreparedInventory.Items);
	}
	if (!Outcome.Result.IsSuccess())
	{
		return Outcome;
	}
	if (Command.Operation != EGolemRPGOperation::Pickup)
	{
		Outcome.Result = ASC->BeginMutationFence(Command.RequestId, Fence);
		if (Outcome.Result.IsSuccess())
		{
			Outcome.Result = ASC->PrepareSourceProjection(Fence, Plan,
				ASC->GetSourceProjection(GolemRPG::ProjectionSource).Revision, ProjectionCapability);
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
		FGolemInventoryView RevalidatedInventory;
		if (Outcome.Result.IsSuccess())
		{
			Outcome.Result = TransactionInventory->GetPreparedView(InventoryCapability, RevalidatedInventory);
		}
		if (Outcome.Result.IsSuccess() && !(RevalidatedInventory == PreparedInventory))
		{
			Outcome.Result = FGolemResult::Failure(EGolemError::Conflict, TEXT("RPGStage"),
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
	FGolemRPGReceipt Receipt;
	Receipt.HumanId = Caller.HumanId;
	Receipt.Command = Command;
	Receipt.InventoryRequest = InventoryRequest;
	Receipt.PickedItem = PickedItem;
	Receipt.CommittedGolem = *CandidateGolem;
	if (Candidate.IsItemizationEnabled())
	{
		Receipt.CommittedGolems.Add(*CandidateGolem);
		Receipt.CommittedCurrency = Candidate.Currency;
	}
	Receipt.CommittedInventoryRevision = PreparedInventory.Revision;
	Receipt.AffectedInstanceId = Outcome.AffectedInstanceId;
	Outcome.CommittedInventoryRevision = Receipt.CommittedInventoryRevision;
	Outcome.CommittedGolemRevision = Receipt.CommittedGolem.Revision;
	ReceiptIndices.Add(Command.RequestId, Receipts.Num());
	Receipts.Add(MoveTemp(Receipt));
	PublishedView = MoveTemp(Candidate);
	Inventory->AssignedInstanceIds.Reset();
	for (const FGolemRPGGolemView& Golem : PublishedView.Golems)
	{
		if (Golem.WeaponRuneInstanceId.IsValid())
		{
			Inventory->AssignedInstanceIds.Add(Golem.WeaponRuneInstanceId);
		}
		if (Golem.SkillRuneInstanceId.IsValid())
		{
			Inventory->AssignedInstanceIds.Add(Golem.SkillRuneInstanceId);
		}
		for (const FGolemEquipmentAssignment& Assignment : Golem.Assignments)
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
	if (!CanPublish())
	{
		return Outcome;
	}
	if (Command.Operation == EGolemRPGOperation::ApplyBuild
		|| Command.Operation == EGolemRPGOperation::ApplyTalentLoadout)
	{
		const FGolemRPGGolemView Confirmed = GetGolemView(Caller.GolemId);
		UGolemEventSubsystem* Bus = GetWorld()->GetSubsystem<UGolemEventSubsystem>();
		if (Bus != nullptr)
		{
			auto Publish = [Bus, &Caller, &Command, &Outcome](FName EventName, FName SubjectId)
			{
				FGolemSkillLabEvent Event;
				Event.ConfirmationId = FGuid::NewGuid();
				Event.Revision = Outcome.CommittedGolemRevision;
				Event.SourceId = Caller.GolemId;
				Event.TargetId = Caller.GolemId;
				Event.EncounterId = Caller.EncounterId;
				Event.RequestId = Command.RequestId;
				Event.EventName = EventName;
				Event.SubjectId = SubjectId;
				Bus->PublishConfirmed(Event);
			};
			Publish(TEXT("LoadoutApplied"), Confirmed.Build.SpecId);
			if (CanPublish() && GolemBefore.Build.SpecId != Confirmed.Build.SpecId)
			{
				Publish(TEXT("SpecChanged"), Confirmed.Build.SpecId);
			}
			if (CanPublish() && (GolemBefore.WeaponRuneInstanceId != Confirmed.WeaponRuneInstanceId))
			{
				Publish(TEXT("RuneAttached"), Confirmed.Build.WeaponRune);
			}
			if (CanPublish() && (GolemBefore.SkillRuneInstanceId != Confirmed.SkillRuneInstanceId))
			{
				Publish(TEXT("RuneAttached"), Confirmed.Build.SkillRune);
			}
		}
	}
	return Outcome;
}

FGolemResult UGolemRPGComponent::RegisterPickup(AGolemItemPickup* Pickup)
{
	const FGolemResult Authority = ValidateAuthority();
	if (!Authority.IsSuccess())
	{
		return Authority;
	}
	if (!IsInitialized() || bTransactionInProgress || !IsValid(Pickup) || Pickup->GetWorld() != GetWorld()
		|| !Pickup->HasAuthority() || !Pickup->GetPickupId().IsValid() || Pickup->IsClaimed()
		|| !Pickup->GetView().Item.ValidateIdentityAndCount().IsSuccess())
	{
		return FGolemResult::Failure(EGolemError::InvalidState, TEXT("RPGPickupRegister"),
			LOCTEXT("RegisterPickup", "Register a live initialized authoritative pickup in this coordinator's world."));
	}
	const FGuid PickupId = Pickup->GetPickupId();
	if (Receipts.ContainsByPredicate([&PickupId](const FGolemRPGReceipt& Receipt)
		{
			return Receipt.Command.Operation == EGolemRPGOperation::Pickup && Receipt.Command.PickupId == PickupId;
		}))
	{
		return FGolemResult::Failure(EGolemError::Conflict, TEXT("RPGPickupRegister"),
			LOCTEXT("RegisterClaimedIdentity", "A durably claimed pickup identity cannot register a fresh unclaimed actor."));
	}
	if (const TWeakObjectPtr<AGolemItemPickup>* Existing = Pickups.Find(Pickup->GetPickupId()))
	{
		if (Existing->Get() == Pickup)
		{
			return FGolemResult::Success();
		}
		return FGolemResult::Failure(EGolemError::DuplicateId, TEXT("RPGPickupRegister"),
			LOCTEXT("DuplicatePickup", "A pickup identity cannot be registered for another actor."));
	}
	Pickups.Add(Pickup->GetPickupId(), Pickup);
	return FGolemResult::Success();
}

FGolemResult UGolemRPGComponent::UnregisterUnclaimedPickup(AGolemItemPickup* Pickup)
{
	const FGolemResult Authority = ValidateAuthority();
	if (!Authority.IsSuccess())
	{
		return Authority;
	}
	if (bTransactionInProgress)
	{
		return FGolemResult::Failure(EGolemError::Conflict, TEXT("RPGPickupUnregister"),
			LOCTEXT("UnregisterReserved", "A pickup cannot be withdrawn during an RPG transaction."));
	}
	if (!IsInitialized() || !IsValid(Pickup) || Pickup->IsActorBeingDestroyed() || !Pickup->HasAuthority()
		|| Pickup->GetWorld() != GetWorld() || !Pickup->GetPickupId().IsValid())
	{
		return FGolemResult::Failure(EGolemError::InvalidState, TEXT("RPGPickupUnregister"),
			LOCTEXT("UnregisterLifetime", "Withdraw only a live authoritative pickup from this coordinator's world."));
	}
	const FGuid PickupId = Pickup->GetPickupId();
	const TWeakObjectPtr<AGolemItemPickup>* Registered = Pickups.Find(PickupId);
	if (Registered == nullptr || Registered->Get() != Pickup)
	{
		return FGolemResult::Failure(EGolemError::NotOwned, TEXT("RPGPickupUnregister"),
			LOCTEXT("UnregisterExactActor", "Only the exact registered pickup actor owns this withdrawal."));
	}
	if (Pickup->IsClaimed() || Pickup->ReservedToken.IsValid() || Receipts.ContainsByPredicate(
		[&PickupId](const FGolemRPGReceipt& Receipt)
		{
			return Receipt.Command.Operation == EGolemRPGOperation::Pickup && Receipt.Command.PickupId == PickupId;
		}))
	{
		return FGolemResult::Failure(EGolemError::Conflict, TEXT("RPGPickupUnregister"),
			LOCTEXT("UnregisterClaim", "A reserved or durably claimed pickup identity cannot be withdrawn."));
	}
	Pickups.Remove(PickupId);
	return FGolemResult::Success();
}

FGolemRPGView UGolemRPGComponent::GetView() const
{
	return PublishedView;
}

FGolemRPGGolemView UGolemRPGComponent::GetGolemView(const FGolemEntityId& GolemId) const
{
	const FGolemRPGGolemView* Golem = PublishedView.Golems.FindByPredicate([&GolemId](const FGolemRPGGolemView& Entry)
		{
			return Entry.GolemId == GolemId;
		});
	return Golem == nullptr ? FGolemRPGGolemView() : *Golem;
}

bool UGolemRPGComponent::IsInitialized() const
{
	return PublishedView.IsInitialized();
}

bool UGolemRPGComponent::IsTransactionInProgress() const
{
	return bTransactionInProgress;
}

FGolemResult UGolemRPGComponent::ValidateAuthority() const
{
	const AActor* OwnerActor = GetOwner();
	const UWorld* World = GetWorld();
	if (!IsInGameThread() || bEndingPlay || !IsValid(OwnerActor) || OwnerActor->IsActorBeingDestroyed()
		|| World == nullptr || World->bIsTearingDown)
	{
		return FGolemResult::Failure(EGolemError::ShuttingDown, TEXT("RPGAuthority"),
			LOCTEXT("Lifetime", "The RPG owner or its world has ended its mutation lifetime."));
	}
	if (!OwnerActor->HasAuthority() || World->GetNetMode() == NM_Client)
	{
		return FGolemResult::Failure(EGolemError::NotAuthority, TEXT("RPGAuthority"),
			LOCTEXT("Authority", "Only the authoritative campaign server may execute RPG commands."));
	}
	return FGolemResult::Success();
}

FGolemResult UGolemRPGComponent::ValidateCaller(const FGolemRPGCallerContext& Caller) const
{
	const FGolemRPGGolemRegistration* Registration = Registrations.Find(Caller.GolemId);
	AActor* Pawn = Caller.Pawn.Get();
	UGolemAbilitySystemComponent* ASC = Caller.AbilitySystem.Get();
	if (!IsInitialized() || Inventory == nullptr || !Caller.HumanId.IsValid() || Registration == nullptr
		|| !IsValid(Pawn) || !IsValid(ASC) || Pawn->IsActorBeingDestroyed() || !Pawn->HasAuthority()
		|| Pawn->GetWorld() != GetWorld() || Registration->Pawn.Get() != Pawn
		|| Registration->AbilitySystem.Get() != ASC || Registration->HeroDefinitionId != Caller.HeroDefinitionId
		|| ASC->GetEntityId() != Caller.GolemId || ASC->GetOwnerActor() != Pawn || ASC->GetAvatarActor() != Pawn)
	{
		return FGolemResult::Failure(EGolemError::NotOwned, TEXT("RPGCaller"),
			LOCTEXT("Caller", "The trusted caller must match a live registered canonical Golem and its own initialized ASC."));
	}
	return FGolemResult::Success();
}

FGolemResult UGolemRPGComponent::ValidateRegistrations(TConstArrayView<FGolemRPGGolemRegistration> InRegistrations,
	const FGolemRPGView* ExpectedView, bool bAllowMutationFence, bool bRequireFreshSource) const
{
	if (InRegistrations.Num() != 3)
	{
		return FGolemResult::Failure(EGolemError::InvalidData, TEXT("RPGRegistrations"),
			LOCTEXT("CanonicalCount", "RPG composition requires exactly three canonical Golem registrations."));
	}
	TSet<FGolemEntityId> Ids;
	TSet<AActor*> Pawns;
	for (const FGolemRPGGolemRegistration& Registration : InRegistrations)
	{
		AActor* Pawn = Registration.Pawn.Get();
		UGolemAbilitySystemComponent* ASC = Registration.AbilitySystem.Get();
		if (!Registration.GolemId.IsValid() || !GolemIdentity::IsValidDefinitionId(Registration.HeroDefinitionId)
			|| Registration.HeroDefinitionId.PrimaryAssetType != FPrimaryAssetType(TEXT("GolemHero"))
			|| !IsValid(Pawn) || !IsValid(ASC) || Pawn->IsActorBeingDestroyed()
			|| !Pawn->HasAuthority() || Pawn->GetWorld() != GetWorld()
			|| ASC->GetEntityId() != Registration.GolemId || ASC->GetOwnerActor() != Pawn || ASC->GetAvatarActor() != Pawn
			|| (bRequireFreshSource && ASC->GetSourceProjection(GolemRPG::ProjectionSource).Revision != 0)
			|| (!bAllowMutationFence && ASC->IsMutationFenced())
			|| Ids.Contains(Registration.GolemId) || Pawns.Contains(Pawn))
		{
			return FGolemResult::Failure(EGolemError::InvalidData, TEXT("RPGRegistrations"),
				LOCTEXT("Registration", "Each canonical registration requires a distinct live initialized ASC and a fresh RPG source."));
		}
		if (ExpectedView != nullptr && !ExpectedView->Golems.ContainsByPredicate(
			[&Registration](const FGolemRPGGolemView& Golem)
			{
				return Golem.GolemId == Registration.GolemId && Golem.HeroDefinitionId == Registration.HeroDefinitionId;
			}))
		{
			return FGolemResult::Failure(EGolemError::InvalidId, TEXT("RPGRegistrations"),
				LOCTEXT("RestoreRegistration", "Reconstruction registrations must retain the snapshot's canonical Golem and hero identities."));
		}
		Ids.Add(Registration.GolemId);
		Pawns.Add(Pawn);
	}
	return FGolemResult::Success();
}

FGolemResult UGolemRPGComponent::ResolveDefinition(const FPrimaryAssetId& Id, UClass* ExpectedClass,
	const UGolemDefinition*& OutDefinition) const
{
	OutDefinition = nullptr;
	if (!GolemIdentity::IsValidDefinitionId(Id))
	{
		return FGolemResult::Failure(EGolemError::InvalidId, TEXT("RPGDefinition"),
			LOCTEXT("DefinitionId", "The requested definition identity is invalid."));
	}
	if (bHasFixtureCatalog)
	{
		const TObjectPtr<UGolemDefinition>* Definition = FixtureDefinitions.Find(Id);
		OutDefinition = Definition == nullptr ? nullptr : Definition->Get();
	}
	else
	{
		UGolemAssetManager* Manager = UGolemAssetManager::GetGolemAssetManager();
		if (Manager == nullptr)
		{
			return FGolemResult::Failure(EGolemError::LoadFailed, TEXT("RPGDefinition"),
				LOCTEXT("AssetManager", "The production typed definition catalog is unavailable."));
		}
		UGolemDefinition* Definition = nullptr;
		const FGolemResult Resolution = Manager->ResolveDefinition(Id, ExpectedClass, Definition);
		if (!Resolution.IsSuccess())
		{
			return Resolution;
		}
		OutDefinition = Definition;
	}
	if (!IsValid(OutDefinition) || OutDefinition->GetPrimaryAssetId() != Id)
	{
		return FGolemResult::Failure(EGolemError::NotFound, FName(*Id.ToString()),
			LOCTEXT("DefinitionMissing", "No actual definition resolves this stable identity."));
	}
	if (!OutDefinition->IsA(ExpectedClass))
	{
		OutDefinition = nullptr;
		return FGolemResult::Failure(EGolemError::TypeMismatch, FName(*Id.ToString()),
			LOCTEXT("DefinitionClass", "The actual definition does not have the required domain type."));
	}
	return OutDefinition->ValidateIdentity();
}

FGolemResult UGolemRPGComponent::ResolveSkills(const FPrimaryAssetId& TreeId)
{
	const UGolemDefinition* Definition = nullptr;
	FGolemResult Result = ResolveDefinition(TreeId, UGolemSkillTreeDefinition::StaticClass(), Definition);
	if (!Result.IsSuccess())
	{
		return Result;
	}
	const UGolemSkillTreeDefinition* Tree = CastChecked<UGolemSkillTreeDefinition>(Definition);
	Result = Tree->ValidateSkillTree();
	if (!Result.IsSuccess())
	{
		return Result;
	}
	TMap<FPrimaryAssetId, TObjectPtr<UGolemSkillDefinition>> Candidate;
	TArray<const UGolemSkillDefinition*> Graph;
	for (const FPrimaryAssetId& Id : Tree->GetSkills())
	{
		const UGolemDefinition* SkillDefinition = nullptr;
		Result = ResolveDefinition(Id, UGolemSkillDefinition::StaticClass(), SkillDefinition);
		if (!Result.IsSuccess())
		{
			return Result;
		}
		UGolemSkillDefinition* Copy = DuplicateObject<UGolemSkillDefinition>(
			const_cast<UGolemSkillDefinition*>(CastChecked<UGolemSkillDefinition>(SkillDefinition)), this);
		if (!IsValid(Copy))
		{
			return FGolemResult::Failure(EGolemError::LoadFailed, TEXT("RPGSkillGraph"),
				LOCTEXT("SkillCopy", "A skill node could not be retained as an immutable runtime definition."));
		}
		Copy->SetFlags(RF_Transient);
		Candidate.Add(Id, Copy);
		Graph.Add(Copy);
		for (const FGolemMaterialCost& Cost : Copy->GetMaterialCosts())
		{
			const UGolemDefinition* Material = nullptr;
			Result = ResolveDefinition(Cost.DefinitionId, UGolemItemDefinition::StaticClass(), Material);
			if (!Result.IsSuccess())
			{
				return Result;
			}
			const UGolemItemDefinition* Item = CastChecked<UGolemItemDefinition>(Material);
			if (!Item->ValidateItem().IsSuccess() || Item->GetItemKind() != EGolemItemKind::Material)
			{
				return FGolemResult::Failure(EGolemError::TypeMismatch, TEXT("RPGSkillCost"),
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

void UGolemRPGComponent::BindInventory(UGolemInventoryComponent* InInventory)
{
	if (Inventory != nullptr && InventoryChangedHandle.IsValid())
	{
		Inventory->OnSnapshotChanged.Remove(InventoryChangedHandle);
	}
	Inventory = InInventory;
	InventoryChangedHandle = Inventory->OnSnapshotChanged.AddUObject(this, &UGolemRPGComponent::HandleInventoryChanged);
}

void UGolemRPGComponent::HandleInventoryChanged()
{
	if (GetOwner() != nullptr && !GetOwner()->HasAuthority())
	{
		HandleReplicatedStateReady();
	}
}

void UGolemRPGComponent::OnRegister()
{
	Super::OnRegister();
	ReplicatedState.Owner = this;
}

void UGolemRPGComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
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

void UGolemRPGComponent::SortView(FGolemRPGView& View)
{
	View.Golems.Sort([](const FGolemRPGGolemView& Left, const FGolemRPGGolemView& Right)
		{
			return Left.GolemId.Value < Right.GolemId.Value;
		});
	for (FGolemRPGGolemView& Golem : View.Golems)
	{
		Golem.Assignments.Sort([](const FGolemEquipmentAssignment& Left, const FGolemEquipmentAssignment& Right)
			{
				return Left.SlotTag.ToString() < Right.SlotTag.ToString();
			});
		Golem.PurchasedSkills.Sort([](const FGolemPurchasedSkill& Left, const FGolemPurchasedSkill& Right)
			{
				return Left.DefinitionId.ToString() < Right.DefinitionId.ToString();
			});
		Golem.TalentLoadouts.Sort([](const FGolemTalentLoadout& Left, const FGolemTalentLoadout& Right)
			{
				return Left.LoadoutId < Right.LoadoutId;
			});
	}
}

#undef LOCTEXT_NAMESPACE
