#include "ThanhAMetricGrid.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionWorldPosition.h"
#include "Materials/MaterialExpressionActorPositionWS.h"
#include "Materials/MaterialExpressionVertexNormalWS.h"
#include "Materials/MaterialExpressionTransform.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "Materials/MaterialFunction.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/PackageName.h"
#include "UObject/MetaData.h"
#include "UObject/SavePackage.h"
#include "Dom/JsonObject.h"

namespace
{
bool Save(UObject* A)
{
    A->GetOutermost()->GetMetaData().SetValue(A,TEXT("ThanhAOwner"),TEXT("ThanhA.Blockout.v1"));
    FSavePackageArgs Args; Args.TopLevelFlags=RF_Public|RF_Standalone; Args.SaveFlags=SAVE_NoError;
    A->MarkPackageDirty(); FAssetRegistryModule::AssetCreated(A);
    return UPackage::SavePackage(A->GetOutermost(),A,*FPackageName::LongPackageNameToFilename(A->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension()),Args);
}
template<class T> T* Node(UMaterial* M) { auto* N=NewObject<T>(M); M->GetExpressionCollection().AddExpression(N); return N; }
}
UMaterialInterface* ThanhAMetricMaterial(const FString& Root,const FString& Hex,const TSharedPtr<FJsonObject>& Scene)
{
    const FString ParentPath=Root/TEXT("M_MetricGrid_v1");
    UMaterial* Parent=nullptr;
    if (FPackageName::DoesPackageExist(ParentPath)) Parent=LoadObject<UMaterial>(nullptr,*(ParentPath+TEXT(".M_MetricGrid_v1")));
    if (!Parent)
    {
        auto* Pack=LoadObject<UMaterial>(nullptr,*Scene->GetObjectField(TEXT("prototyping"))->GetStringField(TEXT("grid_parent"))); if (!Pack) return nullptr;
        Parent=Cast<UMaterial>(StaticDuplicateObject(Pack,CreatePackage(*ParentPath),TEXT("M_MetricGrid_v1"))); Parent->SetFlags(RF_Public|RF_Standalone);
        auto* Grid=Node<UMaterialExpressionCustom>(Parent); Grid->OutputType=CMOT_Float3; Grid->Description=TEXT("Physical centimetres in an orthonormal surface frame; slope distance is intrinsic");
        Grid->Code=TEXT(R"HLSL(
// LocalVertexFactory deliberately removes scale from vertex normal transforms.
// Reconstruct inverse-transpose scaling in the rotation frame, without noisy
// per-pixel derivative normals. Landscape uses its own world normal unchanged.
float3 x=normalize(AxisX), y=normalize(AxisY), z=normalize(cross(x,y));
float3 n=normalize(Normal);
if (ScaleNormals>0.5) n=normalize(x*dot(n,x)/max(Scale.x,0.0001)+y*dot(n,y)/max(Scale.y,0.0001)+z*dot(n,z)/max(Scale.z,0.0001));
float3 t=abs(dot(n,x))<abs(dot(n,y))?x:y;
t=normalize(t-n*dot(t,n)); float3 b=normalize(cross(n,t));
float3 p=Position-Origin; float2 q=float2(dot(p,t),dot(p,b));
if (RoundSurface>0.5 && abs(dot(n,z))<0.6) {
    float3 rp=p-x*RoundX*Scale.x-y*RoundY*Scale.y-z*RoundZ*Scale.z;
    float rx=max(RadiusX*Scale.x,0.01), ry=max(RadiusY*Scale.y,0.01);
    float theta=atan2(dot(rp,y)/ry,dot(rp,x)/rx), arc=theta*rx;
    if (abs(rx-ry)>0.01) {
        arc=0; const int count=32; float delta=theta/count;
        [unroll] for (int i=0;i<count;i++) { float a=(i+0.5)*delta; arc+=sqrt(rx*rx*sin(a)*sin(a)+ry*ry*cos(a)*cos(a))*delta; }
    }
    q=float2(arc,dot(rp,z));
}
float major=MajorCm>0?MajorCm:100, minor=MinorCm>0?MinorCm:10;
float visible=MajorCm>0?GridVisible:1;
float2 footprint=max(fwidth(q),float2(0.02,0.02));
float2 dMajor=abs(frac(q/major+0.5)-0.5)*major;
float2 dMinor=abs(frac(q/minor+0.5)-0.5)*minor;
float majorMask=max(1-smoothstep(0.8,0.8+footprint.x,dMajor.x),1-smoothstep(0.8,0.8+footprint.y,dMajor.y));
float minorMask=max(1-smoothstep(0.15,0.15+footprint.x,dMinor.x),1-smoothstep(0.15,0.15+footprint.y,dMinor.y));
float pixel=max(footprint.x,footprint.y);
majorMask*=saturate(1-pixel/(major*0.7)); minorMask*=saturate(1-pixel/(minor*0.5));
return lerp(Tint,float3(0.045,0.055,0.065),saturate(visible)*max(majorMask*0.85,minorMask*0.38));
)HLSL");
        auto Connect=[&](FName Name,UMaterialExpression* Expression) { FCustomInput I; I.InputName=Name; I.Input.Connect(0,Expression); Grid->Inputs.Add(I); };
        Connect(TEXT("Position"),Node<UMaterialExpressionWorldPosition>(Parent)); Connect(TEXT("Origin"),Node<UMaterialExpressionActorPositionWS>(Parent)); Connect(TEXT("Normal"),Node<UMaterialExpressionVertexNormalWS>(Parent));
        for (const auto& Pair:TArray<TPair<FName,FLinearColor>>{{TEXT("AxisX"),FLinearColor(1,0,0)},{TEXT("AxisY"),FLinearColor(0,1,0)}})
        {
            auto* C=Node<UMaterialExpressionConstant3Vector>(Parent); C->Constant=Pair.Value;
            auto* T=Node<UMaterialExpressionTransform>(Parent); T->TransformSourceType=TRANSFORMSOURCE_Local; T->TransformType=TRANSFORM_World; T->Input.Connect(0,C); Connect(Pair.Key,T);
        }
        auto* Tint=Node<UMaterialExpressionVectorParameter>(Parent); Tint->ParameterName=TEXT("MetricTint"); Tint->DefaultValue=FLinearColor(.5f,.5f,.5f); Connect(TEXT("Tint"),Tint);
        const auto D=Scene->GetObjectField(TEXT("dimensions"));
        const TArray<FName> Names={TEXT("MajorCm"),TEXT("GridVisible"),TEXT("MinorCm"),TEXT("RoundSurface"),TEXT("RadiusX"),TEXT("RadiusY"),TEXT("RoundX"),TEXT("RoundY"),TEXT("RoundZ"),TEXT("ScaleNormals")};
        const TArray<float> Values={float(D->GetNumberField(TEXT("GridSizeCm"))),D->GetBoolField(TEXT("ShowGrid"))?1.f:0.f,float(D->GetNumberField(TEXT("MinorGridSizeCm"))),0,50,50,0,0,0,0};
        for (int32 Index=0;Index<Names.Num();Index++)
        {
            auto* S=Node<UMaterialExpressionScalarParameter>(Parent); S->ParameterName=Names[Index]; S->DefaultValue=Values[Index]; S->bUseCustomPrimitiveData=true; S->PrimitiveDataIndex=Index; Connect(Names[Index],S);
        }
        auto* Scale=Node<UMaterialExpressionMaterialFunctionCall>(Parent);
        auto* ScaleFunction=LoadObject<UMaterialFunction>(nullptr,TEXT("/Engine/Functions/Engine_MaterialFunctions02/WorldPositionOffset/ObjectScale.ObjectScale"));
        if (!ScaleFunction || !Scale->SetMaterialFunction(ScaleFunction)) return nullptr;
        Connect(TEXT("Scale"),Scale);
        Parent->GetEditorOnlyData()->BaseColor.Connect(0,Grid); Parent->PostEditChange(); if (!Save(Parent)) return nullptr;
    }
    const FString Name=TEXT("MI_Metric_")+Hex.RightChop(1); const FString Path=Root/Name;
    if (FPackageName::DoesPackageExist(Path)) return LoadObject<UMaterialInstanceConstant>(nullptr,*(Path+TEXT(".")+Name));
    auto* MI=NewObject<UMaterialInstanceConstant>(CreatePackage(*Path),*Name,RF_Public|RF_Standalone); MI->SetParentEditorOnly(Parent);
    const FLinearColor Style=FLinearColor(FColor::FromHex(Hex)); MI->SetVectorParameterValueEditorOnly(TEXT("MetricTint"),FMath::Lerp(FLinearColor(.52f,.52f,.52f),Style,.35f));
    MI->PostEditChange(); return Save(MI)?MI:nullptr;
}
