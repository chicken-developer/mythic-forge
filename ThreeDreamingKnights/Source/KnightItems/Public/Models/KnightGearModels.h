#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Models/KnightItemModels.h"
#include "KnightGearModels.generated.h"

class UStaticMesh;
class UTexture2D;

UENUM(BlueprintType)
enum class EKnightItemCategory : uint8
{
	Equipment, AttachRune, Consumable, Material, QuestItem, Junk
};

UENUM(BlueprintType)
enum class EKnightEquipmentSubtype : uint8
{
	None, Hammer, Head, Shoulders, Chest, Wrists, Hands, Waist, Feet, Neck, Ring, Relic
};

UENUM(BlueprintType)
enum class EKnightItemRarity : uint8
{
	Poor, Common, Uncommon, Rare, Epic
};

/** Definition metadata. Neither role hints nor theme restrict canonical Knight equipment. */
USTRUCT(BlueprintType)
struct KNIGHTITEMS_API FKnightItemMetadata
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Knight|Items|Catalog")
	EKnightItemCategory Category = EKnightItemCategory::Material;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Knight|Items|Catalog")
	EKnightEquipmentSubtype Subtype = EKnightEquipmentSubtype::None;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Knight|Items|Catalog")
	EKnightItemRarity Rarity = EKnightItemRarity::Common;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Knight|Items|Catalog")
	int32 ItemLevel = 0;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Knight|Items|Catalog")
	int32 RequiredLevel = 1;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Knight|Items|Catalog")
	TArray<FGameplayTag> AllowedSlots;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Knight|Items|Catalog")
	FName SocketProfile;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Knight|Items|Catalog")
	FPrimaryAssetId UniqueEquipGroup;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Knight|Items|Catalog")
	int32 UniqueEquipLimit = 0;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Knight|Items|Catalog")
	FName EquipEffectId;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Knight|Items|Catalog")
	FName UpgradeTrack;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Knight|Items|Catalog")
	int32 BuyPrice = 0;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Knight|Items|Catalog")
	int32 SellPrice = 0;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Knight|Items|Catalog")
	bool bUniqueOwned = false;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Knight|Items|Catalog")
	FText RoleHint;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Knight|Items|Catalog")
	FText Description;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Knight|Items|Catalog")
	FName Theme;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Knight|Items|Catalog")
	TSoftObjectPtr<UTexture2D> Icon;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Knight|Items|Catalog")
	TSoftObjectPtr<UStaticMesh> Visual;

	FKnightResult Validate(const FPrimaryAssetId& DefinitionId) const;
};

USTRUCT()
struct KNIGHTITEMS_API FKnightGearRewardItem
{
	GENERATED_BODY()
	UPROPERTY(SaveGame)
	FPrimaryAssetId DefinitionId;
	UPROPERTY(SaveGame)
	int32 Count = 0;
	bool operator==(const FKnightGearRewardItem& Other) const;
};

/** Authoritative fixture offer retained independently of its eventual all-or-nothing claim. */
USTRUCT()
struct KNIGHTITEMS_API FKnightGearRewardBundle
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
	TArray<FKnightGearRewardItem> Items;
	bool operator==(const FKnightGearRewardBundle& Other) const;
};

namespace KnightGear
{
	inline const FName InventoryLocation(TEXT("Inventory"));
	inline const FName EquipmentLocation(TEXT("Equipment"));
	inline const FName WeaponSocketLocation(TEXT("WeaponSocket"));
	inline const FName SkillSocketLocation(TEXT("SkillSocket"));
	inline constexpr int64 MaximumCurrency = 1000000000;
	inline constexpr int32 MaximumLevel = 100;
	inline constexpr int32 DemoCapacity = 128;
	KNIGHTITEMS_API const TArray<FGameplayTag>& EquipmentSlots();
	KNIGHTITEMS_API FGameplayTag SlotForSubtype(EKnightEquipmentSubtype Subtype);
	KNIGHTITEMS_API bool IsEquipmentSlot(FGameplayTag Slot);
}
