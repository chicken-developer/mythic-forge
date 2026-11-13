#include "Assets/GolemAssetManager.h"

#include "Assets/GolemDefinition.h"
#include "Logging/GolemLog.h"
#include "Settings/GolemSettings.h"
#include "Types/GolemStableIds.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "GolemAssetManager"

UGolemAssetManager* UGolemAssetManager::GetGolemAssetManager()
{
	return Cast<UGolemAssetManager>(UAssetManager::GetIfInitialized());
}

void UGolemAssetManager::StartInitialLoading()
{
	Super::StartInitialLoading();
	const FGolemResult Result = ValidateDefinitionCatalog();
	if (!Result.IsSuccess())
	{
		UE_LOG(LogGolemAssets, Error, TEXT("Definition catalog validation failed: %s"), *Result.Message.ToString());
	}
}

FGolemResult UGolemAssetManager::ValidateDefinitionCatalog()
{
	check(IsInGameThread());
	DefinitionCatalog.Reset();
	DuplicateIds.Reset();
	CatalogErrors.Reset();
	ResolvedDefinitions.Reset();

	const UGolemSettings* Settings = GetDefault<UGolemSettings>();
	FAssetRegistryModule& RegistryModule =
		FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& Registry = RegistryModule.Get();
	Registry.ScanPathsSynchronous(Settings->DefinitionRoots);

	TArray<FAssetData> AssetData;
	Registry.GetAssetsByClass(UGolemDefinition::StaticClass()->GetClassPathName(), AssetData, true);
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

		UGolemDefinition* Definition = Cast<UGolemDefinition>(Asset.GetAsset());
		if (Definition == nullptr)
		{
			CatalogErrors.Add(FGolemResult::Failure(EGolemError::LoadFailed, Asset.PackageName,
				LOCTEXT("CatalogLoadFailed", "A registered definition could not be loaded.")));
			continue;
		}
		const FGolemResult Validation = Definition->ValidateIdentity();
		if (!Validation.IsSuccess())
		{
			FGolemResult Error = Validation;
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
			CatalogErrors.Add(FGolemResult::Failure(EGolemError::DuplicateId, FName(*Id.ToString()),
				FText::Format(LOCTEXT("DuplicateDefinition", "Definition ID {0} is duplicated at {1}."),
					FText::FromString(Id.ToString()), FText::FromName(Asset.PackageName))));
			continue;
		}
		DefinitionCatalog.Add(Id, Asset.GetSoftObjectPath());
	}

	PushBulkScanning();
	for (const FPrimaryAssetType& Type : DefinitionTypes)
	{
		ScanPathsForPrimaryAssets(Type, Settings->DefinitionRoots, UGolemDefinition::StaticClass(), false);
		FPrimaryAssetRules Rules;
		Rules.CookRule = EPrimaryAssetCookRule::AlwaysCook;
		SetPrimaryAssetTypeRules(Type, Rules);
	}
	PopBulkScanning();

	for (const FGolemResult& Error : CatalogErrors)
	{
		UE_LOG(LogGolemAssets, Error, TEXT("Definition validation [%s]: %s"),
			*Error.Context.ToString(), *Error.Message.ToString());
	}
	UE_LOG(LogGolemAssets, Display, TEXT("Definition catalog: %d valid definitions, %d errors"),
		DefinitionCatalog.Num(), CatalogErrors.Num());
	return CatalogErrors.IsEmpty() ? FGolemResult::Success() : CatalogErrors[0];
}

FGolemResult UGolemAssetManager::ResolveDefinition(const FPrimaryAssetId& DefinitionId,
	UGolemDefinition*& OutDefinition)
{
	return ResolveDefinition(DefinitionId, UGolemDefinition::StaticClass(), OutDefinition);
}

