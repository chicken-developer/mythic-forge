#include "Assets/KnightAssetManager.h"

#include "Assets/KnightDefinition.h"
#include "Logging/KnightLog.h"
#include "Settings/KnightSettings.h"
#include "Types/KnightStableIds.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "KnightAssetManager"

UKnightAssetManager* UKnightAssetManager::GetKnightAssetManager()
{
	return Cast<UKnightAssetManager>(UAssetManager::GetIfInitialized());
}

void UKnightAssetManager::StartInitialLoading()
{
	Super::StartInitialLoading();
	const FKnightResult Result = ValidateDefinitionCatalog();
	if (!Result.IsSuccess())
	{
		UE_LOG(LogKnightAssets, Error, TEXT("Definition catalog validation failed: %s"), *Result.Message.ToString());
	}
}

FKnightResult UKnightAssetManager::ValidateDefinitionCatalog()
{
	check(IsInGameThread());
	DefinitionCatalog.Reset();
	DuplicateIds.Reset();
	CatalogErrors.Reset();
	ResolvedDefinitions.Reset();

	const UKnightSettings* Settings = GetDefault<UKnightSettings>();
	FAssetRegistryModule& RegistryModule =
		FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& Registry = RegistryModule.Get();
	Registry.ScanPathsSynchronous(Settings->DefinitionRoots);

	TArray<FAssetData> AssetData;
	Registry.GetAssetsByClass(UKnightDefinition::StaticClass()->GetClassPathName(), AssetData, true);
	TSet<FPrimaryAssetType> DefinitionTypes;
	for (const FAssetData& Asset : AssetData)
	{
		bool bIsInDefinitionRoot = false;
		const FString PackagePath = Asset.PackagePath.ToString();
		for (const FString& Root : Settings->DefinitionRoots)
		{
			if (PackagePath == Root || PackagePath.StartsWith(Root + TEXT("/")))
			{
				bIsInDefinitionRoot = true;
				break;
			}
		}
		if (!bIsInDefinitionRoot)
		{
			continue;
		}

		UKnightDefinition* Definition = Cast<UKnightDefinition>(Asset.GetAsset());
		if (Definition == nullptr)
		{
			CatalogErrors.Add(FKnightResult::Failure(EKnightError::LoadFailed, Asset.PackageName,
				LOCTEXT("CatalogLoadFailed", "A registered definition could not be loaded.")));
			continue;
		}
		const FKnightResult Validation = Definition->ValidateIdentity();
		if (!Validation.IsSuccess())
		{
			FKnightResult Error = Validation;
			Error.Context = Asset.PackageName;
			CatalogErrors.Add(Error);
			continue;
		}

		const FPrimaryAssetId Id = Definition->GetPrimaryAssetId();
		DefinitionTypes.Add(Id.PrimaryAssetType);
		if (DefinitionCatalog.Contains(Id) || DuplicateIds.Contains(Id))
		{
			DefinitionCatalog.Remove(Id);
			DuplicateIds.Add(Id);
			CatalogErrors.Add(FKnightResult::Failure(EKnightError::DuplicateId, FName(*Id.ToString()),
				FText::Format(LOCTEXT("DuplicateDefinition", "Definition ID {0} is duplicated at {1}."),
					FText::FromString(Id.ToString()), FText::FromName(Asset.PackageName))));
			continue;
		}
		DefinitionCatalog.Add(Id, Asset.GetSoftObjectPath());
	}

	PushBulkScanning();
	for (const FPrimaryAssetType& Type : DefinitionTypes)
	{
		ScanPathsForPrimaryAssets(Type, Settings->DefinitionRoots, UKnightDefinition::StaticClass(), false);
		FPrimaryAssetRules Rules;
		Rules.CookRule = EPrimaryAssetCookRule::AlwaysCook;
		SetPrimaryAssetTypeRules(Type, Rules);
	}
	PopBulkScanning();

	for (const FKnightResult& Error : CatalogErrors)
	{
		UE_LOG(LogKnightAssets, Error, TEXT("Definition validation [%s]: %s"),
			*Error.Context.ToString(), *Error.Message.ToString());
	}
	UE_LOG(LogKnightAssets, Display, TEXT("Definition catalog: %d valid definitions, %d errors"),
		DefinitionCatalog.Num(), CatalogErrors.Num());
	return CatalogErrors.IsEmpty() ? FKnightResult::Success() : CatalogErrors[0];
}

