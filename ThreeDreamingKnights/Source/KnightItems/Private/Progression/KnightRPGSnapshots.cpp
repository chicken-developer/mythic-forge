#include "Progression/KnightRPGComponent.h"

#include "Assets/KnightAssetManager.h"
#include "Assets/KnightDefinition.h"
#include "Components/KnightAbilitySystemComponent.h"
#include "Components/KnightLifeComponent.h"
#include "Attributes/KnightAttributeSet.h"
#include "Definitions/KnightAbilitySetDefinition.h"
#include "Definitions/KnightItemDefinition.h"
#include "GameplayAbilitySpec.h"
#include "GameplayEffect.h"
#include "Inventory/KnightInventoryComponent.h"
#include "Tags/KnightGameplayTags.h"
#include "Misc/ScopeExit.h"

#define LOCTEXT_NAMESPACE "KnightRPGSnapshots"

namespace
{
	bool DeriveLegacyRuneParents(const FKnightRPGView& View, TMap<FGuid, FGuid>& OutParents)
	{
		OutParents.Reset();
		for (const FKnightRPGKnightView& Knight : View.Knights)
		{
			if (!Knight.WeaponRuneInstanceId.IsValid())
			{
				continue;
			}
			const FKnightEquipmentAssignment* Weapon = Knight.FindAssignment(KnightGameplayTags::Slot_Equipment_Weapon);
			if (Weapon == nullptr || OutParents.Contains(Knight.WeaponRuneInstanceId))
			{
				return false;
			}
			OutParents.Add(Knight.WeaponRuneInstanceId, Weapon->ItemInstanceId);
		}
		return true;
	}

	FKnightResult GatherPassiveMaximumBonuses(const FKnightGASProjectionPlan& Plan,
		FKnightItemStatBonuses& OutBonuses)
	{
		OutBonuses = FKnightItemStatBonuses();
		TArray<FPrimaryAssetId> SetIds = Plan.OptionalAbilitySetIds;
		if (Plan.OptionalAbilitySetId.IsValid())
		{
			SetIds.Add(Plan.OptionalAbilitySetId);
		}
		UKnightAssetManager* Manager = UKnightAssetManager::GetKnightAssetManager();
		for (const FPrimaryAssetId& SetId : SetIds)
		{
			UKnightDefinition* Resolved = nullptr;
			FKnightResult Result = Manager != nullptr
				? Manager->ResolveDefinition(SetId, UKnightAbilitySetDefinition::StaticClass(), Resolved)
				: FKnightResult::Failure(EKnightError::LoadFailed, TEXT("SnapshotPassiveCatalog"),
					LOCTEXT("PassiveCatalog", "The typed passive ability-set catalog is unavailable."));
			if (!Result.IsSuccess())
			{
				return Result;
			}
			const UKnightAbilitySetDefinition* Set = CastChecked<UKnightAbilitySetDefinition>(Resolved);
			Result = Set->ValidateAbilitySet();
			if (!Result.IsSuccess())
			{
				return Result;
			}
			for (TSubclassOf<UGameplayEffect> EffectClass : Set->GetEffects())
			{
				const UGameplayEffect* Effect = EffectClass.GetDefaultObject();
				if (Effect == nullptr || Effect->DurationPolicy != EGameplayEffectDurationType::Infinite
					|| Effect->Period.GetValueAtLevel(1.0f) != 0.0f || !Effect->Executions.IsEmpty()
					|| Effect->GetStackingType() != EGameplayEffectStackingType::None)
				{
					return FKnightResult::Failure(EKnightError::InvalidData, TEXT("SnapshotPassivePolicy"),
						LOCTEXT("PassivePolicy", "Snapshot passives require the reversible source-projection effect policy."));
				}
				for (const FGameplayModifierInfo& Modifier : Effect->Modifiers)
				{
					float Magnitude = 0.0f;
					const bool bAllowedAttribute = Modifier.Attribute == UKnightAttributeSet::GetMaxHealthAttribute()
						|| Modifier.Attribute == UKnightAttributeSet::GetMaxStaminaAttribute()
						|| Modifier.Attribute == UKnightAttributeSet::GetMaxRuneEnergyAttribute()
						|| Modifier.Attribute == UKnightAttributeSet::GetAttackPowerAttribute()
						|| Modifier.Attribute == UKnightAttributeSet::GetDefenseAttribute();
					if (!bAllowedAttribute || Modifier.ModifierOp != EGameplayModOp::Additive
						|| !Modifier.ModifierMagnitude.GetStaticMagnitudeIfPossible(1.0f, Magnitude)
						|| !FMath::IsFinite(Magnitude) || Magnitude < 0.0f || Magnitude > 10000.0f)
					{
						return FKnightResult::Failure(EKnightError::InvalidData, TEXT("SnapshotPassiveMagnitude"),
							LOCTEXT("PassiveMagnitude", "Snapshot passive modifiers must be finite, static and additive at projection level one."));
					}
					if (Modifier.Attribute == UKnightAttributeSet::GetMaxHealthAttribute())
					{
						OutBonuses.MaxHealth += Magnitude;
					}
					else if (Modifier.Attribute == UKnightAttributeSet::GetMaxStaminaAttribute())
					{
						OutBonuses.MaxStamina += Magnitude;
					}
					else if (Modifier.Attribute == UKnightAttributeSet::GetMaxRuneEnergyAttribute())
					{
						OutBonuses.MaxRuneEnergy += Magnitude;
					}
				}
			}
		}
		return FKnightResult::Success();
	}
}