FGolemResult UGolemAssetManager::ResolveDefinition(const FPrimaryAssetId& DefinitionId, UClass* ExpectedClass,
	UGolemDefinition*& OutDefinition)
{
	check(IsInGameThread());
	OutDefinition = nullptr;
	const FName Context(*DefinitionId.ToString());
	if (!GolemIdentity::IsValidDefinitionId(DefinitionId))
	{
		return FGolemResult::Failure(EGolemError::InvalidId, Context,
			LOCTEXT("ResolveInvalidId", "The requested definition ID is invalid."));
	}
	if (ExpectedClass == nullptr || !ExpectedClass->IsChildOf(UGolemDefinition::StaticClass()))
	{
		return FGolemResult::Failure(EGolemError::TypeMismatch, Context,
			LOCTEXT("InvalidExpectedClass", "The requested class must derive from GolemDefinition."));
	}
	if (DuplicateIds.Contains(DefinitionId))
	{
		return FGolemResult::Failure(EGolemError::DuplicateId, Context,
			LOCTEXT("ResolveDuplicateId", "The requested definition ID is duplicated and cannot be resolved."));
	}
	const FSoftObjectPath* Path = DefinitionCatalog.Find(DefinitionId);
	if (Path == nullptr)
	{
		return FGolemResult::Failure(EGolemError::NotFound, Context,
			LOCTEXT("ResolveMissingId", "No registered definition matches the requested stable ID."));
	}

	UGolemDefinition* Definition = nullptr;
	if (const TObjectPtr<UGolemDefinition>* Existing = ResolvedDefinitions.Find(DefinitionId))
	{
		Definition = Existing->Get();
	}
	if (Definition == nullptr)
	{
		Definition = Cast<UGolemDefinition>(Path->TryLoad());
	}
	if (Definition == nullptr)
	{
		return FGolemResult::Failure(EGolemError::LoadFailed, Context,
			LOCTEXT("ResolveLoadFailed", "The definition package could not be loaded."));
	}
	if (!Definition->IsA(ExpectedClass))
	{
		return FGolemResult::Failure(EGolemError::TypeMismatch, Context,
			LOCTEXT("ResolveTypeMismatch", "The definition has a different type than requested."));
	}
	if (Definition->GetPrimaryAssetId() != DefinitionId)
	{
		return FGolemResult::Failure(EGolemError::InvalidData, Context,
			LOCTEXT("ResolveChangedIdentity", "The definition identity changed after catalog validation."));
	}
	ResolvedDefinitions.Add(DefinitionId, Definition);
	OutDefinition = Definition;
	return FGolemResult::Success();
}

const TArray<FGolemResult>& UGolemAssetManager::GetCatalogErrors() const
{
	return CatalogErrors;
}

int32 UGolemAssetManager::GetDefinitionCount() const
{
	return DefinitionCatalog.Num();
}

TArray<FGolemResult> UGolemAssetManager::ValidateDefinitionIdentities(
	TConstArrayView<UGolemDefinition*> Definitions)
{
	TArray<FGolemResult> Errors;
	TSet<FPrimaryAssetId> Ids;
	for (const UGolemDefinition* Definition : Definitions)
	{
		if (Definition == nullptr)
		{
			Errors.Add(FGolemResult::Failure(EGolemError::InvalidData, NAME_None,
				LOCTEXT("NullDefinition", "A definition batch contains a null entry.")));
			continue;
		}
		const FGolemResult IdentityResult = Definition->ValidateIdentity();
		if (!IdentityResult.IsSuccess())
		{
			Errors.Add(IdentityResult);
			continue;
		}
		const FPrimaryAssetId Id = Definition->GetPrimaryAssetId();
		if (Ids.Contains(Id))
		{
			Errors.Add(FGolemResult::Failure(EGolemError::DuplicateId, FName(*Id.ToString()),
				LOCTEXT("DuplicateBatchId", "A definition batch contains a duplicated stable ID.")));
		}
		Ids.Add(Id);
	}
	return Errors;
}

#undef LOCTEXT_NAMESPACE
