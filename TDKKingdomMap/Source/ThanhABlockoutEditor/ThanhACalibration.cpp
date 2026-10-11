#include "ThanhACalibration.h"
#include "ThanhAMetricGrid.h"
#include "ThanhADimensionComponent.h"
#include "ThanhABlockoutMeshActor.h"
#include "ThanhADimensionTools.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Engine/DirectionalLight.h"
#include "Components/StaticMeshComponent.h"
#include "Components/LightComponent.h"
#include "StaticMeshAttributes.h"
#include "EngineUtils.h"
#include "Editor.h"
#include "FileHelpers.h"
#include "AssetCompilingManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/PackageName.h"
#include "Serialization/JsonSerializer.h"

namespace
{
FVector Vec(const TSharedPtr<FJsonObject>& J,const TCHAR* Key) { const auto& A=J->GetArrayField(Key); return FVector(A[0]->AsNumber(),A[1]->AsNumber(),A[2]->AsNumber()); }
TArray<TSharedPtr<FJsonValue>> Arr(FVector P) { return {MakeShared<FJsonValueNumber>(P.X),MakeShared<FJsonValueNumber>(P.Y),MakeShared<FJsonValueNumber>(P.Z)}; }
void Tag(AActor* A,const FString& Id) { A->Tags.Add(TEXT("ThanhA.Blockout.v1")); A->Tags.Add(FName(*(TEXT("ThanhA.Id.")+Id))); A->SetActorLabel(Id); A->SetFolderPath(TEXT("Debug/Calibration")); }
}
int32 ThanhACalibration(const TSharedPtr<FJsonObject>& Main,const FString& Dir,bool CaptureOnly,
    TFunctionRef<bool(UWorld*,const TSharedPtr<FJsonObject>&,const FString&)> Capture)
{
    FString Text; TSharedPtr<FJsonObject> Scene;
    if (!FFileHelper::LoadFileToString(Text,*(FPaths::ProjectDir()/TEXT("Design/Blockout/calibration.json"))) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Scene)) return 1;
    const FString Path=TEXT("/Game/Maps/Blockout/L_ThanhA_GridCalibration"); if (Scene->GetStringField(TEXT("level"))!=Path) return 1;
    Scene->SetObjectField(TEXT("lighting"),Main->GetObjectField(TEXT("lighting")));
    const FString Map=FPackageName::LongPackageNameToFilename(Path,FPackageName::GetMapPackageExtension());
    UWorld* W=nullptr;
    if (FPaths::FileExists(Map)) { if (!FEditorFileUtils::LoadMap(Map,false,false)) return 1; W=GEditor->GetEditorWorldContext().World(); }
    else if (!CaptureOnly) W=GEditor->NewMap(false);
    if (!W) return 1;
    if (!CaptureOnly)
    {
        TArray<AActor*> Old; for (TActorIterator<AActor> It(W);It;++It) if (It->ActorHasTag(TEXT("ThanhA.Blockout.v1"))) Old.Add(*It); for (auto* A:Old) W->DestroyActor(A);
        const FString Root=TEXT("/Game/Blockout/ThanhA/Generated/")+Main->GetStringField(TEXT("fingerprint")); auto Kit=Main->GetObjectField(TEXT("prototyping"));
        auto* Mat=ThanhAMetricMaterial(Root,TEXT("#96a37b"),Main); if (!Mat) return 1;
        for (auto V:Scene->GetArrayField(TEXT("actors")))
        {
            auto A=V->AsObject(); const FString Key=A->GetStringField(TEXT("mesh")),Id=A->GetStringField(TEXT("id"));
            FString Asset=Key==TEXT("roof")?Root/TEXT("SM_roof.SM_roof"):Kit->GetStringField(Key); auto* Mesh=LoadObject<UStaticMesh>(nullptr,*Asset); if (!Mesh) return 1;
            FAssetCompilingManager::Get().FinishAllCompilation(); auto* Actor=W->SpawnActor<AThanhABlockoutMeshActor>(); Tag(Actor,Id); auto* C=Actor->GetStaticMeshComponent(); C->SetStaticMesh(Mesh); C->SetMaterial(0,Mat); C->SetCollisionProfileName(TEXT("BlockAll"));
            const FBox B=Mesh->GetBoundingBox(); const FVector Size=Vec(A,TEXT("size_m")),Center=Vec(A,TEXT("center_m"));
            Actor->SetActorLocation(Center*100); Actor->SetActorRotation(FRotator(0,A->GetNumberField(TEXT("yaw_deg")),0)); Actor->SetActorScale3D(Size*100/B.GetSize()); C->SetRelativeLocation(-B.GetCenter());
            C->SetDefaultCustomPrimitiveDataFloat(0,100); C->SetDefaultCustomPrimitiveDataFloat(1,1); C->SetDefaultCustomPrimitiveDataFloat(2,10);
            C->SetDefaultCustomPrimitiveDataFloat(9,1);
            if (Key==TEXT("cylinder")) { C->SetDefaultCustomPrimitiveDataFloat(3,1); C->SetDefaultCustomPrimitiveDataFloat(4,B.GetSize().X/2); C->SetDefaultCustomPrimitiveDataFloat(5,B.GetSize().Y/2); }
            auto* D=Actor->Dimensions.Get(); D->StableId=Id; D->Kind=TEXT("primitive"); D->TargetSizeM=Size; D->TargetCenterM=Center; D->GeneratedTransform=Actor->GetActorTransform();
            if (A->TryGetStringField(TEXT("building_id"),D->BuildingId)) D->AxisActorId=D->BuildingId+TEXT("_body");
        }
        auto* Sun=W->SpawnActor<ADirectionalLight>(); Tag(Sun,TEXT("calibration_sun")); Sun->SetActorRotation(FRotator(-45,-30,0)); Sun->GetLightComponent()->SetMobility(EComponentMobility::Movable); Sun->GetLightComponent()->SetIntensity(50000);
        W->UpdateWorldComponents(false,false); ThanhAInitializeGroupDimensions(W); if (!FEditorFileUtils::SaveMap(W,Map)) return 1;
    }
    FAssetCompilingManager::Get().FinishAllCompilation(); auto Report=ThanhAVerifyDimensions(W); Report->SetStringField(TEXT("fingerprint"),Main->GetStringField(TEXT("fingerprint"))); Report->SetStringField(TEXT("level"),Path);
    TArray<TSharedPtr<FJsonValue>> Checks;
    for (TActorIterator<AActor> It(W);It;++It) if (auto* D=It->FindComponentByClass<UThanhADimensionComponent>()) if (D->StableId==TEXT("calibration_rotated") || D->StableId==TEXT("calibration_composite_body"))
    {
        const FTransform Before=It->GetActorTransform(); const auto Initial=ThanhAMeasure(D,D->MeasureWholeBuilding); bool Pass=Initial.Valid && !Initial.Changed;
        if (D->StableId==TEXT("calibration_rotated"))
        { It->SetActorRotation(FRotator(0,73,0)); auto Rotated=ThanhAMeasure(D,false); Pass&=Rotated.SizeM.Equals(FVector(2,4,3),.001); It->SetActorScale3D(Before.GetScale3D()*FVector(1.5,1,1)); auto Scaled=ThanhAMeasure(D,false); Pass&=Scaled.SizeM.Equals(FVector(3,4,3),.001) && Scaled.Changed; }
        else Pass&=Initial.SizeM.Equals(FVector(2,2,4),.001) && FMath::Abs(Initial.BaseM)<.001 && FMath::Abs(Initial.TopM-4)<.001;
        It->SetActorTransform(Before); auto J=MakeShared<FJsonObject>(); J->SetStringField(TEXT("id"),D->StableId); J->SetBoolField(TEXT("pass"),Pass); Checks.Add(MakeShared<FJsonValueObject>(J)); if (!Pass) Report->SetStringField(TEXT("status"),TEXT("fail"));
    }
    Report->SetArrayField(TEXT("rotation_scale_composite_checks"),Checks);
    // Face-aligned ramp capture uses its actual transformed inclined triangles, not a guessed slope.
    for (auto V:Scene->GetArrayField(TEXT("capture_views")))
    {
        auto View=V->AsObject(); bool Align=false; if (!View->TryGetBoolField(TEXT("align_to_slope"),Align) || !Align) continue;
        for (TActorIterator<AStaticMeshActor> It(W);It;++It) if (It->GetActorLabel()==View->GetStringField(TEXT("label_id")))
        {
            auto* C=It->GetStaticMeshComponent(); const auto* D=C->GetStaticMesh()->GetMeshDescription(0); auto Positions=FStaticMeshConstAttributes(*D).GetVertexPositions(); FVector Centroid=FVector::ZeroVector,Normal=FVector::ZeroVector; double Weight=0;
            FVector FirstNormal=FVector::ZeroVector;
            for (FTriangleID T:D->Triangles().GetElementIDs())
            {
                auto Vertices=D->GetTriangleVertices(T); const FVector A=C->GetComponentTransform().TransformPosition(FVector(Positions[Vertices[0]])),B=C->GetComponentTransform().TransformPosition(FVector(Positions[Vertices[2]])),P=C->GetComponentTransform().TransformPosition(FVector(Positions[Vertices[1]]));
                const FVector Cross=FVector::CrossProduct(B-A,P-A),N=Cross.GetSafeNormal(); if (N.Z>.1 && N.Z<.999) { if (FirstNormal.IsNearlyZero()) FirstNormal=N; if (FVector::DotProduct(FirstNormal,N)<.9999) continue; const double Area=Cross.Size(); Centroid+=(A+B+P)/3*Area; Normal+=N*Area; Weight+=Area; }
            }
            if (Weight>0) { Centroid/=Weight; Normal.Normalize(); View->SetArrayField(TEXT("target_m"),Arr(Centroid/100)); View->SetArrayField(TEXT("location_m"),Arr((Centroid+Normal*800)/100)); Report->SetNumberField(*(View->GetStringField(TEXT("id"))+TEXT("_actual_slope_degrees")),FMath::RadiansToDegrees(FMath::Acos(Normal.Z))); }
        }
    }
    if (CaptureOnly) Report->SetBoolField(TEXT("captures_saved"),Capture(W,Scene,Dir));
    FJsonSerializer::Serialize(Report,TJsonWriterFactory<>::Create(&Text)); FFileHelper::SaveStringToFile(Text,*(Dir/TEXT("unreal_calibration_report.json")));
    return Report->GetStringField(TEXT("status"))==TEXT("pass") && (!CaptureOnly || Report->GetBoolField(TEXT("captures_saved")))?0:2;
}
