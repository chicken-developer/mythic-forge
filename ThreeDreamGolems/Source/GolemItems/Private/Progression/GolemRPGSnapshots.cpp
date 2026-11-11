#include "Progression/GolemRPGComponent.h"

#include "Assets/GolemAssetManager.h"
#include "Assets/GolemDefinition.h"
#include "Components/GolemAbilitySystemComponent.h"
#include "Components/GolemLifeComponent.h"
#include "Attributes/GolemAttributeSet.h"
#include "Definitions/GolemAbilitySetDefinition.h"
#include "Definitions/GolemItemDefinition.h"
#include "GameplayAbilitySpec.h"
#include "GameplayEffect.h"
#include "Inventory/GolemInventoryComponent.h"
#include "Tags/GolemGameplayTags.h"
#include "Misc/ScopeExit.h"

#define LOCTEXT_NAMESPACE "GolemRPGSnapshots"

namespace
{
	bool DeriveLegacyRuneParents(const FGolemRPGView& View, TMap<FGuid, FGuid>& OutParents)
	{
		OutParents.Reset();
		for (const FGolemRPGGolemView& Golem : View.Golems)
		{
			if (!Golem.WeaponRuneInstanceId.IsValid())
			{
				continue;
			}
			const FGolemEquipmentAssignment* Weapon = Golem.FindAssignment(GolemGameplayTags::Slot_Equipment_Weapon);
			if (Weapon == nullptr || OutParents.Contains(Golem.WeaponRuneInstanceId))
			{
				return false;
			}
			OutParents.Add(Golem.WeaponRuneInstanceId, Weapon->ItemInstanceId);
		}
		return true;
	}

	FGolemResult GatherPassiveMaximumBonuses(const FGolemGASProjectionPlan& Plan,
		FGolemItemStatBonuses& OutBonuses)
	{
		OutBonuses = FGolemItemStatBonuses();
		TArray<FPrimaryAssetId> SetIds = Plan.OptionalAbilitySetIds;
		if (Plan.OptionalAbilitySetId.IsValid())
		{
			SetIds.Add(Plan.OptionalAbilitySetId);
		}
		UGolemAssetManager* Manager = UGolemAssetManager::GetGolemAssetManager();
		for (const FPrimaryAssetId& SetId : SetIds)
		{
			UGolemDefinition* Resolved = nullptr;
			FGolemResult Result = Manager != nullptr
				? Manager->ResolveDefinition(SetId, UGolemAbilitySetDefinition::StaticClass(), Resolved)
				: FGolemResult::Failure(EGolemError::LoadFailed, TEXT("SnapshotPassiveCatalog"),
					LOCTEXT("PassiveCatalog", "The typed passive ability-set catalog is unavailable."));
			if (!Result.IsSuccess())
			{
				return Result;
			}
			const UGolemAbilitySetDefinition* Set = CastChecked<UGolemAbilitySetDefinition>(Resolved);
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
					return FGolemResult::Failure(EGolemError::InvalidData, TEXT("SnapshotPassivePolicy"),
						LOCTEXT("PassivePolicy", "Snapshot passives require the reversible source-projection effect policy."));
				}
				for (const FGameplayModifierInfo& Modifier : Effect->Modifiers)
				{
					float Magnitude = 0.0f;
					const bool bAllowedAttribute = Modifier.Attribute == UGolemAttributeSet::GetMaxHealthAttribute()
						|| Modifier.Attribute == UGolemAttributeSet::GetMaxStaminaAttribute()
						|| Modifier.Attribute == UGolemAttributeSet::GetMaxRuneEnergyAttribute()
						|| Modifier.Attribute == UGolemAttributeSet::GetAttackPowerAttribute()
						|| Modifier.Attribute == UGolemAttributeSet::GetDefenseAttribute();
					if (!bAllowedAttribute || Modifier.ModifierOp != EGameplayModOp::Additive
						|| !Modifier.ModifierMagnitude.GetStaticMagnitudeIfPossible(1.0f, Magnitude)
						|| !FMath::IsFinite(Magnitude) || Magnitude < 0.0f || Magnitude > 10000.0f)
					{
						return FGolemResult::Failure(EGolemError::InvalidData, TEXT("SnapshotPassiveMagnitude"),
							LOCTEXT("PassiveMagnitude", "Snapshot passive modifiers must be finite, static and additive at projection level one."));
					}
					if (Modifier.Attribute == UGolemAttributeSet::GetMaxHealthAttribute())
					{
						OutBonuses.MaxHealth += Magnitude;
					}
					else if (Modifier.Attribute == UGolemAttributeSet::GetMaxStaminaAttribute())
					{
						OutBonuses.MaxStamina += Magnitude;
					}
					else if (Modifier.Attribute == UGolemAttributeSet::GetMaxRuneEnergyAttribute())
					{
						OutBonuses.MaxRuneEnergy += Magnitude;
					}
				}
			}
		}
		return FGolemResult::Success();
	}
}

