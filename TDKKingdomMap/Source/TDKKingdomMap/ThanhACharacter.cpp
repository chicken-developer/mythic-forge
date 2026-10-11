#include "ThanhACharacter.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonSerializer.h"
#include "HAL/PlatformMisc.h"
#include "HAL/IConsoleManager.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/PlayerController.h"
#include "UnrealClient.h"

AThanhACharacter::AThanhACharacter()
{
    PrimaryActorTick.bCanEverTick = true;
    GetCapsuleComponent()->InitCapsuleSize(42.f, 96.f);
    GetCharacterMovement()->MaxWalkSpeed = 600.f;
    GetCharacterMovement()->MaxStepHeight = 35.f;
    GetCharacterMovement()->SetWalkableFloorAngle(35.f);
    GetCharacterMovement()->JumpZVelocity = 420.f;
    bUseControllerRotationYaw = true;
    UCameraComponent* Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("WalkCamera"));
    Camera->SetupAttachment(GetCapsuleComponent());
    Camera->SetRelativeLocation(FVector(0, 0, 65));
    Camera->bUsePawnControlRotation = true;
}
void AThanhACharacter::SetupPlayerInputComponent(UInputComponent* Input)
{
    Super::SetupPlayerInputComponent(Input);
    Input->BindAxis("MoveForward", this, &AThanhACharacter::Forward);
    Input->BindAxis("MoveRight", this, &AThanhACharacter::Right);
    Input->BindAxis("Turn", this, &AThanhACharacter::Turn);
    Input->BindAxis("LookUp", this, &AThanhACharacter::Look);
    Input->BindAction("Jump", IE_Pressed, this, &ACharacter::Jump);
    Input->BindAction("Jump", IE_Released, this, &ACharacter::StopJumping);
}
void AThanhACharacter::Forward(float Value) { AddMovementInput(FRotationMatrix(FRotator(0, GetControlRotation().Yaw, 0)).GetUnitAxis(EAxis::X), Value); }
void AThanhACharacter::Right(float Value) { AddMovementInput(FRotationMatrix(FRotator(0, GetControlRotation().Yaw, 0)).GetUnitAxis(EAxis::Y), Value); }
void AThanhACharacter::Turn(float Value) { AddControllerYawInput(Value); }
void AThanhACharacter::Look(float Value) { AddControllerPitchInput(Value); }
AThanhAGameMode::AThanhAGameMode() { DefaultPawnClass = AThanhACharacter::StaticClass(); }

