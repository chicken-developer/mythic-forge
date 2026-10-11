#include "ThanhAEnvironment.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetCompilingManager.h"
#include "Editor.h"
#include "FileHelpers.h"
#include "EngineUtils.h"
#include "Engine/StaticMesh.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialFunction.h"
#include "Landscape.h"
#include "LandscapeInfo.h"
#include "LandscapeComponent.h"
#include "LandscapeLayerInfoObject.h"
#include "PhysicsEngine/BodySetup.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "ThanhADimensionComponent.h"
#include "LandscapeEdit.h"
#include "LandscapeGrassType.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialExpressionLandscapeGrassOutput.h"
#include "Materials/MaterialExpressionLandscapeLayerBlend.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialExpressionWorldPosition.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Engine/PostProcessVolume.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "UObject/SavePackage.h"
#include "Misc/SecureHash.h"
#include "StaticMeshAttributes.h"
#include "ShaderCompiler.h"
namespace ThanhA { UStaticMesh* Mesh(const FString&,const FString&,const TArray<FVector>&,const TArray<FIntVector>&,UStaticMesh*); }

namespace Forest
{
using J = TSharedPtr<FJsonObject>;
using V = TSharedPtr<FJsonValue>;
J Obj() { return MakeShared<FJsonObject>(); }
V Val(J O) { return MakeShared<FJsonValueObject>(O); }
TArray<V> XYZ(FVector P) { return {MakeShared<FJsonValueNumber>(P.X),MakeShared<FJsonValueNumber>(P.Y),MakeShared<FJsonValueNumber>(P.Z)}; }
FString Dir() { return FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/TEXT("Design/Environment/Generated")); }
bool Write(const FString& Name,J O) { FString S; FJsonSerializer::Serialize(O.ToSharedRef(),TJsonWriterFactory<>::Create(&S)); return FFileHelper::SaveStringToFile(S,*(Dir()/Name)); }
J Props(UObject* O)
{
    auto R=Obj();
    for (TFieldIterator<FProperty> P(O->GetClass());P;++P)
    {
        // Only inspect editor-visible data, never executable source outside the material graph.
        if (!(P->PropertyFlags & CPF_Edit)) continue;
        FString S; P->ExportTextItem_Direct(S,P->ContainerPtrToValuePtr<void>(O),nullptr,O,PPF_None);
        if (S.Len()<16000) R->SetStringField(P->GetName(),S);
    }
    return R;
}
TArray<V> Graph(TConstArrayView<TObjectPtr<UMaterialExpression>> Expressions)
{
    TArray<V> R; for (UMaterialExpression* E:Expressions) { auto O=Props(E); O->SetStringField(TEXT("class"),E->GetClass()->GetName()); O->SetStringField(TEXT("node"),E->GetName()); R.Add(Val(O)); } return R;
}
J Material(UMaterialInterface* M)
{
    auto O=Obj(); O->SetStringField(TEXT("path"),M->GetPathName()); O->SetStringField(TEXT("base"),GetPathNameSafe(M->GetMaterial()));
    O->SetObjectField(TEXT("properties"),Props(M));
    TArray<FMaterialParameterInfo> P; TArray<FGuid> G; auto S=Obj(); M->GetAllScalarParameterInfo(P,G);
    for (auto& I:P) { float X; if (M->GetScalarParameterValue(I,X)) S->SetNumberField(I.Name.ToString(),X); } O->SetObjectField(TEXT("scalars"),S);
    S=Obj(); M->GetAllVectorParameterInfo(P,G); for (auto& I:P) { FLinearColor X; if (M->GetVectorParameterValue(I,X)) S->SetStringField(I.Name.ToString(),X.ToString()); } O->SetObjectField(TEXT("vectors"),S);
    S=Obj(); M->GetAllTextureParameterInfo(P,G); for (auto& I:P) { UTexture* X=nullptr; if (M->GetTextureParameterValue(I,X)) S->SetStringField(I.Name.ToString(),GetPathNameSafe(X)); } O->SetObjectField(TEXT("textures"),S);
    if (auto* B=Cast<UMaterial>(M)) O->SetArrayField(TEXT("graph"),Graph(B->GetExpressions()));
    return O;
}
J World(const FString& Path)
{
    auto O=Obj(); O->SetStringField(TEXT("map"),Path);
    if (!FEditorFileUtils::LoadMap(FPackageName::LongPackageNameToFilename(Path,FPackageName::GetMapPackageExtension()),false,false)) return O;
    UWorld* W=GEditor->GetEditorWorldContext().World(); TArray<V> Actors,Landscapes;
    for (TActorIterator<AActor> It(W);It;++It)
    {
        auto A=Obj(); A->SetStringField(TEXT("label"),It->GetActorLabel()); A->SetStringField(TEXT("class"),It->GetClass()->GetName()); A->SetStringField(TEXT("transform"),It->GetActorTransform().ToString());
        TArray<V> Cs; TInlineComponentArray<UActorComponent*> Components(*It);
        for (auto* C:Components)
        {
            auto X=Obj(); X->SetStringField(TEXT("class"),C->GetClass()->GetName());
            if (C->IsA<UPrimitiveComponent>() || C->GetClass()->GetName().Contains(TEXT("Light")) || C->GetClass()->GetName().Contains(TEXT("PostProcess")) || C->GetClass()->GetName().Contains(TEXT("Fog"))) X->SetObjectField(TEXT("properties"),Props(C));
            if (auto* S=Cast<UStaticMeshComponent>(C)) { X->SetStringField(TEXT("mesh"),GetPathNameSafe(S->GetStaticMesh())); TArray<V> Mats; for (int32 I=0;I<S->GetNumMaterials();I++) Mats.Add(MakeShared<FJsonValueString>(GetPathNameSafe(S->GetMaterial(I)))); X->SetArrayField(TEXT("materials"),Mats); }
            if (auto* S=Cast<UInstancedStaticMeshComponent>(C)) { X->SetNumberField(TEXT("instances"),S->GetInstanceCount()); if (S->GetInstanceCount()) { FTransform T; S->GetInstanceTransform(0,T,true); X->SetStringField(TEXT("first_instance"),T.ToString()); } }
            Cs.Add(Val(X));
        }
        A->SetArrayField(TEXT("components"),Cs); Actors.Add(Val(A));
        if (auto* L=Cast<ALandscapeProxy>(*It))
        {
            auto X=Obj(); X->SetStringField(TEXT("label"),L->GetActorLabel()); X->SetStringField(TEXT("transform"),L->GetActorTransform().ToString());
            X->SetStringField(TEXT("material"),GetPathNameSafe(L->LandscapeMaterial)); X->SetObjectField(TEXT("properties"),Props(L));
            TArray<V> Names; for (FName N:L->RetrieveTargetLayerNamesFromMaterials(false)) Names.Add(MakeShared<FJsonValueString>(N.ToString())); X->SetArrayField(TEXT("material_layers"),Names);
            TArray<V> Layers; if (auto* Info=L->GetLandscapeInfo()) for (auto& S:Info->Layers) { auto Q=Obj(); Q->SetStringField(TEXT("name"),S.GetLayerName().ToString()); Q->SetStringField(TEXT("info"),GetPathNameSafe(S.LayerInfoObj)); if (S.LayerInfoObj) Q->SetObjectField(TEXT("properties"),Props(S.LayerInfoObj)); Layers.Add(Val(Q)); } X->SetArrayField(TEXT("layers"),Layers); Landscapes.Add(Val(X));
        }
    }
    O->SetArrayField(TEXT("actors"),Actors); O->SetArrayField(TEXT("landscapes"),Landscapes); return O;
}