FGolemResult UGolemRPGComponent::MigrateSnapshot(FGolemRPGSnapshot& Snapshot)
{
	if (Snapshot.SchemaVersion == GolemRPG::SnapshotVersion)
	{
		return FGolemResult::Success();
	}
	if ((Snapshot.SchemaVersion != 1 && Snapshot.SchemaVersion != 2)
		|| (Snapshot.SchemaVersion == 1 && !Snapshot.CombatStates.IsEmpty()))
	{
		return FGolemResult::Failure(EGolemError::InvalidData, TEXT("RPGSnapshotMigration"),
			LOCTEXT("MigrationVersion", "Only the actual M5 and M5.5 schemas have supported adapters to the gear schema."));
	}
	if (!(Snapshot.ItemizationMigration == FGolemItemizationMigration())
		|| Snapshot.InitialView.bGearDemo || Snapshot.View.bGearDemo || Snapshot.InitialView.Currency != 0
		|| Snapshot.View.Currency != 0 || Snapshot.InitialView.DemoRunId.IsValid() || Snapshot.View.DemoRunId.IsValid()
		|| !Snapshot.RewardOffers.IsEmpty() || !Snapshot.View.ClaimedRewards.IsEmpty())
	{
		return FGolemResult::Failure(EGolemError::Conflict, TEXT("LegacyGearMigration"),
			LOCTEXT("LegacyGear", "Historical snapshots cannot claim gear-demo currency or reward data."));
	}
	const auto IsLegacyItem = [](const FGolemItemInstance& Item)
	{
		return Item.ItemRevision == 1 && Item.LocationId == GolemGear::InventoryLocation
			&& !Item.AssignedGolemId.IsValid() && !Item.SlotTag.IsValid() && Item.UpgradeRank == 0
			&& !Item.RuneAttachmentId.IsValid() && !Item.ParentWeaponId.IsValid() && !Item.RewardReceiptId.IsValid();
	};
	for (const FGolemInventorySnapshot* InventorySnapshot : { &Snapshot.InitialInventory, &Snapshot.Inventory })
	{
		for (const FGolemItemInstance& Item : InventorySnapshot->Items)
		{
			if (!IsLegacyItem(Item))
			{
				return FGolemResult::Failure(EGolemError::Conflict, TEXT("LegacyInventoryFields"),
					LOCTEXT("LegacyInventoryFields", "Historical items cannot claim physical locations, ranks or per-item revisions introduced by itemization."));
			}
		}
		for (const FGolemInventoryReceipt& Receipt : InventorySnapshot->Receipts)
		{
			for (const FGolemInventoryMutation& Mutation : Receipt.Request.Mutations)
			{
				if (Mutation.Operation > EGolemInventoryOperation::Remove || Mutation.ExpectedItemRevision != 0
					|| !IsLegacyItem(Mutation.Item))
				{
					return FGolemResult::Failure(EGolemError::Conflict, TEXT("LegacyInventoryJournal"),
						LOCTEXT("LegacyInventoryJournal", "Historical inventory journals retain only their original add/remove payloads."));
				}
			}
		}
	}
	for (const FGolemRPGView* View : { &Snapshot.InitialView, &Snapshot.View })
	{
		for (const FGolemRPGGolemView& Golem : View->Golems)
		{
			if (Golem.bGearEnabled || Golem.Level != 1 || !Golem.ConsumableBindings.IsEmpty())
			{
				return FGolemResult::Failure(EGolemError::Conflict, TEXT("LegacyGearFields"),
					LOCTEXT("LegacyGearFields", "Historical Golems cannot claim level or quick-binding fields introduced by gear fixtures."));
			}
		}
	}
	if (!Snapshot.LegacyWeaponRuneParents.IsEmpty()
		|| !DeriveLegacyRuneParents(Snapshot.View, Snapshot.LegacyWeaponRuneParents))
	{
		return FGolemResult::Failure(EGolemError::Conflict, TEXT("LegacyRuneParent"),
			LOCTEXT("LegacyRuneParent", "Historical attachment ownership must derive one exact equipped hammer parent."));
	}
	for (const FGolemRPGReceipt& Receipt : Snapshot.Receipts)
	{
		if (!Receipt.CommittedGolems.IsEmpty() || Receipt.CommittedCurrency != 0)
		{
			return FGolemResult::Failure(EGolemError::Conflict, TEXT("LegacyGearReceipt"),
				LOCTEXT("LegacyReceiptFields", "Historical receipts cannot claim multi-Golem or currency fields introduced by itemization."));
		}
	}
	if (Snapshot.SchemaVersion == 2)
	{
		for (const FGolemRPGReceipt& Receipt : Snapshot.Receipts)
		{
			if (Receipt.Command.Operation > EGolemRPGOperation::ApplyTalentLoadout)
			{
				return FGolemResult::Failure(EGolemError::Conflict, TEXT("LegacyGearReceipt"),
					LOCTEXT("LegacyReceipt", "M5.5 retains its original exact request and single-Golem receipt payloads."));
			}
		}
		Snapshot.InitialInventory.SchemaVersion = GolemItems::InventorySnapshotVersion;
		Snapshot.Inventory.SchemaVersion = GolemItems::InventorySnapshotVersion;
		Snapshot.SchemaVersion = GolemRPG::SnapshotVersion;
		return FGolemResult::Success();
	}
	auto IsLegacyGolem = [](const FGolemRPGGolemView& Golem)
	{
		return Golem.Build == FGolemSkillBuild() && !Golem.WeaponRuneInstanceId.IsValid()
			&& !Golem.SkillRuneInstanceId.IsValid() && Golem.TalentLoadouts.IsEmpty()
			&& !Golem.ActiveTalentLoadoutId.IsValid();
	};
	for (const FGolemRPGGolemView& Golem : Snapshot.InitialView.Golems)
	{
		if (!IsLegacyGolem(Golem))
		{
			return FGolemResult::Failure(EGolemError::Conflict, TEXT("RPGSnapshotMigration"),
				LOCTEXT("MigrationSeed", "Schema one cannot claim specialization or attachment data introduced by schema two."));
		}
	}
	for (const FGolemRPGGolemView& Golem : Snapshot.View.Golems)
	{
		if (!IsLegacyGolem(Golem))
		{
			return FGolemResult::Failure(EGolemError::Conflict, TEXT("RPGSnapshotMigration"),
				LOCTEXT("MigrationFinal", "The old M5 final state must retain the legacy build and its unchanged item identities."));
		}
	}
	for (const FGolemRPGReceipt& Receipt : Snapshot.Receipts)
	{
		if (Receipt.Command.Operation > EGolemRPGOperation::Respec || !IsLegacyGolem(Receipt.CommittedGolem))
		{
			return FGolemResult::Failure(EGolemError::Conflict, TEXT("RPGSnapshotMigration"),
				LOCTEXT("MigrationReceipt", "Schema one may only contain its original exact M5 operations and legacy receipts."));
		}
	}
	// Preserve every identity, point, paid cost, inventory value and durable request. New catalog remains locked.
	Snapshot.SchemaVersion = GolemRPG::SnapshotVersion;
	Snapshot.InitialInventory.SchemaVersion = GolemItems::InventorySnapshotVersion;
	Snapshot.Inventory.SchemaVersion = GolemItems::InventorySnapshotVersion;
	return FGolemResult::Success();
}