FKnightResult UKnightRPGComponent::MigrateSnapshot(FKnightRPGSnapshot& Snapshot)
{
	if (Snapshot.SchemaVersion == KnightRPG::SnapshotVersion)
	{
		return FKnightResult::Success();
	}
	if ((Snapshot.SchemaVersion != 1 && Snapshot.SchemaVersion != 2)
		|| (Snapshot.SchemaVersion == 1 && !Snapshot.CombatStates.IsEmpty()))
	{
		return FKnightResult::Failure(EKnightError::InvalidData, TEXT("RPGSnapshotMigration"),
			LOCTEXT("MigrationVersion", "Only the actual M5 and M5.5 schemas have supported adapters to the gear schema."));
	}
	if (!(Snapshot.ItemizationMigration == FKnightItemizationMigration())
		|| Snapshot.InitialView.bGearDemo || Snapshot.View.bGearDemo || Snapshot.InitialView.Currency != 0
		|| Snapshot.View.Currency != 0 || Snapshot.InitialView.DemoRunId.IsValid() || Snapshot.View.DemoRunId.IsValid()
		|| !Snapshot.RewardOffers.IsEmpty() || !Snapshot.View.ClaimedRewards.IsEmpty())
	{
		return FKnightResult::Failure(EKnightError::Conflict, TEXT("LegacyGearMigration"),
			LOCTEXT("LegacyGear", "Historical snapshots cannot claim gear-demo currency or reward data."));
	}
	const auto IsLegacyItem = [](const FKnightItemInstance& Item)
	{
		return Item.ItemRevision == 1 && Item.LocationId == KnightGear::InventoryLocation
			&& !Item.AssignedKnightId.IsValid() && !Item.SlotTag.IsValid() && Item.UpgradeRank == 0
			&& !Item.RuneAttachmentId.IsValid() && !Item.ParentWeaponId.IsValid() && !Item.RewardReceiptId.IsValid();
	};
	for (const FKnightInventorySnapshot* InventorySnapshot : { &Snapshot.InitialInventory, &Snapshot.Inventory })
	{
		for (const FKnightItemInstance& Item : InventorySnapshot->Items)
		{
			if (!IsLegacyItem(Item))
			{
				return FKnightResult::Failure(EKnightError::Conflict, TEXT("LegacyInventoryFields"),
					LOCTEXT("LegacyInventoryFields", "Historical items cannot claim physical locations, ranks or per-item revisions introduced by itemization."));
			}
		}
		for (const FKnightInventoryReceipt& Receipt : InventorySnapshot->Receipts)
		{
			for (const FKnightInventoryMutation& Mutation : Receipt.Request.Mutations)
			{
				if (Mutation.Operation > EKnightInventoryOperation::Remove || Mutation.ExpectedItemRevision != 0
					|| !IsLegacyItem(Mutation.Item))
				{
					return FKnightResult::Failure(EKnightError::Conflict, TEXT("LegacyInventoryJournal"),
						LOCTEXT("LegacyInventoryJournal", "Historical inventory journals retain only their original add/remove payloads."));
				}
			}
		}
	}
	for (const FKnightRPGView* View : { &Snapshot.InitialView, &Snapshot.View })
	{
		for (const FKnightRPGKnightView& Knight : View->Knights)
		{
			if (Knight.bGearEnabled || Knight.Level != 1 || !Knight.ConsumableBindings.IsEmpty())
			{
				return FKnightResult::Failure(EKnightError::Conflict, TEXT("LegacyGearFields"),
					LOCTEXT("LegacyGearFields", "Historical Knights cannot claim level or quick-binding fields introduced by gear fixtures."));
			}
		}
	}
	if (!Snapshot.LegacyWeaponRuneParents.IsEmpty()
		|| !DeriveLegacyRuneParents(Snapshot.View, Snapshot.LegacyWeaponRuneParents))
	{
		return FKnightResult::Failure(EKnightError::Conflict, TEXT("LegacyRuneParent"),
			LOCTEXT("LegacyRuneParent", "Historical attachment ownership must derive one exact equipped hammer parent."));
	}
	for (const FKnightRPGReceipt& Receipt : Snapshot.Receipts)
	{
		if (!Receipt.CommittedKnights.IsEmpty() || Receipt.CommittedCurrency != 0)
		{
			return FKnightResult::Failure(EKnightError::Conflict, TEXT("LegacyGearReceipt"),
				LOCTEXT("LegacyReceiptFields", "Historical receipts cannot claim multi-Knight or currency fields introduced by itemization."));
		}
	}
	if (Snapshot.SchemaVersion == 2)
	{
		for (const FKnightRPGReceipt& Receipt : Snapshot.Receipts)
		{
			if (Receipt.Command.Operation > EKnightRPGOperation::ApplyTalentLoadout)
			{
				return FKnightResult::Failure(EKnightError::Conflict, TEXT("LegacyGearReceipt"),
					LOCTEXT("LegacyReceipt", "M5.5 retains its original exact request and single-Knight receipt payloads."));
			}
		}
		Snapshot.InitialInventory.SchemaVersion = KnightItems::InventorySnapshotVersion;
		Snapshot.Inventory.SchemaVersion = KnightItems::InventorySnapshotVersion;
		Snapshot.SchemaVersion = KnightRPG::SnapshotVersion;
		return FKnightResult::Success();
	}
	auto IsLegacyKnight = [](const FKnightRPGKnightView& Knight)
	{
		return Knight.Build == FKnightSkillBuild() && !Knight.WeaponRuneInstanceId.IsValid()
			&& !Knight.SkillRuneInstanceId.IsValid() && Knight.TalentLoadouts.IsEmpty()
			&& !Knight.ActiveTalentLoadoutId.IsValid();
	};
	for (const FKnightRPGKnightView& Knight : Snapshot.InitialView.Knights)
	{
		if (!IsLegacyKnight(Knight))
		{
			return FKnightResult::Failure(EKnightError::Conflict, TEXT("RPGSnapshotMigration"),
				LOCTEXT("MigrationSeed", "Schema one cannot claim specialization or attachment data introduced by schema two."));
		}
	}
	for (const FKnightRPGKnightView& Knight : Snapshot.View.Knights)
	{
		if (!IsLegacyKnight(Knight))
		{
			return FKnightResult::Failure(EKnightError::Conflict, TEXT("RPGSnapshotMigration"),
				LOCTEXT("MigrationFinal", "The old M5 final state must retain the legacy build and its unchanged item identities."));
		}
	}
	for (const FKnightRPGReceipt& Receipt : Snapshot.Receipts)
	{
		if (Receipt.Command.Operation > EKnightRPGOperation::Respec || !IsLegacyKnight(Receipt.CommittedKnight))
		{
			return FKnightResult::Failure(EKnightError::Conflict, TEXT("RPGSnapshotMigration"),
				LOCTEXT("MigrationReceipt", "Schema one may only contain its original exact M5 operations and legacy receipts."));
		}
	}
	// Preserve every identity, point, paid cost, inventory value and durable request. New catalog remains locked.
	Snapshot.SchemaVersion = KnightRPG::SnapshotVersion;
	Snapshot.InitialInventory.SchemaVersion = KnightItems::InventorySnapshotVersion;
	Snapshot.Inventory.SchemaVersion = KnightItems::InventorySnapshotVersion;
	return FKnightResult::Success();
}

FKnightResult UKnightRPGComponent::CaptureSnapshot(FKnightRPGSnapshot& OutSnapshot) const
{
	OutSnapshot = FKnightRPGSnapshot();
	const FKnightResult Authority = ValidateAuthority();
	if (!Authority.IsSuccess())
	{
		return Authority;
	}
	if (!IsInitialized() || bTransactionInProgress || Inventory == nullptr || Inventory->IsReserved())
	{
		return FKnightResult::Failure(EKnightError::InvalidState, TEXT("RPGSnapshot"),
			LOCTEXT("CaptureState", "Capture requires initialized domains outside an in-progress mutation or publication."));
	}
	FKnightRPGSnapshot Candidate;
	Candidate.InitialSkillPoints = InitialSkillPoints;
	Candidate.InitialView = InitialView;
	Candidate.InitialInventory = InitialInventory;
	Candidate.View = PublishedView;
	Candidate.Receipts = Receipts;
	Candidate.RewardOffers = RewardOffers;
	Candidate.ItemizationMigration = ItemizationMigration;
	if (!Candidate.View.IsItemizationEnabled()
		&& !DeriveLegacyRuneParents(Candidate.View, Candidate.LegacyWeaponRuneParents))
	{
		return FKnightResult::Failure(EKnightError::Conflict, TEXT("LegacyRuneCapture"),
			LOCTEXT("LegacyRuneCapture", "Legacy runes require an unambiguous existing parent hammer GUID."));
	}
	for (const FKnightRPGKnightView& Knight : Candidate.View.Knights)
	{
		if (Knight.Build.SpecId.IsNone() && !Knight.bGearEnabled)
		{
			continue;
		}
		const FKnightRPGKnightRegistration* Registration = Registrations.Find(Knight.KnightId);
		AActor* Pawn = Registration == nullptr ? nullptr : Registration->Pawn.Get();
		const UKnightAbilitySystemComponent* ASC = Registration == nullptr ? nullptr : Registration->AbilitySystem.Get();
		const UKnightAttributeSet* Attributes = ASC == nullptr ? nullptr : ASC->GetKnightAttributes();
		const UKnightSkillRuntimeComponent* Runtime = Pawn == nullptr
			? nullptr : Pawn->FindComponentByClass<UKnightSkillRuntimeComponent>();
		if (Runtime == nullptr || Attributes == nullptr || ASC->GetLifeComponent() == nullptr
			|| !ASC->GetLifeComponent()->IsAlive() || Attributes->GetHealth() <= 0.0f
			|| Runtime->HasUnsnapshottedEffects()
			|| ASC->GetActivatableAbilities().ContainsByPredicate([](const FGameplayAbilitySpec& Spec)
			{
				return Spec.IsActive();
			}))
		{
			return FKnightResult::Failure(EKnightError::InvalidState, TEXT("SkillSnapshot"),
				LOCTEXT("SnapshotDebt", "Capture living idle SkillLab Knights after debt, shields, periodic streams and world pickup effects expire."));
		}
		FKnightRPGCombatSnapshot Fragment;
		Fragment.Skills = Runtime->CaptureSnapshot();
		Fragment.Health = Attributes->GetHealth();
		Fragment.Stamina = Attributes->GetStamina();
		Candidate.CombatStates.Add(Knight.KnightId, MoveTemp(Fragment));
	}
	const FKnightResult CaptureResult = Inventory->CaptureSnapshot(Candidate.Inventory);
	if (!CaptureResult.IsSuccess())
	{
		return CaptureResult;
	}
	FKnightRPGView Reconstructed;
	const FKnightResult Validation = ValidateSnapshot(Candidate, Reconstructed);
	if (Validation.IsSuccess())
	{
		OutSnapshot = MoveTemp(Candidate);
	}
	return Validation;
}

