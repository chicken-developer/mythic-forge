#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GolemTrainingMarker.generated.h"

class AGolemGameState;
class AGameStateBase;
class UBoxComponent;
class UPrimitiveComponent;
class UStaticMeshComponent;
class UTextRenderComponent;
class UMaterialInstanceDynamic;
struct FGolemTrainingProgress;

/** A physical training objective. Only authoritative overlap by a controlled canonical Golem counts. */
UCLASS()
class THREEDREAMGOLEMS_API AGolemTrainingMarker : public AActor
{
	GENERATED_BODY()

public:
	AGolemTrainingMarker();
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditInstanceOnly, Category = "Golem|Training|Objective", meta = (ClampMin = "0", ClampMax = "2"))
	int32 MarkerIndex = 0;

	UPROPERTY(EditInstanceOnly, Category = "Golem|Training|Presentation")
	FLinearColor MarkerColor = FLinearColor(0.1f, 0.6f, 1.0f);

private:
	void HandleGameStateSet(AGameStateBase* InGameState);
	UFUNCTION()
	void HandleOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	void HandleTrainingProgress(const FGolemTrainingProgress& Progress);

	UPROPERTY(VisibleAnywhere, Category = "Golem|Training|Components")
	TObjectPtr<UBoxComponent> Trigger;

	UPROPERTY(VisibleAnywhere, Category = "Golem|Training|Components")
	TObjectPtr<UStaticMeshComponent> MarkerMesh;

	UPROPERTY(VisibleAnywhere, Category = "Golem|Training|Components")
	TObjectPtr<UTextRenderComponent> MarkerLabel;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> MarkerMaterial;

	TWeakObjectPtr<AGolemGameState> TrainingSource;
	FDelegateHandle TrainingChangedHandle;
	FDelegateHandle GameStateHandle;
};
