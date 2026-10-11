#include "ThanhADimensionTools.h"
#include "ThanhADimensionComponent.h"
#include "ThanhABlockoutSettings.h"
#include "EngineUtils.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "StaticMeshAttributes.h"
#include "Editor.h"
#include "Engine/Selection.h"
#include "Debug/DebugDrawService.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "CanvasItem.h"
#include "SceneView.h"
#include "Modules/ModuleManager.h"
#include "Containers/Ticker.h"
#include "LandscapeComponent.h"

namespace
{
TArray<FVector> Points(AActor* A)
{
    TArray<FVector> Result;
    TInlineComponentArray<UStaticMeshComponent*> Components(A);
    for (auto* C:Components) if (UStaticMesh* Mesh=C->GetStaticMesh())
    {
        // Only actual static-mesh geometry; never text, arrows, icons or debug components.
        if (const FMeshDescription* D=Mesh->GetMeshDescription(0))
        {
            auto V=FStaticMeshConstAttributes(*D).GetVertexPositions();
            for (FVertexID Id:D->Vertices().GetElementIDs()) Result.Add(C->GetComponentTransform().TransformPosition(FVector(V[Id])));
        }
    }
    return Result;
}
FString Id(AActor* A)
{
    for (FName Tag:A->Tags) if (Tag.ToString().StartsWith(TEXT("ThanhA.Id."))) return Tag.ToString().RightChop(10);
    return {};
}
}
FThanhAMeasurement ThanhAMeasure(const UThanhADimensionComponent* M,bool Whole)
{
    FThanhAMeasurement R; if (!M || !M->GetOwner()) return R;
    AActor* Owner=M->GetOwner(); UWorld* W=Owner->GetWorld(); TArray<AActor*> Members; FQuat Frame=Owner->GetActorQuat();
    const bool Building=Whole && !M->BuildingId.IsEmpty();
    if (Building)
    {
        for (TActorIterator<AActor> It(W);It;++It) if (const auto* Other=It->FindComponentByClass<UThanhADimensionComponent>())
        {
            if (Other->BuildingId==M->BuildingId) Members.Add(*It);
            if (Other->StableId==M->AxisActorId) Frame=It->GetActorQuat();
        }
    }
    else Members.Add(Owner);
    FBox DesignBox(ForceInit),WorldBox(ForceInit);
    for (AActor* A:Members) for (FVector P:Points(A)) { DesignBox+=Frame.UnrotateVector(P); WorldBox+=P; }
    if (!DesignBox.IsValid || !WorldBox.IsValid) return R;
    R.Valid=true; R.SizeM=DesignBox.GetSize()/100; R.CenterCm=WorldBox.GetCenter(); R.BaseM=WorldBox.Min.Z/100; R.TopM=WorldBox.Max.Z/100;
    R.Scope=Building ? TEXT("Whole building (all mesh parts, including roof)") : TEXT("Selected mesh component");
    const FVector Target=Building?M->GroupTargetSizeM:M->TargetSizeM;
    R.Changed=Target.GetMax()>0 && (R.SizeM-Target).GetAbsMax()>.025;
    if (!Building && M->RoutePointsCm.Num()>1)
    {
        const FTransform Delta=Owner->GetActorTransform()*M->GeneratedTransform.Inverse();
        double MaxSlope=0,TotalRun=0,TotalRise=0,Width=0;
        for (int32 i=0;i+1<M->RoutePointsCm.Num();i++)
        {
            FVector A=Delta.TransformPosition(M->RoutePointsCm[i]),B=Delta.TransformPosition(M->RoutePointsCm[i+1]);
            const double Run=FVector::Dist2D(A,B)/100,Rise=FMath::Abs(B.Z-A.Z)/100; TotalRun+=Run; TotalRise+=Rise;
            MaxSlope=FMath::Max(MaxSlope,FMath::RadiansToDegrees(FMath::Atan2(Rise,Run)));
            const FVector Dir=(M->RoutePointsCm[i+1]-M->RoutePointsCm[i]).GetSafeNormal2D();
            Width=FMath::Max(Width,Delta.TransformVector(FVector(-Dir.Y,Dir.X,0)*M->RouteWidthCm).Size()/100);
        }
        if (M->StepCount>0) R.Detail=FString::Printf(TEXT("Stair: W %.2fm | Rise %.2fm | %d steps | Riser %.3fm | Tread %.3fm"),Width,TotalRise,M->StepCount,TotalRise/M->StepCount,TotalRun/M->StepCount);
        else R.Detail=FString::Printf(TEXT("Route: W %.2fm | Rise %.2fm | Horizontal run %.2fm | Max slope %.2f deg"),Width,TotalRise,TotalRun,MaxSlope);
        R.Scope+=TEXT("; route profile transformed with geometry");
    }
    return R;
}
FString ThanhADimensionLabel(const UThanhADimensionComponent* M,const FThanhAMeasurement& R,bool Elevation)
{
    FString Text=(M->MeasureWholeBuilding && !M->BuildingId.IsEmpty()?M->BuildingId:M->StableId)+TEXT("\n")+R.Scope;
    Text+=FString::Printf(TEXT("\nL: %.2f m | W: %.2f m | H: %.2f m"),R.SizeM.X,R.SizeM.Y,R.SizeM.Z);
    if (Elevation) Text+=FString::Printf(TEXT("\nBase Z: %+.2f m | Top Z: %+.2f m"),R.BaseM,R.TopM);
    if (!R.Detail.IsEmpty()) Text+=TEXT("\n")+R.Detail;
    if (R.Changed) Text+=TEXT("\nCHANGED from generated design dimensions");
    return Text;
}
void ThanhAInitializeGroupDimensions(UWorld* W)
{
    TMap<FString,FVector> Sizes;
    for (TActorIterator<AActor> It(W);It;++It) if (auto* M=It->FindComponentByClass<UThanhADimensionComponent>()) if (!M->BuildingId.IsEmpty())
    { if (!Sizes.Contains(M->BuildingId)) Sizes.Add(M->BuildingId,ThanhAMeasure(M,true).SizeM); M->GroupTargetSizeM=Sizes[M->BuildingId]; }
}
TSharedRef<FJsonObject> ThanhAVerifyDimensions(UWorld* W)
{
    auto Report=MakeShared<FJsonObject>(); TArray<TSharedPtr<FJsonValue>> Errors,Examples; int32 Count=0;
    for (TActorIterator<AActor> It(W);It;++It) if (auto* M=It->FindComponentByClass<UThanhADimensionComponent>())
    {
        if (M->StableId.IsEmpty()) continue;
        auto R=ThanhAMeasure(M,false); Count++;
        if (!R.Valid) Errors.Add(MakeShared<FJsonValueString>(M->StableId+TEXT(" has no source mesh geometry")));
        if (R.Changed && M->Kind==TEXT("primitive")) Errors.Add(MakeShared<FJsonValueString>(M->StableId+TEXT(" dimensions do not match target")));
        if (R.Valid && M->Kind==TEXT("primitive") && !R.CenterCm.Equals(M->TargetCenterM*100,2)) Errors.Add(MakeShared<FJsonValueString>(M->StableId+TEXT(" pivot fitting moved the design centre")));
        if (M->StableId==TEXT("council_hall_body") || M->StableId.StartsWith(TEXT("calibration_")))
        { auto J=MakeShared<FJsonObject>(); J->SetStringField(TEXT("id"),M->StableId); J->SetStringField(TEXT("label"),ThanhADimensionLabel(M,ThanhAMeasure(M,M->MeasureWholeBuilding),true)); Examples.Add(MakeShared<FJsonValueObject>(J)); }
    }
    if (!Count) Errors.Add(MakeShared<FJsonValueString>(TEXT("No persisted dimension metadata found after loading the world")));
    Report->SetStringField(TEXT("status"),Errors.Num()?TEXT("fail"):TEXT("pass")); Report->SetNumberField(TEXT("measured_components"),Count); Report->SetArrayField(TEXT("errors"),Errors); Report->SetArrayField(TEXT("examples"),Examples);
    return Report;
}

