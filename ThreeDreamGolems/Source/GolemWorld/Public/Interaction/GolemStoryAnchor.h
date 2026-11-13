#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Types/GolemResult.h"
#include "GolemStoryAnchor.generated.h"

class UStaticMeshComponent;
class UTextRenderComponent;
class UBoxComponent;

/** Actor composition is replaceable; AnchorId remains the saved/content identity. */
UCLASS()
class GOLEMWORLD_API AGolemStoryAnchor : public AActor
{
	GENERATED_BODY()

public:
	AGolemStoryAnchor();
	UPROPERTY(EditInstanceOnly, Category = "Golem|World")
	FName AnchorId;
	UPROPERTY(EditInstanceOnly, Category = "Golem|World")
	FText Label;
	void RefreshPresentation();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type Reason) override;

private:
	UPROPERTY(VisibleAnywhere, Category = "Golem|World")
	TObjectPtr<UStaticMeshComponent> Mesh;
	UPROPERTY(VisibleAnywhere, Category = "Golem|World")
	TObjectPtr<UTextRenderComponent> Caption;
	UPROPERTY(VisibleAnywhere, Category = "Golem|World")
	TObjectPtr<UBoxComponent> Area;
	UFUNCTION()
	void HandleAreaEntered(UPrimitiveComponent* Overlapped, AActor* OtherActor, UPrimitiveComponent* OtherComponent,
		int32 BodyIndex, bool bFromSweep, const FHitResult& Hit);
};
