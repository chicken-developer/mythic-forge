#include "Commandlets/KnightCombatAssetAuthoring.h"

#include "Abilities/KnightBlockAbility.h"
#include "Abilities/KnightDodgeAbility.h"
#include "Abilities/KnightFinisherAbility.h"
#include "Abilities/KnightHeavyAttackAbility.h"
#include "Abilities/KnightLightAttackAbility.h"
#include "Abilities/KnightLockTargetAbility.h"
#include "Abilities/KnightParryAbility.h"
#include "Abilities/KnightRuneAbility.h"
#include "Abilities/KnightTraversalAbility.h"
#include "Animation/KnightAnimNotifyState_ComboWindow.h"
#include "Animation/KnightAnimNotifyState_TraceWindow.h"
#include "Definitions/KnightAbilitySetDefinition.h"
#include "Definitions/KnightCombatProfileDefinition.h"
#include "Tags/KnightGameplayTags.h"

#include "Animation/AnimData/IAnimationDataController.h"
#include "Animation/AnimData/IAnimationDataModel.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "AnimSequenceLevelSequenceLink.h"
#include "AnimNotifyState_MotionWarping.h"
#include "AssetCompilingManager.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/SkeletalMesh.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Misc/SecureHash.h"
#include "RootMotionModifier_SkewWarp.h"
#include "Serialization/ArchiveUObject.h"
#include "UObject/LinkerInstancingContext.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/SoftObjectPtr.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"

DEFINE_LOG_CATEGORY_STATIC(LogKnightCombatAuthoring, Log, All);

namespace KnightCombatAssetAuthoring
{
	const FString VendorRoot = TEXT("/Game/Knight/Vendor/UE58/Mannequins/");
	const FString OriginalRoot = TEXT("/Game/Characters/Mannequins/");
	const FString AnimationRoot = TEXT("/Game/Knight/Combat/Animations/");

