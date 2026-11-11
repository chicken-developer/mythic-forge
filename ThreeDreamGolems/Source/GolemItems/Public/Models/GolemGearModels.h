#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Models/GolemItemModels.h"
#include "GolemGearModels.generated.h"

class UStaticMesh;
class UTexture2D;

UENUM(BlueprintType)
enum class EGolemItemCategory : uint8
{
	Equipment, AttachRune, Consumable, Material, QuestItem, Junk
};

UENUM(BlueprintType)
enum class EGolemEquipmentSubtype : uint8
{
	None, Hammer, Head, Shoulders, Chest, Wrists, Hands, Waist, Feet, Neck, Ring, Relic
};

UENUM(BlueprintType)
enum class EGolemItemRarity : uint8
{
	Poor, Common, Uncommon, Rare, Epic
};

/** Definition metadata. Neither role hints nor theme restrict canonical Golem equipment. */
USTRUCT(BlueprintType)
struct GOLEMITEMS_API FGolemItemMetadata
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Items|Catalog")
	EGolemItemCategory Category = EGolemItemCategory::Material;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Items|Catalog")
	EGolemEquipmentSubtype Subtype = EGolemEquipmentSubtype::None;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Items|Catalog")
	EGolemItemRarity Rarity = EGolemItemRarity::Common;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Items|Catalog")
	int32 ItemLevel = 0;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Items|Catalog")
	int32 RequiredLevel = 1;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Items|Catalog")
	TArray<FGameplayTag> AllowedSlots;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Items|Catalog")
	FName SocketProfile;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Items|Catalog")
	FPrimaryAssetId UniqueEquipGroup;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Items|Catalog")
	int32 UniqueEquipLimit = 0;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Items|Catalog")
	FName EquipEffectId;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Items|Catalog")
	FName UpgradeTrack;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Items|Catalog")
	int32 BuyPrice = 0;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Items|Catalog")
	int32 SellPrice = 0;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Items|Catalog")
	bool bUniqueOwned = false;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Items|Catalog")
	FText RoleHint;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Items|Catalog")
	FText Description;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Items|Catalog")
	FName Theme;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Items|Catalog")
	TSoftObjectPtr<UTexture2D> Icon;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Golem|Items|Catalog")
	TSoftObjectPtr<UStaticMesh> Visual;

	FGolemResult Validate(const FPrimaryAssetId& DefinitionId) const;
};

USTRUCT()
struct GOLEMITEMS_API FGolemGearRewardItem
{
	GENERATED_BODY()
	UPROPERTY(SaveGame)
	FPrimaryAssetId DefinitionId;
	UPROPERTY(SaveGame)
	int32 Count = 0;
	bool operator==(const FGolemGearRewardItem& Other) const;
};

/** Authoritative fixture offer retained independently of its eventual all-or-nothing claim. */
USTRUCT()
struct GOLEMITEMS_API FGolemGearRewardBundle
{
	GENERATED_BODY()
	UPROPERTY(SaveGame)
	FGuid RewardId;
	UPROPERTY(SaveGame)
	FGuid DemoRunId;
	UPROPERTY(SaveGame)
	FGuid EncounterId;
	UPROPERTY(SaveGame)
	int64 Currency = 0;
	UPROPERTY(SaveGame)
	int32 Level = 1;
	UPROPERTY(SaveGame)
	TArray<FGolemGearRewardItem> Items;
	UPROPERTY(SaveGame)
	TArray<FGuid> BoundItems;
	bool operator==(const FGolemGearRewardBundle& Other) const;
};

namespace GolemGear
{
	inline const FName InventoryLocation(TEXT("Inventory"));
	inline const FName EquipmentLocation(TEXT("Equipment"));
	inline const FName WeaponSocketLocation(TEXT("WeaponSocket"));
	inline const FName SkillSocketLocation(TEXT("SkillSocket"));
	inline constexpr int64 MaximumCurrency = 1000000000;
	inline constexpr int32 MaximumLevel = 100;
	inline constexpr int32 DemoCapacity = 128;
	GOLEMITEMS_API const TArray<FGameplayTag>& EquipmentSlots();
	GOLEMITEMS_API FGameplayTag SlotForSubtype(EGolemEquipmentSubtype Subtype);
	GOLEMITEMS_API bool IsEquipmentSlot(FGameplayTag Slot);
}
