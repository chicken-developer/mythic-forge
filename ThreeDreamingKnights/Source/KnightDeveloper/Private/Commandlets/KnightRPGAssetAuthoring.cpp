#include "Commandlets/KnightRPGAssetAuthoring.h"

#include "Assets/KnightAssetManager.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Definitions/KnightItemDefinition.h"
#include "Definitions/KnightLootTableDefinition.h"
#include "Definitions/KnightRuneDefinition.h"
#include "Definitions/KnightSkillDefinition.h"
#include "Definitions/KnightSkillTreeDefinition.h"
#include "HAL/FileManager.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

DEFINE_LOG_CATEGORY_STATIC(LogKnightRPGAssetAuthoring, Log, All);

namespace KnightRPGAssetAuthoring
{
	namespace
	{
		FString PackageName(const TCHAR* Kind, const TCHAR* Name)
		{
			return FString::Printf(TEXT("/Game/Knight/Definitions/Items/DA_Knight_%s_%s"), Kind, Name);
		}
		template <typename DefinitionType>
		DefinitionType* FindOrCreate(const TCHAR* Kind, const TCHAR* Name, FName Type, bool& bOutCreated)
		{
			bOutCreated = false;
			const FString Path = PackageName(Kind, Name);
			const bool bExists = FPackageName::DoesPackageExist(Path);
			UPackage* Package = bExists ? LoadPackage(nullptr, *Path, LOAD_None) : CreatePackage(*Path);
			if (Package == nullptr)
			{
				return nullptr;
			}
			const FName AssetName(*FPackageName::GetLongPackageAssetName(Path));
			UObject* Existing = FindObject<UObject>(Package, *AssetName.ToString());
			DefinitionType* Definition = Cast<DefinitionType>(Existing);
			if ((bExists || Existing != nullptr) && Definition == nullptr)
			{
				UE_LOG(LogKnightRPGAssetAuthoring, Error, TEXT("Existing %s has an incompatible native asset class."), *Path);
				return nullptr;
			}
			if (Definition == nullptr)
			{
				Definition = NewObject<DefinitionType>(Package, AssetName, RF_Public | RF_Standalone);
				if (!Definition->TrySetIdentity(FName(Name), Type))
				{
					return nullptr;
				}
				bOutCreated = true;
				FAssetRegistryModule::AssetCreated(Definition);
			}
			if (Definition->GetPrimaryAssetId() != FPrimaryAssetId(Type, FName(Name)))
			{
				UE_LOG(LogKnightRPGAssetAuthoring, Error, TEXT("Existing %s has a different stable identity; preserving it."), *Path);
				return nullptr;
			}
			return Definition;
		}
		bool Save(UKnightDefinition* Definition, const FKnightResult& Validation)
		{
			if (Definition == nullptr || !Validation.IsSuccess())
			{
				UE_LOG(LogKnightRPGAssetAuthoring, Error, TEXT("RPG definition validation failed: %s."), *Validation.Context.ToString());
				return false;
			}
			UPackage* Package = Definition->GetPackage();
			const FString Filename = FPackageName::LongPackageNameToFilename(Package->GetName(),
				FPackageName::GetAssetPackageExtension());
			if (!IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true))
			{
				return false;
			}
			Package->MarkPackageDirty();
			FSavePackageArgs Args;
			Args.TopLevelFlags = RF_Public | RF_Standalone;
			Args.SaveFlags = SAVE_NoError;
			Args.bSlowTask = false;
			if (!UPackage::SavePackage(Package, Definition, *Filename, Args)
				|| IFileManager::Get().FileSize(*Filename) <= 0)
			{
				UE_LOG(LogKnightRPGAssetAuthoring, Error, TEXT("Failed genuine RPG package save: %s."), *Filename);
				return false;
			}
			UE_LOG(LogKnightRPGAssetAuthoring, Display, TEXT("Saved %s (%lld bytes)."),
				*Filename, IFileManager::Get().FileSize(*Filename));
			return true;
		}
		template <typename DefinitionType>
		DefinitionType* Load(const TCHAR* Kind, const TCHAR* Name)
		{
			const FString Package = PackageName(Kind, Name);
			const FString ObjectPath = Package + TEXT(".") + FPackageName::GetLongPackageAssetName(Package);
			return LoadObject<DefinitionType>(nullptr, *ObjectPath);
		}
		bool CreateItems()
		{
			bool bCreated = false;
			UKnightItemDefinition* Shard = FindOrCreate<UKnightItemDefinition>(TEXT("Item"), TEXT("ForgeShard"), TEXT("KnightItem"), bCreated);
			if (Shard == nullptr)
			{
				return false;
			}
			if (bCreated)
			{
				Shard->SetDisplayName(NSLOCTEXT("KnightRPGAssets", "ForgeShard", "Forge shard"));
				if (!Shard->TrySetItemTuning(EKnightItemKind::Material, 9999).IsSuccess())
				{
					return false;
				}
			}
			if (!Save(Shard, Shard->ValidateItem()))
			{
				return false;
			}
			UKnightItemDefinition* Hammer = FindOrCreate<UKnightItemDefinition>(TEXT("Item"), TEXT("HammerUpgrade"), TEXT("KnightItem"), bCreated);
			if (Hammer == nullptr)
			{
				return false;
			}
			if (bCreated)
			{
				FKnightItemStatBonuses Bonuses;
				Bonuses.MaxHealth = 20.0f;
				Bonuses.AttackPower = 10.0f;
				Hammer->SetDisplayName(NSLOCTEXT("KnightRPGAssets", "HammerUpgrade", "Forged hammer"));
				if (!Hammer->TrySetItemTuning(EKnightItemKind::Equipment, 1, FPrimaryAssetId(), Bonuses).IsSuccess())
				{
					return false;
				}
			}
			if (!Save(Hammer, Hammer->ValidateItem()))
			{
				return false;
			}
			UKnightRuneDefinition* Rune = FindOrCreate<UKnightRuneDefinition>(TEXT("Item"), TEXT("RuneUpgrade"), TEXT("KnightItem"), bCreated);
			if (Rune == nullptr)
			{
				return false;
			}
			if (bCreated)
			{
				FKnightRuneTuning Tuning;
				Tuning.DamageScale = 1.2f;
				Tuning.CostScale = 0.5f;
				Tuning.DurationScale = 1.5f;
				Rune->SetDisplayName(NSLOCTEXT("KnightRPGAssets", "RuneUpgrade", "Focused rune"));
				if (!Rune->TrySetRuneTuning(Tuning).IsSuccess())
				{
					return false;
				}
			}
			return Save(Rune, Rune->ValidateItem());
		}
		bool CreateSkills()
		{
			const TCHAR* Names[] { TEXT("HammerForce"), TEXT("RuneFocus"), TEXT("ImpossibleCost") };
			const FText Labels[] { NSLOCTEXT("KnightRPGAssets", "HammerForce", "Hammer force"),
				NSLOCTEXT("KnightRPGAssets", "RuneFocus", "Rune focus"),
				NSLOCTEXT("KnightRPGAssets", "ImpossibleCost", "Mastery challenge") };
			TArray<FPrimaryAssetId> Ids;
			TArray<const UKnightSkillDefinition*> Nodes;
			for (int32 Index = 0; Index < UE_ARRAY_COUNT(Names); ++Index)
			{
				bool bCreated = false;
				UKnightSkillDefinition* Skill = FindOrCreate<UKnightSkillDefinition>(TEXT("Skill"), Names[Index], TEXT("KnightSkill"), bCreated);
				if (Skill == nullptr)
				{
					return false;
				}
				if (bCreated)
				{
					FKnightMaterialCost Cost;
					Cost.DefinitionId = FPrimaryAssetId(TEXT("KnightItem"), TEXT("ForgeShard"));
					Cost.Count = Index == 2 ? 100 : 2;
					FKnightItemStatBonuses Bonuses;
					FKnightRuneTuning Rune;
					TArray<FPrimaryAssetId> Prerequisites;
					if (Index == 0)
					{
						Bonuses.AttackPower = 5.0f;
					}
					else if (Index == 1)
					{
						Rune.DurationScale = 1.5f;
						Prerequisites.Add(FPrimaryAssetId(TEXT("KnightSkill"), TEXT("HammerForce")));
					}
					Skill->SetDisplayName(Labels[Index]);
					if (!Skill->TrySetSkillTuning(Index == 2 ? 50 : 1, { Cost }, Prerequisites, Bonuses, Rune).IsSuccess())
					{
						return false;
					}
				}
				if (!Save(Skill, Skill->ValidateSkill()))
				{
					return false;
				}
				Ids.Add(Skill->GetPrimaryAssetId());
				Nodes.Add(Skill);
			}
			bool bCreated = false;
			UKnightSkillTreeDefinition* Tree = FindOrCreate<UKnightSkillTreeDefinition>(TEXT("SkillTree"), TEXT("Foundation"), TEXT("KnightSkillTree"), bCreated);
			if (Tree == nullptr)
			{
				return false;
			}
			if (bCreated)
			{
				Tree->SetDisplayName(NSLOCTEXT("KnightRPGAssets", "FoundationTree", "Knight training"));
				if (!Tree->TrySetSkills(Ids).IsSuccess())
				{
					return false;
				}
			}
			return Save(Tree, Tree->ValidateGraph(Nodes));
		}
		bool CreateLoot()
		{
			bool bCreated = false;
			UKnightLootTableDefinition* Loot = FindOrCreate<UKnightLootTableDefinition>(TEXT("LootTable"), TEXT("EncounterA"), TEXT("KnightLootTable"), bCreated);
			if (Loot == nullptr)
			{
				return false;
			}
			if (bCreated)
			{
				TArray<FKnightLootEntry> Entries;
				for (FName Name : { FName(TEXT("HammerUpgrade")), FName(TEXT("RuneUpgrade")), FName(TEXT("ForgeShard")) })
				{
					FKnightLootEntry Entry;
					Entry.DefinitionId = FPrimaryAssetId(TEXT("KnightItem"), Name);
					Entry.MinimumCount = Entry.MaximumCount = Name == TEXT("ForgeShard") ? 10 : 1;
					Entries.Add(Entry);
				}
				Loot->SetDisplayName(NSLOCTEXT("KnightRPGAssets", "EncounterALoot", "Arena spoils"));
				if (!Loot->TrySetEntries(Entries).IsSuccess())
				{
					return false;
				}
			}
			return Save(Loot, Loot->ValidateLootTable());
		}
	}

	bool CreateRPGAssets()
	{
		return CreateItems() && CreateSkills() && CreateLoot() && ValidateRPGAssets();
	}

	bool ValidateRPGAssets()
	{
		UKnightItemDefinition* Shard = Load<UKnightItemDefinition>(TEXT("Item"), TEXT("ForgeShard"));
		UKnightItemDefinition* Hammer = Load<UKnightItemDefinition>(TEXT("Item"), TEXT("HammerUpgrade"));
		UKnightRuneDefinition* Rune = Load<UKnightRuneDefinition>(TEXT("Item"), TEXT("RuneUpgrade"));
		UKnightSkillTreeDefinition* Tree = Load<UKnightSkillTreeDefinition>(TEXT("SkillTree"), TEXT("Foundation"));
		UKnightLootTableDefinition* Loot = Load<UKnightLootTableDefinition>(TEXT("LootTable"), TEXT("EncounterA"));
		if (Shard == nullptr || Hammer == nullptr || Rune == nullptr || Tree == nullptr || Loot == nullptr
			|| !Shard->ValidateItem().IsSuccess() || !Hammer->ValidateItem().IsSuccess() || !Rune->ValidateItem().IsSuccess()
			|| !Tree->ValidateSkillTree().IsSuccess() || !Loot->ValidateLootTable().IsSuccess())
		{
			UE_LOG(LogKnightRPGAssetAuthoring, Error, TEXT("Saved RPG item/tree/loot assets are missing or invalid."));
			return false;
		}
		TArray<const UKnightSkillDefinition*> Nodes;
		for (const TCHAR* Name : { TEXT("HammerForce"), TEXT("RuneFocus"), TEXT("ImpossibleCost") })
		{
			UKnightSkillDefinition* Skill = Load<UKnightSkillDefinition>(TEXT("Skill"), Name);
			if (Skill == nullptr || !Skill->ValidateSkill().IsSuccess())
			{
				return false;
			}
			for (const FKnightMaterialCost& Cost : Skill->GetMaterialCosts())
			{
				if (Cost.DefinitionId != Shard->GetPrimaryAssetId() || Cost.Count > Shard->GetMaximumStack())
				{
					return false;
				}
			}
			Nodes.Add(Skill);
		}
		if (!Tree->ValidateGraph(Nodes).IsSuccess())
		{
			return false;
		}
		UKnightAssetManager* Manager = UKnightAssetManager::GetKnightAssetManager();
		if (Manager == nullptr || !Manager->ValidateDefinitionCatalog().IsSuccess())
		{
			return false;
		}
		TArray<FKnightItemInstance> Generated;
		return Loot->GenerateLoot(305, FGuid(5, 3, 0, 1), Generated).IsSuccess() && Generated.Num() == 3;
	}
}
