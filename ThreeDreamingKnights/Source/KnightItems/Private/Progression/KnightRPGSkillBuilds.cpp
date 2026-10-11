#include "Progression/KnightRPGComponent.h"

#include "Assets/KnightDefinition.h"
#include "Definitions/KnightItemDefinition.h"
#include "Definitions/KnightRuneDefinition.h"
#include "Tags/KnightGameplayTags.h"

#define LOCTEXT_NAMESPACE "KnightRPGSkillBuilds"

FKnightResult UKnightRPGComponent::ConfigureSkillLabBuilds(
	const TMap<FPrimaryAssetId, FKnightSkillBuild>& InBuilds)
{
	const FKnightResult Authority = ValidateAuthority();
	if (!Authority.IsSuccess())
	{
		return Authority;
	}
	if (IsInitialized() || bTransactionInProgress || !InitialSkillLabBuilds.IsEmpty() || InBuilds.Num() != 3)
	{
		return FKnightResult::Failure(EKnightError::InvalidState, TEXT("SkillLabSeed"),
			LOCTEXT("InitialBuilds", "Configure exactly three initial SkillLab builds once before initialization."));
	}
	for (const TPair<FPrimaryAssetId, FKnightSkillBuild>& Entry : InBuilds)
	{
		const FKnightSkillBuild& Build = Entry.Value;
		const FKnightResult Validation = FKnightSkillCatalog::ValidateBuild(Entry.Key, Build);
		if (!Validation.IsSuccess())
		{
			return Validation;
		}
		if (Build.SpecId.IsNone() || !Build.ClassChoice.IsNone() || !Build.SpecChoice.IsNone()
			|| !Build.WeaponRune.IsNone() || !Build.SkillRune.IsNone())
		{
			return FKnightResult::Failure(EKnightError::InvalidData, TEXT("SkillLabSeed"),
				LOCTEXT("UnspentSeed", "The SkillLab initial build grants a spec and free core actions; paid choices remain unspent."));
		}
	}
	InitialSkillLabBuilds = InBuilds;
	return FKnightResult::Success();
}