	bool SaveAsset(UObject* Asset)
	{
		if (!IsValid(Asset) || !Asset->GetPackage()->GetName().StartsWith(TEXT("/Game/Knight/")))
		{
			UE_LOG(LogKnightCombatAuthoring, Error, TEXT("Combat authoring may save only owned Knight packages."));
			return false;
		}
		UPackage* Package = Asset->GetPackage();
		const FString Filename = FPaths::ConvertRelativePathToFull(
			FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension()));
		const FString OwnedContentRoot = FPaths::ConvertRelativePathToFull(
			FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Knight/")));
		if (!FPaths::IsUnderDirectory(Filename, OwnedContentRoot))
		{
			UE_LOG(LogKnightCombatAuthoring, Error, TEXT("Combat save escaped project Knight content: %s."), *Filename);
			return false;
		}
		if (!IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true))
		{
			return false;
		}
		Asset->SetFlags(RF_Public | RF_Standalone);
		Package->MarkPackageDirty();
		FSavePackageArgs Arguments;
		Arguments.TopLevelFlags = RF_Public | RF_Standalone;
		Arguments.SaveFlags = SAVE_NoError;
		Arguments.bSlowTask = false;
		if (!UPackage::SavePackage(Package, Asset, *Filename, Arguments)
			|| IFileManager::Get().FileSize(*Filename) <= 0)
		{
			UE_LOG(LogKnightCombatAuthoring, Error, TEXT("Failed to save combat asset %s."), *Filename);
			return false;
		}
		UE_LOG(LogKnightCombatAuthoring, Display, TEXT("Saved %s (%lld bytes)."),
			*Filename, IFileManager::Get().FileSize(*Filename));
		return true;
	}

	/** Collects both hard and soft package references without mutating imported UObject data. */
	class FKnightDependencyCollector final : public FArchiveUObject
	{
	public:
		FKnightDependencyCollector()
		{
			ArIsObjectReferenceCollector = true;
			ArIgnoreOuterRef = true;
			ArIgnoreArchetypeRef = true;
		}

		using FArchiveUObject::operator<<;

		virtual FArchive& operator<<(UObject*& Object) override
		{
			if (Object != nullptr)
			{
				RecordPackage(Object->GetOutermost()->GetName());
			}
			return *this;
		}

		virtual FArchive& operator<<(FSoftObjectPath& Path) override
		{
			RecordSoftPath(Path);
			return *this;
		}

		virtual FArchive& operator<<(FSoftObjectPtr& Object) override
		{
			// FArchiveUObject skips soft pointers for reference collectors unless this overload is explicit.
			RecordSoftPath(Object.GetUniqueID());
			return *this;
		}

		TSet<FString> VendorDependencies;
		TSet<FSoftObjectPath> VendorSoftReferences;
		TSet<FString> InvalidDependencies;

	private:
		void RecordSoftPath(const FSoftObjectPath& Path)
		{
			if (!Path.IsNull())
			{
				const FString PackageName = Path.GetLongPackageName();
				RecordPackage(PackageName);
				if (PackageName.StartsWith(VendorRoot))
				{
					VendorSoftReferences.Add(Path);
				}
			}
		}

		void RecordPackage(const FString& PackageName)
		{
			if (PackageName.StartsWith(VendorRoot))
			{
				VendorDependencies.Add(PackageName);
			}
			else if (PackageName.StartsWith(TEXT("/Game/")) && !PackageName.StartsWith(TEXT("/Game/Knight/")))
			{
				InvalidDependencies.Add(PackageName);
			}
		}
	};

	void CollectDependencies(UPackage* Package, FKnightDependencyCollector& Collector)
	{
		ForEachObjectWithOuter(Package, [&Collector](UObject* Object)
		{
			Object->Serialize(Collector);
		}, EGetObjectsFlags::IncludeNestedObjects);
	}

	bool RemoveUnavailableEditorSequenceLink(UPackage* Package)
	{
		UAnimSequence* Sequence = FindObject<UAnimSequence>(Package,
			*FPackageName::GetLongPackageAssetName(Package->GetName()));
		if (Sequence == nullptr)
		{
			return true;
		}
		UAnimSequenceLevelSequenceLink* Link = Cast<UAnimSequenceLevelSequenceLink>(
			Sequence->GetAssetUserDataOfClass(UAnimSequenceLevelSequenceLink::StaticClass()));
		if (Link == nullptr || Link->PathToLevelSequence.IsNull())
		{
			return true;
		}
		const FString LinkPackage = Link->PathToLevelSequence.GetLongPackageName();
		if (!LinkPackage.StartsWith(OriginalRoot + TEXT("Animations/EditableAnimations/"))
			|| FPackageName::DoesPackageExist(LinkPackage))
		{
			return true;
		}
		if (!Link->IsEditorOnly())
		{
			UE_LOG(LogKnightCombatAuthoring, Error, TEXT("A missing sequence link is not editor-only: %s."),
				*Link->GetPathName());
			return false;
		}
		// Installed templates omit their source Sequencer scenes. This authoring backlink is explicitly
		// editor-only; removing it preserves the baked pose, notifies, skeleton and all runtime references.
		// Clear the orphan's path as well because dependency validation inspects every nested UObject.
		const FString RemovedPath = Link->PathToLevelSequence.ToString();
		Link->PathToLevelSequence.Reset();
		Sequence->RemoveUserDataOfClass(UAnimSequenceLevelSequenceLink::StaticClass());
		Sequence->MarkPackageDirty();
		UE_LOG(LogKnightCombatAuthoring, Display, TEXT("Removed unavailable editor-only %s backlink %s from %s."),
			*Link->GetClass()->GetPathName(), *RemovedPath, *Sequence->GetPathName());
		return true;
	}

	bool ImportMannequinAssets()
	{
		const FString SourceDirectory = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::EngineDir(),
			TEXT("../Templates/TemplateResources/High/Characters/Content/Mannequins/")));
		if (!IFileManager::Get().DirectoryExists(*SourceDirectory))
		{
			UE_LOG(LogKnightCombatAuthoring, Error, TEXT("Installed UE58 mannequin template content is unavailable: %s."),
				*SourceDirectory);
			return false;
		}
		TArray<FString> SourceFiles;
		IFileManager::Get().FindFilesRecursive(SourceFiles, *SourceDirectory, TEXT("*.uasset"), true, false);
		TMap<FString, FMD5Hash> SourceHashes;
		for (const FString& Filename : SourceFiles)
		{
			const FMD5Hash Hash = FMD5Hash::HashFile(*Filename);
			if (!Hash.IsValid())
			{
				UE_LOG(LogKnightCombatAuthoring, Error, TEXT("Cannot hash readonly template source %s."), *Filename);
				return false;
			}
			SourceHashes.Add(Filename, Hash);
		}
		FLinkerInstancingContext InstancingContext(true);
		TMap<FString, FString> OriginalPackages;
		for (const FString& Filename : SourceFiles)
		{
			FString RelativePath = Filename;
			if (!FPaths::MakePathRelativeTo(RelativePath, *SourceDirectory) || RelativePath.StartsWith(TEXT("..")))
			{
				UE_LOG(LogKnightCombatAuthoring, Error, TEXT("Template source escaped its verified root: %s."), *Filename);
				return false;
			}
			const FString RelativePackage = FPaths::ChangeExtension(RelativePath, TEXT(""));
			const FString OriginalPackage = OriginalRoot + RelativePackage;
			const FString DestinationPackage = VendorRoot + RelativePackage;
			OriginalPackages.Add(DestinationPackage, OriginalPackage);
			InstancingContext.AddPackageMapping(FName(*OriginalPackage), FName(*DestinationPackage));
			const FString AssetName = FPackageName::GetLongPackageAssetName(OriginalPackage);
			InstancingContext.AddPathMapping(FSoftObjectPath(OriginalPackage + TEXT(".") + AssetName),
				FSoftObjectPath(DestinationPackage + TEXT(".") + AssetName));
		}

		// Imports resolve read-only template files while every loaded UObject belongs to the mapped Knight package.
		FPackageName::RegisterMountPoint(OriginalRoot, SourceDirectory);
		ON_SCOPE_EXIT
		{
			FPackageName::UnRegisterMountPoint(OriginalRoot, SourceDirectory);
		};
		TArray<FString> PendingPackages =
		{
			// The skeleton's soft preview mesh must be available before its first PostLoad lookup.
			VendorRoot + TEXT("Meshes/SKM_Quinn_Simple"),
			VendorRoot + TEXT("Meshes/SKM_Manny_Simple"),
			VendorRoot + TEXT("Anims/Unarmed/MM_Idle"),
			VendorRoot + TEXT("Anims/Unarmed/Walk/MF_Unarmed_Walk_Fwd"),
			VendorRoot + TEXT("Anims/Unarmed/Attack/MM_Attack_01"),
			VendorRoot + TEXT("Anims/Unarmed/Attack/MM_Attack_02"),
			VendorRoot + TEXT("Anims/Unarmed/Attack/MM_Attack_03"),
			VendorRoot + TEXT("Anims/Unarmed/Attack/MM_ChargedAttack"),
			VendorRoot + TEXT("Anims/Unarmed/Jump/MM_Dash"),
			VendorRoot + TEXT("Anims/Unarmed/Jump/MM_Jump"),
			VendorRoot + TEXT("Anims/Rifle/HitReact/MM_HitReact_Front_Hvy_01"),
			VendorRoot + TEXT("Anims/Death/MM_Death_Front_01")
		};
		TSet<FString> VisitedPackages;
		TArray<TStrongObjectPtr<UPackage>> ImportedPackages;
		for (int32 Index = 0; Index < PendingPackages.Num(); ++Index)
		{
			const FString Destination = PendingPackages[Index];
			if (VisitedPackages.Contains(Destination))
			{
				continue;
			}
			VisitedPackages.Add(Destination);
			const FString* Original = OriginalPackages.Find(Destination);
			if (Original == nullptr)
			{
				UE_LOG(LogKnightCombatAuthoring, Error, TEXT("Vendor dependency has no verified template source: %s."),
					*Destination);
				return false;
			}
			UPackage* Package = FPackageName::DoesPackageExist(Destination)
				? LoadPackage(nullptr, *Destination, LOAD_None)
				: LoadPackage(CreatePackage(*Destination), **Original, LOAD_None, nullptr, &InstancingContext);
			if (Package == nullptr || Package->GetName() != Destination)
			{
				UE_LOG(LogKnightCombatAuthoring, Error, TEXT("Instanced template import failed: %s."), *Destination);
				return false;
			}
			ImportedPackages.Emplace(Package);
			if (!RemoveUnavailableEditorSequenceLink(Package))
			{
				return false;
			}
			FKnightDependencyCollector Collector;
			CollectDependencies(Package, Collector);
			if (!Collector.InvalidDependencies.IsEmpty())
			{
				for (const FString& Dependency : Collector.InvalidDependencies)
				{
					UE_LOG(LogKnightCombatAuthoring, Error, TEXT("Unremapped dependency %s survives in %s."),
						*Dependency, *Destination);
				}
				return false;
			}
			for (const FString& Dependency : Collector.VendorDependencies)
			{
				PendingPackages.AddUnique(Dependency);
			}
		}
		FAssetCompilingManager::Get().FinishAllCompilation();
		for (const TStrongObjectPtr<UPackage>& Package : ImportedPackages)
		{
			UObject* Asset = FindObject<UObject>(Package.Get(),
				*FPackageName::GetLongPackageAssetName(Package->GetName()));
			if (Asset == nullptr || !Asset->IsAsset())
			{
				UE_LOG(LogKnightCombatAuthoring, Error, TEXT("Imported package has no expected real asset: %s."),
					*Package->GetName());
				return false;
			}
			FAssetRegistryModule::AssetCreated(Asset);
			if (!SaveAsset(Asset))
			{
				return false;
			}
		}
		FKnightDependencyCollector Closure;
		for (const TStrongObjectPtr<UPackage>& Package : ImportedPackages)
		{
			CollectDependencies(Package.Get(), Closure);
		}
		if (!Closure.InvalidDependencies.IsEmpty())
		{
			for (const FString& Dependency : Closure.InvalidDependencies)
			{
				UE_LOG(LogKnightCombatAuthoring, Error, TEXT("Unremapped dependency survives import closure: %s."),
					*Dependency);
			}
			return false;
		}
		for (const FString& Dependency : Closure.VendorDependencies)
		{
			const FString Filename = FPackageName::LongPackageNameToFilename(
				Dependency, FPackageName::GetAssetPackageExtension());
			UPackage* Package = FindObject<UPackage>(nullptr, *Dependency);
			UObject* Asset = Package != nullptr
				? FindObject<UObject>(Package, *FPackageName::GetLongPackageAssetName(Dependency)) : nullptr;
			if (!VisitedPackages.Contains(Dependency) || IFileManager::Get().FileSize(*Filename) <= 0
				|| Asset == nullptr || !Asset->IsAsset())
			{
				UE_LOG(LogKnightCombatAuthoring, Error, TEXT("Vendor closure has an unsaved or unresolved package: %s."),
					*Dependency);
				return false;
			}
		}
		for (const FSoftObjectPath& Reference : Closure.VendorSoftReferences)
		{
			UObject* Object = Reference.ResolveObject();
			if (Object == nullptr)
			{
				Object = Reference.TryLoad();
			}
			if (!IsValid(Object) || Object->GetOutermost()->GetName() != Reference.GetLongPackageName())
			{
				UE_LOG(LogKnightCombatAuthoring, Error, TEXT("Vendor soft reference does not resolve to its owned asset: %s."),
					*Reference.ToString());
				return false;
			}
		}
		UE_LOG(LogKnightCombatAuthoring, Display, TEXT("Imported and validated %d self-contained UE58 template packages."),
			ImportedPackages.Num());
		FString Provenance = TEXT("SourceFilename\tMD5Before\tMD5After\tResult\n");
		for (const TPair<FString, FMD5Hash>& Source : SourceHashes)
		{
			const FMD5Hash AfterHash = FMD5Hash::HashFile(*Source.Key);
			Provenance += FString::Printf(TEXT("%s\t%s\t%s\t%s\n"), *Source.Key,
				*LexToString(Source.Value), *LexToString(AfterHash),
				Source.Value == AfterHash ? TEXT("UNCHANGED") : TEXT("CHANGED"));
			if (Source.Value != AfterHash)
			{
				UE_LOG(LogKnightCombatAuthoring, Error, TEXT("Readonly engine source hash changed: %s."), *Source.Key);
				return false;
			}
		}
		const FString EvidenceDirectory = FPaths::ConvertRelativePathToFull(
			FPaths::Combine(FPaths::ProjectDir(), TEXT("Evidence/M3/")));
		if (!IFileManager::Get().MakeDirectory(*EvidenceDirectory, true)
			|| !FFileHelper::SaveStringToFile(Provenance,
				*FPaths::Combine(EvidenceDirectory, TEXT("UE58_template_provenance.tsv"))))
		{
			UE_LOG(LogKnightCombatAuthoring, Error, TEXT("Cannot record template import provenance."));
			return false;
		}
		return true;
	}

	template<typename TAsset>
	TAsset* FindOrCreateAsset(const FString& PackageName, bool& bOutCreated)
	{
		bOutCreated = !FPackageName::DoesPackageExist(PackageName);
		UPackage* Package = bOutCreated ? CreatePackage(*PackageName) : LoadPackage(nullptr, *PackageName, LOAD_None);
		if (Package == nullptr)
		{
			return nullptr;
		}
		const FName AssetName(*FPackageName::GetLongPackageAssetName(PackageName));
		TAsset* Asset = FindObject<TAsset>(Package, *AssetName.ToString());
		if (bOutCreated && Asset == nullptr)
		{
			Asset = NewObject<TAsset>(Package, AssetName, RF_Public | RF_Standalone);
			FAssetRegistryModule::AssetCreated(Asset);
		}
		return Asset;
	}

	UAnimSequence* LoadSequence(const TCHAR* RelativeName)
	{
		const FString PackageName = VendorRoot + RelativeName;
		return LoadObject<UAnimSequence>(nullptr,
			*(PackageName + TEXT(".") + FPackageName::GetLongPackageAssetName(PackageName)));
	}

	bool ValidateTraversalSequence(UAnimSequence* Sequence)
	{
		if (Sequence == nullptr || !Sequence->bEnableRootMotion || Sequence->GetPlayLength() <= 0.0f)
		{
			return false;
		}
		const FAnimExtractContext Context(0.0, true);
		const FVector Delta = Sequence->ExtractRootMotionFromRange(0.0, Sequence->GetPlayLength(), Context).GetTranslation();
		const FVector HalfDelta = Sequence->ExtractRootMotionFromRange(0.0,
			Sequence->GetPlayLength() * 0.5, Context).GetTranslation();
		if (Delta.ContainsNaN() || HalfDelta.ContainsNaN() || Delta.X < 299.0 || FMath::Abs(Delta.Z) > 1.0
			|| HalfDelta.Z < 100.0)
		{
			UE_LOG(LogKnightCombatAuthoring, Error, TEXT("Traversal has no valid authored root trajectory: end %s, mid %s."),
				*Delta.ToString(), *HalfDelta.ToString());
			return false;
		}
		UE_LOG(LogKnightCombatAuthoring, Display, TEXT("Traversal real CPU root motion: full %s, midpoint %s."),
			*Delta.ToString(), *HalfDelta.ToString());
		return true;
	}

	UAnimSequence* CreateTraversalSequence()
	{
		const FString PackageName = AnimationRoot + TEXT("AS_Knight_Traversal");
		UAnimSequence* Sequence = nullptr;
		if (FPackageName::DoesPackageExist(PackageName))
		{
			Sequence = LoadObject<UAnimSequence>(nullptr, *(PackageName + TEXT(".AS_Knight_Traversal")));
		}
		else
		{
			UAnimSequence* Source = LoadSequence(TEXT("Anims/Unarmed/Jump/MM_Jump"));
			if (Source == nullptr || Source->GetSkeleton() == nullptr)
			{
				return nullptr;
			}
			const IAnimationDataModel* SourceModel = Source->GetDataModel();
			if (SourceModel == nullptr)
			{
				return nullptr;
			}
			Sequence = NewObject<UAnimSequence>(CreatePackage(*PackageName), TEXT("AS_Knight_Traversal"),
				RF_Public | RF_Standalone);
			if (Sequence == nullptr || Sequence->GetDataModel() == nullptr)
			{
				return nullptr;
			}
			const FReferenceSkeleton& Reference = Source->GetSkeleton()->GetReferenceSkeleton();
			if (Reference.GetNum() == 0 || SourceModel->GetNumberOfKeys() < 2)
			{
				return nullptr;
			}
			const FName RootName = Reference.GetBoneName(0);
			const FTransform RootPose = Reference.GetRefBonePose()[0];
			TArray<FVector3f> Positions;
			TArray<FQuat4f> Rotations;
			TArray<FVector3f> Scales;
			for (int32 Key = 0; Key < SourceModel->GetNumberOfKeys(); ++Key)
			{
				const float Alpha = static_cast<float>(Key) / static_cast<float>(SourceModel->GetNumberOfKeys() - 1);
				Positions.Add(FVector3f(RootPose.GetTranslation())
					+ FVector3f(300.0f * Alpha, 0.0f, 120.0f * FMath::Sin(PI * Alpha)));
				Rotations.Add(FQuat4f(RootPose.GetRotation()));
				Scales.Add(FVector3f(RootPose.GetScale3D()));
			}
			{
				IAnimationDataController& Controller = Sequence->GetController();
				IAnimationDataController::FScopedBracket Bracket(Controller,
					NSLOCTEXT("KnightCombatAuthoring", "RootTrajectory", "Author Knight traversal root motion"), false);
				Sequence->SetSkeleton(Source->GetSkeleton());
				Sequence->BoneCompressionSettings = Source->BoneCompressionSettings;
				Sequence->CurveCompressionSettings = Source->CurveCompressionSettings;
				// Copy the real pose, tracks, curves and attributes, without unrelated FBX reimport metadata.
				// Full UObject duplication broadcasts model population before import-data PostLoad completes.
				Controller.PopulateWithExistingModel(Source->GetDataModelInterface());
				const IAnimationDataModel* Model = Sequence->GetDataModel();
				if (Model->GetNumberOfKeys() != SourceModel->GetNumberOfKeys()
					|| Model->GetNumBoneTracks() != SourceModel->GetNumBoneTracks())
				{
					UE_LOG(LogKnightCombatAuthoring, Error, TEXT("Traversal pose copy lost source keys or bone tracks."));
					return nullptr;
				}
				if ((!Model->IsValidBoneTrackName(RootName) && !Controller.AddBoneCurve(RootName, false))
					|| !Controller.SetBoneTrackKeys(RootName, Positions, Rotations, Scales, false))
				{
					return nullptr;
				}
				Sequence->bEnableRootMotion = true;
				Sequence->RootMotionRootLock = ERootMotionRootLock::AnimFirstFrame;
				Controller.NotifyPopulated();
			}
			UE_LOG(LogKnightCombatAuthoring, Display,
				TEXT("Traversal copied real animation model from %s: %d keys, %d bone tracks."),
				*Source->GetPathName(), Sequence->GetDataModel()->GetNumberOfKeys(),
				Sequence->GetDataModel()->GetNumBoneTracks());
			Sequence->PostEditChange();
			FAssetRegistryModule::AssetCreated(Sequence);
			FAssetCompilingManager::Get().FinishAllCompilation();
		}
		return ValidateTraversalSequence(Sequence) && SaveAsset(Sequence) ? Sequence : nullptr;
	}

	void AddNotifyWindow(UAnimMontage* Montage, UAnimNotifyState* State, const FName Name,
		const float StartTime, const float Duration, const int32 TrackIndex)
	{
		while (Montage->AnimNotifyTracks.Num() <= TrackIndex)
		{
			Montage->AnimNotifyTracks.Emplace(
				FName(*FString::Printf(TEXT("KnightTrack%d"), Montage->AnimNotifyTracks.Num())), FLinearColor::White);
		}
		FAnimNotifyEvent& Notify = Montage->Notifies.AddDefaulted_GetRef();
		Notify.NotifyStateClass = State;
		Notify.NotifyName = Name;
		Notify.TrackIndex = TrackIndex;
		Notify.Link(Montage, StartTime);
		Notify.SetDuration(Duration);
		Notify.EndLink.Link(Montage, StartTime + Duration);
		Notify.MontageTickType = EMontageNotifyTickType::BranchingPoint;
	}

	bool RepairAndValidateNotifyTracks(UAnimMontage* Montage)
	{
		int32 RequiredTrackCount = 1;
		for (const FAnimNotifyEvent& Notify : Montage->Notifies)
		{
			if (Notify.TrackIndex < 0 || Notify.TrackIndex > 20)
			{
				UE_LOG(LogKnightCombatAuthoring, Error, TEXT("Owned montage has an invalid notify track: %s, %s, %d."),
					*Montage->GetPathName(), *Notify.NotifyName.ToString(), Notify.TrackIndex);
				return false;
			}
			RequiredTrackCount = FMath::Max(RequiredTrackCount, Notify.TrackIndex + 1);
		}
		const int32 PreviousTrackCount = Montage->AnimNotifyTracks.Num();
		while (Montage->AnimNotifyTracks.Num() < RequiredTrackCount)
		{
			Montage->AnimNotifyTracks.Emplace(
				FName(*FString::Printf(TEXT("KnightTrack%d"), Montage->AnimNotifyTracks.Num())), FLinearColor::White);
		}
		if (Montage->AnimNotifyTracks.Num() != PreviousTrackCount)
		{
			UE_LOG(LogKnightCombatAuthoring, Display, TEXT("Repaired owned montage notify tracks %d -> %d: %s."),
				PreviousTrackCount, Montage->AnimNotifyTracks.Num(), *Montage->GetPathName());
			Montage->MarkPackageDirty();
		}
		Montage->SortNotifies();
		Montage->RefreshCacheData();
		for (const FAnimNotifyEvent& Notify : Montage->Notifies)
		{
			if (!Montage->AnimNotifyTracks.IsValidIndex(Notify.TrackIndex)
				|| !Montage->AnimNotifyTracks[Notify.TrackIndex].Notifies.Contains(&Notify))
			{
				UE_LOG(LogKnightCombatAuthoring, Error, TEXT("Notify cache does not match the authored track: %s, %s."),
					*Montage->GetPathName(), *Notify.NotifyName.ToString());
				return false;
			}
		}
		return true;
	}

	bool ValidateMontageWindows(UAnimMontage* Montage, const TArray<FName>& Sections,
		const bool bAttack, const bool bCombo, const bool bTraversal)
	{
		for (const FName Section : Sections)
		{
			const int32 SectionIndex = Montage->GetSectionIndex(Section);
			if (SectionIndex == INDEX_NONE)
			{
				UE_LOG(LogKnightCombatAuthoring, Error, TEXT("Existing montage lacks expected section %s: %s."),
					*Section.ToString(), *Montage->GetPathName());
				return false;
			}
			float SectionStart = 0.0f;
			float SectionEnd = 0.0f;
			Montage->GetSectionStartAndEndTime(SectionIndex, SectionStart, SectionEnd);
			bool bHasTrace = !bAttack;
			bool bHasCombo = !bCombo;
			bool bHasWarp = !bTraversal;
			for (const FAnimNotifyEvent& Notify : Montage->Notifies)
			{
				if (!FMath::IsFinite(Notify.GetTime()) || !FMath::IsFinite(Notify.GetDuration())
					|| Notify.GetDuration() <= 0.0f || Notify.NotifyStateClass == nullptr
					|| Notify.NotifyStateClass->GetOuter() != Montage)
				{
					UE_LOG(LogKnightCombatAuthoring, Error, TEXT("Owned montage has an invalid native notify window: %s, %s."),
						*Montage->GetPathName(), *Notify.NotifyName.ToString());
					return false;
				}
				if (Notify.GetTime() < SectionStart - UE_KINDA_SMALL_NUMBER
					|| Notify.GetEndTriggerTime() > SectionEnd + UE_KINDA_SMALL_NUMBER)
				{
					continue;
				}
				if (const UKnightAnimNotifyState_TraceWindow* Trace =
					Cast<UKnightAnimNotifyState_TraceWindow>(Notify.NotifyStateClass))
				{
					bHasTrace |= Trace->WindowId == Section;
				}
				bHasCombo |= Notify.NotifyStateClass->IsA<UKnightAnimNotifyState_ComboWindow>();
				if (const UAnimNotifyState_MotionWarping* WarpNotify =
					Cast<UAnimNotifyState_MotionWarping>(Notify.NotifyStateClass))
				{
					const URootMotionModifier_SkewWarp* Warp = Cast<URootMotionModifier_SkewWarp>(WarpNotify->RootMotionModifier);
					bHasWarp |= Warp != nullptr && Warp->WarpTargetName == FName(TEXT("KnightTraversal"))
						&& Warp->bWarpTranslation && Warp->bWarpRotation && !Warp->bWarpToFeetLocation && !Warp->bIgnoreZAxis;
				}
			}
			if (!bHasTrace || !bHasCombo || !bHasWarp)
			{
				UE_LOG(LogKnightCombatAuthoring, Error,
					TEXT("Owned montage section %s lacks its native windows: trace=%d combo=%d warp=%d, %s."),
					*Section.ToString(), bHasTrace, bHasCombo, bHasWarp, *Montage->GetPathName());
				return false;
			}
		}
		return true;
	}

	UAnimMontage* CreateMontage(const TCHAR* AssetName, const TArray<UAnimSequence*>& Sequences,
		const TArray<FName>& Sections, const bool bAttack = false, const bool bCombo = false,
		const bool bTraversal = false, const float MinimumSegmentDuration = 0.0f)
	{
		bool bCreated = false;
		UAnimMontage* Montage = FindOrCreateAsset<UAnimMontage>(AnimationRoot + AssetName, bCreated);
		if (Montage == nullptr || Sequences.IsEmpty() || Sequences.Num() != Sections.Num())
		{
			return nullptr;
		}
		for (UAnimSequence* Sequence : Sequences)
		{
			if (Sequence == nullptr || Sequence->GetSkeleton() == nullptr || Sequence->GetPlayLength() <= 0.0f)
			{
				return nullptr;
			}
		}
		if (bCreated)
		{
			Montage->SetSkeleton(Sequences[0]->GetSkeleton());
			Montage->BlendIn.SetBlendTime(0.08f);
			Montage->BlendOut.SetBlendTime(0.1f);
			Montage->SlotAnimTracks.Reset();
			FSlotAnimationTrack& Slot = Montage->AddSlot(TEXT("DefaultSlot"));
			float StartTime = 0.0f;
			for (int32 Index = 0; Index < Sequences.Num(); ++Index)
			{
				UAnimSequence* Sequence = Sequences[Index];
				FAnimSegment& Segment = Slot.AnimTrack.AnimSegments.AddDefaulted_GetRef();
				Segment.SetAnimReference(Sequence, true);
				Segment.StartPos = StartTime;
				Segment.AnimStartTime = 0.0f;
				Segment.AnimEndTime = Sequence->GetPlayLength();
				Segment.AnimPlayRate = Sequence->GetPlayLength()
					/ FMath::Max(Sequence->GetPlayLength(), MinimumSegmentDuration);
				Segment.LoopingCount = 1;
				StartTime += Segment.GetLength();
			}
			Montage->SetCompositeLength(StartTime);
			StartTime = 0.0f;
			for (int32 Index = 0; Index < Sequences.Num(); ++Index)
			{
				const float Duration = Slot.AnimTrack.AnimSegments[Index].GetLength();
				Montage->AddAnimCompositeSection(Sections[Index], StartTime);
				if (bAttack)
				{
					UKnightAnimNotifyState_TraceWindow* Trace = NewObject<UKnightAnimNotifyState_TraceWindow>(Montage);
					Trace->WindowId = Sections[Index];
					AddNotifyWindow(Montage, Trace, TEXT("KnightTrace"), StartTime + Duration * 0.22f,
						Duration * 0.42f, 0);
				}
				if (bCombo)
				{
					AddNotifyWindow(Montage, NewObject<UKnightAnimNotifyState_ComboWindow>(Montage),
						TEXT("KnightCombo"), StartTime + Duration * 0.5f, Duration * 0.4f, 1);
				}
				StartTime += Duration;
			}
			for (FCompositeSection& Section : Montage->CompositeSections)
			{
				Section.NextSectionName = NAME_None;
			}
			if (bTraversal)
			{
				UAnimNotifyState_MotionWarping* Notify = NewObject<UAnimNotifyState_MotionWarping>(Montage);
				URootMotionModifier_SkewWarp* Warp = NewObject<URootMotionModifier_SkewWarp>(Notify);
				Warp->WarpTargetName = TEXT("KnightTraversal");
				Warp->bWarpTranslation = true;
				Warp->bWarpToFeetLocation = false;
				Warp->bIgnoreZAxis = false;
				Warp->bWarpRotation = true;
				Notify->RootMotionModifier = Warp;
				AddNotifyWindow(Montage, Notify, TEXT("KnightTraversalWarp"), 0.0f, StartTime - 0.01f, 0);
			}
		}
		if (!RepairAndValidateNotifyTracks(Montage))
		{
			return nullptr;
		}
		if (bCreated)
		{
			Montage->PostEditChange();
		}
		if (Montage->GetSkeleton() != Sequences[0]->GetSkeleton() || Montage->GetPlayLength() <= 0.0f
			|| Montage->SlotAnimTracks.Num() != 1 || Montage->SlotAnimTracks[0].SlotName != FName(TEXT("DefaultSlot"))
			|| Montage->SlotAnimTracks[0].AnimTrack.AnimSegments.IsEmpty())
		{
			return nullptr;
		}
		for (const FAnimSegment& Segment : Montage->SlotAnimTracks[0].AnimTrack.AnimSegments)
		{
			if (!FMath::IsFinite(Segment.GetLength()) || Segment.GetLength() <= 0.0f
				|| Segment.GetLength() + UE_KINDA_SMALL_NUMBER < MinimumSegmentDuration)
			{
				UE_LOG(LogKnightCombatAuthoring, Error, TEXT("Owned montage has an invalid segment duration: %s."),
					*Montage->GetPathName());
				return nullptr;
			}
		}
		if (!ValidateMontageWindows(Montage, Sections, bAttack, bCombo, bTraversal))
		{
			return nullptr;
		}
		return SaveAsset(Montage) ? Montage : nullptr;
	}

	bool CreateCombatAbilitySet()
	{
		bool bCreated = false;
		UKnightAbilitySetDefinition* Set = FindOrCreateAsset<UKnightAbilitySetDefinition>(
			TEXT("/Game/Knight/Definitions/DA_Knight_AbilitySet_Combat"), bCreated);
		if (Set == nullptr)
		{
			return false;
		}
		if (bCreated)
		{
			if (!Set->TrySetIdentity(TEXT("FoundationCombat"), TEXT("KnightAbilitySet")))
			{
				return false;
			}
			const TArray<FKnightAbilitySetEntry> Abilities =
			{
				{ UKnightLightAttackAbility::StaticClass(), KnightGameplayTags::Input_Combat_Primary, 1 },
				{ UKnightHeavyAttackAbility::StaticClass(), KnightGameplayTags::Input_Combat_Heavy, 1 },
				{ UKnightBlockAbility::StaticClass(), KnightGameplayTags::Input_Combat_Block, 1 },
				{ UKnightParryAbility::StaticClass(), KnightGameplayTags::Input_Combat_Parry, 1 },
				{ UKnightDodgeAbility::StaticClass(), KnightGameplayTags::Input_Combat_Dodge, 1 },
				{ UKnightLockTargetAbility::StaticClass(), KnightGameplayTags::Input_Combat_LockOn, 1 },
				{ UKnightRuneAbility::StaticClass(), KnightGameplayTags::Input_Combat_Rune, 1 },
				{ UKnightFinisherAbility::StaticClass(), KnightGameplayTags::Input_Combat_Finisher, 1 },
				{ UKnightTraversalAbility::StaticClass(), KnightGameplayTags::Input_World_Traverse, 1 }
			};
			TArray<FKnightTagRelationship> Relationships;
			const TArray<FGameplayTag> ActionTags =
			{
				KnightGameplayTags::Ability_Combat_Primary, KnightGameplayTags::Ability_Combat_Heavy,
				KnightGameplayTags::Ability_Combat_Block, KnightGameplayTags::Ability_Combat_Parry,
				KnightGameplayTags::Ability_Combat_Dodge, KnightGameplayTags::Ability_Combat_Rune,
				KnightGameplayTags::Ability_Combat_Finisher, KnightGameplayTags::Ability_World_Traverse
			};
			for (const FGameplayTag Action : ActionTags)
			{
				FKnightTagRelationship& Relationship = Relationships.AddDefaulted_GetRef();
				Relationship.AbilityTag = Action;
				Relationship.ActivationRequiredTags.AddTag(KnightGameplayTags::State_Life_Alive);
				Relationship.ActivationBlockedTags.AddTag(KnightGameplayTags::State_Life_Downed);
				Relationship.ActivationBlockedTags.AddTag(KnightGameplayTags::State_Life_Dead);
				Relationship.ActivationBlockedTags.AddTag(KnightGameplayTags::State_Combat_Staggered);
				Relationship.ActivationBlockedTags.AddTag(KnightGameplayTags::State_Combat_Finishing);
				Relationship.ActivationBlockedTags.AddTag(KnightGameplayTags::State_Combat_Traversing);
				if (Action == KnightGameplayTags::Ability_Combat_Dodge)
				{
					Relationship.AbilityTagsToCancel.AddTag(KnightGameplayTags::Ability_Combat_Primary);
					Relationship.AbilityTagsToCancel.AddTag(KnightGameplayTags::Ability_Combat_Heavy);
					Relationship.AbilityTagsToCancel.AddTag(KnightGameplayTags::Ability_Combat_Block);
					Relationship.AbilityTagsToCancel.AddTag(KnightGameplayTags::Ability_Combat_Parry);
					Relationship.ActivationBlockedTags.AddTag(KnightGameplayTags::State_Combat_Dodging);
				}
				else
				{
					Relationship.ActivationBlockedTags.AddTag(KnightGameplayTags::State_Combat_Attacking);
					Relationship.ActivationBlockedTags.AddTag(KnightGameplayTags::State_Combat_Blocking);
					Relationship.ActivationBlockedTags.AddTag(KnightGameplayTags::State_Combat_Parrying);
					Relationship.ActivationBlockedTags.AddTag(KnightGameplayTags::State_Combat_Dodging);
				}
			}
			if (!Set->TrySetAbilities(Abilities).IsSuccess() || !Set->TrySetRelationships(Relationships).IsSuccess())
			{
				return false;
			}
			Set->SetDisplayName(NSLOCTEXT("KnightCombatAuthoring", "CombatSet", "Foundation combat abilities"));
		}
		return Set->GetPrimaryAssetId() == FPrimaryAssetId(TEXT("KnightAbilitySet"), TEXT("FoundationCombat"))
			&& Set->ValidateAbilitySet().IsSuccess() && SaveAsset(Set);
	}

	bool CreateCombatAssets()
	{
		if (!ImportMannequinAssets() || !CreateCombatAbilitySet())
		{
			return false;
		}
		UAnimSequence* AttackA = LoadSequence(TEXT("Anims/Unarmed/Attack/MM_Attack_01"));
		UAnimSequence* AttackB = LoadSequence(TEXT("Anims/Unarmed/Attack/MM_Attack_02"));
		UAnimSequence* AttackC = LoadSequence(TEXT("Anims/Unarmed/Attack/MM_Attack_03"));
		UAnimSequence* HeavySequence = LoadSequence(TEXT("Anims/Unarmed/Attack/MM_ChargedAttack"));
		UAnimSequence* TraversalSequence = CreateTraversalSequence();
		UAnimMontage* Light = CreateMontage(TEXT("AM_Knight_Light"), { AttackA, AttackB, AttackC },
			{ TEXT("LightA"), TEXT("LightB"), TEXT("LightC") }, true, true);
		UAnimMontage* Heavy = CreateMontage(TEXT("AM_Knight_Heavy"), { HeavySequence }, { TEXT("Heavy") }, true);
		UAnimMontage* Dodge = CreateMontage(TEXT("AM_Knight_Dodge"),
			{ LoadSequence(TEXT("Anims/Unarmed/Jump/MM_Dash")) }, { TEXT("Dodge") });
		UAnimMontage* Rune = CreateMontage(TEXT("AM_Knight_Rune"), { AttackC }, { TEXT("Rune") });
		UAnimMontage* Stagger = CreateMontage(TEXT("AM_Knight_Stagger"),
			{ LoadSequence(TEXT("Anims/Rifle/HitReact/MM_HitReact_Front_Hvy_01")) }, { TEXT("Stagger") });
		UAnimMontage* Traversal = CreateMontage(TEXT("AM_Knight_Traversal"),
			{ TraversalSequence }, { TEXT("Traversal") }, false, false, true);
		UAnimMontage* Finisher = CreateMontage(TEXT("AM_Knight_Finisher"), { HeavySequence },
			{ TEXT("Finisher") }, false, false, false, 3.0f);
		UAnimMontage* Victim = CreateMontage(TEXT("AM_Knight_FinisherVictim"),
			{ LoadSequence(TEXT("Anims/Death/MM_Death_Front_01")) }, { TEXT("Victim") }, false, false, false, 3.0f);
		USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr,
			TEXT("/Game/Knight/Vendor/UE58/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"));
		if (Light == nullptr || Heavy == nullptr || Dodge == nullptr || Rune == nullptr || Stagger == nullptr
			|| Traversal == nullptr || Finisher == nullptr || Victim == nullptr || Mesh == nullptr)
		{
			UE_LOG(LogKnightCombatAuthoring, Error, TEXT("Combat montage authoring failed; no profile will be saved."));
			return false;
		}
		bool bCreated = false;
		UKnightCombatProfileDefinition* Profile = FindOrCreateAsset<UKnightCombatProfileDefinition>(
			TEXT("/Game/Knight/Definitions/DA_Knight_CombatProfile_Foundation"), bCreated);
		if (Profile == nullptr)
		{
			return false;
		}
		if (bCreated)
		{
			if (!Profile->TrySetIdentity(TEXT("FoundationCombat"), TEXT("KnightCombatProfile"))
				|| !Profile->TrySetCombatAbilitySetId(FPrimaryAssetId(TEXT("KnightAbilitySet"), TEXT("FoundationCombat"))).IsSuccess()
				|| !Profile->TrySetPresentation(Mesh, LoadSequence(TEXT("Anims/Unarmed/MM_Idle")),
					LoadSequence(TEXT("Anims/Unarmed/Walk/MF_Unarmed_Walk_Fwd"))).IsSuccess())
			{
				return false;
			}
			TArray<FKnightAttackDefinition> Attacks;
			for (const FName Section : { FName(TEXT("LightA")), FName(TEXT("LightB")), FName(TEXT("LightC")) })
			{
				FKnightAttackDefinition& Attack = Attacks.AddDefaulted_GetRef();
				Attack.ActionTag = KnightGameplayTags::Ability_Combat_Primary;
				Attack.Montage = Light;
				Attack.SectionName = Section;
			}
			FKnightAttackDefinition& HeavyAttack = Attacks.AddDefaulted_GetRef();
			HeavyAttack.ActionTag = KnightGameplayTags::Ability_Combat_Heavy;
			HeavyAttack.Montage = Heavy;
			HeavyAttack.SectionName = TEXT("Heavy");
			HeavyAttack.Damage = 30.0f;
			HeavyAttack.PoiseDamage = 50.0f;
			HeavyAttack.StaminaCost = 15.0f;
			FKnightCombatTuning Tuning;
			Tuning.StaggerDuration = 5.0f;
			if (!Profile->TrySetAttacks(Attacks).IsSuccess()
				|| !Profile->TrySetActionMontages(Dodge, Rune, Stagger, Traversal, Finisher, Victim).IsSuccess()
				|| !Profile->TrySetTuning(Tuning).IsSuccess())
			{
				return false;
			}
			Profile->SetDisplayName(NSLOCTEXT("KnightCombatAuthoring", "FoundationProfile", "Foundation hammer combat"));
		}
		const FKnightResult Validation = Profile->ValidateCombatProfile();
		if (!Validation.IsSuccess())
		{
			UE_LOG(LogKnightCombatAuthoring, Error, TEXT("Combat profile validation failed: %s."),
				*Validation.Message.ToString());
			return false;
		}
		return Profile->GetPrimaryAssetId() == FPrimaryAssetId(TEXT("KnightCombatProfile"), TEXT("FoundationCombat"))
			&& SaveAsset(Profile);
	}
}
