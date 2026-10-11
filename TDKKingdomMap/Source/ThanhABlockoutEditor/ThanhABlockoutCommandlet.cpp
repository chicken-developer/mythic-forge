#include "ThanhABlockoutCommandlet.h"
#include "ThanhACharacter.h"
#include "ThanhAAssetAudit.h"
#include "ThanhAEnvironment.h"
#include "ThanhAMetricGrid.h"
#include "ThanhARoadGeometry.h"
#include "ThanhADimensionTools.h"
#include "ThanhADimensionComponent.h"
#include "ThanhABlockoutMeshActor.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Engine/Canvas.h"
#include "Engine/Font.h"
#include "ThanhACalibration.h"
#include "Engine/Engine.h"
#include "CanvasItem.h"
#include "CanvasTypes.h"
#include "Editor.h"
#include "FileHelpers.h"
#include "EngineUtils.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Engine/ExponentialHeightFog.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/WorldSettings.h"
#include "Landscape.h"
#include "LandscapeComponent.h"
#include "LandscapeInfo.h"
#include "LandscapeHeightfieldCollisionComponent.h"
#include "PhysicsEngine/BodySetup.h"
#include "MeshDescription.h"
#include "StaticMeshAttributes.h"
#include "StaticMeshOperations.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionConstant.h"
#include "MaterialDomain.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"
#include "UObject/MetaData.h"
#include "ImageUtils.h"
#include "RenderingThread.h"
#include "Modules/ModuleManager.h"
#include "ShaderCompiler.h"
#include "AssetCompilingManager.h"
#include "InstancedFoliageActor.h"
#include "FoliageType.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Misc/SecureHash.h"

DEFINE_LOG_CATEGORY_STATIC(LogThanhA, Log, All);

