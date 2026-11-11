#include "Commandlets/KnightGearAssetAuthoring.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Catalog/KnightGearCatalog.h"
#include "Definitions/KnightItemDefinition.h"
#include "Definitions/KnightRuneDefinition.h"
#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

DEFINE_LOG_CATEGORY_STATIC(LogKnightGearAuthoring, Log, All);

bool KnightGearAssetAuthoring::CreateGearAssets()
{
	TArray<UKnightItemDefinition*> Definitions;
	TArray<TSharedPtr<FJsonValue>> Entries;
	TMap<FString, int32> Counts;
	for (const FKnightGearSeed& Seed : FKnightGearCatalog::GetSeeds())
	{
		const bool bRune = Seed.Metadata.Category == EKnightItemCategory::AttachRune;
		const FString Path = bRune
			? TEXT("/Game/Knight/Definitions/Items/DA_Knight_Rune_") + Seed.DefinitionId.PrimaryAssetName.ToString()
			: TEXT("/Game/Knight/Definitions/Items/DA_Knight_Item_M056_") + Seed.CatalogId.ToString();
		const bool bExists = FPackageName::DoesPackageExist(Path);
		if (bRune && !bExists)
		{
			UE_LOG(LogKnightGearAuthoring, Error, TEXT("Required existing M5.5 Rune missing: %s"), *Path);
			return false;
		}
		UPackage* Package = bExists ? LoadPackage(nullptr, *Path, LOAD_None) : CreatePackage(*Path);
		if (Package == nullptr)
		{
			return false;
		}
		const FName AssetName(*FPackageName::GetLongPackageAssetName(Path));
		UKnightItemDefinition* Definition = FindObject<UKnightItemDefinition>(Package, *AssetName.ToString());
		if (Definition == nullptr && !bExists)
		{
			Definition = NewObject<UKnightItemDefinition>(Package, AssetName, RF_Public | RF_Standalone);
			FAssetRegistryModule::AssetCreated(Definition);
		}
		if (Definition == nullptr || (bRune && !Definition->IsA<UKnightRuneDefinition>()))
		{
			return false;
		}
		const FKnightResult Configured = FKnightGearCatalog::ConfigureDefinition(Definition, Seed);
		if (!Configured.IsSuccess() || !Definition->ValidateItem().IsSuccess())
		{
			UE_LOG(LogKnightGearAuthoring, Error, TEXT("Gear catalog validation failed: %s [%s]"),
				*Seed.CatalogId.ToString(), *Configured.Context.ToString());
			return false;
		}
		const FString Filename = FPackageName::LongPackageNameToFilename(Path, FPackageName::GetAssetPackageExtension());
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);
		Package->MarkPackageDirty();
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		SaveArgs.SaveFlags = SAVE_NoError;
		if (!UPackage::SavePackage(Package, Definition, *Filename, SaveArgs)
			|| IFileManager::Get().FileSize(*Filename) <= 0)
		{
			return false;
		}
		Definitions.Add(Definition);
		const TSharedRef<FJsonObject> Row = MakeShared<FJsonObject>();
		Row->SetStringField(TEXT("catalogKey"), TEXT("M056.") + Seed.CatalogId.ToString());
		Row->SetStringField(TEXT("definitionId"), Definition->GetPrimaryAssetId().ToString());
		Row->SetStringField(TEXT("assetPath"), Path);
		Row->SetStringField(TEXT("name"), Seed.Name.ToString());
		const FString Category = StaticEnum<EKnightItemCategory>()->GetNameStringByValue(static_cast<int64>(Seed.Metadata.Category));
		++Counts.FindOrAdd(Category);
		Row->SetStringField(TEXT("category"), Category);
		Row->SetStringField(TEXT("subtype"), StaticEnum<EKnightEquipmentSubtype>()->GetNameStringByValue(static_cast<int64>(Seed.Metadata.Subtype)));
		Row->SetStringField(TEXT("rarity"), StaticEnum<EKnightItemRarity>()->GetNameStringByValue(static_cast<int64>(Seed.Metadata.Rarity)));
		Row->SetNumberField(TEXT("itemLevel"), Seed.Metadata.ItemLevel);
		Row->SetNumberField(TEXT("requiredLevel"), Seed.Metadata.RequiredLevel);
		Row->SetNumberField(TEXT("maximumStack"), Seed.MaximumStack);
		Row->SetStringField(TEXT("equipEffect"), Seed.Metadata.EquipEffectId.ToString());
		Row->SetBoolField(TEXT("reusedM55Rune"), bRune);
		const TSharedRef<FJsonObject> Stats = MakeShared<FJsonObject>();
		Stats->SetNumberField(TEXT("Power"), Seed.Stats.AttackPower);
		Stats->SetNumberField(TEXT("Health"), Seed.Stats.MaxHealth);
		Stats->SetNumberField(TEXT("Armor"), Seed.Stats.Defense);
		Stats->SetNumberField(TEXT("Stamina"), Seed.Stats.MaxStamina);
		Stats->SetNumberField(TEXT("Resource"), Seed.Stats.MaxRuneEnergy);
		Stats->SetNumberField(TEXT("Poise"), Seed.Stats.Poise);
		Stats->SetNumberField(TEXT("CritChance"), Seed.Stats.CritChance);
		Stats->SetNumberField(TEXT("Haste"), Seed.Stats.Haste);
		Stats->SetNumberField(TEXT("CooldownRecovery"), Seed.Stats.CooldownRecovery);
		Stats->SetNumberField(TEXT("MasteryBonus"), Seed.Stats.MasteryBonus);
		Stats->SetNumberField(TEXT("Versatility"), Seed.Stats.Versatility);
		Row->SetObjectField(TEXT("stats"), Stats);
		Entries.Add(MakeShared<FJsonValueObject>(Row));
	}
	const FKnightResult Validated = FKnightGearCatalog::ValidateCatalog(Definitions);
	if (!Validated.IsSuccess())
	{
		UE_LOG(LogKnightGearAuthoring, Error, TEXT("41-item manifest rejected: %s"), *Validated.Context.ToString());
		return false;
	}
	const TSharedRef<FJsonObject> Manifest = MakeShared<FJsonObject>();
	Manifest->SetStringField(TEXT("status"), TEXT("PASS"));
	Manifest->SetNumberField(TEXT("definitionCount"), Definitions.Num());
	Manifest->SetNumberField(TEXT("equipmentSlotCount"), KnightGear::EquipmentSlots().Num());
	Manifest->SetNumberField(TEXT("equipmentSubtypeCount"), 11);
	const TSharedRef<FJsonObject> CountObject = MakeShared<FJsonObject>();
	for (const TPair<FString, int32>& Entry : Counts)
	{
		CountObject->SetNumberField(Entry.Key, Entry.Value);
	}
	Manifest->SetObjectField(TEXT("categoryCounts"), CountObject);
	Manifest->SetArrayField(TEXT("definitions"), Entries);
	TArray<TSharedPtr<FJsonValue>> Slots;
	for (const FGameplayTag Slot : KnightGear::EquipmentSlots())
	{
		Slots.Add(MakeShared<FJsonValueString>(Slot.ToString()));
	}
	Manifest->SetArrayField(TEXT("canonicalSlots"), Slots);
	FString Json;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
	FJsonSerializer::Serialize(Manifest, Writer);
	const FString Directory = FPaths::ProjectDir() / TEXT("Saved/Evidence/M56");
	IFileManager::Get().MakeDirectory(*Directory, true);
	return FFileHelper::SaveStringToFile(Json, *(Directory / TEXT("catalog_manifest.json")),
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}
