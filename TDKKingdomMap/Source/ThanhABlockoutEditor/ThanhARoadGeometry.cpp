#include "ThanhARoadGeometry.h"
#include "Engine/StaticMesh.h"
#include "StaticMeshAttributes.h"
#include "AssetCompilingManager.h"

namespace
{
using namespace UE::Geometry;
FVector Vec(const TArray<TSharedPtr<FJsonValue>>& V) { return FVector(V[0]->AsNumber(),V[1]->AsNumber(),V[2]->AsNumber()); }
bool SourceMesh(const TSharedPtr<FJsonObject>& A,const TMap<FString,UStaticMesh*>& Sources,FDynamicMesh3& Out)
{
    const FString Key=A->GetStringField(TEXT("mesh")); UStaticMesh* M=Sources.FindRef(Key); if (!M) return false;
    FAssetCompilingManager::Get().FinishAllCompilation(); const FMeshDescription* D=M->GetMeshDescription(0); if (!D) return false;
    FStaticMeshConstAttributes Attr(*D); auto Positions=Attr.GetVertexPositions(); const FVector Size=Vec(A->GetArrayField(TEXT("size_m")));
    FVector Scale=Size,Offset=FVector::ZeroVector;
    if (Key==TEXT("box") || Key==TEXT("cylinder")) { const FBox B=M->GetBoundingBox(); Scale=Size*100/B.GetSize(); Offset=-B.GetCenter(); }
    const FTransform Transform(FRotator(0,A->GetNumberField(TEXT("yaw_deg")),0),Vec(A->GetArrayField(TEXT("position_m"))),Scale);
    TMap<FVertexID,int32> IDs; TMap<FString,int32> Shared;
    for (FVertexID V:D->Vertices().GetElementIDs())
    {
        FVector P=Transform.TransformPosition((FVector(Positions[V])+Offset)/100); FString K=FString::Printf(TEXT("%.7f,%.7f,%.7f"),P.X,P.Y,P.Z);
        int32* Existing=Shared.Find(K); int32 Index=Existing?*Existing:Out.AppendVertex(P); Shared.Add(K,Index); IDs.Add(V,Index);
    }
    for (FTriangleID T:D->Triangles().GetElementIDs()) { const auto Vs=D->GetTriangleVertices(T); if (IDs[Vs[0]]==IDs[Vs[1]] || IDs[Vs[1]]==IDs[Vs[2]] || IDs[Vs[2]]==IDs[Vs[0]]) continue; if (Out.AppendTriangle(IDs[Vs[0]],IDs[Vs[2]],IDs[Vs[1]])<0) return false; }
    return Out.IsClosed();
}
struct FFace { FVector P[3],Normal; FBox Bounds; };
FFace Face(const FDynamicMesh3& M,int32 Triangle)
{
    FFace F; auto T=M.GetTriangle(Triangle); F.P[0]=M.GetVertex(T.A); F.P[1]=M.GetVertex(T.B); F.P[2]=M.GetVertex(T.C);
    F.Normal=FVector::CrossProduct(F.P[1]-F.P[0],F.P[2]-F.P[0]).GetSafeNormal(); F.Bounds=FBox(ForceInit); for (auto P:F.P) F.Bounds+=P; return F;
}
TArray<FVector> Half(const TArray<FVector>& Polygon,FVector Origin,FVector Normal,bool Inside,double Tolerance)
{
    TArray<FVector> Out; if (!Polygon.Num()) return Out;
    for (int32 i=0;i<Polygon.Num();i++)
    {
        const FVector A=Polygon[i],B=Polygon[(i+1)%Polygon.Num()]; const double DA=FVector::DotProduct(A-Origin,Normal),DB=FVector::DotProduct(B-Origin,Normal);
        const bool KeepA=Inside?DA<=Tolerance:DA>=-Tolerance,KeepB=Inside?DB<=Tolerance:DB>=-Tolerance;
        if (KeepA) Out.Add(A);
        if (KeepA!=KeepB && FMath::Abs(DA-DB)>1e-14) Out.Add(A+(B-A)*FMath::Clamp(DA/(DA-DB),0.,1.));
    }
    return Out;
}
double Area(const TArray<FVector>& P)
{
    double A=0; for (int32 i=1;i+1<P.Num();i++) A+=FVector::CrossProduct(P[i]-P[0],P[i+1]-P[0]).Size()/2; return A;
}
TArray<TArray<FVector>> Subtract(const TArray<FVector>& Polygon,const FFace& Cut,double Tolerance)
{
    TArray<TArray<FVector>> Outside; auto Inside=Polygon;
    for (int32 i=0;i<3 && Inside.Num()>=3;i++)
    {
        FVector Normal=FVector::CrossProduct(Cut.P[(i+1)%3]-Cut.P[i],Cut.Normal).GetSafeNormal();
        auto Part=Half(Inside,Cut.P[i],Normal,false,Tolerance); if (Part.Num()>=3 && Area(Part)>1e-10) Outside.Add(MoveTemp(Part));
        Inside=Half(Inside,Cut.P[i],Normal,true,Tolerance);
    }
    return Outside;
}
}
bool ThanhATrimRoadSurfaces(const TSharedPtr<FJsonObject>& Scene,const TMap<FString,UStaticMesh*>& Sources,
    TMap<FString,UE::Geometry::FDynamicMesh3>& Results,TMap<FString,UE::Geometry::FDynamicMesh3>& CollisionSources,TSharedRef<FJsonObject> Receipt)
{
    using namespace UE::Geometry;
    TArray<FFace> Earlier; TArray<TSharedPtr<FJsonValue>> Parts; int32 Count=0,Cuts=0;
    const double Tolerance=Scene->GetObjectField(TEXT("geometry"))->GetNumberField(TEXT("surface_tolerance_m"));
    for (auto Value:Scene->GetArrayField(TEXT("actors")))
    {
        auto A=Value->AsObject(); if (A->GetStringField(TEXT("folder"))!=TEXT("Roads")) continue;
        const FString Id=A->GetStringField(TEXT("id")); FDynamicMesh3 Original,Visible; if (!SourceMesh(A,Sources,Original)) { UE_LOG(LogTemp,Error,TEXT("Road source is not a closed solid: %s"),*Id); return false; }
        double RemovedArea=0; TArray<FFace> Added;
        for (int32 T:Original.TriangleIndicesItr())
        {
            auto F=Face(Original,T); TArray<TArray<FVector>> Pieces={{F.P[0],F.P[1],F.P[2]}};
            const double Before=Area(Pieces[0]);
            for (const auto& E:Earlier)
            {
                // Bounds only prune candidates; deletion requires matching planes/normals
                // and exact polygon intersection. Internal solid intersections are allowed.
                if (!F.Bounds.ExpandBy(Tolerance).Intersect(E.Bounds) || FVector::DotProduct(F.Normal,E.Normal)<.9999999) continue;
                if (FMath::Abs(FVector::DotProduct(F.P[0]-E.P[0],E.Normal))>Tolerance || FMath::Abs(FVector::DotProduct(F.P[1]-E.P[0],E.Normal))>Tolerance || FMath::Abs(FVector::DotProduct(F.P[2]-E.P[0],E.Normal))>Tolerance) continue;
                TArray<TArray<FVector>> Next; for (const auto& P:Pieces) Next.Append(Subtract(P,E,Tolerance)); Pieces=MoveTemp(Next); if (!Pieces.Num()) break;
            }
            double After=0;
            for (const auto& P:Pieces)
            {
                After+=Area(P); const int32 Base=Visible.VertexCount(); for (auto V:P) Visible.AppendVertex(V);
                for (int32 i=1;i+1<P.Num();i++) if (FVector::CrossProduct(P[i]-P[0],P[i+1]-P[0]).SizeSquared()>1e-18) Visible.AppendTriangle(Base,Base+i,Base+i+1);
            }
            if (Before-After>1e-8) { RemovedArea+=Before-After; Cuts++; }
            Added.Add(F);
        }
        Earlier.Append(Added); CollisionSources.Add(Id,Original);
        if (Visible.TriangleCount()>0) Results.Add(Id,MoveTemp(Visible));
        auto P=MakeShared<FJsonObject>(); P->SetStringField(TEXT("id"),Id); P->SetBoolField(TEXT("collision_source_closed"),true); P->SetNumberField(TEXT("removed_coplanar_area_m2"),RemovedArea); P->SetNumberField(TEXT("source_triangles"),Original.TriangleCount()); Parts.Add(MakeShared<FJsonValueObject>(P)); Count++;
    }
    Receipt->SetStringField(TEXT("execution"),TEXT("Exact coplanar face polygon subtraction; original watertight collision preserved in separate complex collision assets. Internal solid intersections remain intentional."));
    Receipt->SetArrayField(TEXT("road_solids"),Parts); Receipt->SetNumberField(TEXT("source_road_solids"),Count); Receipt->SetNumberField(TEXT("faces_clipped"),Cuts); Receipt->SetBoolField(TEXT("all_collision_sources_closed"),true);
    return true;
}