namespace ThanhA
{
const FName Owner(TEXT("ThanhA.Blockout.v1"));
const FString LevelPath(TEXT("/Game/Maps/Blockout/L_ThanhA_Blockout"));
TSharedPtr<FJsonObject> ReadJson(const FString& Path)
{
    FString Text;
    if (!FFileHelper::LoadFileToString(Text,*Path)) return nullptr;
    TSharedPtr<FJsonObject> Result;
    if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Result)) return nullptr;
    return Result;
}
bool WriteJson(const FString& Path, const TSharedRef<FJsonObject>& Value)
{
    FString Text; FJsonSerializer::Serialize(Value,TJsonWriterFactory<>::Create(&Text));
    return FFileHelper::SaveStringToFile(Text,*Path);
}
FVector Vector(const TArray<TSharedPtr<FJsonValue>>& A)
{
    return FVector(A[0]->AsNumber(),A[1]->AsNumber(), A.Num()>2 ? A[2]->AsNumber() : 0);
}
FVector V(const TSharedPtr<FJsonObject>& O, const TCHAR* Key) { return Vector(O->GetArrayField(Key)); }
bool SaveAsset(UObject* Asset)
{
    UPackage* P=Asset->GetOutermost();
    FSavePackageArgs Args; Args.TopLevelFlags=RF_Public|RF_Standalone; Args.SaveFlags=SAVE_NoError;
    P->MarkPackageDirty();
    return UPackage::SavePackage(P,Asset,*FPackageName::LongPackageNameToFilename(P->GetName(),FPackageName::GetAssetPackageExtension()),Args);
}
void Own(AActor* A,const FString& Id,const FString& Folder)
{
    A->Tags.AddUnique(Owner); A->Tags.AddUnique(FName(*(TEXT("ThanhA.Id.")+Id)));
    A->SetActorLabel(Id); A->SetFolderPath(FName(*Folder));
}
UMaterial* Material(const FString& Root,const FString& Hex)
{
    const FString Name=TEXT("M_")+Hex.RightChop(1); const FString Path=Root+TEXT("/")+Name;
    if (FPackageName::DoesPackageExist(Path)) if (UMaterial* Existing=LoadObject<UMaterial>(nullptr,*(Path+TEXT(".")+Name))) return Existing;
    UPackage* Package=CreatePackage(*Path);
    UMaterial* M=NewObject<UMaterial>(Package,*Name,RF_Public|RF_Standalone);
    M->GetOutermost()->GetMetaData().SetValue(M,TEXT("ThanhAOwner"),TEXT("ThanhA.Blockout.v1"));
    auto* Color=NewObject<UMaterialExpressionConstant3Vector>(M); Color->Constant=FLinearColor(FColor::FromHex(Hex));
    auto* Rough=NewObject<UMaterialExpressionConstant>(M); Rough->R=.85f;
    M->GetExpressionCollection().AddExpression(Color); M->GetExpressionCollection().AddExpression(Rough);
    M->GetEditorOnlyData()->BaseColor.Connect(0,Color); M->GetEditorOnlyData()->Roughness.Connect(0,Rough);
    M->PostEditChange(); FAssetRegistryModule::AssetCreated(M);
    if (!SaveAsset(M)) return nullptr;
    return M;
}
UStaticMesh* Mesh(const FString& Root,const FString& Id,const TArray<FVector>& Vertices,const TArray<FIntVector>& Triangles,UStaticMesh* CollisionSource=nullptr)
{
    FString Name=TEXT("SM_")+Id.Replace(TEXT("-"),TEXT("_")); FString Path=Root+TEXT("/")+Name;
    if (FPackageName::DoesPackageExist(Path)) if (auto* Existing=LoadObject<UStaticMesh>(nullptr,*(Path+TEXT(".")+Name))) return Existing;
    auto* M=NewObject<UStaticMesh>(CreatePackage(*Path),*Name,RF_Public|RF_Standalone);
    M->GetOutermost()->GetMetaData().SetValue(M,TEXT("ThanhAOwner"),TEXT("ThanhA.Blockout.v1"));
    FMeshDescription D; FStaticMeshAttributes Attr(D); Attr.Register();
    auto Position=Attr.GetVertexPositions(); auto UV=Attr.GetVertexInstanceUVs(); UV.SetNumChannels(1);
    auto Group=D.CreatePolygonGroup(); Attr.GetPolygonGroupMaterialSlotNames()[Group]=TEXT("Blockout");
    TArray<FVertexID> IDs;
    for (const FVector& P:Vertices) { auto ID=D.CreateVertex(); Position[ID]=FVector3f(P*100); IDs.Add(ID); }
    for (const FIntVector& T:Triangles)
    {
        TArray<FVertexInstanceID> Instances;
        // Compiler geometry uses mathematical CCW; UE MeshDescription uses reversed cross product.
        for (int32 Index: {T.X,T.Z,T.Y})
        {
            auto Instance=D.CreateVertexInstance(IDs[Index]); UV.Set(Instance,0,FVector2f(Vertices[Index].X,Vertices[Index].Y)); Instances.Add(Instance);
        }
        D.CreatePolygon(Group,Instances);
    }
    auto Hardness=Attr.GetEdgeHardnesses();
    for (FEdgeID Edge:D.Edges().GetElementIDs()) Hardness[Edge]=true;
    FStaticMeshOperations::ComputeTriangleTangentsAndNormals(D);
    FStaticMeshOperations::ComputeTangentsAndNormals(D,EComputeNTBsFlags::Normals|EComputeNTBsFlags::Tangents);
    M->GetStaticMaterials().Add(FStaticMaterial(UMaterial::GetDefaultMaterial(MD_Surface),TEXT("Blockout")));
    M->SetNumSourceModels(1);
    M->CreateMeshDescription(0,MoveTemp(D)); M->CommitMeshDescription(0);
    M->GetSourceModel(0).BuildSettings.bRecomputeNormals=false;
    M->GetSourceModel(0).BuildSettings.bRecomputeTangents=false;
    M->GetSourceModel(0).BuildSettings.bGenerateLightmapUVs=false;
    M->ComplexCollisionMesh=CollisionSource;
    M->CreateBodySetup(); M->GetBodySetup()->CollisionTraceFlag=CTF_UseComplexAsSimple;
    M->Build(false); M->GetBodySetup()->CreatePhysicsMeshes();
    FAssetRegistryModule::AssetCreated(M);
    return SaveAsset(M) ? M : nullptr;
}
TSharedRef<FJsonObject> CollisionReport(UWorld* W,const TSharedPtr<FJsonObject>& Scene)
{
    auto Report=MakeShared<FJsonObject>(); TArray<TSharedPtr<FJsonValue>> Errors,Samples;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(ThanhAGround),true);
    auto Ground=[&](FVector P,double Expected,const FString& Id,bool Record)
    {
        P*=100; FHitResult Hit;
        bool Ok=W->LineTraceSingleByChannel(Hit,P+FVector(0,0,200),P-FVector(0,0,300),ECC_Visibility,Query);
        if (!Ok || FMath::Abs(Hit.ImpactPoint.Z/100-Expected)>.18)
            Errors.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("%s ground trace expected %.3fm, actual %.3fm (hit=%d)"),*Id,Expected,Hit.ImpactPoint.Z/100,Ok)));
        if (Record)
        {
            auto S=MakeShared<FJsonObject>(); S->SetStringField(TEXT("id"),Id); S->SetNumberField(TEXT("expected_m"),Expected); S->SetNumberField(TEXT("actual_m"),Hit.ImpactPoint.Z/100); S->SetBoolField(TEXT("hit"),Ok);
            Samples.Add(MakeShared<FJsonValueObject>(S));
        }
        return Ok;
    };
    for (auto Item:Scene->GetArrayField(TEXT("samples")))
    {
        auto S=Item->AsObject(); FVector P=V(S,TEXT("position_m")); double Z=S->GetNumberField(TEXT("ground_m")); P.Z=Z;
        Ground(P,Z+.04,S->GetStringField(TEXT("id")),true);
    }
    TArray<TSharedPtr<FJsonValue>> LandscapeSamples;
    for (TActorIterator<ALandscape> It(W);It;++It) if (It->ActorHasTag(Owner))
    {
        for (auto Item:Scene->GetArrayField(TEXT("samples")))
        {
            auto S=Item->AsObject(); FVector P=V(S,TEXT("position_m"))*100;
            auto Height=It->GetHeightAtLocation(P); double Expected=S->GetNumberField(TEXT("ground_m"));
            S->TryGetNumberField(TEXT("landscape_ground_m"),Expected);
            auto Sample=MakeShared<FJsonObject>(); Sample->SetStringField(TEXT("id"),S->GetStringField(TEXT("id")));
            Sample->SetNumberField(TEXT("expected_m"),Expected); Sample->SetBoolField(TEXT("height_available"),Height.IsSet());
            Sample->SetNumberField(TEXT("walking_datum_m"),S->GetNumberField(TEXT("ground_m")));
            if (Height.IsSet()) Sample->SetNumberField(TEXT("landscape_collision_m"),Height.GetValue()/100);
            if (!Height.IsSet() || FMath::Abs(Height.GetValue()/100-Expected)>.15)
                Errors.Add(MakeShared<FJsonValueString>(TEXT("Imported Landscape sample mismatch: ")+S->GetStringField(TEXT("id"))));
            LandscapeSamples.Add(MakeShared<FJsonValueObject>(Sample));
        }
    }
    if (LandscapeSamples.Num()==0) Errors.Add(MakeShared<FJsonValueString>(TEXT("No owned Landscape to verify")));
    int32 TraceCount=0,SweepCount=0;
    auto Pawn=Scene->GetObjectField(TEXT("pawn")); const double Radius=Pawn->GetNumberField(TEXT("radius_m"))*100,Half=Pawn->GetNumberField(TEXT("half_height_m"))*100;
    FCollisionShape Capsule=FCollisionShape::MakeCapsule(Radius,Half);
    for (auto Item:Scene->GetArrayField(TEXT("routes")))
    {
        auto R=Item->AsObject(); const auto& Points=R->GetArrayField(TEXT("points_m")); const FString Id=R->GetStringField(TEXT("id"));
        for (int32 K=0;K+1<Points.Num();K++)
        {
            FVector A=Vector(Points[K]->AsArray()),B=Vector(Points[K+1]->AsArray()); int32 N=FMath::CeilToInt(FVector::Dist2D(A,B));
            for (int32 I=0;I<=N;I++)
            {
                FVector P=FMath::Lerp(A,B,double(I)/N); Ground(P,P.Z+.04,Id,false); TraceCount++;
                if (I<N)
                {
                    FVector Q=FMath::Lerp(A,B,double(I+1)/N); FHitResult Hit;
                    bool Blocked=W->SweepSingleByChannel(Hit,P*100+FVector(0,0,Half+15),Q*100+FVector(0,0,Half+15),FQuat::Identity,ECC_Visibility,Capsule,Query);
                    SweepCount++;
                    if (Blocked) Errors.Add(MakeShared<FJsonValueString>(Id+TEXT(" capsule blocked by ")+GetNameSafe(Hit.GetActor())+FString::Printf(TEXT(" at %.1f,%.1fm"),P.X,P.Y)));
                }
            }
        }
    }
    int32 StairTraces=0;
    for (auto Item:Scene->GetArrayField(TEXT("stairs")))
    {
        auto Stair=Item->AsObject(); const auto& Points=Stair->GetArrayField(TEXT("points_m"));
        FVector A=Vector(Points[0]->AsArray()),B=Vector(Points[1]->AsArray()); double Rise=Stair->GetNumberField(TEXT("step_rise_m"));
        int32 Count=FMath::CeilToInt(FMath::Abs(B.Z-A.Z)/Rise); double Previous=A.Z+.04;
        for (int32 I=0;I<Count;I++)
        {
            FVector P=FMath::Lerp(A,B,(I+.5)/Count); P.Z=A.Z+(B.Z-A.Z)*(I+1)/Count;
            Ground(P,P.Z+.04,Stair->GetStringField(TEXT("id")),false); StairTraces++;
            FHitResult Hit;
            if (W->LineTraceSingleByChannel(Hit,P*100+FVector(0,0,200),P*100-FVector(0,0,200),ECC_Visibility,Query))
            {
                double Actual=Hit.ImpactPoint.Z/100;
                if (FMath::Abs(Actual-Previous)>Pawn->GetNumberField(TEXT("max_step_m"))+.03)
                    Errors.Add(MakeShared<FJsonValueString>(TEXT("Actual stair rise exceeds Pawn step: ")+Stair->GetStringField(TEXT("id"))));
                Previous=Actual;
            }
        }
    }
    Report->SetStringField(TEXT("status"),Errors.Num()?TEXT("fail"):TEXT("pass"));
    Report->SetStringField(TEXT("execution"),TEXT("Unreal Editor world physics queries: actual imported Landscape and static mesh collision; this is not a character playtest"));
    Report->SetArrayField(TEXT("errors"),Errors); Report->SetArrayField(TEXT("samples"),Samples); Report->SetArrayField(TEXT("landscape_samples"),LandscapeSamples); Report->SetNumberField(TEXT("route_ground_traces"),TraceCount); Report->SetNumberField(TEXT("route_capsule_sweeps"),SweepCount);
    Report->SetNumberField(TEXT("actual_stair_ground_traces"),StairTraces);
    return Report;
}
bool Capture(UWorld* W,const TSharedPtr<FJsonObject>& Scene,const FString& Dir)
{
    if (!FApp::CanEverRender()) return false;
    TArray<TSharedPtr<FJsonValue>> Views=Scene->GetArrayField(TEXT("capture_views"));
    if (FParse::Param(FCommandLine::Get(),TEXT("EnvironmentViews")))
    {
        auto CameraData=ReadJson(FPaths::ProjectDir()/TEXT("Design/Environment/capture_views.json"));
        if (!CameraData) return false;
        Views=CameraData->GetArrayField(TEXT("capture_views"));
    }
    UFont* CaptureFont=LoadObject<UFont>(nullptr,TEXT("/Engine/EngineFonts/RobotoDistanceField.RobotoDistanceField"));
    FAssetCompilingManager::Get().FinishAllCompilation();
    if (GShaderCompilingManager) GShaderCompilingManager->FinishAllCompilation();
    USkyLightComponent::UpdateSkyCaptureContents(W);
    auto* Camera=W->SpawnActor<ASceneCapture2D>(); auto* C=Camera->GetCaptureComponent2D();
    auto* RT=NewObject<UTextureRenderTarget2D>(Camera); RT->RenderTargetFormat=RTF_RGBA8; RT->InitAutoFormat(1600,1200);
    C->TextureTarget=RT; C->CaptureSource=SCS_FinalColorLDR; C->bCaptureEveryFrame=false; C->bCaptureOnMovement=false;
    C->bAlwaysPersistRenderingState=true;
    C->PostProcessSettings.bOverride_AutoExposureMethod=true; C->PostProcessSettings.AutoExposureMethod=AEM_Manual;
    C->PostProcessSettings.bOverride_AutoExposureBias=true;
    C->PostProcessSettings.AutoExposureBias=Scene->GetObjectField(TEXT("lighting"))->GetNumberField(TEXT("capture_exposure_bias"));
    C->PostProcessSettings.bOverride_AutoExposureApplyPhysicalCameraExposure=true;
    C->PostProcessSettings.AutoExposureApplyPhysicalCameraExposure=false;
    bool All=true;
    for (auto Item:Views)
    {
        auto View=Item->AsObject(); FVector P=V(View,TEXT("location_m"))*100,Target=V(View,TEXT("target_m"))*100;
        double AdvanceSeconds=0;
        if (View->TryGetNumberField(TEXT("advance_world_seconds"),AdvanceSeconds) && AdvanceSeconds>0) W->Tick(LEVELTICK_TimeOnly,AdvanceSeconds);
        FString ViewMode; View->TryGetStringField(TEXT("view_mode"),ViewMode);
        C->ShowFlags.SetWireframe(ViewMode==TEXT("wireframe")); C->ShowFlags.SetLighting(ViewMode!=TEXT("unlit") && ViewMode!=TEXT("wireframe"));
        const bool AtmosphericView=ViewMode!=TEXT("unlit") && ViewMode!=TEXT("wireframe");
        C->ShowFlags.SetFog(AtmosphericView); C->ShowFlags.SetVolumetricFog(AtmosphericView); C->ShowFlags.SetAtmosphere(AtmosphericView);
        C->CaptureSource=ViewMode==TEXT("base_color")?SCS_BaseColor:SCS_FinalColorLDR;
        C->PostProcessSettings.AutoExposureBias=(ViewMode==TEXT("unlit") || ViewMode==TEXT("wireframe"))?0:Scene->GetObjectField(TEXT("lighting"))->GetNumberField(TEXT("capture_exposure_bias"));
        Camera->SetActorLocation(P); Camera->SetActorRotation((Target-P).Rotation());
        double Width=0;
        if (View->TryGetNumberField(TEXT("ortho_width_m"),Width))
        { C->ProjectionType=ECameraProjectionMode::Orthographic; C->OrthoWidth=Width*100; if (View->GetStringField(TEXT("id"))==TEXT("top_down")) Camera->SetActorRotation(FRotator(-90,-90,0)); }
        else { C->ProjectionType=ECameraProjectionMode::Perspective; C->FOVAngle=View->GetNumberField(TEXT("fov")); }
        W->SendAllEndOfFrameUpdates();
        for (int32 Warmup=0;Warmup<4;Warmup++) { C->CaptureScene(); FlushRenderingCommands(); }
        FString LabelId;
        if (View->TryGetStringField(TEXT("label_id"),LabelId)) for (TActorIterator<AActor> It(W);It;++It) if (const auto* Metadata=It->FindComponentByClass<UThanhADimensionComponent>()) if (Metadata->StableId==LabelId)
        {
            UCanvas* Canvas=nullptr; FVector2D CanvasSize; FDrawToRenderTargetContext Context;
            UKismetRenderingLibrary::BeginDrawCanvasToRenderTarget(W,RT,Canvas,CanvasSize,Context);
            if (!CaptureFont || CaptureFont->FontCacheType!=EFontCacheType::Offline || !CaptureFont->Characters.Num()) { UE_LOG(LogThanhA,Error,TEXT("Capture font has no offline glyphs")); All=false; UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(W,Context); break; }
            FCanvasTextItem Text(FVector2D(25,25),FText::FromString(ThanhADimensionLabel(Metadata,ThanhAMeasure(Metadata,Metadata->MeasureWholeBuilding),true)),CaptureFont,FLinearColor::White); Text.Scale=FVector2D(.7,.7);
            Text.EnableShadow(FLinearColor::Black); if (Canvas) Canvas->Canvas->DrawItem(Text);
            UE_LOG(LogThanhA,Display,TEXT("Capture label %s: %d glyphs, %d atlas pages, drawn %.1fx%.1f"),*LabelId,CaptureFont->Characters.Num(),CaptureFont->Textures.Num(),Text.DrawnSize.X,Text.DrawnSize.Y);
            UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(W,Context); FlushRenderingCommands(); break;
        }
        TArray<FColor> Pixels; bool ReadOk=RT->GameThread_GetRenderTargetResource()->ReadPixels(Pixels);
        int32 VisiblePixels=0; for (FColor Pixel:Pixels) if (Pixel.R>2 || Pixel.G>2 || Pixel.B>2) VisiblePixels++;
        if (!ReadOk || Pixels.Num()!=1600*1200 || VisiblePixels<100)
        { UE_LOG(LogThanhA,Error,TEXT("Capture %s is empty/black; no valid image saved."),*View->GetStringField(TEXT("id"))); All=false; continue; }
        // With +X East,+Y North in UE, this camera has South at screen top.
        // Reverse exported rows for a North-up, East-right plan; preserve perspective views.
        if (View->GetStringField(TEXT("id"))==TEXT("top_down")) for (int32 Y=0;Y<600;Y++) for (int32 X=0;X<1600;X++) Swap(Pixels[Y*1600+X],Pixels[(1199-Y)*1600+X]);
        TArray64<uint8> PNG; FImageUtils::PNGCompressImageArray(1600,1200,Pixels,PNG);
        All &= FFileHelper::SaveArrayToFile(PNG,*(Dir/TEXT("editor_")+View->GetStringField(TEXT("id"))+TEXT(".png")));
    }
    if (FParse::Param(FCommandLine::Get(),TEXT("EnvironmentViews")))
    {
        auto Receipt=MakeShared<FJsonObject>(); Receipt->SetArrayField(TEXT("capture_views"),Views);
        Receipt->SetStringField(TEXT("execution"),TEXT("Actual UE DX12 SceneCapture; camera-only override, no generation input changes"));
        Receipt->SetBoolField(TEXT("captures_saved"),All); Receipt->SetNumberField(TEXT("width"),1600); Receipt->SetNumberField(TEXT("height"),1200);
        All &= WriteJson(Dir/TEXT("camera_capture_report.json"),Receipt);
    }
    W->DestroyActor(Camera); return All;
}
}

