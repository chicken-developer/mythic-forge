#include "Commandlets/GolemRPGAssetAuthoring.h"

#include "Assets/GolemAssetManager.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Definitions/GolemItemDefinition.h"
#include "Definitions/GolemLootTableDefinition.h"
#include "Definitions/GolemRuneDefinition.h"
#include "Definitions/GolemSkillDefinition.h"
#include "Definitions/GolemSkillTreeDefinition.h"
#include "HAL/FileManager.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

DEFINE_LOG_CATEGORY_STATIC(LogGolemRPGAssetAuthoring, Log, All);

namespace GolemRPGAssetAuthoring
{
	namespace
	{
		FString PackageName(const TCHAR* Kind, const TCHAR* Name)
		{
			return FString::Printf(TEXT("/Game/Golem/Definitions/Items/DA_Golem_%s_%s"), Kind, Name);
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
				UE_LOG(LogGolemRPGAssetAuthoring, Error, TEXT("Existing %s has an incompatible native asset class."), *Path);
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
				UE_LOG(LogGolemRPGAssetAuthoring, Error, TEXT("Existing %s has a different stable identity; preserving it."), *Path);
				return nullptr;
			}
			return Definition;
		}
		bool Save(UGolemDefinition* Definition, const FGolemResult& Validation)
		{
			if (Definition == nullptr || !Validation.IsSuccess())
			{
				UE_LOG(LogGolemRPGAssetAuthoring, Error, TEXT("RPG definition validation failed: %s."), *Validation.Context.ToString());
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
				UE_LOG(LogGolemRPGAssetAuthoring, Error, TEXT("Failed genuine RPG package save: %s."), *Filename);
				return false;
			}
			UE_LOG(LogGolemRPGAssetAuthoring, Display, TEXT("Saved %s (%lld bytes)."),
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
			UGolemItemDefinition* Shard = FindOrCreate<UGolemItemDefinition>(TEXT("Item"), TEXT("ForgeShard"), TEXT("GolemItem"), bCreated);
			if (Shard == nullptr)
			{
				return false;
			}
			if (bCreated)
			{
				Shard->SetDisplayName(NSLOCTEXT("GolemRPGAssets", "ForgeShard", "Forge shard"));
				if (!Shard->TrySetItemTuning(EGolemItemKind::Material, 9999).IsSuccess())
				{
					return false;
				}
			}
			if (!Save(Shard, Shard->ValidateItem()))
			{
				return false;
			}
			UGolemItemDefinition* Hammer = FindOrCreate<UGolemItemDefinition>(TEXT("Item"), TEXT("HammerUpgrade"), TEXT("GolemItem"), bCreated);
			if (Hammer == nullptr)
			{
				return false;
			}
			if (bCreated)
			{
				FGolemItemStatBonuses Bonuses;
				Bonuses.MaxHealth = 20.0f;
				Bonuses.AttackPower = 10.0f;
				Hammer->SetDisplayName(NSLOCTEXT("GolemRPGAssets", "HammerUpgrade", "Forged hammer"));
				if (!Hammer->TrySetItemTuning(EGolemItemKind::Equipment, 1, FPrimaryAssetId(), Bonuses).IsSuccess())
				{
					return false;
				}
			}
			if (!Save(Hammer, Hammer->ValidateItem()))
			{
				return false;
			}
			UGolemRuneDefinition* Rune = FindOrCreate<UGolemRuneDefinition>(TEXT("Item"), TEXT("RuneUpgrade"), TEXT("GolemItem"), bCreated);
			if (Rune == nullptr)
			{
				return false;
			}
			if (bCreated)
			{
				FGolemRuneTuning Tuning;
				Tuning.DamageScale = 1.2f;
				Tuning.CostScale = 0.5f;
				Tuning.DurationScale = 1.5f;
				Rune->SetDisplayName(NSLOCTEXT("GolemRPGAssets", "RuneUpgrade", "Focused rune"));
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
			const FText Labels[] { NSLOCTEXT("GolemRPGAssets", "HammerForce", "Hammer force"),
				NSLOCTEXT("GolemRPGAssets", "RuneFocus", "Rune focus"),
				NSLOCTEXT("GolemRPGAssets", "ImpossibleCost", "Mastery challenge") };
			TArray<FPrimaryAssetId> Ids;
			TArray<const UGolemSkillDefinition*> Nodes;
			for (int32 Index = 0; Index < UE_ARRAY_COUNT(Names); ++Index)
			{
				bool bCreated = false;
				UGolemSkillDefinition* Skill = FindOrCreate<UGolemSkillDefinition>(TEXT("Skill"), Names[Index], TEXT("GolemSkill"), bCreated);
				if (Skill == nullptr)
				{
					return false;
				}
				if (bCreated)
				{
					FGolemMaterialCost Cost;
					Cost.DefinitionId = FPrimaryAssetId(TEXT("GolemItem"), TEXT("ForgeShard"));
					Cost.Count = Index == 2 ? 100 : 2;
					FGolemItemStatBonuses Bonuses;
					FGolemRuneTuning Rune;
					TArray<FPrimaryAssetId> Prerequisites;
					if (Index == 0)
					{
						Bonuses.AttackPower = 5.0f;
					}
					else if (Index == 1)
					{
						Rune.DurationScale = 1.5f;
						Prerequisites.Add(FPrimaryAssetId(TEXT("GolemSkill"), TEXT("HammerForce")));
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
			UGolemSkillTreeDefinition* Tree = FindOrCreate<UGolemSkillTreeDefinition>(TEXT("SkillTree"), TEXT("Foundation"), TEXT("GolemSkillTree"), bCreated);
			if (Tree == nullptr)
			{
				return false;
			}
			if (bCreated)
			{
				Tree->SetDisplayName(NSLOCTEXT("GolemRPGAssets", "FoundationTree", "Golem training"));
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
			UGolemLootTableDefinition* Loot = FindOrCreate<UGolemLootTableDefinition>(TEXT("LootTable"), TEXT("EncounterA"), TEXT("GolemLootTable"), bCreated);
			if (Loot == nullptr)
			{
				return false;
			}
			if (bCreated)
			{
				TArray<FGolemLootEntry> Entries;
				for (FName Name : { FName(TEXT("HammerUpgrade")), FName(TEXT("RuneUpgrade")), FName(TEXT("ForgeShard")) })
				{
					FGolemLootEntry Entry;
					Entry.DefinitionId = FPrimaryAssetId(TEXT("GolemItem"), Name);
					Entry.MinimumCount = Entry.MaximumCount = Name == TEXT("ForgeShard") ? 10 : 1;
					Entries.Add(Entry);
				}
				Loot->SetDisplayName(NSLOCTEXT("GolemRPGAssets", "EncounterALoot", "Arena spoils"));
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
		UGolemItemDefinition* Shard = Load<UGolemItemDefinition>(TEXT("Item"), TEXT("ForgeShard"));
		UGolemItemDefinition* Hammer = Load<UGolemItemDefinition>(TEXT("Item"), TEXT("HammerUpgrade"));
		UGolemRuneDefinition* Rune = Load<UGolemRuneDefinition>(TEXT("Item"), TEXT("RuneUpgrade"));
		UGolemSkillTreeDefinition* Tree = Load<UGolemSkillTreeDefinition>(TEXT("SkillTree"), TEXT("Foundation"));
		UGolemLootTableDefinition* Loot = Load<UGolemLootTableDefinition>(TEXT("LootTable"), TEXT("EncounterA"));
		if (Shard == nullptr || Hammer == nullptr || Rune == nullptr || Tree == nullptr || Loot == nullptr
			|| !Shard->ValidateItem().IsSuccess() || !Hammer->ValidateItem().IsSuccess() || !Rune->ValidateItem().IsSuccess()
			|| !Tree->ValidateSkillTree().IsSuccess() || !Loot->ValidateLootTable().IsSuccess())
		{
			UE_LOG(LogGolemRPGAssetAuthoring, Error, TEXT("Saved RPG item/tree/loot assets are missing or invalid."));
			return false;
		}
		TArray<const UGolemSkillDefinition*> Nodes;
		for (const TCHAR* Name : { TEXT("HammerForce"), TEXT("RuneFocus"), TEXT("ImpossibleCost") })
		{
			UGolemSkillDefinition* Skill = Load<UGolemSkillDefinition>(TEXT("Skill"), Name);
			if (Skill == nullptr || !Skill->ValidateSkill().IsSuccess())
			{
				return false;
			}
			for (const FGolemMaterialCost& Cost : Skill->GetMaterialCosts())
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
		UGolemAssetManager* Manager = UGolemAssetManager::GetGolemAssetManager();
		if (Manager == nullptr || !Manager->ValidateDefinitionCatalog().IsSuccess())
		{
			return false;
		}
		TArray<FGolemItemInstance> Generated;
		return Loot->GenerateLoot(305, FGuid(5, 3, 0, 1), Generated).IsSuccess() && Generated.Num() == 3;
	}
}
