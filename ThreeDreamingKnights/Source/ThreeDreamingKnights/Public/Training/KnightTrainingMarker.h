#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "KnightTrainingMarker.generated.h"

class AKnightGameState;
class AGameStateBase;
class UBoxComponent;
class UPrimitiveComponent;
class UStaticMeshComponent;
class UTextRenderComponent;
class UMaterialInstanceDynamic;
struct FKnightTrainingProgress;

/** A physical training objective. Only authoritative overlap by a controlled canonical Knight counts. */
UCLASS()
class THREEDREAMINGKNIGHTS_API AKnightTrainingMarker : public AActor
{
	GENERATED_BODY()

public:
	AKnightTrainingMarker();
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditInstanceOnly, Category = "Knight|Training|Objective", meta = (ClampMin = "0", ClampMax = "2"))
	int32 MarkerIndex = 0;

	UPROPERTY(EditInstanceOnly, Category = "Knight|Training|Presentation")
	FLinearColor MarkerColor = FLinearColor(0.1f, 0.6f, 1.0f);

private:
	void HandleGameStateSet(AGameStateBase* InGameState);
	UFUNCTION()
	void HandleOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	void HandleTrainingProgress(const FKnightTrainingProgress& Progress);

	UPROPERTY(VisibleAnywhere, Category = "Knight|Training|Components")
	TObjectPtr<UBoxComponent> Trigger;

	UPROPERTY(VisibleAnywhere, Category = "Knight|Training|Components")
	TObjectPtr<UStaticMeshComponent> MarkerMesh;

	UPROPERTY(VisibleAnywhere, Category = "Knight|Training|Components")
	TObjectPtr<UTextRenderComponent> MarkerLabel;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> MarkerMaterial;

	TWeakObjectPtr<AKnightGameState> TrainingSource;
	FDelegateHandle TrainingChangedHandle;
	FDelegateHandle GameStateHandle;
};