UThanhABlockoutCommandlet::UThanhABlockoutCommandlet()
{
    // UE 5.8 AllocateScene returns FNULLSceneInterface when GIsClient is false.
    IsClient=true; IsServer=false; IsEditor=true; LogToConsole=true; ShowErrorCount=true;
}
int32 UThanhABlockoutCommandlet::Main(const FString& Params)
{
    using namespace ThanhA;
    FString Mode=TEXT("validate"); FParse::Value(*Params,TEXT("mode="),Mode);
    if (Mode==TEXT("samplefoliageaudit"))
    {
        auto Config=ReadJson(FPaths::ProjectDir()/TEXT("Design/Environment/environment.json")); if (!Config) return 1;
        const FString Sample=Config->GetStringField(TEXT("sample_map"));
        if (!FEditorFileUtils::LoadMap(FPackageName::LongPackageNameToFilename(Sample,FPackageName::GetMapPackageExtension()),false,false)) return 1;
        auto Receipt=MakeShared<FJsonObject>(); TArray<TSharedPtr<FJsonValue>> Types;
        for (TActorIterator<AInstancedFoliageActor> It(GEditor->GetEditorWorldContext().World());It;++It)
        for (const auto& Pair:It->GetFoliageInfos())
        {
            auto Row=MakeShared<FJsonObject>(); auto* Type=Pair.Key;
            Row->SetStringField(TEXT("type"),Type->GetPathName()); Row->SetStringField(TEXT("source"),GetPathNameSafe(Type->GetSource()));
            Row->SetNumberField(TEXT("density"),Type->Density); Row->SetNumberField(TEXT("radius_cm"),Type->Radius);
            auto Properties=MakeShared<FJsonObject>();
            for (TFieldIterator<FProperty> P(Type->GetClass());P;++P) if (P->HasAnyPropertyFlags(CPF_Edit))
            { FString S; P->ExportTextItem_Direct(S,P->ContainerPtrToValuePtr<void>(Type),nullptr,Type,PPF_None); Properties->SetStringField(P->GetName(),S); }
            Row->SetObjectField(TEXT("editable_properties"),Properties);
            if (auto* C=Pair.Value->GetComponent())
            {
                Row->SetNumberField(TEXT("instances"),C->GetInstanceCount());
                Row->SetNumberField(TEXT("start_cull_cm"),C->InstanceStartCullDistance); Row->SetNumberField(TEXT("end_cull_cm"),C->InstanceEndCullDistance);
                Row->SetStringField(TEXT("collision_profile"),C->GetCollisionProfileName().ToString());
                FVector Lo(DBL_MAX),Hi(-DBL_MAX);
                for (int32 I=0;I<C->GetInstanceCount();++I) { FTransform T; C->GetInstanceTransform(I,T,true); const FVector S=T.GetScale3D(); Lo=Lo.ComponentMin(S); Hi=Hi.ComponentMax(S); }
                auto JsonVector=[](FVector P) { return TArray<TSharedPtr<FJsonValue>>{MakeShared<FJsonValueNumber>(P.X),MakeShared<FJsonValueNumber>(P.Y),MakeShared<FJsonValueNumber>(P.Z)}; };
                if (C->GetInstanceCount()) { Row->SetArrayField(TEXT("actual_scale_min"),JsonVector(Lo)); Row->SetArrayField(TEXT("actual_scale_max"),JsonVector(Hi)); }
            }
            Types.Add(MakeShared<FJsonValueObject>(Row));
        }
        Receipt->SetStringField(TEXT("sample_map"),Sample); Receipt->SetStringField(TEXT("execution"),TEXT("Read-only UE loaded sample: embedded UFoliageType settings and all placed instance scale bounds; no sample packages saved")); Receipt->SetArrayField(TEXT("foliage_types"),Types);
        return WriteJson(FPaths::ProjectDir()/TEXT("Design/Environment/Generated/sample_foliage.json"),Receipt) && Types.Num()>0?0:1;
    }
    if (Mode==TEXT("environmentaudit")) return ThanhAEnvironmentAudit()?0:1;
    if (Mode.StartsWith(TEXT("environment"))) return ThanhAEnvironmentRun(Mode,Capture)?0:1;
    const bool bStaging=FParse::Param(*Params,TEXT("staging"));
    const bool bDetails=Mode==TEXT("capture") && FParse::Param(*Params,TEXT("details"));
    const FString StagedPath=TEXT("/Game/Maps/Blockout/L_ThanhA_Blockout_Staged");
    if (Mode==TEXT("generate") && !FParse::Param(FCommandLine::Get(),TEXT("nullrhi")))
    { UE_LOG(LogThanhA,Error,TEXT("Generation requires -nullrhi. Run -mode=capture in a separate renderer process after saving the map.")); return 1; }
    FString Dir=FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/TEXT("Design/Blockout/Generated"));
    if (Mode==TEXT("audit")) return ThanhAAuditAssets(Dir)?0:1;
    auto Scene=ReadJson(Dir/TEXT("scene.json")); auto Meta=ReadJson(Dir/TEXT("heightmap_metadata.json")); auto Design=ReadJson(Dir/TEXT("design_validation.json"));
    if (!Scene || !Meta || !Design || Scene->GetStringField(TEXT("schema"))!=TEXT("thanh-a-scene") || Scene->GetIntegerField(TEXT("version"))!=1 || Scene->GetStringField(TEXT("units"))!=TEXT("m") || Scene->GetStringField(TEXT("level"))!=LevelPath || Design->GetStringField(TEXT("status"))!=TEXT("pass"))
    { UE_LOG(LogThanhA,Error,TEXT("Invalid or unprepared input. Run Tools/Blockout/prepare.py first.")); return 1; }
    TSet<FString> IDs;
    for (const auto& Pair:TArray<TPair<FString,FString>>{
        {TEXT("layout_md5"),FPaths::ProjectDir()/TEXT("Design/Blockout/layout.json")},
        {TEXT("compiler_md5"),FPaths::ProjectDir()/TEXT("Tools/Blockout/prepare.py")},
        {TEXT("heightmap_md5"),Dir/TEXT("terrain.r16")}})
    {
        FString Expected;
        if (!Scene->TryGetStringField(Pair.Key,Expected) || !LexToString(FMD5Hash::HashFile(*Pair.Value)).Equals(Expected,ESearchCase::IgnoreCase))
        { UE_LOG(LogThanhA,Error,TEXT("Stale/modified prepared input %s. Run prepare.py; no level modifications performed."),*Pair.Key); return 1; }
    }
    if (Mode==TEXT("calibrate") || Mode==TEXT("calibratecapture")) return ThanhACalibration(Scene,Dir,Mode==TEXT("calibratecapture"),Capture);
    for (auto Item:Scene->GetArrayField(TEXT("actors")))
    {
        FString Id=Item->AsObject()->GetStringField(TEXT("id"));
        if (IDs.Contains(Id)) { UE_LOG(LogThanhA,Error,TEXT("Duplicate actor ID %s"),*Id); return 1; } IDs.Add(Id);
    }
    TArray<uint8> Raw; int32 N=Meta->GetArrayField(TEXT("resolution"))[0]->AsNumber();
    if (!FFileHelper::LoadFileToArray(Raw,*(Dir/TEXT("terrain.r16"))) || Raw.Num()!=N*N*2)
    { UE_LOG(LogThanhA,Error,TEXT("Invalid heightmap byte length")); return 1; }
    if (Mode==TEXT("validate")) { UE_LOG(LogThanhA,Display,TEXT("Dry-run pass: %d actor IDs, %dx%d heightmap; no level changes."),IDs.Num(),N,N); return 0; }
    if (Mode!=TEXT("generate") && Mode!=TEXT("verify") && Mode!=TEXT("capture")) return 1;
    FString MapFile=FPackageName::LongPackageNameToFilename(LevelPath,FPackageName::GetMapPackageExtension());
    if (bStaging && Mode!=TEXT("generate")) MapFile=FPackageName::LongPackageNameToFilename(StagedPath,FPackageName::GetMapPackageExtension());
    UWorld* W=nullptr;
    if (FPaths::FileExists(MapFile)) { if (!FEditorFileUtils::LoadMap(MapFile,false,false)) return 1; W=GEditor->GetEditorWorldContext().World(); }
    else if (Mode==TEXT("generate")) W=GEditor->NewMap(false);
    if (!W || W->IsPartitionedWorld()) { UE_LOG(LogThanhA,Error,TEXT("Expected nonpartitioned target blockout map")); return 1; }
    if (Mode==TEXT("generate") && FParse::Param(*Params,TEXT("ownershipprobe")))
    {
        bool Exists=false; for (TActorIterator<AActor> It(W);It;++It) Exists |= It->ActorHasTag(TEXT("ThanhA.Test.ManualProbe"));
        if (!Exists)
        {
            auto* Probe=W->SpawnActor<AActor>(); Probe->SetActorLabel(TEXT("Manual_Rerun_Preservation_Probe"));
            Probe->Tags.Add(TEXT("ThanhA.Test.ManualProbe")); Probe->SetFolderPath(TEXT("Debug"));
        }
    }
    if (Mode==TEXT("generate"))
        for (TActorIterator<AActor> It(W);It;++It) if (It->ActorHasTag(TEXT("ThanhA.Environment.v1")))
        { UE_LOG(LogThanhA,Error,TEXT("This level contains finished environment data. Blockout Generate is disabled to protect Landscape paint, paths and vegetation. Use Tools/Environment/run.ps1 to regenerate environment; Verify/Capture remain available.")); return 1; }
    auto Report=MakeShared<FJsonObject>(); Report->SetStringField(TEXT("engine"),FEngineVersion::Current().ToString());
    Report->SetStringField(TEXT("level"),bStaging?StagedPath:LevelPath);
    Report->SetStringField(TEXT("fingerprint"),Scene->GetStringField(TEXT("fingerprint")));
    if (Mode==TEXT("generate"))
    {
        // Resolve/build every asset BEFORE replacing any owned actors. Assets are input-versioned.
        const FString Root=TEXT("/Game/Blockout/ThanhA/Generated/")+Scene->GetStringField(TEXT("fingerprint"));
        TMap<FString,UStaticMesh*> Meshes;
        auto Kit=Scene->GetObjectField(TEXT("prototyping"));
        Meshes.Add(TEXT("box"),LoadObject<UStaticMesh>(nullptr,*Kit->GetStringField(TEXT("box"))));
        Meshes.Add(TEXT("cylinder"),LoadObject<UStaticMesh>(nullptr,*Kit->GetStringField(TEXT("cylinder"))));
        Meshes.Add(TEXT("roof"),Mesh(Root,TEXT("roof"),{{-.5,-.5,0},{.5,-.5,0},{0,-.5,1},{-.5,.5,0},{.5,.5,0},{0,.5,1}},{{0,1,2},{3,5,4},{0,5,3},{0,2,5},{1,5,2},{1,4,5},{0,4,1},{0,3,4}}));
        Meshes.Add(TEXT("pyramid"),Mesh(Root,TEXT("pyramid"),{{-.5,-.5,0},{.5,-.5,0},{.5,.5,0},{-.5,.5,0},{0,0,1}},{{0,1,4},{1,2,4},{2,3,4},{3,0,4},{0,2,1},{0,3,2}}));
        for (const auto& Entry:Scene->GetObjectField(TEXT("meshes"))->Values)
        {
            TArray<FVector> Vertices; TArray<FIntVector> Triangles; auto O=Entry.Value->AsObject();
            for (auto X:O->GetArrayField(TEXT("vertices_m"))) Vertices.Add(Vector(X->AsArray()));
            for (auto X:O->GetArrayField(TEXT("triangles"))) { auto T=X->AsArray(); Triangles.Add(FIntVector(T[0]->AsNumber(),T[1]->AsNumber(),T[2]->AsNumber())); }
            const FString Key(Entry.Key.ToView());
            Meshes.Add(Key,Mesh(Root,Key,Vertices,Triangles));
        }
        TMap<FString,UMaterialInterface*> Materials;
        for (auto Item:Scene->GetArrayField(TEXT("actors")))
        {
            auto A=Item->AsObject(); FString Color=A->GetStringField(TEXT("color"));
            if (!Materials.Contains(Color)) Materials.Add(Color,A->GetStringField(TEXT("id"))==TEXT("water_datum")?Material(Root,Color):ThanhAMetricMaterial(Root,Color,Scene));
            if (!Meshes.FindRef(A->GetStringField(TEXT("mesh"))) || !Materials.FindRef(Color)) return 1;
        }
        UMaterialInterface* GroundMaterial=ThanhAMetricMaterial(Root,TEXT("#96a37b"),Scene); if (!GroundMaterial) return 1;
        // Resolve exact intersection geometry before removing any existing level content.
        TMap<FString,UE::Geometry::FDynamicMesh3> RoadSolids,CollisionSolids; auto GeometryReceipt=MakeShared<FJsonObject>();
        if (!ThanhATrimRoadSurfaces(Scene,Meshes,RoadSolids,CollisionSolids,GeometryReceipt)) return 1;
        TMap<FString,UStaticMesh*> RoadMeshes;
        for (auto& Pair:RoadSolids)
        {
            TArray<FVector> Vertices; TArray<FIntVector> Triangles; TMap<int32,int32> VertexIDs;
            for (int32 Id:Pair.Value.VertexIndicesItr()) { VertexIDs.Add(Id,Vertices.Num()); Vertices.Add(Pair.Value.GetVertex(Id)); }
            for (int32 Id:Pair.Value.TriangleIndicesItr()) { auto T=Pair.Value.GetTriangle(Id); Triangles.Add(FIntVector(VertexIDs[T.A],VertexIDs[T.B],VertexIDs[T.C])); }
            TArray<FVector> ColliderVertices; TArray<FIntVector> ColliderTriangles; VertexIDs.Reset(); auto& Collider=CollisionSolids[Pair.Key];
            for (int32 Id:Collider.VertexIndicesItr()) { VertexIDs.Add(Id,ColliderVertices.Num()); ColliderVertices.Add(Collider.GetVertex(Id)); }
            for (int32 Id:Collider.TriangleIndicesItr()) { auto T=Collider.GetTriangle(Id); ColliderTriangles.Add(FIntVector(VertexIDs[T.A],VertexIDs[T.B],VertexIDs[T.C])); }
            auto* CollisionMesh=Mesh(Root,Pair.Key+TEXT("_collision"),ColliderVertices,ColliderTriangles); if (!CollisionMesh) return 1;
            auto* Result=Mesh(Root,Pair.Key+TEXT("_joined"),Vertices,Triangles,CollisionMesh); if (!Result) return 1; RoadMeshes.Add(Pair.Key,Result);
        }
        WriteJson(Dir/TEXT("unreal_geometry_report.json"),GeometryReceipt);
        TArray<AActor*> Old;
        for (TActorIterator<AActor> It(W);It;++It) if (It->GetLevel()==W->PersistentLevel && It->ActorHasTag(Owner)) Old.Add(*It);
        int32 ManualBefore=0; for (TActorIterator<AActor> It(W);It;++It) if (!It->ActorHasTag(Owner)) ManualBefore++;
        for (auto* A:Old) W->DestroyActor(A);
        // Delete only tagged actors in this loaded target map. Never touch untagged manual content.
        auto* Landscape=W->SpawnActor<ALandscape>(); Own(Landscape,TEXT("designed_landscape"),TEXT("Terrain"));
        FVector Offset=V(Meta,TEXT("actor_offset_cm")); const auto& XY=Meta->GetArrayField(TEXT("xy_scale_cm"));
        Landscape->SetActorLocation(Offset); Landscape->SetActorScale3D(FVector(XY[0]->AsNumber(),XY[1]->AsNumber(),Meta->GetNumberField(TEXT("z_scale"))));
        Landscape->LandscapeMaterial=GroundMaterial;
        TArray<uint16> Heights; Heights.SetNumUninitialized(N*N);
        for (int32 I=0;I<Heights.Num();I++) Heights[I]=uint16(Raw[2*I])|(uint16(Raw[2*I+1])<<8);
        TMap<FGuid,TArray<uint16>> HeightData; HeightData.Add(FGuid(),MoveTemp(Heights));
        TMap<FGuid,TArray<FLandscapeImportLayerInfo>> Layers; Layers.Add(FGuid(),{});
        Landscape->Import(FGuid::NewGuid(),0,0,N-1,N-1,Meta->GetIntegerField(TEXT("num_subsections")),Meta->GetIntegerField(TEXT("subsection_quads")),HeightData,*(Dir/TEXT("terrain.r16")),Layers,ELandscapeImportAlphamapType::Additive,{});
        Landscape->PostEditChange();
        auto Display=Scene->GetObjectField(TEXT("dimensions"));
        for (ULandscapeComponent* C:Landscape->LandscapeComponents) { C->SetDefaultCustomPrimitiveDataFloat(0,Display->GetNumberField(TEXT("GridSizeCm"))); C->SetDefaultCustomPrimitiveDataFloat(1,Display->GetBoolField(TEXT("ShowGrid"))?1:0); C->SetDefaultCustomPrimitiveDataFloat(2,Display->GetNumberField(TEXT("MinorGridSizeCm"))); }
        for (auto Item:Scene->GetArrayField(TEXT("actors")))
        {
            auto A=Item->AsObject(); const FString Id=A->GetStringField(TEXT("id")); const bool Road=A->GetStringField(TEXT("folder"))==TEXT("Roads");
            if (Road && !RoadMeshes.Contains(Id)) continue; // Entirely inside an earlier road solid, proven by mesh difference.
            auto* Actor=W->SpawnActor<AThanhABlockoutMeshActor>();
            Own(Actor,A->GetStringField(TEXT("id")),A->GetStringField(TEXT("folder")));
            auto* C=Actor->GetStaticMeshComponent(); C->SetMobility(EComponentMobility::Static); C->SetStaticMesh(Road?RoadMeshes[Id]:Meshes.FindRef(A->GetStringField(TEXT("mesh"))));
            C->SetMaterial(0,Materials.FindRef(A->GetStringField(TEXT("color"))));
            C->SetCollisionProfileName(A->GetBoolField(TEXT("collision")) ? TEXT("BlockAll") : TEXT("NoCollision"));
            if (!Road)
            {
                Actor->SetActorLocation(V(A,TEXT("position_m"))*100); Actor->SetActorRotation(FRotator(0,A->GetNumberField(TEXT("yaw_deg")),0));
                const FString Key=A->GetStringField(TEXT("mesh")); const FVector Target=V(A,TEXT("size_m"));
                if (Key==TEXT("box") || Key==TEXT("cylinder"))
                { const FBox Bounds=C->GetStaticMesh()->GetBoundingBox(); Actor->SetActorScale3D(Target*100/Bounds.GetSize()); C->SetRelativeLocation(-Bounds.GetCenter()); }
                else Actor->SetActorScale3D(Target);
            }
            C->SetDefaultCustomPrimitiveDataFloat(0,Display->GetNumberField(TEXT("GridSizeCm"))); C->SetDefaultCustomPrimitiveDataFloat(1,Display->GetBoolField(TEXT("ShowGrid"))?1:0); C->SetDefaultCustomPrimitiveDataFloat(2,Display->GetNumberField(TEXT("MinorGridSizeCm")));
            C->SetDefaultCustomPrimitiveDataFloat(9,1);
            if (A->GetStringField(TEXT("mesh"))==TEXT("cylinder"))
            { const FVector Radius=Road?V(A,TEXT("size_m"))*50:C->GetStaticMesh()->GetBoundingBox().GetSize()/2; C->SetDefaultCustomPrimitiveDataFloat(3,1); C->SetDefaultCustomPrimitiveDataFloat(4,Radius.X); C->SetDefaultCustomPrimitiveDataFloat(5,Radius.Y); if (Road) { const FVector RoundOffset=V(A,TEXT("position_m"))*100; for (int32 i=0;i<3;i++) C->SetDefaultCustomPrimitiveDataFloat(i+6,RoundOffset[i]); } }
            if (Id!=TEXT("water_datum"))
            {
                auto* D=Actor->Dimensions.Get(); D->StableId=Id;
                D->Kind=Road?TEXT("route"):TEXT("primitive"); D->TargetSizeM=Road?C->GetStaticMesh()->GetBoundingBox().GetSize()/100:V(A,TEXT("size_m")); D->TargetCenterM=V(A,TEXT("position_m")); D->GeneratedTransform=Actor->GetActorTransform();
                if (!Road && (A->GetStringField(TEXT("mesh"))==TEXT("roof") || A->GetStringField(TEXT("mesh"))==TEXT("pyramid"))) D->TargetCenterM.Z+=D->TargetSizeM.Z/2;
                for (const auto& B:Scene->GetArrayField(TEXT("buildings"))) if (Id.StartsWith(B->AsObject()->GetStringField(TEXT("id"))+TEXT("_"))) { D->BuildingId=B->AsObject()->GetStringField(TEXT("id")); D->AxisActorId=D->BuildingId+TEXT("_body"); break; }
                for (const FString Kind:{FString(TEXT("routes")),FString(TEXT("stairs"))}) for (const auto& R:Scene->GetArrayField(Kind)) if (R->AsObject()->GetStringField(TEXT("id"))==Id)
                { D->RouteWidthCm=R->AsObject()->GetNumberField(TEXT("width_m"))*100; for (auto P:R->AsObject()->GetArrayField(TEXT("points_m"))) D->RoutePointsCm.Add(Vector(P->AsArray())*100); if (Kind==TEXT("stairs")) D->StepCount=FMath::CeilToInt(FMath::Abs(D->RoutePointsCm.Last().Z-D->RoutePointsCm[0].Z)/(R->AsObject()->GetNumberField(TEXT("step_rise_m"))*100)); }
            }
        }
        ThanhAInitializeGroupDimensions(W);
        W->GetWorldSettings()->DefaultGameMode=AThanhAGameMode::StaticClass();
        auto* Start=W->SpawnActor<APlayerStart>(); Own(Start,TEXT("walk_start"),TEXT("Debug"));
        FVector P=V(Scene,TEXT("player_start_m"));
        Start->SetActorLocation(P*100+FVector(0,0,110)); Start->SetActorRotation(FRotator(0,-90,0));
        auto Lighting=Scene->GetObjectField(TEXT("lighting")); FVector SunRotation=V(Lighting,TEXT("sun_rotation_deg"));
        auto* Sun=W->SpawnActor<ADirectionalLight>(); Own(Sun,TEXT("sun"),TEXT("Debug")); Sun->SetActorRotation(FRotator(SunRotation.X,SunRotation.Y,SunRotation.Z));
        Sun->GetLightComponent()->SetMobility(EComponentMobility::Movable); Sun->GetLightComponent()->SetIntensity(Lighting->GetNumberField(TEXT("sun_intensity_lux")));
        Sun->GetComponent()->SetAtmosphereSunLight(true); Sun->GetComponent()->SetAtmosphereSunLightIndex(0);
        auto* Fill=W->SpawnActor<ADirectionalLight>(); Own(Fill,TEXT("blockout_fill"),TEXT("Debug")); Fill->SetActorRotation(FRotator(-35,145,0));
        Fill->GetLightComponent()->SetMobility(EComponentMobility::Movable); Fill->GetLightComponent()->SetIntensity(Lighting->GetNumberField(TEXT("fill_intensity_lux"))); Fill->GetLightComponent()->SetCastShadows(false);
        auto* Atmosphere=W->SpawnActor<ASkyAtmosphere>(); Own(Atmosphere,TEXT("sky_atmosphere"),TEXT("Debug"));
        auto* Sky=W->SpawnActor<ASkyLight>(); Own(Sky,TEXT("sky"),TEXT("Debug")); Sky->GetLightComponent()->SetMobility(EComponentMobility::Movable);
        Sky->GetLightComponent()->SetIntensity(Lighting->GetNumberField(TEXT("sky_intensity"))); Sky->GetLightComponent()->SetRealTimeCaptureEnabled(true);
        Sky->GetLightComponent()->SetLowerHemisphereColor(FLinearColor(.12f,.16f,.2f));
        auto* Fog=W->SpawnActor<AExponentialHeightFog>(); Own(Fog,TEXT("atmosphere_fog"),TEXT("Debug"));
        Fog->GetComponent()->SetFogDensity(Lighting->GetNumberField(TEXT("fog_density"))); Fog->GetComponent()->SetFogInscatteringColor(FLinearColor(.65f,.75f,.85f));
        W->UpdateWorldComponents(false,false); W->SendAllEndOfFrameUpdates(); FlushRenderingCommands();
        Report->SetNumberField(TEXT("replaced_owned_actors"),Old.Num()); Report->SetNumberField(TEXT("manual_actors_before"),ManualBefore);
        Report->SetNumberField(TEXT("generated_scene_actors"),Scene->GetArrayField(TEXT("actors")).Num());
        const FString SaveFile=bStaging?FPackageName::LongPackageNameToFilename(StagedPath,FPackageName::GetMapPackageExtension()):MapFile;
        if (!FEditorFileUtils::SaveMap(W,SaveFile)) { UE_LOG(LogThanhA,Error,TEXT("Map save failed")); return 1; }
        Report->SetBoolField(TEXT("level_saved"),true);
    }
    // Loaded static mesh render/collision data is compiled asynchronously in Editor worlds.
    FAssetCompilingManager::Get().FinishAllCompilation();
    W->UpdateWorldComponents(false,false); W->SendAllEndOfFrameUpdates(); FlushRenderingCommands();
    auto Collision=CollisionReport(W,Scene); WriteJson(Dir/TEXT("unreal_collision_report.json"),Collision);
    auto Dimensions=ThanhAVerifyDimensions(W); WriteJson(Dir/TEXT("unreal_dimensions_report.json"),Dimensions); Report->SetStringField(TEXT("dimensions_status"),Dimensions->GetStringField(TEXT("status")));
    Report->SetStringField(TEXT("collision_status"),Collision->GetStringField(TEXT("status")));
    int32 Owned=0,Manual=0; TSet<FName> StableIDs; bool Duplicate=false,ManualProbe=false; TArray<FString> Signatures;
    for (TActorIterator<AActor> It(W);It;++It)
    {
        if (It->ActorHasTag(Owner))
        {
            Owned++; for (FName Tag:It->Tags) if (Tag.ToString().StartsWith(TEXT("ThanhA.Id.")))
            { Duplicate |= StableIDs.Contains(Tag); StableIDs.Add(Tag); Signatures.Add(Tag.ToString()+It->GetActorTransform().ToString()); }
        }
        else { Manual++; ManualProbe |= It->ActorHasTag(TEXT("ThanhA.Test.ManualProbe")); }
    }
    Report->SetNumberField(TEXT("owned_actor_count"),Owned); Report->SetNumberField(TEXT("manual_actor_count"),Manual); Report->SetBoolField(TEXT("duplicate_ids"),Duplicate);
    Signatures.Sort(); Report->SetStringField(TEXT("actor_transform_md5"),FMD5::HashAnsiString(*FString::Join(Signatures,TEXT("\n"))));
    Report->SetBoolField(TEXT("manual_probe_present"),ManualProbe);
    int32 LandscapeCount=0,LandscapeComponents=0,AtmosphereCount=0;
    for (TActorIterator<ALandscape> It(W);It;++It) if (It->ActorHasTag(Owner)) { LandscapeCount++; LandscapeComponents+=It->LandscapeComponents.Num(); }
    for (TActorIterator<ASkyAtmosphere> It(W);It;++It) if (It->ActorHasTag(Owner)) AtmosphereCount++;
    Report->SetNumberField(TEXT("landscape_actor_count"),LandscapeCount); Report->SetNumberField(TEXT("landscape_component_count"),LandscapeComponents);
    Report->SetNumberField(TEXT("sky_atmosphere_actor_count"),AtmosphereCount);
    bool CapturesOk=true;
    if (Mode==TEXT("capture"))
    {
        if (bDetails)
        {
            const auto Views=ReadJson(FPaths::ProjectDir()/TEXT("Design/Blockout/detail_capture_views.json"));
            if (!Views || Views->GetIntegerField(TEXT("version"))!=1 || Views->GetStringField(TEXT("units"))!=TEXT("m")) return 1;
            Scene->SetArrayField(TEXT("capture_views"),Views->GetArrayField(TEXT("capture_views")));
        }
        CapturesOk=Capture(W,Scene,Dir); Report->SetBoolField(TEXT("captures_saved"),CapturesOk); Report->SetArrayField(TEXT("capture_views"),Scene->GetArrayField(TEXT("capture_views")));
    }
    Report->SetStringField(TEXT("mode"),Mode);
    const FString ReportFile=Mode==TEXT("generate") ? TEXT("unreal_execution_report.json") : Mode==TEXT("capture") ? (bDetails?TEXT("unreal_detail_capture_report.json"):TEXT("unreal_capture_report.json")) : TEXT("unreal_verification_report.json");
    WriteJson(Dir/(bStaging?TEXT("staged_")+ReportFile:ReportFile),Report);
    UE_LOG(LogThanhA,Display,TEXT("Map %s: %d owned actors; duplicate IDs=%d; collision=%s"),*LevelPath,Owned,Duplicate,*Collision->GetStringField(TEXT("status")));
    return Duplicate || !CapturesOk || Collision->GetStringField(TEXT("status"))!=TEXT("pass") || Dimensions->GetStringField(TEXT("status"))!=TEXT("pass") ? 2 : 0;
}