class FThanhABlockoutEditorModule : public IModuleInterface
{
    FDelegateHandle DrawHandle; FTSTicker::FDelegateHandle TickHandle;
    TWeakObjectPtr<UWorld> GridWorld; FVector LastGrid=FVector(-1);
    void Draw(UCanvas* Canvas,APlayerController*)
    {
        if (!GEditor || !Canvas || !Canvas->SceneView || GEditor->PlayWorld) return;
        const auto* Settings=GetDefault<UThanhABlockoutSettings>(); if (!Settings->ShowDimensions) return;
        UWorld* W=GEditor->GetEditorWorldContext().World(); if (!W) return;
        TArray<AActor*> Actors;
        if (Settings->SelectedOnly) { for (FSelectionIterator It(*GEditor->GetSelectedActors());It;++It) if (AActor* A=Cast<AActor>(*It)) Actors.Add(A); }
        else for (TActorIterator<AActor> It(W);It;++It) if (It->FindComponentByClass<UThanhADimensionComponent>()) Actors.Add(*It);
        TSet<FString> Drawn; int32 Labels=0;
        for (AActor* A:Actors) if (const auto* M=A->FindComponentByClass<UThanhADimensionComponent>())
        {
            if (M->StableId.IsEmpty()) continue;
            const FString Key=M->MeasureWholeBuilding&&!M->BuildingId.IsEmpty()?M->BuildingId:M->StableId;
            if (Drawn.Contains(Key)) continue; Drawn.Add(Key);
            const auto Measure=ThanhAMeasure(M,M->MeasureWholeBuilding); if (!Measure.Valid) continue;
            FVector P=Canvas->Project(Measure.CenterCm+FVector(0,0,(Measure.TopM-Measure.BaseM)*50+60));
            if (P.Z<=0 || P.X<0 || P.X>Canvas->SizeX || P.Y<0 || P.Y>Canvas->SizeY) continue;
            FCanvasTextItem Text(FVector2D(P.X,P.Y),FText::FromString(ThanhADimensionLabel(M,Measure,Settings->ShowElevation)),GEngine->GetSmallFont(),Measure.Changed?FLinearColor::Yellow:FLinearColor::White);
            Text.EnableShadow(FLinearColor::Black); Canvas->Canvas->DrawItem(Text);
            if (++Labels>=Settings->MaxLabels) break;
        }
    }
    bool UpdateGrid(float)
    {
        if (!GEditor || GEditor->PlayWorld) return true; UWorld* W=GEditor->GetEditorWorldContext().World(); if (!W) return true;
        const auto* S=GetDefault<UThanhABlockoutSettings>(); const FVector Values(S->GridSizeCm,S->ShowGrid?1:0,S->MinorGridSizeCm);
        if (GridWorld==W && LastGrid==Values) return true; GridWorld=W; LastGrid=Values;
        for (TActorIterator<AActor> It(W);It;++It) if (It->ActorHasTag(TEXT("ThanhA.Blockout.v1")))
        { TInlineComponentArray<UPrimitiveComponent*> Components(*It); for (auto* C:Components) if (C->IsA<UStaticMeshComponent>() || C->IsA<ULandscapeComponent>()) for (int32 i=0;i<3;i++) C->SetCustomPrimitiveDataFloat(i,Values[i]); }
        return true;
    }
public:
    virtual void StartupModule() override
    {
        if (IsRunningCommandlet()) return;
        DrawHandle=UDebugDrawService::Register(TEXT("OnScreenDebug"),FDebugDrawDelegate::CreateRaw(this,&FThanhABlockoutEditorModule::Draw));
        TickHandle=FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this,&FThanhABlockoutEditorModule::UpdateGrid),.25f);
    }
    virtual void ShutdownModule() override { if (DrawHandle.IsValid()) UDebugDrawService::Unregister(DrawHandle); if (TickHandle.IsValid()) FTSTicker::GetCoreTicker().RemoveTicker(TickHandle); }
};
IMPLEMENT_MODULE(FThanhABlockoutEditorModule,ThanhABlockoutEditor);
