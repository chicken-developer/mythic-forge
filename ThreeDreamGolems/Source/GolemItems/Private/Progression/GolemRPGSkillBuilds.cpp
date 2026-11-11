#include "Progression/GolemRPGComponent.h"

#include "Assets/GolemDefinition.h"
#include "Definitions/GolemItemDefinition.h"
#include "Definitions/GolemRuneDefinition.h"
#include "Tags/GolemGameplayTags.h"

#define LOCTEXT_NAMESPACE "GolemRPGSkillBuilds"

FGolemResult UGolemRPGComponent::ConfigureSkillLabBuilds(
	const TMap<FPrimaryAssetId, FGolemSkillBuild>& InBuilds)
{
	const FGolemResult Authority = ValidateAuthority();
	if (!Authority.IsSuccess())
	{
		return Authority;
	}
	if (IsInitialized() || bTransactionInProgress || !InitialSkillLabBuilds.IsEmpty() || InBuilds.Num() != 3)
	{
		return FGolemResult::Failure(EGolemError::InvalidState, TEXT("SkillLabSeed"),
			LOCTEXT("InitialBuilds", "Configure exactly three initial SkillLab builds once before initialization."));
	}
	for (const TPair<FPrimaryAssetId, FGolemSkillBuild>& Entry : InBuilds)
	{
		const FGolemSkillBuild& Build = Entry.Value;
		const FGolemResult Validation = FGolemSkillCatalog::ValidateBuild(Entry.Key, Build);
		if (!Validation.IsSuccess())
		{
			return Validation;
		}
		if (Build.SpecId.IsNone() || !Build.ClassChoice.IsNone() || !Build.SpecChoice.IsNone()
			|| !Build.WeaponRune.IsNone() || !Build.SkillRune.IsNone())
		{
			return FGolemResult::Failure(EGolemError::InvalidData, TEXT("SkillLabSeed"),
				LOCTEXT("UnspentSeed", "The SkillLab initial build grants a spec and free core actions; paid choices remain unspent."));
		}
	}
	InitialSkillLabBuilds = InBuilds;
	return FGolemResult::Success();
}