void AThanhACharacter::BeginPlay()
{
    Super::BeginPlay();
    if (FParse::Param(FCommandLine::Get(), TEXT("ThanhAEnvironmentBenchmark")))
    { BeginEnvironmentBenchmark(); return; }
    bAutoWalk = FParse::Param(FCommandLine::Get(), TEXT("ThanhAAutoWalk"));
    if (!bAutoWalk) return;
    FString Text; TSharedPtr<FJsonObject> Scene;
    const FString Path = FPaths::ProjectDir()/TEXT("Design/Blockout/Generated/scene.json");
    if (!FFileHelper::LoadFileToString(Text,*Path) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Scene))
    { FinishWalk(false,TEXT("Cannot read prepared scene")); return; }
    WalkFingerprint=Scene->GetStringField(TEXT("fingerprint"));
    FParse::Value(FCommandLine::Get(),TEXT("ThanhAStairWalk="),StairTestId);
    if (!StairTestId.IsEmpty())
    {
        for (auto V:Scene->GetArrayField(TEXT("stairs"))) if (V->AsObject()->GetStringField(TEXT("id"))==StairTestId)
        {
            const auto& Route=V->AsObject()->GetArrayField(TEXT("points_m"));
            auto Point=[](const TSharedPtr<FJsonValue>& J) { const auto& P=J->AsArray(); return FVector(P[0]->AsNumber(),P[1]->AsNumber(),P[2]->AsNumber())*100; };
            const FVector A=Point(Route[0]),B=Point(Route[1]),Direction=(B-A).GetSafeNormal2D();
            // One initial test spawn on the bottom landing; actual movement up and down afterwards.
            SetActorLocation(A-Direction*120+FVector(0,0,GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+10));
            WalkPoints={A,B+Direction*120,B,A-Direction*120};
        }
        if (!WalkPoints.Num()) { FinishWalk(false,TEXT("Unknown stair test ID")); return; }
    }
    else
    for (auto V : Scene->GetArrayField(TEXT("playtest_waypoints_m")))
    {
        const auto& P=V->AsArray(); WalkPoints.Add(FVector(P[0]->AsNumber(),P[1]->AsNumber(),P[2]->AsNumber())*100);
    }
    if (!WalkPoints.Num()) { FinishWalk(false,TEXT("No route")); return; }
    // Real CharacterMovement at deterministic 60 Hz, without teleport or jump along the route.
    FApp::SetFixedDeltaTime(1.0/60.0); FApp::SetUseFixedTimeStep(true);
    PreviousWalkLocation=GetActorLocation();
}
void AThanhACharacter::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (bEnvironmentBenchmark) { TickEnvironmentBenchmark(); return; }
    if (!bAutoWalk) return;
    WalkFrames++;
    if (WalkFrames<30) return; // Allow PlayerStart floor settling.
    FVector P=GetActorLocation();
    if (FVector::Dist2D(P,PreviousWalkLocation)<.5) StuckSeconds+=DeltaSeconds;
    else StuckSeconds=0;
    PreviousWalkLocation=P;
    if (StuckSeconds>10 || WalkFrames>90000)
    { FinishWalk(false,FString::Printf(TEXT("Stopped reaching waypoint %d at %.2f,%.2f,%.2fm"),WalkIndex,P.X/100,P.Y/100,P.Z/100)); return; }
    FVector Target=WalkPoints[WalkIndex]; FVector Delta=Target-P; Delta.Z=0;
    if (Delta.Size2D()<90)
    {
        const float Error=FMath::Abs(P.Z-GetCapsuleComponent()->GetScaledCapsuleHalfHeight()-Target.Z)/100;
        MaxGroundError=FMath::Max(MaxGroundError,Error);
        if (Error>.65) { FinishWalk(false,TEXT("Character ground height differs from route")); return; }
        WalkIndex++;
        if (WalkIndex>=WalkPoints.Num()) { FinishWalk(true,StairTestId.IsEmpty()?TEXT("Main Gate, Market, Plaza, Council forecourt, Waterfront, Residential and Craft reached using CharacterMovement"):TEXT("Stair ascent and descent reached using CharacterMovement")); return; }
        Delta=WalkPoints[WalkIndex]-P; Delta.Z=0;
    }
    AddMovementInput(Delta.GetSafeNormal2D(),1);
}
void AThanhACharacter::FinishWalk(bool Success,const FString& Reason)
{
    bAutoWalk=false;
    auto Report=MakeShared<FJsonObject>();
    Report->SetStringField(TEXT("fingerprint"),WalkFingerprint);
    Report->SetStringField(TEXT("level"),GetWorld()->GetOutermost()->GetName());
    Report->SetStringField(TEXT("status"),Success?TEXT("pass"):TEXT("fail")); Report->SetStringField(TEXT("reason"),Reason);
    Report->SetStringField(TEXT("execution"),TEXT("Standalone Unreal game world; native CharacterMovement, fixed 60Hz, actual collision, no jumps or route teleports; headless automated walk, not manual PIE"));
    Report->SetNumberField(TEXT("waypoints_reached"),WalkIndex); Report->SetNumberField(TEXT("waypoints_total"),WalkPoints.Num());
    Report->SetNumberField(TEXT("simulation_seconds"),WalkFrames/60.0); Report->SetNumberField(TEXT("max_waypoint_ground_error_m"),MaxGroundError);
    FString Text;
    const FString Filename=StairTestId.IsEmpty()?TEXT("unreal_playtest_report.json"):TEXT("unreal_stair_playtest_")+StairTestId+TEXT(".json");
    Report->SetStringField(TEXT("stair_id"),StairTestId);
    FJsonSerializer::Serialize(Report,TJsonWriterFactory<>::Create(&Text));
    FFileHelper::SaveStringToFile(Text,*(FPaths::ProjectDir()/TEXT("Design/Blockout/Generated/")+Filename));
    UE_LOG(LogTemp,Display,TEXT("ThanhA CharacterMovement walk %s: %s"),Success?TEXT("PASS"):TEXT("FAIL"),*Reason);
    FPlatformMisc::RequestExitWithStatus(false,Success?0:2);
}