FKnightResult UKnightRPGComponent::ValidateSnapshot(const FKnightRPGSnapshot& Snapshot, FKnightRPGView& OutView) const
{
	OutView = FKnightRPGView();
	if (Inventory == nullptr || Snapshot.SchemaVersion != KnightRPG::SnapshotVersion
		|| Snapshot.InitialSkillPoints < 0 || Snapshot.InitialSkillPoints > KnightRPG::MaximumSkillPoints
		|| !Snapshot.InitialView.IsInitialized() || Snapshot.InitialView.Revision != 1
		|| Snapshot.InitialView.IsItemizationEnabled() != Snapshot.InitialView.bGearDemo
		|| !Snapshot.View.IsInitialized() || Snapshot.View.Revision != static_cast<int64>(Snapshot.Receipts.Num()) + 1
			+ (Snapshot.ItemizationMigration.IsPresent() ? 1 : 0)
		|| Snapshot.Receipts.Num() > KnightItems::MaximumTransactionReceipts
		|| Snapshot.InitialView.SkillTreeId != Snapshot.View.SkillTreeId
		|| Snapshot.InitialInventory.InventoryId != Snapshot.Inventory.InventoryId
		|| Snapshot.InitialInventory.Capacity != Snapshot.Inventory.Capacity
		|| Snapshot.InitialView.InventoryId != Snapshot.InitialInventory.InventoryId
		|| Snapshot.InitialView.InventoryRevision != Snapshot.InitialInventory.Revision
		|| Snapshot.View.InventoryId != Snapshot.Inventory.InventoryId
		|| Snapshot.View.InventoryRevision != Snapshot.Inventory.Revision)
	{
		return FKnightResult::Failure(EKnightError::InvalidData, TEXT("RPGRestore"),
			LOCTEXT("Header", "The RPG snapshot has an invalid schema, identity, initial state, revision or journal bound."));
	}
	const FKnightItemizationMigration& Migration = Snapshot.ItemizationMigration;
	if (Migration.IsPresent() ? Migration.LegacyReceiptCount < 0
		|| Migration.LegacyReceiptCount > Snapshot.Receipts.Num() || Snapshot.InitialView.IsItemizationEnabled()
		|| !Snapshot.View.IsItemizationEnabled() || Snapshot.View.bGearDemo
		|| Migration.InventoryRequest.RequestId != Migration.MigrationId
		|| Snapshot.InitialInventory.Receipts.ContainsByPredicate([&Migration](const FKnightInventoryReceipt& Receipt)
		{
			return Receipt.Request.RequestId == Migration.MigrationId;
		})
		: !(Migration == FKnightItemizationMigration()))
	{
		return FKnightResult::Failure(EKnightError::InvalidData, TEXT("ItemizationBoundary"),
			LOCTEXT("ItemizationBoundary", "The optional itemization boundary must identify one exact legacy journal prefix."));
	}
	if (Snapshot.InitialView.bGearDemo != Snapshot.View.bGearDemo
		|| Snapshot.InitialView.DemoRunId != Snapshot.View.DemoRunId
		|| !Snapshot.InitialView.ClaimedRewards.IsEmpty()
		|| (!Snapshot.View.bGearDemo && !Snapshot.RewardOffers.IsEmpty()))
	{
		return FKnightResult::Failure(EKnightError::Conflict, TEXT("GearSnapshotScope"),
			LOCTEXT("GearSnapshotScope", "The complete snapshot retains one fixture scope and starts before any reward claim."));
	}
	TSet<FGuid> OfferIds;
	for (const FKnightGearRewardBundle& Offer : Snapshot.RewardOffers)
	{
		if (!Offer.RewardId.IsValid() || OfferIds.Contains(Offer.RewardId) || Offer.DemoRunId != Snapshot.View.DemoRunId
			|| Offer.Currency < 0 || Offer.Currency > KnightGear::MaximumCurrency || Offer.Level < 1 || Offer.Level > 8
			|| (Offer.Items.IsEmpty() && Offer.BoundItems.IsEmpty()) || Offer.Items.Num() > 41
			|| Offer.BoundItems.Num() > 3)
		{
			return FKnightResult::Failure(EKnightError::InvalidData, TEXT("SnapshotRewardOffer"),
				LOCTEXT("SnapshotRewardOffer", "Every persisted reward is one complete bounded offer for the retained demo generation."));
		}
		OfferIds.Add(Offer.RewardId);
		TSet<FGuid> BoundIds;
		for (const FGuid& BoundId : Offer.BoundItems)
		{
			if (!BoundId.IsValid() || BoundIds.Contains(BoundId))
			{
				return FKnightResult::Failure(EKnightError::InvalidData, TEXT("SnapshotRewardBinding"),
					LOCTEXT("SnapshotRewardBinding", "Retained starter bindings require distinct exact instance IDs."));
			}
			BoundIds.Add(BoundId);
		}
		TSet<FPrimaryAssetId> DefinitionIds;
		for (const FKnightGearRewardItem& Item : Offer.Items)
		{
			const UKnightDefinition* Definition = nullptr;
			if (Item.Count < 1 || Item.Count > KnightItems::MaximumStackCount || DefinitionIds.Contains(Item.DefinitionId)
				|| !ResolveDefinition(Item.DefinitionId, UKnightItemDefinition::StaticClass(), Definition).IsSuccess())
			{
				return FKnightResult::Failure(EKnightError::InvalidData, TEXT("SnapshotRewardItem"),
					LOCTEXT("SnapshotRewardItem", "Persisted reward items require distinct real definitions and positive finite quantities."));
			}
			DefinitionIds.Add(Item.DefinitionId);
		}
	}
	TSet<FGuid> ClaimedIds;
	for (const FGuid& Id : Snapshot.View.ClaimedRewards)
	{
		if (!OfferIds.Contains(Id) || ClaimedIds.Contains(Id))
		{
			return FKnightResult::Failure(EKnightError::Conflict, TEXT("SnapshotRewardClaim"),
				LOCTEXT("SnapshotRewardClaim", "Claimed reward IDs must name distinct retained authoritative offers."));
		}
		ClaimedIds.Add(Id);
	}
	TMap<FGuid, FGuid> LegacyParents;
	if ((Snapshot.View.IsItemizationEnabled() && !Snapshot.LegacyWeaponRuneParents.IsEmpty())
		|| (!Snapshot.View.IsItemizationEnabled() && (!DeriveLegacyRuneParents(Snapshot.View, LegacyParents)
			|| !LegacyParents.OrderIndependentCompareEqual(Snapshot.LegacyWeaponRuneParents))))
	{
		return FKnightResult::Failure(EKnightError::Conflict, TEXT("SnapshotRuneAdapter"),
			LOCTEXT("SnapshotRuneAdapter", "The legacy parent adapter must agree with confirmed exact equipment ownership."));
	}
	TArray<FKnightItemInstance> InitialItems;
	TSet<FGuid> InitialIds;
	TMap<FGuid, int32> InitialIndices;
	FKnightResult Result = Inventory->ValidateSnapshot(Snapshot.InitialInventory, InitialItems, InitialIds, InitialIndices);
	if (!Result.IsSuccess())
	{
		return Result;
	}
	TArray<FKnightItemInstance> FinalItems;
	TSet<FGuid> FinalIds;
	TMap<FGuid, int32> FinalIndices;
	Result = Inventory->ValidateSnapshot(Snapshot.Inventory, FinalItems, FinalIds, FinalIndices);
	if (!Result.IsSuccess())
	{
		return Result;
	}
	if (Snapshot.InitialInventory.Receipts.Num() > Snapshot.Inventory.Receipts.Num())
	{
		return FKnightResult::Failure(EKnightError::Conflict, TEXT("RPGRestore"),
			LOCTEXT("InventoryPrefixCount", "The initial inventory journal cannot be longer than its final journal."));
	}
	for (int32 Index = 0; Index < Snapshot.InitialInventory.Receipts.Num(); ++Index)
	{
		if (!(Snapshot.InitialInventory.Receipts[Index] == Snapshot.Inventory.Receipts[Index]))
		{
			return FKnightResult::Failure(EKnightError::Conflict, TEXT("RPGRestore"),
				LOCTEXT("InventoryPrefix", "The complete initial inventory history must be an unchanged prefix of the final history."));
		}
	}
	FKnightRPGView Current = Snapshot.InitialView;
	TSet<FKnightEntityId> KnightIds;
	for (const FKnightRPGKnightView& Knight : Current.Knights)
	{
		if (!Knight.IsInitialized() || Knight.Revision != 1 || Knight.SkillPoints != Snapshot.InitialSkillPoints
			|| (!Current.bGearDemo && (!Knight.Assignments.IsEmpty() || !Knight.StatBonuses.IsEmpty()))
			|| !Knight.PurchasedSkills.IsEmpty()
			|| !(Knight.RuneTuning == FKnightRuneTuning()) || KnightIds.Contains(Knight.KnightId)
			|| !Knight.Build.ClassChoice.IsNone() || !Knight.Build.SpecChoice.IsNone()
			|| !Knight.Build.WeaponRune.IsNone() || !Knight.Build.SkillRune.IsNone()
			|| Knight.WeaponRuneInstanceId.IsValid() || Knight.SkillRuneInstanceId.IsValid()
			|| !Knight.TalentLoadouts.IsEmpty() || Knight.ActiveTalentLoadoutId.IsValid())
		{
			return FKnightResult::Failure(EKnightError::InvalidData, TEXT("RPGRestore"),
				LOCTEXT("InitialKnight", "Initial canonical RPG values must be distinct fresh Knights with the declared initial points."));
		}
		if (!Knight.Build.SpecId.IsNone()
			&& !FKnightSkillCatalog::ValidateBuild(Knight.HeroDefinitionId, Knight.Build).IsSuccess())
		{
			return FKnightResult::Failure(EKnightError::InvalidData, TEXT("SkillSnapshotSeed"),
				LOCTEXT("InitialSpec", "The initial unlocked spec must belong to its unchanged canonical hero identity."));
		}
		KnightIds.Add(Knight.KnightId);
	}
	if (Current.bGearDemo)
	{
		Result = ValidateGearLocations(Current, InitialItems);
		if (!Result.IsSuccess())
		{
			return Result;
		}
		for (FKnightRPGKnightView& Knight : Current.Knights)
		{
			if (!Knight.bGearEnabled || (Knight.Level != 1 && Knight.Level != 8))
			{
				return FKnightResult::Failure(EKnightError::InvalidData, TEXT("GearInitialLevel"),
					LOCTEXT("GearInitialLevel", "Gear fixtures start at level one or eight with explicit quick bindings."));
			}
			FKnightGASProjectionPlan Plan;
			Result = BuildProjection(Knight, InitialItems, Plan);
			if (!Result.IsSuccess())
			{
				return Result;
			}
		}
	}
	SortView(Current);
	if (!(Current == Snapshot.InitialView))
	{
		return FKnightResult::Failure(EKnightError::InvalidData, TEXT("RPGRestore"),
			LOCTEXT("CanonicalOrder", "Snapshot values must use the canonical deterministic Knight and slot ordering."));
	}
	FKnightInventoryView CurrentInventory;
	CurrentInventory.InventoryId = Snapshot.InitialInventory.InventoryId;
	CurrentInventory.Capacity = Snapshot.InitialInventory.Capacity;
	CurrentInventory.Revision = Snapshot.InitialInventory.Revision;
	CurrentInventory.Items = MoveTemp(InitialItems);
	TSet<FGuid> UsedIds = MoveTemp(InitialIds);
	TSet<FGuid> RequestIds;
	if (Migration.IsPresent())
	{
		RequestIds.Add(Migration.MigrationId);
	}
	TSet<FGuid> PickedIds;
	int32 InventoryReceiptIndex = Snapshot.InitialInventory.Receipts.Num();
	for (int32 ReceiptIndex = 0; ReceiptIndex <= Snapshot.Receipts.Num(); ++ReceiptIndex)
	{
		if (Migration.IsPresent() && ReceiptIndex == Migration.LegacyReceiptCount)
		{
			FKnightRPGView Migrated;
			FKnightInventoryRequest MigrationRequest;
			Result = BuildItemizationMigration(Current, CurrentInventory, Migration.MigrationId,
				Migrated, MigrationRequest);
			if (!Result.IsSuccess())
			{
				return Result;
			}
			if (!(MigrationRequest == Migration.InventoryRequest))
			{
				return FKnightResult::Failure(EKnightError::Conflict, TEXT("ItemizationPayload"),
					LOCTEXT("ItemizationPayload", "The migration locations must derive exactly from the verified legacy prefix."));
			}
			if (!MigrationRequest.Mutations.IsEmpty())
			{
				if (!Snapshot.Inventory.Receipts.IsValidIndex(InventoryReceiptIndex)
					|| !(Snapshot.Inventory.Receipts[InventoryReceiptIndex].Request == MigrationRequest))
				{
					return FKnightResult::Failure(EKnightError::Conflict, TEXT("ItemizationInventoryHistory"),
						LOCTEXT("ItemizationInventoryHistory", "The administrative locations must match the next exact inventory receipt."));
				}
				TArray<FKnightItemInstance> MigratedItems;
				TSet<FGuid> MigratedIds;
				Result = Inventory->BuildCandidate(CurrentInventory.Items, UsedIds, CurrentInventory.Capacity,
					MigrationRequest, MigratedItems, MigratedIds);
				if (!Result.IsSuccess())
				{
					return Result;
				}
				CurrentInventory.Items = MoveTemp(MigratedItems);
				UsedIds = MoveTemp(MigratedIds);
				++CurrentInventory.Revision;
				++InventoryReceiptIndex;
			}
			Migrated.InventoryRevision = CurrentInventory.Revision;
			Current = MoveTemp(Migrated);
		}
		if (ReceiptIndex == Snapshot.Receipts.Num())
		{
			break;
		}
		const FKnightRPGReceipt& Receipt = Snapshot.Receipts[ReceiptIndex];
		Result = Receipt.Command.ValidateShape();
		if (!Result.IsSuccess())
		{
			return Result;
		}
		if (!Receipt.HumanId.IsValid() || RequestIds.Contains(Receipt.Command.RequestId))
		{
			return FKnightResult::Failure(EKnightError::DuplicateId, TEXT("RPGRestore"),
				LOCTEXT("ReceiptIdentity", "Successful RPG receipts require valid human identities and unique exact request GUIDs."));
		}
		RequestIds.Add(Receipt.Command.RequestId);
		const bool bPickup = Receipt.Command.Operation == EKnightRPGOperation::Pickup;
		if ((bPickup && PickedIds.Contains(Receipt.Command.PickupId))
			|| (!bPickup && !(Receipt.PickedItem == FKnightItemInstance())))
		{
			return FKnightResult::Failure(EKnightError::Conflict, TEXT("RPGRestore"),
				LOCTEXT("PickupHistory", "Each pickup commits once, and unrelated commands cannot contain historical picked items."));
		}
		if (bPickup)
		{
			PickedIds.Add(Receipt.Command.PickupId);
		}
		FKnightRPGView Candidate;
		FKnightInventoryRequest DerivedRequest;
		FGuid Affected;
		Result = BuildCandidate(Current, CurrentInventory, Receipt.Command,
			bPickup ? &Receipt.PickedItem : nullptr, &Receipt.InventoryRequest.Mutations, Candidate, DerivedRequest, Affected);
		if (!Result.IsSuccess())
		{
			return Result;
		}
		if (!(DerivedRequest == Receipt.InventoryRequest) || Affected != Receipt.AffectedInstanceId)
		{
			return FKnightResult::Failure(EKnightError::Conflict, TEXT("RPGRestore"),
				LOCTEXT("ReceiptPayload", "The retained derived inventory mutation or affected identity disagrees with the actual command."));
		}
		if (!DerivedRequest.Mutations.IsEmpty())
		{
			if (!Snapshot.Inventory.Receipts.IsValidIndex(InventoryReceiptIndex)
				|| !(Snapshot.Inventory.Receipts[InventoryReceiptIndex].Request == DerivedRequest))
			{
				return FKnightResult::Failure(EKnightError::Conflict, TEXT("RPGRestore"),
					LOCTEXT("CostJournal", "Every RPG cost or pickup must match the next exact successful inventory receipt."));
			}
			TArray<FKnightItemInstance> CandidateItems;
			TSet<FGuid> CandidateIds;
			Result = Inventory->BuildCandidate(CurrentInventory.Items, UsedIds, CurrentInventory.Capacity,
				DerivedRequest, CandidateItems, CandidateIds);
			if (!Result.IsSuccess())
			{
				return Result;
			}
			CurrentInventory.Items = MoveTemp(CandidateItems);
			UsedIds = MoveTemp(CandidateIds);
			++CurrentInventory.Revision;
			++InventoryReceiptIndex;
		}
		Candidate.InventoryRevision = CurrentInventory.Revision;
		FKnightRPGKnightView* Knight = Candidate.Knights.FindByPredicate([&Receipt](const FKnightRPGKnightView& Entry)
			{
				return Entry.KnightId == Receipt.Command.ExpectedKnightId;
			});
		FKnightGASProjectionPlan Plan;
		Result = BuildProjection(*Knight, CurrentInventory.Items, Plan);
		if (Result.IsSuccess())
		{
			Result = ValidateBuildAttachments(*Knight, Candidate, CurrentInventory.Items);
		}
		if (!Result.IsSuccess())
		{
			return Result;
		}
		if (Candidate.IsItemizationEnabled())
		{
			TArray<FKnightRPGKnightView> CommittedKnights;
			for (FKnightRPGKnightView& Changed : Candidate.Knights)
			{
				const FKnightRPGKnightView* Previous = Current.Knights.FindByPredicate(
					[&Changed](const FKnightRPGKnightView& Entry) { return Entry.KnightId == Changed.KnightId; });
				if (Previous->Revision == Changed.Revision)
				{
					continue;
				}
				Result = BuildProjection(Changed, CurrentInventory.Items, Plan);
				if (Result.IsSuccess())
				{
					Result = ValidateBuildAttachments(Changed, Candidate, CurrentInventory.Items);
				}
				if (!Result.IsSuccess())
				{
					return Result;
				}
				CommittedKnights.Add(Changed);
			}
			if (CommittedKnights != Receipt.CommittedKnights || Candidate.Currency != Receipt.CommittedCurrency
				|| !ValidateGearLocations(Candidate, CurrentInventory.Items).IsSuccess())
			{
				return FKnightResult::Failure(EKnightError::Conflict, TEXT("GearReceiptState"),
					LOCTEXT("GearReceiptState", "All affected Knight states, exact locations and currency must match the retained receipt."));
			}
			if (Receipt.Command.Operation == EKnightRPGOperation::ClaimReward)
			{
				const FKnightGearRewardBundle* Bundle = Snapshot.RewardOffers.FindByPredicate(
					[&Receipt](const FKnightGearRewardBundle& Entry) { return Entry.RewardId == Receipt.Command.RewardId; });
				if (Bundle == nullptr || !(*Bundle == Receipt.Reward))
				{
					return FKnightResult::Failure(EKnightError::Conflict, TEXT("GearRewardHistory"),
						LOCTEXT("GearRewardHistory", "The claimed reward retains its exact registered bundle and identity."));
				}
			}
		}
		if (!(*Knight == Receipt.CommittedKnight) || CurrentInventory.Revision != Receipt.CommittedInventoryRevision)
		{
			return FKnightResult::Failure(EKnightError::Conflict, TEXT("RPGRestore"),
				LOCTEXT("KnightReceipt", "The retained Knight points, assignments, paid costs or derived stats disagree with journal reconstruction."));
		}
		Current = MoveTemp(Candidate);
	}
	if (InventoryReceiptIndex != Snapshot.Inventory.Receipts.Num() || CurrentInventory.Items != FinalItems
		|| CurrentInventory.Revision != Snapshot.Inventory.Revision || !(Current == Snapshot.View))
	{
		return FKnightResult::Failure(EKnightError::Conflict, TEXT("RPGRestore"),
			LOCTEXT("FinalState", "The complete reconstructed RPG and inventory values disagree with the claimed final snapshot."));
	}
	int32 ActiveBuilds = 0;
	for (const FKnightRPGKnightView& Knight : Current.Knights)
	{
		if (Knight.Build.SpecId.IsNone() && !Knight.bGearEnabled)
		{
			continue;
		}
		++ActiveBuilds;
		const FKnightRPGCombatSnapshot* Fragment = Snapshot.CombatStates.Find(Knight.KnightId);
		if (Fragment == nullptr || !FMath::IsFinite(Fragment->Health) || Fragment->Health <= 0.0f
			|| !FMath::IsFinite(Fragment->Stamina) || Fragment->Stamina < 0.0f
			|| !FMath::IsFinite(Fragment->Skills.Resource) || Fragment->Skills.Resource < 0.0f
			|| Fragment->Skills.Resource > 100000.0f || Fragment->Skills.CooldownRemaining.Num() != 4
			|| Fragment->Skills.Stacks < 0 || Fragment->Skills.Stacks > 5
			|| !UKnightSkillRuntimeComponent::ValidateSnapshot(Fragment->Skills).IsSuccess())
		{
			return FKnightResult::Failure(EKnightError::InvalidData, TEXT("SkillSnapshot"),
				LOCTEXT("CombatFragment", "Every unlocked Knight needs positive health, finite stamina/resource and four cooldown groups."));
		}
	}
	if (Snapshot.CombatStates.Num() != ActiveBuilds)
	{
		return FKnightResult::Failure(EKnightError::InvalidData, TEXT("SkillSnapshotIdentity"),
			LOCTEXT("CombatIds", "Combat fragments must match the exact unlocked canonical Knight identities."));
	}
	OutView = MoveTemp(Current);
	return FKnightResult::Success();
}