FKnightResult UKnightRPGComponent::BuildSkillCandidate(FKnightRPGKnightView& Knight,
	const FKnightRPGView& Existing, const FKnightInventoryView& InventoryView,
	const FKnightRPGCommand& Command) const
{
	if (Knight.Build.SpecId.IsNone())
	{
		return FKnightResult::Failure(EKnightError::NotOwned, TEXT("SkillBuildUnlock"),
			LOCTEXT("NotUnlocked", "This campaign Knight has not unlocked the SkillLab specialization catalog."));
	}
	FKnightTalentLoadout* Preset = Knight.TalentLoadouts.FindByPredicate(
		[&Command](const FKnightTalentLoadout& Entry)
		{
			return Entry.LoadoutId == Command.TalentLoadoutId;
		});
	if (Command.Operation != EKnightRPGOperation::ApplyBuild
		&& Command.Operation != EKnightRPGOperation::NewTalentLoadout && Preset == nullptr)
	{
		return FKnightResult::Failure(EKnightError::NotFound, TEXT("TalentLoadout"),
			LOCTEXT("MissingPreset", "The exact talent loadout belongs to this Knight and must still exist."));
	}
	if (Command.Operation == EKnightRPGOperation::NewTalentLoadout
		|| Command.Operation == EKnightRPGOperation::RenameTalentLoadout
		|| Command.Operation == EKnightRPGOperation::DuplicateTalentLoadout)
	{
		if (Knight.TalentLoadouts.ContainsByPredicate([&Command](const FKnightTalentLoadout& Entry)
			{
				return Entry.Name.Equals(Command.TalentLoadoutName, ESearchCase::IgnoreCase)
					&& (Command.Operation != EKnightRPGOperation::RenameTalentLoadout
						|| Entry.LoadoutId != Command.TalentLoadoutId);
			}))
		{
			return FKnightResult::Failure(EKnightError::Conflict, TEXT("TalentLoadoutName"),
				LOCTEXT("DuplicateName", "Talent loadout names must be unique for this Knight."));
		}
	}
	if (Command.Operation == EKnightRPGOperation::NewTalentLoadout
		|| Command.Operation == EKnightRPGOperation::DuplicateTalentLoadout)
	{
		if (Knight.TalentLoadouts.Num() >= KnightRPG::MaximumTalentLoadouts
			|| Knight.TalentLoadouts.ContainsByPredicate([&Command](const FKnightTalentLoadout& Entry)
				{
					return Entry.LoadoutId == Command.RequestId;
				}))
		{
			return FKnightResult::Failure(EKnightError::CapacityExceeded, TEXT("TalentLoadouts"),
				LOCTEXT("LoadoutBound", "This Knight has reached the finite talent loadout capacity."));
		}
		FKnightTalentLoadout NewPreset;
		NewPreset.LoadoutId = Command.RequestId;
		NewPreset.Name = Command.TalentLoadoutName;
		NewPreset.Build = Preset == nullptr ? Command.ProposedBuild : Preset->Build;
		const FKnightResult Validation = FKnightSkillCatalog::ValidateBuild(Knight.HeroDefinitionId, NewPreset.Build);
		if (!Validation.IsSuccess())
		{
			return Validation;
		}
		const int32 Spent = !NewPreset.Build.ClassChoice.IsNone() + !NewPreset.Build.SpecChoice.IsNone();
		const int32 Available = Knight.SkillPoints + !Knight.Build.ClassChoice.IsNone() + !Knight.Build.SpecChoice.IsNone();
		if (Spent > Available)
		{
			return FKnightResult::Failure(EKnightError::InsufficientResources, TEXT("TalentPoints"),
				LOCTEXT("PresetPoints", "The saved talent build exceeds this Knight's shared point budget."));
		}
		Knight.TalentLoadouts.Add(MoveTemp(NewPreset));
		return FKnightResult::Success();
	}
	if (Command.Operation == EKnightRPGOperation::RenameTalentLoadout)
	{
		if (Preset->Version >= MAX_int32)
		{
			return FKnightResult::Failure(EKnightError::CapacityExceeded, TEXT("TalentLoadoutVersion"),
				LOCTEXT("VersionBound", "The talent loadout version cannot overflow."));
		}
		Preset->Name = Command.TalentLoadoutName;
		++Preset->Version;
		return FKnightResult::Success();
	}
	if (Command.Operation == EKnightRPGOperation::DeleteTalentLoadout)
	{
		Knight.TalentLoadouts.RemoveAll([&Command](const FKnightTalentLoadout& Entry)
			{
				return Entry.LoadoutId == Command.TalentLoadoutId;
			});
		if (Knight.ActiveTalentLoadoutId == Command.TalentLoadoutId)
		{
			// Deleting a selected preset removes its name, while the effective build remains equipped.
			Knight.ActiveTalentLoadoutId.Invalidate();
		}
		return FKnightResult::Success();
	}
	FKnightSkillBuild Proposed = Command.Operation == EKnightRPGOperation::ApplyBuild
		? Command.ProposedBuild : Preset->Build;
	if (Command.Operation == EKnightRPGOperation::ApplyTalentLoadout)
	{
		Proposed.WeaponRune = Knight.Build.WeaponRune;
		Proposed.SkillRune = Knight.Build.SkillRune;
	}
	const FKnightResult Validation = FKnightSkillCatalog::ValidateBuild(Knight.HeroDefinitionId, Proposed);
	if (!Validation.IsSuccess())
	{
		return Validation;
	}
	const int32 PreviousSpent = !Knight.Build.ClassChoice.IsNone() + !Knight.Build.SpecChoice.IsNone();
	const int32 ProposedSpent = !Proposed.ClassChoice.IsNone() + !Proposed.SpecChoice.IsNone();
	const int64 Remaining = static_cast<int64>(Knight.SkillPoints) + PreviousSpent - ProposedSpent;
	if (Remaining < 0 || Remaining > KnightRPG::MaximumSkillPoints)
	{
		return FKnightResult::Failure(EKnightError::InsufficientResources, TEXT("TalentPoints"),
			LOCTEXT("SharedPoints", "Class and specialization choices each cost one point from the same shared pool."));
	}
	const bool bTalentChanged = Knight.Build.SpecId != Proposed.SpecId
		|| Knight.Build.ClassChoice != Proposed.ClassChoice || Knight.Build.SpecChoice != Proposed.SpecChoice;
	Knight.SkillPoints = static_cast<int32>(Remaining);
	Knight.Build = Proposed;
	if (Command.Operation == EKnightRPGOperation::ApplyBuild)
	{
		Knight.WeaponRuneInstanceId = Command.WeaponRuneInstanceId;
		Knight.SkillRuneInstanceId = Command.SkillRuneInstanceId;
		if (bTalentChanged)
		{
			Knight.ActiveTalentLoadoutId.Invalidate();
		}
	}
	else
	{
		Knight.ActiveTalentLoadoutId = Command.TalentLoadoutId;
	}
	return ValidateBuildAttachments(Knight, Existing, InventoryView.Items);
}