void AThanhACharacter::BeginEnvironmentBenchmark()
{
    FString Text; TSharedPtr<FJsonObject> Config;
    if (!FFileHelper::LoadFileToString(Text,*(FPaths::ProjectDir()/TEXT("Design/Environment/environment.json"))) ||
        !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Config))
    { FPlatformMisc::RequestExitWithStatus(false,2); return; }
    auto Vector=[](const TArray<TSharedPtr<FJsonValue>>& P) { return FVector(P[0]->AsNumber(),P[1]->AsNumber(),P[2]->AsNumber())*100; };
    for (auto Value:Config->GetArrayField(TEXT("capture_views")))
    {
        auto View=Value->AsObject(); if (View->GetStringField(TEXT("id"))!=TEXT("outside_vegetation")) continue;
        const FVector Eye=Vector(View->GetArrayField(TEXT("location_m"))),Target=Vector(View->GetArrayField(TEXT("target_m")));
        GetCharacterMovement()->DisableMovement(); SetActorEnableCollision(false);
        SetActorLocation(Eye-FVector(0,0,65));
        if (Controller) Controller->SetControlRotation((Target-Eye).Rotation());
        if (auto* Camera=FindComponentByClass<UCameraComponent>())
        {
            Camera->FieldOfView=View->GetNumberField(TEXT("fov"));
            Camera->PostProcessSettings.bOverride_AutoExposureMethod=true; Camera->PostProcessSettings.AutoExposureMethod=AEM_Manual;
            Camera->PostProcessSettings.bOverride_AutoExposureBias=true; Camera->PostProcessSettings.AutoExposureBias=Config->GetNumberField(TEXT("capture_exposure_bias"));
            Camera->PostProcessSettings.bOverride_AutoExposureApplyPhysicalCameraExposure=true; Camera->PostProcessSettings.AutoExposureApplyPhysicalCameraExposure=false;
        }
        FApp::SetUseFixedTimeStep(false); bEnvironmentBenchmark=true; BenchmarkPreviousTime=FPlatformTime::Seconds();
        return;
    }
    FPlatformMisc::RequestExitWithStatus(false,2);
}