J Manifest(UWorld* W)
{
    auto R=Obj(); TArray<V> Actors;
    TArray<AActor*> Sorted; for (TActorIterator<AActor> It(W);It;++It) if (!It->ActorHasTag(TEXT("ThanhA.Environment.v1"))) Sorted.Add(*It);
    Sorted.Sort([](const AActor& A,const AActor& B){return A.GetPathName()<B.GetPathName();});
    for (auto* A:Sorted)
    {
        auto O=Obj(); O->SetStringField(TEXT("name"),A->GetName()); O->SetStringField(TEXT("guid"),A->GetActorGuid().ToString()); O->SetStringField(TEXT("label"),A->GetActorLabel()); O->SetStringField(TEXT("class"),A->GetClass()->GetName());
        O->SetStringField(TEXT("transform"),A->GetActorTransform().ToString()); O->SetStringField(TEXT("folder"),A->GetFolderPath().ToString());
        TArray<V> Tags; for (auto T:A->Tags) Tags.Add(MakeShared<FJsonValueString>(T.ToString())); O->SetArrayField(TEXT("tags"),Tags);
        FBox B=A->GetComponentsBoundingBox(true); if (B.IsValid) { O->SetArrayField(TEXT("bounds_min_cm"),XYZ(B.Min)); O->SetArrayField(TEXT("bounds_max_cm"),XYZ(B.Max)); }
        TArray<V> Cs; TInlineComponentArray<UActorComponent*> Components(A);
        for (auto* C:Components)
        {
            auto X=Obj(); X->SetStringField(TEXT("name"),C->GetName()); X->SetStringField(TEXT("class"),C->GetClass()->GetName());
            if (auto* S=Cast<UStaticMeshComponent>(C))
            {
                X->SetStringField(TEXT("mesh"),GetPathNameSafe(S->GetStaticMesh())); X->SetStringField(TEXT("relative_transform"),S->GetRelativeTransform().ToString()); X->SetNumberField(TEXT("collision_enabled"),int32(S->GetCollisionEnabled())); X->SetStringField(TEXT("collision_profile"),S->GetCollisionProfileName().ToString());
                FString Responses; FCollisionResponseContainer::StaticStruct()->ExportText(Responses,&S->GetCollisionResponseToChannels(),nullptr,S,PPF_None,nullptr); X->SetStringField(TEXT("collision_responses"),Responses);
                TArray<V> Mats; for (UMaterialInterface* M:S->OverrideMaterials) Mats.Add(MakeShared<FJsonValueString>(GetPathNameSafe(M))); X->SetArrayField(TEXT("material_overrides"),Mats);
            }
            if (auto* D=Cast<UThanhADimensionComponent>(C))
            {
                auto Data=Obj(); for (TFieldIterator<FProperty> P(UThanhADimensionComponent::StaticClass(),EFieldIteratorFlags::ExcludeSuper);P;++P) { FString S; P->ExportTextItem_Direct(S,P->ContainerPtrToValuePtr<void>(D),nullptr,D,PPF_None); Data->SetStringField(P->GetName(),S); } X->SetObjectField(TEXT("dimensions"),Data);
            }
            Cs.Add(Val(X));
        }
        O->SetArrayField(TEXT("components"),Cs); Actors.Add(Val(O));
    }
    R->SetArrayField(TEXT("actors"),Actors); return R;
}
J Read(const FString& File)
{
    FString S; J O; if (FFileHelper::LoadFileToString(S,*File)) FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(S),O); return O;
}
FVector Vector(const TArray<V>& A) { return FVector(A[0]->AsNumber(),A[1]->AsNumber(),A.Num()>2?A[2]->AsNumber():0); }
bool Save(UObject* O)
{
    auto* P=O->GetOutermost(); P->MarkPackageDirty(); FSavePackageArgs A; A.TopLevelFlags=RF_Public|RF_Standalone; A.SaveFlags=SAVE_NoError;
    return UPackage::SavePackage(P,O,*FPackageName::LongPackageNameToFilename(P->GetName(),FPackageName::GetAssetPackageExtension()),A);
}
template<class T> T* Duplicate(T* Source,const FString& Root,const FString& Name)
{
    const FString P=Root/TEXT("")+Name;
    if (auto* Existing=LoadObject<T>(nullptr,*(P+TEXT(".")+Name),nullptr,LOAD_NoWarn)) return Existing;
    auto* Result=Cast<T>(StaticDuplicateObject(Source,CreatePackage(*P),*Name)); if (Result) FAssetRegistryModule::AssetCreated(Result); return Result;
}
TArray<uint16> Heights(ALandscape* L,int32 N)
{
    TArray<uint16> H; H.SetNumZeroed(N*N); FLandscapeEditDataInterface E(L->GetLandscapeInfo()); E.GetHeightDataFast(0,0,N-1,N-1,H.GetData(),N); return H;
}
FString Hash(const TArray<uint16>& H) { return FMD5::HashBytes(reinterpret_cast<const uint8*>(H.GetData()),H.Num()*2); }
void Own(AActor* A,const FString& Id,const FString& Folder)
{
    A->Tags.Add(TEXT("ThanhA.Environment.v1")); A->Tags.Add(FName(*(TEXT("ThanhA.Environment.Id.")+Id))); A->SetActorLabel(Id); A->SetFolderPath(FName(*Folder));
}
J CaptureScene(const TArray<V>& Views,float Exposure)
{
    auto S=Obj(); auto L=Obj(); L->SetNumberField(TEXT("capture_exposure_bias"),Exposure); S->SetObjectField(TEXT("lighting"),L); S->SetArrayField(TEXT("capture_views"),Views); return S;
}
TMap<FString,FString> ProtectedSignatures(J M,const TSet<FString>& Natural)
{
    TMap<FString,FString> R;
    for (auto& V:M->GetArrayField(TEXT("actors")))
    {
        auto O=V->AsObject(); bool Mesh=false; FString Id;
        for (auto& T:O->GetArrayField(TEXT("tags"))) if (T->AsString().StartsWith(TEXT("ThanhA.Id."))) Id=T->AsString().RightChop(10);
        for (auto& C:O->GetArrayField(TEXT("components"))) if (C->AsObject()->HasField(TEXT("mesh"))) Mesh=true;
        if (!Mesh || Natural.Contains(Id)) continue;
        FString S; FJsonSerializer::Serialize(O.ToSharedRef(),TJsonWriterFactory<>::Create(&S)); R.Add(O->GetStringField(TEXT("name")),S);
    }
    return R;
}
UStaticMesh* Shoulder(UStaticMeshComponent* C,ALandscape* L,const FString& Root,const FString& Id)
{
    const FMeshDescription* D=C->GetStaticMesh()->GetMeshDescription(0); if (!D) return nullptr;
    FStaticMeshConstAttributes Attributes(*D); const auto Positions=Attributes.GetVertexPositions();
    struct Edge { FVector A,B; int32 Count=0; };
    TMap<FString,Edge> Edges;
    for (FTriangleID T:D->Triangles().GetElementIDs())
    {
        TArray<FVector> P; for (FVertexInstanceID V:D->GetTriangleVertexInstances(T)) P.Add(C->GetComponentTransform().TransformPosition(FVector(Positions[D->GetVertexInstanceVertex(V)])));
        if (FVector::CrossProduct(P[1]-P[0],P[2]-P[0]).GetSafeNormal().Z>-.7) continue;
        for (int32 I=0;I<3;I++)
        {
            const FVector A=P[I],B=P[(I+1)%3]; const FString AS=A.ToString(),BS=B.ToString(); const FString Key=AS<BS?AS+TEXT("|")+BS:BS+TEXT("|")+AS;
            auto& E=Edges.FindOrAdd(Key); E.A=A; E.B=B; E.Count++;
        }
    }
    TMap<FString,FVector> Normals;
    for (auto& Pair:Edges) if (Pair.Value.Count==1)
    {
        auto& E=Pair.Value; const FVector Dir=(E.B-E.A).GetSafeNormal2D(); const FVector Out(-Dir.Y,Dir.X,0);
        Normals.FindOrAdd(E.A.ToString())+=Out; Normals.FindOrAdd(E.B.ToString())+=Out;
    }
    TArray<FVector> Vertices; TArray<FIntVector> Triangles;
    for (auto& Pair:Edges) if (Pair.Value.Count==1)
    {
        auto& E=Pair.Value; FVector A=E.A+Normals[E.A.ToString()].GetSafeNormal2D()*125,B=E.B+Normals[E.B.ToString()].GetSafeNormal2D()*125;
        auto ZA=L->GetHeightAtLocation(A),ZB=L->GetHeightAtLocation(B); if (!ZA.IsSet() || !ZB.IsSet()) continue;
        A.Z=ZA.GetValue()+2; B.Z=ZB.GetValue()+2;
        // An adjacent road supplies this junction. Do not put a shoulder through its walkable surface.
        FHitResult Hit; FCollisionQueryParams Q(SCENE_QUERY_STAT(Shoulder),true); Q.AddIgnoredComponent(C);
        const FVector Mid=(A+B)*.5;
        if (C->GetWorld()->LineTraceSingleByChannel(Hit,Mid+FVector(0,0,100),Mid-FVector(0,0,100),ECC_Visibility,Q) && Hit.Component.IsValid() && Hit.GetActor()->GetFolderPath().ToString()==TEXT("Roads")) continue;
        int32 I=Vertices.Num(); Vertices.Append({E.A/100,E.B/100,B/100,A/100}); Triangles.Add(FIntVector(I,I+1,I+2)); Triangles.Add(FIntVector(I,I+2,I+3));
    }
    return Vertices.Num()?ThanhA::Mesh(Root,TEXT("shoulder_")+Id,Vertices,Triangles,nullptr):nullptr;
}
}