FKnightResult UKnightRPGComponent::ValidateBuildAttachments(const FKnightRPGKnightView& Knight,
	const FKnightRPGView& View, const TArray<FKnightItemInstance>& Items) const
{
	const FName RuneKeys[] { Knight.Build.WeaponRune, Knight.Build.SkillRune };
	const FGuid RuneInstances[] { Knight.WeaponRuneInstanceId, Knight.SkillRuneInstanceId };
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(RuneKeys); ++Index)
	{
		if (RuneKeys[Index].IsNone())
		{
			if (RuneInstances[Index].IsValid())
			{
				return FKnightResult::Failure(EKnightError::InvalidData, TEXT("RuneAttachment"),
					LOCTEXT("EmptySocket", "An empty rune socket cannot reserve an inventory instance."));
			}
			continue;
		}
		const FKnightItemInstance* Item = Items.FindByPredicate([&RuneInstances, Index](const FKnightItemInstance& Entry)
			{
				return Entry.InstanceId == RuneInstances[Index] && Entry.Count == 1;
			});
		const UKnightDefinition* Resolved = nullptr;
		if (Item == nullptr)
		{
			return FKnightResult::Failure(EKnightError::NotOwned, TEXT("RuneAttachment"),
				LOCTEXT("OwnedRune", "Attach the exact unstackable rune instance present in shared inventory."));
		}
		const FKnightResult Resolution = ResolveDefinition(Item->DefinitionId, UKnightRuneDefinition::StaticClass(), Resolved);
		if (!Resolution.IsSuccess())
		{
			return Resolution;
		}
		const UKnightRuneDefinition* Definition = CastChecked<UKnightRuneDefinition>(Resolved);
		if (Knight.bGearEnabled && Item->LocationId != KnightGear::InventoryLocation)
		{
			const FKnightEquipmentAssignment* Weapon = Knight.FindAssignment(KnightGameplayTags::Slot_Equipment_Weapon);
			const bool bOwnWeaponSocket = Index == 0 && Item->LocationId == KnightGear::WeaponSocketLocation
				&& Weapon != nullptr && Item->ParentWeaponId == Weapon->ItemInstanceId
				&& Item->AssignedKnightId == Knight.KnightId;
			const bool bOwnSkillSocket = Index == 1 && Item->LocationId == KnightGear::SkillSocketLocation
				&& Item->AssignedKnightId == Knight.KnightId;
			if (!bOwnWeaponSocket && !bOwnSkillSocket)
			{
				return FKnightResult::Failure(EKnightError::NotOwned, TEXT("RuneParentOwnership"),
					LOCTEXT("RuneParentOwnership", "A rune already attached to another exact weapon or Knight cannot be moved implicitly."));
			}
		}
		if (Definition->GetAttachmentRuneId() != RuneKeys[Index] || !Definition->ValidateItem().IsSuccess()
			|| (Index == 0 && RuneInstances[0] == RuneInstances[1]))
		{
			return FKnightResult::Failure(EKnightError::TypeMismatch, TEXT("RuneAttachment"),
				LOCTEXT("RuneDefinition", "The source item must define this exact compatible weapon or skill rune."));
		}
		for (const FKnightRPGKnightView& Other : View.Knights)
		{
			if (Other.Assignments.ContainsByPredicate([&RuneInstances, Index](const FKnightEquipmentAssignment& Assignment)
				{
					return Assignment.ItemInstanceId == RuneInstances[Index];
				}) || (Other.KnightId != Knight.KnightId
				&& (Other.WeaponRuneInstanceId == RuneInstances[Index] || Other.SkillRuneInstanceId == RuneInstances[Index])))
			{
				return FKnightResult::Failure(EKnightError::Conflict, TEXT("RuneAttachmentOwner"),
					LOCTEXT("SingleOwner", "The rune instance is already equipped or attached to another socket."));
			}
		}
	}
	if (!Knight.Build.WeaponRune.IsNone())
	{
		const FKnightEquipmentAssignment* Weapon = Knight.FindAssignment(KnightGameplayTags::Slot_Equipment_Weapon);
		const UKnightDefinition* Resolved = nullptr;
		if (Weapon == nullptr)
		{
			return FKnightResult::Failure(EKnightError::InvalidState, TEXT("WeaponRuneSocket"),
				LOCTEXT("HeavyHammer", "Aftershock requires an equipped M5 heavy hammer."));
		}
		const FKnightResult Resolution = ResolveDefinition(Weapon->DefinitionId, UKnightItemDefinition::StaticClass(), Resolved);
		if (!Resolution.IsSuccess())
		{
			return Resolution;
		}
		if (CastChecked<UKnightItemDefinition>(Resolved)->GetWeaponFamily() != TEXT("HeavyHammer"))
		{
			return FKnightResult::Failure(EKnightError::TypeMismatch, TEXT("WeaponRuneCompatibility"),
				LOCTEXT("WeaponFamily", "Aftershock is compatible only with the HeavyHammer weapon family."));
		}
	}
	return FKnightResult::Success();
}

#undef LOCTEXT_NAMESPACE
