#include "Progression/GolemRPGComponent.h"

#include "Components/GolemAbilitySystemComponent.h"
#include "Components/GolemLifeComponent.h"
#include "Definitions/GolemItemDefinition.h"
#include "Events/GolemEventSubsystem.h"
#include "Events/GolemItemEvent.h"
#include "Events/GolemSkillLabEvent.h"
#include "GameplayAbilitySpec.h"
#include "Inventory/GolemInventoryComponent.h"
#include "Skills/GolemSkillRuntimeComponent.h"
#include "Engine/World.h"
#include "Misc/ScopeExit.h"

#define LOCTEXT_NAMESPACE "GolemRPGGearTransactions"

FGolemRPGCommandResult UGolemRPGComponent::ExecuteGearCommand(const FGolemRPGCallerContext& Caller,
	const FGolemRPGCommand& Command)
{
	FGolemRPGCommandResult Outcome;
	Outcome.Operation = Command.Operation;
	Outcome.RequestId = Command.RequestId;
	Outcome.GolemId = Caller.GolemId;
	Outcome.InventoryId = Command.InventoryId;
	UGolemInventoryComponent* TransactionInventory = Inventory;
	const bool bConsumable = Command.Operation == EGolemRPGOperation::UseConsumable;
	if (Command.Operation == EGolemRPGOperation::DeliverQuestItem && !Caller.bQuestDeliveryAuthorized)
	{
		Outcome.Result = FGolemResult::Failure(EGolemError::NotOwned, TEXT("QuestDeliveryAuthority"), FText::GetEmpty());
		return Outcome;
	}
	const TWeakObjectPtr<AActor> PublicationOwner(GetOwner());
	const FGolemRPGView Before = PublishedView;
	const auto CanPublish = [this, &Caller]()
	{
		return ValidateAuthority().IsSuccess() && ValidateCaller(Caller).IsSuccess();
	};
	TArray<FGolemEntityId> Participants { Caller.GolemId };
	if (Command.Operation == EGolemRPGOperation::TransferEquipment)
	{
		if (!Caller.EditableGolems.Contains(Caller.GolemId)
			|| !Caller.EditableGolems.Contains(Command.TargetGolemId))
		{
			Outcome.Result = FGolemResult::Failure(EGolemError::NotOwned, TEXT("GearTransferOwnership"),
				LOCTEXT("BothOwners", "Transfer requires this human's authoritative permission for both canonical Golems."));
			return Outcome;
		}
		Participants.Add(Command.TargetGolemId);
	}
	if (Command.Operation == EGolemRPGOperation::ClaimReward)
	{
		Participants.Reset();
		for (const FGolemRPGGolemView& Golem : Before.Golems)
		{
			Participants.Add(Golem.GolemId);
		}
	}
	auto ValidateParticipants = [this, &Caller, &Participants, bConsumable]()
	{
		FGolemResult Result = ValidateAuthority();
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
			return FGolemResult::Failure(EGolemError::InvalidState, TEXT("GearSanctuary"),
				LOCTEXT("Sanctuary", "Equipment, vendor, rewards and build changes require the authoritative sanctuary."));
		}
		for (const FGolemEntityId& Id : Participants)
		{
			const FGolemRPGGolemRegistration* Registration = Registrations.Find(Id);
			UGolemAbilitySystemComponent* ASC = Registration == nullptr ? nullptr : Registration->AbilitySystem.Get();
			AActor* Pawn = Registration == nullptr ? nullptr : Registration->Pawn.Get();
			const UGolemSkillRuntimeComponent* Runtime = Pawn == nullptr
				? nullptr : Pawn->FindComponentByClass<UGolemSkillRuntimeComponent>();
			if (!IsValid(Pawn) || Pawn->IsActorBeingDestroyed() || !IsValid(ASC)
				|| ASC->GetLifeComponent() == nullptr || !ASC->GetLifeComponent()->IsAlive()
				|| (!bConsumable && (Runtime != nullptr && Runtime->HasDeferredDamage()
					|| ASC->GetActivatableAbilities().ContainsByPredicate([](const FGameplayAbilitySpec& Spec)
					{
						return Spec.IsActive();
					}))))
			{
				return FGolemResult::Failure(EGolemError::InvalidState, TEXT("GearParticipantState"),
					LOCTEXT("ParticipantState", "All affected Golems must be living and free of active actions or deferred damage."));
			}
		}
		return FGolemResult::Success();
	};
	Outcome.Result = ValidateParticipants();
	if (!Outcome.Result.IsSuccess())
	{
		return Outcome;
	}
	FGolemRPGView Candidate;
	FGolemInventoryRequest Request;
	Outcome.Result = BuildCandidate(Before, Inventory->GetView(), Command, nullptr, nullptr,
		Candidate, Request, Outcome.AffectedInstanceId);
	if (!Outcome.Result.IsSuccess())
	{
		return Outcome;
	}
	bTransactionInProgress = true;
	FGolemInventoryCapability InventoryCapability;
	TArray<UGolemAbilitySystemComponent*> ASCs;
	TArray<FGolemMutationFence> Fences;
	TArray<FGolemProjectionCapability> Capabilities;
	UGolemSkillRuntimeComponent* ConsumableRuntime = nullptr;
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
	FGolemInventoryView PreparedInventory;
	if (Outcome.Result.IsSuccess())
	{
		Outcome.Result = Inventory->GetPreparedView(InventoryCapability, PreparedInventory);
	}
	if (!Outcome.Result.IsSuccess())
	{
		return Outcome;
	}
	for (FGolemRPGGolemView& Golem : Candidate.Golems)
	{
		const FGolemRPGGolemView* Previous = Before.Golems.FindByPredicate([&Golem](const FGolemRPGGolemView& Entry)
		{
			return Entry.GolemId == Golem.GolemId;
		});
		if (Previous->Revision == Golem.Revision)
		{
			continue;
		}
		FGolemGASProjectionPlan Plan;
		Outcome.Result = BuildProjection(Golem, PreparedInventory.Items, Plan);
		if (Outcome.Result.IsSuccess())
		{
			Outcome.Result = ValidateBuildAttachments(Golem, Candidate, PreparedInventory.Items);
		}
		if (!Outcome.Result.IsSuccess())
		{
			return Outcome;
		}
		UGolemAbilitySystemComponent* ASC = Registrations.FindChecked(Golem.GolemId).AbilitySystem.Get();
		ASCs.Add(ASC);
		Fences.AddDefaulted();
		Capabilities.AddDefaulted();
		Outcome.Result = ASC->BeginMutationFence(Command.RequestId, Fences.Last());
		if (Outcome.Result.IsSuccess() && !bConsumable)
		{
			Outcome.Result = ASC->PrepareSourceProjection(Fences.Last(), Plan,
				ASC->GetSourceProjection(GolemRPG::ProjectionSource).Revision, Capabilities.Last());
		}
		if (Outcome.Result.IsSuccess() && !bConsumable)
		{
			Outcome.Result = ASC->StageSourceProjection(Fences.Last(), Capabilities.Last());
		}
		if (!Outcome.Result.IsSuccess())
		{
			return Outcome;
		}
		if (bConsumable && Golem.GolemId == Caller.GolemId)
		{
			ConsumableRuntime = Caller.Pawn->FindComponentByClass<UGolemSkillRuntimeComponent>();
			const FGolemInventoryView CurrentInventory = Inventory->GetView();
			const FGolemItemInstance* Used = CurrentInventory.Items.FindByPredicate(
				[&Outcome](const FGolemItemInstance& Item) { return Item.InstanceId == Outcome.AffectedInstanceId; });
			const UGolemDefinition* Definition = nullptr;
			Outcome.Result = Used != nullptr ? ResolveDefinition(Used->DefinitionId,
				UGolemItemDefinition::StaticClass(), Definition)
				: FGolemResult::Failure(EGolemError::NotFound, TEXT("Consumable"), LOCTEXT("MissingUse", "The bound consumable is unavailable."));
			if (Outcome.Result.IsSuccess() && ConsumableRuntime != nullptr)
			{
				Outcome.Result = ConsumableRuntime->PrepareConsumable(
					CastChecked<UGolemItemDefinition>(Definition)->GetMetadata().SocketProfile, Fences.Last());
			}
			else if (ConsumableRuntime == nullptr)
			{
				Outcome.Result = FGolemResult::Failure(EGolemError::InvalidState, TEXT("ConsumableRuntime"),
					LOCTEXT("UseRuntime", "The controlled Golem requires its own initialized consumable runtime."));
			}
			if (!Outcome.Result.IsSuccess())
			{
				return Outcome;
			}
		}
	}
	Outcome.Result = ValidateParticipants();
	FGolemInventoryView Revalidated;
	if (Outcome.Result.IsSuccess())
	{
		Outcome.Result = TransactionInventory->GetPreparedView(InventoryCapability, Revalidated);
	}
	if (Outcome.Result.IsSuccess() && !(PreparedInventory == Revalidated))
	{
		Outcome.Result = FGolemResult::Failure(EGolemError::Conflict, TEXT("GearStage"),
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
	FGolemRPGReceipt Receipt;
	Receipt.HumanId = Caller.HumanId;
	Receipt.Command = Command;
	Receipt.InventoryRequest = Request;
	Receipt.CommittedInventoryRevision = PreparedInventory.Revision;
	Receipt.AffectedInstanceId = Outcome.AffectedInstanceId;
	Receipt.CommittedCurrency = Candidate.Currency;
	for (const FGolemRPGGolemView& Golem : Candidate.Golems)
	{
		if (Golem.GolemId == Caller.GolemId)
		{
			Receipt.CommittedGolem = Golem;
		}
		if (Before.Golems.FindByPredicate([&Golem](const FGolemRPGGolemView& Previous)
			{ return Previous.GolemId == Golem.GolemId; })->Revision != Golem.Revision)
		{
			Receipt.CommittedGolems.Add(Golem);
		}
	}
	if (Command.Operation == EGolemRPGOperation::ClaimReward)
	{
		Receipt.Reward = *RewardOffers.FindByPredicate([&Command](const FGolemGearRewardBundle& Bundle)
		{
			return Bundle.RewardId == Command.RewardId;
		});
	}
	Outcome.CommittedInventoryRevision = Receipt.CommittedInventoryRevision;
	Outcome.CommittedGolemRevision = Receipt.CommittedGolem.Revision;
	ReceiptIndices.Add(Command.RequestId, Receipts.Num());
	Receipts.Add(MoveTemp(Receipt));
	PublishedView = MoveTemp(Candidate);
	Inventory->AssignedInstanceIds.Reset();
	for (const FGolemItemInstance& Item : PreparedInventory.Items)
	{
		if (Item.LocationId != GolemGear::InventoryLocation)
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
	UGolemEventSubsystem* Bus = GetWorld()->GetSubsystem<UGolemEventSubsystem>();
	if (Bus != nullptr)
	{
		FName EventName;
		switch (Command.Operation)
		{
		case EGolemRPGOperation::Equip:
		case EGolemRPGOperation::TransferEquipment: EventName = TEXT("ItemEquipped"); break;
		case EGolemRPGOperation::UpgradeItem: EventName = TEXT("ItemUpgraded"); break;
		case EGolemRPGOperation::UseConsumable: EventName = TEXT("ConsumableUsed"); break;
		case EGolemRPGOperation::ClaimReward: EventName = TEXT("RewardClaimed"); break;
		case EGolemRPGOperation::DeliverQuestItem: EventName = TEXT("ItemDelivered"); break;
		default: break;
		}
		if (!EventName.IsNone())
		{
			FGolemItemEvent Event;
			Event.ConfirmationId = FGuid::NewGuid();
			Event.SourceId = Caller.GolemId;
			Event.TargetId = Command.TargetGolemId.IsValid() ? Command.TargetGolemId : Caller.GolemId;
			Event.Revision = PublishedView.Revision;
			Event.EventName = EventName;
			Event.RequestId = Command.RequestId;
			Event.DemoRunId = PublishedView.DemoRunId;
			Event.EncounterId = Command.Operation == EGolemRPGOperation::ClaimReward
				? Receipts.Last().Reward.EncounterId : Caller.EncounterId;
			Event.InstanceId = Outcome.AffectedInstanceId;
			Event.Quantity = Command.Quantity;
			const FGolemItemInstance* Item = PreparedInventory.Items.FindByPredicate(
				[&Outcome](const FGolemItemInstance& Entry) { return Entry.InstanceId == Outcome.AffectedInstanceId; });
			const FGolemInventoryMutation* Mutation = Request.Mutations.FindByPredicate(
				[&Outcome](const FGolemInventoryMutation& Entry) { return Entry.Item.InstanceId == Outcome.AffectedInstanceId; });
			Event.DefinitionId = Item != nullptr ? Item->DefinitionId
				: Mutation != nullptr ? Mutation->Item.DefinitionId : Command.ItemDefinitionId;
			Bus->PublishConfirmed(Event);
		}
		if (Command.Operation == EGolemRPGOperation::BuyItem || Command.Operation == EGolemRPGOperation::ClaimReward)
		{
			for (const FGolemInventoryMutation& Mutation : Request.Mutations)
			{
				if (!CanPublish())
				{
					return Outcome;
				}
				if (Mutation.Operation != EGolemInventoryOperation::Add)
				{
					continue;
				}
				FGolemItemEvent Event;
				Event.ConfirmationId = FGuid::NewGuid();
				Event.SourceId = Caller.GolemId;
				Event.TargetId = Caller.GolemId;
				Event.Revision = PublishedView.Revision;
				Event.EventName = TEXT("ItemAcquired");
				Event.RequestId = Command.RequestId;
				Event.DemoRunId = PublishedView.DemoRunId;
				Event.EncounterId = Command.Operation == EGolemRPGOperation::ClaimReward
					? Receipts.Last().Reward.EncounterId : Caller.EncounterId;
				Event.InstanceId = Mutation.Item.InstanceId;
				Event.DefinitionId = Mutation.Item.DefinitionId;
				Event.Quantity = Mutation.Item.Count;
				Bus->PublishConfirmed(Event);
			}
		}
		if (Command.Operation == EGolemRPGOperation::ApplyGearPreset || Command.Operation == EGolemRPGOperation::SwapRings)
		{
			const FGolemRPGGolemView Confirmed = GetGolemView(Caller.GolemId);
			for (const FGolemEquipmentAssignment& Assignment : Confirmed.Assignments)
			{
				const FGolemRPGGolemView* Previous = Before.Golems.FindByPredicate(
					[&Caller](const FGolemRPGGolemView& Golem) { return Golem.GolemId == Caller.GolemId; });
				const FGolemEquipmentAssignment* PreviousAssignment = Previous->FindAssignment(Assignment.SlotTag);
				if (PreviousAssignment != nullptr && *PreviousAssignment == Assignment)
				{
					continue;
				}
				if (!CanPublish())
				{
					return Outcome;
				}
				FGolemItemEvent Event;
				Event.ConfirmationId = FGuid::NewGuid();
				Event.SourceId = Caller.GolemId;
				Event.TargetId = Caller.GolemId;
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
		const FGolemRPGGolemView Confirmed = GetGolemView(Caller.GolemId);
		const FGolemRPGGolemView* Previous = Before.Golems.FindByPredicate(
			[&Caller](const FGolemRPGGolemView& Golem) { return Golem.GolemId == Caller.GolemId; });
		auto PublishSkillFact = [Bus, &Caller, &Command, &Outcome](FName Name, FName Subject)
		{
			FGolemSkillLabEvent Event;
			Event.ConfirmationId = FGuid::NewGuid();
			Event.SourceId = Caller.GolemId;
			Event.TargetId = Caller.GolemId;
			Event.Revision = Outcome.CommittedGolemRevision;
			Event.EncounterId = Caller.EncounterId;
			Event.RequestId = Command.RequestId;
			Event.EventName = Name;
			Event.SubjectId = Subject;
			Bus->PublishConfirmed(Event);
		};
		if (Command.Operation == EGolemRPGOperation::ApplyBuild || Command.Operation == EGolemRPGOperation::ApplyTalentLoadout)
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