bool ThanhAEnvironmentRun(const FString& Mode,TFunctionRef<bool(UWorld*,const TSharedPtr<FJsonObject>&,const FString&)> Capture)
{
    using namespace Forest;
    IFileManager::Get().MakeDirectory(*Dir(),true);
    const FString Target=TEXT("/Game/Maps/Blockout/L_ThanhA_Blockout"),Trial=TEXT("/Game/Maps/Environment/L_ThanhA_Environment_Trial");
    const bool IsTrial=Mode.Contains(TEXT("trial"));
    auto Config=Read(FPaths::ProjectDir()/TEXT("Design/Environment/environment.json"));
    if (Mode==TEXT("environmentsamplecapture"))
    {
        if (!Config || !FEditorFileUtils::LoadMap(FPackageName::LongPackageNameToFilename(Config->GetStringField(TEXT("sample_map")),FPackageName::GetMapPackageExtension()),false,false)) return false;
        auto* SampleWorld=GEditor->GetEditorWorldContext().World();
        TArray<FVector> Cameras;
        for (auto& View:Config->GetArrayField(TEXT("sample_views"))) Cameras.Add(Vector(View->AsObject()->GetArrayField(TEXT("location_m")))*100);
        for (TActorIterator<ALandscape> It(SampleWorld);It;++It) { It->BuildGrassMaps(); It->UpdateGrass(Cameras,true); }
        return Capture(SampleWorld,CaptureScene(Config->GetArrayField(TEXT("sample_views")),Config->GetNumberField(TEXT("sample_exposure_bias"))),Dir());
    }
    const FString Path=IsTrial && Mode!=TEXT("environmenttrial")?Trial:Target;
    if (!FEditorFileUtils::LoadMap(FPackageName::LongPackageNameToFilename(Path,FPackageName::GetMapPackageExtension()),false,false)) return false;
    auto* W=GEditor->GetEditorWorldContext().World();
    if (Mode==TEXT("environmentsnapshot")) return Write(TEXT("baseline_manifest.json"),Manifest(W));
    if (!Config) return false;
    ALandscape* L=nullptr; for (TActorIterator<ALandscape> It(W);It;++It) if (It->ActorHasTag(TEXT("ThanhA.Id.designed_landscape"))) L=*It;
    if (!L) return false;
    const int32 N=1009;
    if (Mode.Contains(TEXT("capture")))
    {
        // Grass maps require an actual renderer. Bake the package's Grass Output before captures.
        FAssetCompilingManager::Get().FinishAllCompilation(); if (GShaderCompilingManager) GShaderCompilingManager->FinishAllCompilation();
        L->BuildGrassMaps();
        const auto& Views=Config->GetArrayField(IsTrial?TEXT("trial_views"):TEXT("capture_views"));
        TArray<FVector> Cameras; for (auto& Vw:Views) Cameras.Add(Vector(Vw->AsObject()->GetArrayField(TEXT("location_m")))*100);
        L->UpdateGrass(Cameras,true); W->SendAllEndOfFrameUpdates();
        auto R=Obj(); R->SetStringField(TEXT("execution"),TEXT("Actual DX12 Unreal SceneCapture, package Landscape Grass Output"));
        R->SetNumberField(TEXT("outdated_grass_maps"),L->GetOutdatedGrassMapCount());
        const bool Ok=Capture(W,CaptureScene(Views,Config->GetNumberField(TEXT("capture_exposure_bias"))),Dir());
        TArray<V> Cs; for (UHierarchicalInstancedStaticMeshComponent* C:L->FoliageComponents) if (C) { auto Q=Obj(); Q->SetStringField(TEXT("mesh"),GetPathNameSafe(C->GetStaticMesh())); Q->SetNumberField(TEXT("render_instances"),C->GetNumRenderInstances()); Cs.Add(Val(Q)); } R->SetArrayField(TEXT("grass_components"),Cs);
        R->SetBoolField(TEXT("captures_saved"),Ok); Write(IsTrial?TEXT("trial_capture.json"):TEXT("capture.json"),R);
        // Persist baked grass data; this never imports/replaces Landscape geometry.
        return Ok && FEditorFileUtils::SaveMap(W,FPackageName::LongPackageNameToFilename(Path,FPackageName::GetMapPackageExtension()));
    }
    if (Mode!=TEXT("environmenttrial") && Mode!=TEXT("environmentgenerate") && Mode!=TEXT("environmentverify")) return false;
    auto Data=Read(Dir()/TEXT("environment_compiled.json")); if (!Data) return false;
    if (Data->GetStringField(TEXT("config_md5"))!=LexToString(FMD5Hash::HashFile(*(FPaths::ProjectDir()/TEXT("Design/Environment/environment.json"))))) return false;
    const FString Root=TEXT("/Game/Environment/ThanhA/")+Data->GetStringField(TEXT("fingerprint"));
    auto Before=Manifest(W); const auto H=Heights(L,N); const FString HeightHash=Hash(H);
    auto Report=Obj(); Report->SetStringField(TEXT("engine"),FEngineVersion::Current().ToString()); Report->SetStringField(TEXT("mode"),Mode); Report->SetStringField(TEXT("fingerprint"),Data->GetStringField(TEXT("fingerprint"))); Report->SetStringField(TEXT("height_before_md5"),HeightHash);
    TArray<V> Errors;
    if (Mode!=TEXT("environmentverify"))
    {
        // Prepare and verify every source before modifying the loaded map.
        auto* Sample=LoadObject<UMaterialInstanceConstant>(nullptr,*Config->GetStringField(TEXT("landscape_instance"))); if (!Sample) return false;
        auto* Master=Duplicate(Sample->GetMaterial(),Root,TEXT("M_ThanhA_Landscape")); if (!Master) return false;
        for (UMaterialExpression* E:Master->GetExpressions()) if (auto* G=Cast<UMaterialExpressionLandscapeGrassOutput>(E)) for (auto& Input:G->GrassTypes)
        {
            if (!Input.GrassType) continue;
            auto* Source=LoadObject<ULandscapeGrassType>(nullptr,*Config->GetObjectField(TEXT("grass_types"))->GetStringField(Input.Name.ToString())); if (!Source) return false;
            auto* T=Duplicate(Source,Root,TEXT("GT_ThanhA_")+Input.Name.ToString()); if (!T) return false;
            T->GrassVarieties=Source->GrassVarieties;
            for (auto& Variety:T->GrassVarieties)
            {
                Variety.GrassDensity.Default*=Config->GetNumberField(TEXT("grass_density_multiplier"));
                Variety.StartCullDistance.Default=Config->GetIntegerField(TEXT("grass_start_cull_cm")); Variety.EndCullDistance.Default=Config->GetIntegerField(TEXT("grass_end_cull_cm"));
                Variety.bKeepInstanceBufferCPUCopy=true; Variety.bCastDynamicShadow=false;
            }
            // Reused assets are immutable for a given fingerprint; only initialize on first creation.
            T->PostEditChange(); if (!Save(T)) return false; Input.GrassType=T;
        }
        Master->PostEditChange(); if (!Save(Master)) return false;
        auto* MI=Duplicate(Sample,Root,TEXT("MI_ThanhA_Landscape")); MI->SetParentEditorOnly(Master); MI->PostEditChange(); if (!Save(MI)) return false;
        // Roads need a mesh-compatible shader. It uses discovered pack ground textures and 4 m world UVs.
        UMaterial* Road=LoadObject<UMaterial>(nullptr,*(Root/TEXT("M_ThanhA_Path.M_ThanhA_Path")),nullptr,LOAD_NoWarn);
        if (!Road)
        {
            Road=NewObject<UMaterial>(CreatePackage(*(Root/TEXT("M_ThanhA_Path"))),TEXT("M_ThanhA_Path"),RF_Public|RF_Standalone);
            auto* P=NewObject<UMaterialExpressionWorldPosition>(Road); auto* UV=NewObject<UMaterialExpressionCustom>(Road); UV->Code=TEXT("return P.xy / 400.0;"); UV->OutputType=CMOT_Float2; FCustomInput In; In.InputName=TEXT("P"); In.Input.Connect(0,P); UV->Inputs.Add(In);
            auto* B=NewObject<UMaterialExpressionTextureSample>(Road); B->Texture=LoadObject<UTexture>(nullptr,*Config->GetStringField(TEXT("ground_basecolor"))); B->SamplerType=SAMPLERTYPE_Color; B->Coordinates.Connect(0,UV);
            auto* Normal=NewObject<UMaterialExpressionTextureSample>(Road); Normal->Texture=LoadObject<UTexture>(nullptr,*Config->GetStringField(TEXT("ground_normal"))); Normal->SamplerType=SAMPLERTYPE_Normal; Normal->Coordinates.Connect(0,UV);
            auto* Rough=NewObject<UMaterialExpressionConstant>(Road); Rough->R=1;
            for (auto* E:TArray<UMaterialExpression*>{P,UV,B,Normal,Rough}) Road->GetExpressionCollection().AddExpression(E);
            Road->GetEditorOnlyData()->BaseColor.Connect(0,B); Road->GetEditorOnlyData()->Normal.Connect(0,Normal); Road->GetEditorOnlyData()->Roughness.Connect(0,Rough); Road->PostEditChange(); FAssetRegistryModule::AssetCreated(Road); if (!Save(Road)) return false;
        }
        TMap<FString,UStaticMesh*> Meshes; for (auto& Entry:Data->GetArrayField(TEXT("instances")))
        { const auto O=Entry->AsObject(); const FString M=O->GetStringField(TEXT("mesh")); if (!Meshes.Contains(M)) { auto* Mesh=LoadObject<UStaticMesh>(nullptr,*M); if (!Mesh) return false; Meshes.Add(M,Mesh); } }
        FAssetCompilingManager::Get().FinishAllCompilation();
        auto* Cliff=LoadObject<UMaterialInterface>(nullptr,*Config->GetStringField(TEXT("cliff_material"))); if (!Cliff) return false;
        TArray<AActor*> Old; for (TActorIterator<AActor> It(W);It;++It) if (It->ActorHasTag(TEXT("ThanhA.Environment.v1"))) Old.Add(*It); for (auto* A:Old) W->DestroyActor(A);
        L->LandscapeMaterial=MI; L->PostEditChange();
        for (const auto& Layer:Config->GetObjectField(TEXT("layers"))->Values)
        {
            const FString Name(Layer.Key.ToView()); auto* Source=LoadObject<ULandscapeLayerInfoObject>(nullptr,*Layer.Value->AsString()); if (!Source) return false;
            auto* Info=Duplicate(Source,Root,TEXT("LI_ThanhA_")+Name); if (!Save(Info)) return false;
            if (auto* OldInfo=L->GetLandscapeInfo()->GetLayerInfoByName(FName(*Name))) if (OldInfo!=Info) L->GetLandscapeInfo()->ReplaceLayer(OldInfo,Info);
            L->AddTargetLayer(FName(*Name),FLandscapeTargetLayerSettings(Info),false); L->GetLandscapeInfo()->UpdateLayerInfoMap(L);
            TArray<uint8> Weight; if (!FFileHelper::LoadFileToArray(Weight,*(Dir()/(Name+TEXT(".raw")))) || Weight.Num()!=N*N) return false;
            FLandscapeEditDataInterface Edit(L->GetLandscapeInfo()); Edit.SetAlphaData(Info,0,0,N-1,N-1,Weight.GetData(),N,ELandscapeLayerPaintingRestriction::None); Edit.Flush();
        }
        for (ULandscapeComponent* C:L->LandscapeComponents) C->UpdateMaterialInstances(); L->FlushGrassComponents();
        const TArray<V>& Natural=Data->GetArrayField(TEXT("natural_proxy_ids")); TSet<FString> Allowed; for (auto& I:Natural) Allowed.Add(I->AsString());
        TArray<V> Replaced;
        for (TActorIterator<AStaticMeshActor> It(W);It;++It)
        {
            FString Id; for (FName T:It->Tags) if (T.ToString().StartsWith(TEXT("ThanhA.Id."))) Id=T.ToString().RightChop(10);
            if (!It->ActorHasTag(TEXT("ThanhA.Blockout.v1")) || !Allowed.Contains(Id)) continue;
            if (IsTrial && !It->GetComponentsBoundingBox(true).Intersect(FBox(Vector(Config->GetArrayField(TEXT("trial_min_m")))*100,Vector(Config->GetArrayField(TEXT("trial_max_m")))*100))) continue;
            const bool IsRoad=It->GetFolderPath().ToString()==TEXT("Roads");
            It->GetStaticMeshComponent()->SetMaterial(0,IsRoad?Road:Cliff); Replaced.Add(MakeShared<FJsonValueString>(Id));
            if (IsRoad) if (auto* Mesh=Shoulder(It->GetStaticMeshComponent(),L,Root,Id))
            {
                auto* A=W->SpawnActor<AStaticMeshActor>(); Own(A,TEXT("shoulder_")+Id,TEXT("Environment/Paths")); auto* C=A->GetStaticMeshComponent(); C->SetMobility(EComponentMobility::Static); C->SetStaticMesh(Mesh); C->SetMaterial(0,Road); C->SetCollisionProfileName(TEXT("NoCollision"));
            }
        }
        Report->SetArrayField(TEXT("natural_proxies_retextured"),Replaced);
        TMap<FString,UHierarchicalInstancedStaticMeshComponent*> Groups;
        TArray<V> Applied,Rejected;
        const FBox TrialBox(Vector(Config->GetArrayField(TEXT("trial_min_m")))*100,Vector(Config->GetArrayField(TEXT("trial_max_m")))*100);
        for (auto& Entry:Data->GetArrayField(TEXT("instances")))
        {
            auto O=Entry->AsObject(); FVector P=Vector(O->GetArrayField(TEXT("position_m")))*100;
            if (IsTrial && !TrialBox.IsInsideXY(P)) continue;
            const FString M=O->GetStringField(TEXT("mesh")); auto* Mesh=Meshes[M]; const float Scale=O->GetNumberField(TEXT("scale"));
            const auto Z=L->GetHeightAtLocation(P); if (!Z.IsSet()) { Rejected.Add(MakeShared<FJsonValueString>(O->GetStringField(TEXT("id")))); continue; }
            P.Z=Z.GetValue(); const FString Kind=O->GetStringField(TEXT("kind"));
            const FBox Bounds=Mesh->GetBoundingBox(); const bool Rock=Kind.Contains(TEXT("rock"));
            // Vegetation pivots are at ground; flower_01 has elevated petals, so preserve its source pivot.
            if (Rock) P.Z-=Bounds.Min.Z*Scale+Bounds.GetSize().Z*Scale*O->GetNumberField(TEXT("embed_fraction"));
            else P.Z-=FMath::Min(0.0,Bounds.Min.Z)*Scale*.5;
            const FString Group=Kind+TEXT("_")+Mesh->GetName(); auto* C=Groups.FindRef(Group);
            if (!C)
            {
                auto* A=W->SpawnActor<AActor>(); Own(A,Group,Rock?TEXT("Environment/Rocks"):TEXT("Environment/Vegetation"));
                auto* RootComponent=NewObject<USceneComponent>(A,TEXT("Root")); RootComponent->SetMobility(EComponentMobility::Static); A->SetRootComponent(RootComponent); A->AddInstanceComponent(RootComponent); RootComponent->RegisterComponent();
                C=NewObject<UHierarchicalInstancedStaticMeshComponent>(A,*Group); A->AddInstanceComponent(C); C->SetupAttachment(RootComponent); C->SetMobility(EComponentMobility::Static); C->SetStaticMesh(Mesh); C->SetCollisionProfileName(O->GetBoolField(TEXT("collision"))?TEXT("BlockAll"):TEXT("NoCollision")); C->SetCanEverAffectNavigation(O->GetBoolField(TEXT("collision"))); C->SetCullDistances(Config->GetIntegerField(TEXT("vegetation_start_cull_cm")),Config->GetIntegerField(TEXT("vegetation_end_cull_cm"))); C->InstancingRandomSeed=Config->GetIntegerField(TEXT("seed")); C->bAutoRebuildTreeOnInstanceChanges=false; C->RegisterComponent(); Groups.Add(Group,C);
            }
            C->AddInstance(FTransform(FRotator(0,O->GetNumberField(TEXT("yaw_deg")),0),P,FVector(Scale)),true); auto Q=Obj(); Q->SetStringField(TEXT("id"),O->GetStringField(TEXT("id"))); Q->SetStringField(TEXT("mesh"),M); Q->SetArrayField(TEXT("location_cm"),XYZ(P)); Q->SetNumberField(TEXT("scale"),Scale); Applied.Add(Val(Q));
        }
        for (auto& G:Groups) G.Value->BuildTreeIfOutdated(false,true);
        Report->SetArrayField(TEXT("applied_instances"),Applied); Report->SetArrayField(TEXT("rejected_instances"),Rejected); Report->SetNumberField(TEXT("instanced_groups"),Groups.Num());
        auto* Marker=W->SpawnActor<AActor>(); Own(Marker,TEXT("environment_manifest"),TEXT("Environment")); Marker->Tags.Add(FName(*(TEXT("ThanhA.Environment.Fingerprint.")+Data->GetStringField(TEXT("fingerprint")))));
        // Reorient the existing sun using the sample direction, preserving the city's calibrated physical exposure.
        FVector SR=Vector(Config->GetArrayField(TEXT("sun_rotation_deg"))); for (TActorIterator<ADirectionalLight> It(W);It;++It)
        {
            if (It->ActorHasTag(TEXT("ThanhA.Id.sun"))) It->SetActorRotation(FRotator(SR.X,SR.Y,0));
            if (It->ActorHasTag(TEXT("ThanhA.Id.blockout_fill"))) It->SetActorRotation(FRotator(-35,SR.Y+180,0));
        }
        W->UpdateWorldComponents(false,false); W->SendAllEndOfFrameUpdates();
    }
    const FString AfterHash=Hash(Heights(L,N)); Report->SetStringField(TEXT("height_after_md5"),AfterHash); if (AfterHash!=HeightHash) Errors.Add(MakeShared<FJsonValueString>(TEXT("Landscape height changed")));
    if (AfterHash!=Data->GetStringField(TEXT("heightmap_md5"))) Errors.Add(MakeShared<FJsonValueString>(TEXT("Landscape differs from the established heightmap")));
    auto WeightChecks=Obj();
    for (const auto& Layer:Config->GetObjectField(TEXT("layers"))->Values)
    {
        const FString Name(Layer.Key.ToView()); auto* Info=L->GetLandscapeInfo()->GetLayerInfoByName(FName(*Name)); if (!Info) return false;
        TArray<uint8> ActualWeights,ExpectedWeights; ActualWeights.SetNumZeroed(N*N);
        FLandscapeEditDataInterface Edit(L->GetLandscapeInfo()); Edit.GetWeightDataFast(Info,0,0,N-1,N-1,ActualWeights.GetData(),N);
        if (!FFileHelper::LoadFileToArray(ExpectedWeights,*(Dir()/(Name+TEXT(".raw"))))) return false;
        const bool Same=ActualWeights==ExpectedWeights; WeightChecks->SetBoolField(Name,Same); if (!Same) Errors.Add(MakeShared<FJsonValueString>(TEXT("Saved landscape weights differ: ")+Name));
    }
    Report->SetObjectField(TEXT("actual_weightmaps_match_masks"),WeightChecks);
    auto After=Manifest(W);
    auto Baseline=Read(Dir()/TEXT("baseline_manifest.json")); if (!Baseline) return false;
    TSet<FString> Natural; for (auto& I:Data->GetArrayField(TEXT("natural_proxy_ids"))) Natural.Add(I->AsString());
    auto Expected=ProtectedSignatures(Baseline,Natural),Actual=ProtectedSignatures(After,Natural);
    for (auto& Pair:Expected) if (!Actual.Contains(Pair.Key) || Actual[Pair.Key]!=Pair.Value) Errors.Add(MakeShared<FJsonValueString>(TEXT("Protected blockout changed: ")+Pair.Key));
    Report->SetNumberField(TEXT("protected_static_mesh_actors"),Expected.Num());
    Write(IsTrial?TEXT("trial_after_manifest.json"):TEXT("after_manifest.json"),After);
    if (Mode!=TEXT("environmentverify")) Write(IsTrial?TEXT("trial_before_manifest.json"):TEXT("before_manifest.json"),Before);
    Report->SetArrayField(TEXT("errors"),Errors); Report->SetStringField(TEXT("status"),Errors.Num()?TEXT("fail"):TEXT("pass"));
    Report->SetStringField(TEXT("landscape_material"),GetPathNameSafe(L->LandscapeMaterial));
    const FString SavePath=IsTrial?Trial:Target;
    bool Saved=Mode==TEXT("environmentverify") || (Errors.Num()==0 && FEditorFileUtils::SaveMap(W,FPackageName::LongPackageNameToFilename(SavePath,FPackageName::GetMapPackageExtension())));
    Report->SetBoolField(TEXT("saved"),Saved); Write(IsTrial?TEXT("trial_execution.json"):Mode==TEXT("environmentverify")?TEXT("verification.json"):TEXT("execution.json"),Report);
    return Saved && Errors.Num()==0;
}
bool ThanhAEnvironmentAudit()
{
    using namespace Forest;
    IFileManager::Get().MakeDirectory(*Dir(),true);
    auto R=Obj(); R->SetStringField(TEXT("execution"),TEXT("UE Asset Registry and loaded sample maps, read-only; no source assets saved")); R->SetStringField(TEXT("engine"),FEngineVersion::Current().ToString());
    auto& Registry=FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get(); Registry.SearchAllAssets(true);
    TArray<FAssetData> All; Registry.GetAllAssets(All); TArray<V> Catalog,Materials,Meshes,Types; TArray<FString> Maps;
    // The installed directory was discovered on disk. Registry is the authoritative catalog.
    for (auto& A:All) if (A.PackagePath.ToString().StartsWith(TEXT("/Game/Stylized_Forest")))
    {
        auto X=Obj(); X->SetStringField(TEXT("path"),A.GetObjectPathString()); X->SetStringField(TEXT("class"),A.AssetClassPath.ToString()); Catalog.Add(Val(X));
        if (A.AssetClassPath.GetAssetName()==TEXT("World")) { Maps.Add(A.PackageName.ToString()); continue; }
        auto* O=A.GetAsset(); if (!O) continue;
        if (auto* Mat=Cast<UMaterialInterface>(O)) Materials.Add(Val(Material(Mat)));
        else if (auto* F=Cast<UMaterialFunction>(O)) { X->SetArrayField(TEXT("graph"),Graph(F->GetExpressions())); Materials.Add(Val(X)); }
        else if (auto* M=Cast<UStaticMesh>(O))
        {
            FAssetCompilingManager::Get().FinishAllCompilation(); X->SetArrayField(TEXT("bounds_min_cm"),XYZ(M->GetBoundingBox().Min)); X->SetArrayField(TEXT("bounds_max_cm"),XYZ(M->GetBoundingBox().Max)); X->SetNumberField(TEXT("lod_count"),M->GetNumLODs());
            if (auto* B=M->GetBodySetup()) { X->SetNumberField(TEXT("simple_collision_shapes"),B->AggGeom.GetElementCount()); X->SetNumberField(TEXT("collision_trace_flag"),B->CollisionTraceFlag); }
            TArray<V> Mats; for (auto& S:M->GetStaticMaterials()) Mats.Add(MakeShared<FJsonValueString>(GetPathNameSafe(S.MaterialInterface))); X->SetArrayField(TEXT("materials"),Mats); Meshes.Add(Val(X));
        }
        else { X->SetObjectField(TEXT("properties"),Props(O)); Types.Add(Val(X)); }
    }
    R->SetArrayField(TEXT("catalog"),Catalog); R->SetArrayField(TEXT("materials"),Materials); R->SetArrayField(TEXT("meshes"),Meshes); R->SetArrayField(TEXT("types"),Types);
    TArray<V> Samples; Maps.Sort(); for (auto& P:Maps) Samples.Add(Val(World(P))); R->SetArrayField(TEXT("samples"),Samples);
    R->SetObjectField(TEXT("target"),World(TEXT("/Game/Maps/Blockout/L_ThanhA_Blockout")));
    Write(TEXT("baseline_manifest.json"),Manifest(GEditor->GetEditorWorldContext().World()));
    return Catalog.Num()>0 && Write(TEXT("forest_audit.json"),R);
}