FKnightResult UKnightRPGComponent::TryRestoreSnapshot(UKnightInventoryComponent* InInventory,
	const FKnightRPGSnapshot& InSnapshot, TConstArrayView<FKnightRPGKnightRegistration> InRegistrationView)
{
	return RestoreSnapshot(InInventory, InSnapshot, InRegistrationView, FGuid());
}

FKnightResult UKnightRPGComponent::TryRestoreItemizationSnapshot(UKnightInventoryComponent* InInventory,
	const FKnightRPGSnapshot& Snapshot, TConstArrayView<FKnightRPGKnightRegistration> InRegistrations,
	const FGuid& MigrationId)
{
	if (!MigrationId.IsValid())
	{
		return FKnightResult::Failure(EKnightError::InvalidId, TEXT("ItemizationMigrationId"),
			LOCTEXT("ItemizationMigrationId", "Explicit itemization import requires a new stable administrative request identity."));
	}
	return RestoreSnapshot(InInventory, Snapshot, InRegistrations, MigrationId);
}

FKnightResult UKnightRPGComponent::RestoreSnapshot(UKnightInventoryComponent* InInventory,
	const FKnightRPGSnapshot& InSnapshot, TConstArrayView<FKnightRPGKnightRegistration> InRegistrationView,
	const FGuid& MigrationId)
{
	FKnightRPGSnapshot Snapshot = InSnapshot;
	TArray<FKnightRPGKnightRegistration> RestoreRegistrations;
	RestoreRegistrations.Append(InRegistrationView.GetData(), InRegistrationView.Num());
	const TConstArrayView<FKnightRPGKnightRegistration> InRegistrations(RestoreRegistrations);
	FKnightResult Result = ValidateAuthority();
	if (!Result.IsSuccess())
	{
		return Result;
	}
	Result = MigrateSnapshot(Snapshot);
	if (!Result.IsSuccess())
	{
		return Result;
	}
	if (IsInitialized() || bTransactionInProgress || !IsValid(InInventory) || InInventory->GetOwner() != GetOwner()
		|| InInventory->IsInitialized() || InInventory->IsReserved())
	{
		return FKnightResult::Failure(EKnightError::InvalidState, TEXT("RPGRestore"),
			LOCTEXT("FreshOnly", "RPG reconstruction requires fresh unreserved coordinator and inventory domains."));
	}
	Result = ValidateRegistrations(InRegistrations, &Snapshot.View);
	if (!Result.IsSuccess())
	{
		return Result;
	}
	Result = ResolveSkills(Snapshot.View.SkillTreeId);
	if (!Result.IsSuccess())
	{
		return Result;
	}
	const TArray<FKnightGearRewardBundle> PreviousOffers = RewardOffers;
	ON_SCOPE_EXIT
	{
		if (!IsInitialized())
		{
			RewardOffers = PreviousOffers;
		}
	};
	Inventory = InInventory;
	RewardOffers = Snapshot.RewardOffers;
	FKnightRPGView Candidate;
	Result = ValidateSnapshot(Snapshot, Candidate);
	if (!Result.IsSuccess())
	{
		Inventory = nullptr;
		return Result;
	}
	if (MigrationId.IsValid())
	{
		Result = AppendItemizationMigration(Snapshot, MigrationId);
		if (!Result.IsSuccess())
		{
			Inventory = nullptr;
			return Result;
		}
		for (const FKnightRPGKnightRegistration& Registration : InRegistrations)
		{
			if (Snapshot.CombatStates.Contains(Registration.KnightId))
			{
				continue;
			}
			const UKnightAttributeSet* Attributes = Registration.AbilitySystem->GetKnightAttributes();
			const UKnightSkillRuntimeComponent* Runtime = Registration.Pawn->FindComponentByClass<UKnightSkillRuntimeComponent>();
			if (Attributes == nullptr || Runtime == nullptr)
			{
				Inventory = nullptr;
				return FKnightResult::Failure(EKnightError::InvalidState, TEXT("ItemizationRuntime"),
					LOCTEXT("ItemizationRuntime", "Fresh itemization owners require the native equipment runtime and attribute set."));
			}
			// Schema one has no combat fragment. Retain the existing fresh-owner baseline policy without a refill.
			FKnightRPGCombatSnapshot Fragment;
			Fragment.Health = Attributes->GetHealth();
			Fragment.Stamina = Attributes->GetStamina();
			Fragment.Skills = Runtime->CaptureSnapshot();
			Snapshot.CombatStates.Add(Registration.KnightId, MoveTemp(Fragment));
		}
		Result = ValidateSnapshot(Snapshot, Candidate);
		if (!Result.IsSuccess())
		{
			Inventory = nullptr;
			return Result;
		}
	}
	for (const FKnightRPGKnightRegistration& Registration : InRegistrations)
	{
		FKnightRPGCombatSnapshot* Fragment = Snapshot.CombatStates.Find(Registration.KnightId);
		if (Fragment == nullptr)
		{
			continue;
		}
		FKnightRPGKnightView* Knight = Candidate.Knights.FindByPredicate(
			[&Registration](const FKnightRPGKnightView& Entry)
			{
				return Entry.KnightId == Registration.KnightId;
			});
		const UKnightAttributeSet* Attributes = Registration.AbilitySystem->GetKnightAttributes();
		UKnightSkillRuntimeComponent* Runtime = Registration.Pawn->FindComponentByClass<UKnightSkillRuntimeComponent>();
		FKnightGASProjectionPlan Plan;
		FKnightItemStatBonuses PassiveBonuses;
		Result = Knight != nullptr ? BuildProjection(*Knight, Snapshot.Inventory.Items, Plan)
			: FKnightResult::Failure(EKnightError::InvalidId, TEXT("SnapshotPassiveKnight"),
				LOCTEXT("PassiveKnight", "The passive capacity owner must match its canonical Knight."));
		if (Result.IsSuccess())
		{
			Result = GatherPassiveMaximumBonuses(Plan, PassiveBonuses);
		}
		if (!Result.IsSuccess())
		{
			Inventory = nullptr;
			return Result;
		}
		if (MigrationId.IsValid() && Attributes != nullptr)
		{
			Fragment->Health = FMath::Min(Fragment->Health,
				Attributes->GetMaxHealth() + Knight->StatBonuses.MaxHealth + PassiveBonuses.MaxHealth);
			Fragment->Stamina = FMath::Min(Fragment->Stamina,
				Attributes->GetMaxStamina() + Knight->StatBonuses.MaxStamina + PassiveBonuses.MaxStamina);
			Fragment->Skills.Resource = FMath::Min(Fragment->Skills.Resource,
				Attributes->GetMaxRuneEnergy() + Knight->StatBonuses.MaxRuneEnergy + PassiveBonuses.MaxRuneEnergy);
		}
		// Flat item/class bonuses and optional passive effects are independent source contributions.
		if (Runtime == nullptr || Attributes == nullptr
			|| Fragment->Health > Attributes->GetMaxHealth() + Knight->StatBonuses.MaxHealth + PassiveBonuses.MaxHealth
			|| Fragment->Stamina > Attributes->GetMaxStamina() + Knight->StatBonuses.MaxStamina + PassiveBonuses.MaxStamina
			|| Fragment->Skills.Resource > Attributes->GetMaxRuneEnergy()
				+ Knight->StatBonuses.MaxRuneEnergy + PassiveBonuses.MaxRuneEnergy)
		{
			Inventory = nullptr;
			return FKnightResult::Failure(EKnightError::InvalidData, TEXT("SkillSnapshotCapacity"),
				LOCTEXT("RestoreCapacity", "Combat values must fit the reconstructed build and its matching live runtime component."));
		}
	}
	bTransactionInProgress = true;
	FKnightInventoryCapability InventoryCapability;
	TArray<FKnightMutationFence> Fences;
	TArray<FKnightProjectionCapability> Capabilities;
	TArray<UKnightAbilitySystemComponent*> ASCs;
	const TWeakObjectPtr<AActor> PublicationOwner(GetOwner());
	bool bCommitted = false;
	ON_SCOPE_EXIT
	{
		for (int32 Index = ASCs.Num() - 1; Index >= 0; --Index)
		{
			if (IsValid(ASCs[Index]) && !bCommitted && Capabilities[Index].IsValid())
			{
				ASCs[Index]->AbortSourceProjection(Fences[Index], Capabilities[Index]);
			}
			if (IsValid(ASCs[Index]) && Fences[Index].IsValid())
			{
				if (bCommitted && Capabilities[Index].IsValid())
				{
					ASCs[Index]->PublishSourceProjection(Fences[Index], Capabilities[Index], false, PublicationOwner.Get());
				}
				ASCs[Index]->EndMutationFence(Fences[Index]);
			}
		}
		if (IsValid(InInventory) && InventoryCapability.IsValid())
		{
			if (bCommitted)
			{
				InInventory->ResetPrepared();
			}
			else
			{
				InInventory->AbortPrepared(InventoryCapability);
			}
		}
		if (!bCommitted)
		{
			Inventory = nullptr;
		}
		bTransactionInProgress = false;
	};
	Result = Inventory->PrepareRestore(Snapshot.Inventory, InventoryCapability);
	if (!Result.IsSuccess())
	{
		return Result;
	}
	for (const FKnightRPGKnightRegistration& Registration : InRegistrations)
	{
		UKnightAbilitySystemComponent* ASC = Registration.AbilitySystem.Get();
		ASCs.Add(ASC);
		Fences.AddDefaulted();
		Capabilities.AddDefaulted();
		FKnightRPGKnightView* Knight = Candidate.Knights.FindByPredicate([&Registration](const FKnightRPGKnightView& Entry)
			{
				return Entry.KnightId == Registration.KnightId;
			});
		FKnightGASProjectionPlan Plan;
		Result = BuildProjection(*Knight, Snapshot.Inventory.Items, Plan);
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
	// Validate every participant after the last raw GAS staging callback, before the first commit.
	Result = ValidateAuthority();
	if (Result.IsSuccess())
	{
		Result = ValidateRegistrations(InRegistrations, &Snapshot.View, true);
	}
	FKnightInventoryView RevalidatedInventory;
	if (Result.IsSuccess())
	{
		Result = InInventory->GetPreparedView(InventoryCapability, RevalidatedInventory);
	}
	if (Result.IsSuccess() && (RevalidatedInventory.InventoryId != Snapshot.Inventory.InventoryId
		|| RevalidatedInventory.Revision != Snapshot.Inventory.Revision
		|| RevalidatedInventory.Capacity != Snapshot.Inventory.Capacity
		|| RevalidatedInventory.Items != Snapshot.Inventory.Items))
	{
		Result = FKnightResult::Failure(EKnightError::Conflict, TEXT("RPGRestoreStage"),
			LOCTEXT("ChangedPreparedInventory", "The exact prepared inventory changed during reconstruction staging."));
	}
	if (!Result.IsSuccess())
	{
		return Result;
	}
	for (int32 Index = 0; Index < ASCs.Num(); ++Index)
	{
		Result = ASCs[Index]->ValidateStagedSourceProjection(Fences[Index], Capabilities[Index]);
		if (!Result.IsSuccess())
		{
			return Result;
		}
	}
	for (int32 Index = 0; Index < ASCs.Num(); ++Index)
	{
		Result = ASCs[Index]->CommitSourceProjection(Fences[Index], Capabilities[Index]);
		if (!Result.IsSuccess())
		{
			return Result;
		}
	}
	Result = Inventory->CommitPrepared(InventoryCapability);
	if (!Result.IsSuccess())
	{
		return Result;
	}
	InitialSkillPoints = Snapshot.InitialSkillPoints;
	InitialInventory = Snapshot.InitialInventory;
	InitialView = Snapshot.InitialView;
	ItemizationMigration = Snapshot.ItemizationMigration;
	PublishedView = MoveTemp(Candidate);
	Receipts = Snapshot.Receipts;
	for (int32 Index = 0; Index < Receipts.Num(); ++Index)
	{
		ReceiptIndices.Add(Receipts[Index].Command.RequestId, Index);
	}
	for (const FKnightRPGKnightRegistration& Registration : InRegistrations)
	{
		Registrations.Add(Registration.KnightId, Registration);
	}
	for (const FKnightRPGKnightView& Knight : PublishedView.Knights)
	{
		if (Knight.WeaponRuneInstanceId.IsValid())
		{
			Inventory->AssignedInstanceIds.Add(Knight.WeaponRuneInstanceId);
		}
		if (Knight.SkillRuneInstanceId.IsValid())
		{
			Inventory->AssignedInstanceIds.Add(Knight.SkillRuneInstanceId);
		}
		for (const FKnightEquipmentAssignment& Assignment : Knight.Assignments)
		{
			Inventory->AssignedInstanceIds.Add(Assignment.ItemInstanceId);
		}
	}
	if (PublishedView.IsItemizationEnabled())
	{
		for (const FKnightItemInstance& Item : Snapshot.Inventory.Items)
		{
			if (Item.LocationId != KnightGear::InventoryLocation)
			{
				Inventory->AssignedInstanceIds.Add(Item.InstanceId);
			}
		}
	}
	BindInventory(InInventory);
	bCommitted = true;
	RefreshReplicatedState();
	const auto CanPublish = [this, InRegistrations, &Snapshot]()
	{
		return ValidateAuthority().IsSuccess()
			&& ValidateRegistrations(InRegistrations, &Snapshot.View, true, false).IsSuccess();
	};
	for (const FKnightRPGKnightRegistration& Registration : InRegistrations)
	{
		const FKnightRPGCombatSnapshot* Fragment = Snapshot.CombatStates.Find(Registration.KnightId);
		if (Fragment != nullptr)
		{
			UKnightSkillRuntimeComponent* Runtime = Registration.Pawn->FindComponentByClass<UKnightSkillRuntimeComponent>();
			Registration.AbilitySystem->SetNumericAttributeBase(UKnightAttributeSet::GetHealthAttribute(), Fragment->Health);
			if (!CanPublish())
			{
				return FKnightResult::Success();
			}
			Registration.AbilitySystem->SetNumericAttributeBase(UKnightAttributeSet::GetStaminaAttribute(), Fragment->Stamina);
			if (!CanPublish())
			{
				return FKnightResult::Success();
			}
			Runtime->RestoreSnapshot(Fragment->Skills, false);
			if (!CanPublish())
			{
				return FKnightResult::Success();
			}
		}
	}
	for (int32 Index = 0; Index < ASCs.Num(); ++Index)
	{
		ASCs[Index]->PublishSourceProjection(Fences[Index], Capabilities[Index], true, PublicationOwner.Get());
		if (!CanPublish())
		{
			return FKnightResult::Success();
		}
	}
	InInventory->PublishPrepared(InventoryCapability, false);
	if (!CanPublish())
	{
		return FKnightResult::Success();
	}
	OnSnapshotChanged.Broadcast();
	return FKnightResult::Success();
}

void UKnightRPGComponent::RefreshReplicatedState()
{
	ReplicatedState.Owner = this;
	ReplicatedState.Records.Reset();
	FKnightRPGRecord& Control = ReplicatedState.Records.AddDefaulted_GetRef();
	Control.bIsControl = true;
	Control.Revision = PublishedView.Revision;
	Control.InventoryId = PublishedView.InventoryId;
	Control.InventoryRevision = PublishedView.InventoryRevision;
	Control.SkillTreeId = PublishedView.SkillTreeId;
	Control.Currency = PublishedView.Currency;
	Control.DemoRunId = PublishedView.DemoRunId;
	Control.bGearDemo = PublishedView.bGearDemo;
	Control.ClaimedRewards = PublishedView.ClaimedRewards;
	ReplicatedState.MarkItemDirty(Control);
	for (const FKnightRPGKnightView& Knight : PublishedView.Knights)
	{
		FKnightRPGRecord& Record = ReplicatedState.Records.AddDefaulted_GetRef();
		Record.Revision = PublishedView.Revision;
		Record.Knight = Knight;
		ReplicatedState.MarkItemDirty(Record);
	}
	ReplicatedState.MarkArrayDirty();
	GetOwner()->ForceNetUpdate();
}

void UKnightRPGComponent::HandleReplicatedStateReady()
{
	if (bEndingPlay || Inventory == nullptr || GetOwner() == nullptr || GetOwner()->HasAuthority())
	{
		return;
	}
	const FKnightRPGRecord* Control = nullptr;
	for (const FKnightRPGRecord& Record : ReplicatedState.Records)
	{
		if (Record.bIsControl)
		{
			if (Control != nullptr)
			{
				return;
			}
			Control = &Record;
		}
	}
	const FKnightInventoryView InventoryView = Inventory->GetView();
	if (Control == nullptr || Control->Revision <= PublishedView.Revision || Control->Revision < 1
		|| Control->InventoryId != InventoryView.InventoryId || Control->InventoryRevision != InventoryView.Revision
		|| !KnightIdentity::IsValidDefinitionId(Control->SkillTreeId)
		|| Control->SkillTreeId.PrimaryAssetType != FPrimaryAssetType(TEXT("KnightSkillTree")))
	{
		return;
	}
	FKnightRPGView Candidate;
	Candidate.Revision = Control->Revision;
	Candidate.InventoryId = Control->InventoryId;
	Candidate.InventoryRevision = Control->InventoryRevision;
	Candidate.SkillTreeId = Control->SkillTreeId;
	Candidate.Currency = Control->Currency;
	Candidate.DemoRunId = Control->DemoRunId;
	Candidate.bGearDemo = Control->bGearDemo;
	Candidate.ClaimedRewards = Control->ClaimedRewards;
	TSet<FKnightEntityId> KnightIds;
	TSet<FGuid> AssignedIds;
	for (const FKnightRPGRecord& Record : ReplicatedState.Records)
	{
		if (Record.bIsControl || Record.Revision != Candidate.Revision)
		{
			continue;
		}
		if (!Record.Knight.IsInitialized() || KnightIds.Contains(Record.Knight.KnightId)
			|| Record.Knight.Assignments.Num() > (Record.Knight.bGearEnabled ? 12 : 2)
			|| Record.Knight.PurchasedSkills.Num() > KnightRPG::MaximumSkills
			|| !Record.Knight.StatBonuses.Validate().IsSuccess() || !Record.Knight.RuneTuning.Validate().IsSuccess()
			|| Record.Knight.TalentLoadouts.Num() > KnightRPG::MaximumTalentLoadouts
			|| !FKnightSkillCatalog::ValidateBuild(Record.Knight.HeroDefinitionId, Record.Knight.Build).IsSuccess()
			|| (Record.Knight.Build.WeaponRune.IsNone() != !Record.Knight.WeaponRuneInstanceId.IsValid())
			|| (Record.Knight.Build.SkillRune.IsNone() != !Record.Knight.SkillRuneInstanceId.IsValid()))
		{
			return;
		}
		const FGuid AttachmentIds[] { Record.Knight.WeaponRuneInstanceId, Record.Knight.SkillRuneInstanceId };
		for (const FGuid& AttachmentId : AttachmentIds)
		{
			if (!AttachmentId.IsValid())
			{
				continue;
			}
			if (AssignedIds.Contains(AttachmentId) || !InventoryView.Items.ContainsByPredicate(
				[&AttachmentId](const FKnightItemInstance& Item)
				{
					return Item.InstanceId == AttachmentId && Item.Count == 1;
				}))
			{
				return;
			}
			AssignedIds.Add(AttachmentId);
		}
		for (const FKnightEquipmentAssignment& Assignment : Record.Knight.Assignments)
		{
			if (AssignedIds.Contains(Assignment.ItemInstanceId) || !InventoryView.Items.ContainsByPredicate(
				[&Assignment](const FKnightItemInstance& Item)
				{
					return Item.InstanceId == Assignment.ItemInstanceId && Item.DefinitionId == Assignment.DefinitionId
						&& Item.Count == 1;
				}))
			{
				return;
			}
			AssignedIds.Add(Assignment.ItemInstanceId);
		}
		KnightIds.Add(Record.Knight.KnightId);
		Candidate.Knights.Add(Record.Knight);
	}
	if (Candidate.Knights.Num() != 3)
	{
		return;
	}
	SortView(Candidate);
	PublishedView = MoveTemp(Candidate);
	OnSnapshotChanged.Broadcast();
}

#undef LOCTEXT_NAMESPACE
