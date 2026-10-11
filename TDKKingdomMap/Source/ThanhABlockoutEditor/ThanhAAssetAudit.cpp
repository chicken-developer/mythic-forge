#include "ThanhAAssetAudit.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "EngineUtils.h"
#include "Editor.h"
#include "FileHelpers.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/Material.h"
#include "Materials/MaterialFunction.h"
#include "Materials/MaterialExpression.h"
#include "PhysicsEngine/BodySetup.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "AssetCompilingManager.h"

namespace
{
TArray<TSharedPtr<FJsonValue>> XYZ(FVector V) { return {MakeShared<FJsonValueNumber>(V.X),MakeShared<FJsonValueNumber>(V.Y),MakeShared<FJsonValueNumber>(V.Z)}; }
TArray<TSharedPtr<FJsonValue>> Graph(TConstArrayView<TObjectPtr<UMaterialExpression>> Expressions)
{
    TArray<TSharedPtr<FJsonValue>> Result;
    for (UMaterialExpression* E:Expressions)
    {
        auto J=MakeShared<FJsonObject>(); J->SetStringField(TEXT("class"),E->GetClass()->GetName()); J->SetStringField(TEXT("node"),E->GetName());
        for (TFieldIterator<FProperty> P(E->GetClass());P;++P)
        {
            FString Name=P->GetName();
            if (Name==TEXT("Code") || Name==TEXT("ParameterName") || Name==TEXT("DefaultValue") || Name==TEXT("MaterialFunction") || Name==TEXT("UTiling") || Name==TEXT("VTiling") || Name.StartsWith(TEXT("Const")))
            { FString Value; P->ExportTextItem_Direct(Value,P->ContainerPtrToValuePtr<void>(E),nullptr,E,PPF_None); J->SetStringField(Name,Value); }
        }
        Result.Add(MakeShared<FJsonValueObject>(J));
    }
    return Result;
}
}
bool ThanhAAuditAssets(const FString& Dir)
{
    auto Report=MakeShared<FJsonObject>(); Report->SetStringField(TEXT("execution"),TEXT("Actual Unreal Asset Registry, loaded assets and saved level; read-only"));
    auto& Registry=FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get(); Registry.SearchAllAssets(true);
    TArray<FAssetData> Assets; Registry.GetAssetsByPath(FName(TEXT("/Game/LevelPrototyping")),Assets,true);
    Assets.Sort([](const FAssetData& A,const FAssetData& B){return A.GetObjectPathString()<B.GetObjectPathString();});
    TArray<TSharedPtr<FJsonValue>> Meshes,Materials,Functions;
    for (const FAssetData& A:Assets)
    {
        UObject* Object=A.GetAsset(); if (!Object) continue;
        auto J=MakeShared<FJsonObject>(); J->SetStringField(TEXT("object_path"),A.GetObjectPathString());
        if (auto* M=Cast<UStaticMesh>(Object))
        {
            FAssetCompilingManager::Get().FinishAllCompilation(); const FBox B=M->GetBoundingBox();
            J->SetArrayField(TEXT("bounds_min_cm"),XYZ(B.Min)); J->SetArrayField(TEXT("bounds_max_cm"),XYZ(B.Max));
            J->SetArrayField(TEXT("bounds_center_cm"),XYZ(B.GetCenter())); J->SetArrayField(TEXT("bounds_size_cm"),XYZ(B.GetSize())); J->SetArrayField(TEXT("mesh_pivot_cm"),XYZ(FVector::ZeroVector));
            TArray<TSharedPtr<FJsonValue>> Slots;
            for (const auto& S:M->GetStaticMaterials()) { auto Slot=MakeShared<FJsonObject>(); Slot->SetStringField(TEXT("slot"),S.MaterialSlotName.ToString()); Slot->SetStringField(TEXT("material"),GetPathNameSafe(S.MaterialInterface)); Slots.Add(MakeShared<FJsonValueObject>(Slot)); }
            J->SetArrayField(TEXT("material_slots"),Slots);
            if (UBodySetup* Bdy=M->GetBodySetup()) { J->SetNumberField(TEXT("simple_collision_shapes"),Bdy->AggGeom.GetElementCount()); J->SetStringField(TEXT("collision_trace_flag"),StaticEnum<ECollisionTraceFlag>()->GetNameStringByValue(Bdy->CollisionTraceFlag)); }
            if (const FMeshDescription* D=M->GetMeshDescription(0)) J->SetNumberField(TEXT("lod0_triangles"),D->Triangles().Num());
            Meshes.Add(MakeShared<FJsonValueObject>(J));
        }
        else if (auto* Mat=Cast<UMaterialInterface>(Object))
        {
            J->SetStringField(TEXT("base_material"),GetPathNameSafe(Mat->GetMaterial()));
            TArray<FMaterialParameterInfo> Params; TArray<FGuid> GUIDs; Mat->GetAllScalarParameterInfo(Params,GUIDs);
            auto Values=MakeShared<FJsonObject>(); for (const auto& P:Params) { float Value=0; Mat->GetScalarParameterValue(P,Value); Values->SetNumberField(P.Name.ToString(),Value); } J->SetObjectField(TEXT("scalar_parameters"),Values);
            Mat->GetAllVectorParameterInfo(Params,GUIDs); Values=MakeShared<FJsonObject>(); for (const auto& P:Params) { FLinearColor Value; Mat->GetVectorParameterValue(P,Value); Values->SetStringField(P.Name.ToString(),Value.ToString()); } J->SetObjectField(TEXT("vector_parameters"),Values);
            if (auto* Base=Cast<UMaterial>(Object)) J->SetArrayField(TEXT("graph"),Graph(Base->GetExpressions()));
            Materials.Add(MakeShared<FJsonValueObject>(J));
        }
        else if (auto* F=Cast<UMaterialFunction>(Object)) { J->SetArrayField(TEXT("graph"),Graph(F->GetExpressions())); Functions.Add(MakeShared<FJsonValueObject>(J)); }
    }
    Report->SetArrayField(TEXT("meshes"),Meshes); Report->SetArrayField(TEXT("materials"),Materials); Report->SetArrayField(TEXT("material_functions"),Functions);
    const FString Map=FPackageName::LongPackageNameToFilename(TEXT("/Game/Maps/Blockout/L_ThanhA_Blockout"),FPackageName::GetMapPackageExtension());
    TArray<TSharedPtr<FJsonValue>> Actors; int32 Manual=0,Owned=0; TSet<FName> IDs; bool Duplicates=false;
    if (FEditorFileUtils::LoadMap(Map,false,false))
    {
        auto* W=GEditor->GetEditorWorldContext().World();
        for (TActorIterator<AActor> It(W);It;++It)
        {
            if (It->ActorHasTag(TEXT("ThanhA.Blockout.v1"))) { Owned++; for (FName Tag:It->Tags) if (Tag.ToString().StartsWith(TEXT("ThanhA.Id."))) { Duplicates|=IDs.Contains(Tag); IDs.Add(Tag); } } else Manual++;
            if (auto* A=Cast<AStaticMeshActor>(*It))
            {
                auto* C=A->GetStaticMeshComponent(); auto J=MakeShared<FJsonObject>(); J->SetStringField(TEXT("actor_label"),A->GetActorLabel()); J->SetBoolField(TEXT("generator_owned"),A->ActorHasTag(TEXT("ThanhA.Blockout.v1"))); J->SetStringField(TEXT("mesh"),GetPathNameSafe(C->GetStaticMesh())); J->SetStringField(TEXT("actor_transform"),A->GetActorTransform().ToString()); J->SetStringField(TEXT("component_relative_transform"),C->GetRelativeTransform().ToString());
                TArray<TSharedPtr<FJsonValue>> Overrides; for (UMaterialInterface* M:C->OverrideMaterials) Overrides.Add(MakeShared<FJsonValueString>(GetPathNameSafe(M))); J->SetArrayField(TEXT("material_overrides"),Overrides); Actors.Add(MakeShared<FJsonValueObject>(J));
            }
        }
    }
    Report->SetNumberField(TEXT("owned_actors"),Owned); Report->SetNumberField(TEXT("manual_actors"),Manual); Report->SetBoolField(TEXT("duplicate_stable_ids"),Duplicates); Report->SetArrayField(TEXT("existing_static_mesh_actors"),Actors);
    FString Text; FJsonSerializer::Serialize(Report,TJsonWriterFactory<>::Create(&Text));
    return Meshes.Num()>0 && FFileHelper::SaveStringToFile(Text,*(Dir/TEXT("prototyping_asset_audit.json")));
}
