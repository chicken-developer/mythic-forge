#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GameFramework/GameModeBase.h"
#include "ThanhACharacter.generated.h"

UCLASS()
class TDKKINGDOMMAP_API AThanhACharacter : public ACharacter
{
    GENERATED_BODY()
public:
    AThanhACharacter();
    virtual void SetupPlayerInputComponent(UInputComponent* Input) override;
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
private:
    void Forward(float Value);
    void Right(float Value);
    void Turn(float Value);
    void Look(float Value);
    void FinishWalk(bool Success, const FString& Reason);
    void BeginEnvironmentBenchmark();
    void TickEnvironmentBenchmark();
    bool bEnvironmentBenchmark = false;
    int32 BenchmarkFrames = 0;
    double BenchmarkPreviousTime = 0;
    TArray<double> BenchmarkFrameMilliseconds;
    TArray<FVector> WalkPoints;
    int32 WalkIndex = 0;
    int32 WalkFrames = 0;
    float StuckSeconds = 0;
    FVector PreviousWalkLocation = FVector::ZeroVector;
    float MaxGroundError = 0;
    bool bAutoWalk = false;
    FString WalkFingerprint;
    FString StairTestId;
};

UCLASS()
class TDKKINGDOMMAP_API AThanhAGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    AThanhAGameMode();
};