FKnightResult UKnightAssetManager::ResolveDefinition(const FPrimaryAssetId& DefinitionId,
	UKnightDefinition*& OutDefinition)
{
	return ResolveDefinition(DefinitionId, UKnightDefinition::StaticClass(), OutDefinition);
}

FKnightResult UKnightAssetManager::ResolveDefinition(const FPrimaryAssetId& DefinitionId, UClass* ExpectedClass,
	UKnightDefinition*& OutDefinition)
{
	check(IsInGameThread());
	OutDefinition = nullptr;
	const FName Context(*DefinitionId.ToString());
	if (!KnightIdentity::IsValidDefinitionId(DefinitionId))
	{
		return FKnightResult::Failure(EKnightError::InvalidId, Context,
			LOCTEXT("ResolveInvalidId", "The requested definition ID is invalid."));
	}
	if (ExpectedClass == nullptr || !ExpectedClass->IsChildOf(UKnightDefinition::StaticClass()))
	{
		return FKnightResult::Failure(EKnightError::TypeMismatch, Context,
			LOCTEXT("InvalidExpectedClass", "The requested class must derive from KnightDefinition."));
	}
	if (DuplicateIds.Contains(DefinitionId))
	{
		return FKnightResult::Failure(EKnightError::DuplicateId, Context,
			LOCTEXT("ResolveDuplicateId", "The requested definition ID is duplicated and cannot be resolved."));
	}
	const FSoftObjectPath* Path = DefinitionCatalog.Find(DefinitionId);
	if (Path == nullptr)
	{
		return FKnightResult::Failure(EKnightError::NotFound, Context,
			LOCTEXT("ResolveMissingId", "No registered definition matches the requested stable ID."));
	}

	UKnightDefinition* Definition = nullptr;
	if (const TObjectPtr<UKnightDefinition>* Existing = ResolvedDefinitions.Find(DefinitionId))
	{
		Definition = Existing->Get();
	}
	if (Definition == nullptr)
	{
		Definition = Cast<UKnightDefinition>(Path->TryLoad());
	}
	if (Definition == nullptr)
	{
		return FKnightResult::Failure(EKnightError::LoadFailed, Context,
			LOCTEXT("ResolveLoadFailed", "The definition package could not be loaded."));
	}
	if (!Definition->IsA(ExpectedClass))
	{
		return FKnightResult::Failure(EKnightError::TypeMismatch, Context,
			LOCTEXT("ResolveTypeMismatch", "The definition has a different type than requested."));
	}
	if (Definition->GetPrimaryAssetId() != DefinitionId)
	{
		return FKnightResult::Failure(EKnightError::InvalidData, Context,
			LOCTEXT("ResolveChangedIdentity", "The definition identity changed after catalog validation."));
	}
	ResolvedDefinitions.Add(DefinitionId, Definition);
	OutDefinition = Definition;
	return FKnightResult::Success();
}

const TArray<FKnightResult>& UKnightAssetManager::GetCatalogErrors() const
{
	return CatalogErrors;
}

int32 UKnightAssetManager::GetDefinitionCount() const
{
	return DefinitionCatalog.Num();
}

TArray<FKnightResult> UKnightAssetManager::ValidateDefinitionIdentities(
	TConstArrayView<UKnightDefinition*> Definitions)
{
	TArray<FKnightResult> Errors;
	TSet<FPrimaryAssetId> Ids;
	for (const UKnightDefinition* Definition : Definitions)
	{
		if (Definition == nullptr)
		{
			Errors.Add(FKnightResult::Failure(EKnightError::InvalidData, NAME_None,
				LOCTEXT("NullDefinition", "A definition batch contains a null entry.")));
			continue;
		}
		const FKnightResult IdentityResult = Definition->ValidateIdentity();
		if (!IdentityResult.IsSuccess())
		{
			Errors.Add(IdentityResult);
			continue;
		}
		const FPrimaryAssetId Id = Definition->GetPrimaryAssetId();
		if (Ids.Contains(Id))
		{
			Errors.Add(FKnightResult::Failure(EKnightError::DuplicateId, FName(*Id.ToString()),
				LOCTEXT("DuplicateBatchId", "A definition batch contains a duplicated stable ID.")));
		}
		Ids.Add(Id);
	}
	return Errors;
}

#undef LOCTEXT_NAMESPACE