FGolemResult UGolemRPGComponent::BuildSkillCandidate(FGolemRPGGolemView& Golem,
	const FGolemRPGView& Existing, const FGolemInventoryView& InventoryView,
	const FGolemRPGCommand& Command) const
{
	if (Golem.Build.SpecId.IsNone())
	{
		return FGolemResult::Failure(EGolemError::NotOwned, TEXT("SkillBuildUnlock"),
			LOCTEXT("NotUnlocked", "This campaign Golem has not unlocked the SkillLab specialization catalog."));
	}
	FGolemTalentLoadout* Preset = Golem.TalentLoadouts.FindByPredicate(
		[&Command](const FGolemTalentLoadout& Entry)
		{
			return Entry.LoadoutId == Command.TalentLoadoutId;
		});
	if (Command.Operation != EGolemRPGOperation::ApplyBuild
		&& Command.Operation != EGolemRPGOperation::NewTalentLoadout && Preset == nullptr)
	{
		return FGolemResult::Failure(EGolemError::NotFound, TEXT("TalentLoadout"),
			LOCTEXT("MissingPreset", "The exact talent loadout belongs to this Golem and must still exist."));
	}
	if (Command.Operation == EGolemRPGOperation::NewTalentLoadout
		|| Command.Operation == EGolemRPGOperation::RenameTalentLoadout
		|| Command.Operation == EGolemRPGOperation::DuplicateTalentLoadout)
	{
		if (Golem.TalentLoadouts.ContainsByPredicate([&Command](const FGolemTalentLoadout& Entry)
			{
				return Entry.Name.Equals(Command.TalentLoadoutName, ESearchCase::IgnoreCase)
					&& (Command.Operation != EGolemRPGOperation::RenameTalentLoadout
						|| Entry.LoadoutId != Command.TalentLoadoutId);
			}))
		{
			return FGolemResult::Failure(EGolemError::Conflict, TEXT("TalentLoadoutName"),
				LOCTEXT("DuplicateName", "Talent loadout names must be unique for this Golem."));
		}
	}
	if (Command.Operation == EGolemRPGOperation::NewTalentLoadout
		|| Command.Operation == EGolemRPGOperation::DuplicateTalentLoadout)
	{
		if (Golem.TalentLoadouts.Num() >= GolemRPG::MaximumTalentLoadouts
			|| Golem.TalentLoadouts.ContainsByPredicate([&Command](const FGolemTalentLoadout& Entry)
				{
					return Entry.LoadoutId == Command.RequestId;
				}))
		{
			return FGolemResult::Failure(EGolemError::CapacityExceeded, TEXT("TalentLoadouts"),
				LOCTEXT("LoadoutBound", "This Golem has reached the finite talent loadout capacity."));
		}
		FGolemTalentLoadout NewPreset;
		NewPreset.LoadoutId = Command.RequestId;
		NewPreset.Name = Command.TalentLoadoutName;
		NewPreset.Build = Preset == nullptr ? Command.ProposedBuild : Preset->Build;
		const FGolemResult Validation = FGolemSkillCatalog::ValidateBuild(Golem.HeroDefinitionId, NewPreset.Build);
		if (!Validation.IsSuccess())
		{
			return Validation;
		}
		const int32 Spent = !NewPreset.Build.ClassChoice.IsNone() + !NewPreset.Build.SpecChoice.IsNone();
		const int32 Available = Golem.SkillPoints + !Golem.Build.ClassChoice.IsNone() + !Golem.Build.SpecChoice.IsNone();
		if (Spent > Available)
		{
			return FGolemResult::Failure(EGolemError::InsufficientResources, TEXT("TalentPoints"),
				LOCTEXT("PresetPoints", "The saved talent build exceeds this Golem's shared point budget."));
		}
		Golem.TalentLoadouts.Add(MoveTemp(NewPreset));
		return FGolemResult::Success();
	}
	if (Command.Operation == EGolemRPGOperation::RenameTalentLoadout)
	{
		if (Preset->Version >= MAX_int32)
		{
			return FGolemResult::Failure(EGolemError::CapacityExceeded, TEXT("TalentLoadoutVersion"),
				LOCTEXT("VersionBound", "The talent loadout version cannot overflow."));
		}
		Preset->Name = Command.TalentLoadoutName;
		++Preset->Version;
		return FGolemResult::Success();
	}
	if (Command.Operation == EGolemRPGOperation::DeleteTalentLoadout)
	{
		Golem.TalentLoadouts.RemoveAll([&Command](const FGolemTalentLoadout& Entry)
			{
				return Entry.LoadoutId == Command.TalentLoadoutId;
			});
		if (Golem.ActiveTalentLoadoutId == Command.TalentLoadoutId)
		{
			// Deleting a selected preset removes its name, while the effective build remains equipped.
			Golem.ActiveTalentLoadoutId.Invalidate();
		}
		return FGolemResult::Success();
	}
	FGolemSkillBuild Proposed = Command.Operation == EGolemRPGOperation::ApplyBuild
		? Command.ProposedBuild : Preset->Build;
	if (Command.Operation == EGolemRPGOperation::ApplyTalentLoadout)
	{
		Proposed.WeaponRune = Golem.Build.WeaponRune;
		Proposed.SkillRune = Golem.Build.SkillRune;
	}
	const FGolemResult Validation = FGolemSkillCatalog::ValidateBuild(Golem.HeroDefinitionId, Proposed);
	if (!Validation.IsSuccess())
	{
		return Validation;
	}
	const int32 PreviousSpent = !Golem.Build.ClassChoice.IsNone() + !Golem.Build.SpecChoice.IsNone();
	const int32 ProposedSpent = !Proposed.ClassChoice.IsNone() + !Proposed.SpecChoice.IsNone();
	const int64 Remaining = static_cast<int64>(Golem.SkillPoints) + PreviousSpent - ProposedSpent;
	if (Remaining < 0 || Remaining > GolemRPG::MaximumSkillPoints)
	{
		return FGolemResult::Failure(EGolemError::InsufficientResources, TEXT("TalentPoints"),
			LOCTEXT("SharedPoints", "Class and specialization choices each cost one point from the same shared pool."));
	}
	const bool bTalentChanged = Golem.Build.SpecId != Proposed.SpecId
		|| Golem.Build.ClassChoice != Proposed.ClassChoice || Golem.Build.SpecChoice != Proposed.SpecChoice;
	Golem.SkillPoints = static_cast<int32>(Remaining);
	Golem.Build = Proposed;
	if (Command.Operation == EGolemRPGOperation::ApplyBuild)
	{
		Golem.WeaponRuneInstanceId = Command.WeaponRuneInstanceId;
		Golem.SkillRuneInstanceId = Command.SkillRuneInstanceId;
		if (bTalentChanged)
		{
			Golem.ActiveTalentLoadoutId.Invalidate();
		}
	}
	else
	{
		Golem.ActiveTalentLoadoutId = Command.TalentLoadoutId;
	}
	return ValidateBuildAttachments(Golem, Existing, InventoryView.Items);
}

