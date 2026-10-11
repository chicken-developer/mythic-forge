#include "Progression/KnightRPGComponent.h"

#include "Components/KnightAbilitySystemComponent.h"
#include "Components/KnightLifeComponent.h"
#include "Definitions/KnightItemDefinition.h"
#include "Events/KnightEventSubsystem.h"
#include "Events/KnightItemEvent.h"
#include "Events/KnightSkillLabEvent.h"
#include "GameplayAbilitySpec.h"
#include "Inventory/KnightInventoryComponent.h"
#include "Skills/KnightSkillRuntimeComponent.h"
#include "Engine/World.h"
#include "Misc/ScopeExit.h"

#define LOCTEXT_NAMESPACE "KnightRPGGearTransactions"

FKnightRPGCommandResult UKnightRPGComponent::ExecuteGearCommand(const FKnightRPGCallerContext& Caller,
	const FKnightRPGCommand& Command)
{
	FKnightRPGCommandResult Outcome;
	Outcome.Operation = Command.Operation;
	Outcome.RequestId = Command.RequestId;
	Outcome.KnightId = Caller.KnightId;
	Outcome.InventoryId = Command.InventoryId;
	UKnightInventoryComponent* TransactionInventory = Inventory;
	const bool bConsumable = Command.Operation == EKnightRPGOperation::UseConsumable;
	const TWeakObjectPtr<AActor> PublicationOwner(GetOwner());
	const FKnightRPGView Before = PublishedView;
	const auto CanPublish = [this, &Caller]()
	{
		return ValidateAuthority().IsSuccess() && ValidateCaller(Caller).IsSuccess();
	};
	TArray<FKnightEntityId> Participants { Caller.KnightId };
	if (Command.Operation == EKnightRPGOperation::TransferEquipment)
	{
		if (!Caller.EditableKnights.Contains(Caller.KnightId)
			|| !Caller.EditableKnights.Contains(Command.TargetKnightId))
		{
			Outcome.Result = FKnightResult::Failure(EKnightError::NotOwned, TEXT("GearTransferOwnership"),
				LOCTEXT("BothOwners", "Transfer requires this human's authoritative permission for both canonical Knights."));
			return Outcome;
		}
		Participants.Add(Command.TargetKnightId);
	}
	if (Command.Operation == EKnightRPGOperation::ClaimReward)
	{
		Participants.Reset();
		for (const FKnightRPGKnightView& Knight : Before.Knights)
		{
			Participants.Add(Knight.KnightId);
		}
	}
	auto ValidateParticipants = [this, &Caller, &Participants, bConsumable]()
	{
		FKnightResult Result = ValidateAuthority();
		if (Result.IsSuccess())
		{
			Result = ValidateCaller(Caller);
		}
		if (!Result.IsSuccess())
		{
			return Result;
		}
		if (!bConsumable && (!Caller.bInSafeZone || Caller.bInCombat))
		{
			return FKnightResult::Failure(EKnightError::InvalidState, TEXT("GearSanctuary"),
				LOCTEXT("Sanctuary", "Equipment, vendor, rewards and build changes require the authoritative sanctuary."));
		}
		for (const FKnightEntityId& Id : Participants)
		{
			const FKnightRPGKnightRegistration* Registration = Registrations.Find(Id);
			UKnightAbilitySystemComponent* ASC = Registration == nullptr ? nullptr : Registration->AbilitySystem.Get();
			AActor* Pawn = Registration == nullptr ? nullptr : Registration->Pawn.Get();
			const UKnightSkillRuntimeComponent* Runtime = Pawn == nullptr
				? nullptr : Pawn->FindComponentByClass<UKnightSkillRuntimeComponent>();
			if (!IsValid(Pawn) || Pawn->IsActorBeingDestroyed() || !IsValid(ASC)
				|| ASC->GetLifeComponent() == nullptr || !ASC->GetLifeComponent()->IsAlive()
				|| (!bConsumable && (Runtime != nullptr && Runtime->HasDeferredDamage()
					|| ASC->GetActivatableAbilities().ContainsByPredicate([](const FGameplayAbilitySpec& Spec)
					{
						return Spec.IsActive();
					}))))
			{
				return FKnightResult::Failure(EKnightError::InvalidState, TEXT("GearParticipantState"),
					LOCTEXT("ParticipantState", "All affected Knights must be living and free of active actions or deferred damage."));
			}
		}
		return FKnightResult::Success();
	};
	Outcome.Result = ValidateParticipants();
	if (!Outcome.Result.IsSuccess())
	{
		return Outcome;
	}
	FKnightRPGView Candidate;
	FKnightInventoryRequest Request;
	Outcome.Result = BuildCandidate(Before, Inventory->GetView(), Command, nullptr, nullptr,
		Candidate, Request, Outcome.AffectedInstanceId);
	if (!Outcome.Result.IsSuccess())
	{
		return Outcome;
	}
	bTransactionInProgress = true;
	FKnightInventoryCapability InventoryCapability;
	TArray<UKnightAbilitySystemComponent*> ASCs;
	TArray<FKnightMutationFence> Fences;
	TArray<FKnightProjectionCapability> Capabilities;
	UKnightSkillRuntimeComponent* ConsumableRuntime = nullptr;
	bool bCommitted = false;
	ON_SCOPE_EXIT
	{
		if (!bCommitted && IsValid(ConsumableRuntime))
		{
			ConsumableRuntime->AbortPreparedConsumable();
		}
		for (int32 Index = ASCs.Num() - 1; Index >= 0; --Index)
		{
			if (!IsValid(ASCs[Index]) || !Fences[Index].IsValid())
			{
				continue;
			}
			if (Capabilities[Index].IsValid())
			{
				if (bCommitted)
				{
					ASCs[Index]->PublishSourceProjection(Fences[Index], Capabilities[Index], false, PublicationOwner.Get());
				}
				else
				{
					ASCs[Index]->AbortSourceProjection(Fences[Index], Capabilities[Index]);
				}
			}
			ASCs[Index]->EndMutationFence(Fences[Index]);
		}
		if (IsValid(TransactionInventory) && InventoryCapability.IsValid())
		{
			if (bCommitted)
			{
				TransactionInventory->ResetPrepared();
			}
			else
			{
				TransactionInventory->AbortPrepared(InventoryCapability);
			}
		}
		bTransactionInProgress = false;
	};
	Outcome.Result = Request.Mutations.IsEmpty()
		? Inventory->ReserveRead(Command.InventoryId, Command.ExpectedInventoryRevision, InventoryCapability)
		: Inventory->PrepareCoordinatorTransaction(Request, InventoryCapability);
	FKnightInventoryView PreparedInventory;
	if (Outcome.Result.IsSuccess())
	{
		Outcome.Result = Inventory->GetPreparedView(InventoryCapability, PreparedInventory);
	}
	if (!Outcome.Result.IsSuccess())
	{
		return Outcome;
	}
	for (FKnightRPGKnightView& Knight : Candidate.Knights)
	{
		const FKnightRPGKnightView* Previous = Before.Knights.FindByPredicate([&Knight](const FKnightRPGKnightView& Entry)
		{
			return Entry.KnightId == Knight.KnightId;
		});
		if (Previous->Revision == Knight.Revision)
		{
			continue;
		}
		FKnightGASProjectionPlan Plan;
		Outcome.Result = BuildProjection(Knight, PreparedInventory.Items, Plan);
		if (Outcome.Result.IsSuccess())
		{
			Outcome.Result = ValidateBuildAttachments(Knight, Candidate, PreparedInventory.Items);
		}
		if (!Outcome.Result.IsSuccess())
		{
			return Outcome;
		}
		UKnightAbilitySystemComponent* ASC = Registrations.FindChecked(Knight.KnightId).AbilitySystem.Get();
		ASCs.Add(ASC);
		Fences.AddDefaulted();
		Capabilities.AddDefaulted();
		Outcome.Result = ASC->BeginMutationFence(Command.RequestId, Fences.Last());
		if (Outcome.Result.IsSuccess() && !bConsumable)
		{
			Outcome.Result = ASC->PrepareSourceProjection(Fences.Last(), Plan,
				ASC->GetSourceProjection(KnightRPG::ProjectionSource).Revision, Capabilities.Last());
		}
		if (Outcome.Result.IsSuccess() && !bConsumable)
		{
			Outcome.Result = ASC->StageSourceProjection(Fences.Last(), Capabilities.Last());
		}
		if (!Outcome.Result.IsSuccess())
		{
			return Outcome;
		}
		if (bConsumable && Knight.KnightId == Caller.KnightId)
		{
			ConsumableRuntime = Caller.Pawn->FindComponentByClass<UKnightSkillRuntimeComponent>();
			const FKnightInventoryView CurrentInventory = Inventory->GetView();
			const FKnightItemInstance* Used = CurrentInventory.Items.FindByPredicate(
				[&Outcome](const FKnightItemInstance& Item) { return Item.InstanceId == Outcome.AffectedInstanceId; });
			const UKnightDefinition* Definition = nullptr;
			Outcome.Result = Used != nullptr ? ResolveDefinition(Used->DefinitionId,
				UKnightItemDefinition::StaticClass(), Definition)
				: FKnightResult::Failure(EKnightError::NotFound, TEXT("Consumable"), LOCTEXT("MissingUse", "The bound consumable is unavailable."));
			if (Outcome.Result.IsSuccess() && ConsumableRuntime != nullptr)
			{
				Outcome.Result = ConsumableRuntime->PrepareConsumable(
					CastChecked<UKnightItemDefinition>(Definition)->GetMetadata().SocketProfile, Fences.Last());
			}
			else if (ConsumableRuntime == nullptr)
			{
				Outcome.Result = FKnightResult::Failure(EKnightError::InvalidState, TEXT("ConsumableRuntime"),
					LOCTEXT("UseRuntime", "The controlled Knight requires its own initialized consumable runtime."));
			}
			if (!Outcome.Result.IsSuccess())
			{
				return Outcome;
			}
		}
	}
	Outcome.Result = ValidateParticipants();
	FKnightInventoryView Revalidated;
	if (Outcome.Result.IsSuccess())
	{
		Outcome.Result = TransactionInventory->GetPreparedView(InventoryCapability, Revalidated);
	}
	if (Outcome.Result.IsSuccess() && !(PreparedInventory == Revalidated))
	{
		Outcome.Result = FKnightResult::Failure(EKnightError::Conflict, TEXT("GearStage"),
			LOCTEXT("PreparedChanged", "The prepared inventory changed during source staging."));
	}
	for (int32 Index = 0; Outcome.Result.IsSuccess() && Index < ASCs.Num(); ++Index)
	{
		if (Capabilities[Index].IsValid())
		{
			Outcome.Result = ASCs[Index]->ValidateStagedSourceProjection(Fences[Index], Capabilities[Index]);
		}
	}
	if (!Outcome.Result.IsSuccess())
	{
		return Outcome;
	}
	// Every fallible preflight is complete; install all participants before any subscriber runs.
	for (int32 Index = 0; Index < ASCs.Num(); ++Index)
	{
		if (!Capabilities[Index].IsValid())
		{
			continue;
		}
		Outcome.Result = ASCs[Index]->CommitSourceProjection(Fences[Index], Capabilities[Index]);
		if (!Outcome.Result.IsSuccess())
		{
			return Outcome;
		}
	}
	if (ConsumableRuntime != nullptr)
	{
		ConsumableRuntime->CommitPreparedConsumable();
	}
	Outcome.Result = Inventory->CommitPrepared(InventoryCapability);
	if (!Outcome.Result.IsSuccess())
	{
		return Outcome;
	}
	Candidate.InventoryRevision = PreparedInventory.Revision;
	FKnightRPGReceipt Receipt;
	Receipt.HumanId = Caller.HumanId;
	Receipt.Command = Command;
	Receipt.InventoryRequest = Request;
	Receipt.CommittedInventoryRevision = PreparedInventory.Revision;
	Receipt.AffectedInstanceId = Outcome.AffectedInstanceId;
	Receipt.CommittedCurrency = Candidate.Currency;
	for (const FKnightRPGKnightView& Knight : Candidate.Knights)
	{
		if (Knight.KnightId == Caller.KnightId)
		{
			Receipt.CommittedKnight = Knight;
		}
		if (Before.Knights.FindByPredicate([&Knight](const FKnightRPGKnightView& Previous)
			{ return Previous.KnightId == Knight.KnightId; })->Revision != Knight.Revision)
		{
			Receipt.CommittedKnights.Add(Knight);
		}
	}
	if (Command.Operation == EKnightRPGOperation::ClaimReward)
	{
		Receipt.Reward = *RewardOffers.FindByPredicate([&Command](const FKnightGearRewardBundle& Bundle)
		{
			return Bundle.RewardId == Command.RewardId;
		});
	}
	Outcome.CommittedInventoryRevision = Receipt.CommittedInventoryRevision;
	Outcome.CommittedKnightRevision = Receipt.CommittedKnight.Revision;
	ReceiptIndices.Add(Command.RequestId, Receipts.Num());
	Receipts.Add(MoveTemp(Receipt));
	PublishedView = MoveTemp(Candidate);
	Inventory->AssignedInstanceIds.Reset();
	for (const FKnightItemInstance& Item : PreparedInventory.Items)
	{
		if (Item.LocationId != KnightGear::InventoryLocation)
		{
			Inventory->AssignedInstanceIds.Add(Item.InstanceId);
		}
	}
	bCommitted = true;
	RefreshReplicatedState();
	for (int32 Index = 0; Index < ASCs.Num(); ++Index)
	{
		if (Capabilities[Index].IsValid())
		{
			ASCs[Index]->PublishSourceProjection(Fences[Index], Capabilities[Index], true, PublicationOwner.Get());
		}
		if (!CanPublish())
		{
			return Outcome;
		}
	}
	if (ConsumableRuntime != nullptr)
	{
		ConsumableRuntime->PublishPreparedConsumable();
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
	UKnightEventSubsystem* Bus = GetWorld()->GetSubsystem<UKnightEventSubsystem>();
	if (Bus != nullptr)
	{
		FName EventName;
		switch (Command.Operation)
		{
		case EKnightRPGOperation::Equip:
		case EKnightRPGOperation::TransferEquipment: EventName = TEXT("ItemEquipped"); break;
		case EKnightRPGOperation::UpgradeItem: EventName = TEXT("ItemUpgraded"); break;
		case EKnightRPGOperation::UseConsumable: EventName = TEXT("ConsumableUsed"); break;
		case EKnightRPGOperation::ClaimReward: EventName = TEXT("RewardClaimed"); break;
		default: break;
		}
		if (!EventName.IsNone())
		{
			FKnightItemEvent Event;
			Event.ConfirmationId = FGuid::NewGuid();
			Event.SourceId = Caller.KnightId;
			Event.TargetId = Command.TargetKnightId.IsValid() ? Command.TargetKnightId : Caller.KnightId;
			Event.Revision = PublishedView.Revision;
			Event.EventName = EventName;
			Event.RequestId = Command.RequestId;
			Event.DemoRunId = PublishedView.DemoRunId;
			Event.EncounterId = Command.Operation == EKnightRPGOperation::ClaimReward
				? Receipts.Last().Reward.EncounterId : Caller.EncounterId;
			Event.InstanceId = Outcome.AffectedInstanceId;
			Event.Quantity = Command.Quantity;
			const FKnightItemInstance* Item = PreparedInventory.Items.FindByPredicate(
				[&Outcome](const FKnightItemInstance& Entry) { return Entry.InstanceId == Outcome.AffectedInstanceId; });
			const FKnightInventoryMutation* Mutation = Request.Mutations.FindByPredicate(
				[&Outcome](const FKnightInventoryMutation& Entry) { return Entry.Item.InstanceId == Outcome.AffectedInstanceId; });
			Event.DefinitionId = Item != nullptr ? Item->DefinitionId
				: Mutation != nullptr ? Mutation->Item.DefinitionId : Command.ItemDefinitionId;
			Bus->PublishConfirmed(Event);
		}
		if (Command.Operation == EKnightRPGOperation::BuyItem || Command.Operation == EKnightRPGOperation::ClaimReward)
		{
			for (const FKnightInventoryMutation& Mutation : Request.Mutations)
			{
				if (!CanPublish())
				{
					return Outcome;
				}
				if (Mutation.Operation != EKnightInventoryOperation::Add)
				{
					continue;
				}
				FKnightItemEvent Event;
				Event.ConfirmationId = FGuid::NewGuid();
				Event.SourceId = Caller.KnightId;
				Event.TargetId = Caller.KnightId;
				Event.Revision = PublishedView.Revision;
				Event.EventName = TEXT("ItemAcquired");
				Event.RequestId = Command.RequestId;
				Event.DemoRunId = PublishedView.DemoRunId;
				Event.EncounterId = Command.Operation == EKnightRPGOperation::ClaimReward
					? Receipts.Last().Reward.EncounterId : Caller.EncounterId;
				Event.InstanceId = Mutation.Item.InstanceId;
				Event.DefinitionId = Mutation.Item.DefinitionId;
				Event.Quantity = Mutation.Item.Count;
				Bus->PublishConfirmed(Event);
			}
		}
		if (Command.Operation == EKnightRPGOperation::ApplyGearPreset || Command.Operation == EKnightRPGOperation::SwapRings)
		{
			const FKnightRPGKnightView Confirmed = GetKnightView(Caller.KnightId);
			for (const FKnightEquipmentAssignment& Assignment : Confirmed.Assignments)
			{
				const FKnightRPGKnightView* Previous = Before.Knights.FindByPredicate(
					[&Caller](const FKnightRPGKnightView& Knight) { return Knight.KnightId == Caller.KnightId; });
				const FKnightEquipmentAssignment* PreviousAssignment = Previous->FindAssignment(Assignment.SlotTag);
				if (PreviousAssignment != nullptr && *PreviousAssignment == Assignment)
				{
					continue;
				}
				if (!CanPublish())
				{
					return Outcome;
				}
				FKnightItemEvent Event;
				Event.ConfirmationId = FGuid::NewGuid();
				Event.SourceId = Caller.KnightId;
				Event.TargetId = Caller.KnightId;
				Event.Revision = PublishedView.Revision;
				Event.EventName = TEXT("ItemEquipped");
				Event.RequestId = Command.RequestId;
				Event.DemoRunId = PublishedView.DemoRunId;
				Event.EncounterId = Caller.EncounterId;
				Event.InstanceId = Assignment.ItemInstanceId;
				Event.DefinitionId = Assignment.DefinitionId;
				Event.Quantity = 1;
				Bus->PublishConfirmed(Event);
			}
		}
		if (!CanPublish())
		{
			return Outcome;
		}
		const FKnightRPGKnightView Confirmed = GetKnightView(Caller.KnightId);
		const FKnightRPGKnightView* Previous = Before.Knights.FindByPredicate(
			[&Caller](const FKnightRPGKnightView& Knight) { return Knight.KnightId == Caller.KnightId; });
		auto PublishSkillFact = [Bus, &Caller, &Command, &Outcome](FName Name, FName Subject)
		{
			FKnightSkillLabEvent Event;
			Event.ConfirmationId = FGuid::NewGuid();
			Event.SourceId = Caller.KnightId;
			Event.TargetId = Caller.KnightId;
			Event.Revision = Outcome.CommittedKnightRevision;
			Event.EncounterId = Caller.EncounterId;
			Event.RequestId = Command.RequestId;
			Event.EventName = Name;
			Event.SubjectId = Subject;
			Bus->PublishConfirmed(Event);
		};
		if (Command.Operation == EKnightRPGOperation::ApplyBuild || Command.Operation == EKnightRPGOperation::ApplyTalentLoadout)
		{
			PublishSkillFact(TEXT("LoadoutApplied"), Confirmed.Build.SpecId);
		}
		if (CanPublish() && Previous->Build.SpecId != Confirmed.Build.SpecId)
		{
			PublishSkillFact(TEXT("SpecChanged"), Confirmed.Build.SpecId);
		}
		if (CanPublish() && Previous->WeaponRuneInstanceId != Confirmed.WeaponRuneInstanceId)
		{
			PublishSkillFact(TEXT("RuneAttached"), Confirmed.Build.WeaponRune);
		}
		if (CanPublish() && Previous->SkillRuneInstanceId != Confirmed.SkillRuneInstanceId)
		{
			PublishSkillFact(TEXT("RuneAttached"), Confirmed.Build.SkillRune);
		}
	}
	return Outcome;
}

#undef LOCTEXT_NAMESPACE