FGolemResult UGolemRPGComponent::CaptureSnapshot(FGolemRPGSnapshot& OutSnapshot) const
{
	OutSnapshot = FGolemRPGSnapshot();
	const FGolemResult Authority = ValidateAuthority();
	if (!Authority.IsSuccess())
	{
		return Authority;
	}
	if (!IsInitialized() || bTransactionInProgress || Inventory == nullptr || Inventory->IsReserved())
	{
		return FGolemResult::Failure(EGolemError::InvalidState, TEXT("RPGSnapshot"),
			LOCTEXT("CaptureState", "Capture requires initialized domains outside an in-progress mutation or publication."));
	}
	FGolemRPGSnapshot Candidate;
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
		return FGolemResult::Failure(EGolemError::Conflict, TEXT("LegacyRuneCapture"),
			LOCTEXT("LegacyRuneCapture", "Legacy runes require an unambiguous existing parent hammer GUID."));
	}
	for (const FGolemRPGGolemView& Golem : Candidate.View.Golems)
	{
		if (Golem.Build.SpecId.IsNone() && !Golem.bGearEnabled)
		{
			continue;
		}
		const FGolemRPGGolemRegistration* Registration = Registrations.Find(Golem.GolemId);
		AActor* Pawn = Registration == nullptr ? nullptr : Registration->Pawn.Get();
		const UGolemAbilitySystemComponent* ASC = Registration == nullptr ? nullptr : Registration->AbilitySystem.Get();
		const UGolemAttributeSet* Attributes = ASC == nullptr ? nullptr : ASC->GetGolemAttributes();
		const UGolemSkillRuntimeComponent* Runtime = Pawn == nullptr
			? nullptr : Pawn->FindComponentByClass<UGolemSkillRuntimeComponent>();
		if (Runtime == nullptr || Attributes == nullptr || ASC->GetLifeComponent() == nullptr
			|| !ASC->GetLifeComponent()->IsAlive() || Attributes->GetHealth() <= 0.0f
			|| Runtime->HasUnsnapshottedEffects()
			|| ASC->GetActivatableAbilities().ContainsByPredicate([](const FGameplayAbilitySpec& Spec)
			{
				return Spec.IsActive();
			}))
		{
			return FGolemResult::Failure(EGolemError::InvalidState, TEXT("SkillSnapshot"),
				LOCTEXT("SnapshotDebt", "Capture living idle SkillLab Golems after debt, shields, periodic streams and world pickup effects expire."));
		}
		FGolemRPGCombatSnapshot Fragment;
		Fragment.Skills = Runtime->CaptureSnapshot();
		Fragment.Health = Attributes->GetHealth();
		Fragment.Stamina = Attributes->GetStamina();
		Candidate.CombatStates.Add(Golem.GolemId, MoveTemp(Fragment));
	}
	const FGolemResult CaptureResult = Inventory->CaptureSnapshot(Candidate.Inventory);
	if (!CaptureResult.IsSuccess())
	{
		return CaptureResult;
	}
	FGolemRPGView Reconstructed;
	const FGolemResult Validation = ValidateSnapshot(Candidate, Reconstructed);
	if (Validation.IsSuccess())
	{
		OutSnapshot = MoveTemp(Candidate);
	}
	return Validation;
}