FGolemResult UGolemRPGComponent::ValidateBuildAttachments(const FGolemRPGGolemView& Golem,
	const FGolemRPGView& View, const TArray<FGolemItemInstance>& Items) const
{
	const FName RuneKeys[] { Golem.Build.WeaponRune, Golem.Build.SkillRune };
	const FGuid RuneInstances[] { Golem.WeaponRuneInstanceId, Golem.SkillRuneInstanceId };
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(RuneKeys); ++Index)
	{
		if (RuneKeys[Index].IsNone())
		{
			if (RuneInstances[Index].IsValid())
			{
				return FGolemResult::Failure(EGolemError::InvalidData, TEXT("RuneAttachment"),
					LOCTEXT("EmptySocket", "An empty rune socket cannot reserve an inventory instance."));
			}
			continue;
		}
		const FGolemItemInstance* Item = Items.FindByPredicate([&RuneInstances, Index](const FGolemItemInstance& Entry)
			{
				return Entry.InstanceId == RuneInstances[Index] && Entry.Count == 1;
			});
		const UGolemDefinition* Resolved = nullptr;
		if (Item == nullptr)
		{
			return FGolemResult::Failure(EGolemError::NotOwned, TEXT("RuneAttachment"),
				LOCTEXT("OwnedRune", "Attach the exact unstackable rune instance present in shared inventory."));
		}
		const FGolemResult Resolution = ResolveDefinition(Item->DefinitionId, UGolemRuneDefinition::StaticClass(), Resolved);
		if (!Resolution.IsSuccess())
		{
			return Resolution;
		}
		const UGolemRuneDefinition* Definition = CastChecked<UGolemRuneDefinition>(Resolved);
		if (Golem.bGearEnabled && Item->LocationId != GolemGear::InventoryLocation)
		{
			const FGolemEquipmentAssignment* Weapon = Golem.FindAssignment(GolemGameplayTags::Slot_Equipment_Weapon);
			const bool bOwnWeaponSocket = Index == 0 && Item->LocationId == GolemGear::WeaponSocketLocation
				&& Weapon != nullptr && Item->ParentWeaponId == Weapon->ItemInstanceId
				&& Item->AssignedGolemId == Golem.GolemId;
			const bool bOwnSkillSocket = Index == 1 && Item->LocationId == GolemGear::SkillSocketLocation
				&& Item->AssignedGolemId == Golem.GolemId;
			if (!bOwnWeaponSocket && !bOwnSkillSocket)
			{
				return FGolemResult::Failure(EGolemError::NotOwned, TEXT("RuneParentOwnership"),
					LOCTEXT("RuneParentOwnership", "A rune already attached to another exact weapon or Golem cannot be moved implicitly."));
			}
		}
		if (Definition->GetAttachmentRuneId() != RuneKeys[Index] || !Definition->ValidateItem().IsSuccess()
			|| (Index == 0 && RuneInstances[0] == RuneInstances[1]))
		{
			return FGolemResult::Failure(EGolemError::TypeMismatch, TEXT("RuneAttachment"),
				LOCTEXT("RuneDefinition", "The source item must define this exact compatible weapon or skill rune."));
		}
		for (const FGolemRPGGolemView& Other : View.Golems)
		{
			if (Other.Assignments.ContainsByPredicate([&RuneInstances, Index](const FGolemEquipmentAssignment& Assignment)
				{
					return Assignment.ItemInstanceId == RuneInstances[Index];
				}) || (Other.GolemId != Golem.GolemId
				&& (Other.WeaponRuneInstanceId == RuneInstances[Index] || Other.SkillRuneInstanceId == RuneInstances[Index])))
			{
				return FGolemResult::Failure(EGolemError::Conflict, TEXT("RuneAttachmentOwner"),
					LOCTEXT("SingleOwner", "The rune instance is already equipped or attached to another socket."));
			}
		}
	}
	if (!Golem.Build.WeaponRune.IsNone())
	{
		const FGolemEquipmentAssignment* Weapon = Golem.FindAssignment(GolemGameplayTags::Slot_Equipment_Weapon);
		const UGolemDefinition* Resolved = nullptr;
		if (Weapon == nullptr)
		{
			return FGolemResult::Failure(EGolemError::InvalidState, TEXT("WeaponRuneSocket"),
				LOCTEXT("HeavyHammer", "Aftershock requires an equipped M5 heavy hammer."));
		}
		const FGolemResult Resolution = ResolveDefinition(Weapon->DefinitionId, UGolemItemDefinition::StaticClass(), Resolved);
		if (!Resolution.IsSuccess())
		{
			return Resolution;
		}
		if (CastChecked<UGolemItemDefinition>(Resolved)->GetWeaponFamily() != TEXT("HeavyHammer"))
		{
			return FGolemResult::Failure(EGolemError::TypeMismatch, TEXT("WeaponRuneCompatibility"),
				LOCTEXT("WeaponFamily", "Aftershock is compatible only with the HeavyHammer weapon family."));
		}
	}
	return FGolemResult::Success();
}

#undef LOCTEXT_NAMESPACE