void AThanhACharacter::TickEnvironmentBenchmark()
{
    const double Now=FPlatformTime::Seconds(),Elapsed=(Now-BenchmarkPreviousTime)*1000; BenchmarkPreviousTime=Now;
    ++BenchmarkFrames;
    if (BenchmarkFrames==180)
        FScreenshotRequest::RequestScreenshot(FPaths::ProjectDir()/TEXT("Design/Environment/Generated/benchmark_view.png"),false,false);
    // Screenshot readback/PNG encoding is intentionally in warmup, outside the FPS sample.
    if (BenchmarkFrames>240) BenchmarkFrameMilliseconds.Add(Elapsed);
    if (BenchmarkFrameMilliseconds.Num()<600) return;
    auto Report=MakeShared<FJsonObject>();
    Report->SetStringField(TEXT("execution"),TEXT("Actual standalone UE game, DX12 offscreen viewport, stationary outside_vegetation camera, wall-clock frame intervals; 240 warmup + 600 sampled frames. Screenshot readback/encoding runs during warmup only. Not manual PIE or a full-map performance guarantee."));
    Report->SetStringField(TEXT("engine"),FEngineVersion::Current().ToString());
    Report->SetStringField(TEXT("cpu"),FPlatformMisc::GetCPUBrand()); Report->SetStringField(TEXT("gpu"),FPlatformMisc::GetPrimaryGPUBrand());
    Report->SetStringField(TEXT("level"),GetWorld()->GetOutermost()->GetName());
    const FIntPoint Size=GEngine && GEngine->GameViewport && GEngine->GameViewport->Viewport?GEngine->GameViewport->Viewport->GetSizeXY():FIntPoint::ZeroValue;
    Report->SetNumberField(TEXT("width"),Size.X); Report->SetNumberField(TEXT("height"),Size.Y);
    auto Settings=MakeShared<FJsonObject>();
    for (const TCHAR* Name:{TEXT("sg.ViewDistanceQuality"),TEXT("sg.AntiAliasingQuality"),TEXT("sg.ShadowQuality"),TEXT("sg.GlobalIlluminationQuality"),TEXT("sg.ReflectionQuality"),TEXT("sg.PostProcessQuality"),TEXT("sg.TextureQuality"),TEXT("sg.EffectsQuality"),TEXT("sg.FoliageQuality"),TEXT("sg.ShadingQuality"),TEXT("r.ScreenPercentage"),TEXT("r.VSync"),TEXT("t.MaxFPS")})
        if (auto* C=IConsoleManager::Get().FindConsoleVariable(Name)) Settings->SetNumberField(Name,C->GetFloat());
    Report->SetObjectField(TEXT("scalability_cvars"),Settings);
    double Total=0; TArray<TSharedPtr<FJsonValue>> RawFrames;
    for (double T:BenchmarkFrameMilliseconds) { Total+=T; RawFrames.Add(MakeShared<FJsonValueNumber>(T)); }
    Report->SetArrayField(TEXT("frame_intervals_ms"),RawFrames);
    BenchmarkFrameMilliseconds.Sort();
    Report->SetNumberField(TEXT("sampled_frames"),BenchmarkFrameMilliseconds.Num()); Report->SetNumberField(TEXT("sampled_seconds"),Total/1000);
    Report->SetNumberField(TEXT("average_fps"),BenchmarkFrameMilliseconds.Num()*1000/Total);
    Report->SetNumberField(TEXT("mean_frame_ms"),Total/BenchmarkFrameMilliseconds.Num());
    Report->SetNumberField(TEXT("p95_frame_ms"),BenchmarkFrameMilliseconds[FMath::FloorToInt((BenchmarkFrameMilliseconds.Num()-1)*.95)]);
    Report->SetNumberField(TEXT("max_frame_ms"),BenchmarkFrameMilliseconds.Last());
    Report->SetBoolField(TEXT("fixed_simulation_timestep"),FApp::UseFixedTimeStep());
    FString Input; TSharedPtr<FJsonObject> Compiled;
    if (FFileHelper::LoadFileToString(Input,*(FPaths::ProjectDir()/TEXT("Design/Environment/Generated/environment_compiled.json"))) && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Input),Compiled))
        Report->SetStringField(TEXT("environment_fingerprint"),Compiled->GetStringField(TEXT("fingerprint")));
    FString Output; FJsonSerializer::Serialize(Report,TJsonWriterFactory<>::Create(&Output));
    FFileHelper::SaveStringToFile(Output,*(FPaths::ProjectDir()/TEXT("Design/Environment/Generated/performance.json")));
    bEnvironmentBenchmark=false; FPlatformMisc::RequestExitWithStatus(false,Size.X>0 && Size.Y>0?0:2);
}