FGolemResult UGolemRPGComponent::ValidateSnapshot(const FGolemRPGSnapshot& Snapshot, FGolemRPGView& OutView) const
{
	OutView = FGolemRPGView();
	if (Inventory == nullptr || Snapshot.SchemaVersion != GolemRPG::SnapshotVersion
		|| Snapshot.InitialSkillPoints < 0 || Snapshot.InitialSkillPoints > GolemRPG::MaximumSkillPoints
		|| !Snapshot.InitialView.IsInitialized() || Snapshot.InitialView.Revision != 1
		|| Snapshot.InitialView.IsItemizationEnabled() != Snapshot.InitialView.bGearDemo
		|| !Snapshot.View.IsInitialized() || Snapshot.View.Revision != static_cast<int64>(Snapshot.Receipts.Num()) + 1
			+ (Snapshot.ItemizationMigration.IsPresent() ? 1 : 0)
		|| Snapshot.Receipts.Num() > GolemItems::MaximumTransactionReceipts
		|| Snapshot.InitialView.SkillTreeId != Snapshot.View.SkillTreeId
		|| Snapshot.InitialInventory.InventoryId != Snapshot.Inventory.InventoryId
		|| Snapshot.InitialInventory.Capacity != Snapshot.Inventory.Capacity
		|| Snapshot.InitialView.InventoryId != Snapshot.InitialInventory.InventoryId
		|| Snapshot.InitialView.InventoryRevision != Snapshot.InitialInventory.Revision
		|| Snapshot.View.InventoryId != Snapshot.Inventory.InventoryId
		|| Snapshot.View.InventoryRevision != Snapshot.Inventory.Revision)
	{
		return FGolemResult::Failure(EGolemError::InvalidData, TEXT("RPGRestore"),
			LOCTEXT("Header", "The RPG snapshot has an invalid schema, identity, initial state, revision or journal bound."));
	}
	const FGolemItemizationMigration& Migration = Snapshot.ItemizationMigration;
	if (Migration.IsPresent() ? Migration.LegacyReceiptCount < 0
		|| Migration.LegacyReceiptCount > Snapshot.Receipts.Num() || Snapshot.InitialView.IsItemizationEnabled()
		|| !Snapshot.View.IsItemizationEnabled() || Snapshot.View.bGearDemo
		|| Migration.InventoryRequest.RequestId != Migration.MigrationId
		|| Snapshot.InitialInventory.Receipts.ContainsByPredicate([&Migration](const FGolemInventoryReceipt& Receipt)
		{
			return Receipt.Request.RequestId == Migration.MigrationId;
		})
		: !(Migration == FGolemItemizationMigration()))
	{
		return FGolemResult::Failure(EGolemError::InvalidData, TEXT("ItemizationBoundary"),
			LOCTEXT("ItemizationBoundary", "The optional itemization boundary must identify one exact legacy journal prefix."));
	}
	if (Snapshot.InitialView.bGearDemo != Snapshot.View.bGearDemo
		|| Snapshot.InitialView.DemoRunId != Snapshot.View.DemoRunId
		|| !Snapshot.InitialView.ClaimedRewards.IsEmpty()
		|| (!Snapshot.View.bGearDemo && !Snapshot.RewardOffers.IsEmpty()))
	{
		return FGolemResult::Failure(EGolemError::Conflict, TEXT("GearSnapshotScope"),
			LOCTEXT("GearSnapshotScope", "The complete snapshot retains one fixture scope and starts before any reward claim."));
	}
	TSet<FGuid> OfferIds;
	for (const FGolemGearRewardBundle& Offer : Snapshot.RewardOffers)
	{
		if (!Offer.RewardId.IsValid() || OfferIds.Contains(Offer.RewardId) || Offer.DemoRunId != Snapshot.View.DemoRunId
			|| Offer.Currency < 0 || Offer.Currency > GolemGear::MaximumCurrency || Offer.Level < 1 || Offer.Level > 8
			|| (Offer.Items.IsEmpty() && Offer.BoundItems.IsEmpty()) || Offer.Items.Num() > 41
			|| Offer.BoundItems.Num() > 3)
		{
			return FGolemResult::Failure(EGolemError::InvalidData, TEXT("SnapshotRewardOffer"),
				LOCTEXT("SnapshotRewardOffer", "Every persisted reward is one complete bounded offer for the retained demo generation."));
		}
		OfferIds.Add(Offer.RewardId);
		TSet<FGuid> BoundIds;
		for (const FGuid& BoundId : Offer.BoundItems)
		{
			if (!BoundId.IsValid() || BoundIds.Contains(BoundId))
			{
				return FGolemResult::Failure(EGolemError::InvalidData, TEXT("SnapshotRewardBinding"),
					LOCTEXT("SnapshotRewardBinding", "Retained starter bindings require distinct exact instance IDs."));
			}
			BoundIds.Add(BoundId);
		}
		TSet<FPrimaryAssetId> DefinitionIds;
		for (const FGolemGearRewardItem& Item : Offer.Items)
		{
			const UGolemDefinition* Definition = nullptr;
			if (Item.Count < 1 || Item.Count > GolemItems::MaximumStackCount || DefinitionIds.Contains(Item.DefinitionId)
				|| !ResolveDefinition(Item.DefinitionId, UGolemItemDefinition::StaticClass(), Definition).IsSuccess())
			{
				return FGolemResult::Failure(EGolemError::InvalidData, TEXT("SnapshotRewardItem"),
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
			return FGolemResult::Failure(EGolemError::Conflict, TEXT("SnapshotRewardClaim"),
				LOCTEXT("SnapshotRewardClaim", "Claimed reward IDs must name distinct retained authoritative offers."));
		}
		ClaimedIds.Add(Id);
	}
	TMap<FGuid, FGuid> LegacyParents;
	if ((Snapshot.View.IsItemizationEnabled() && !Snapshot.LegacyWeaponRuneParents.IsEmpty())
		|| (!Snapshot.View.IsItemizationEnabled() && (!DeriveLegacyRuneParents(Snapshot.View, LegacyParents)
			|| !LegacyParents.OrderIndependentCompareEqual(Snapshot.LegacyWeaponRuneParents))))
	{
		return FGolemResult::Failure(EGolemError::Conflict, TEXT("SnapshotRuneAdapter"),
			LOCTEXT("SnapshotRuneAdapter", "The legacy parent adapter must agree with confirmed exact equipment ownership."));
	}
	TArray<FGolemItemInstance> InitialItems;
	TSet<FGuid> InitialIds;
	TMap<FGuid, int32> InitialIndices;
	FGolemResult Result = Inventory->ValidateSnapshot(Snapshot.InitialInventory, InitialItems, InitialIds, InitialIndices);
	if (!Result.IsSuccess())
	{
		return Result;
	}
	TArray<FGolemItemInstance> FinalItems;
	TSet<FGuid> FinalIds;
	TMap<FGuid, int32> FinalIndices;
	Result = Inventory->ValidateSnapshot(Snapshot.Inventory, FinalItems, FinalIds, FinalIndices);
	if (!Result.IsSuccess())
	{
		return Result;
	}
	if (Snapshot.InitialInventory.Receipts.Num() > Snapshot.Inventory.Receipts.Num())
	{
		return FGolemResult::Failure(EGolemError::Conflict, TEXT("RPGRestore"),
			LOCTEXT("InventoryPrefixCount", "The initial inventory journal cannot be longer than its final journal."));
	}
	for (int32 Index = 0; Index < Snapshot.InitialInventory.Receipts.Num(); ++Index)
	{
		if (!(Snapshot.InitialInventory.Receipts[Index] == Snapshot.Inventory.Receipts[Index]))
		{
			return FGolemResult::Failure(EGolemError::Conflict, TEXT("RPGRestore"),
				LOCTEXT("InventoryPrefix", "The complete initial inventory history must be an unchanged prefix of the final history."));
		}
	}
	FGolemRPGView Current = Snapshot.InitialView;
	TSet<FGolemEntityId> GolemIds;
	for (const FGolemRPGGolemView& Golem : Current.Golems)
	{
		if (!Golem.IsInitialized() || Golem.Revision != 1 || Golem.SkillPoints != Snapshot.InitialSkillPoints
			|| (!Current.bGearDemo && (!Golem.Assignments.IsEmpty() || !Golem.StatBonuses.IsEmpty()))
			|| !Golem.PurchasedSkills.IsEmpty()
			|| !(Golem.RuneTuning == FGolemRuneTuning()) || GolemIds.Contains(Golem.GolemId)
			|| !Golem.Build.ClassChoice.IsNone() || !Golem.Build.SpecChoice.IsNone()
			|| !Golem.Build.WeaponRune.IsNone() || !Golem.Build.SkillRune.IsNone()
			|| Golem.WeaponRuneInstanceId.IsValid() || Golem.SkillRuneInstanceId.IsValid()
			|| !Golem.TalentLoadouts.IsEmpty() || Golem.ActiveTalentLoadoutId.IsValid())
		{
			return FGolemResult::Failure(EGolemError::InvalidData, TEXT("RPGRestore"),
				LOCTEXT("InitialGolem", "Initial canonical RPG values must be distinct fresh Golems with the declared initial points."));
		}
		if (!Golem.Build.SpecId.IsNone()
			&& !FGolemSkillCatalog::ValidateBuild(Golem.HeroDefinitionId, Golem.Build).IsSuccess())
		{
			return FGolemResult::Failure(EGolemError::InvalidData, TEXT("SkillSnapshotSeed"),
				LOCTEXT("InitialSpec", "The initial unlocked spec must belong to its unchanged canonical hero identity."));
		}
		GolemIds.Add(Golem.GolemId);
	}
	if (Current.bGearDemo)
	{
		Result = ValidateGearLocations(Current, InitialItems);
		if (!Result.IsSuccess())
		{
			return Result;
		}
		for (FGolemRPGGolemView& Golem : Current.Golems)
		{
			if (!Golem.bGearEnabled || (Golem.Level != 1 && Golem.Level != 8))
			{
				return FGolemResult::Failure(EGolemError::InvalidData, TEXT("GearInitialLevel"),
					LOCTEXT("GearInitialLevel", "Gear fixtures start at level one or eight with explicit quick bindings."));
			}
			FGolemGASProjectionPlan Plan;
			Result = BuildProjection(Golem, InitialItems, Plan);
			if (!Result.IsSuccess())
			{
				return Result;
			}
		}
	}
	SortView(Current);
	if (!(Current == Snapshot.InitialView))
	{
		return FGolemResult::Failure(EGolemError::InvalidData, TEXT("RPGRestore"),
			LOCTEXT("CanonicalOrder", "Snapshot values must use the canonical deterministic Golem and slot ordering."));
	}
	FGolemInventoryView CurrentInventory;
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
			FGolemRPGView Migrated;
			FGolemInventoryRequest MigrationRequest;
			Result = BuildItemizationMigration(Current, CurrentInventory, Migration.MigrationId,
				Migrated, MigrationRequest);
			if (!Result.IsSuccess())
			{
				return Result;
			}
			if (!(MigrationRequest == Migration.InventoryRequest))
			{
				return FGolemResult::Failure(EGolemError::Conflict, TEXT("ItemizationPayload"),
					LOCTEXT("ItemizationPayload", "The migration locations must derive exactly from the verified legacy prefix."));
			}
			if (!MigrationRequest.Mutations.IsEmpty())
			{
				if (!Snapshot.Inventory.Receipts.IsValidIndex(InventoryReceiptIndex)
					|| !(Snapshot.Inventory.Receipts[InventoryReceiptIndex].Request == MigrationRequest))
				{
					return FGolemResult::Failure(EGolemError::Conflict, TEXT("ItemizationInventoryHistory"),
						LOCTEXT("ItemizationInventoryHistory", "The administrative locations must match the next exact inventory receipt."));
				}
				TArray<FGolemItemInstance> MigratedItems;
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
		const FGolemRPGReceipt& Receipt = Snapshot.Receipts[ReceiptIndex];
		Result = Receipt.Command.ValidateShape();
		if (!Result.IsSuccess())
		{
			return Result;
		}
		if (!Receipt.HumanId.IsValid() || RequestIds.Contains(Receipt.Command.RequestId))
		{
			return FGolemResult::Failure(EGolemError::DuplicateId, TEXT("RPGRestore"),
				LOCTEXT("ReceiptIdentity", "Successful RPG receipts require valid human identities and unique exact request GUIDs."));
		}
		RequestIds.Add(Receipt.Command.RequestId);
		const bool bPickup = Receipt.Command.Operation == EGolemRPGOperation::Pickup;
		if ((bPickup && PickedIds.Contains(Receipt.Command.PickupId))
			|| (!bPickup && !(Receipt.PickedItem == FGolemItemInstance())))
		{
			return FGolemResult::Failure(EGolemError::Conflict, TEXT("RPGRestore"),
				LOCTEXT("PickupHistory", "Each pickup commits once, and unrelated commands cannot contain historical picked items."));
		}
		if (bPickup)
		{
			PickedIds.Add(Receipt.Command.PickupId);
		}
		FGolemRPGView Candidate;
		FGolemInventoryRequest DerivedRequest;
		FGuid Affected;
		Result = BuildCandidate(Current, CurrentInventory, Receipt.Command,
			bPickup ? &Receipt.PickedItem : nullptr, &Receipt.InventoryRequest.Mutations, Candidate, DerivedRequest, Affected);
		if (!Result.IsSuccess())
		{
			return Result;
		}
		if (!(DerivedRequest == Receipt.InventoryRequest) || Affected != Receipt.AffectedInstanceId)
		{
			return FGolemResult::Failure(EGolemError::Conflict, TEXT("RPGRestore"),
				LOCTEXT("ReceiptPayload", "The retained derived inventory mutation or affected identity disagrees with the actual command."));
		}
		if (!DerivedRequest.Mutations.IsEmpty())
		{
			if (!Snapshot.Inventory.Receipts.IsValidIndex(InventoryReceiptIndex)
				|| !(Snapshot.Inventory.Receipts[InventoryReceiptIndex].Request == DerivedRequest))
			{
				return FGolemResult::Failure(EGolemError::Conflict, TEXT("RPGRestore"),
					LOCTEXT("CostJournal", "Every RPG cost or pickup must match the next exact successful inventory receipt."));
			}
			TArray<FGolemItemInstance> CandidateItems;
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
		FGolemRPGGolemView* Golem = Candidate.Golems.FindByPredicate([&Receipt](const FGolemRPGGolemView& Entry)
			{
				return Entry.GolemId == Receipt.Command.ExpectedGolemId;
			});
		FGolemGASProjectionPlan Plan;
		Result = BuildProjection(*Golem, CurrentInventory.Items, Plan);
		if (Result.IsSuccess())
		{
			Result = ValidateBuildAttachments(*Golem, Candidate, CurrentInventory.Items);
		}
		if (!Result.IsSuccess())
		{
			return Result;
		}
		if (Candidate.IsItemizationEnabled())
		{
			TArray<FGolemRPGGolemView> CommittedGolems;
			for (FGolemRPGGolemView& Changed : Candidate.Golems)
			{
				const FGolemRPGGolemView* Previous = Current.Golems.FindByPredicate(
					[&Changed](const FGolemRPGGolemView& Entry) { return Entry.GolemId == Changed.GolemId; });
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
				CommittedGolems.Add(Changed);
			}
			if (CommittedGolems != Receipt.CommittedGolems || Candidate.Currency != Receipt.CommittedCurrency
				|| !ValidateGearLocations(Candidate, CurrentInventory.Items).IsSuccess())
			{
				return FGolemResult::Failure(EGolemError::Conflict, TEXT("GearReceiptState"),
					LOCTEXT("GearReceiptState", "All affected Golem states, exact locations and currency must match the retained receipt."));
			}
			if (Receipt.Command.Operation == EGolemRPGOperation::ClaimReward)
			{
				const FGolemGearRewardBundle* Bundle = Snapshot.RewardOffers.FindByPredicate(
					[&Receipt](const FGolemGearRewardBundle& Entry) { return Entry.RewardId == Receipt.Command.RewardId; });
				if (Bundle == nullptr || !(*Bundle == Receipt.Reward))
				{
					return FGolemResult::Failure(EGolemError::Conflict, TEXT("GearRewardHistory"),
						LOCTEXT("GearRewardHistory", "The claimed reward retains its exact registered bundle and identity."));
				}
			}
		}
		if (!(*Golem == Receipt.CommittedGolem) || CurrentInventory.Revision != Receipt.CommittedInventoryRevision)
		{
			return FGolemResult::Failure(EGolemError::Conflict, TEXT("RPGRestore"),
				LOCTEXT("GolemReceipt", "The retained Golem points, assignments, paid costs or derived stats disagree with journal reconstruction."));
		}
		Current = MoveTemp(Candidate);
	}
	if (InventoryReceiptIndex != Snapshot.Inventory.Receipts.Num() || CurrentInventory.Items != FinalItems
		|| CurrentInventory.Revision != Snapshot.Inventory.Revision || !(Current == Snapshot.View))
	{
		return FGolemResult::Failure(EGolemError::Conflict, TEXT("RPGRestore"),
			LOCTEXT("FinalState", "The complete reconstructed RPG and inventory values disagree with the claimed final snapshot."));
	}
	int32 ActiveBuilds = 0;
	for (const FGolemRPGGolemView& Golem : Current.Golems)
	{
		if (Golem.Build.SpecId.IsNone() && !Golem.bGearEnabled)
		{
			continue;
		}
		++ActiveBuilds;
		const FGolemRPGCombatSnapshot* Fragment = Snapshot.CombatStates.Find(Golem.GolemId);
		if (Fragment == nullptr || !FMath::IsFinite(Fragment->Health) || Fragment->Health <= 0.0f
			|| !FMath::IsFinite(Fragment->Stamina) || Fragment->Stamina < 0.0f
			|| !FMath::IsFinite(Fragment->Skills.Resource) || Fragment->Skills.Resource < 0.0f
			|| Fragment->Skills.Resource > 100000.0f || Fragment->Skills.CooldownRemaining.Num() != 4
			|| Fragment->Skills.Stacks < 0 || Fragment->Skills.Stacks > 5
			|| !UGolemSkillRuntimeComponent::ValidateSnapshot(Fragment->Skills).IsSuccess())
		{
			return FGolemResult::Failure(EGolemError::InvalidData, TEXT("SkillSnapshot"),
				LOCTEXT("CombatFragment", "Every unlocked Golem needs positive health, finite stamina/resource and four cooldown groups."));
		}
	}
	if (Snapshot.CombatStates.Num() != ActiveBuilds)
	{
		return FGolemResult::Failure(EGolemError::InvalidData, TEXT("SkillSnapshotIdentity"),
			LOCTEXT("CombatIds", "Combat fragments must match the exact unlocked canonical Golem identities."));
	}
	OutView = MoveTemp(Current);
	return FGolemResult::Success();
}

FGolemResult UGolemRPGComponent::TryRestoreSnapshot(UGolemInventoryComponent* InInventory,
	const FGolemRPGSnapshot& InSnapshot, TConstArrayView<FGolemRPGGolemRegistration> InRegistrationView)
{
	return RestoreSnapshot(InInventory, InSnapshot, InRegistrationView, FGuid());
}

FGolemResult UGolemRPGComponent::TryRestoreItemizationSnapshot(UGolemInventoryComponent* InInventory,
	const FGolemRPGSnapshot& Snapshot, TConstArrayView<FGolemRPGGolemRegistration> InRegistrations,
	const FGuid& MigrationId)
{
	if (!MigrationId.IsValid())
	{
		return FGolemResult::Failure(EGolemError::InvalidId, TEXT("ItemizationMigrationId"),
			LOCTEXT("ItemizationMigrationId", "Explicit itemization import requires a new stable administrative request identity."));
	}
	return RestoreSnapshot(InInventory, Snapshot, InRegistrations, MigrationId);
}

FGolemResult UGolemRPGComponent::RestoreSnapshot(UGolemInventoryComponent* InInventory,
	const FGolemRPGSnapshot& InSnapshot, TConstArrayView<FGolemRPGGolemRegistration> InRegistrationView,
	const FGuid& MigrationId)
{
	FGolemRPGSnapshot Snapshot = InSnapshot;
	TArray<FGolemRPGGolemRegistration> RestoreRegistrations;
	RestoreRegistrations.Append(InRegistrationView.GetData(), InRegistrationView.Num());
	const TConstArrayView<FGolemRPGGolemRegistration> InRegistrations(RestoreRegistrations);
	FGolemResult Result = ValidateAuthority();
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
		return FGolemResult::Failure(EGolemError::InvalidState, TEXT("RPGRestore"),
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
	const TArray<FGolemGearRewardBundle> PreviousOffers = RewardOffers;
	ON_SCOPE_EXIT
	{
		if (!IsInitialized())
		{
			RewardOffers = PreviousOffers;
		}
	};
	Inventory = InInventory;
	RewardOffers = Snapshot.RewardOffers;
	FGolemRPGView Candidate;
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
		for (const FGolemRPGGolemRegistration& Registration : InRegistrations)
		{
			if (Snapshot.CombatStates.Contains(Registration.GolemId))
			{
				continue;
			}
			const UGolemAttributeSet* Attributes = Registration.AbilitySystem->GetGolemAttributes();
			const UGolemSkillRuntimeComponent* Runtime = Registration.Pawn->FindComponentByClass<UGolemSkillRuntimeComponent>();
			if (Attributes == nullptr || Runtime == nullptr)
			{
				Inventory = nullptr;
				return FGolemResult::Failure(EGolemError::InvalidState, TEXT("ItemizationRuntime"),
					LOCTEXT("ItemizationRuntime", "Fresh itemization owners require the native equipment runtime and attribute set."));
			}
			// Schema one has no combat fragment. Retain the existing fresh-owner baseline policy without a refill.
			FGolemRPGCombatSnapshot Fragment;
			Fragment.Health = Attributes->GetHealth();
			Fragment.Stamina = Attributes->GetStamina();
			Fragment.Skills = Runtime->CaptureSnapshot();
			Snapshot.CombatStates.Add(Registration.GolemId, MoveTemp(Fragment));
		}
		Result = ValidateSnapshot(Snapshot, Candidate);
		if (!Result.IsSuccess())
		{
			Inventory = nullptr;
			return Result;
		}
	}
	for (const FGolemRPGGolemRegistration& Registration : InRegistrations)
	{
		FGolemRPGCombatSnapshot* Fragment = Snapshot.CombatStates.Find(Registration.GolemId);
		if (Fragment == nullptr)
		{
			continue;
		}
		FGolemRPGGolemView* Golem = Candidate.Golems.FindByPredicate(
			[&Registration](const FGolemRPGGolemView& Entry)
			{
				return Entry.GolemId == Registration.GolemId;
			});
		const UGolemAttributeSet* Attributes = Registration.AbilitySystem->GetGolemAttributes();
		UGolemSkillRuntimeComponent* Runtime = Registration.Pawn->FindComponentByClass<UGolemSkillRuntimeComponent>();
		FGolemGASProjectionPlan Plan;
		FGolemItemStatBonuses PassiveBonuses;
		Result = Golem != nullptr ? BuildProjection(*Golem, Snapshot.Inventory.Items, Plan)
			: FGolemResult::Failure(EGolemError::InvalidId, TEXT("SnapshotPassiveGolem"),
				LOCTEXT("PassiveGolem", "The passive capacity owner must match its canonical Golem."));
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
				Attributes->GetMaxHealth() + Golem->StatBonuses.MaxHealth + PassiveBonuses.MaxHealth);
			Fragment->Stamina = FMath::Min(Fragment->Stamina,
				Attributes->GetMaxStamina() + Golem->StatBonuses.MaxStamina + PassiveBonuses.MaxStamina);
			Fragment->Skills.Resource = FMath::Min(Fragment->Skills.Resource,
				Attributes->GetMaxRuneEnergy() + Golem->StatBonuses.MaxRuneEnergy + PassiveBonuses.MaxRuneEnergy);
		}
		// Flat item/class bonuses and optional passive effects are independent source contributions.
		if (Runtime == nullptr || Attributes == nullptr
			|| Fragment->Health > Attributes->GetMaxHealth() + Golem->StatBonuses.MaxHealth + PassiveBonuses.MaxHealth
			|| Fragment->Stamina > Attributes->GetMaxStamina() + Golem->StatBonuses.MaxStamina + PassiveBonuses.MaxStamina
			|| Fragment->Skills.Resource > Attributes->GetMaxRuneEnergy()
				+ Golem->StatBonuses.MaxRuneEnergy + PassiveBonuses.MaxRuneEnergy)
		{
			Inventory = nullptr;
			return FGolemResult::Failure(EGolemError::InvalidData, TEXT("SkillSnapshotCapacity"),
				LOCTEXT("RestoreCapacity", "Combat values must fit the reconstructed build and its matching live runtime component."));
		}
	}
	bTransactionInProgress = true;
	FGolemInventoryCapability InventoryCapability;
	TArray<FGolemMutationFence> Fences;
	TArray<FGolemProjectionCapability> Capabilities;
	TArray<UGolemAbilitySystemComponent*> ASCs;
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
	for (const FGolemRPGGolemRegistration& Registration : InRegistrations)
	{
		UGolemAbilitySystemComponent* ASC = Registration.AbilitySystem.Get();
		ASCs.Add(ASC);
		Fences.AddDefaulted();
		Capabilities.AddDefaulted();
		FGolemRPGGolemView* Golem = Candidate.Golems.FindByPredicate([&Registration](const FGolemRPGGolemView& Entry)
			{
				return Entry.GolemId == Registration.GolemId;
			});
		FGolemGASProjectionPlan Plan;
		Result = BuildProjection(*Golem, Snapshot.Inventory.Items, Plan);
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
	FGolemInventoryView RevalidatedInventory;
	if (Result.IsSuccess())
	{
		Result = InInventory->GetPreparedView(InventoryCapability, RevalidatedInventory);
	}
	if (Result.IsSuccess() && (RevalidatedInventory.InventoryId != Snapshot.Inventory.InventoryId
		|| RevalidatedInventory.Revision != Snapshot.Inventory.Revision
		|| RevalidatedInventory.Capacity != Snapshot.Inventory.Capacity
		|| RevalidatedInventory.Items != Snapshot.Inventory.Items))
	{
		Result = FGolemResult::Failure(EGolemError::Conflict, TEXT("RPGRestoreStage"),
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
	for (const FGolemRPGGolemRegistration& Registration : InRegistrations)
	{
		Registrations.Add(Registration.GolemId, Registration);
	}
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
	if (PublishedView.IsItemizationEnabled())
	{
		for (const FGolemItemInstance& Item : Snapshot.Inventory.Items)
		{
			if (Item.LocationId != GolemGear::InventoryLocation)
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
	for (const FGolemRPGGolemRegistration& Registration : InRegistrations)
	{
		const FGolemRPGCombatSnapshot* Fragment = Snapshot.CombatStates.Find(Registration.GolemId);
		if (Fragment != nullptr)
		{
			UGolemSkillRuntimeComponent* Runtime = Registration.Pawn->FindComponentByClass<UGolemSkillRuntimeComponent>();
			Registration.AbilitySystem->SetNumericAttributeBase(UGolemAttributeSet::GetHealthAttribute(), Fragment->Health);
			if (!CanPublish())
			{
				return FGolemResult::Success();
			}
			Registration.AbilitySystem->SetNumericAttributeBase(UGolemAttributeSet::GetStaminaAttribute(), Fragment->Stamina);
			if (!CanPublish())
			{
				return FGolemResult::Success();
			}
			Runtime->RestoreSnapshot(Fragment->Skills, false);
			if (!CanPublish())
			{
				return FGolemResult::Success();
			}
		}
	}
	for (int32 Index = 0; Index < ASCs.Num(); ++Index)
	{
		ASCs[Index]->PublishSourceProjection(Fences[Index], Capabilities[Index], true, PublicationOwner.Get());
		if (!CanPublish())
		{
			return FGolemResult::Success();
		}
	}
	InInventory->PublishPrepared(InventoryCapability, false);
	if (!CanPublish())
	{
		return FGolemResult::Success();
	}
	OnSnapshotChanged.Broadcast();
	return FGolemResult::Success();
}

void UGolemRPGComponent::RefreshReplicatedState()
{
	ReplicatedState.Owner = this;
	ReplicatedState.Records.Reset();
	FGolemRPGRecord& Control = ReplicatedState.Records.AddDefaulted_GetRef();
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
	for (const FGolemRPGGolemView& Golem : PublishedView.Golems)
	{
		FGolemRPGRecord& Record = ReplicatedState.Records.AddDefaulted_GetRef();
		Record.Revision = PublishedView.Revision;
		Record.Golem = Golem;
		ReplicatedState.MarkItemDirty(Record);
	}
	ReplicatedState.MarkArrayDirty();
	GetOwner()->ForceNetUpdate();
}

void UGolemRPGComponent::HandleReplicatedStateReady()
{
	if (bEndingPlay || Inventory == nullptr || GetOwner() == nullptr || GetOwner()->HasAuthority())
	{
		return;
	}
	const FGolemRPGRecord* Control = nullptr;
	for (const FGolemRPGRecord& Record : ReplicatedState.Records)
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
	const FGolemInventoryView InventoryView = Inventory->GetView();
	if (Control == nullptr || Control->Revision <= PublishedView.Revision || Control->Revision < 1
		|| Control->InventoryId != InventoryView.InventoryId || Control->InventoryRevision != InventoryView.Revision
		|| !GolemIdentity::IsValidDefinitionId(Control->SkillTreeId)
		|| Control->SkillTreeId.PrimaryAssetType != FPrimaryAssetType(TEXT("GolemSkillTree")))
	{
		return;
	}
	FGolemRPGView Candidate;
	Candidate.Revision = Control->Revision;
	Candidate.InventoryId = Control->InventoryId;
	Candidate.InventoryRevision = Control->InventoryRevision;
	Candidate.SkillTreeId = Control->SkillTreeId;
	Candidate.Currency = Control->Currency;
	Candidate.DemoRunId = Control->DemoRunId;
	Candidate.bGearDemo = Control->bGearDemo;
	Candidate.ClaimedRewards = Control->ClaimedRewards;
	TSet<FGolemEntityId> GolemIds;
	TSet<FGuid> AssignedIds;
	for (const FGolemRPGRecord& Record : ReplicatedState.Records)
	{
		if (Record.bIsControl || Record.Revision != Candidate.Revision)
		{
			continue;
		}
		if (!Record.Golem.IsInitialized() || GolemIds.Contains(Record.Golem.GolemId)
			|| Record.Golem.Assignments.Num() > (Record.Golem.bGearEnabled ? 12 : 2)
			|| Record.Golem.PurchasedSkills.Num() > GolemRPG::MaximumSkills
			|| !Record.Golem.StatBonuses.Validate().IsSuccess() || !Record.Golem.RuneTuning.Validate().IsSuccess()
			|| Record.Golem.TalentLoadouts.Num() > GolemRPG::MaximumTalentLoadouts
			|| !FGolemSkillCatalog::ValidateBuild(Record.Golem.HeroDefinitionId, Record.Golem.Build).IsSuccess()
			|| (Record.Golem.Build.WeaponRune.IsNone() != !Record.Golem.WeaponRuneInstanceId.IsValid())
			|| (Record.Golem.Build.SkillRune.IsNone() != !Record.Golem.SkillRuneInstanceId.IsValid()))
		{
			return;
		}
		const FGuid AttachmentIds[] { Record.Golem.WeaponRuneInstanceId, Record.Golem.SkillRuneInstanceId };
		for (const FGuid& AttachmentId : AttachmentIds)
		{
			if (!AttachmentId.IsValid())
			{
				continue;
			}
			if (AssignedIds.Contains(AttachmentId) || !InventoryView.Items.ContainsByPredicate(
				[&AttachmentId](const FGolemItemInstance& Item)
				{
					return Item.InstanceId == AttachmentId && Item.Count == 1;
				}))
			{
				return;
			}
			AssignedIds.Add(AttachmentId);
		}
		for (const FGolemEquipmentAssignment& Assignment : Record.Golem.Assignments)
		{
			if (AssignedIds.Contains(Assignment.ItemInstanceId) || !InventoryView.Items.ContainsByPredicate(
				[&Assignment](const FGolemItemInstance& Item)
				{
					return Item.InstanceId == Assignment.ItemInstanceId && Item.DefinitionId == Assignment.DefinitionId
						&& Item.Count == 1;
				}))
			{
				return;
			}
			AssignedIds.Add(Assignment.ItemInstanceId);
		}
		GolemIds.Add(Record.Golem.GolemId);
		Candidate.Golems.Add(Record.Golem);
	}
	if (Candidate.Golems.Num() != 3)
	{
		return;
	}
	SortView(Candidate);
	PublishedView = MoveTemp(Candidate);
	OnSnapshotChanged.Broadcast();
}

#undef LOCTEXT_NAMESPACE
